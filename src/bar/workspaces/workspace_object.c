#include "workspace_object.h"
#include "glib.h"
#include <gio/gio.h>

struct _WorkspaceObject {
  GObject parent_instance;
  Workspace *state;
};

G_DEFINE_TYPE(WorkspaceObject, workspace_object, G_TYPE_OBJECT)

static void workspace_object_dispose(GObject *obj) {
  WorkspaceObject *self = (WorkspaceObject *)obj;
  if (self->state) {
    g_free(self->state->name);
  }

  g_clear_pointer(&self->state, g_free); // free struct
  G_OBJECT_CLASS(workspace_object_parent_class)->dispose(obj);
}

static void workspace_object_class_init(WorkspaceObjectClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = workspace_object_dispose;
}

static void workspace_object_init(WorkspaceObject *self) {
  // no special init
}

WorkspaceObject *workspace_object_new(int id, char *name) {
  WorkspaceObject *obj = g_object_new(workspace_object_get_type(), NULL);
  if (!name) {
    g_error("Tried to create object with name: %s\n", name);
  }

  obj->state = g_new0(Workspace, 1);
  obj->state->id = id;
  obj->state->name = g_strdup(name);
  return obj;
}

Workspace *workspace_object_get_state(WorkspaceObject *self) {
  g_return_val_if_fail(WORKSPACE_IS_WORKSPACE_OBJECT(self), NULL);
  return self->state;
}
