#include "networking.h"
#include "NetworkManager.h"
#include "glib-object.h"
#include "glib.h"
#include "nm-core-types.h"
#include <stdio.h>

static NMClient *global_client = NULL;
static NMDeviceWifi *cached_wifi = NULL;

static void on_client_ready(GObject *source, GAsyncResult *res,
                            gpointer user_data) {
  GError *error = NULL;
  NMClient *client;

  client = nm_client_new_finish(res, &error);
  if (!client) {
    g_printerr("Failed to create NMClient: %s\n", error->message);
    g_error_free(error);
    return;
  }
  global_client = g_object_ref(client);
  const GPtrArray *devices = nm_client_get_devices(global_client);
  if (!devices) {
    g_printerr("Failed to get devices from client\n");
    return;
  }

  for (guint i = 0; i < devices->len; i++) {
    NMDevice *device = g_ptr_array_index(devices, i);
    if (NM_IS_DEVICE_WIFI(device)) {
      NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
      /* Cache with a reference, and return a fresh ref to caller */
      if (NULL != cached_wifi)
        g_object_unref(cached_wifi);
      cached_wifi = g_object_ref(wifi);
      break;
    }
  }
  if (cached_wifi == NULL) {
    g_printerr("Could not get a wifi device\n");
    return;
  }

  NetInitData *data = user_data;
  data->run(data->ctx);
}

void net_init(NetInitData *data) {
  nm_client_new_async(NULL, on_client_ready, data);
}

NMClient *net_get_client(void) { return g_object_ref(global_client); }
NMDeviceWifi *net_get_wifi_device(void) { return g_object_ref(cached_wifi); }

gchar *ap_get_ssid(NMAccessPoint *ap) {
  if (!ap) {
    return NULL;
  }
  gsize len;
  GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
  if (!ssid_bytes) {
    return NULL;
  }

  const guint8 *ssid_data = g_bytes_get_data(ssid_bytes, &len);
  if (!ssid_data || len > 32 || (uintptr_t)ssid_data < 0xfffff) {
    g_warning("ssid_data is invalid(%p) or len to long, len(%lu)", ssid_data,
              len);
    return NULL;
  }
  return nm_utils_ssid_to_utf8(ssid_data, len);
}

gchar *wireless_setting_get_ssid(NMSettingWireless *setting) {
  if (!setting) {
    return NULL;
  }
  gsize len;
  GBytes *ssid_bytes = nm_setting_wireless_get_ssid(setting);
  if (!ssid_bytes) {
    return NULL;
  }

  const guint8 *ssid_data = g_bytes_get_data(ssid_bytes, &len);
  if (!ssid_data || len > 32 || (uintptr_t)ssid_data < 0xfffff) {
    g_warning(
        "Wireless setting ssid_data is invalid(%p) or len to long, len(%lu)",
        ssid_data, len);
    return NULL;
  }
  return nm_utils_ssid_to_utf8(ssid_data, len);
}
