#include "wifi_page.h"
#include "networking.h"
#include "page.h"
#include "util.h"
#include <NetworkManager.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <gtk/gtkrevealer.h>

#define MAX_NAME_LEN 20
#define ENTRY_HEIGHT 30

typedef struct {
  NMAccessPoint *active_ap;
} WifiData;

static NMDeviceWifi *get_wifi_device(void) {
  NMDeviceWifi *wifi_device;
  GError *error;
  wifi_device = wifi_util_get_primary_wifi_device(&error);
  if (!wifi_device) {
    g_warning("Could not get the wifi_device from wifi.c: %s\n",
              error->message);
    return NULL;
  }
  return wifi_device;
}

static void launch_wifi_settings(void) {
  sh("env XDG_CURRENT_DESKTOP=gnome gnome-control-center wifi");
}

static void wifi_rescan(void) { sh("nmcli device wifi rescan"); }

static gint compare_aps(gconstpointer a_p, gconstpointer b_p) {
  const NMAccessPoint *a = a_p;
  const NMAccessPoint *b = b_p;

  if (!a || !G_IS_OBJECT(a))
    return 1;
  if (!b || !G_IS_OBJECT(b))
    return -1;

  return nm_access_point_get_strength(b) - nm_access_point_get_strength(a);
}

static GtkWidget *ap_entry(NMAccessPoint *ap, PageButton *pb) {
  if (!ap || !G_IS_OBJECT(ap)) {
    return gtk_label_new("<Invalid AP>");
  }

  g_autofree gchar *ssid_full = ap_get_ssid(ap);
  if (ssid_full == NULL) {
    return gtk_label_new("<No Name>");
  }

  g_autofree gchar *ssid = truncate_string(ssid_full, MAX_NAME_LEN);
  GtkWidget *label = gtk_label_new(ssid);
  gtk_widget_add_css_class(label, "entry");
  gtk_widget_set_cursor(label, get_pointer_cursor());
  gtk_widget_set_tooltip_text(label, ssid_full);

  WifiData *wd = (WifiData *)pb;
  if (ap == wd->active_ap) {
    gtk_widget_add_css_class(label, "active");
  }

  return label;
}

static void on_aps_changed(NMDeviceWifi *device, GParamSpec *pspec,
                           gpointer user_data) {
  PageButton *pb = user_data;
  // Should not remove the first child, since it is a header
  remove_children_start(pb->revealer_box, 1);

  const GPtrArray *aps = nm_device_wifi_get_access_points(device);
  GHashTable *seen =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  for (size_t i = 0; i < aps->len; i++) {
    NMAccessPoint *ap = g_ptr_array_index(aps, i);
    gchar *ssid = ap_get_ssid(ap);
    if (!ssid) {
      ssid = g_strdup("");
    }
    NMAccessPoint *found_ap = g_hash_table_lookup(seen, ssid);

    if (!found_ap) {
      g_hash_table_insert(seen, g_strdup(ssid), g_object_ref(ap));
    }
  }

  GPtrArray *filtered_aps = g_hash_table_get_values_as_ptr_array(seen);
  gtk_widget_set_size_request(pb->revealer_scrolled, -1,
                              MIN(400, 50 + filtered_aps->len * ENTRY_HEIGHT));

  g_hash_table_destroy(seen);

  g_ptr_array_sort(filtered_aps, compare_aps);

  for (size_t i = 0; i < filtered_aps->len; i++) {
    NMAccessPoint *ap = g_ptr_array_index(filtered_aps, i);
    GtkWidget *ap_widget = ap_entry(ap, pb);
    gtk_box_append(GTK_BOX(pb->revealer_box), ap_widget);
  }
}

static void on_wireless_enabled_notify(NMClient *client, GParamSpec *pspec,
                                       gpointer user_data) {
  PageButton *pb = user_data;
  gboolean enabled = nm_client_wireless_get_enabled(client);
  if (enabled) {
    gtk_widget_remove_css_class(pb->toggle_btn, "off");
    gtk_image_set_from_icon_name(GTK_IMAGE(pb->image),
                                 "network-wireless-symbolic");
  } else {
    gtk_widget_add_css_class(pb->toggle_btn, "off");
    gtk_image_set_from_icon_name(GTK_IMAGE(pb->image),
                                 "network-wireless-offline-symbolic");
  }
}

