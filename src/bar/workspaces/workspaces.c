#include "workspaces.h"
#include "cJSON.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "util.h"
#include "workspace_object.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct {
  GtkWidget *box;
  int active_workspace_id;
  GListStore *workspaces;
  guint index;
} HyprlandState;

typedef struct {
  HyprlandState *hs;
  guint position;
  guint removed;
  guint added;
} UpdateUiState;

static void hyprland_state_free(HyprlandState *hs) {
  if (!hs)
    return;
  g_clear_object(&hs->workspaces); // unrefs and sets to NULL
  g_free(hs);                      // free the struct itself
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(HyprlandState, hyprland_state_free)

static gboolean workspace_object_equal(gconstpointer a, gconstpointer b) {
  const WorkspaceObject *a_o = a;
  const WorkspaceObject *b_o = b;
  Workspace *a_w = workspace_object_get_state(a_o);
  Workspace *b_w = workspace_object_get_state(b_o);
  return a_w->id == b_w->id;
}

static gint workspace_compare(const Workspace *a, const Workspace *b) {
  if (a->id < b->id)
    return -1;
  else if (a->id > b->id)
    return 1;
  else
    return 0;
}

static gint workspace_object_compare(gconstpointer a, gconstpointer b,
                                     gpointer data) {
  const WorkspaceObject *a_o = a;
  const WorkspaceObject *b_o = b;
  Workspace *a_w = workspace_object_get_state(a_o);
  Workspace *b_w = workspace_object_get_state(b_o);
  return workspace_compare(a_w, b_w);
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

int get_hyprland_workspaces(GListStore *list) {
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
    int id = (int)cJSON_GetNumberValue(id_item);
    char *name = cJSON_GetStringValue(name_item);

    g_print("From json: id(%d), name(%s)\n", id, name);
    g_list_store_insert_sorted(list, workspace_object_new(id, name),
                               workspace_object_compare, NULL);
  }

  return count;
}

static gboolean remove_from_list(GListStore *list, int id, char *name) {
  guint position = -1;
  WorkspaceObject *item = workspace_object_new(id, name);

  if (!g_list_store_find_with_equal_func(list, item, workspace_object_equal,
                                         &position)) {
    g_message("Could not find item to remove from list");
    return FALSE;
  }

  g_list_store_remove(list, position);

  g_object_unref(item);

  return TRUE;
}

static gboolean add_to_list(GListStore *list, int id, char *name) {
  WorkspaceObject *item = workspace_object_new(id, name);
  g_print("Added %d - %s\n", id, name);

  g_list_store_insert_sorted(list, item, workspace_object_compare, NULL);

  return TRUE;
}

static void on_button_click(GtkButton *self, gpointer data) {
  HyprlandState *hs = data;

  guint len = g_list_model_get_n_items(G_LIST_MODEL(hs->workspaces));
  for (size_t i = 0; i < len; i++) {
    GObject *obj = g_list_model_get_object(G_LIST_MODEL(hs->workspaces), i);
    if (!obj)
      continue; // Safety check

    Workspace *workspace =
        workspace_object_get_state(WORKSPACE_WORKSPACE_OBJECT(obj));

    if (!workspace) {
      g_object_unref(obj);
      continue;
    }
    g_message("Workspace: %d, %s", workspace->id, workspace->name);
  }

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

  g_message("Output %s", output_buf);
}

static gboolean update_ui(gpointer data) {
  UpdateUiState *ui = data;
  HyprlandState *hs = ui->hs;
  g_print("Workspaces changed\n");
  g_print("Position(%u), removed(%u), added(%u)\n", ui->position, ui->removed,
          ui->added);

  remove_children(hs->box);
  guint list_size = g_list_model_get_n_items(G_LIST_MODEL(hs->workspaces));

  for (guint i = 0; i < list_size; i++) {
    GObject *obj = g_list_model_get_object(G_LIST_MODEL(hs->workspaces), i);
    if (!obj)
      continue; // Safety check

    Workspace *workspace =
        workspace_object_get_state(WORKSPACE_WORKSPACE_OBJECT(obj));

    if (!workspace) {
      g_object_unref(obj);
      continue;
    }

    GtkWidget *button = gtk_button_new_with_label(workspace->name);
    g_object_set_data(G_OBJECT(button), "workspace-id",
                      GINT_TO_POINTER(workspace->id));

    // int id = workspace->id;
    // g_object_set_data(G_OBJECT(button), "workspace-id", GINT_TO_POINTER(id));
    // g_print("Workspace id(%d), name(%s)\n", workspace->id, workspace->name);
    // g_message("Id(%d), position(%d)\n", id, ui->position + i);

    g_signal_connect(button, "clicked", G_CALLBACK(on_button_click), hs);

    gtk_box_append(GTK_BOX(hs->box), button);
  }

  free(ui);
  return G_SOURCE_REMOVE;
}

static void items_changed(GListModel *self, guint position, guint removed,
                          guint added, gpointer user_data) {
  UpdateUiState *ui = g_new0(UpdateUiState, 1);
  ui->hs = user_data;
  ui->position = position;
  ui->removed = removed;
  ui->added = added;
  g_idle_add(update_ui, ui);
}

void init_hyprland(HyprlandState *hs) {
  hs->workspaces = g_list_store_new(workspace_object_get_type());
  g_signal_connect(hs->workspaces, "items-changed", G_CALLBACK(items_changed),
                   hs);

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

  get_hyprland_workspaces(hs->workspaces);
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
      if (strncmp(line, "createworkspacev2>>", 19) == 0) {
        g_message("%s", line);
        char *id_str = strtok(line + 19, ",");
        char *name = strtok(NULL, ",");
        int id = atoi(id_str);
        g_message("Created workspace id(%d), name(%s)", id, name);
        add_to_list(hs->workspaces, id, name);
      } else if (strncmp(line, "destroyworkspacev2>>", 20) == 0) {
        g_message("%s", line);
        char *id_str = strtok(line + 20, ",");
        char *name = strtok(NULL, ",");
        int id = atoi(id_str);
        g_message("Destroyed workspace id(%d), name(%s)", id, name);
        remove_from_list(hs->workspaces, id, name);
      } else if (strncmp(line, "workspacev2>>", 13) == 0) {
        char *id_str = strtok(line + 13, ",");
        char *name = strtok(NULL, ",");
        int id = atoi(id_str);
        hs->active_workspace_id = id;
        // g_message("Found workspace id(%d), name(%s)", id, name);
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
