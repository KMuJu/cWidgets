#include "adapter.h"
#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

struct _Adapter {
  GDBusProxy parent_instance;
};

G_DEFINE_TYPE(Adapter, adapter, G_TYPE_DBUS_PROXY)

static void adapter_class_init(AdapterClass *klass) {}

static void adapter_init(Adapter *self) {}

gchar *adapter_get_address(Adapter *self) {
  g_return_val_if_fail(BLUETOOTH_IS_ADAPTER(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Address");
  if (value == NULL) {
    return NULL;
  }

  gchar *address = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return address;
}

gchar *adapter_get_name(Adapter *self) {
  g_return_val_if_fail(BLUETOOTH_IS_ADAPTER(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Name");
  if (value == NULL) {
    return NULL;
  }

  gchar *name = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return name;
}

gboolean adapter_get_powered(Adapter *self) {
  g_return_val_if_fail(BLUETOOTH_IS_ADAPTER(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Powered");
  if (value == NULL) {
    return FALSE;
  }

  gboolean powered = g_variant_get_boolean(value);
  g_variant_unref(value);
  return powered;
}

gboolean adapter_get_discovering(Adapter *self) {
  g_return_val_if_fail(BLUETOOTH_IS_ADAPTER(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Discovering");
  if (value == NULL) {
    return FALSE;
  }

  gboolean discovering = g_variant_get_boolean(value);
  g_variant_unref(value);
  return discovering;
}