static void set_wireless_cb(GObject *object, GAsyncResult *result,
                            gpointer user_data) {
  GError *error = NULL;

  if (!nm_client_dbus_set_property_finish(NM_CLIENT(object), result, &error)) {
    g_printerr("Failed to set WirelessEnabled: %s\n", error->message);
    g_error_free(error);
  }
}

static void on_toggle_wifi_click(GtkButton *self, gpointer data) {
  NMClient *client = data;
  gboolean enable = !nm_client_wireless_get_enabled(client);
  nm_client_dbus_set_property(client, NM_DBUS_PATH, NM_DBUS_INTERFACE,
                              "WirelessEnabled", g_variant_new("b", enable), -1,
                              NULL, set_wireless_cb, NULL);
}

static void on_active_ap_changed(NMDeviceWifi *device, GParamSpec *pspec,
                                 gpointer user_data) {
  PageButton *pb = user_data;
  WifiData *wd = (WifiData *)pb->page_data;
  if (wd->active_ap) {
    g_object_unref(wd->active_ap);
    wd->active_ap = NULL;
  }
  GtkWidget *label = pb->active_name;
  NMAccessPoint *_ap = nm_device_wifi_get_active_access_point(device);
  if (_ap) {
    wd->active_ap = g_object_ref(_ap);
    g_autofree char *ssid_str = ap_get_ssid(wd->active_ap);
    if (!ssid_str)
      return;

    g_autofree gchar *label_str = truncate_string(ssid_str, MAX_NAME_LEN);

    gtk_label_set_text(GTK_LABEL(label), label_str);
  } else {
    gtk_label_set_text(GTK_LABEL(label), "Not connected");
  }
}

GtkWidget *wifi_page(void) {
  GError *error = NULL;
  NMClient *client = wifi_util_get_client(&error);
  if (!client) {
    g_message("Could not get nmclient in wifi page: %s", error->message);
    return gtk_box_new(GTK_ORIENTATION_VERTICAL,
                       0); // Returns empty box instead of NULL
  }
  NMDeviceWifi *wifi_device = get_wifi_device();
  if (NULL == wifi_device)
    return gtk_box_new(GTK_ORIENTATION_VERTICAL,
                       0); // Returns empty box instead of NULL

  PageButton *pb = create_page_button("Wifi", "network-wireless-symbolic");
  WifiData *wd = g_new0(WifiData, 1);
  pb->page_data = (void *)wd;

  // Header above entries
  GtkWidget *entries_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(entries_header, "header");
  GtkWidget *entries_open_settings =
      gtk_button_new_from_icon_name("applications-system-symbolic");
  gtk_widget_add_css_class(entries_open_settings, "open-settings");
  gtk_widget_set_cursor(entries_open_settings, get_pointer_cursor());
  g_signal_connect(entries_open_settings, "clicked", launch_wifi_settings,
                   NULL);
  GtkWidget *page_name = gtk_label_new("Wifi");
  gtk_widget_set_hexpand(page_name, TRUE);
  GtkWidget *rescan_wifi_btn =
      gtk_button_new_from_icon_name("view-refresh-symbolic");
  gtk_widget_add_css_class(rescan_wifi_btn, "rescan-wifi");
  gtk_widget_set_cursor(rescan_wifi_btn, get_pointer_cursor());
  g_signal_connect(rescan_wifi_btn, "clicked", wifi_rescan, NULL);

  gtk_box_append(GTK_BOX(entries_header), entries_open_settings);
  gtk_box_append(GTK_BOX(entries_header), page_name);
  gtk_box_append(GTK_BOX(entries_header), rescan_wifi_btn);

  gtk_box_append(GTK_BOX(pb->revealer_box), entries_header);

  g_signal_connect(pb->toggle_btn, "clicked", G_CALLBACK(on_toggle_wifi_click),
                   client);

  g_signal_connect(client, "notify::wireless-enabled",
                   G_CALLBACK(on_wireless_enabled_notify), pb);
  on_wireless_enabled_notify(client, NULL, pb);

  g_signal_connect(wifi_device, "notify::active-access-point",
                   G_CALLBACK(on_active_ap_changed), pb);
  on_active_ap_changed(wifi_device, NULL, pb);

  g_signal_connect(wifi_device, "notify::access-points",
                   G_CALLBACK(on_aps_changed), pb);
  on_aps_changed(wifi_device, NULL, pb);

  return pb->box;
}
