#include "settings_window.h"
#include "settings.h"
#include "tasks.h"

#include <stdio.h>

/* Per-window state shared between the calendar and task-list callbacks. */
typedef struct {
    App         *app;
    GtkCalendar *cal;        /* month view, marks days that have tasks  */
    GtkWidget   *day_label;  /* "Tasks on <date>" header                */
    GtkWidget   *list;       /* vbox holding one row per task           */
    GtkWidget   *entry;      /* new-task entry for the selected day     */

    GtkWidget    *cbtn[6];   /* colour buttons (for Reset to refresh)   */
    unsigned int *cfield[6]; /* the Settings field each button edits    */
    int           ncolors;
} Ctx;

/* ---- colour helpers ---------------------------------------------------- */

static void hex_to_rgba(unsigned int hex, GdkRGBA *c) {
    c->red   = ((hex >> 16) & 0xff) / 255.0;
    c->green = ((hex >> 8) & 0xff) / 255.0;
    c->blue  = (hex & 0xff) / 255.0;
    c->alpha = 1.0;
}

static unsigned int rgba_to_hex(const GdkRGBA *c) {
    unsigned r = (unsigned)(c->red * 255.0 + 0.5);
    unsigned g = (unsigned)(c->green * 255.0 + 0.5);
    unsigned b = (unsigned)(c->blue * 255.0 + 0.5);
    return (r << 16) | (g << 8) | b;
}

/* ---- date helpers ------------------------------------------------------ */

/* The "YYYY-MM-DD" string for the calendar's currently selected day. */
static void selected_date(Ctx *ctx, char *out11) {
    guint y, m, d;
    gtk_calendar_get_date(ctx->cal, &y, &m, &d);   /* m is 0-based */
    snprintf(out11, DATE_LEN, "%04u-%02u-%02u", y, m + 1, d);
}

/* Mark every day in the displayed month that has at least one task. */
static void mark_task_days(Ctx *ctx) {
    guint y, m, d;
    gtk_calendar_get_date(ctx->cal, &y, &m, &d);
    gtk_calendar_clear_marks(ctx->cal);

    TaskStore *s = &ctx->app->store;
    for (int i = 0; i < s->count; i++) {
        int yy, mm, dd;
        if (sscanf(s->days[i].date, "%d-%d-%d", &yy, &mm, &dd) == 3 &&
            yy == (int)y && mm == (int)m + 1 && s->days[i].count > 0)
            gtk_calendar_mark_day(ctx->cal, dd);
    }
}

/* ---- task list for the selected day ------------------------------------ */

static void refresh_task_list(Ctx *ctx);

static void on_toggle(GtkButton *b, gpointer ud) {
    Ctx *ctx = ud;
    const char *id = g_object_get_data(G_OBJECT(b), "task-id");
    char date[DATE_LEN];
    selected_date(ctx, date);
    tasks_toggle(&ctx->app->store, date, id);
    refresh_task_list(ctx);
    gtk_widget_queue_draw(ctx->app->area);
}

static void on_delete(GtkButton *b, gpointer ud) {
    Ctx *ctx = ud;
    const char *id = g_object_get_data(G_OBJECT(b), "task-id");
    char date[DATE_LEN];
    selected_date(ctx, date);
    tasks_delete(&ctx->app->store, date, id);
    refresh_task_list(ctx);
    mark_task_days(ctx);                 /* a day may have just emptied */
    gtk_widget_queue_draw(ctx->app->area);
}

static void on_add(GtkWidget *w, gpointer ud) {
    (void)w;
    Ctx *ctx = ud;
    const char *txt = gtk_entry_get_text(GTK_ENTRY(ctx->entry));
    if (!txt || !*txt)
        return;
    char date[DATE_LEN];
    selected_date(ctx, date);
    tasks_add(&ctx->app->store, date, txt);
    gtk_entry_set_text(GTK_ENTRY(ctx->entry), "");
    refresh_task_list(ctx);
    mark_task_days(ctx);
    gtk_widget_queue_draw(ctx->app->area);
}

