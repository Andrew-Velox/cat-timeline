#ifndef TASKS_H
#define TASKS_H

#include <stdbool.h>

#define MAX_ID 24          /* fixed buffer for a generated task id */
#define DATE_LEN 11        /* "YYYY-MM-DD" + NUL */

/* A single todo item belonging to one day. */
typedef struct {
    char id[MAX_ID];       /* unique id: timestamp + counter */
    char *text;            /* heap-allocated task description  */
    bool done;             /* completion state                 */
} Task;

/* All tasks that belong to one calendar day (keyed by date string). */
typedef struct {
    char date[DATE_LEN];   /* "YYYY-MM-DD" */
    Task *tasks;           /* dynamic array of tasks */
    int count;             /* number of tasks in use */
    int cap;               /* allocated capacity     */
} DayTasks;

/* The whole in-memory task database (mirror of tasks.json). */
typedef struct {
    DayTasks *days;        /* dynamic array of days */
    int count;
    int cap;
} TaskStore;

/* Initialise an empty store. */
void task_store_init(TaskStore *s);
/* Free every allocation owned by the store. */
void task_store_free(TaskStore *s);

/* Load the JSON file from disk into the store (clears it first). */
bool tasks_load(TaskStore *s);
/* Serialise the whole store to disk, creating dirs as needed. */
bool tasks_save(TaskStore *s);

/* Look up a day by date string, returning NULL when absent. */
DayTasks *tasks_find_day(TaskStore *s, const char *date);
/* Look up a day, creating an empty one when it does not exist. */
DayTasks *tasks_get_or_create_day(TaskStore *s, const char *date);

/* Append a new task to a day and persist; returns the new task. */
Task *tasks_add(TaskStore *s, const char *date, const char *text);
/* Flip the done flag of a task by id and persist. */
void tasks_toggle(TaskStore *s, const char *date, const char *id);
/* Remove a task by id and persist (prunes the day if it empties). */
void tasks_delete(TaskStore *s, const char *date, const char *id);
/* Number of tasks stored for a given date (0 if none). */
int tasks_count_for_day(TaskStore *s, const char *date);

/* Fill out11 with the "YYYY-MM-DD" date that is `offset` days from today. */
void date_for_offset(int offset, char *out11);
/* Fill out with a short label like "Jun 14" for today+offset. */
void date_label_short(int offset, char *out, int n);
/* Fill out with a long label like "Mon, Jun 14" for today+offset. */
void date_label_long(int offset, char *out, int n);

#endif /* TASKS_H */
