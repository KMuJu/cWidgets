#include "bar/bar.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk4-layer-shell/gtk4-layer-shell.h"
#include "wp/core.h"
#include <unistd.h>
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

  g_message("Loaded plugin: %d", ctx->pending_plugins);

  // If there are no more plugins to load
  // install object manager to trigger run
  if (--ctx->pending_plugins == 0) {
    g_autoptr(WpPlugin) mixer_api = wp_plugin_find(core, "mixer-api");
    g_object_set(mixer_api, "scale", 1 /* cubic */, NULL);
    wp_core_install_object_manager(ctx->core, ctx->om);
  }
}

static void run(MainContext *ctx) {
  gtk_init();
  load_css();
  GdkDisplay *display = gdk_display_get_default();
  GListModel *monitors = gdk_display_get_monitors(display);
  guint n_monitors = g_list_model_get_n_items(monitors);

  for (guint i = 0; i < n_monitors; i++) {

    GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, i));
    const char *name = gdk_monitor_get_model(monitor);
    g_message("Assigning window to monitor: %s", name);

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    g_object_unref(monitor);
    GtkWidget *window = gtk_window_new();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Gtkbar%d", i);
    gtk_window_set_title(GTK_WINDOW(window), buffer);
    gtk_window_set_default_size(GTK_WINDOW(window), geometry.width, -1);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    gtk_window_set_display(GTK_WINDOW(window), display);

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_monitor(GTK_WINDOW(window), monitor);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP,
                         geometry.y);
    gtk_window_present(GTK_WINDOW(window));
    bar(window, ctx->dbus_connection, ctx->core, ctx->om);
  }
}

int main(int argc, char *argv[]) {
  g_message("Starting cWidget\n");

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  MainContext ctx = {0};
  ctx.loop = loop;

  // DBUS
  GError *error = NULL;
  GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection) {
    g_printerr("Failed to connect to system bus: %s\n", error->message);
    g_error_free(error);
    return 1;
  }

  ctx.dbus_connection = connection;

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

  return ctx.exit_code;
}
