#include "settings_window.h"
#include "settings.h"
#include "tasks.h"
#include "style.h"
#include "window.h"
#include "calwidget.h"

#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Per-window state shared between the calendar and task-list callbacks. */
typedef struct {
    App         *app;
    GtkWidget   *cal;        /* custom Cairo month calendar             */
    GtkWidget   *day_label;  /* selected-day header (friendly date)     */
    GtkWidget   *list;       /* vbox holding one row per task           */
    GtkWidget   *entry;      /* new-task entry for the selected day     */
    GtkWidget   *home_unfin; /* Home tab: pending tasks (Unfinished)    */
    GtkWidget   *home_done;  /* Home tab: completed tasks (Done)        */
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
    int y, m, d;
    cal_widget_get_selected(ctx->cal, &y, &m, &d);
    snprintf(out11, DATE_LEN, "%04d-%02d-%02d", y, m, d);
}

/* Repaint the calendar so its task dots reflect the current store. */
static void mark_task_days(Ctx *ctx) {
    cal_widget_refresh(ctx->cal);
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

/* Fill one Home list with tasks whose done-state matches `want_done`, grouped
 * into a card per day. Shows a centred icon + message when there are none. */
static void fill_grouped(Ctx *ctx, GtkWidget *box, gboolean want_done,
                         const char *icon, const char *empty_text) {
    clear_box(box);

    TaskStore *s = &ctx->app->store;
    DayTasks **days = g_new(DayTasks *, s->count > 0 ? s->count : 1);
    int m = 0;
    for (int i = 0; i < s->count; i++)
        if (s->days[i].count > 0)
            days[m++] = &s->days[i];
    qsort(days, m, sizeof(DayTasks *), cmp_day_ptr);

    gboolean any = FALSE;
    for (int i = 0; i < m; i++) {
        DayTasks *d = days[i];
        int match = 0;
        for (int j = 0; j < d->count; j++)
            if ((d->tasks[j].done ? TRUE : FALSE) == want_done) match++;
        if (!match)
            continue;
        any = TRUE;

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
            if ((d->tasks[j].done ? TRUE : FALSE) == want_done)
                gtk_box_pack_start(GTK_BOX(card),
                                   make_task_row(ctx, d->date, &d->tasks[j]), FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(box), card, FALSE, FALSE, 0);
    }
    g_free(days);

    if (!any) {
        GtkWidget *e = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_valign(e, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(e, TRUE);
        GtkWidget *img = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 44);
        style_class(img, "tp-emptyicon");
        gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
        GtkWidget *lbl = gtk_label_new(empty_text);
        style_class(lbl, "tp-empty");
        gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(e), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(e), lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), e, TRUE, TRUE, 0);
    }
    gtk_widget_show_all(box);
}

static void build_home_list(Ctx *ctx) {
    if (!ctx->home_unfin)
        return;
    fill_grouped(ctx, ctx->home_unfin, FALSE, "view-list-symbolic",
                 "No unfinished tasks");
    fill_grouped(ctx, ctx->home_done, TRUE, "object-select-symbolic",
                 "Finished tasks will go here");
}

static void on_calendar_changed(int y, int m, int d, gpointer ud) {
    (void)y; (void)m; (void)d;
    Ctx *ctx = ud;
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

/* A vertical icon-over-label widget for a left-rail button. */
static GtkWidget *nav_label(const char *icon, const char *text) {
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *img = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 14);
    GtkWidget *lbl = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(b), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), lbl, FALSE, FALSE, 0);
    gtk_widget_set_halign(b, GTK_ALIGN_CENTER);
    gtk_widget_show_all(b);
    return b;
}

/* Switch the content stack to the page named on the toggled rail button. */
static void on_nav_toggled(GtkToggleButton *b, gpointer ud) {
    (void)ud;
    if (!gtk_toggle_button_get_active(b))
        return;
    GtkWidget *stack = g_object_get_data(G_OBJECT(b), "stack");
    const char *page = g_object_get_data(G_OBJECT(b), "page");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), page);
}

