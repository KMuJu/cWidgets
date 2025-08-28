#include "bluetooth_page.h"
#include "adapter.h"
#include "bt.h"
#include "device.h"
#include "page.h"
#include "util.h"
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <gtk/gtkrevealer.h>

#define MAX_NAME_LEN 20
#define ENTRY_HEIGHT 30

typedef struct {
  Device *current_device;
  Bluetooth *bt;
} BluetoothData;

static void launch_bt_settings(void) { sh("blueberry"); }

static void bluetooth_discover(GtkSwitch *self, gboolean state,
                               gpointer user_data) {
  BluetoothData *bd = user_data;
  Adapter *adapter = bluetooth_get_adapter(bd->bt);
  adapter_set_discovering(adapter, state);
}

static void toggle_bluetooth(GtkButton *self, gpointer data) {
  Bluetooth *bt = data;
  Adapter *adapter = bluetooth_get_adapter(bt);
  gboolean powered = adapter_get_powered(adapter);
  adapter_set_powered(adapter, !powered);
}

static void on_powered_changed(Bluetooth *self, gboolean powered,
                               gpointer user_data) {
  PageButton *pb = user_data;
  if (powered) {
    gtk_widget_remove_css_class(pb->toggle_btn, "off");
    gtk_image_set_from_icon_name(GTK_IMAGE(pb->image), "bluetooth-symbolic");
  } else {
    gtk_widget_add_css_class(pb->toggle_btn, "off");
    gtk_image_set_from_icon_name(GTK_IMAGE(pb->image),
                                 "bluetooth-disconnected-symbolic");
  }
}

static GtkWidget *device_entry(Device *device) {
  g_autofree gchar *name = device_get_name(device);
  if (!name)
    return NULL;

  g_autofree gchar *name_trunc = truncate_string(name, MAX_NAME_LEN);
  GtkWidget *label = gtk_label_new(name_trunc);
  gtk_widget_add_css_class(label, "entry");
  gtk_widget_set_cursor(label, get_pointer_cursor());

  return label;
}

// Return TRUE to delete from hash table, FALSE otherwise
static gboolean for_each_device(gpointer key, gpointer value,
                                gpointer user_data) {
  PageButton *pb = user_data;
  Device *device = value;
  gboolean connected = device_get_connected(device);
  if (connected) {
    g_autofree gchar *name = device_get_name(device);
    g_autofree gchar *name_trunc = truncate_string(name, MAX_NAME_LEN);
    gtk_label_set_text(GTK_LABEL(pb->active_name), name_trunc);
  }
  GtkWidget *entry = device_entry(device);
  if (entry) {
    gtk_box_append(GTK_BOX(pb->revealer_box), entry);
    return FALSE;
  }

  return TRUE;
}

static void on_devices_change(Bluetooth *bt, GHashTable *devices,
                              gpointer user_data) {
  PageButton *pb = user_data;
  GHashTable *copy = g_hash_table_new_similar(devices);

  // Copy table so that it can be pruned
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, devices);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    g_hash_table_insert(copy, g_strdup(key), g_object_ref(value));
  }

  remove_children_start(pb->revealer_box, 1);

  // removes bad devices (no name)
  g_hash_table_foreach_steal(copy, for_each_device, user_data);

  gtk_widget_set_size_request(
      pb->revealer_scrolled, -1,
      MIN(400, 50 + g_hash_table_size(copy) * ENTRY_HEIGHT));

  g_hash_table_unref(copy);
}

GtkWidget *bluetooth_page(void) {
  PageButton *pb = create_page_button("Bluetooth", "bluetooth-symbolic");
  BluetoothData *bd = g_new0(BluetoothData, 1);
  bd->bt = bluetooth_get_default();
  if (bd->bt == NULL) {
    g_warning("Could not get default bluetooth");
    return gtk_label_new("No bluetooth");
  }
  pb->page_data = (void *)bd;

  GtkWidget *entries_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(entries_header, "header");
  GtkWidget *entries_open_settings =
      gtk_button_new_from_icon_name("applications-system-symbolic");
  gtk_widget_add_css_class(entries_open_settings, "open-settings");
  gtk_widget_set_cursor(entries_open_settings, get_pointer_cursor());
  g_signal_connect(entries_open_settings, "clicked", launch_bt_settings, NULL);
  GtkWidget *page_name = gtk_label_new("Bluetooth");
  gtk_widget_set_hexpand(page_name, TRUE);
  GtkWidget *rescan_wifi_switch = gtk_switch_new();
  gtk_widget_add_css_class(rescan_wifi_switch, "bluetooth-discover");
  gtk_widget_set_cursor(rescan_wifi_switch, get_pointer_cursor());
  g_signal_connect(rescan_wifi_switch, "state-set",
                   G_CALLBACK(bluetooth_discover), bd);

  gtk_box_append(GTK_BOX(entries_header), entries_open_settings);
  gtk_box_append(GTK_BOX(entries_header), page_name);
  gtk_box_append(GTK_BOX(entries_header), rescan_wifi_switch);

  gtk_box_append(GTK_BOX(pb->revealer_box), entries_header);

  g_signal_connect(pb->toggle_btn, "clicked", G_CALLBACK(toggle_bluetooth),
                   bd->bt);

  g_signal_connect(bd->bt, "devices-changed", G_CALLBACK(on_devices_change),
                   pb);

  g_signal_connect(bd->bt, "powered", G_CALLBACK(on_powered_changed), pb);

  bluetooth_install_signals(bd->bt);
  bluetooth_call_signals(bd->bt);

  return pb->box;
}
