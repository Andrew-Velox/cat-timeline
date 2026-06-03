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

/* Draw the soft accent-coloured horizontal line with a glow and faded edges. */
static void draw_line(App *app, cairo_t *cr) {
    double r, g, b;
    hex_rgb(app->settings.accent, &r, &g, &b);
    double br, bg, bb;                                   /* brighter mid-stop */
    hex_rgb(hex_lighten(app->settings.accent, 0.12), &br, &bg, &bb);

    /* x-gradient: transparent at both ends, bright accent in the middle. */
    cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, WIN_W, 0);
    cairo_pattern_add_color_stop_rgba(grad, 0.00, r, g, b, 0.0);
    cairo_pattern_add_color_stop_rgba(grad, 0.18, r, g, b, 0.55);
    cairo_pattern_add_color_stop_rgba(grad, 0.50, br, bg, bb, 0.9);
    cairo_pattern_add_color_stop_rgba(grad, 0.82, r, g, b, 0.55);
    cairo_pattern_add_color_stop_rgba(grad, 1.00, r, g, b, 0.0);

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source(cr, grad);

    /* Three layered strokes of decreasing width fake a soft outer glow. */
    const double widths[] = {7.0, 3.5, 1.6};
    const double alphas[] = {0.18, 0.4, 1.0};
    for (int i = 0; i < 3; i++) {
        cairo_push_group(cr);
        cairo_set_source(cr, grad);
        cairo_set_line_width(cr, widths[i]);
        cairo_move_to(cr, 0, LINE_Y);
        cairo_line_to(cr, WIN_W, LINE_Y);
        cairo_stroke(cr);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, alphas[i]);
    }
    cairo_pattern_destroy(grad);
}

/* A soft highlight that flows along the line, looping left -> right.
 * Driven by app->tick so it advances smoothly on every timer frame. */
static void draw_pulse(App *app, cairo_t *cr) {
    const double period = 42.0;                 /* ticks per sweep (~4.2s) */
    double t = fmod(app->tick / period, 1.0);   /* 0 -> 1 across the line  */
    double px = t * WIN_W;                       /* highlight centre x      */
    double env = sin(t * M_PI);                  /* fade in/out at the ends */
    double alpha = 0.55 * env;
    if (alpha < 0.01)
        return;

    double r, gg, b;
    hex_rgb(hex_lighten(app->settings.accent, 0.45), &r, &gg, &b);

    const double radius = 48.0;
    cairo_pattern_t *g = cairo_pattern_create_radial(px, LINE_Y, 0, px, LINE_Y, radius);
    cairo_pattern_add_color_stop_rgba(g, 0.0, r, gg, b, alpha);
    cairo_pattern_add_color_stop_rgba(g, 1.0, r, gg, b, 0.0);

    cairo_save(cr);
    /* Hug the line: clip to a thin band so the glow stays on the timeline. */
    cairo_rectangle(cr, 0, LINE_Y - 6.0, WIN_W, 12.0);
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_pattern_destroy(g);
}

/* Draw the vertical task dashes (one per task) above a dot. */
static void draw_dashes(App *app, cairo_t *cr, DayTasks *d, int offset, double cx) {
    if (!d || d->count == 0)
        return;

    int shown = d->count > 8 ? 8 : d->count;
    const double w = 2.5;          /* dash width  */
    const double h = 8.0;          /* dash height */
    const double step = w + 4.0;   /* centre-to-centre spacing */
    double total = (shown - 1) * step;
    double start = cx - total / 2.0;
    double top = LINE_Y - 16.0;    /* bottom of the dash column */

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, w);

    for (int i = 0; i < shown; i++) {
        double x = start + i * step;
        unsigned int col;
        double glow = 0.0;
        if (d->tasks[i].done) {
            col = app->settings.done;
        } else if (offset < 0) {
            col = app->settings.past;
        } else if (offset == 0) {
            col = hex_lighten(app->settings.task, 0.2);   /* pending, today */
        } else {
            col = app->settings.task;                     /* pending, future */
            glow = 1.0;             /* future pending tasks get a soft glow */
        }

        if (glow > 0.0) {
            set_hex(cr, col, 0.25);
            cairo_set_line_width(cr, w + 3.0);
            cairo_move_to(cr, x, top);
            cairo_line_to(cr, x, top - h);
            cairo_stroke(cr);
            cairo_set_line_width(cr, w);
        }
        set_hex(cr, col, 1.0);
        cairo_move_to(cr, x, top);
        cairo_line_to(cr, x, top - h);
        cairo_stroke(cr);
    }

    /* Overflow indicator above the column when there are more than 8 tasks. */
    if (d->count > 8) {
        char buf[16];
        snprintf(buf, sizeof(buf), "+%d", d->count - 8);
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        set_hex(cr, hex_lighten(app->settings.task, 0.2), 0.9);
        cairo_move_to(cr, cx - ext.width / 2.0, top - h - 4);
        cairo_show_text(cr, buf);
    }
}

/* Draw a single day dot with its state colour, plus a date label below. */
static void draw_dot(App *app, cairo_t *cr, int offset) {
    double cx = dot_x(offset);
    if (cx < -DOT_SPACING || cx > WIN_W + DOT_SPACING)
        return;                    /* off-screen, skip */

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
        set_hex(cr, col, 0.25);
        cairo_arc(cr, cx, LINE_Y, r + 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    set_hex(cr, col, 1.0);
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
        set_hex(cr, offset == 0 ? app->settings.accent : 0xffffff, offset == 0 ? 1.0 : 0.8);
        cairo_move_to(cr, cx - ext.width / 2.0, LINE_Y + 16);
        cairo_show_text(cr, label);
    }
}

/* Render the whole timeline: line, dots, labels and per-day task dashes. */
void timeline_draw(App *app, cairo_t *cr) {
    draw_line(app, cr);
    draw_pulse(app, cr);

    /* One extra day each side so dots scroll in/out instead of popping. */
    for (int off = -PAST_DAYS - 1; off <= FUTURE_DAYS + 1; off++) {
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
