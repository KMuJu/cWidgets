#include "bar.h"
#include "audio/audio.h"
#include "battery/battery.h"
#include "bluetooth/bt.h"
#include "bluetooth/device.h"
#include "date_time/date_time.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "wifi/wifi.h"
#include "workspaces/workspaces.h"
#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

static void on_bt_connected(Bluetooth *bluetooth, gboolean connected,
                            gpointer user_data) {
  GtkWidget *bluetooth_icon = user_data;
  gtk_widget_set_visible(bluetooth_icon, connected);
}

void bar(GtkWidget *window, GDBusConnection *conneciton, WpCore *core,
         WpObjectManager *om) {
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
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

  GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 13);

  GtkWidget *bluetooth_icon =
      gtk_image_new_from_icon_name("bluetooth-symbolic");
  Bluetooth *bt = bluetooth_get_default();
  g_signal_connect(bt, "connected", G_CALLBACK(on_bt_connected),
                   bluetooth_icon);
  bluetooth_install_signals(bt);
  gtk_box_append(GTK_BOX(right_box), bluetooth_icon);

  start_audio_widget(right_box, core, om);
  add_wifi_widget(right_box);
  start_date_time_widget(right_box);

  gtk_center_box_set_start_widget(GTK_CENTER_BOX(box), battery_box);
  gtk_center_box_set_center_widget(GTK_CENTER_BOX(box), workspaces_box);
  gtk_center_box_set_end_widget(GTK_CENTER_BOX(box), right_box);

  gtk_window_set_child(GTK_WINDOW(window), box);
}
