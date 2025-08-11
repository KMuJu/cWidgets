#include "networking.h"
#include "NetworkManager.h"
#include "glib-object.h"
#include "glib.h"
#include <stdio.h>

static NMClient *global_client = NULL;
static NMDeviceWifi *cached_wifi = NULL;
static GMutex init_mutex;

GQuark wifi_util_error_quark(void) {
  return g_quark_from_static_string("wifi-util-error-quark");
}

static gboolean ensure_client(GError **error) {
  if (global_client)
    return TRUE;
  g_mutex_init(&init_mutex);

  g_mutex_lock(&init_mutex);
  if (!global_client) {
    GError *local_error = NULL;
    global_client = nm_client_new(NULL, &local_error);
    if (!global_client) {
      if (error)
        *error = g_error_copy(local_error);
      g_error_free(local_error);
      g_mutex_unlock(&init_mutex);
      return FALSE;
    }
  }
  g_mutex_unlock(&init_mutex);
  return TRUE;
}

NMDeviceWifi *wifi_util_get_primary_wifi_device(GError **error) {
  if (cached_wifi)
    return g_object_ref(cached_wifi); /* return new ref */

  if (!ensure_client(error))
    return NULL;

  const GPtrArray *devices = nm_client_get_devices(global_client);
  if (!devices) {
    if (error)
      *error = g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                   "Failed to get devices from NMClient");
    return NULL;
  }

  for (guint i = 0; i < devices->len; i++) {
    NMDevice *device = g_ptr_array_index(devices, i);
    if (NM_IS_DEVICE_WIFI(device)) {
      NMDeviceWifi *wifi = NM_DEVICE_WIFI(device);
      /* Cache with a reference, and return a fresh ref to caller */
      if (NULL != cached_wifi)
        g_object_unref(cached_wifi);
      cached_wifi = g_object_ref(wifi);
      return g_object_ref(cached_wifi);
    }
  }

  if (error)
    *error =
        g_error_new_literal(WIFI_UTIL_ERROR, WIFI_UTIL_ERROR_NO_WIFI_DEVICE,
                            "No Wi-Fi device found");
  return NULL;
}

void wifi_util_refresh(void) {
  g_mutex_lock(&init_mutex);
  g_clear_object(&cached_wifi);
  g_mutex_unlock(&init_mutex);
}

void wifi_util_cleanup(void) {
  g_mutex_lock(&init_mutex);
  g_clear_object(&cached_wifi);
  g_clear_object(&global_client);
  g_mutex_unlock(&init_mutex);
}
