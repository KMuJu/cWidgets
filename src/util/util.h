#ifndef UTIL_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

GdkCursor *get_pointer_cursor(void);
void cursor_util_cleanup(void);

GtkWidget *find_child_by_property_int(GtkWidget *box, const char *property,
                                      gint id);

GtkWidget *get_nth_child(GtkWidget *box, guint index);

void remove_children(GtkWidget *box);

#endif /* !UTIL_H */
