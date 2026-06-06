#include "timeline.h"
#include <math.h>

/* Screen x of a day's dot. The strip scrolls left as the day elapses, so the
 * dot sits on the cat (TODAY_X) at that day's midnight and drifts off by one
 * full slot over 24h — tomorrow's dot reaching the cat as today ends. */
double dot_x(int offset) {
    return TODAY_X + (offset - day_fraction()) * DOT_SPACING;
}

/* Day offset whose (scrolled) dot is horizontally closest to screen x. */
int nearest_offset(double x) {
    return (int)lround((x - TODAY_X) / DOT_SPACING + day_fraction());
}

#define PORTAL_X 10.0          /* portal centre, inset from each end */

/* Fade a dot out as it reaches a portal, so days vanish into the portal
 * instead of sliding past it. 1 = fully shown, 0 = at/beyond the portal. */
static double edge_fade(double cx) {
    const double fade = 9.0;
    double d = cx - PORTAL_X;                 /* distance from left portal  */
    double dr = (WIN_W - PORTAL_X) - cx;      /* distance from right portal */
    if (dr < d) d = dr;
    if (d <= 0.0) return 0.0;
    if (d >= fade) return 1.0;
    return d / fade;
}

/* Draw the accent-coloured horizontal line with edges that fade out. */
static void draw_line(App *app, cairo_t *cr) {
    double r, g, b;
    hex_rgb(app->settings.accent, &r, &g, &b);

    /* x-gradient: transparent at both ends, solid accent in the middle. */
    cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, WIN_W, 0);
    cairo_pattern_add_color_stop_rgba(grad, 0.00, r, g, b, 0.0);
    cairo_pattern_add_color_stop_rgba(grad, 0.18, r, g, b, 1.0);
    cairo_pattern_add_color_stop_rgba(grad, 0.82, r, g, b, 1.0);
    cairo_pattern_add_color_stop_rgba(grad, 1.00, r, g, b, 0.0);

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source(cr, grad);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, 0, LINE_Y);
    cairo_line_to(cr, WIN_W, LINE_Y);
    cairo_stroke(cr);

    cairo_pattern_destroy(grad);
}

/* A glowing, sparkling "portal" at one end of the timeline, centred at (px,
 * LINE_Y). Additive blending makes it luminous; app->tick (10 fps) animates a
 * gentle breathing glow plus twinkling motes that drift around the mouth. */
// static void draw_portal(App *app, cairo_t *cr, double px) {
//     double t = app->tick / 10.0;                  /* seconds */
//     double pulse = 0.5 + 0.5 * sin(t * 2.2);      /* 0..1 breathing */

//     double r, g, b;
//     hex_rgb(hex_lighten(app->settings.portal, 0.35), &r, &g, &b);

//     double ph = 15.0 + 5.0 * pulse;               /* mouth half-height */
//     double pw = 5.0 + 1.5 * pulse;                /* mouth half-width  */

//     cairo_save(cr);
//     cairo_set_operator(cr, CAIRO_OPERATOR_ADD);

//     /* Portal mouth: a soft vertical-ellipse glow (unit circle scaled). */
//     cairo_save(cr);
//     cairo_translate(cr, px, LINE_Y);
//     cairo_scale(cr, pw, ph);
//     cairo_pattern_t *grad = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
//     cairo_pattern_add_color_stop_rgba(grad, 0.0, r, g, b, 0.55 + 0.25 * pulse);
//     cairo_pattern_add_color_stop_rgba(grad, 0.45, r, g, b, 0.28);
//     cairo_pattern_add_color_stop_rgba(grad, 1.0, r, g, b, 0.0);
//     cairo_set_source(cr, grad);
//     cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
//     cairo_fill(cr);
//     cairo_pattern_destroy(grad);
//     cairo_restore(cr);

//     /* Bright vertical core streak. */
//     cairo_pattern_t *core = cairo_pattern_create_linear(px, LINE_Y - ph, px, LINE_Y + ph);
//     cairo_pattern_add_color_stop_rgba(core, 0.0, r, g, b, 0.0);
//     cairo_pattern_add_color_stop_rgba(core, 0.5, r, g, b, 0.75 + 0.2 * pulse);
//     cairo_pattern_add_color_stop_rgba(core, 1.0, r, g, b, 0.0);
//     cairo_set_source(cr, core);
//     cairo_set_line_width(cr, 1.6);
//     cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
//     cairo_move_to(cr, px, LINE_Y - ph * 0.7);
//     cairo_line_to(cr, px, LINE_Y + ph * 0.7);
//     cairo_stroke(cr);
//     cairo_pattern_destroy(core);

//     cairo_restore(cr);
// }

