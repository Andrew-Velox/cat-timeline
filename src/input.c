#include "input.h"
#include "timeline.h"
#include "settings_window.h"
#include "style.h"
#include <gdk/gdkkeysyms.h>
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
    GtkWidget *prog = g_object_get_data(G_OBJECT(app->popover), "prog");
    GtkWidget *foot = g_object_get_data(G_OBJECT(app->popover), "foot");

    /* Clear existing rows. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(rows));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    char date[DATE_LEN];
    date_for_offset(app->popover_offset, date);
    DayTasks *d = tasks_find_day(&app->store, date);
    int ntasks = d ? d->count : 0;
    int ndone = 0;
    for (int i = 0; i < ntasks; i++)
        if (d->tasks[i].done) ndone++;

    /* Progress bar + footer tally reflect the day's completion. */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog),
                                  ntasks ? (double)ndone / ntasks : 0.0);
    gtk_widget_set_visible(prog, ntasks > 0);
    char tally[48];
    if (ntasks)
        g_snprintf(tally, sizeof(tally), "%d of %d done", ndone, ntasks);
    else
        g_strlcpy(tally, "Nothing planned yet", sizeof(tally));
    gtk_label_set_text(GTK_LABEL(foot), tally);

    char task_hex[8], done_hex[8];
    style_hex(app->settings.task, task_hex);
    style_hex(app->settings.done, done_hex);

    if (ntasks == 0) {
        GtkWidget *empty = gtk_label_new("No tasks — add one below.");
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        style_class(empty, "tp-empty");
        gtk_box_pack_start(GTK_BOX(rows), empty, FALSE, FALSE, 0);
    } else {
        for (int i = 0; i < ntasks; i++) {
            Task *t = &d->tasks[i];
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

            /* Colour-coded status glyph (○ pending / ✓ done) plus the task
             * text (struck through and dimmed when done), via Pango markup. */
            char *esc = g_markup_escape_text(t->text, -1);
            char markup[640];
            if (t->done)
                g_snprintf(markup, sizeof(markup),
                    "<span foreground='%s' size='large'>\xE2\x9C\x93</span>  "
                    "<span foreground='#9aa0a6' strikethrough='true'>%s</span>",
                    done_hex, esc);
            else
                g_snprintf(markup, sizeof(markup),
                    "<span foreground='%s' size='large'>\xE2\x97\x8B</span>  "
                    "<span foreground='#2a2a30'>%s</span>",
                    task_hex, esc);
            g_free(esc);

            GtkWidget *toggle = gtk_button_new_with_label("");
            GtkWidget *lbl = gtk_bin_get_child(GTK_BIN(toggle));
            gtk_label_set_markup(GTK_LABEL(lbl), markup);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
            gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
            gtk_button_set_relief(GTK_BUTTON(toggle), GTK_RELIEF_NONE);
            style_class(toggle, "tp-task");
            gtk_widget_set_hexpand(toggle, TRUE);
            g_object_set_data_full(G_OBJECT(toggle), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(toggle, "clicked", G_CALLBACK(on_task_toggle), app);

            GtkWidget *del = gtk_button_new_with_label("\xE2\x9C\x95");
            gtk_button_set_relief(GTK_BUTTON(del), GTK_RELIEF_NONE);
            style_class(del, "tp-del");
            g_object_set_data_full(G_OBJECT(del), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(del, "clicked", G_CALLBACK(on_task_delete), app);

            gtk_box_pack_start(GTK_BOX(row), toggle, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(row), del, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(rows), row, FALSE, FALSE, 0);
        }
    }
    gtk_widget_show_all(rows);
}

/* Forget the editor window once it is destroyed so a fresh one is built next. */
static void on_editor_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    App *app = ud;
    app->popover = NULL;
}

/* Close button / Escape dismiss the editor. */
static void on_editor_close(GtkWidget *w, gpointer ud) {
    (void)w;
    App *app = ud;
    if (app->popover)
        gtk_widget_destroy(app->popover);
}

static gboolean on_editor_key(GtkWidget *w, GdkEventKey *e, gpointer ud) {
    (void)w;
    App *app = ud;
    if (e->keyval == GDK_KEY_Escape) {
        if (app->popover)
            gtk_widget_destroy(app->popover);
        return TRUE;
    }
    return FALSE;
}

/* Park the editor just above the timeline widget, centred on the clicked dot.
 * (On X11 this is exact; native Wayland ignores moves and the compositor
 * places it — same trade-off as the settings window.) */
static void place_editor(App *app, GtkWidget *win, int off) {
    GtkRequisition req;
    gtk_widget_get_preferred_size(win, NULL, &req);

    int rx = 0, ry = 0;
    GdkWindow *aw = gtk_widget_get_window(app->area);
    if (aw)
        gdk_window_get_origin(aw, &rx, &ry);

    int x = rx + (int)dot_x(off) - req.width / 2;
    int y = ry - req.height - 10;

    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *mon = aw ? gdk_display_get_monitor_at_window(display, aw)
                         : gdk_display_get_primary_monitor(display);
    if (mon) {
        GdkRectangle geo;
        gdk_monitor_get_geometry(mon, &geo);
        if (x < geo.x + 8) x = geo.x + 8;
        if (x + req.width > geo.x + geo.width - 8) x = geo.x + geo.width - req.width - 8;
        if (y < geo.y + 8) y = geo.y + 8;
    }
    gtk_window_move(GTK_WINDOW(win), x, y);
}

/* Build the day's task editor as a compact top-level panel (reliable input on
 * Wayland/Hyprland, where GtkPopover grabs misbehave) and pop it up by the dot. */
