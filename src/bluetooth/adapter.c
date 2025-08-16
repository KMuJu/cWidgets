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

static void on_set_called(GDBusProxy *proxy, GAsyncResult *res,
                          gpointer user_data) {
  GError *error = NULL;
  GVariant *result = g_dbus_proxy_call_finish(proxy, res, &error);
  if (!result) {
    g_warning("Set(Powered) call failed: %s", error->message);
    g_error_free(error);
    return;
  }
  g_variant_unref(result);
}
void adapter_set_powered(Adapter *self, gboolean powered) {
  g_dbus_proxy_call(G_DBUS_PROXY(self), "Set",
                    g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered",
                                  g_variant_new_boolean(powered)), // Parameter
                    G_DBUS_CALL_FLAGS_NONE,                        // Flags
                    -1,                                 // Default timeout
                    NULL,                               // Cancellable
                    (GAsyncReadyCallback)on_set_called, // Callback
                    NULL);
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

static void on_start_discovery_called(GDBusProxy *proxy, GAsyncResult *res,
                                      gpointer user_data) {
  GError *error = NULL;
  GVariant *result = g_dbus_proxy_call_finish(proxy, res, &error);
  if (!result) {
    g_warning("StartDiscovery call failed: %s", error->message);
    g_error_free(error);
    return;
  }
  g_message("Started discovering");
  g_variant_unref(result);
}
static void on_stop_discovery_called(GDBusProxy *proxy, GAsyncResult *res,
                                     gpointer user_data) {
  GError *error = NULL;
  GVariant *result = g_dbus_proxy_call_finish(proxy, res, &error);
  if (!result) {
    g_warning("SetDiscovery call failed: %s", error->message);
    g_error_free(error);
    return;
  }
  g_message("Stopped discovering");
  g_variant_unref(result);
}

void adapter_set_discovering(Adapter *self, gboolean discovering) {
  if (discovering)
    g_dbus_proxy_call(
        G_DBUS_PROXY(self), // the proxy
        "StartDiscovery",   // method name
        NULL,               // parameters (here: dict<string,variant>)
        G_DBUS_CALL_FLAGS_NONE,
        -1,   // timeout (ms, -1 = default)
        NULL, // cancellable
        (GAsyncReadyCallback)on_start_discovery_called, // callback
        NULL);
  else
    g_dbus_proxy_call(G_DBUS_PROXY(self), // the proxy
                      "StopDiscovery",    // method name
                      NULL, // parameters (here: dict<string,variant>)
                      G_DBUS_CALL_FLAGS_NONE, //
                      -1,                     // timeout (ms, -1 = default)
                      NULL,                   // cancellable
                      (GAsyncReadyCallback)on_stop_discovery_called, // callback
                      NULL);
}
