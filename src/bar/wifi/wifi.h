#ifndef BAR_WIFI_H
#define BAR_WIFI_H

#include "NetworkManager.h"
#include "glib-object.h"
#include <gtk/gtk.h>

typedef struct {
  GtkWidget *image;
  gulong strength_handler_id;
  NMAccessPoint *previous_ap;
} ActiveApState;

void add_wifi_widget(GtkWidget *box);

void on_strength_changed(NMAccessPoint *ap, GParamSpec *pspec,
                         gpointer user_data);

void on_active_ap_changed(NMDeviceWifi *device, GParamSpec *pspec,
                          gpointer user_data);

#endif // !BAR_WIFI_H
