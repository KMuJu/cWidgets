#include "workspaces.h"
#include "cJSON.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "util.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct {
  gint id;
  char *name;
  char *active_window;
} Workspace;

void workspace_free(gpointer data) {
  Workspace *w = data;
  g_free(w->name);
  g_free(w->active_window);
  g_free(w);
}

typedef struct {
  GtkWidget *box;
  gint active_workspace_id;
  GPtrArray *workspaces;
  guint index;
} HyprlandState;

static void hyprland_state_free(HyprlandState *hs) {
  if (!hs)
    return;
  g_clear_object(&hs->workspaces); // unrefs and sets to NULL
  g_free(hs);                      // free the struct itself
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(HyprlandState, hyprland_state_free)

static gint workspace_compare(gconstpointer a_p, gconstpointer b_p) {
  const Workspace *a = a_p;
  const Workspace *b = b_p;
  if (a->id < b->id)
    return -1;
  else if (a->id > b->id)
    return 1;
  else
    return 0;
}

// Caller must close the socket
static int hyprland_sock(void) {
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  const char *hyprland_instance = getenv("HYPRLAND_INSTANCE_SIGNATURE");

  if (!xdg_runtime_dir || !hyprland_instance) {
    g_printerr("Hyprland environment variables not set.\n");
    return -1;
  }

  char socket_path[256];
  snprintf(socket_path, sizeof(socket_path), "%s/hypr/%s/.socket2.sock",
           xdg_runtime_dir, hyprland_instance);

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    g_printerr("Could not create hyprland socket\n");
    return -1;
  }

  struct sockaddr_un sockaddr;
  sockaddr.sun_family = AF_UNIX;
  strncpy(sockaddr.sun_path, socket_path, sizeof(sockaddr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    g_printerr("Could not connect to hyprland socket\n");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int hyprland_ipc1(const char *msg, char *output, gint output_len) {
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  const char *hyprland_instance = getenv("HYPRLAND_INSTANCE_SIGNATURE");

  if (!xdg_runtime_dir || !hyprland_instance) {
    g_printerr("Hyprland environment variables not set.\n");
    return -1;
  }

  char socket_path[256];
  snprintf(socket_path, sizeof(socket_path), "%s/hypr/%s/.socket.sock",
           xdg_runtime_dir, hyprland_instance);

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    g_printerr("Could not create hyprland socket\n");
    return -1;
  }

  struct sockaddr_un sockaddr;
  sockaddr.sun_family = AF_UNIX;
  strncpy(sockaddr.sun_path, socket_path, sizeof(sockaddr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    g_printerr("Could not connect to hyprland socket");
    close(sockfd);
    return -1;
  }

  if (write(sockfd, msg, strlen(msg)) < 0) {
    g_printerr("Could not write %s to the hyprland socket\n", msg);
    close(sockfd);
    return -1;
  }

  size_t len = read(sockfd, output, output_len - 1);
  close(sockfd);

  if (len <= 0) {
    g_printerr("Could not read from hyprland socket\n");
    return -1;
  }

  output[len] = '\0';

  return 0;
}

int get_hyprland_workspaces(GPtrArray *array) {
  char buffer[8192] = {0};
  int count = 0;

  if (hyprland_ipc1("j/workspaces", buffer, sizeof(buffer)) != 0)
    return -1;

  cJSON *root = cJSON_Parse(buffer);
  if (!root) {
    g_message("Invalid json from hyprland ipc");
    return -1;
  }

  count = cJSON_GetArraySize(root);

  for (int i = 0; i < count; i++) {
    cJSON *item = cJSON_GetArrayItem(root, i);
    cJSON *id_item = cJSON_GetObjectItem(item, "id");
    cJSON *name_item = cJSON_GetObjectItem(item, "name");
    cJSON *title_item = cJSON_GetObjectItem(item, "lastwindowtitle");
    int id = (int)cJSON_GetNumberValue(id_item);
    char *name = cJSON_GetStringValue(name_item);
    char *title = cJSON_GetStringValue(title_item);

    Workspace *w = g_new0(Workspace, 1);
    w->id = id;
    w->name = g_strdup(name);
    w->active_window = g_strdup(title);
    g_ptr_array_add(array, w);
  }

  cJSON_Delete(root);

  return count;
}

static void on_button_click(GtkButton *self, gpointer data) {
  HyprlandState *hs = data;

  gint workspace_id =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(self), "workspace-id"));

  if (hs->active_workspace_id == workspace_id)
    return;

  char output_buf[8] = {0};
  char buf[32] = {0};
  snprintf(buf, sizeof(buf), "dispatch workspace %d", workspace_id);
  if (hyprland_ipc1(buf, output_buf, sizeof(output_buf))) {
    g_message("Could not move to workspace");
    return;
  }
}

static gboolean set_active_workspace(gpointer data) {
  HyprlandState *hs = data;

  GtkWidget *child = gtk_widget_get_first_child(hs->box);
  while (child) {
    gint id =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "workspace-id"));
    if (hs->active_workspace_id != id)
      gtk_widget_remove_css_class(child, "active");
    else
      gtk_widget_add_css_class(child, "active");

    child = gtk_widget_get_next_sibling(child);
  }

  return G_SOURCE_REMOVE;
}

