#include "settings_window.h"
#include "settings.h"
#include "tasks.h"
#include "style.h"
#include "window.h"

#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Per-window state shared between the calendar and task-list callbacks. */
typedef struct {
    App         *app;
    GtkCalendar *cal;        /* month view, marks days that have tasks  */
    GtkWidget   *day_label;  /* selected-day header (friendly date)     */
    GtkWidget   *list;       /* vbox holding one row per task           */
    GtkWidget   *entry;      /* new-task entry for the selected day     */
    GtkWidget   *home_list;  /* Home tab: all tasks, grouped by day     */
    GtkWidget   *home_count; /* Home tab: "N pending tasks" subtitle    */
    GtkWidget   *day_count;  /* Tasks tab: "X of Y done" footer         */
    GtkWidget   *prog;       /* Tasks tab: completion progress bar      */

    GtkWidget    *cbtn[7];   /* colour buttons (for Reset to refresh)   */
    unsigned int *cfield[7]; /* the Settings field each button edits    */
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

/* TRUE when the "YYYY-MM-DD" string is today's date. */
static gboolean date_is_today(const char *date) {
    char today[DATE_LEN];
    date_for_offset(0, today);
    return strcmp(date, today) == 0;
}

/* Pretty form of a "YYYY-MM-DD" string: "Sat, Jun 6" (single-spaced). */
static void date_pretty(const char *date, char *out, int n) {
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) == 3) {
        struct tm tm;
        memset(&tm, 0, sizeof tm);
        tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d;
        mktime(&tm);
        char buf[32];
        if (strftime(buf, sizeof buf, "%a, %b %e", &tm) > 0) {
            char *dbl = strstr(buf, "  ");      /* %e pads single digits */
            if (dbl) memmove(dbl, dbl + 1, strlen(dbl));
            g_strlcpy(out, buf, n);
            return;
        }
    }
    g_strlcpy(out, date, n);
}

/* ---- task rows (shared by the Tasks tab and the Home tab) -------------- */

static void refresh_task_list(Ctx *ctx);
static void build_home_list(Ctx *ctx);

/* Rebuild every view after a change: calendar marks, the day list, the Home
 * list, and the widget itself. */
static void refresh_all(Ctx *ctx) {
    mark_task_days(ctx);
    refresh_task_list(ctx);
    build_home_list(ctx);
    gtk_widget_queue_draw(ctx->app->area);
}

static void on_toggle(GtkButton *b, gpointer ud) {
    Ctx *ctx = ud;
    tasks_toggle(&ctx->app->store, g_object_get_data(G_OBJECT(b), "task-date"),
                 g_object_get_data(G_OBJECT(b), "task-id"));
    refresh_all(ctx);
}

static void on_delete(GtkButton *b, gpointer ud) {
    Ctx *ctx = ud;
    tasks_delete(&ctx->app->store, g_object_get_data(G_OBJECT(b), "task-date"),
                 g_object_get_data(G_OBJECT(b), "task-id"));
    refresh_all(ctx);
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
    refresh_all(ctx);
}

/* Build one task row: a ○/✓ toggle (text struck through when done) + × delete.
 * The owning day is stamped on the buttons so a row works from any list. */
static GtkWidget *make_task_row(Ctx *ctx, const char *date, Task *t) {
    char task_hex[8], done_hex[8];
    style_hex(ctx->app->settings.task, task_hex);
    style_hex(ctx->app->settings.done, done_hex);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    char *esc = g_markup_escape_text(t->text, -1);
    char markup[640];
    if (t->done)
        g_snprintf(markup, sizeof(markup),
            "<span foreground='%s' size='large'>\xE2\x9C\x93</span>  "
            "<span foreground='#6b7280' strikethrough='true'>%s</span>",
            done_hex, esc);
    else
        g_snprintf(markup, sizeof(markup),
            "<span foreground='%s' size='large'>\xE2\x97\x8B</span>  "
            "<span foreground='#e9eaed'>%s</span>",
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
    g_object_set_data_full(G_OBJECT(toggle), "task-date", g_strdup(date), g_free);
    g_signal_connect(toggle, "clicked", G_CALLBACK(on_toggle), ctx);

    GtkWidget *del = gtk_button_new_with_label("\xE2\x9C\x95");
    gtk_button_set_relief(GTK_BUTTON(del), GTK_RELIEF_NONE);
    style_class(del, "tp-del");
    g_object_set_data_full(G_OBJECT(del), "task-id", g_strdup(t->id), g_free);
    g_object_set_data_full(G_OBJECT(del), "task-date", g_strdup(date), g_free);
    g_signal_connect(del, "clicked", G_CALLBACK(on_delete), ctx);

    gtk_box_pack_start(GTK_BOX(row), toggle, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(row), del, FALSE, FALSE, 0);
    return row;
}

static void clear_box(GtkWidget *box) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
}

