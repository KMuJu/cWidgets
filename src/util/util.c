#include "util.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>

static GdkCursor *pointer_cursor = NULL;

GdkCursor *get_pointer_cursor(void) {
  if (pointer_cursor)
    return pointer_cursor;

  pointer_cursor = gdk_cursor_new_from_name("pointer", NULL);
  return pointer_cursor;
}

void cursor_util_cleanup(void) { g_clear_object(&pointer_cursor); }

GtkWidget *find_child_by_property_int(GtkWidget *box, const char *property,
                                      gint value) {
  GtkWidget *child = gtk_widget_get_first_child(box);
  while (child != NULL) {
    gint child_property =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), property));
    if (child_property == value) {
      return child;
    }

    child = gtk_widget_get_next_sibling(child);
  }

  return NULL;
}

GtkWidget *get_nth_child(GtkWidget *box, guint index) {
  GtkWidget *child = gtk_widget_get_first_child(box);
  guint i = 0;
  while (child != NULL) {
    if (i == index)
      return child;

    child = gtk_widget_get_next_sibling(child);
    i++;
  }

  return NULL;
}

void remove_children(GtkWidget *box) {
  GtkWidget *child = gtk_widget_get_first_child(box);
  while (child != NULL) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_widget_unparent(child);
    child = next;
  }
}

void remove_children_start(GtkWidget *box, guint start) {
  GtkWidget *child = gtk_widget_get_first_child(box);
  guint i = 0;
  while (child != NULL) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    if (i >= start)
      gtk_widget_unparent(child);
    child = next;
    i++;
  }
}

void sh(const gchar *cmd) {
  GError *error = NULL;
  if (!g_spawn_command_line_async(cmd, &error)) {
    g_printerr("Failed to call '%s': %s\n", cmd, error->message);
    g_error_free(error);
  }
}
