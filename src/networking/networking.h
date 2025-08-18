#ifndef NETOWRKING_H
#define NETOWRKING_H

#include "NetworkManager.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Returns a referenced NMDeviceWifi* (caller must unref). On error or not found
 * returns NULL. */
NMDeviceWifi *wifi_util_get_primary_wifi_device(GError **error);
NMClient *wifi_util_get_client(GError **error);

/* Force-refresh the cached device (e.g., if interfaces changed). */
void wifi_util_refresh(void);

/* Cleanup any cached objects; safe to call at program exit. */
void wifi_util_cleanup(void);

typedef enum {
  WIFI_UTIL_ERROR_NO_WIFI_DEVICE,
  WIFI_UTIL_ERROR_CLIENT_INIT_FAILED,
} WifiUtilError;

#define WIFI_UTIL_ERROR (wifi_util_error_quark())
GQuark wifi_util_error_quark(void);

gchar *ap_get_ssid(NMAccessPoint *ap);

G_END_DECLS
#endif // !NETOWRKING_H
