#include "audio_slider.h"
#include "glibconfig.h"
#include "gtk/gtkrevealer.h"
#include "util.h"
#include "wp/core.h"
#include "wp/node.h"
#include "wp/object-manager.h"
#include "wp/proxy-interfaces.h"
#include <glib-object.h>
#include <gtk/gtk.h>
#include <math.h>
#include <pipewire/keys.h>
#include <wp/plugin.h>

#define SCALE_STEP 5
#define MAX_NAME_LEN 20
#define ID "id"

static const char ICON_MUTED[] = "audio-volume-muted-symbolic";
static const char ICON_LOW[] = "audio-volume-low-symbolic";
static const char ICON_MEDIUM[] = "audio-volume-medium-symbolic";
static const char ICON_HIGH[] = "audio-volume-high-symbolic";
static const char ICON_OVERAMPLIFIED[] = "audio-volume-overamplified-symbolic";

static const gchar *sink_media_class = "Audio/Sink";

typedef struct {
  gulong value_changed_id;
  guint32 default_sink_id;
  WpPlugin *mixer_api;
  WpPlugin *def_nodes_api;
  WpObjectManager *om;
  GtkWidget *scale;
  GtkWidget *image;
  GtkWidget *revealer;
  GtkWidget *arrow_image;
  GtkWidget *revealer_box;
} AudioSlider;

static void set_current_sink(GtkButton *btn, gpointer user_data) {
  guint32 id = GPOINTER_TO_UINT(user_data);
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "wpctl set-default %u", id);
  sh(cmd);
}

static GtkWidget *create_sink_entry(WpNode *node, gboolean active) {
  g_autoptr(WpProperties) props =
      wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(node));

  const gchar *node_name = wp_properties_get(props, PW_KEY_NODE_NICK);
  if (!node_name)
    node_name = wp_properties_get(props, PW_KEY_NODE_DESCRIPTION);

  g_autofree gchar *name = truncate_string(node_name, MAX_NAME_LEN);
  GtkWidget *button = gtk_button_new();
  gtk_widget_set_tooltip_text(button, node_name);
  gtk_widget_set_cursor_from_name(button, "pointer");
  gtk_widget_add_css_class(button, "entry");
  if (active)
    gtk_widget_add_css_class(button, "active");

  GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_button_set_child(GTK_BUTTON(button), btn_box);
  GtkWidget *dot = gtk_label_new(active ? "⬤ " : "  ");
  GtkWidget *name_label = gtk_label_new(node_name);
  gtk_box_append(GTK_BOX(btn_box), dot);
  gtk_box_append(GTK_BOX(btn_box), name_label);

  guint32 node_id = wp_proxy_get_bound_id(WP_PROXY(node));
  g_signal_connect(button, "clicked", G_CALLBACK(set_current_sink),
                   GUINT_TO_POINTER(node_id));

  g_object_set_data(G_OBJECT(button), ID, GUINT_TO_POINTER(node_id));

  return button;
}

static void on_object_added(WpObjectManager *om, WpObject *object,
                            gpointer user_data) {
  AudioSlider *as = user_data;
  if (!WP_IS_NODE(object)) {
    g_printerr("Object added is not a node\n");
    return;
  }

  WpNode *node = WP_NODE(object);
  guint32 node_id = wp_proxy_get_bound_id(WP_PROXY(node));

  g_autoptr(WpProperties) props =
      wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(node));

  const gchar *media_class = wp_properties_get(props, "media.class");

  if (g_strcmp0(media_class, sink_media_class) == 0) {
    GtkWidget *sink_entry =
        create_sink_entry(node, node_id == as->default_sink_id);
    gtk_box_append(GTK_BOX(as->revealer_box), sink_entry);
  }
}

