#include "wifi_icon.h"
#include "glib-object.h"
#include "networking.h"
#include <NetworkManager.h>
#include <glib.h>
#include <gtk/gtk.h>

static const char ICON_EXCELLENT[] =
    "network-wireless-signal-excellent-symbolic";
static const char ICON_OK[] = "network-wireless-signal-ok-symbolic";
static const char ICON_GOOD[] = "network-wireless-signal-good-symbolic";
static const char ICON_WEAK[] = "network-wireless-signal-weak-symbolic";
static const char ICON_NONE[] = "network-wireless-signal-none-symbolic";
static const char ICON_ACQUIRING[] = "network-wireless-acquiring-symbolic";
static const char ICON_CONNECTED[] = "network-wireless-connected-symbolic";
static const char ICON_DISABLED[] = "network-wireless-disabled-symbolic";
static const char ICON_OFFLINE[] = "network-wireless-offline-symbolic";
static const char ICON_NO_ROUTE[] = "network-wireless-no-route-symbolic";
static const char ICON_HOTSPOT[] = "network-wireless-hotspot-symbolic";

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

void active_ap_state_free(ActiveApState *s) {
  if (!s)
    return;
  if (s->strength_handler_id && s->previous_ap)
    g_signal_handler_disconnect(s->previous_ap, s->strength_handler_id);
  g_clear_object(&s->previous_ap);
  g_free(s);
}

void add_wifi_widget(GtkWidget *box) {
  if (!GTK_IS_BOX(box)) {
    g_warning("Tried to add wifi widget to widget that is not a box");
    return;
  }

  ActiveApState *activeApState = calloc(1, sizeof(ActiveApState));
  g_object_set_data_full(G_OBJECT(box), "wifi-state", activeApState,
                         (GDestroyNotify)active_ap_state_free);

  GtkWidget *image = gtk_image_new_from_icon_name(ICON_OFFLINE);
  gtk_box_append(GTK_BOX(box), image);
  activeApState->image = image;

  NMDeviceWifi *wifi_device = get_wifi_device();
  if (NULL == wifi_device)
    return;

  g_signal_connect(wifi_device, "notify::active-access-point",
                   G_CALLBACK(on_active_ap_changed), activeApState);

  on_active_ap_changed(wifi_device, NULL, activeApState);

  g_object_unref(wifi_device);
}

void on_active_ap_changed(NMDeviceWifi *device, GParamSpec *pspec,
                          gpointer user_data) {
  ActiveApState *s = user_data;
  GtkWidget *image = s->image;

  if (s->strength_handler_id && s->previous_ap) {
    g_message("Prev ap(%p) handler(%lu)", (void *)s->previous_ap,
              s->strength_handler_id);
    g_signal_handler_disconnect(s->previous_ap, s->strength_handler_id);
    g_clear_object(&s->previous_ap);
    s->strength_handler_id = 0;
  }

  NMAccessPoint *_ap = nm_device_wifi_get_active_access_point(device);
  if (_ap) {
    NMAccessPoint *ap = g_object_ref(_ap);
    if (s->previous_ap)
      s->previous_ap = g_object_ref(ap);

    g_autofree char *ssid_str = ap_get_ssid(ap);
    if (!ssid_str)
      return;

    gtk_widget_set_tooltip_text(image, ssid_str);

    s->strength_handler_id = g_signal_connect(
        ap, "notify::strength", G_CALLBACK(on_strength_changed), s);

    on_strength_changed(ap, NULL, s);
  } else {
    gtk_widget_set_tooltip_text(image, "disconnected");
    gtk_image_set_from_icon_name(GTK_IMAGE(image), ICON_OFFLINE);
  }
}

void on_strength_changed(NMAccessPoint *ap, GParamSpec *pspec,
                         gpointer user_data) {
  ActiveApState *s = user_data;
  GtkWidget *image = s->image;
  guint8 strength = nm_access_point_get_strength(ap); // 0–100%

  const char *icon;
  if (strength >= 80)
    icon = ICON_EXCELLENT;
  else if (strength >= 60)
    icon = ICON_GOOD;
  else if (strength >= 40)
    icon = ICON_OK;
  else if (strength >= 20)
    icon = ICON_WEAK;
  else
    icon = ICON_NONE;

  gtk_image_set_from_icon_name(GTK_IMAGE(image), icon);
}