static gboolean update_ui(gpointer data) {
  HyprlandState *hs = data;
  g_ptr_array_set_size(hs->workspaces, 0);

  get_hyprland_workspaces(hs->workspaces);

  g_ptr_array_sort_values(hs->workspaces, workspace_compare);

  remove_children(hs->box);
  guint list_size = hs->workspaces->len;

  for (guint i = 0; i < list_size; i++) {
    Workspace *workspace = g_ptr_array_index(hs->workspaces, i);

    GtkWidget *button = gtk_button_new_with_label(workspace->name);

    g_object_set_data(G_OBJECT(button), "workspace-id",
                      GINT_TO_POINTER(workspace->id));

    gtk_widget_set_tooltip_text(button, workspace->active_window);
    gtk_widget_set_cursor(button, get_pointer_cursor());
    gtk_widget_add_css_class(button, "workspace-button");

    g_signal_connect(button, "clicked", G_CALLBACK(on_button_click), hs);

    gtk_box_append(GTK_BOX(hs->box), button);
  }

  return set_active_workspace(hs);
}

void init_hyprland(HyprlandState *hs) {
  hs->workspaces = g_ptr_array_new_with_free_func(workspace_free);

  char buffer[8192] = {0};

  if (hyprland_ipc1("j/activeworkspace", buffer, sizeof(buffer)) != 0)
    return;

  cJSON *root = cJSON_Parse(buffer);
  if (!root) {
    g_message("Invalid json from hyprland ipc");
    return;
  }

  cJSON *id_item = cJSON_GetObjectItem(root, "id");
  hs->active_workspace_id = (int)cJSON_GetNumberValue(id_item);
  cJSON_Delete(root);

  g_idle_add(update_ui, hs);
}

gpointer listen_to_hyprland_socket(gpointer data) {
  GtkWidget *box = data;
  g_autoptr(HyprlandState) hs = g_new0(HyprlandState, 1);
  hs->box = box;
  init_hyprland(hs);
  int sockfd = hyprland_sock();
  if (sockfd < 0)
    return NULL;

  char buf[4096];
  ssize_t len;

  while ((len = recv(sockfd, buf, sizeof(buf) - 1, 0)) > 0) {
    buf[len] = '\0';

    char *line = strtok(buf, "\n");
    while (line) {
      // g_message("Line: %s", line);
      if (strncmp(line, "createworkspacev2>>", 19) == 0) {
        g_idle_add(update_ui, hs);
      } else if (strncmp(line, "destroyworkspacev2>>", 20) == 0) {
        g_idle_add(update_ui, hs);
      } else if (strncmp(line, "workspacev2>>", 13) == 0) {
        char *id_str = strtok(line + 13, ",");
        int id = atoi(id_str);
        hs->active_workspace_id = id;
        g_idle_add(set_active_workspace, hs);
      }

      line = strtok(NULL, "\n");
    }
  }

  if (len < 0) {
    perror("recv");
  }

  return NULL;
}

void start_workspace_widget(GtkWidget *box) {}