/* ---- Tasks tab: the selected calendar day ------------------------------ */

static void refresh_task_list(Ctx *ctx) {
    char date[DATE_LEN];
    selected_date(ctx, date);

    DayTasks *d = tasks_find_day(&ctx->app->store, date);
    int nt = d ? d->count : 0, nd = 0;
    for (int i = 0; i < nt; i++)
        if (d->tasks[i].done) nd++;

    char nice[32];
    date_pretty(date, nice, sizeof nice);
    gtk_label_set_text(GTK_LABEL(ctx->day_label), nice);

    char tally[48];
    if (nt)
        g_snprintf(tally, sizeof tally, "%d of %d done", nd, nt);
    else
        g_strlcpy(tally, "Nothing planned yet", sizeof tally);
    gtk_label_set_text(GTK_LABEL(ctx->day_count), tally);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->prog),
                                  nt ? (double)nd / nt : 0.0);
    gtk_widget_set_visible(ctx->prog, nt > 0);

    clear_box(ctx->list);
    if (nt == 0) {
        GtkWidget *empty = gtk_label_new("Nothing planned — add a task below.");
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        style_class(empty, "tp-empty");
        gtk_box_pack_start(GTK_BOX(ctx->list), empty, FALSE, FALSE, 2);
    } else {
        for (int i = 0; i < nt; i++)
            gtk_box_pack_start(GTK_BOX(ctx->list),
                               make_task_row(ctx, date, &d->tasks[i]), FALSE, FALSE, 0);
    }
    gtk_widget_show_all(ctx->list);
}

/* ---- Home tab: every task, grouped by day in date order ---------------- */

static int cmp_day_ptr(const void *a, const void *b) {
    const DayTasks *const *pa = a, *const *pb = b;
    return strcmp((*pa)->date, (*pb)->date);
}

static void build_home_list(Ctx *ctx) {
    if (!ctx->home_list)
        return;
    clear_box(ctx->home_list);

    TaskStore *s = &ctx->app->store;
    DayTasks **days = g_new(DayTasks *, s->count > 0 ? s->count : 1);
    int m = 0, pending = 0;
    for (int i = 0; i < s->count; i++) {
        if (s->days[i].count > 0)
            days[m++] = &s->days[i];
        for (int j = 0; j < s->days[i].count; j++)
            if (!s->days[i].tasks[j].done) pending++;
    }
    qsort(days, m, sizeof(DayTasks *), cmp_day_ptr);

    char sub[40];
    g_snprintf(sub, sizeof sub, "%d pending task%s", pending, pending == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(ctx->home_count), sub);

    if (m == 0) {
        GtkWidget *empty = gtk_label_new("No tasks yet.\nAdd some from the Tasks tab.");
        gtk_label_set_justify(GTK_LABEL(empty), GTK_JUSTIFY_CENTER);
        gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(empty, 24);
        style_class(empty, "tp-empty");
        gtk_box_pack_start(GTK_BOX(ctx->home_list), empty, FALSE, FALSE, 0);
    } else {
        for (int i = 0; i < m; i++) {
            DayTasks *d = days[i];

            /* One rounded card per day: an uppercase header then its rows. */
            GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
            style_class(card, "tp-card");

            char nice[32], head[48];
            date_pretty(d->date, nice, sizeof nice);
            if (date_is_today(d->date))
                g_snprintf(head, sizeof head, "TODAY · %s", nice);
            else
                g_strlcpy(head, nice, sizeof head);
            char *up = g_ascii_strup(head, -1);
            GtkWidget *hdr = gtk_label_new(up);
            g_free(up);
            gtk_widget_set_halign(hdr, GTK_ALIGN_START);
            style_class(hdr, "tp-date");
            gtk_box_pack_start(GTK_BOX(card), hdr, FALSE, FALSE, 0);

            for (int j = 0; j < d->count; j++)
                gtk_box_pack_start(GTK_BOX(card),
                                   make_task_row(ctx, d->date, &d->tasks[j]), FALSE, FALSE, 0);

            gtk_box_pack_start(GTK_BOX(ctx->home_list), card, FALSE, FALSE, 0);
        }
    }
    g_free(days);
    gtk_widget_show_all(ctx->home_list);
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

/* Switch the widget shape (line strip vs. circular ring) and re-lay-out. */
static void on_layout_toggled(GtkToggleButton *b, gpointer ud) {
    if (!gtk_toggle_button_get_active(b))
        return;                          /* only act on the newly-active one */
    Ctx *ctx = ud;
    ctx->app->settings.layout = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "layout"));
    settings_save(&ctx->app->settings);
    window_apply_layout(ctx->app);
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

