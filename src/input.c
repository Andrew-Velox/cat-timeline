#include "input.h"
#include "timeline.h"
#include <math.h>

/* ---- hit testing ------------------------------------------------------- */

/* Return TRUE and the day offset when (x,y) falls within a dot's hover zone. */
static gboolean hit_dot(double x, double y, int *out_off) {
    int off = nearest_offset(x);
    if (off < -PAST_DAYS - 1 || off > FUTURE_DAYS + 1)
        return FALSE;
    if (fabs(x - dot_x(off)) <= DOT_SPACING / 2.0 && fabs(y - LINE_Y) <= HOVER_VPAD) {
        *out_off = off;
        return TRUE;
    }
    return FALSE;
}

/* ---- task popover ------------------------------------------------------ */

static void popover_refresh(App *app);

/* Toggle the clicked task's done state, then rebuild the popover + canvas. */
static void on_task_toggle(GtkButton *b, gpointer ud) {
    App *app = ud;
    const char *id = g_object_get_data(G_OBJECT(b), "task-id");
    char date[DATE_LEN];
    date_for_offset(app->popover_offset, date);
    tasks_toggle(&app->store, date, id);
    popover_refresh(app);
    gtk_widget_queue_draw(app->area);
}

/* Delete the clicked task, then rebuild the popover + canvas. */
static void on_task_delete(GtkButton *b, gpointer ud) {
    App *app = ud;
    const char *id = g_object_get_data(G_OBJECT(b), "task-id");
    char date[DATE_LEN];
    date_for_offset(app->popover_offset, date);
    tasks_delete(&app->store, date, id);
    popover_refresh(app);
    gtk_widget_queue_draw(app->area);
}

/* Add the entry's text as a new task for the popover's day. */
static void on_task_add(GtkWidget *w, gpointer ud) {
    (void)w;
    App *app = ud;
    GtkEntry *entry = g_object_get_data(G_OBJECT(app->popover), "entry");
    const char *txt = gtk_entry_get_text(entry);
    if (txt && *txt) {
        char date[DATE_LEN];
        date_for_offset(app->popover_offset, date);
        tasks_add(&app->store, date, txt);
        gtk_entry_set_text(entry, "");
        popover_refresh(app);
        gtk_widget_queue_draw(app->area);
    }
}

/* Rebuild the task-row list inside the popover from current store data. */
static void popover_refresh(App *app) {
    GtkWidget *rows = g_object_get_data(G_OBJECT(app->popover), "rows");

    /* Clear existing rows. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(rows));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    char date[DATE_LEN];
    date_for_offset(app->popover_offset, date);
    DayTasks *d = tasks_find_day(&app->store, date);

    if (!d || d->count == 0) {
        GtkWidget *empty = gtk_label_new("No tasks yet.");
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(rows), empty, FALSE, FALSE, 2);
    } else {
        for (int i = 0; i < d->count; i++) {
            Task *t = &d->tasks[i];
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

            /* The task label doubles as a toggle button (flat styling). */
            char label[160];
            snprintf(label, sizeof(label), "%s %.140s", t->done ? "✓" : "○", t->text);
            GtkWidget *toggle = gtk_button_new_with_label(label);
            gtk_button_set_relief(GTK_BUTTON(toggle), GTK_RELIEF_NONE);
            gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(toggle)), GTK_ALIGN_START);
            gtk_widget_set_hexpand(toggle, TRUE);
            g_object_set_data_full(G_OBJECT(toggle), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(toggle, "clicked", G_CALLBACK(on_task_toggle), app);

            GtkWidget *del = gtk_button_new_with_label("✕");
            gtk_button_set_relief(GTK_BUTTON(del), GTK_RELIEF_NONE);
            g_object_set_data_full(G_OBJECT(del), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(del, "clicked", G_CALLBACK(on_task_delete), app);

            gtk_box_pack_start(GTK_BOX(row), toggle, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(row), del, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(rows), row, FALSE, FALSE, 0);
        }
    }
    gtk_widget_show_all(rows);
}

/* Forget the popover once it closes so a fresh one is built next time. */
static void on_popover_closed(GtkWidget *p, gpointer ud) {
    App *app = ud;
    app->popover = NULL;
    gtk_widget_destroy(p);
}

