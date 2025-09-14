#include "wifi_page.h"
#include "gtk/gtkshortcut.h"
#include "networking.h"
#include "nm-core-types.h"
#include "nm-dbus-interface.h"
#include "page.h"
#include "util.h"
#include <NetworkManager.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <gtk/gtkrevealer.h>

#define MAX_NAME_LEN 24
#define ENTRY_HEIGHT 30

typedef struct {
  NMClient *client;
  NMAccessPoint *active_ap;
} WifiData;

static GPtrArray *cached_connections = NULL;

static void on_connection_added(NMClient *client, NMRemoteConnection *conn,
                                gpointer user_data) {
  g_ptr_array_add(cached_connections, g_object_ref(conn));
}

static void on_connection_removed(NMClient *client, NMRemoteConnection *conn,
                                  gpointer user_data) {
  for (guint i = 0; i < cached_connections->len; i++) {
    NMRemoteConnection *c = g_ptr_array_index(cached_connections, i);
    if (c == conn) {
      g_ptr_array_remove_index(cached_connections, i);
      break;
    }
  }
}

static void get_all_connections(NMClient *client) {
  const GPtrArray *conns = nm_client_get_connections(client);
  for (size_t i = 0; i < conns->len; i++) {
    NMRemoteConnection *conn = g_ptr_array_index(conns, i);
    on_connection_added(client, conn, NULL);
  }
}

static gboolean ap_is_known(NMAccessPoint *ap) {
  g_autofree char *ap_ssid = ap_get_ssid(ap);
  if (!ap_ssid)
    return FALSE;
  for (size_t i = 0; i < cached_connections->len; i++) {
    NMRemoteConnection *conn = g_ptr_array_index(cached_connections, i);
    NMSettingWireless *setting =
        nm_connection_get_setting_wireless(NM_CONNECTION(conn));
    g_autofree gchar *conn_ssid = wireless_setting_get_ssid(setting);

    if (conn_ssid && g_str_equal(ap_ssid, conn_ssid))
      return TRUE;
  }
  return FALSE;
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

  return nm_access_point_get_strength((NMAccessPoint *)b) -
         nm_access_point_get_strength((NMAccessPoint *)a);
}

static gboolean is_ap_secured(NMAccessPoint *ap) {
  NM80211ApFlags flags = nm_access_point_get_flags(ap);
  NM80211ApSecurityFlags wpa = nm_access_point_get_wpa_flags(ap);
  NM80211ApSecurityFlags rsn = nm_access_point_get_rsn_flags(ap);

  return wpa != 0 || rsn != 0 || (flags & 0x1) == 1;
}

static void forget_ap(gchar *ssid) {
  char cmd[512] = {0};
  g_message("Forgetting: %s", ssid);
  snprintf(cmd, sizeof(cmd), "nmcli con del id %s", ssid);
  sh(cmd);
  // This is to rerun the on_aps_changed to update the known aps.
  // It might not work, this is weird, might run before the previous cmd is
  // finished.
  sh("nmcli dev wifi rescan");
}

static void open_revealer_on_button_click(GtkButton *btn, gpointer user_data) {
  GtkRevealer *revealer = GTK_REVEALER(user_data);
  gboolean open = gtk_revealer_get_reveal_child(revealer);
  gtk_revealer_set_reveal_child(revealer, !open);
}

static void password_entered(GtkPasswordEntry *entry, gpointer user_data) {
  NMAccessPoint *ap = NM_ACCESS_POINT(user_data);
  const char *password = gtk_editable_get_text(GTK_EDITABLE(entry));
  g_print("Password entered: %s\n", password);
  g_autofree gchar *ssid = ap_get_ssid(ap);

  char cmd[512] = {0};
  snprintf(cmd, sizeof(cmd), "nmcli dev wifi connect '%s' password '%s'", ssid,
           password);
  sh(cmd);
  // Same here as in forget ap
  sh("nmcli dev wifi rescan");
}

