#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include <limits.h>
#include "tasks.h"

/* Strict C99 does not expose M_PI from <math.h>; provide a fallback. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- window / timeline geometry (pixels) ------------------------------- */
#define WIN_W        320
#define WIN_H        90
#define LINE_Y       60.0           /* horizontal line, ~67% from top  */
#define TODAY_X      80.0           /* today's dot, 25% from left edge */
#define DOT_SPACING  50.0           /* gap between adjacent day dots   */
#define PAST_DAYS    1              /* visible days to the left        */
#define FUTURE_DAYS  3              /* visible days to the right       */
#define HOVER_VPAD   22.0           /* vertical hover tolerance         */

#define HOVER_NONE   INT_MIN

/* Shared application state, passed to every callback as user-data. */
typedef struct {
    GtkWidget *window;      /* top-level borderless window        */
    GtkWidget *area;        /* the single GtkDrawingArea          */
    GtkWidget *popover;     /* task-management popover (or NULL)  */
    int popover_offset;     /* day offset the popover is editing  */

    TaskStore store;        /* in-memory task database            */

    int frame;              /* cat animation frame, 0..7          */
    unsigned long tick;     /* monotonic timer tick (timeline fx) */
    double mouse_x, mouse_y;/* last pointer position on the area  */
    int hover_offset;       /* day offset under cursor, or none   */
    gboolean has_hover;     /* whether a dot is currently hovered */
} App;

/* Set the Cairo source colour from a 0xRRGGBB hex value plus alpha. */
static inline void set_hex(cairo_t *cr, unsigned int hex, double a) {
    cairo_set_source_rgba(cr,
                          ((hex >> 16) & 0xff) / 255.0,
                          ((hex >> 8) & 0xff) / 255.0,
                          (hex & 0xff) / 255.0,
                          a);
}

#endif /* APP_H */
