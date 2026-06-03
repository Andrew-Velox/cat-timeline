#include "app.h"
#include "window.h"
#include "cat.h"

/* Program entry point: init GTK, build state and window, run the main loop. */
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    App app = {0};
    app.hover_offset = HOVER_NONE;
    app.has_hover = FALSE;

    task_store_init(&app.store);
    tasks_load(&app.store);                 /* load saved tasks (ok if none) */

    window_create(&app);

    /* Single 100ms timer drives the cat animation only. */
    g_timeout_add(100, cat_tick, &app);

    gtk_widget_show_all(app.window);
    gtk_main();

    task_store_free(&app.store);
    return 0;
}
