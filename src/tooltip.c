#include "tooltip.h"
#include "timeline.h"
#include <math.h>
#include <string.h>

#define TIP_W       212.0
#define TIP_BG      0xE6E6E7
#define TIP_BORDER  0xA1A1A1
#define TIP_MAXROWS 6           /* max task rows before a "+N more" line */

/* Append a rounded-rectangle sub-path to the current path. */
static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

/* Draw the floating tooltip for the hovered dot at the top of the window. */
void tooltip_draw(App *app, cairo_t *cr) {
    int off = app->hover_offset;
    char date[DATE_LEN];
    date_for_offset(off, date);
    DayTasks *d = tasks_find_day(&app->store, date);
    int ntasks = d ? d->count : 0;
    int nshown = ntasks > TIP_MAXROWS ? TIP_MAXROWS : ntasks;

    /* Row count: header + (tasks | "no tasks") + hint. */
    int rows = 1 + (ntasks ? nshown : 1) + 1;
    if (ntasks > TIP_MAXROWS)
        rows++;                                  /* "+N more" line */

    const double pad = 10.0, line_h = 15.0;
    double h = pad * 2 + line_h * rows;
    double x = dot_x(off) - TIP_W / 2.0;
    if (x < 8) x = 8;
    if (x > WIN_W - TIP_W - 8) x = WIN_W - TIP_W - 8;
    double y = 8.0;

    /* Background panel with border. */
    rounded_rect(cr, x, y, TIP_W, h, 7);
    set_hex(cr, TIP_BG, 0.97);
    cairo_fill_preserve(cr);
    set_hex(cr, TIP_BORDER, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);

    double tx = x + pad;
    double ty = y + pad + 11;

    /* Header line: date (or "today") plus a +Nd / -Nd offset badge. */
    char header[40];
    if (off == 0)
        g_strlcpy(header, "today", sizeof(header));
    else
        date_label_long(off, header, sizeof(header));
    set_hex(cr, 0x000000, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, header);

    if (off != 0) {
        char badge[12];
        snprintf(badge, sizeof(badge), "%+dd", off);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, badge, &ext);
        set_hex(cr, 0x000000, 1.0);
        cairo_move_to(cr, x + TIP_W - pad - ext.width, ty);
        cairo_show_text(cr, badge);
    }
    ty += line_h;

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);

    if (ntasks == 0) {
        set_hex(cr, 0x000000, 1.0);
        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, "no tasks");
        ty += line_h;
    } else {
        for (int i = 0; i < nshown; i++) {
            Task *t = &d->tasks[i];
            char row[96];
            /* › marks a pending task, ✓ marks a done one. */
            snprintf(row, sizeof(row), "%s %.72s", t->done ? "✓" : "›", t->text);
            if (t->done)
                set_hex(cr, 0x000000, 0.85);
            else
                set_hex(cr, 0x000000, 1.0);
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, row);
            ty += line_h;
        }
        if (ntasks > TIP_MAXROWS) {
            char more[24];
            snprintf(more, sizeof(more), "+%d more", ntasks - TIP_MAXROWS);
            set_hex(cr, 0x000000, 1.0);
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, more);
            ty += line_h;
        }
    }

    /* Hint line. */
    set_hex(cr, 0x000000, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, "click to manage");
}
