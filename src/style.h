#ifndef STYLE_H
#define STYLE_H

#include "app.h"

/* Install (once) the screen-wide CSS that themes the task editor and the
 * settings window. Reloaded on each call so palette changes take effect. */
void style_ensure(App *app);

/* Add a CSS class to a widget's style context. */
void style_class(GtkWidget *w, const char *cls);

/* Format a 0xRRGGBB value as a "#rrggbb" string into buf (>= 8 bytes). */
char *style_hex(unsigned int c, char *buf);

#endif /* STYLE_H */
