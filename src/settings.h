#ifndef SETTINGS_H
#define SETTINGS_H

/* User-adjustable colours (0xRRGGBB), persisted to settings.json next to the
 * task data. Past-day dots are derived from `accent`/`future` at draw time. */
typedef struct {
    unsigned int accent;   /* timeline line, glow pulse, today's dot */
    unsigned int cat;      /* the running cat tint                   */
    unsigned int task;     /* pending task dashes (today + future)   */
    unsigned int future;   /* upcoming day dots                      */
    unsigned int done;     /* completed task dashes                  */
    unsigned int past;     /* past day dots + past pending dashes    */
} Settings;

/* Fill s with the built-in default palette. */
void settings_defaults(Settings *s);
/* Load settings.json into s (defaults first, so missing keys are filled). */
void settings_load(Settings *s);
/* Persist s to settings.json, creating the data dir as needed. */
void settings_save(const Settings *s);

#endif /* SETTINGS_H */
