#include "adapter.h"
#include "bt.h"
#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

static void on_adapter_properties_changed(Adapter *proxy,
                                          GVariant *changed_properties,
                                          GStrv invalidated_properties,
                                          gpointer user_data);

struct _Adapter {
  GDBusProxy parent_instance;
  Bluetooth *bt;
};

enum {
  SIGNAL_POWERED,
  SIGNAL_DISCOVERING,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = {0};

G_DEFINE_TYPE(Adapter, adapter, G_TYPE_DBUS_PROXY)

static void adapter_class_init(AdapterClass *klass) {
  signals[SIGNAL_POWERED] =
      g_signal_new("powered", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_DISCOVERING] =
      g_signal_new("discovering", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
                   0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void adapter_init(Adapter *self) {
  g_signal_connect(self, "g-properties-changed",
                   G_CALLBACK(on_adapter_properties_changed), NULL);
}

static void on_adapter_properties_changed(Adapter *proxy,
                                          GVariant *changed_properties,
                                          GStrv invalidated_properties,
                                          gpointer user_data) {
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init(&iter, changed_properties);
  while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
    if (g_str_equal(key, "Powered")) {
      gboolean powered = g_variant_get_boolean(value);
      g_signal_emit(BLUETOOTH_ADAPTER(proxy), signals[SIGNAL_POWERED], 0,
                    powered);
    } else if (g_str_equal(key, "Discovering")) {
      gboolean discovering = g_variant_get_boolean(value);
      g_signal_emit(BLUETOOTH_ADAPTER(proxy), signals[SIGNAL_DISCOVERING], 0,
                    discovering);
    }
    g_variant_unref(value);
  }

  bluetooth_sync(proxy->bt);
  bluetooth_update_adapters(proxy->bt);
}

void adapter_set_bt(Adapter *self, void *bt) { self->bt = BLUETOOTH_BT(bt); }

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
  GError *error = NULL;

  // Get the object path of this adapter
  const gchar *object_path = g_dbus_proxy_get_object_path(G_DBUS_PROXY(self));

  // Create a temporary proxy for org.freedesktop.DBus.Properties
  GDBusProxy *props_proxy = g_dbus_proxy_new_sync(
      g_dbus_proxy_get_connection(G_DBUS_PROXY(self)), // same connection
      G_DBUS_PROXY_FLAGS_NONE, NULL,
      "org.bluez", // bus name
      object_path, // adapter object path (/org/bluez/hci0)
      "org.freedesktop.DBus.Properties", // <-- important
      NULL, &error);

  if (!props_proxy) {
    g_warning("Failed to create Properties proxy: %s", error->message);
    g_error_free(error);
    return;
  }

  g_dbus_proxy_call(
      props_proxy, "Set",
      g_variant_new("(ssv)",
                    "org.bluez.Adapter1",       // interface owning the property
                    "Powered",                  // property name
                    g_variant_new("b", powered) // value wrapped in variant
                    ),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)on_set_called,
      NULL);

  g_object_unref(props_proxy);
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