static void open_popover(App *app, int off) {
    if (app->popover)
        gtk_widget_destroy(app->popover);

    style_ensure(app);
    app->popover_offset = off;

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->popover = win;
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(app->window));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    style_class(box, "taskpanel");
    style_class(box, "tp-box");
    gtk_widget_set_size_request(box, 252, -1);
    gtk_container_add(GTK_CONTAINER(win), box);

    /* Header: date (left), +Nd / TODAY badge and a × close (right). */
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    char header[40];
    if (off == 0)
        g_strlcpy(header, "Today", sizeof(header));
    else
        date_label_long(off, header, sizeof(header));
    GtkWidget *title = gtk_label_new(header);
    style_class(title, "tp-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    char badge_txt[12];
    if (off == 0)
        g_strlcpy(badge_txt, "TODAY", sizeof(badge_txt));
    else
        g_snprintf(badge_txt, sizeof(badge_txt), "%+dd", off);
    GtkWidget *badge = gtk_label_new(badge_txt);
    style_class(badge, "tp-badge");
    gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);

    GtkWidget *close = gtk_button_new_with_label("\xE2\x9C\x95");
    style_class(close, "tp-close");
    gtk_widget_set_valign(close, GTK_ALIGN_CENTER);
    g_signal_connect(close, "clicked", G_CALLBACK(on_editor_close), app);

    gtk_box_pack_start(GTK_BOX(head), title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(head), close, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(head), badge, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), head, FALSE, FALSE, 0);

    /* Slim completion bar. */
    GtkWidget *prog = gtk_progress_bar_new();
    style_class(prog, "tp-prog");
    gtk_widget_set_no_show_all(prog, TRUE);  /* visibility set in refresh */
    gtk_box_pack_start(GTK_BOX(box), prog, FALSE, FALSE, 0);

    /* Scrollable container for task rows (capped so long lists scroll). */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 40);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 168);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    GtkWidget *rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_container_add(GTK_CONTAINER(scroll), rows);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    /* Footer tally. */
    GtkWidget *foot = gtk_label_new("");
    style_class(foot, "tp-foot");
    gtk_widget_set_halign(foot, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), foot, FALSE, FALSE, 0);

    /* Entry + Add button row. */
    GtkWidget *addrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *entry = gtk_entry_new();
    style_class(entry, "tp-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "New task…");
    gtk_widget_set_hexpand(entry, TRUE);
    g_signal_connect(entry, "activate", G_CALLBACK(on_task_add), app);
    GtkWidget *add = gtk_button_new_with_label("Add");
    style_class(add, "tp-add");
    g_signal_connect(add, "clicked", G_CALLBACK(on_task_add), app);
    gtk_box_pack_start(GTK_BOX(addrow), entry, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(addrow), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), addrow, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(win), "rows", rows);
    g_object_set_data(G_OBJECT(win), "entry", entry);
    g_object_set_data(G_OBJECT(win), "prog", prog);
    g_object_set_data(G_OBJECT(win), "foot", foot);
    g_signal_connect(win, "destroy", G_CALLBACK(on_editor_destroy), app);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_editor_key), app);

    popover_refresh(app);
    gtk_widget_show_all(win);
    place_editor(app, win, off);
    gtk_window_present(GTK_WINDOW(win));
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

    /* Armed by an empty-space press: start the window move once we actually
     * move, so a stationary double-click can still open the settings window. */
    if (app->drag_armed && (e->state & GDK_BUTTON1_MASK)) {
        app->drag_armed = FALSE;
        gtk_window_begin_move_drag(GTK_WINDOW(app->window), 1,
                                   (int)e->x_root, (int)e->y_root, e->time);
        return TRUE;
    }

    int off = HOVER_NONE;
    gboolean hit = hit_dot(e->x, e->y, &off);
    if (hit != app->has_hover || (hit && off != app->hover_offset)) {
        app->has_hover = hit;
        app->hover_offset = hit ? off : HOVER_NONE;
        gtk_widget_queue_draw(app->area);   /* repaint the dot highlight */
    }
    return FALSE;
}

/* Clear the hover state (dot highlight) when the pointer leaves the area. */
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

    /* Double-click on empty space opens the settings + calendar window. */
    if (e->type == GDK_2BUTTON_PRESS && e->button == GDK_BUTTON_PRIMARY) {
        app->drag_armed = FALSE;
        int off;
        if (!hit_dot(e->x, e->y, &off))
            settings_window_open(app);
        return TRUE;
    }
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
        /* Empty space: arm a drag (begins on motion) so a double-click here
         * still reaches us as GDK_2BUTTON_PRESS. */
        app->drag_armed = TRUE;
        return TRUE;
    }
    return FALSE;
}

/* Disarm a pending drag when the button is released without moving. */
static gboolean on_button_release(GtkWidget *w, GdkEventButton *e, gpointer ud) {
    (void)w; (void)e;
    App *app = ud;
    app->drag_armed = FALSE;
    return FALSE;
}

/* Subscribe to the pointer/button events the widget needs and wire handlers. */
void input_attach(App *app) {
    gtk_widget_add_events(app->area,
                          GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK);
    g_signal_connect(app->area, "motion-notify-event", G_CALLBACK(on_motion), app);
    g_signal_connect(app->area, "leave-notify-event", G_CALLBACK(on_leave), app);
    g_signal_connect(app->area, "button-press-event", G_CALLBACK(on_button_press), app);
    g_signal_connect(app->area, "button-release-event", G_CALLBACK(on_button_release), app);
}