/* Add one rail button (radio-grouped, drawn as a pill) bound to a stack page. */
static GtkWidget *add_nav(GtkWidget *rail, GtkWidget **group, GtkWidget *stack,
                          const char *page, const char *icon, const char *text) {
    GtkWidget *b = gtk_radio_button_new_from_widget(
                       *group ? GTK_RADIO_BUTTON(*group) : NULL);
    if (!*group)
        *group = b;
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), FALSE);   /* no radio dot */
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(b), nav_label(icon, text));
    style_class(b, "nav-btn");
    g_object_set_data(G_OBJECT(b), "stack", stack);
    g_object_set_data_full(G_OBJECT(b), "page", g_strdup(page), g_free);
    g_signal_connect(b, "toggled", G_CALLBACK(on_nav_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(rail), b, FALSE, FALSE, 0);
    return b;
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
    /* Fixed size: tiling compositors (Hyprland) auto-float fixed-size windows
     * but tile resizable ones fullscreen — so keep it non-resizable to float. */
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_NONE);
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 440);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key_press), NULL);

    Ctx *ctx = g_new0(Ctx, 1);
    ctx->app = app;
    g_object_set_data_full(G_OBJECT(win), "ctx", ctx, g_free);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    style_class(root, "settings");
    style_class(root, "taskpanel");
    style_class(root, "tp-box");
    gtk_widget_set_size_request(root, 470, -1);   /* wide box with a left rail */
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

    /* Body: a vertically-centred left rail next to a content stack. */
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(root), body, TRUE, TRUE, 0);

    GtkWidget *rail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(rail, GTK_ALIGN_CENTER);
    GtkWidget *main_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(main_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_box_pack_start(GTK_BOX(body), rail, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), main_stack, TRUE, TRUE, 0);

    GtkWidget *page_home = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_home), 8);
    GtkWidget *page_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(page_tasks), 8);
    GtkWidget *page_appearance = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page_appearance), 8);

    /* Each page scrolls internally; a sensible minimum keeps the default size
     * compact, while vexpand lets pages fill the window when it's enlarged. */
    const int PAGE_H = 300;
    GtkWidget *tasks_vp = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tasks_vp),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(tasks_vp), PAGE_H);
    gtk_widget_set_vexpand(tasks_vp, TRUE);
    gtk_container_add(GTK_CONTAINER(tasks_vp), page_tasks);

    GtkWidget *appear_vp = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(appear_vp),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(appear_vp), PAGE_H);
    gtk_widget_set_vexpand(appear_vp, TRUE);
    gtk_container_add(GTK_CONTAINER(appear_vp), page_appearance);

    gtk_stack_add_named(GTK_STACK(main_stack), page_home, "home");
    gtk_stack_add_named(GTK_STACK(main_stack), tasks_vp, "tasks");
    gtk_stack_add_named(GTK_STACK(main_stack), appear_vp, "appearance");

    GtkWidget *grp = NULL;
    GtkWidget *nav_home = add_nav(rail, &grp, main_stack, "home",
                                  "view-list-symbolic", "Home");
    add_nav(rail, &grp, main_stack, "tasks", "x-office-calendar-symbolic", "Tasks");
    add_nav(rail, &grp, main_stack, "appearance",
            "applications-graphics-symbolic", "Appearance");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nav_home), TRUE);
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "home");

    /* --- Home: Unfinished / Done tabs --- */
    GtkWidget *home_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(home_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    GtkWidget *uf_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(uf_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(uf_scroll), 260);
    gtk_widget_set_vexpand(uf_scroll, TRUE);
    ctx->home_unfin = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(uf_scroll), ctx->home_unfin);

    GtkWidget *dn_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dn_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(dn_scroll), 260);
    gtk_widget_set_vexpand(dn_scroll, TRUE);
    ctx->home_done = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(dn_scroll), ctx->home_done);

    gtk_stack_add_titled(GTK_STACK(home_stack), uf_scroll, "unfinished", "Unfinished");
    gtk_stack_add_titled(GTK_STACK(home_stack), dn_scroll, "done", "Done");

    GtkWidget *sw = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(sw), GTK_STACK(home_stack));
    gtk_widget_set_halign(sw, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(page_home), sw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page_home), home_stack, TRUE, TRUE, 0);

    /* --- Tasks: calendar + selected-day editor --- */
    GtkWidget *month = gtk_label_new("THIS MONTH");
    style_class(month, "tp-sub");
    gtk_widget_set_halign(month, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page_tasks), month, FALSE, FALSE, 0);

    ctx->cal = cal_widget_new(app, on_calendar_changed, ctx);
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->cal, FALSE, FALSE, 0);

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

    /* Task rows pack directly into the page; the page itself scrolls. */
    ctx->list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(page_tasks), ctx->list, FALSE, FALSE, 0);

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
    gtk_widget_set_margin_top(reset, 6);
    g_signal_connect(reset, "clicked", G_CALLBACK(on_reset), ctx);
    gtk_box_pack_start(GTK_BOX(page_appearance), reset, FALSE, FALSE, 0);

    g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), app);

    mark_task_days(ctx);
    refresh_task_list(ctx);
    build_home_list(ctx);

    gtk_widget_show_all(win);
    place_near_widget(win);            /* position once natural size is known */
}
