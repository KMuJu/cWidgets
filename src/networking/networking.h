#ifndef NETOWRKING_H
#define NETOWRKING_H

#include "NetworkManager.h"
#include "main.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
  void (*run)(MainContext *);
  MainContext *ctx;
} NetInitData;

void net_init(NetInitData *data);
NMClient *net_get_client(void);
NMDeviceWifi *net_get_wifi_device(void);

gchar *ap_get_ssid(NMAccessPoint *ap);
gchar *wireless_setting_get_ssid(NMSettingWireless *setting);

G_END_DECLS
#endif // !NETOWRKING_H
