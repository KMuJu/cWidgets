#include "header.h"
#include "util.h"

static void on_settings_clicked(void) {
  GError *error = NULL;
  if (!g_spawn_command_line_async(
          "env XDG_CURRENT_DESKTOP=gnome gnome-control-center", &error)) {
    g_printerr("Failed to launch gnome-control-center: %s\n", error->message);
    g_error_free(error);
  }
}

GtkWidget *qs_header(void) {
  GtkWidget *header_box = gtk_center_box_new();

  GtkWidget *settings_btn =
      gtk_button_new_from_icon_name("applications-system-symbolic");
  gtk_widget_add_css_class(settings_btn, "settings");
  gtk_widget_set_cursor(settings_btn, get_pointer_cursor());
  g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked),
                   NULL);

  GtkWidget *title = gtk_label_new("Settings");
  gtk_widget_add_css_class(title, "title");

  GtkWidget *power_btn = gtk_button_new_from_icon_name("system-shutdown");
  gtk_widget_add_css_class(power_btn, "power_btn");
  gtk_widget_set_cursor(power_btn, get_pointer_cursor());
  // TODO: Add power settings

  gtk_center_box_set_start_widget(GTK_CENTER_BOX(header_box), settings_btn);
  gtk_center_box_set_center_widget(GTK_CENTER_BOX(header_box), title);
  gtk_center_box_set_end_widget(GTK_CENTER_BOX(header_box), power_btn);
  return header_box;
}
