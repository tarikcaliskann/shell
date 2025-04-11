#ifndef VIEW_H
#define VIEW_H

#include <gtk/gtk.h>
#include "model.h"

void update_messages(GtkWidget *message_view, ShmBuf *shmp);
void setup_gui(int *argc, char ***argv, ShmBuf *shmp);

#endif
