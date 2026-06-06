#include "style.h"

char *style_hex(unsigned int c, char *buf) {
    g_snprintf(buf, 8, "#%06x", c & 0xFFFFFF);
    return buf;
}

void style_class(GtkWidget *w, const char *cls) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), cls);
}

/* Dark theme for the settings window and the task editor, matching the mobile
 * app: near-black panels, rounded task pills, uppercase section headers and a
 * fixed blue UI accent. The widget's own palette stays user-defined; only the
 * task-completion colour (`done`) flows into the chrome (the progress bar). */
void style_ensure(App *app) {
    static GtkCssProvider *prov = NULL;

    char done[8];
    style_hex(app->settings.done, done);

    char css[4600];
    g_snprintf(css, sizeof(css),
        /* ---- panel + typography ---- */
        ".taskpanel { background-color:#111317; border:1px solid #262a32;"
        "  border-radius:16px; }"
        ".tp-box { padding:14px; }"
        ".tp-title { color:#f0f1f3; font-weight:bold; font-size:15px; }"
        ".tp-sub { color:#6b7280; font-size:10px; font-weight:bold;"
        "  letter-spacing:1px; }"
        ".tp-foot { color:#8b9099; font-size:10px; }"
        ".tp-empty { color:#6b7280; font-style:italic; padding:10px 2px; }"
        ".tp-badge { background-color:#1f2630; color:#3da9fc; border-radius:8px;"
        "  padding:2px 8px; font-size:9px; font-weight:bold; }"
        ".tp-close { background:none; border:none; box-shadow:none; outline:none;"
        "  min-height:0; min-width:0; padding:0 4px; color:#8b9099; font-size:14px; }"
        ".tp-close:hover { color:#ff6b6b; }"
        /* ---- task pill rows ---- */
        ".tp-task { background-color:#1d2129; border:1px solid #23272f;"
        "  border-radius:12px; box-shadow:none; outline:none; min-height:0;"
        "  padding:9px 12px; color:#e9eaed; }"
        ".tp-task:hover { background-color:#232831; }"
        ".tp-del { background:none; border:none; box-shadow:none; outline:none;"
        "  min-height:0; padding:2px 8px; color:#ff6b6b; }"
        ".tp-del:hover { color:#ff8a8a; background-color:rgba(255,107,107,0.12);"
        "  border-radius:8px; }"
        /* ---- progress bar ---- */
        ".tp-prog trough { min-height:5px; border-radius:3px;"
        "  background-color:#23272f; border:none; }"
        ".tp-prog progress { min-height:5px; border-radius:3px; background-color:%s; }"
        /* ---- composer ---- */
        ".tp-entry { border-radius:12px; padding:10px 12px; background-color:#14171c;"
        "  color:#e9eaed; border:1px solid #262a32; box-shadow:none; }"
        ".tp-entry:focus { border-color:#3da9fc; }"
        ".tp-entry image { color:#6b7280; }"
        ".tp-add { background-image:none; background-color:#3da9fc; color:#08151f;"
        "  font-weight:bold; border-radius:12px; padding:9px 18px;"
        "  border:none; box-shadow:none; }"
        ".tp-add:hover { background-color:#5cb8fd; }"
        ".tp-compose { border-top:1px solid #23262e; padding-top:10px; margin-top:2px; }"
        /* ---- settings-window specifics ---- */
        ".tp-reset { background-image:none; background-color:#1d2129; color:#c2c7cf;"
        "  border:1px solid #2a2f38; border-radius:10px; padding:6px 14px;"
        "  box-shadow:none; }"
        ".tp-reset:hover { background-color:#232831; }"
        ".tp-card { background-color:#16181d; border:1px solid #23262e;"
        "  border-radius:16px; padding:10px; }"
        ".tp-date { color:#6b7280; font-size:10px; font-weight:bold;"
        "  letter-spacing:1px; padding:2px 2px 8px 4px; }"
        ".settings { color:#e9eaed; background-color:#111317; }"
        ".settings notebook, .settings notebook stack { background-color:transparent; }"
        ".settings notebook > header { background:transparent; border:none; }"
        ".settings notebook > header tabs tab { padding:5px 12px; min-height:0;"
        "  color:#8b9099; border:none; background:none; }"
        ".settings notebook > header tabs tab:checked { color:#f0f1f3;"
        "  box-shadow:inset 0 -2px #3da9fc; }"
        /* left rail: stacked icon+label pills instead of underlined tabs */
        ".settings notebook > header.left { background:transparent; border:none;"
        "  padding:2px; margin-right:6px; }"
        ".settings notebook > header.left tabs tab { padding:9px 8px; margin:3px 0;"
        "  border-radius:11px; box-shadow:none; color:#8b9099; }"
        ".settings notebook > header.left tabs tab:checked { color:#f0f1f3;"
        "  background-color:#262a32; box-shadow:none; }"
        ".settings notebook > header.left tabs tab:hover { background-color:#1d2129; }"
        ".settings stackswitcher button { background:none; border:none;"
        "  box-shadow:none; outline:none; color:#8b9099; padding:6px 16px;"
        "  min-height:0; font-weight:bold; }"
        ".settings stackswitcher button:checked { color:#f0f1f3;"
        "  box-shadow:inset 0 -2px #3da9fc; }"
        ".tp-emptyicon { color:#3a3f48; }"
        /* ---- calendar ---- */
        ".settings calendar { background-color:#16181d; color:#e9eaed;"
        "  border:1px solid #23262e; border-radius:14px; padding:8px; font-size:12px; }"
        ".settings calendar.header { color:#e9eaed; }"
        ".settings calendar.button { color:#8b9099; }"
        ".settings calendar.highlight { color:#8b9099; }"
        ".settings calendar:indeterminate { color:#4b4f57; }"
        ".settings calendar:selected { background-color:#3da9fc; color:#08151f;"
        "  font-weight:bold; border-radius:9px; }",
        done);

    if (!prov) {
        prov = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(), GTK_STYLE_PROVIDER(prov),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
}
