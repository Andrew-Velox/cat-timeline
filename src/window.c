#include "window.h"
#include "timeline.h"
#include "cat.h"
#include "input.h"

/* Draw callback: paint background, then timeline and cat. */
static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer ud) {
    (void)w;
    App *app = ud;

    /* Fully transparent background: clear the surface, paint only content. */
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    timeline_draw(app, cr);
    cat_draw(app, cr);

    return FALSE;
}

/* Move the window to the bottom-right corner of the primary monitor. */
static void place_bottom_right(GtkWidget *win) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    if (!mon)
        return;
    GdkRectangle geo;
    gdk_monitor_get_geometry(mon, &geo);
    const int margin = 24;
    int x = geo.x + geo.width - WIN_W - margin;
    int y = geo.y + geo.height - WIN_H - margin;
    gtk_window_move(GTK_WINDOW(win), x, y);
}

/* Give the window an RGBA visual so the background can be drawn cleanly. */
static void use_rgba_visual(GtkWidget *win) {
    GdkScreen *screen = gtk_widget_get_screen(win);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
        gtk_widget_set_visual(win, visual);
}

/* Create and configure the toplevel window plus its single drawing area. */
void window_create(App *app) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->window = win;

    gtk_window_set_title(GTK_WINDOW(win), "cat-timeline");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);       /* borderless     */
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);       /* always on top  */
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);/* no taskbar     */
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);  /* no pager       */
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_widget_set_app_paintable(win, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), WIN_W, WIN_H);
    gtk_widget_set_size_request(win, WIN_W, WIN_H);

    use_rgba_visual(win);

    GtkWidget *area = gtk_drawing_area_new();
    app->area = area;
    gtk_widget_set_size_request(area, WIN_W, WIN_H);
    gtk_container_add(GTK_CONTAINER(win), area);

    g_signal_connect(area, "draw", G_CALLBACK(on_draw), app);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    input_attach(app);

    /* Position once the window has been realised so monitor data is ready. */
    g_signal_connect(win, "realize", G_CALLBACK(place_bottom_right), NULL);
}
