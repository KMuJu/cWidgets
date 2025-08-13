#include "battery.h"
#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkrevealer.h"
#include "gtk/gtkshortcut.h"
#include "util.h"
#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

char *dbus_path = "/org/freedesktop/UPower/devices/DisplayDevice";

struct BatteryWidgets {
  GtkWidget *label;
  GtkWidget *image;
  double energyFull;
};

struct PowerProfile {
  char *label;
  char *cmd;
};

static void battery_ui_refresh(gpointer data, int energy, gboolean charging) {
  struct BatteryWidgets *bw = data;
  GtkWidget *label = bw->label;
  GtkWidget *image = bw->image;
  double energyFull = bw->energyFull;

  int percentage = (int)((energy * 100) / energyFull);
  int last_digit = percentage % 10;
  int level_icon = (percentage / 10) * 10;
  if (last_digit >= 5)
    level_icon += 10;

  char *state = charging ? "-charging" : "";
  char icon_name[36];
  snprintf(icon_name, sizeof(icon_name), "battery-level-%d%s-symbolic",
           level_icon, state);

  char percent_str[8];
  snprintf(percent_str, sizeof(percent_str), "%d%%", percentage);

  gtk_label_set_label(GTK_LABEL(label), percent_str);
  gtk_image_set_from_icon_name(GTK_IMAGE(image), icon_name);
}

static void on_proxy_properties_changed(GDBusProxy *proxy,
                                        GVariant *changed_properties,
                                        GStrv invalidated_properties,
                                        gpointer user_data) {
  GVariantIter *iter;
  const gchar *key;
  GVariant *value;
  double energy;

  GVariant *full = g_dbus_proxy_get_cached_property(proxy, "EnergyFull");
  double energyFull = g_variant_get_double(full);

  // a bit ugly, but sets the energyFull member
  ((struct BatteryWidgets *)user_data)->energyFull = energyFull;

  GVariant *state_v = g_dbus_proxy_get_cached_property(proxy, "State");
  const uint state = state_v ? g_variant_get_uint32(state_v) : 0;
  const gboolean charging =
      state == 1 /*Charging*/ || state == 4 /*Fully charged*/;

  g_variant_get(changed_properties, "a{sv}", &iter);
  while (g_variant_iter_next(iter, "{sv}", &key, &value)) {
    if (strcmp(key, "Energy") == 0) {
      energy = g_variant_get_double(value);
      battery_ui_refresh(user_data, energy, charging);

      g_variant_unref(value);
      break;
    }
  }
  g_variant_iter_free(iter);
}

int initial_sync(GDBusProxy *proxy, struct BatteryWidgets *bw) {
  // Get initial Percentage value
  GVariant *state_v = g_dbus_proxy_get_cached_property(proxy, "State");
  if (!state_v) {
    g_printerr("Could not get state\n");
    return 1;
  }
  const uint state = state_v ? g_variant_get_uint32(state_v) : 0;
  const gboolean charging =
      state == 1 /*Charging*/ || state == 4 /*Fully charged*/;

  GVariant *full = g_dbus_proxy_get_cached_property(proxy, "EnergyFull");
  if (!full) {
    g_printerr("Could not get EnergyFull\n");
    return 1;
  }
  double energyFull = g_variant_get_double(full);
  bw->energyFull = energyFull;

  GVariant *energy = g_dbus_proxy_get_cached_property(proxy, "Energy");
  if (!energy) {
    g_printerr("Could not get cached property\n");
    return 1;
  }

  if (energy) {
    int level = (int)(g_variant_get_double(energy) + 0.5);
    battery_ui_refresh(bw, level, charging);
    g_variant_unref(energy);
  } else {
    g_printerr("Failed to get initial battery percentage\n");
    return 1;
  }

  return 0;
}

static void on_battery_button_click(GtkButton *self, gpointer data) {
  GtkWidget *revealer = data;
  gboolean open = gtk_revealer_get_child_revealed(GTK_REVEALER(revealer));
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), !open);
}

static void on_power_profile_button_click(GtkButton *self, gpointer data) {
  char *cmd = data;
  GError *error = NULL;
  gboolean success =
      g_spawn_sync(NULL, // working dir
                   (gchar *[]){"powerprofilesctl", "set", cmd, NULL},
                   NULL,                // envp
                   G_SPAWN_SEARCH_PATH, // flags
                   NULL,                // child setup
                   NULL,                // user data
                   NULL,                // stdout
                   NULL,                // stderr
                   NULL,                // exit status
                   &error);

  if (!success) {
    g_printerr("Error: %s\n", error->message);
    g_error_free(error);
  }
}

void start_battery_widget(GtkWidget *box, GDBusConnection *connection) {
  GtkWidget *battery_button = gtk_button_new();
  GtkWidget *battery_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *image = gtk_image_new_from_icon_name("battery-symbolic");
  GtkWidget *label = gtk_label_new("...");

  GdkCursor *pointer = get_pointer_cursor();
  gtk_widget_set_cursor(battery_button, pointer);

  gtk_box_append(GTK_BOX(battery_box), image);
  gtk_box_append(GTK_BOX(battery_box), label);
  gtk_button_set_child(GTK_BUTTON(battery_button), battery_box);
  gtk_box_append(GTK_BOX(box), battery_button);

  GtkWidget *revealer = gtk_revealer_new();
  GtkWidget *revealer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_revealer_set_child(GTK_REVEALER(revealer), revealer_box);
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);

  gtk_widget_add_css_class(revealer_box, "power_profiles");

  struct PowerProfile power_profiles[3] = {
      {.label = "Performance", .cmd = "performance"},
      {.label = "Balanced", .cmd = "balanced"},
      {.label = "Power saver", .cmd = "power-saver"}};

  for (size_t i = 0; i < sizeof(power_profiles) / sizeof(struct PowerProfile);
       i++) {
    struct PowerProfile pp = power_profiles[i];
    GtkWidget *button = gtk_button_new_with_label(pp.label);
    gtk_widget_set_cursor(button, pointer);

    g_signal_connect(button, "clicked",
                     G_CALLBACK(on_power_profile_button_click), pp.cmd);
    gtk_box_append(GTK_BOX(revealer_box), button);
  }

  gtk_box_append(GTK_BOX(box), revealer);

  g_signal_connect(battery_button, "clicked",
                   G_CALLBACK(on_battery_button_click), revealer);

  struct BatteryWidgets *bw = calloc(1, sizeof(struct BatteryWidgets));
  bw->label = label;
  bw->image = image;

  // Create proxy for the battery device
  GError *error = NULL;
  GDBusProxy *proxy =
      g_dbus_proxy_new_sync(connection, G_DBUS_PROXY_FLAGS_NONE,
                            NULL, // Interface info (NULL = auto introspect)
                            "org.freedesktop.UPower", dbus_path,
                            "org.freedesktop.UPower.Device", NULL, &error);

  if (!proxy) {
    g_printerr("Failed to create proxy: %s\n", error->message);
    g_error_free(error);
    return;
  }

  initial_sync(proxy, bw);

  // Connect to PropertiesChanged via proxy
  g_signal_connect(proxy, "g-properties-changed",
                   G_CALLBACK(on_proxy_properties_changed), bw);
}
