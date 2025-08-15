#include "device.h"
#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

struct _Device {
  GDBusProxy parent_instance;
};

G_DEFINE_TYPE(Device, device, G_TYPE_DBUS_PROXY)

static void device_dispose(GObject *gobject);
static void device_finalize(GObject *gobject);

static void device_class_init(DeviceClass *klass) {}

static void device_init(Device *self) {}

gboolean device_get_paired(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Paired");
  if (value == NULL) {
    return FALSE;
  }

  gboolean paired = g_variant_get_boolean(value);
  g_variant_unref(value);
  return paired;
}

gboolean device_get_connected(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Connected");
  if (value == NULL) {
    return FALSE;
  }

  gboolean connected = g_variant_get_boolean(value);
  g_variant_unref(value);
  return connected;
}

gchar *device_get_name(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Name");
  if (value == NULL) {
    return NULL;
  }

  gchar *name = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return name;
}

gchar *device_get_address(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Address");
  if (value == NULL) {
    return NULL;
  }

  gchar *address = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return address;
}
