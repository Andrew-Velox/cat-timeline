#include "tasks.h"
#include "cJSON.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- paths ------------------------------------------------------------- */

/* Build the data directory path: $XDG_DATA_HOME/cat-timeline (~/.local/share/...). */
static char *data_dir(void) {
    return g_build_filename(g_get_user_data_dir(), "cat-timeline", NULL);
}

/* Build the full path to tasks.json inside the data directory. */
static char *data_file(void) {
    char *dir = data_dir();
    char *file = g_build_filename(dir, "tasks.json", NULL);
    g_free(dir);
    return file;
}

/* ---- id generation ----------------------------------------------------- */

/* Generate a short unique id from the current time plus a process counter. */
static void gen_id(char *out) {
    static unsigned long counter = 0;
    snprintf(out, MAX_ID, "%lx%03lx", (unsigned long)time(NULL), counter++ & 0xfff);
}

/* ---- date helpers ------------------------------------------------------ */

/* Return a normalised struct tm for local midnight, `offset` days from today. */
static struct tm tm_for_offset(int offset) {
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_mday += offset;   /* mktime normalises month/year/DST roll-over */
    mktime(&t);
    return t;
}

/* Fraction (0..1) of the local day elapsed (00:00 -> 0, next midnight -> 1).
 * If CAT_TIMELINE_DEMO is set, a "day" lasts that many seconds instead, so
 * the scroll is fast enough to watch (useful for testing the effect). */
double day_fraction(void) {
    const char *demo = g_getenv("CAT_TIMELINE_DEMO");
    if (demo && *demo) {
        double span = g_strtod(demo, NULL);          /* seconds per fake day */
        if (span < 1.0)
            span = 1.0;
        double t = g_get_monotonic_time() / 1000000.0;
        return fmod(t / span, 1.0);
    }
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    double secs = t.tm_hour * 3600.0 + t.tm_min * 60.0 + t.tm_sec;
    return secs / 86400.0;
}

/* Fill out11 with "YYYY-MM-DD" for today + offset days. */
void date_for_offset(int offset, char *out11) {
    struct tm t = tm_for_offset(offset);
    strftime(out11, DATE_LEN, "%Y-%m-%d", &t);
}

/* Fill out with a short "Mon DD" style label for today + offset days. */
void date_label_short(int offset, char *out, int n) {
    struct tm t = tm_for_offset(offset);
    strftime(out, n, "%b %e", &t);
}

/* Fill out with a long "Wkd, Mon DD" style label for today + offset days. */
void date_label_long(int offset, char *out, int n) {
    struct tm t = tm_for_offset(offset);
    strftime(out, n, "%a, %b %e", &t);
}

/* ---- store lifecycle --------------------------------------------------- */

/* Initialise an empty in-memory store. */
void task_store_init(TaskStore *s) {
    s->days = NULL;
    s->count = 0;
    s->cap = 0;
}

/* Free a single day's task array and its strings. */
static void free_day(DayTasks *d) {
    for (int i = 0; i < d->count; i++)
        free(d->tasks[i].text);
    free(d->tasks);
    d->tasks = NULL;
    d->count = d->cap = 0;
}

/* Release every allocation owned by the store and reset it. */
void task_store_free(TaskStore *s) {
    for (int i = 0; i < s->count; i++)
        free_day(&s->days[i]);
    free(s->days);
    task_store_init(s);
}

/* ---- lookups & growth -------------------------------------------------- */

/* Return the day matching `date`, or NULL when it is not present. */
DayTasks *tasks_find_day(TaskStore *s, const char *date) {
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->days[i].date, date) == 0)
            return &s->days[i];
    return NULL;
}

/* Return the day matching `date`, allocating an empty one when missing. */
DayTasks *tasks_get_or_create_day(TaskStore *s, const char *date) {
    DayTasks *d = tasks_find_day(s, date);
    if (d)
        return d;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->days = realloc(s->days, s->cap * sizeof(*s->days));
    }
    d = &s->days[s->count++];
    memset(d, 0, sizeof(*d));
    g_strlcpy(d->date, date, DATE_LEN);
    return d;
}

