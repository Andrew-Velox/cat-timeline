#ifndef WINDOW_H
#define WINDOW_H

#include "app.h"

/* Build the widget window (layer-shell desktop surface where available). */
void window_create(App *app);

/* Drag the widget by adjusting its layer-shell margins. `x`/`y` are pointer
 * coordinates relative to the drawing area: begin grabs, update follows the
 * pointer, end persists the new position. (No-ops without layer-shell.) */
void window_drag_begin(App *app, double x, double y);
void window_drag_update(App *app, double x, double y);
void window_drag_end(App *app);

/* Queue a repaint of just the widget's rectangle. */
void window_redraw(App *app);

/* Re-apply window geometry after the layout (line <-> circle) changes. */
void window_apply_layout(App *app);

/* Top-left of the widget within its (possibly full-screen) surface, so input
 * coordinates can be translated to widget-local space. */
void window_widget_origin(App *app, double *ox, double *oy);

#endif /* WINDOW_H */
