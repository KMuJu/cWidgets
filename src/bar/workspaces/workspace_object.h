#ifndef WORKSPACE_OBJECT_H

#include <glib-object.h>

typedef struct {
  gint id;
  char *name;
} Workspace;

G_DECLARE_FINAL_TYPE(WorkspaceObject, workspace_object, WORKSPACE,
                     WORKSPACE_OBJECT, GObject)

WorkspaceObject *workspace_object_new(int id, char *name);
Workspace *workspace_object_get_state(WorkspaceObject *self);

#define WORKSPACE_STATE(list, pos)                                             \
  workspace_object_get_state(WORKSPACE_WORKSPACE_OBJECT(                       \
      g_list_model_get_object(G_LIST_MODEL(list), pos)))
#endif // !WORKSPACE_OBJECT_H