static void on_object_removed(WpObjectManager *om, WpObject *object,
                              gpointer user_data) {
  AudioSlider *as = user_data;
  if (!WP_IS_NODE(object)) {
    g_printerr("Object removed is not a node\n");
    return;
  }
  WpNode *node = WP_NODE(object);
  guint32 node_id = wp_proxy_get_bound_id(WP_PROXY(node));

  g_autoptr(WpProperties) props =
      wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(node));

  GtkWidget *child = gtk_widget_get_first_child(as->revealer_box);
  GtkWidget *next;

  while (child) {
    next = gtk_widget_get_next_sibling(child);
    guint32 id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(child), ID));
    if (id == node_id) {
      gtk_widget_unparent(child);
      return;
    }
    child = next;
  }
}

static void current_objects(WpObjectManager *om, gpointer user_data) {
  g_autoptr(WpIterator) iterator = wp_object_manager_new_iterator(om);

  g_auto(GValue) item = G_VALUE_INIT;
  while (wp_iterator_next(iterator, &item)) {
    WpObject *object = g_value_get_object(&item);
    on_object_added(om, object, user_data);
    g_value_unset(&item);
  }
}

static void on_object_changed(WpObjectManager *om, gpointer user_data) {
  AudioSlider *as = user_data;
  remove_children(as->revealer_box);
  current_objects(om, as);
}

static void update_volume_image(AudioSlider *as, gdouble volume,
                                gboolean mute) {
  const char *icon_name;
  if (mute) {
    icon_name = ICON_MUTED;
  } else if (volume == 0) {
    icon_name = ICON_MUTED;
  } else if (volume < 33) {
    icon_name = ICON_LOW;
  } else if (volume < 66) {
    icon_name = ICON_MEDIUM;
  } else if (volume <= 100) {
    icon_name = ICON_HIGH;
  } else {
    icon_name = ICON_OVERAMPLIFIED;
  }
  gtk_image_set_from_icon_name(GTK_IMAGE(as->image), icon_name);
}

static void value_changed(GtkRange *self, gpointer user_data) {
  AudioSlider *as = (AudioSlider *)user_data;

  gdouble volume = gtk_range_get_value(self);
  gboolean mute = volume == 0;
  update_volume_image(as, volume, mute);

  char volume_cmd[256] = {0};
  snprintf(volume_cmd, sizeof(volume_cmd),
           "wpctl set-volume @DEFAULT_AUDIO_SINK@ %f%%", volume);
  sh(volume_cmd);
  char mute_cmd[256] = {0};
  snprintf(mute_cmd, sizeof(mute_cmd), "wpctl set-mute @DEFAULT_AUDIO_SINK@ %d",
           mute);
  sh(mute_cmd);
}

static void on_mixer_changed(WpPlugin *mixer_api, guint32 node_id,
                             gpointer user_data) {
  AudioSlider *as = (AudioSlider *)user_data;

  if (node_id == as->default_sink_id) {
    GVariant *variant = NULL;
    gboolean mute = FALSE;
    gdouble volume = 1.0;

    g_signal_emit_by_name(as->mixer_api, "get-volume", node_id, &variant);
    if (!variant) {
      fprintf(stderr, "Node %d does not support volume\n", node_id);
      return;
    }
    g_variant_lookup(variant, "volume", "d", &volume);
    g_variant_lookup(variant, "mute", "b", &mute);

    volume *= 100; // From 0-1 to 0-100

    g_signal_handler_block(as->scale, as->value_changed_id);
    gtk_range_set_value(GTK_RANGE(as->scale), volume);
    g_signal_handler_unblock(as->scale, as->value_changed_id);

    update_volume_image(as, volume, mute);
  }
}

static void on_def_nodes_changed(WpPlugin *def_node_api, gpointer user_data) {
  AudioSlider *as = (AudioSlider *)user_data;

  if (def_node_api) {
    g_signal_emit_by_name(def_node_api, "get-default-node", sink_media_class,
                          &as->default_sink_id);
    on_object_changed(as->om, as);
    if (as->mixer_api)
      on_mixer_changed(as->mixer_api, as->default_sink_id, as);
  }
}

