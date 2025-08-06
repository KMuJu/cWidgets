#include "bar.h"
#include "battery/battery.h"
#include "gtk/gtkshortcut.h"
#include "wifi/wifi.h"
#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

void bar(GtkWidget *window) {
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_widget_set_hexpand(box, TRUE);
  int bar_margin = 5;
  int bar_margin_side = 8;
  gtk_widget_set_margin_top(box, bar_margin);
  gtk_widget_set_margin_bottom(box, bar_margin);
  gtk_widget_set_margin_start(box, bar_margin_side);
  gtk_widget_set_margin_end(box, bar_margin_side);

  GtkWidget *battery_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  start_battery_widget(battery_box);

  GtkWidget *workspaces_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_hexpand(workspaces_box, TRUE);
  gtk_widget_set_halign(workspaces_box, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(workspaces_box), gtk_label_new("Workspaces"));

  GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

  add_wifi_widget(left_box);

  gtk_box_append(GTK_BOX(box), battery_box);
  gtk_box_append(GTK_BOX(box), workspaces_box);
  gtk_box_append(GTK_BOX(box), left_box);

  gtk_window_set_child(GTK_WINDOW(window), box);
}
