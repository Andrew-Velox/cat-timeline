#include "circle.h"
#include "tasks.h"
#include <math.h>
#include <string.h>

/* Fixed angles of the two portals (the open gap at the top of the ring). */
static double portal_future_ang(void) { return BASE_ANG - (CIRC_FUTURE + 0.5) * DAY_ARC; }
static double portal_past_ang(void)   { return BASE_ANG + (CIRC_PAST + 0.5) * DAY_ARC; }

/* Angle of a day's dot, scrolling with the elapsed fraction of the day. */
static double dot_ang(int offset) {
    return BASE_ANG - (offset - day_fraction()) * DAY_ARC;
}

void circle_dot_pos(int offset, double *x, double *y) {
    double a = dot_ang(offset);
    *x = RING_CX + RING_R * cos(a);
    *y = RING_CY + RING_R * sin(a);
}

void circle_cat_anchor(double *cx, double *feet_y) {
    *cx = RING_CX;
    *feet_y = RING_CY + RING_R;          /* bottom of the ring = "now" */
}

/* Fade a dot/ticks out as the angle nears (or passes) a portal, so days vanish
 * into the portal instead of sliding past the gap. 1 = shown, 0 = at portal. */
static double ang_fade(double a) {
    double tf = portal_future_ang(), tp = portal_past_ang();
    if (a < tf || a > tp)
        return 0.0;
    double d = a - tf;
    double dr = tp - a;
    if (dr < d) d = dr;
    double fade = DAY_ARC * 0.4;
    if (d <= 0.0) return 0.0;
    if (d >= fade) return 1.0;
    return d / fade;
}

gboolean circle_hit_dot(double x, double y, int *out_off) {
    double best = 1e9;
    int bo = HOVER_NONE;
    for (int o = -CIRC_PAST; o <= CIRC_FUTURE; o++) {
        double dx, dy;
        circle_dot_pos(o, &dx, &dy);
        double d = hypot(x - dx, y - dy);
        if (d < best) { best = d; bo = o; }
    }
    if (bo != HOVER_NONE && best <= 13.0) {
        *out_off = bo;
        return TRUE;
    }
    return FALSE;
}

/* The glowing ring arc (open at the top), drawn as layered strokes for glow. */
static void draw_ring(App *app, cairo_t *cr) {
    double r, g, b;
    hex_rgb(app->settings.accent, &r, &g, &b);
    const double widths[] = {7.0, 3.5, 1.6};
    const double alphas[] = {0.16, 0.4, 1.0};
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < 3; i++) {
        cairo_new_path(cr);
        cairo_set_source_rgba(cr, r, g, b, alphas[i]);
        cairo_set_line_width(cr, widths[i]);
        cairo_arc(cr, RING_CX, RING_CY, RING_R, portal_future_ang(), portal_past_ang());
        cairo_stroke(cr);
    }
}