/* Append an empty task slot to a day and return it. */
static Task *day_new_task(DayTasks *d) {
    if (d->count == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 4;
        d->tasks = realloc(d->tasks, d->cap * sizeof(*d->tasks));
    }
    Task *t = &d->tasks[d->count++];
    memset(t, 0, sizeof(*t));
    return t;
}

/* ---- mutations (each persists immediately) ----------------------------- */

/* Add a task with the given text to a date and save to disk. */
Task *tasks_add(TaskStore *s, const char *date, const char *text) {
    DayTasks *d = tasks_get_or_create_day(s, date);
    Task *t = day_new_task(d);
    gen_id(t->id);
    t->text = g_strdup(text);
    t->done = false;
    tasks_save(s);
    return t;
}

/* Toggle the done flag for the task with `id` on `date` and save. */
void tasks_toggle(TaskStore *s, const char *date, const char *id) {
    DayTasks *d = tasks_find_day(s, date);
    if (!d)
        return;
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->tasks[i].id, id) == 0) {
            d->tasks[i].done = !d->tasks[i].done;
            tasks_save(s);
            return;
        }
    }
}

/* Remove the day at index `idx`, compacting the array. */
static void remove_day(TaskStore *s, int idx) {
    free_day(&s->days[idx]);
    for (int i = idx; i < s->count - 1; i++)
        s->days[i] = s->days[i + 1];
    s->count--;
}

/* Delete the task with `id` from `date`; prune empty days; save. */
void tasks_delete(TaskStore *s, const char *date, const char *id) {
    DayTasks *d = tasks_find_day(s, date);
    if (!d)
        return;
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->tasks[i].id, id) == 0) {
            free(d->tasks[i].text);
            for (int j = i; j < d->count - 1; j++)
                d->tasks[j] = d->tasks[j + 1];
            d->count--;
            if (d->count == 0)
                remove_day(s, (int)(d - s->days));
            tasks_save(s);
            return;
        }
    }
}

/* Return how many tasks are stored for the given date. */
int tasks_count_for_day(TaskStore *s, const char *date) {
    DayTasks *d = tasks_find_day(s, date);
    return d ? d->count : 0;
}

/* ---- persistence ------------------------------------------------------- */

/* Read the JSON file from disk and populate the store. */
bool tasks_load(TaskStore *s) {
    task_store_free(s);
    char *path = data_file();
    char *raw = NULL;
    gsize len = 0;
    gboolean ok = g_file_get_contents(path, &raw, &len, NULL);
    g_free(path);
    if (!ok)
        return false;             /* no file yet is fine */

    cJSON *root = cJSON_Parse(raw);
    g_free(raw);
    if (!root)
        return false;

    cJSON *day = NULL;
    cJSON_ArrayForEach(day, root) {
        if (!cJSON_IsArray(day))
            continue;
        DayTasks *d = tasks_get_or_create_day(s, day->string);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, day) {
            cJSON *jid = cJSON_GetObjectItem(item, "id");
            cJSON *jtext = cJSON_GetObjectItem(item, "text");
            cJSON *jdone = cJSON_GetObjectItem(item, "done");
            Task *t = day_new_task(d);
            if (cJSON_IsString(jid))
                g_strlcpy(t->id, jid->valuestring, MAX_ID);
            else
                gen_id(t->id);
            t->text = g_strdup(cJSON_IsString(jtext) ? jtext->valuestring : "");
            t->done = cJSON_IsTrue(jdone);
        }
    }
    cJSON_Delete(root);
    return true;
}

/* Serialise the whole store to JSON and write it to disk atomically. */
bool tasks_save(TaskStore *s) {
    cJSON *root = cJSON_CreateObject();
    for (int i = 0; i < s->count; i++) {
        DayTasks *d = &s->days[i];
        cJSON *arr = cJSON_CreateArray();
        for (int j = 0; j < d->count; j++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "id", d->tasks[j].id);
            cJSON_AddStringToObject(item, "text", d->tasks[j].text ? d->tasks[j].text : "");
            cJSON_AddBoolToObject(item, "done", d->tasks[j].done);
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddItemToObject(root, d->date, arr);
    }

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    if (!out)
        return false;

    char *dir = data_dir();
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    char *path = data_file();
    gboolean ok = g_file_set_contents(path, out, -1, NULL);
    g_free(path);
    free(out);
    return ok;
}
