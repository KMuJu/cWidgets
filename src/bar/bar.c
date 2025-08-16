#include "bar.h"
#include "audio/audio.h"
#include "battery/battery.h"
#include "bluetooth/bt.h"
#include "date_time/date_time.h"
#include "quicksettings/quicksettings.h"
#include "util.h"
#include "wifi/wifi.h"
#include "workspaces/workspaces.h"
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

static void on_bt_connected(Bluetooth *bluetooth, gboolean connected,
                            gpointer user_data) {
  GtkWidget *bluetooth_icon = user_data;
  gtk_widget_set_visible(bluetooth_icon, connected);
}

GtkWidget *bar_init_window(GdkDisplay *display, GdkMonitor *monitor) {
  GdkRectangle geometry;
  gdk_monitor_get_geometry(monitor, &geometry);
  g_object_unref(monitor);
  GtkWidget *window = gtk_window_new();
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "bar");
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
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
  gtk_window_present(GTK_WINDOW(window));
  return window;
}

void bar(GdkDisplay *display, GdkMonitor *monitor, GDBusConnection *conneciton,
         WpCore *core, WpObjectManager *om) {
  GtkWidget *window = bar_init_window(display, monitor);
  GtkWidget *box = gtk_center_box_new();
  gtk_widget_set_hexpand(box, TRUE);
  gtk_widget_add_css_class(box, "bar");

  GtkWidget *battery_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  start_battery_widget(battery_box, conneciton);

  GtkWidget *workspaces_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(workspaces_box, "workspaces");
  gtk_widget_set_hexpand(workspaces_box, TRUE);
  gtk_widget_set_halign(workspaces_box, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(workspaces_box), gtk_label_new("Workspaces"));
  g_thread_new("workspace", listen_to_hyprland_socket, workspaces_box);

  GtkWidget *right_button = gtk_button_new();
  gtk_widget_add_css_class(right_button, "toggle-button");
  gtk_widget_set_cursor(right_button, get_pointer_cursor());
  g_signal_connect_swapped(right_button, "clicked",
                           G_CALLBACK(toggle_quick_settings), monitor);

  GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 13);
  gtk_button_set_child(GTK_BUTTON(right_button), right_box);

  GtkWidget *bluetooth_icon =
      gtk_image_new_from_icon_name("bluetooth-symbolic");
  Bluetooth *bt = bluetooth_get_default();
  g_signal_connect(bt, "connected", G_CALLBACK(on_bt_connected),
                   bluetooth_icon);
  bluetooth_call_signals(bt);
  gtk_box_append(GTK_BOX(right_box), bluetooth_icon);

  start_audio_widget(right_box, core, om);
  add_wifi_widget(right_box);
  start_date_time_widget(right_box);

  gtk_center_box_set_start_widget(GTK_CENTER_BOX(box), battery_box);
  gtk_center_box_set_center_widget(GTK_CENTER_BOX(box), workspaces_box);
  gtk_center_box_set_end_widget(GTK_CENTER_BOX(box), right_button);

  gtk_window_set_child(GTK_WINDOW(window), box);
}
