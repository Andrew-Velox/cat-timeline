#include "settings.h"
#include "cJSON.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Full path to settings.json inside the app's data directory. */
static char *settings_path(void) {
    char *dir = g_build_filename(g_get_user_data_dir(), "cat-timeline", NULL);
    char *file = g_build_filename(dir, "settings.json", NULL);
    g_free(dir);
    return file;
}

/* Built-in default palette. */
void settings_defaults(Settings *s) {
    s->accent = 0xffffff;
    s->cat    = 0x1c71d8;
    s->task   = 0xff0000;
    s->future = 0xffffff;
    s->done   = 0x089000;
    s->past   = 0x999999;
    s->portal = 0xffffff;
    s->layout = LAYOUT_CIRCLE;
}

/* Read a "#rrggbb" (or "rrggbb") string field, falling back when absent. */
static unsigned int parse_hex(const cJSON *o, const char *key, unsigned int fb) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (cJSON_IsString(v) && v->valuestring) {
        const char *p = v->valuestring;
        if (*p == '#')
            p++;
        return (unsigned int)(strtoul(p, NULL, 16) & 0xffffff);
    }
    return fb;
}

/* Load settings.json; defaults are applied first so partial files are fine. */
void settings_load(Settings *s) {
    settings_defaults(s);

    char *path = settings_path();
    char *data = NULL;
    gsize len = 0;
    if (g_file_get_contents(path, &data, &len, NULL)) {
        cJSON *root = cJSON_Parse(data);
        if (root) {
            s->accent = parse_hex(root, "accent", s->accent);
            s->cat    = parse_hex(root, "cat",    s->cat);
            s->task   = parse_hex(root, "task",   s->task);
            s->future = parse_hex(root, "future", s->future);
            s->done   = parse_hex(root, "done",   s->done);
            s->past   = parse_hex(root, "past",   s->past);
            s->portal = parse_hex(root, "portal", s->portal);
            const cJSON *lay = cJSON_GetObjectItemCaseSensitive(root, "layout");
            if (cJSON_IsNumber(lay))
                s->layout = lay->valueint;
            cJSON_Delete(root);
        }
        g_free(data);
    }
    g_free(path);
}

/* Add one colour as a "#rrggbb" string. */
static void add_hex(cJSON *o, const char *key, unsigned int v) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", v & 0xffffff);
    cJSON_AddStringToObject(o, key, buf);
}

/* Serialise the whole palette to settings.json. */
void settings_save(const Settings *s) {
    cJSON *root = cJSON_CreateObject();
    add_hex(root, "accent", s->accent);
    add_hex(root, "cat",    s->cat);
    add_hex(root, "task",   s->task);
    add_hex(root, "future", s->future);
    add_hex(root, "done",   s->done);
    add_hex(root, "past",   s->past);
    add_hex(root, "portal", s->portal);
    cJSON_AddNumberToObject(root, "layout", s->layout);

    char *txt = cJSON_Print(root);
    cJSON_Delete(root);
    if (!txt)
        return;

    char *dir = g_build_filename(g_get_user_data_dir(), "cat-timeline", NULL);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    char *path = settings_path();
    g_file_set_contents(path, txt, -1, NULL);
    g_free(path);
    free(txt);
}