/* Rebuild the header label and task rows for the selected calendar day. */
static void refresh_task_list(Ctx *ctx) {
    char date[DATE_LEN];
    selected_date(ctx, date);

    char header[48];
    snprintf(header, sizeof(header), "Tasks on %s", date);
    gtk_label_set_text(GTK_LABEL(ctx->day_label), header);

    GList *children = gtk_container_get_children(GTK_CONTAINER(ctx->list));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    DayTasks *d = tasks_find_day(&ctx->app->store, date);
    if (!d || d->count == 0) {
        GtkWidget *empty = gtk_label_new("No tasks for this day.");
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(ctx->list), empty, FALSE, FALSE, 2);
    } else {
        for (int i = 0; i < d->count; i++) {
            Task *t = &d->tasks[i];
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

            char label[160];
            snprintf(label, sizeof(label), "%s %.140s", t->done ? "✓" : "○", t->text);
            GtkWidget *toggle = gtk_button_new_with_label(label);
            gtk_button_set_relief(GTK_BUTTON(toggle), GTK_RELIEF_NONE);
            gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(toggle)), GTK_ALIGN_START);
            gtk_widget_set_hexpand(toggle, TRUE);
            g_object_set_data_full(G_OBJECT(toggle), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(toggle, "clicked", G_CALLBACK(on_toggle), ctx);

            GtkWidget *del = gtk_button_new_with_label("✕");
            gtk_button_set_relief(GTK_BUTTON(del), GTK_RELIEF_NONE);
            g_object_set_data_full(G_OBJECT(del), "task-id", g_strdup(t->id), g_free);
            g_signal_connect(del, "clicked", G_CALLBACK(on_delete), ctx);

            gtk_box_pack_start(GTK_BOX(row), toggle, TRUE, TRUE, 0);
            gtk_box_pack_end(GTK_BOX(row), del, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(ctx->list), row, FALSE, FALSE, 0);
        }
    }
    gtk_widget_show_all(ctx->list);
}

static void on_calendar_changed(GtkCalendar *cal, gpointer ud) {
    (void)cal;
    Ctx *ctx = ud;
    mark_task_days(ctx);
    refresh_task_list(ctx);
}

/* ---- appearance section ------------------------------------------------ */

static void on_color_set(GtkColorButton *btn, gpointer ud) {
    Ctx *ctx = ud;
    unsigned int *field = g_object_get_data(G_OBJECT(btn), "field");
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &c);
    *field = rgba_to_hex(&c);
    settings_save(&ctx->app->settings);
    gtk_widget_queue_draw(ctx->app->area);
}

static void on_reset(GtkButton *b, gpointer ud) {
    (void)b;
    Ctx *ctx = ud;
    settings_defaults(&ctx->app->settings);
    for (int i = 0; i < ctx->ncolors; i++) {
        GdkRGBA c;
        hex_to_rgba(*ctx->cfield[i], &c);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ctx->cbtn[i]), &c);
    }
    settings_save(&ctx->app->settings);
    gtk_widget_queue_draw(ctx->app->area);
}

/* Add one "label + colour button" row to the appearance grid. */
static void add_color_row(Ctx *ctx, GtkWidget *grid, int idx, const char *name,
                          unsigned int *field) {
    GtkWidget *label = gtk_label_new(name);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);

    GdkRGBA c;
    hex_to_rgba(*field, &c);
    GtkWidget *btn = gtk_color_button_new_with_rgba(&c);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(btn), FALSE);
    g_object_set_data(G_OBJECT(btn), "field", field);
    g_signal_connect(btn, "color-set", G_CALLBACK(on_color_set), ctx);

    gtk_grid_attach(GTK_GRID(grid), label, 0, idx, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn,   1, idx, 1, 1);

    ctx->cbtn[idx]   = btn;
    ctx->cfield[idx] = field;
}

/* ---- window lifecycle -------------------------------------------------- */

static void on_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    App *app = ud;
    app->settings_win = NULL;          /* Ctx is freed via set_data_full */
}

