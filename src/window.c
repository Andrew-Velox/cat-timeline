#include "window.h"
#include "timeline.h"
#include "cat.h"
#include "input.h"

#ifdef HAVE_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <stdio.h>
#include <math.h>
#endif

#define EDGE_MARGIN 24          /* gap from the screen's bottom-right corner */

#ifdef HAVE_LAYER_SHELL
/* ---- draggable layer-shell positioning ---------------------------------
 * The surface fills the whole output (a transparent bottom layer); the widget
 * is drawn at an offset inside it. That way the pointer never leaves the
 * surface mid-drag, so dragging is smooth at any speed. The input region is
 * limited to the widget's rectangle (so desktop clicks elsewhere pass through)
 * and only opened up to the full surface while a drag is in progress.
 *
 * s_margin_{r,b}: gap (px) from the right/bottom screen edges. Persisted so the
 * widget reopens where the user left it. */
static int s_margin_r = EDGE_MARGIN;
static int s_margin_b = EDGE_MARGIN;
static double s_grab_x, s_grab_y;       /* grab offset within the widget */

static void monitor_size(App *app, int *w, int *h) {
    GdkDisplay *display = gdk_display_get_default();
    GdkWindow *gw = gtk_widget_get_window(app->window);
    GdkMonitor *mon = gw ? gdk_display_get_monitor_at_window(display, gw)
                         : gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    GdkRectangle geo = { 0, 0, 1920, 1080 };
    if (mon)
        gdk_monitor_get_geometry(mon, &geo);
    *w = geo.width;
    *h = geo.height;
}

/* Top-left of the widget within the full-screen surface. */
static void widget_origin(App *app, double *ox, double *oy) {
    int mw, mh;
    monitor_size(app, &mw, &mh);
    *ox = mw - s_margin_r - WIN_W;
    *oy = mh - s_margin_b - WIN_H;
}

static void clamp_margins(App *app) {
    int mw, mh;
    monitor_size(app, &mw, &mh);
    int max_r = mw - WIN_W;
    int max_b = mh - WIN_H;
    if (s_margin_r < 0) s_margin_r = 0;
    if (s_margin_b < 0) s_margin_b = 0;
    if (max_r >= 0 && s_margin_r > max_r) s_margin_r = max_r;
    if (max_b >= 0 && s_margin_b > max_b) s_margin_b = max_b;
}

static char *position_file(void) {
    return g_build_filename(g_get_user_data_dir(), "cat-timeline", "position", NULL);
}

static void load_position(void) {
    char *path = position_file();
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d %d", &s_margin_r, &s_margin_b) != 2) {
            s_margin_r = EDGE_MARGIN;
            s_margin_b = EDGE_MARGIN;
        }
        fclose(f);
    }
    g_free(path);
}

static void save_position(void) {
    char *dir = g_build_filename(g_get_user_data_dir(), "cat-timeline", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "position", NULL);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d %d\n", s_margin_r, s_margin_b);
        fclose(f);
    }
    g_free(path);
    g_free(dir);
}

/* Restrict pointer input to the widget rect (full == TRUE opens it to the whole
 * surface, used while dragging so the pointer can't slip off and stall). */
static void set_input_region(App *app, gboolean full) {
    GdkWindow *gw = gtk_widget_get_window(app->window);
    if (!gw)
        return;
    if (full) {
        gdk_window_input_shape_combine_region(gw, NULL, 0, 0);
        return;
    }
    double ox, oy;
    widget_origin(app, &ox, &oy);
    cairo_rectangle_int_t r = { (int)ox, (int)oy, WIN_W, WIN_H };
    cairo_region_t *reg = cairo_region_create_rectangle(&r);
    gdk_window_input_shape_combine_region(gw, reg, 0, 0);
    cairo_region_destroy(reg);
}

void window_drag_begin(App *app, double fx, double fy) {
    double ox, oy;
    widget_origin(app, &ox, &oy);
    s_grab_x = fx - ox;                  /* where on the widget it was grabbed */
    s_grab_y = fy - oy;
    set_input_region(app, TRUE);
}

/* Move so the grabbed point stays under the pointer. No feedback loop: the
 * surface is fixed (full screen), only the drawn offset moves. */
