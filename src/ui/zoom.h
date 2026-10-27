#ifndef NOTEPAD_ZOOM_H
#define NOTEPAD_ZOOM_H

#include "ui/ui.h"

void initialize_zoom(NotepadApp* app);
void clear_zoom(NotepadApp* app);
void set_app_zoom(NotepadApp* app, int percent);
GtkWidget* create_zoom_menu(NotepadApp* app);

#endif