/* Park the panel just above the widget in the monitor's bottom-right corner. */
static void place_near_widget(GtkWidget *win) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    if (!mon)
        return;
    GdkRectangle geo;
    gdk_monitor_get_geometry(mon, &geo);

    GtkRequisition req;
    gtk_widget_get_preferred_size(win, NULL, &req);   /* natural size */

    const int margin = 12;
    int x = geo.x + geo.width - req.width - margin;
    int y = geo.y + geo.height - WIN_H - margin - req.height - 12;
    if (y < geo.y + margin)
        y = geo.y + margin;
    gtk_window_move(GTK_WINDOW(win), x, y);
}

void settings_window_open(App *app) {
    if (app->settings_win) {           /* already open: just focus it */
        gtk_window_present(GTK_WINDOW(app->settings_win));
        return;
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->settings_win = win;
    gtk_window_set_title(GTK_WINDOW(win), "cat-timeline — Settings");
    /* Compact floating panel that hovers above other apps. The dialog/utility
     * hints + non-resizable keep tiling compositors (Hyprland) from tiling it. */
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_NONE);
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 360);

    Ctx *ctx = g_new0(Ctx, 1);
    ctx->app = app;
    g_object_set_data_full(G_OBJECT(win), "ctx", ctx, g_free);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root), 8);
    gtk_container_add(GTK_CONTAINER(win), root);

    GtkWidget *tabs = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(root), tabs, TRUE, TRUE, 0);

    GtkWidget *page_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_tasks), 8);
    GtkWidget *page_appearance = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_appearance), 8);

    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page_tasks, gtk_label_new("Tasks"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page_appearance, gtk_label_new("Appearance"));

    /* --- Calendar --- */
    GtkWidget *cal = gtk_calendar_new();
    ctx->cal = GTK_CALENDAR(cal);
    gtk_box_pack_start(GTK_BOX(page_tasks), cal, FALSE, FALSE, 0);

    /* --- Selected-day tasks --- */
    ctx->day_label = gtk_label_new("");
    gtk_widget_set_halign(ctx->day_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->day_label, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 80);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 140);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    ctx->list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scroll), ctx->list);
    gtk_box_pack_start(GTK_BOX(page_tasks), scroll, TRUE, TRUE, 0);

    GtkWidget *addrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    ctx->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->entry), "New task for this day…");
    gtk_widget_set_hexpand(ctx->entry, TRUE);
    g_signal_connect(ctx->entry, "activate", G_CALLBACK(on_add), ctx);
    GtkWidget *add = gtk_button_new_with_label("Add");
    g_signal_connect(add, "clicked", G_CALLBACK(on_add), ctx);
    gtk_box_pack_start(GTK_BOX(addrow), ctx->entry, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(addrow), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page_tasks), addrow, FALSE, FALSE, 0);

    /* --- Appearance --- */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(page_appearance), grid, FALSE, FALSE, 0);

    Settings *s = &app->settings;
    add_color_row(ctx, grid, 0, "Timeline & today", &s->accent);
    add_color_row(ctx, grid, 1, "Cat",              &s->cat);
    add_color_row(ctx, grid, 2, "Tasks",            &s->task);
    add_color_row(ctx, grid, 3, "Upcoming days",    &s->future);
    add_color_row(ctx, grid, 4, "Past days",        &s->past);
    add_color_row(ctx, grid, 5, "Completed",        &s->done);
    ctx->ncolors = 6;

    GtkWidget *reset = gtk_button_new_with_label("Reset to defaults");
    g_signal_connect(reset, "clicked", G_CALLBACK(on_reset), ctx);
    gtk_box_pack_end(GTK_BOX(root), reset, FALSE, FALSE, 4);

    /* Wire calendar updates and populate the initial day. */
    g_signal_connect(cal, "day-selected", G_CALLBACK(on_calendar_changed), ctx);
    g_signal_connect(cal, "month-changed", G_CALLBACK(on_calendar_changed), ctx);
    g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), app);

    mark_task_days(ctx);
    refresh_task_list(ctx);

    gtk_widget_show_all(win);
    place_near_widget(win);            /* position once natural size is known */
}
