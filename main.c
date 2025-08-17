#include <stdio.h>
#include <gtk/gtk.h>
#include "notepad.h"
#include "ui.h"

int main(int argc, char* argv[])
{
    puts("记事本程序已启动。");
    gtk_init(&argc, &argv);

    NotepadApp* app = notepad_app_new();
    notepad_app_run(app);
    notepad_app_free(app);

    return 0;
}
