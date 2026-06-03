#include "style.h"

char *style_hex(unsigned int c, char *buf) {
    g_snprintf(buf, 8, "#%06x", c & 0xFFFFFF);
    return buf;
}

void style_class(GtkWidget *w, const char *cls) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), cls);
}

void style_ensure(App *app) {
    static GtkCssProvider *prov = NULL;

    char accent[8], badge[8], hover[8], done[8];
    style_hex(app->settings.accent, accent);
    style_hex(hex_lighten(app->settings.accent, 0.62), badge);
    style_hex(hex_lighten(app->settings.accent, 0.22), hover);
    style_hex(app->settings.done, done);

    char css[3600];
    g_snprintf(css, sizeof(css),
        /* ---- shared light card + task rows (editor & settings) ---- */
        ".taskpanel { background-color:#fbfbfd; border:1px solid #d3d3db;"
        "  border-radius:12px; }"
        ".tp-box { padding:14px; }"
        ".tp-title { color:#1b1b1f; font-weight:bold; font-size:13px; }"
        ".tp-badge { background-color:%s; color:#2a2730; border-radius:7px;"
        "  padding:1px 8px; font-size:9px; font-weight:bold; }"
        ".tp-close { background:none; border:none; box-shadow:none; outline:none;"
        "  min-height:0; min-width:0; padding:0 4px; color:#9aa0a6; font-size:13px; }"
        ".tp-close:hover { color:#e05555; }"
        ".tp-foot { color:#868c95; font-size:9px; }"
        ".tp-empty { color:#868c95; font-style:italic; padding:8px 2px; }"
        ".tp-task { background-image:none; background-color:transparent;"
        "  border:none; box-shadow:none; outline:none; min-height:0;"
        "  padding:3px 6px; color:#2a2a30; }"
        ".tp-task:hover { background-color:rgba(0,0,0,0.06); border-radius:7px; }"
        ".tp-del { background-image:none; background-color:transparent;"
        "  border:none; box-shadow:none; outline:none; min-height:0;"
        "  padding:2px 7px; color:#aeb4bd; }"
        ".tp-del:hover { color:#e05555; background-color:rgba(224,85,85,0.14);"
        "  border-radius:7px; }"
        ".tp-prog trough { min-height:6px; border-radius:3px;"
        "  background-color:#e6e6ee; border:none; }"
        ".tp-prog progress { min-height:6px; border-radius:3px; background-color:%s; }"
        ".tp-entry { border-radius:8px; padding:5px 8px; background-color:#ffffff;"
        "  color:#2a2a30; border:1px solid #d8d8e0; box-shadow:none; }"
        ".tp-entry:focus { border-color:%s; }"
        ".tp-add { background-image:none; background-color:%s; color:#2a2730;"
        "  font-weight:bold; border-radius:8px; padding:5px 14px;"
        "  border:none; box-shadow:none; }"
        ".tp-add:hover { background-color:%s; }"
        /* ---- settings-window specifics ---- */
        ".tp-sub { color:#868c95; font-size:10px; font-weight:bold; }"
        ".tp-reset { background-image:none; background-color:#f0f0f4;"
        "  color:#5a5f66; border:1px solid #dcdce4; border-radius:8px;"
        "  padding:4px 12px; box-shadow:none; }"
        ".tp-reset:hover { background-color:#e8e8ee; }"
        ".settings { color:#2a2a30; }"
        ".settings notebook, .settings notebook stack { background-color:transparent; }"
        ".settings notebook > header { background:transparent; border:none; }"
        ".settings notebook > header tabs tab { padding:4px 10px; min-height:0;"
        "  color:#868c95; border:none; background:none; }"
        ".settings notebook > header tabs tab:checked { color:#1b1b1f;"
        "  box-shadow:inset 0 -2px %s; }"
        ".settings calendar { background-color:#ffffff; color:#2a2a30;"
        "  border:1px solid #e2e2ea; border-radius:8px; padding:2px; }"
        ".settings calendar.header { color:#2a2a30; }"
        ".settings calendar.button { color:#5a5f66; }"
        ".settings calendar.highlight { color:#868c95; }"
        ".settings calendar:selected { background-color:%s; color:#2a2730;"
        "  border-radius:5px; }",
        badge, done, accent, accent, hover, accent, accent);

    if (!prov) {
        prov = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(), GTK_STYLE_PROVIDER(prov),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
}
