#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include <gtk/gtk.h>
#include <stdio.h>

#include <NetworkManager.h>

#include "networkmanager.h"

// Expects only one wifi device on the system
// AP *active_ap;
static GtkWidget *available_aps;

static char *get_ssid_from_ap(NMAccessPoint *ap) {
  if (!ap)
    return g_strdup("(null)");

  // Hold a reference so the AP can't disappear mid-use
  NMAccessPoint *ap_ref = g_object_ref(ap);
  char *ssid = NULL;

  GBytes *ssid_bytes = nm_access_point_get_ssid(ap_ref);
  if (ssid_bytes) {
    gsize len = 0;
    const guint8 *ssid_data = g_bytes_get_data(ssid_bytes, &len);
    if (len > 256)
      return NULL;
    if (ssid_data && len > 0) {
      // Defensive local copy in case the original backing store is unstable.
      guint8 local_buf[257] = {0};
      memcpy(local_buf, ssid_data, len);

      g_print("len: %zu\n", len);
      ssid = nm_utils_ssid_to_utf8(local_buf, len);
      g_print("ABABABAB\n");
      // if (ssid && !g_utf8_validate(ssid, -1, NULL)) {
      //   g_print("Invalid utf8\n");
      //   g_free(ssid);
      //   ssid = g_strdup("(invalid utf8)");
      // }
    }
    g_print("Unreffing\n");
    g_bytes_unref(ssid_bytes);
  }

  if (!ssid)
    ssid = g_strdup("(unknown)");

  g_object_unref(ap_ref);
  return ssid; // caller must g_free()
}

static void on_strength_changed(NMAccessPoint *ap, GParamSpec *pspec,
                                gpointer user_data) {
  CurrentApState *s = user_data;
  GtkWidget *strength_label = s->strength_label;
  guint8 strength = nm_access_point_get_strength(ap); // 0–100%
  // active_ap->strength = strength;
  g_print("AP strength changed: %d\n", strength);

  char text[32];
  snprintf(text, sizeof(text), "%d", strength);
  gtk_label_set_text(GTK_LABEL(strength_label), text);
}

static void on_active_ap_changed(NMDeviceWifi *device, GParamSpec *pspec,
                                 gpointer user_data) {
  CurrentApState *s = user_data;
  GtkWidget *wifi_label = s->wifi_label;
  GtkWidget *strength_label = s->strength_label;

  if (s->strength_handler_id) {
    g_signal_handler_disconnect(s->previous_ap, s->strength_handler_id);
    g_clear_object(&s->previous_ap);
    s->strength_handler_id = 0;
  }

  NMAccessPoint *ap = nm_device_wifi_get_active_access_point(device);
  if (ap) {
    s->previous_ap = g_object_ref(ap);
    s->strength_handler_id = g_signal_connect(
        ap, "notify::strength", G_CALLBACK(on_strength_changed), s);

    on_strength_changed(ap, NULL, s);

    char *ssid = get_ssid_from_ap(ap);
    if (NULL == ssid)
      ssid = "(NULL)";

    g_print("Active AP changed: %s\n", ssid ? ssid : "(unknown)");

    gtk_label_set_text(GTK_LABEL(wifi_label), ssid);
    g_free(ssid);
  } else {
    g_print("No active AP.\n");
    // g_free(active_ap->ssid);
    // active_ap->ap = NULL;
  }
}