/* Build the popover UI (header, scrollable rows, entry + add) for a day. */
static void open_popover(App *app, int off) {
    if (app->popover)
        gtk_widget_destroy(app->popover);

    app->popover_offset = off;
    app->popover = gtk_popover_new(app->area);
    gtk_popover_set_position(GTK_POPOVER(app->popover), GTK_POS_TOP);
    GdkRectangle rect = { (int)(dot_x(off) - 6), (int)(LINE_Y - 6), 12, 12 };
    gtk_popover_set_pointing_to(GTK_POPOVER(app->popover), &rect);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_widget_set_size_request(box, 240, -1);

    /* Header with the day's date. */
    char header[40];
    if (off == 0)
        g_strlcpy(header, "Today", sizeof(header));
    else
        date_label_long(off, header, sizeof(header));
    GtkWidget *title = gtk_label_new(NULL);
    char markup[96];
    snprintf(markup, sizeof(markup), "<b>%s</b>", header);
    gtk_label_set_markup(GTK_LABEL(title), markup);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    /* Scrollable container for task rows. */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 180);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    GtkWidget *rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scroll), rows);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    /* Entry + Add button row. */
    GtkWidget *addrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "New task…");
    gtk_widget_set_hexpand(entry, TRUE);
    g_signal_connect(entry, "activate", G_CALLBACK(on_task_add), app);
    GtkWidget *add = gtk_button_new_with_label("Add");
    g_signal_connect(add, "clicked", G_CALLBACK(on_task_add), app);
    gtk_box_pack_start(GTK_BOX(addrow), entry, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(addrow), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), addrow, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(app->popover), box);
    g_object_set_data(G_OBJECT(app->popover), "rows", rows);
    g_object_set_data(G_OBJECT(app->popover), "entry", entry);
    g_signal_connect(app->popover, "closed", G_CALLBACK(on_popover_closed), app);

    popover_refresh(app);
    gtk_widget_show_all(box);
    gtk_popover_popup(GTK_POPOVER(app->popover));
    gtk_widget_grab_focus(entry);
}

/* ---- context menu ------------------------------------------------------ */

/* Quit the application from the right-click menu. */
static void on_quit(GtkMenuItem *item, gpointer ud) {
    (void)item; (void)ud;
    gtk_main_quit();
}

/* Pop up the right-click context menu containing "Quit". */
static void show_context_menu(GdkEvent *ev) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit, "activate", G_CALLBACK(on_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), ev);
}

/* ---- raw events -------------------------------------------------------- */

/* Track the pointer, update which dot is hovered, redraw only on change. */
static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer ud) {
    (void)w;
    App *app = ud;
    app->mouse_x = e->x;
    app->mouse_y = e->y;

    int off = HOVER_NONE;
    gboolean hit = hit_dot(e->x, e->y, &off);
    if (hit != app->has_hover || (hit && off != app->hover_offset)) {
        app->has_hover = hit;
        app->hover_offset = hit ? off : HOVER_NONE;
        gtk_widget_queue_draw(app->area);
    }
    return FALSE;
}

/* Clear the hover state when the pointer leaves the drawing area. */
static gboolean on_leave(GtkWidget *w, GdkEventCrossing *e, gpointer ud) {
    (void)w; (void)e;
    App *app = ud;
    if (app->has_hover) {
        app->has_hover = FALSE;
        app->hover_offset = HOVER_NONE;
        gtk_widget_queue_draw(app->area);
    }
    return FALSE;
}

/* Route button presses: right-click menu, click-a-dot, or drag the window. */
static gboolean on_button_press(GtkWidget *w, GdkEventButton *e, gpointer ud) {
    (void)w;
    App *app = ud;
    if (e->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (e->button == GDK_BUTTON_SECONDARY) {
        show_context_menu((GdkEvent *)e);
        return TRUE;
    }
    if (e->button == GDK_BUTTON_PRIMARY) {
        int off;
        if (hit_dot(e->x, e->y, &off)) {
            open_popover(app, off);
            return TRUE;
        }
        /* Empty space: let the user drag the borderless window around. */
        gtk_window_begin_move_drag(GTK_WINDOW(app->window),
                                   e->button, e->x_root, e->y_root, e->time);
        return TRUE;
    }
    return FALSE;
}

/* Subscribe to the pointer/button events the widget needs and wire handlers. */
void input_attach(App *app) {
    gtk_widget_add_events(app->area,
                          GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK);
    g_signal_connect(app->area, "motion-notify-event", G_CALLBACK(on_motion), app);
    g_signal_connect(app->area, "leave-notify-event", G_CALLBACK(on_leave), app);
    g_signal_connect(app->area, "button-press-event", G_CALLBACK(on_button_press), app);
}