/* Draw the vertical task dashes (one per task) above a dot. */
static void draw_dashes(App *app, cairo_t *cr, DayTasks *d, int offset, double cx) {
    if (!d || d->count == 0)
        return;
    double fade = edge_fade(cx);
    if (fade <= 0.0)
        return;

    /* Only pending tasks are shown; completed ones drop off the widget. */
    int npend = 0;
    for (int i = 0; i < d->count; i++)
        if (!d->tasks[i].done) npend++;
    if (npend == 0)
        return;

    int shown = npend > 8 ? 8 : npend;
    const double w = 2.5;          /* dash width  */
    const double h = 8.0;          /* dash height */
    const double step = w + 4.0;   /* centre-to-centre spacing */
    double total = (shown - 1) * step;
    double start = cx - total / 2.0;
    double top = LINE_Y - 16.0;    /* bottom of the dash column */

    unsigned int col;
    double glow = 0.0;
    if (offset < 0) {
        col = app->settings.past;
    } else if (offset == 0) {
        col = hex_lighten(app->settings.task, 0.2);   /* pending, today */
    } else {
        col = app->settings.task;                     /* pending, future */
        glow = 1.0;                /* future pending tasks get a soft glow */
    }

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    int drawn = 0;
    for (int i = 0; i < d->count && drawn < shown; i++) {
        if (d->tasks[i].done)
            continue;
        double x = start + drawn * step;
        if (glow > 0.0) {
            set_hex(cr, col, 0.25 * fade);
            cairo_set_line_width(cr, w + 3.0);
            cairo_move_to(cr, x, top);
            cairo_line_to(cr, x, top - h);
            cairo_stroke(cr);
        }
        set_hex(cr, col, fade);
        cairo_set_line_width(cr, w);
        cairo_move_to(cr, x, top);
        cairo_line_to(cr, x, top - h);
        cairo_stroke(cr);
        drawn++;
    }

    /* Overflow indicator when there are more than 8 pending tasks. */
    if (npend > 8) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d", npend - 8);
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        set_hex(cr, hex_lighten(app->settings.task, 0.2), 0.9 * fade);
        cairo_move_to(cr, cx - ext.width / 2.0, top - h - 4);
        cairo_show_text(cr, buf);
    }
}

/* Draw a single day dot with its state colour, plus a date label below. */
static void draw_dot(App *app, cairo_t *cr, int offset) {
    double cx = dot_x(offset);
    if (cx < -DOT_SPACING || cx > WIN_W + DOT_SPACING)
        return;                    /* off-screen, skip */
    double fade = edge_fade(cx);   /* dissolve into the portals at the ends */
    if (fade <= 0.0)
        return;

    gboolean hovered = app->has_hover && app->hover_offset == offset;
    unsigned int col;
    double r;
    double glow = 0.0;

    if (offset == 0) {
        col = app->settings.accent;
        r = 4.5;
        glow = 1.0;
    } else if (offset < 0) {
        col = app->settings.past;
        r = 3.0;
    } else {
        col = app->settings.future;
        r = 3.0;
    }
    if (hovered) {
        col = hex_lighten(col, 0.4);
        r += 1.5;
        glow = 1.0;
    }

    if (glow > 0.0) {
        set_hex(cr, col, 0.25 * fade);
        cairo_arc(cr, cx, LINE_Y, r + 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    set_hex(cr, col, fade);
    cairo_arc(cr, cx, LINE_Y, r, 0, 2 * M_PI);
    cairo_fill(cr);

    /* Date labels: "today" under today, a short date under every other dot. */
    char label[24];
    if (offset == 0) {
        g_strlcpy(label, "today", sizeof(label));
    } else {
        date_label_short(offset, label, sizeof(label));
        /* strftime %e pads single digits with a space; trim it. */
        if (label[4] == ' ')
            memmove(&label[4], &label[5], strlen(&label[5]) + 1);
    }
    if (label[0]) {
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, label, &ext);
        set_hex(cr, offset == 0 ? app->settings.accent : 0xffffff,
                (offset == 0 ? 1.0 : 0.8) * fade);
        cairo_move_to(cr, cx - ext.width / 2.0, LINE_Y + 16);
        cairo_show_text(cr, label);
    }
}

/* Render the whole timeline: line, dots, labels and per-day task dashes. */
void timeline_draw(App *app, cairo_t *cr) {
    draw_line(app, cr);
    // draw_portal(app, cr, PORTAL_X);          /* sparkling portals at both ends */
    // draw_portal(app, cr, WIN_W - PORTAL_X);

    /* PAST_DAYS + today + FUTURE_DAYS dots, plus one extra on the right that
     * fades in from the portal as the strip scrolls (the fade hides it until
     * it crosses the portal, so it never shows as an extra dot outside). */
    for (int off = -PAST_DAYS; off <= FUTURE_DAYS + 1; off++) {
        double cx = dot_x(off);
        if (cx < -DOT_SPACING || cx > WIN_W + DOT_SPACING)
            continue;
        char date[DATE_LEN];
        date_for_offset(off, date);
        DayTasks *d = tasks_find_day(&app->store, date);
        draw_dot(app, cr, off);
        draw_dashes(app, cr, d, off, cx);
    }
}
