#include "header.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkrevealer.h"
#include "gtk/gtkshortcut.h"
#include "util.h"

#define SELECTED_POWER_BTN_CLASS "on"
#define ACTION "action"
#define ACTIVE "active"

#define SHUTDOWN_ACTION "poweroff"
#define REBOOT_ACTION "reboot"
#define SUSPEND_ACTION "systemctl suspend"
#define LOCK_ACTION "loginctl lock-session"
#define LOG_OUT_ACTION "hyprctl dispatch exit"

static void on_settings_clicked(void) {
  sh("env XDG_CURRENT_DESKTOP=gnome gnome-control-center");
}

static GtkWidget *power_settings_btn(const gchar *label_str,
                                     const gchar *icon_name) {
  GtkWidget *btn = gtk_button_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *image = gtk_image_new_from_icon_name(icon_name);
  GtkWidget *label = gtk_label_new(label_str);
  gtk_box_append(GTK_BOX(box), image);
  gtk_box_append(GTK_BOX(box), label);
  gtk_button_set_child(GTK_BUTTON(btn), box);

  gtk_widget_add_css_class(btn, "power-settings-btn");
  gtk_widget_set_cursor(btn, get_pointer_cursor());
  return btn;
}

static void open_confirm_revealer(GtkWidget *revealer, const gchar *action,
                                  GtkWidget *btn) {
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), TRUE);
  g_object_set_data_full(G_OBJECT(revealer), ACTION, g_strdup(action), g_free);
  g_object_set_data_full(G_OBJECT(revealer), ACTIVE, g_object_ref(btn),
                         g_object_unref);

  gtk_widget_add_css_class(btn, SELECTED_POWER_BTN_CLASS);
}

static void close_confirm_revealer(GtkWidget *revealer) {
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  g_object_set_data(G_OBJECT(revealer), ACTION, NULL);

  GtkWidget *btn = g_object_get_data(G_OBJECT(revealer), ACTIVE);
  if (btn) {
    gtk_widget_remove_css_class(btn, SELECTED_POWER_BTN_CLASS);
  }

  g_object_set_data(G_OBJECT(revealer), ACTIVE, NULL);
}

static void on_confirm(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  g_autofree const gchar *action =
      g_strdup(g_object_get_data(G_OBJECT(revealer), ACTION));
  if (!action) {
    g_warning("Tried to confirm with no action");
  } else {
    close_confirm_revealer(revealer);
    sh(action);
  }
}

static void on_shutdown_click(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  const gchar *action = g_object_get_data(G_OBJECT(revealer), ACTION);
  if (action && g_str_equal(action, SHUTDOWN_ACTION)) {
    close_confirm_revealer(revealer);
  } else {
    open_confirm_revealer(revealer, SHUTDOWN_ACTION, GTK_WIDGET(btn));
  }
}

static void on_reboot_click(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  const gchar *action = g_object_get_data(G_OBJECT(revealer), ACTION);
  if (action && g_str_equal(action, REBOOT_ACTION)) {
    close_confirm_revealer(revealer);
  } else {
    open_confirm_revealer(revealer, REBOOT_ACTION, GTK_WIDGET(btn));
  }
}

static void on_suspend_click(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  const gchar *action = g_object_get_data(G_OBJECT(revealer), ACTION);
  if (action && g_str_equal(action, SUSPEND_ACTION)) {
    close_confirm_revealer(revealer);
  } else {
    open_confirm_revealer(revealer, SUSPEND_ACTION, GTK_WIDGET(btn));
  }
}

static void on_lock_click(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  const gchar *action = g_object_get_data(G_OBJECT(revealer), ACTION);
  if (action && g_str_equal(action, LOCK_ACTION)) {
    close_confirm_revealer(revealer);
  } else {
    open_confirm_revealer(revealer, LOCK_ACTION, GTK_WIDGET(btn));
  }
}

static void on_log_out_click(GtkButton *btn, gpointer data) {
  GtkWidget *revealer = data;
  const gchar *action = g_object_get_data(G_OBJECT(revealer), ACTION);
  if (action && g_str_equal(action, LOG_OUT_ACTION)) {
    close_confirm_revealer(revealer);
  } else {
    open_confirm_revealer(revealer, LOG_OUT_ACTION, GTK_WIDGET(btn));
  }
}

