#include "bar/bar.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkcssprovider.h"
#include "gtk4-layer-shell/gtk4-layer-shell.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  gtk_init();
  GdkDisplay *display = gdk_display_get_default();
  GListModel *monitors = gdk_display_get_monitors(display);
  guint n_monitors = g_list_model_get_n_items(monitors);

  for (guint i = 0; i < n_monitors; i++) {

    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, i));
    const char *name = gdk_monitor_get_model(monitor);
    g_message("Assigning window to monitor: %s", name);

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    g_object_unref(monitor);
    GtkWidget *window = gtk_window_new();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Gtkbar%d", i);
    gtk_window_set_title(GTK_WINDOW(window), buffer);
    gtk_window_set_default_size(GTK_WINDOW(window), geometry.width, -1);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    gtk_window_set_display(GTK_WINDOW(window), display);

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_monitor(GTK_WINDOW(window), monitor);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP,
                         geometry.y);
    gtk_window_present(GTK_WINDOW(window));
    bar(window);
  }
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);
}
