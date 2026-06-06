#ifndef CALWIDGET_H
#define CALWIDGET_H

#include "app.h"

/* Called when the user picks a day (month is 1-12). */
typedef void (*CalSelectCb)(int year, int month, int day, gpointer ud);

/* A compact Cairo-drawn month calendar: square cells, centred numbers, a filled
 * circle for today/selected, and a dot under days that have tasks. */
GtkWidget *cal_widget_new(App *app, CalSelectCb cb, gpointer ud);

/* Currently selected day (month 1-12). */
void cal_widget_get_selected(GtkWidget *w, int *y, int *m, int *d);

/* Repaint (e.g. after task data changed, so the day dots update). */
void cal_widget_refresh(GtkWidget *w);

#endif /* CALWIDGET_H */
