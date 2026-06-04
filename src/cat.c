#include "cat.h"
#include "timeline.h"
#include "window.h"
#include "runcat_frames.h"
#include <math.h>

#define CAT_FRAMES 5                  /* RunCat runner has 5 frames */
#define CAT_SIZE   30.0              /* on-screen size in px (sprites are 32x32) */

/* Decoded sprite frames, lazily loaded once on first draw. */
static cairo_surface_t *g_frames[CAT_FRAMES];
static gboolean g_loaded = FALSE;

/* Decode the embedded PNG byte arrays into cached Cairo surfaces. */
static void load_frames(void) {
    for (int i = 0; i < CAT_FRAMES; i++) {
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        gdk_pixbuf_loader_write(loader, cat_frame_data[i], cat_frame_len[i], NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        GdkPixbuf *pb = gdk_pixbuf_loader_get_pixbuf(loader);
        g_frames[i] = pb ? gdk_cairo_surface_create_from_pixbuf(pb, 1, NULL) : NULL;
        g_object_unref(loader);      /* also releases the pixbuf */
    }
    g_loaded = TRUE;
}

/* Paint one sprite (using its alpha as a mask) tinted with `col` at `alpha`. */
static void blit_tinted(cairo_t *cr, cairo_surface_t *s, double cx, double top,
                        double size, unsigned int col, double alpha) {
    if (!s)
        return;
    cairo_save(cr);
    cairo_translate(cr, cx - size / 2.0, top);
    cairo_scale(cr, size / 32.0, size / 32.0);   /* sprites are 32x32 */
    set_hex(cr, col, alpha);
    cairo_mask_surface(cr, s, 0, 0);             /* tint via the sprite's alpha */
    cairo_restore(cr);
}

/* Draw fading paw-print dots trailing behind the cat. */
static void draw_paw_trail(cairo_t *cr, double cx, unsigned int col) {
    for (int i = 0; i < 4; i++) {
        double x = cx - 18 - i * 8.0;
        double a = 0.28 - i * 0.06;
        if (a <= 0)
            break;
        set_hex(cr, col, a);
        cairo_arc(cr, x, LINE_Y + 2, 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

/* Draw the running RunCat sprite (tinted, with a soft glow) on today's dot. */
void cat_draw(App *app, cairo_t *cr) {
    if (!g_loaded)
        load_frames();

    int f = app->frame % CAT_FRAMES;
    unsigned int col = app->settings.cat;
    double cx = TODAY_X;
    double bob = sin((app->frame / (double)CAT_FRAMES) * 2.0 * M_PI) * 1.0;
    double top = LINE_Y - CAT_SIZE + 5.0 + bob;   /* feet rest near the line */

    draw_paw_trail(cr, cx, col);
    blit_tinted(cr, g_frames[f], cx, top - 1.0, CAT_SIZE + 4.0, col, 0.16); /* glow */
    blit_tinted(cr, g_frames[f], cx, top, CAT_SIZE, col, 1.0);             /* solid */
}

/* Timer callback: advance to the next sprite frame and queue a redraw. */
gboolean cat_tick(gpointer user_data) {
    App *app = user_data;
    app->frame = (app->frame + 1) % CAT_FRAMES;
    app->tick++;                       /* drives the timeline pulse */
    window_redraw(app);
    return G_SOURCE_CONTINUE;
}
