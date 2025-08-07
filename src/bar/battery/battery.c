#include "battery.h"
#include "gio/gio.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <dirent.h>
#include <stdint.h>
#include <string.h>

char *dbus_path = "/org/freedesktop/UPower/devices/DisplayDevice";

struct BatteryWidgets {
  GtkWidget *label;
  GtkWidget *image;
};

static void battery_ui_refresh(gpointer data, int level) {
  struct BatteryWidgets *bw = data;
  GtkWidget *label = bw->label;
  GtkWidget *image = bw->image;

  int last_digit = level % 10;
  int level_icon = (level / 10) * 10;
  if (last_digit >= 5)
    level_icon += 10;

  char icon_name[32];
  snprintf(icon_name, sizeof(icon_name), "battery-level-%d-symbolic",
           level_icon);

  char percent_str[8];
  snprintf(percent_str, sizeof(percent_str), "%d%%", level);

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

  g_variant_get(changed_properties, "a{sv}", &iter);
  while (g_variant_iter_next(iter, "{sv}", &key, &value)) {
    if (strcmp(key, "Energy") == 0) {
      energy = g_variant_get_double(value);
      g_message("Energy: %f, EnergyPercentage: %d\n", energy,
                (int)((energy * 100) / energyFull));
      battery_ui_refresh(user_data, (int)((energy * 100) / energyFull));
    }
    g_variant_unref(value);
  }
  g_variant_iter_free(iter);
}

void start_battery_widget(GtkWidget *box, GDBusConnection *connection) {
  GtkWidget *image = gtk_image_new_from_icon_name("battery-symbolic");
  GtkWidget *label = gtk_label_new("...");

  gtk_box_append(GTK_BOX(box), image);
  gtk_box_append(GTK_BOX(box), label);

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

  GVariant *percentage = g_dbus_proxy_get_cached_property(proxy, "Percentage");
  if (percentage)
    g_variant_print(percentage, TRUE);

  // Get initial Percentage value
  GVariant *value = g_dbus_proxy_get_cached_property(proxy, "Percentage");
  if (!value) {
    g_printerr("Could not get cached property\n");
    return;
  }

  if (value) {
    int level = (int)(g_variant_get_double(value) + 0.5);
    battery_ui_refresh(bw, level);
    g_variant_unref(value);
  } else {
    g_printerr("Failed to get initial battery percentage\n");
  }

  // Connect to PropertiesChanged via proxy
  g_signal_connect(proxy, "g-properties-changed",
                   G_CALLBACK(on_proxy_properties_changed), bw);
}
