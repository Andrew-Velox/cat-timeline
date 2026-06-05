#ifndef CIRCLE_H
#define CIRCLE_H

#include "app.h"

/* ---- ring geometry (within the CIRC_W x CIRC_H square canvas) ----------- */
#define RING_CX     100.0           /* ring centre x                        */
#define RING_CY     92.0            /* ring centre y (room for cat below)   */
#define RING_R      64.0            /* ring radius                          */
#define DAY_ARC     (44.0 * M_PI / 180.0)  /* angle between adjacent days   */
#define BASE_ANG    (M_PI / 2.0)    /* "now" sits at the bottom of the ring */
#define CIRC_PAST   2               /* past days shown to the left          */
#define CIRC_FUTURE 3               /* future days shown to the right       */

/* Screen position of a day's dot on the ring. The ring scrolls with the day
 * just like the line: "now" is pinned at the bottom and tomorrow rotates down
 * toward it, arriving at midnight. */
void circle_dot_pos(int offset, double *x, double *y);

/* Where the cat runs: the bottom of the ring (the live "now" point). */
void circle_cat_anchor(double *cx, double *feet_y);

/* Hit-test a widget-local point against the ring dots. */
gboolean circle_hit_dot(double x, double y, int *out_off);

/* Draw the circular timeline: ring, portals, dots, day numbers, task ticks. */
void circle_draw(App *app, cairo_t *cr);

#endif /* CIRCLE_H */