/* A sparkling portal at one end of the gap (additive, gently breathing). */
static void draw_portal(App *app, cairo_t *cr, double a) {
    double px = RING_CX + RING_R * cos(a);
    double py = RING_CY + RING_R * sin(a);
    double t = app->tick / 10.0;
    double pulse = 0.5 + 0.5 * sin(t * 2.2);

    double r, g, b;
    hex_rgb(hex_lighten(app->settings.portal, 0.35), &r, &g, &b);
    double rad = 6.0 + 2.0 * pulse;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
    cairo_pattern_t *grad = cairo_pattern_create_radial(px, py, 0, px, py, rad);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, r, g, b, 0.70 + 0.2 * pulse);
    cairo_pattern_add_color_stop_rgba(grad, 0.4, r, g, b, 0.30);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, r, g, b, 0.0);
    cairo_set_source(cr, grad);
    cairo_arc(cr, px, py, rad, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    set_hex(cr, hex_lighten(app->settings.portal, 0.6), 0.9);
    cairo_arc(cr, px, py, 1.6, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
}

/* Task ticks fanned radially outward from a day's dot. */
static void draw_ticks(App *app, cairo_t *cr, DayTasks *d, int offset,
                       double a, double fade) {
    if (!d || d->count == 0)
        return;
    int shown = d->count > 6 ? 6 : d->count;
    const double base = RING_R + 4.0;    /* inner end, just outside the ring */
    const double h = 7.0;                /* tick length                      */
    const double da = 4.5 * M_PI / 180.0;/* angular gap between ticks         */
    double a0 = a - (shown - 1) * da / 2.0;

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < shown; i++) {
        double ai = a0 + i * da;
        unsigned int col;
        double glow = 0.0;
        if (d->tasks[i].done) {
            col = app->settings.done;
        } else if (offset < 0) {
            col = app->settings.past;
        } else if (offset == 0) {
            col = hex_lighten(app->settings.task, 0.2);
        } else {
            col = app->settings.task;
            glow = 1.0;
        }
        double x1 = RING_CX + base * cos(ai),       y1 = RING_CY + base * sin(ai);
        double x2 = RING_CX + (base + h) * cos(ai), y2 = RING_CY + (base + h) * sin(ai);
        if (glow > 0.0) {
            set_hex(cr, col, 0.25 * fade);
            cairo_set_line_width(cr, 5.0);
            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);
        }
        set_hex(cr, col, fade);
        cairo_set_line_width(cr, 2.2);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }

    if (d->count > 6) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d", d->count - 6);
        double lx = RING_CX + (base + h + 7.0) * cos(a);
        double ly = RING_CY + (base + h + 7.0) * sin(a);
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 8);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        set_hex(cr, hex_lighten(app->settings.task, 0.2), 0.9 * fade);
        cairo_move_to(cr, lx - ext.width / 2.0, ly + ext.height / 2.0);
        cairo_show_text(cr, buf);
    }
}

/* One day dot, with its state colour and a day-of-month number inside it. */
static void draw_dot(App *app, cairo_t *cr, int offset, double a, double fade) {
    double x = RING_CX + RING_R * cos(a);
    double y = RING_CY + RING_R * sin(a);

    gboolean hovered = app->has_hover && app->hover_offset == offset;
    unsigned int col;
    double r = 3.0, glow = 0.0;
    if (offset == 0) {
        col = app->settings.accent; r = 4.5; glow = 1.0;
    } else if (offset < 0) {
        col = app->settings.past;
    } else {
        col = app->settings.future;
    }
    if (hovered) {
        col = hex_lighten(col, 0.4);
        r += 1.5;
        glow = 1.0;
    }

    if (glow > 0.0) {
        set_hex(cr, col, 0.25 * fade);
        cairo_arc(cr, x, y, r + 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    set_hex(cr, col, fade);
    cairo_arc(cr, x, y, r, 0, 2 * M_PI);
    cairo_fill(cr);

    /* Day-of-month number, set just inside the ring toward the centre. */
    char date[DATE_LEN];
    date_for_offset(offset, date);
    char num[3] = { date[8], date[9], 0 };
    if (num[0] == '0') { num[0] = num[1]; num[1] = 0; }

    double lr = RING_R - 13.0;
    double lx = RING_CX + lr * cos(a);
    double ly = RING_CY + lr * sin(a);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, num, &ext);
    set_hex(cr, offset == 0 ? app->settings.accent : 0xffffff, (offset == 0 ? 1.0 : 0.75) * fade);
    cairo_move_to(cr, lx - ext.width / 2.0 - ext.x_bearing, ly - ext.height / 2.0 - ext.y_bearing);
    cairo_show_text(cr, num);
}

void circle_draw(App *app, cairo_t *cr) {
    draw_ring(app, cr);
    draw_portal(app, cr, portal_future_ang());
    draw_portal(app, cr, portal_past_ang());

    for (int off = -CIRC_PAST - 1; off <= CIRC_FUTURE + 1; off++) {
        double a = dot_ang(off);
        double fade = ang_fade(a);
        if (fade <= 0.0)
            continue;
        char date[DATE_LEN];
        date_for_offset(off, date);
        DayTasks *d = tasks_find_day(&app->store, date);
        draw_dot(app, cr, off, a, fade);
        draw_ticks(app, cr, d, off, a, fade);
    }
}
