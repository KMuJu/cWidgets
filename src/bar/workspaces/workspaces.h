#ifndef WORKSPACES_H

#include <gtk/gtk.h>

void start_workspace_widget(GtkWidget *box);

gpointer listen_to_hyprland_socket(gpointer data);

#endif // !WORKSPACES_H
