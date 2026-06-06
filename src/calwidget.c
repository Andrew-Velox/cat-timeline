#include "calwidget.h"
#include "tasks.h"
#include <math.h>
#include <string.h>
#include <time.h>

/* ---- layout constants -------------------------------------------------- */
#define CAL_PAD   10
#define CAL_HEAD  30      /* month title row             */
#define CAL_WK    22      /* weekday-letter row          */
#define CAL_CH    30.0    /* day-cell height             */
#define CAL_W     252     /* widget width                */
#define CAL_ROWS  6

#define UI_ACCENT 0x3da9fc

typedef struct {
    App        *app;
    int         view_y, view_m;     /* displayed month (m 1-12) */
    int         sel_y, sel_m, sel_d;
    CalSelectCb cb;
    gpointer    ud;
} Cal;

/* ---- date maths -------------------------------------------------------- */

static int days_in_month(int y, int m) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return dm[m - 1];
}

static int first_wday(int y, int m) {
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = 1;
    mktime(&t);
    return t.tm_wday;               /* 0 = Sunday */
}

static void today_ymd(int *y, int *m, int *d) {
    time_t n = time(NULL);
    struct tm t = *localtime(&n);
    *y = t.tm_year + 1900; *m = t.tm_mon + 1; *d = t.tm_mday;
}

static gboolean day_has_tasks(Cal *c, int y, int m, int d) {
    char date[DATE_LEN];
    snprintf(date, sizeof date, "%04d-%02d-%02d", y, m, d);
    DayTasks *dt = tasks_find_day(&c->app->store, date);
    return dt && dt->count > 0;
}

/* ---- drawing ----------------------------------------------------------- */

static void draw_centered(cairo_t *cr, const char *s, double cx, double cy) {
    cairo_text_extents_t ext;
    cairo_text_extents(cr, s, &ext);
    cairo_move_to(cr, cx - ext.width / 2.0 - ext.x_bearing,
                      cy - ext.height / 2.0 - ext.y_bearing);
    cairo_show_text(cr, s);
}

