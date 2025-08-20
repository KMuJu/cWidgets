#include "bar/bar.h"
#include "bluetooth/bt.h"
#include "log.h"
#include "quicksettings/quicksettings.h"
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <wp/core.h>
#include <wp/wp.h>

typedef struct {
  GMainLoop *loop;
  GDBusConnection *dbus_connection;
  WpCore *core;
  WpObjectManager *om;
  guint pending_plugins;
  gint exit_code;
} MainContext;

static void load_css(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_path(provider, "style.css");

  GdkDisplay *display = gdk_display_get_default();
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  g_object_unref(provider);
}

static void on_plugin_loaded(WpCore *core, GAsyncResult *res,
                             MainContext *ctx) {
  GError *error = NULL;

  if (!wp_core_load_component_finish(core, res, &error)) {
    fprintf(stderr, "%s\n", error->message);
    ctx->exit_code = 1;
    g_main_loop_quit(ctx->loop);
    return;
  }

  // If there are no more plugins to load
  // install object manager to trigger run function
  if (--ctx->pending_plugins == 0) {
    g_autoptr(WpPlugin) mixer_api = wp_plugin_find(core, "mixer-api");
    g_object_set(mixer_api, "scale", 1 /* cubic */, NULL);
    wp_core_install_object_manager(ctx->core, ctx->om);
  }
}

static void run(MainContext *ctx) {
  gtk_init();
  load_css();

  init_logger("cWidgets.log");
  if (log_file) {
    dup2(fileno(log_file), STDERR_FILENO);
  }
  redirect_glib_logs();

  LOG("Application started");

  bluetooth_install_signals(bluetooth_get_default());

  GdkDisplay *display = gdk_display_get_default();
  GListModel *monitors = gdk_display_get_monitors(display);
  guint n_monitors = g_list_model_get_n_items(monitors);

  for (guint i = 0; i < n_monitors; i++) {

    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, i));
    const char *name = gdk_monitor_get_model(monitor);
    g_message("Assigning windows to monitor: %s", name);

    bar(display, monitor, ctx->dbus_connection, ctx->core, ctx->om);

    start_quick_settings(display, monitor);
  }
}

int main(int argc, char *argv[]) {
  g_message("Starting cWidget\n");

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  MainContext ctx = {0};
  ctx.loop = loop;

  // DBUS
  GError *error = NULL;
  ctx.dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!ctx.dbus_connection) {
    g_printerr("Failed to connect to system bus: %s\n", error->message);
    g_error_free(error);
    return 1;
  }
  bluetooth_set_dbus_conn(ctx.dbus_connection);

  // WIREPLUMBER
  wp_init(WP_INIT_PIPEWIRE | WP_INIT_SPA_TYPES | WP_INIT_SET_PW_LOG);
  WpCore *core = wp_core_new(g_main_context_default(), NULL, NULL);
  WpObjectManager *om = wp_object_manager_new();
  wp_object_manager_add_interest(om, WP_TYPE_NODE,
                                 WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class",
                                 "=s", "Audio/Sink", NULL);

  ctx.core = core;
  ctx.om = om;

  ctx.pending_plugins++;
  wp_core_load_component(ctx.core, "libwireplumber-module-default-nodes-api",
                         "module", NULL, NULL, NULL,
                         (GAsyncReadyCallback)on_plugin_loaded, &ctx);

  ctx.pending_plugins++;
  wp_core_load_component(core, "libwireplumber-module-mixer-api", "module",
                         NULL, NULL, NULL,
                         (GAsyncReadyCallback)on_plugin_loaded, &ctx);

  /* connect */
  if (!wp_core_connect(core)) {
    g_printerr("Could not connect to PipeWire\n");
    return 1;
  }

  g_signal_connect_swapped(ctx.core, "disconnected",
                           (GCallback)g_main_loop_quit, ctx.loop);
  // Run after object manager is installed
  g_signal_connect_swapped(om, "installed", (GCallback)run, &ctx);

  g_main_loop_run(loop);

  LOG("Application exiting");
  close_logger();

  return ctx.exit_code;
}