/* Close button dismisses the window. */
static void on_close_clicked(GtkWidget *w, gpointer ud) {
    (void)w;
    App *app = ud;
    if (app->settings_win)
        gtk_widget_destroy(app->settings_win);
}

/* Escape closes the window. */
static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer ud) {
    (void)ud;
    if (e->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(w);
        return TRUE;
    }
    return FALSE;
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

    style_ensure(app);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->settings_win = win;
    gtk_window_set_title(GTK_WINDOW(win), "cat-timeline — Settings");
    /* Compact floating panel that hovers above other apps. The dialog/utility
     * hints + non-resizable keep tiling compositors (Hyprland) from tiling it. */
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);   /* own header instead */
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_NONE);
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 360);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key_press), NULL);

    Ctx *ctx = g_new0(Ctx, 1);
    ctx->app = app;
    g_object_set_data_full(G_OBJECT(win), "ctx", ctx, g_free);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    style_class(root, "settings");
    style_class(root, "taskpanel");
    style_class(root, "tp-box");
    gtk_container_add(GTK_CONTAINER(win), root);

    /* Header: title (left) + × close (right). */
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *htitle = gtk_label_new("Settings");
    style_class(htitle, "tp-title");
    gtk_widget_set_halign(htitle, GTK_ALIGN_START);
    GtkWidget *hclose = gtk_button_new_with_label("\xE2\x9C\x95");
    style_class(hclose, "tp-close");
    gtk_widget_set_valign(hclose, GTK_ALIGN_CENTER);
    g_signal_connect(hclose, "clicked", G_CALLBACK(on_close_clicked), app);
    gtk_box_pack_start(GTK_BOX(head), htitle, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(head), hclose, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), head, FALSE, FALSE, 0);

    GtkWidget *tabs = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(root), tabs, TRUE, TRUE, 0);

    GtkWidget *page_home = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_home), 8);
    GtkWidget *page_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(page_tasks), 8);
    GtkWidget *page_appearance = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_appearance), 8);

    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page_home, gtk_label_new("Home"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page_tasks, gtk_label_new("Tasks"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page_appearance, gtk_label_new("Appearance"));

    /* --- Home: all tasks, grouped by day in date order --- */
    ctx->home_count = gtk_label_new("");
    style_class(ctx->home_count, "tp-sub");
    gtk_widget_set_halign(ctx->home_count, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_home), ctx->home_count, FALSE, FALSE, 0);

    GtkWidget *home_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(home_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(home_scroll), 240);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(home_scroll), 340);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(home_scroll), TRUE);
    ctx->home_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(home_scroll), ctx->home_list);
    gtk_box_pack_start(GTK_BOX(page_home), home_scroll, TRUE, TRUE, 0);

    /* --- Tasks: calendar + selected-day editor --- */
    GtkWidget *month = gtk_label_new("THIS MONTH");
    style_class(month, "tp-sub");
    gtk_widget_set_halign(month, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_tasks), month, FALSE, FALSE, 0);

    GtkWidget *cal = gtk_calendar_new();
    ctx->cal = GTK_CALENDAR(cal);
    gtk_calendar_set_display_options(GTK_CALENDAR(cal),
        GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES);
    gtk_box_pack_start(GTK_BOX(page_tasks), cal, FALSE, FALSE, 0);

    GtkWidget *seld = gtk_label_new("SELECTED DAY");
    style_class(seld, "tp-sub");
    gtk_widget_set_halign(seld, GTK_ALIGN_START);
    gtk_widget_set_margin_top(seld, 6);
    gtk_box_pack_start(GTK_BOX(page_tasks), seld, FALSE, FALSE, 0);

    /* Big friendly date for the selected day ("Sat, Jun 6"). */
    ctx->day_label = gtk_label_new("");
    style_class(ctx->day_label, "tp-title");
    gtk_widget_set_halign(ctx->day_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->day_label, FALSE, FALSE, 0);

    /* Composer: a clear entry + accent Add button. */
    GtkWidget *addrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    ctx->entry = gtk_entry_new();
    style_class(ctx->entry, "tp-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->entry), "Add a task…");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(ctx->entry),
        GTK_ENTRY_ICON_PRIMARY, "list-add-symbolic");
    gtk_widget_set_hexpand(ctx->entry, TRUE);
    g_signal_connect(ctx->entry, "activate", G_CALLBACK(on_add), ctx);
    GtkWidget *add = gtk_button_new_with_label("Add");
    style_class(add, "tp-add");
    g_signal_connect(add, "clicked", G_CALLBACK(on_add), ctx);
    gtk_box_pack_start(GTK_BOX(addrow), ctx->entry, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(addrow), add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page_tasks), addrow, FALSE, FALSE, 0);

    /* Completion tally ("1 of 2 done") + thin progress bar. */
    ctx->day_count = gtk_label_new("");
    style_class(ctx->day_count, "tp-foot");
    gtk_widget_set_halign(ctx->day_count, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->day_count, FALSE, FALSE, 0);

    ctx->prog = gtk_progress_bar_new();
    style_class(ctx->prog, "tp-prog");
    gtk_widget_set_no_show_all(ctx->prog, TRUE);   /* visibility set in refresh */
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->prog, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 70);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 150);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    ctx->list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(scroll), ctx->list);
    gtk_box_pack_start(GTK_BOX(page_tasks), scroll, TRUE, TRUE, 0);

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
    add_color_row(ctx, grid, 6, "Portals",          &s->portal);
    ctx->ncolors = 7;

    /* Shape chooser: line strip or circular ring. */
    GtkWidget *shape = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *slabel = gtk_label_new("Shape");
    gtk_widget_set_halign(slabel, GTK_ALIGN_START);
    gtk_widget_set_hexpand(slabel, TRUE);
    GtkWidget *r_line = gtk_radio_button_new_with_label(NULL, "Line");
    GtkWidget *r_circ = gtk_radio_button_new_with_label_from_widget(
                            GTK_RADIO_BUTTON(r_line), "Circle");
    g_object_set_data(G_OBJECT(r_line), "layout", GINT_TO_POINTER(LAYOUT_LINE));
    g_object_set_data(G_OBJECT(r_circ), "layout", GINT_TO_POINTER(LAYOUT_CIRCLE));
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(s->layout == LAYOUT_CIRCLE ? r_circ : r_line), TRUE);
    g_signal_connect(r_line, "toggled", G_CALLBACK(on_layout_toggled), ctx);
    g_signal_connect(r_circ, "toggled", G_CALLBACK(on_layout_toggled), ctx);
    gtk_box_pack_start(GTK_BOX(shape), slabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(shape), r_line, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(shape), r_circ, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page_appearance), shape, FALSE, FALSE, 6);

    GtkWidget *reset = gtk_button_new_with_label("Reset to defaults");
    style_class(reset, "tp-reset");
    g_signal_connect(reset, "clicked", G_CALLBACK(on_reset), ctx);
    gtk_box_pack_end(GTK_BOX(root), reset, FALSE, FALSE, 4);

    /* Wire calendar updates and populate the initial day. */
    g_signal_connect(cal, "day-selected", G_CALLBACK(on_calendar_changed), ctx);
    g_signal_connect(cal, "month-changed", G_CALLBACK(on_calendar_changed), ctx);
    g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), app);

    mark_task_days(ctx);
    refresh_task_list(ctx);
    build_home_list(ctx);

    gtk_widget_show_all(win);
    place_near_widget(win);            /* position once natural size is known */
}
