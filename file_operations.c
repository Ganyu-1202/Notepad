//
// Created by ganyu on 2025/8/16.
//

#include "file_operations.h"
#include "output_exception.h"

void on_new_file(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    // 检查当前文件是否需要保存
    if (!notepad_check_save_changes(app))
        return;

    app->ui->recording_changes = FALSE;
    gtk_text_buffer_set_text(app->ui->buffer, "", -1);
    app->ui->recording_changes = TRUE;
    clear_undo_stack(&app->ui->undo_stack);
    clear_undo_stack(&app->ui->redo_stack);
    app->encoding = ENCODING_UTF8;
    app->encoding_inferred = false;
    notepad_set_modified(app, false);
    if (app->filename)
    {
        g_free(app->filename);
        app->filename = NULL;
    }
    gtk_window_set_title(GTK_WINDOW(app->ui->window), "记事本 - 新文件");

    // 更新状态栏信息
    update_cursor_position(app);
    update_line_ending_type(app);
    update_encoding_type(app);
}

void on_open_file(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    // 检查当前文件是否需要保存
    if (!notepad_check_save_changes(app))
        return;

    GtkWidget* dialog = gtk_file_chooser_dialog_new("打开文件",
                                                    GTK_WINDOW(app->ui->window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    "打开", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GError* error = NULL;
        if (load_document(app, filename, &error))
        {
            update_cursor_position(app);
        }
        else
        {
            // 打开文件失败，显示错误对话框
            gchar* error_message = g_strdup_printf("无法打开文件：\"%s\":\n%s", filename, error->message);
            show_error_dialog(GTK_WINDOW(app->ui->window), "打开文件失败", error_message);
            g_free(error_message);
            g_error_free(error);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void on_save_file(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    if (!app->filename)
    {
        on_save_as_file(widget, data);
        return;
    }

    GError* error = NULL;
    if (save_document(app, app->filename, &error))
    {
        notepad_set_modified(app, false);
    }
    else
    {
        gchar* error_message = g_strdup_printf("无法保存文件 \"%s\":\n%s", app->filename, error->message);
        show_error_dialog(GTK_WINDOW(app->ui->window), "保存文件失败", error_message);
        g_free(error_message);
        g_error_free(error);
    }
}

void on_save_as_file(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    GtkWidget* dialog = gtk_file_chooser_dialog_new("另存为",
                                                    GTK_WINDOW(app->ui->window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    "保存", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        GError* error = NULL;
        if (save_document(app, filename, &error))
        {
            if (app->filename)
                g_free(app->filename);
            app->filename = g_strdup(filename);

            gchar* title = g_strdup_printf("记事本 - %s", filename);
            gtk_window_set_title(GTK_WINDOW(app->ui->window), title);
            g_free(title);

            notepad_set_modified(app, FALSE); // 保存成功，设置为未修改状态
        }
        else
        {
            gchar* error_message = g_strdup_printf("无法保存文件 \"%s\":\n%s", filename, error->message);
            show_error_dialog(GTK_WINDOW(app->ui->window), "保存文件失败", error_message);
            g_free(error_message);
            g_error_free(error);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void on_exit(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    if (notepad_check_save_changes(app))
        gtk_main_quit();
}
