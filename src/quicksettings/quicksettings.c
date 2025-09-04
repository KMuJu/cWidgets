#include "quicksettings.h"
#include "audio_slider.h"
#include "bluetooth_page.h"
#include "gdk/gdk.h"
#include "gtk4-layer-shell.h"
#include "header.h"
#include "togglebutton.h"
#include "wifi_page.h"
#include "wp/core.h"
#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
  GdkMonitor *monitor;
  GtkWidget *window;
} MonitorWidget;

static GHashTable *windows = NULL;
static GtkWidget *current_open_window = NULL;

static void init_windows(void) {
  if (NULL != windows)
    return;
  windows = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static GtkWidget *qs_window(GdkDisplay *display, GdkMonitor *monitor) {
  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);
  g_object_unref(monitor);
  GtkWidget *window = gtk_window_new();
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "quicksettings");
  gtk_window_set_title(GTK_WINDOW(window), buffer);
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

  gtk_window_set_display(GTK_WINDOW(window), display);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_monitor(GTK_WINDOW(window), monitor);
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
  gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP,
                       geometry.y);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

  gtk_window_present(GTK_WINDOW(window));

  gtk_widget_set_visible(window, FALSE);
  gtk_widget_set_hexpand(window, FALSE);
  gtk_widget_set_vexpand(window, FALSE);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  return window;
}

static void quicksettings(GtkWidget *window, WpObjectManager *om,
                          WpCore *core) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(box, "quicksettings");

  GtkWidget *header_box = qs_header();

  gtk_box_append(GTK_BOX(box), header_box);

  GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_halign(buttons, GTK_ALIGN_CENTER);

  GtkWidget *notification = notification_button();
  GtkWidget *colorpicker = colorpicker_button();
  gtk_box_append(GTK_BOX(buttons), notification);
  gtk_box_append(GTK_BOX(buttons), colorpicker);

  gtk_box_append(GTK_BOX(box), buttons);

  GtkWidget *audio_slider = create_audio_slider(om, core);
  gtk_box_append(GTK_BOX(box), audio_slider);

  GtkWidget *wifi = wifi_page();
  gtk_box_append(GTK_BOX(box), wifi);

  GtkWidget *bluetooth = bluetooth_page();
  gtk_box_append(GTK_BOX(box), bluetooth);

  gtk_window_set_child(GTK_WINDOW(window), box);
}

void start_quick_settings(GdkDisplay *display, GdkMonitor *monitor,
                          WpObjectManager *om, WpCore *core) {
  init_windows();
  GtkWidget *window = qs_window(display, monitor);
  quicksettings(window, om, core);

  g_hash_table_insert(windows, monitor, window);
}

void toggle_quick_settings(GdkMonitor *monitor) {
  GtkWidget *window = g_hash_table_lookup(windows, monitor);
  if (NULL == window) {
    g_warning("Tried to toggle qs for monitor(%p) not in hash table",
              (void *)monitor);
    return;
  }
  if (current_open_window != NULL) {
    gboolean visible = gtk_widget_get_visible(current_open_window);
    gtk_widget_set_visible(current_open_window, !visible);

    if (window == current_open_window) {
      current_open_window = NULL;
      return;
    }
  }

  gboolean visible = !gtk_widget_get_visible(window);
  gtk_widget_set_visible(window, visible);

  if (visible)
    current_open_window = window;
  else
    current_open_window = NULL;
}
