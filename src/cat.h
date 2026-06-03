#ifndef CAT_H
#define CAT_H

#include "app.h"

/* Draw the running cat (all Cairo paths) on today's dot for the current frame. */
void cat_draw(App *app, cairo_t *cr);

/* g_timeout callback: advance the animation frame and request a redraw. */
gboolean cat_tick(gpointer user_data);

#endif /* CAT_H */
