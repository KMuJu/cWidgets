#include <NetworkManager.h>
#include <glib.h>
#include <gtk/gtk.h>

#define NETWORK_MONITOR_IS_ACTIVE(appeared, vanished)                          \
  g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.freedesktop.NetworkManager",        \
                   G_BUS_NAME_WATCHER_FLAGS_NONE, appeared, vanished, NULL,    \
                   NULL)

typedef struct {
  NMDeviceWifi *wifi_device;
  NMAccessPoint *previous_ap;
  gulong strength_handler_id;
  GtkWidget *wifi_label;
  GtkWidget *strength_label;
} CurrentApState;

int network_init(NMClient *client, CurrentApState *current_ap_state);
