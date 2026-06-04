#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include <limits.h>
#include "tasks.h"
#include "settings.h"

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
    GtkWidget *settings_win;/* settings/calendar window (or NULL) */

    TaskStore store;        /* in-memory task database            */
    Settings settings;      /* user-adjustable colour palette     */

    int frame;              /* cat animation frame, 0..7          */
    unsigned long tick;     /* monotonic timer tick (timeline fx) */
    double mouse_x, mouse_y;/* last pointer position on the area  */
    int hover_offset;       /* day offset under cursor, or none   */
    gboolean has_hover;     /* whether a dot is currently hovered */
    gboolean dragging;      /* actively dragging the widget        */
    gboolean press_pending; /* primary pressed, click-vs-drag undecided */
    double press_x, press_y;/* where the press landed (area coords)     */
    int press_off;          /* dot offset under the press, or HOVER_NONE */
} App;

/* Set the Cairo source colour from a 0xRRGGBB hex value plus alpha. */
static inline void set_hex(cairo_t *cr, unsigned int hex, double a) {
    cairo_set_source_rgba(cr,
                          ((hex >> 16) & 0xff) / 255.0,
                          ((hex >> 8) & 0xff) / 255.0,
                          (hex & 0xff) / 255.0,
                          a);
}

/* Split a 0xRRGGBB value into 0..1 colour components. */
static inline void hex_rgb(unsigned int hex, double *r, double *g, double *b) {
    *r = ((hex >> 16) & 0xff) / 255.0;
    *g = ((hex >> 8) & 0xff) / 255.0;
    *b = (hex & 0xff) / 255.0;
}

/* Mix a colour toward white by `amt` (0..1) — used for hover/highlight tints. */
static inline unsigned int hex_lighten(unsigned int hex, double amt) {
    double r = (hex >> 16) & 0xff, g = (hex >> 8) & 0xff, b = hex & 0xff;
    r += (255.0 - r) * amt;
    g += (255.0 - g) * amt;
    b += (255.0 - b) * amt;
    return ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
}

#endif /* APP_H */