static void on_aps_changed(NMDeviceWifi *device, GParamSpec *pspec,
                           gpointer user_data) {
  const GPtrArray *raw_access_points = nm_device_wifi_get_access_points(device);
  // Make a stable, ref'd copy of the AP pointers to avoid races during
  // iteration
  GPtrArray *access_points = g_ptr_array_new_with_free_func(g_object_unref);
  for (guint i = 0; i < raw_access_points->len; i++) {
    NMAccessPoint *ap = NM_ACCESS_POINT(raw_access_points->pdata[i]);
    if (ap)
      g_ptr_array_add(access_points, g_object_ref(ap)); // own each AP
  }

  g_print("Clearing children\n");

  // Clear previous list
  GtkWidget *child = gtk_widget_get_first_child(available_aps);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_widget_unparent(child);
    child = next;
  }

  g_print("Creating hash_table\n");
  // Map SSID -> best NMAccessPoint* (highest strength)
  GHashTable *best_by_ssid = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_object_unref);

  g_print("Looping over aps\n");
  for (guint i = 0; i < access_points->len; i++) {
    NMAccessPoint *ap = access_points->pdata[i];

    GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
    char *ssid;
    if (ssid_bytes)
      ssid = nm_utils_ssid_to_utf8(g_bytes_get_data(ssid_bytes, NULL),
                                   g_bytes_get_size(ssid_bytes));
    else
      ssid = g_strdup("--");

    guint8 strength = nm_access_point_get_strength(ap);
    NMAccessPoint *existing = g_hash_table_lookup(best_by_ssid, ssid);

    if (!existing) {
      g_hash_table_insert(best_by_ssid, g_strdup(ssid), g_object_ref(ap));
    } else {
      guint8 existing_strength = nm_access_point_get_strength(existing);
      if (strength > existing_strength) {
        // Replace with the stronger AP
        g_hash_table_replace(best_by_ssid, g_strdup(ssid), g_object_ref(ap));
      }
    }

    g_free(ssid);
  }

  g_print("Iterating over hashtable\n");
  // Display deduped list
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, best_by_ssid);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    char *ssid = key;
    NMAccessPoint *ap = value;
    guint8 strength = nm_access_point_get_strength(ap);
    char label_text[128];
    snprintf(label_text, sizeof(label_text), "%s (%d%%)", ssid, strength);
    GtkWidget *ap_label = gtk_label_new(label_text);
    gtk_box_append(GTK_BOX(available_aps), ap_label);
  }

  g_print("Destrouing hashtable\n");
  g_hash_table_destroy(best_by_ssid);
  g_print("Aps changed has finished\n");
}

static void rescan(GtkWidget *button, gpointer data) {
  CurrentApState *s = data;
  g_print("Requested a rescan\n");
  nm_device_wifi_request_scan_async(s->wifi_device, FALSE, NULL, NULL);
}

// Will not work proparly if there is more than one wifi device
int network_init(NMClient *client, CurrentApState *s) {
  // active_ap = g_new0(AP, 1);
  int i;
  const GPtrArray *devices = nm_client_get_devices(client);
  for (i = 0; i < devices->len; i++) {
    NMDevice *device = devices->pdata[i];
    const char *name = nm_device_get_iface(device);

    if (!NM_IS_DEVICE_WIFI(device))
      continue;

    s->wifi_device = NM_DEVICE_WIFI(device);
    break;
  }
  if (NULL == s->wifi_device) {
    fprintf(stderr, "Could not find a wifi_device\n");
    return 1;
  }

  g_signal_connect(s->wifi_device, "notify::active-access-point",
                   G_CALLBACK(on_active_ap_changed), s);

  on_active_ap_changed(s->wifi_device, NULL, s);

  g_signal_connect(s->wifi_device, "notify::access-points",
                   G_CALLBACK(on_aps_changed), NULL);

  on_aps_changed(s->wifi_device, NULL, NULL);

  return 0;
}

static void activate(GtkApplication *app, gpointer user_data) {
  CurrentApState *s = g_new0(CurrentApState, 1);
  GtkWidget *window;
  GtkWidget *outer_box;
  GtkWidget *wifi_box;
  GtkWidget *wifi_label;
  GtkWidget *strength_label;
  GtkWidget *rescan_button;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Window");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

  outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(outer_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(outer_box, GTK_ALIGN_CENTER);
  gtk_window_set_child(GTK_WINDOW(window), outer_box);

  wifi_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(wifi_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(wifi_box, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(outer_box), wifi_box);

  wifi_label = gtk_label_new("Connecting...");
  strength_label = gtk_label_new("Connecting...");

  gtk_box_append(GTK_BOX(wifi_box), wifi_label);
  gtk_box_append(GTK_BOX(wifi_box), strength_label);

  rescan_button = gtk_button_new_with_label("scan");
  g_signal_connect(rescan_button, "clicked", G_CALLBACK(rescan), s);

  gtk_box_append(GTK_BOX(outer_box), rescan_button);

  available_aps = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_halign(available_aps, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(available_aps, GTK_ALIGN_CENTER);

  gtk_box_append(GTK_BOX(outer_box), available_aps);

  s->wifi_label = wifi_label;
  s->strength_label = strength_label;

  // Initialize NetworkManager now that UI exists
  NMClient *client;
  GError *error = NULL;
  client = nm_client_new(NULL, &error);
  if (!client) {
    g_printerr("Failed to create NMClient: %s\n", error->message);
    g_error_free(error);
  } else if (!nm_client_get_nm_running(client)) {
    g_printerr("NetworkManager is not running.\n");
    g_object_unref(client);
  } else {
    if (network_init(client, s) != 0) {
      g_printerr("network_init failed.\n");
    }
    // keep client around if needed; you can store it somewhere or unref on exit
  }

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
  const GPtrArray *connections;

  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  // g_clear_object(&previous_ap);

  return status;
}
