#include "cat.h"
#include "timeline.h"
#include <math.h>

#define CAT_COLOR 0xf0e0ff          /* soft white-purple */

/* Draw a filled ellipse centred at (cx,cy) with the given radii. */
static void ellipse(cairo_t *cr, double cx, double cy, double rx, double ry) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
}

/* Draw the cat silhouette once, inflated by `infl` (used for the glow pass). */
static void draw_cat_shape(cairo_t *cr, double cx, double phase, double infl,
                           double alpha, gboolean draw_eye) {
    double bob = sin(phase * 2.0) * 1.5;       /* vertical bob while running */
    double body_cy = LINE_Y - 11.0 + bob;
    double body_rx = 13.0, body_ry = 6.0;
    double hx = cx + 12.0, hy = body_cy - 5.0; /* head centre */
    double head_r = 6.0;

    set_hex(cr, CAT_COLOR, alpha);

    /* Legs: two pairs that swing in opposite phase, drawn behind the body. */
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 2.2 + infl);
    double hip_y = body_cy + body_ry - 1.0;
    double foot_y = LINE_Y + 1.0;
    struct { double hipx, swing_phase; } legs[4] = {
        { cx + 6.0, phase },
        { cx + 3.0, phase + M_PI },
        { cx - 6.0, phase + M_PI },
        { cx - 9.0, phase },
    };
    for (int i = 0; i < 4; i++) {
        double foot_x = legs[i].hipx + sin(legs[i].swing_phase) * 5.0;
        cairo_move_to(cr, legs[i].hipx, hip_y);
        cairo_line_to(cr, foot_x, foot_y);
        cairo_stroke(cr);
    }

    /* Tail: a curve sweeping up from the rear, angle drifting slowly. */
    double tail_a = sin(phase * 0.5) * 0.5;
    cairo_set_line_width(cr, 2.4 + infl);
    cairo_move_to(cr, cx - body_rx, body_cy);
    cairo_curve_to(cr,
                   cx - body_rx - 8, body_cy - 2,
                   cx - body_rx - 12, body_cy - 10 + tail_a * 6,
                   cx - body_rx - 6, body_cy - 16 + tail_a * 8);
    cairo_stroke(cr);

    /* Body. */
    ellipse(cr, cx, body_cy, body_rx + infl, body_ry + infl);

    /* Head. */
    cairo_arc(cr, hx, hy, head_r + infl, 0, 2 * M_PI);
    cairo_fill(cr);

    /* Ears: two triangles on top of the head. */
    cairo_move_to(cr, hx - 5, hy - 3);
    cairo_line_to(cr, hx - 6, hy - 9 - infl);
    cairo_line_to(cr, hx - 1, hy - 5);
    cairo_close_path(cr);
    cairo_move_to(cr, hx + 1, hy - 5);
    cairo_line_to(cr, hx + 5, hy - 9 - infl);
    cairo_line_to(cr, hx + 5, hy - 3);
    cairo_close_path(cr);
    cairo_fill(cr);

    /* Eye: a small dark dot (skipped on the glow pass). */
    if (draw_eye) {
        set_hex(cr, 0x1a1030, 0.9);
        cairo_arc(cr, hx + 3, hy - 1, 1.3, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

/* Draw fading paw-print dots trailing behind the cat. */
static void draw_paw_trail(cairo_t *cr, double cx) {
    for (int i = 0; i < 4; i++) {
        double x = cx - 16 - i * 9.0;
        double a = 0.32 - i * 0.07;
        if (a <= 0)
            break;
        set_hex(cr, CAT_COLOR, a);
        cairo_arc(cr, x, LINE_Y + 2, 1.6, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

/* Draw the running cat with a soft glow on today's dot. */
void cat_draw(App *app, cairo_t *cr) {
    double cx = TODAY_X;
    double phase = (app->frame / 8.0) * 2.0 * M_PI;

    draw_paw_trail(cr, cx);
    draw_cat_shape(cr, cx, phase, 2.0, 0.18, FALSE);  /* glow pass  */
    draw_cat_shape(cr, cx, phase, 0.0, 1.0, TRUE);    /* solid pass */
}

/* Timer callback: advance to the next of 8 frames and queue a redraw. */
gboolean cat_tick(gpointer user_data) {
    App *app = user_data;
    app->frame = (app->frame + 1) % 8;
    gtk_widget_queue_draw(app->area);
    return G_SOURCE_CONTINUE;
}