// TODO: Do something about signals for the ap
// Might have to add a custom unref function to unconnect the signals
static GtkWidget *ap_entry(NMAccessPoint *ap, PageButton *pb) {
  if (!ap || !G_IS_OBJECT(ap)) {
    return gtk_label_new("<Invalid AP>");
  }

  g_autofree gchar *ssid_full = ap_get_ssid(ap);
  if (ssid_full == NULL) {
    return gtk_label_new("<No Name>");
  }

  // State for ap
  g_autofree gchar *ssid = truncate_string(ssid_full, MAX_NAME_LEN);
  gboolean secured = is_ap_secured(ap);
  gboolean known = ap_is_known(ap);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(box, "entry-box");
  GtkWidget *button = gtk_button_new();
  GtkWidget *entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_button_set_child(GTK_BUTTON(button), entry_box);
  GtkWidget *image = gtk_image_new_from_icon_name(
      "network-wireless-signal-excellent-symbolic");
  GtkWidget *name = gtk_label_new(ssid);
  gtk_widget_set_hexpand(name, TRUE);

  gtk_box_append(GTK_BOX(entry_box), image);
  gtk_box_append(GTK_BOX(entry_box), name);

  if (secured && !known) {
    GtkWidget *lock =
        gtk_image_new_from_icon_name("system-lock-screen-symbolic");

    gtk_box_append(GTK_BOX(entry_box), lock);
  }

  gtk_widget_add_css_class(button, "entry");
  gtk_widget_set_cursor(button, get_pointer_cursor());
  gtk_widget_set_tooltip_text(button, ssid_full);

  gtk_box_append(GTK_BOX(box), button);

  if (known)
    gtk_widget_add_css_class(button, "known");

  WifiData *wd = (WifiData *)pb;
  if (ap == wd->active_ap) {
    gtk_widget_add_css_class(button, "active");
  }

  if (secured || known) {
    GtkWidget *revealer = gtk_revealer_new();
    GtkWidget *entry_revealer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(entry_revealer_box, "options-box");
    gtk_revealer_set_child(GTK_REVEALER(revealer), entry_revealer_box);

    if (!known) { // Is secured and not known -> connect button with password
      GtkWidget *button = gtk_button_new_with_label("Connect");
      gtk_widget_set_cursor_from_name(button, "pointer");
      gtk_widget_add_css_class(button, "option-btn");
      gtk_box_append(GTK_BOX(entry_revealer_box), button);
      GtkWidget *inner_revealer = gtk_revealer_new();
      GtkWidget *inner_revealer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
      GtkWidget *password_entry = gtk_password_entry_new();
      gtk_box_append(GTK_BOX(entry_revealer_box), inner_revealer);
      gtk_revealer_set_child(GTK_REVEALER(inner_revealer), inner_revealer_box);
      gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(password_entry),
                                            TRUE);
      gtk_box_append(GTK_BOX(inner_revealer_box), password_entry);

      g_signal_connect(password_entry, "activate", G_CALLBACK(password_entered),
                       ap);

      g_signal_connect(button, "clicked",
                       G_CALLBACK(open_revealer_on_button_click),
                       inner_revealer);
    } else { // Remove connection
      GtkWidget *button = gtk_button_new_with_label("Forget");
      gtk_widget_add_css_class(button, "option-btn");
      gtk_widget_set_cursor_from_name(button, "pointer");
      gtk_box_append(GTK_BOX(entry_revealer_box), button);

      g_signal_connect_swapped(button, "clicked", G_CALLBACK(forget_ap), ssid);
    }

    gtk_box_append(GTK_BOX(box), revealer);

    g_signal_connect(button, "clicked",
                     G_CALLBACK(open_revealer_on_button_click), revealer);
  }

  return box;
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
  NMClient *client = net_get_client();
  if (!client) {
    g_message("Could not get nmclient in wifi page");
    return gtk_box_new(GTK_ORIENTATION_VERTICAL,
                       0); // Returns empty box instead of NULL
  }
  NMDeviceWifi *wifi_device = net_get_wifi_device();
  if (NULL == wifi_device)
    return gtk_box_new(GTK_ORIENTATION_VERTICAL,
                       0); // Returns empty box instead of NULL

  cached_connections = g_ptr_array_new_with_free_func(g_object_unref);

  PageButton *pb = create_page_button("Wifi", "network-wireless-symbolic");
  WifiData *wd = g_new0(WifiData, 1);
  wd->client = client;
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

  // Connection signals
  // Should happen before all access points
  g_signal_connect(client, "connection-added", G_CALLBACK(on_connection_added),
                   NULL);
  g_signal_connect(client, "connection-removed",
                   G_CALLBACK(on_connection_removed), NULL);
  get_all_connections(client);

  // Enabled
  g_signal_connect(client, "notify::wireless-enabled",
                   G_CALLBACK(on_wireless_enabled_notify), pb);
  on_wireless_enabled_notify(client, NULL, pb);

  // Active access point
  g_signal_connect(wifi_device, "notify::active-access-point",
                   G_CALLBACK(on_active_ap_changed), pb);
  on_active_ap_changed(wifi_device, NULL, pb);

  // All access points
  g_signal_connect(wifi_device, "notify::access-points",
                   G_CALLBACK(on_aps_changed), pb);
  on_aps_changed(wifi_device, NULL, pb);

  return pb->box;
}