static GtkWidget *create_power_settings(void) {
  GtkWidget *revealer = gtk_revealer_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_revealer_set_child(GTK_REVEALER(revealer), box);
  gtk_widget_add_css_class(box, "power-settings");
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

  GtkWidget *upper_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *lower_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_halign(upper_row, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(lower_row, GTK_ALIGN_CENTER);

  gtk_box_append(GTK_BOX(box), upper_row);
  gtk_box_append(GTK_BOX(box), lower_row);

  GtkWidget *confirm_revealer = gtk_revealer_new();
  GtkWidget *confirm_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(confirm_grid), 2);
  gtk_grid_set_column_spacing(GTK_GRID(confirm_grid), 2);
  gtk_revealer_set_child(GTK_REVEALER(confirm_revealer), confirm_grid);
  gtk_widget_set_halign(confirm_revealer, GTK_ALIGN_CENTER);

  gtk_box_append(GTK_BOX(box), confirm_revealer);

  g_object_set_data(G_OBJECT(confirm_revealer), ACTION, NULL);

  GtkWidget *confirm_label = gtk_label_new("Are you sure?");
  GtkWidget *yes_btn = gtk_button_new_with_label("Yes");
  GtkWidget *no_btn = gtk_button_new_with_label("No");
  gtk_grid_attach(GTK_GRID(confirm_grid), confirm_label, 0, 0, 2, 1);
  gtk_grid_attach(GTK_GRID(confirm_grid), yes_btn, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(confirm_grid), no_btn, 1, 1, 1, 1);

  gtk_widget_add_css_class(yes_btn, "button");
  gtk_widget_set_cursor(yes_btn, get_pointer_cursor());
  gtk_widget_add_css_class(no_btn, "button");
  gtk_widget_set_cursor(no_btn, get_pointer_cursor());

  g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_confirm),
                   confirm_revealer);
  g_signal_connect_swapped(
      no_btn, "clicked", G_CALLBACK(close_confirm_revealer), confirm_revealer);

  GtkWidget *shutdown =
      power_settings_btn("Shutdown", "system-shutdown-symbolic");
  GtkWidget *reboot = power_settings_btn("Reboot", "system-reboot-symbolic");
  GtkWidget *suspend =
      power_settings_btn("Suspend", "weather-clear-night-symbolic");
  GtkWidget *lock = power_settings_btn("Lock", "system-lock-screen-symbolic");
  GtkWidget *log_out = power_settings_btn("Log out", "system-log-out-symbolic");

  g_signal_connect(shutdown, "clicked", G_CALLBACK(on_shutdown_click),
                   confirm_revealer);
  g_signal_connect(reboot, "clicked", G_CALLBACK(on_reboot_click),
                   confirm_revealer);
  g_signal_connect(suspend, "clicked", G_CALLBACK(on_suspend_click),
                   confirm_revealer);
  g_signal_connect(lock, "clicked", G_CALLBACK(on_lock_click),
                   confirm_revealer);
  g_signal_connect(log_out, "clicked", G_CALLBACK(on_log_out_click),
                   confirm_revealer);

  gtk_box_append(GTK_BOX(upper_row), shutdown);
  gtk_box_append(GTK_BOX(upper_row), reboot);
  gtk_box_append(GTK_BOX(upper_row), suspend);
  gtk_box_append(GTK_BOX(lower_row), lock);
  gtk_box_append(GTK_BOX(lower_row), log_out);

  return revealer;
}

static void close_inner_revealer(GtkRevealer *parent_revealer) {
  GtkWidget *box = gtk_revealer_get_child(parent_revealer);
  GtkWidget *child = gtk_widget_get_first_child(box);

  while (child) {
    if (GTK_IS_REVEALER(child)) {
      close_confirm_revealer(child);
      return;
    }

    child = gtk_widget_get_next_sibling(child);
  }

  g_warning("Found no inner revealer of the power settings");
}

static void open_revealer(GtkButton *button, gpointer data) {
  GtkRevealer *revealer = GTK_REVEALER(data);
  gboolean open = gtk_revealer_get_reveal_child(revealer);
  gtk_revealer_set_reveal_child(revealer, !open);
  if (!open) { // Will be open
    gtk_widget_add_css_class(GTK_WIDGET(button), "on");
  } else { // Will be closed
    gtk_widget_remove_css_class(GTK_WIDGET(button), "on");
    close_inner_revealer(revealer);
  }
}

GtkWidget *qs_header(void) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *header_center_box = gtk_center_box_new();
  gtk_box_append(GTK_BOX(box), header_center_box);

  GtkWidget *settings_btn =
      gtk_button_new_from_icon_name("applications-system-symbolic");
  gtk_widget_add_css_class(settings_btn, "settings");
  gtk_widget_set_cursor(settings_btn, get_pointer_cursor());
  g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked),
                   NULL);

  GtkWidget *title = gtk_label_new("Settings");
  gtk_widget_add_css_class(title, "title");

  GtkWidget *power_btn = gtk_button_new_from_icon_name("system-shutdown");
  gtk_widget_add_css_class(power_btn, "power-btn");
  gtk_widget_set_cursor(power_btn, get_pointer_cursor());

  GtkWidget *power_settings = create_power_settings();
  gtk_box_append(GTK_BOX(box), power_settings);
  g_signal_connect(power_btn, "clicked", G_CALLBACK(open_revealer),
                   power_settings);

  gtk_center_box_set_start_widget(GTK_CENTER_BOX(header_center_box),
                                  settings_btn);
  gtk_center_box_set_center_widget(GTK_CENTER_BOX(header_center_box), title);
  gtk_center_box_set_end_widget(GTK_CENTER_BOX(header_center_box), power_btn);

  return box;
}
