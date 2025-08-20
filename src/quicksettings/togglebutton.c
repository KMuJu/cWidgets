#include "togglebutton.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "util.h"

static ToggleButtonProps props = {.icon_on = "preferences-system-notifications",
                                  .icon_off = "notifications-disabled-symbolic",
                                  .cmd = "dunstctl set-paused toggle"};

#define off "off"

static void on_toggle_click(GtkWidget *btn, gpointer data) {
  ToggleButtonProps *tbp = data;
  const gchar *cmd = tbp->cmd;
  GError *error = NULL;
  if (!g_spawn_command_line_async(cmd, &error)) {
    g_printerr("Failed to call cmd on toggle click: %s\n", error->message);
    g_error_free(error);
    return;
  }
  gboolean is_off = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), off));

  if (is_off) {
    gtk_widget_remove_css_class(btn, off);
  } else {
    gtk_widget_add_css_class(btn, off);
  }

  g_object_set_data(G_OBJECT(btn), off, GINT_TO_POINTER(!is_off));

  GtkWidget *child = gtk_button_get_child(GTK_BUTTON(btn));
  if (child == NULL) {
    g_warning("Toggle button has no child");
    return;
  }
  if (!GTK_IS_IMAGE(child)) {
    g_warning("Toggle button child is not an image");
    return;
  }
  gtk_image_set_from_icon_name(GTK_IMAGE(child),
                               is_off ? tbp->icon_on : tbp->icon_off);
}

GtkWidget *togglebutton(ToggleButtonProps *props, gboolean is_off) {
  GtkWidget *btn =
      gtk_button_new_from_icon_name(is_off ? props->icon_off : props->icon_on);
  gtk_widget_add_css_class(btn, "toggle-button");
  if (is_off)
    gtk_widget_add_css_class(btn, off);
  g_object_set_data(G_OBJECT(btn), off, GINT_TO_POINTER(is_off));
  gtk_widget_set_cursor(btn, get_pointer_cursor());

  g_signal_connect(btn, "clicked", G_CALLBACK(on_toggle_click), props);

  return btn;
}

gboolean get_notification_state(void) {
  gchar *stdout_buf = NULL;
  GError *error = NULL;
  gboolean paused = FALSE;
  if (!g_spawn_command_line_sync("dunstctl is-paused", &stdout_buf, NULL, NULL,
                                 &error)) {
    g_printerr("Failed to call cmd on toggle click: %s\n", error->message);
    g_error_free(error);
  } else {
    if (stdout_buf) {
      g_strstrip(stdout_buf);

      if (g_strcmp0(stdout_buf, "true") == 0)
        paused = TRUE;
    }
  }
  return paused;
}

GtkWidget *notification_button(void) {

  gboolean is_off = !get_notification_state();

  GtkWidget *btn = togglebutton(&props, is_off);

  return btn;
}

static void call_colorpicker(void) {
  GError *error = NULL;
  if (!g_spawn_command_line_async("hyprpicker -a", &error)) {
    g_printerr("Failed to call hyprpicker: %s\n", error->message);
    g_error_free(error);
    return;
  }
}

GtkWidget *colorpicker_button(void) {
  GtkWidget *colorpicker =
      gtk_button_new_from_icon_name("color-select-symbolic");
  gtk_widget_add_css_class(colorpicker, "button");
  gtk_widget_set_cursor(colorpicker, get_pointer_cursor());

  g_signal_connect(colorpicker, "clicked", G_CALLBACK(call_colorpicker), NULL);

  return colorpicker;
}
