#ifndef TIMELINE_H
#define TIMELINE_H

#include "app.h"

/* Screen x-coordinate of the dot for a given day offset (0 = today). */
double dot_x(int offset);
/* Day offset whose dot is horizontally closest to screen x. */
int nearest_offset(double x);

/* Draw the timeline: glowing line, edge fade, dots, labels, task dashes. */
void timeline_draw(App *app, cairo_t *cr);

#endif /* TIMELINE_H */