static gboolean on_change_value(GtkRange *range, GtkScrollType scroll,
                                double value, gpointer user_data) {
  gdouble step = SCALE_STEP;
  gdouble rounded = round(value / step) * step;

  gtk_range_set_value(range, rounded);
  return TRUE; // stop default handler
}

static void toggle_revealer(GtkButton *btn, gpointer user_data) {
  AudioSlider *as = user_data;
  GtkRevealer *revealer = GTK_REVEALER(as->revealer);
  gboolean open = gtk_revealer_get_reveal_child(revealer);
  gtk_revealer_set_reveal_child(revealer, !open);
  gtk_image_set_from_icon_name(GTK_IMAGE(as->arrow_image),
                               !open ? "go-up-symbolic" // Revealer will be open
                                     : "go-down-symbolic");
}

GtkWidget *create_audio_slider(WpObjectManager *om, WpCore *core) {
  AudioSlider *as = g_new0(AudioSlider, 1);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(box, "audio-slider");
  GtkWidget *scale_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_hexpand(scale_box, TRUE);
  gtk_widget_add_css_class(scale_box, "scale-box");
  GtkWidget *image = gtk_image_new_from_icon_name(ICON_MUTED);
  gtk_widget_add_css_class(image, "icon");
  GtkWidget *scale =
      gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, SCALE_STEP);
  gtk_range_set_round_digits(GTK_RANGE(scale), 0);
  gtk_widget_set_hexpand(scale, TRUE);
  gtk_widget_set_cursor_from_name(scale, "pointer");
  GtkWidget *toggle_revealer_btn = gtk_button_new();
  GtkWidget *arrow_image = gtk_image_new_from_icon_name("go-down-symbolic");
  gtk_button_set_child(GTK_BUTTON(toggle_revealer_btn), arrow_image);
  gtk_widget_set_cursor_from_name(arrow_image, "pointer");

  gtk_box_append(GTK_BOX(scale_box), image);
  gtk_box_append(GTK_BOX(scale_box), scale);
  gtk_box_append(GTK_BOX(scale_box), toggle_revealer_btn);

  gtk_box_append(GTK_BOX(box), scale_box);

  GtkWidget *revealer = gtk_revealer_new();
  GtkWidget *revealer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_revealer_set_child(GTK_REVEALER(revealer), revealer_box);
  gtk_widget_add_css_class(revealer_box, "audio-sinks");

  gtk_box_append(GTK_BOX(box), revealer);

  as->om = om;
  as->scale = scale;
  as->image = image;
  as->revealer = revealer;
  as->arrow_image = arrow_image;
  as->revealer_box = revealer_box;

  g_signal_connect(toggle_revealer_btn, "clicked", G_CALLBACK(toggle_revealer),
                   as);

  as->value_changed_id =
      g_signal_connect(scale, "value-changed", G_CALLBACK(value_changed), as);
  g_signal_connect(scale, "change-value", G_CALLBACK(on_change_value), NULL);

  as->def_nodes_api = wp_plugin_find(core, "default-nodes-api");
  if (as->def_nodes_api) {
    g_signal_connect(as->def_nodes_api, "changed",
                     G_CALLBACK(on_def_nodes_changed), as);

    on_def_nodes_changed(as->def_nodes_api, as);
  } else {
    g_warning("Could not find def_node-api plugin");
  }
  as->mixer_api = wp_plugin_find(core, "mixer-api");
  if (as->mixer_api) {
    g_signal_connect(as->mixer_api, "changed", G_CALLBACK(on_mixer_changed),
                     as);
    on_mixer_changed(as->mixer_api, as->default_sink_id, as);
  } else {
    g_warning("Could not find mixer-api plugin");
  }

  g_signal_connect(om, "object-added", G_CALLBACK(on_object_added), as);
  g_signal_connect(om, "object-removed", G_CALLBACK(on_object_removed), as);
  g_signal_connect(om, "objects-changed", G_CALLBACK(on_object_changed), as);
  current_objects(om, as);

  return box;
}