static gboolean cal_draw(GtkWidget *w, cairo_t *cr, gpointer ud) {
    Cal *c = ud;
    int W = gtk_widget_get_allocated_width(w);
    int H = gtk_widget_get_allocated_height(w);

    /* Card background. */
    double rr = 14.0, x = 0.5, y = 0.5, ww = W - 1, hh = H - 1;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + ww - rr, y + rr,      rr, -M_PI / 2, 0);
    cairo_arc(cr, x + ww - rr, y + hh - rr, rr, 0, M_PI / 2);
    cairo_arc(cr, x + rr,      y + hh - rr, rr, M_PI / 2, M_PI);
    cairo_arc(cr, x + rr,      y + rr,      rr, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
    set_hex(cr, 0x16181d, 1.0);
    cairo_fill_preserve(cr);
    set_hex(cr, 0x23262e, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    /* Title: "June 2026" centred, with ‹ › arrows. */
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year = c->view_y - 1900; t.tm_mon = c->view_m - 1; t.tm_mday = 1;
    mktime(&t);
    char title[40];
    strftime(title, sizeof title, "%B %Y", &t);
    cairo_set_font_size(cr, 13);
    set_hex(cr, 0xe9eaed, 1.0);
    draw_centered(cr, title, W / 2.0, CAL_PAD + CAL_HEAD / 2.0);

    cairo_set_font_size(cr, 16);
    set_hex(cr, 0x8b9099, 1.0);
    draw_centered(cr, "\xE2\x80\xB9", CAL_PAD + 12, CAL_PAD + CAL_HEAD / 2.0);   /* ‹ */
    draw_centered(cr, "\xE2\x80\xBA", W - CAL_PAD - 12, CAL_PAD + CAL_HEAD / 2.0);/* › */

    /* Weekday letters. */
    double cw = (W - 2 * CAL_PAD) / 7.0;
    const char *wd[] = {"S", "M", "T", "W", "T", "F", "S"};
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    set_hex(cr, 0x6b7280, 1.0);
    for (int i = 0; i < 7; i++)
        draw_centered(cr, wd[i], CAL_PAD + cw * (i + 0.5), CAL_PAD + CAL_HEAD + CAL_WK / 2.0);

    /* Day grid. */
    int gx = CAL_PAD, gy = CAL_PAD + CAL_HEAD + CAL_WK;
    int fw = first_wday(c->view_y, c->view_m);
    int nd = days_in_month(c->view_y, c->view_m);
    int ty, tm, td;
    today_ymd(&ty, &tm, &td);

    for (int d = 1; d <= nd; d++) {
        int idx = fw + d - 1;
        int col = idx % 7, row = idx / 7;
        double cx = gx + cw * (col + 0.5);
        double cy = gy + CAL_CH * (row + 0.5);

        gboolean is_sel = (c->view_y == c->sel_y && c->view_m == c->sel_m && d == c->sel_d);
        gboolean is_today = (c->view_y == ty && c->view_m == tm && d == td);
        double rad = 13.0;

        if (is_sel) {
            set_hex(cr, UI_ACCENT, 1.0);
            cairo_arc(cr, cx, cy, rad, 0, 2 * M_PI);
            cairo_fill(cr);
        } else if (is_today) {
            set_hex(cr, UI_ACCENT, 1.0);
            cairo_set_line_width(cr, 1.4);
            cairo_arc(cr, cx, cy, rad, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        char num[12];
        snprintf(num, sizeof num, "%d", d);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                               is_sel || is_today ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        if (is_sel)
            set_hex(cr, 0x08151f, 1.0);
        else if (is_today)
            set_hex(cr, UI_ACCENT, 1.0);
        else
            set_hex(cr, 0xe9eaed, 1.0);
        draw_centered(cr, num, cx, cy);

        /* Task dot under the number (hidden when selected to avoid clutter). */
        if (!is_sel && day_has_tasks(c, c->view_y, c->view_m, d)) {
            set_hex(cr, c->app->settings.task, 1.0);
            cairo_arc(cr, cx, cy + rad - 1.0, 1.6, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    return FALSE;
}

/* ---- input ------------------------------------------------------------- */

static gboolean cal_click(GtkWidget *w, GdkEventButton *e, gpointer ud) {
    Cal *c = ud;
    if (e->type != GDK_BUTTON_PRESS || e->button != GDK_BUTTON_PRIMARY)
        return FALSE;
    int W = gtk_widget_get_allocated_width(w);

    /* Title row: month navigation. */
    if (e->y >= CAL_PAD && e->y <= CAL_PAD + CAL_HEAD) {
        if (e->x <= CAL_PAD + 34) {
            if (--c->view_m < 1) { c->view_m = 12; c->view_y--; }
        } else if (e->x >= W - CAL_PAD - 34) {
            if (++c->view_m > 12) { c->view_m = 1; c->view_y++; }
        }
        gtk_widget_queue_draw(w);
        return TRUE;
    }

    /* Day grid: select a day. */
    int gx = CAL_PAD, gy = CAL_PAD + CAL_HEAD + CAL_WK;
    double cw = (W - 2 * CAL_PAD) / 7.0;
    if (e->y < gy)
        return TRUE;
    int col = (int)((e->x - gx) / cw);
    int row = (int)((e->y - gy) / CAL_CH);
    if (col < 0 || col > 6 || row < 0 || row >= CAL_ROWS)
        return TRUE;
    int day = row * 7 + col - first_wday(c->view_y, c->view_m) + 1;
    if (day < 1 || day > days_in_month(c->view_y, c->view_m))
        return TRUE;

    c->sel_y = c->view_y; c->sel_m = c->view_m; c->sel_d = day;
    gtk_widget_queue_draw(w);
    if (c->cb)
        c->cb(c->sel_y, c->sel_m, c->sel_d, c->ud);
    return TRUE;
}

/* ---- public ------------------------------------------------------------ */

GtkWidget *cal_widget_new(App *app, CalSelectCb cb, gpointer ud) {
    Cal *c = g_new0(Cal, 1);
    c->app = app; c->cb = cb; c->ud = ud;
    today_ymd(&c->sel_y, &c->sel_m, &c->sel_d);
    c->view_y = c->sel_y; c->view_m = c->sel_m;

    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, CAL_W,
        CAL_PAD * 2 + CAL_HEAD + CAL_WK + (int)(CAL_CH * CAL_ROWS));
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    g_object_set_data_full(G_OBJECT(area), "cal", c, g_free);
    g_signal_connect(area, "draw", G_CALLBACK(cal_draw), c);
    g_signal_connect(area, "button-press-event", G_CALLBACK(cal_click), c);
    return area;
}

void cal_widget_get_selected(GtkWidget *w, int *y, int *m, int *d) {
    Cal *c = g_object_get_data(G_OBJECT(w), "cal");
    *y = c->sel_y; *m = c->sel_m; *d = c->sel_d;
}

void cal_widget_refresh(GtkWidget *w) {
    gtk_widget_queue_draw(w);
}