void window_drag_update(App *app, double fx, double fy) {
    int mw, mh;
    monitor_size(app, &mw, &mh);
    double ox = fx - s_grab_x;
    double oy = fy - s_grab_y;
    s_margin_r = (int)lround(mw - WIN_W - ox);
    s_margin_b = (int)lround(mh - WIN_H - oy);
    clamp_margins(app);
    gtk_widget_queue_draw(app->area);    /* widget moved: repaint everything */
}

void window_drag_end(App *app) {
    set_input_region(app, FALSE);
    save_position();
}

/* Repaint just the widget's rectangle (cheap; avoids full-screen damage). */
void window_redraw(App *app) {
    double ox, oy;
    widget_origin(app, &ox, &oy);
    gtk_widget_queue_draw_area(app->area, (int)ox, (int)oy, WIN_W, WIN_H);
}

void window_widget_origin(App *app, double *ox, double *oy) {
    widget_origin(app, ox, oy);
}
#else
void window_drag_begin(App *app, double x, double y) { (void)app; (void)x; (void)y; }
void window_drag_update(App *app, double x, double y) { (void)app; (void)x; (void)y; }
void window_drag_end(App *app) { (void)app; }
void window_redraw(App *app) { gtk_widget_queue_draw(app->area); }
void window_widget_origin(App *app, double *ox, double *oy) { (void)app; *ox = 0; *oy = 0; }
#endif

/* Draw callback: paint background, then timeline and cat (offset to the widget
 * position when the surface is full-screen). */
static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer ud) {
    (void)w;
    App *app = ud;

    /* Fully transparent background: clear the surface, paint only content. */
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

#ifdef HAVE_LAYER_SHELL
    double ox, oy;
    widget_origin(app, &ox, &oy);
    cairo_translate(cr, ox, oy);
#endif
    timeline_draw(app, cr);
    cat_draw(app, cr);

    return FALSE;
}

/* Give the window an RGBA visual so the background can be drawn cleanly. */
static void use_rgba_visual(GtkWidget *win) {
    GdkScreen *screen = gtk_widget_get_screen(win);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
        gtk_widget_set_visual(win, visual);
}

#ifdef HAVE_LAYER_SHELL
/* Set the initial (widget-rect) input region once the surface is mapped. */
static gboolean on_map(GtkWidget *win, GdkEvent *e, gpointer ud) {
    (void)win; (void)e;
    set_input_region((App *)ud, FALSE);
    return FALSE;
}
#else
/* Move the window to the bottom-right corner (fallback without layer-shell). */
static void place_bottom_right(GtkWidget *win) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    if (!mon)
        return;
    GdkRectangle geo;
    gdk_monitor_get_geometry(mon, &geo);
    int x = geo.x + geo.width - WIN_W - EDGE_MARGIN;
    int y = geo.y + geo.height - WIN_H - EDGE_MARGIN;
    gtk_window_move(GTK_WINDOW(win), x, y);
}
#endif

/* Create and configure the window plus its single drawing area. */
void window_create(App *app) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->window = win;

    gtk_window_set_title(GTK_WINDOW(win), "cat-timeline");
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_widget_set_app_paintable(win, TRUE);

    use_rgba_visual(win);

    GtkWidget *area = gtk_drawing_area_new();
    app->area = area;
    gtk_container_add(GTK_CONTAINER(win), area);

#ifdef HAVE_LAYER_SHELL
    /* Transparent full-output surface on the bottom layer: a desktop widget
     * that sits below normal windows yet takes clicks, drawn at the saved
     * position and freely draggable. */
    load_position();
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_namespace(GTK_WINDOW(win), "cat-timeline");
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
    g_signal_connect(win, "map-event", G_CALLBACK(on_map), app);
#else
    /* Fallback (no layer-shell / X11): borderless always-on-top utility window. */
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size(GTK_WINDOW(win), WIN_W, WIN_H);
    gtk_widget_set_size_request(win, WIN_W, WIN_H);
    gtk_widget_set_size_request(area, WIN_W, WIN_H);
    g_signal_connect(win, "realize", G_CALLBACK(place_bottom_right), NULL);
#endif

    g_signal_connect(area, "draw", G_CALLBACK(on_draw), app);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    input_attach(app);
}
