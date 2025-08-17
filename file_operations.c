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

    gtk_text_buffer_set_text(app->ui->buffer, "", -1);
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

        gchar* content;
        gsize length;
        GError* error = NULL;
        if (g_file_get_contents(filename, &content, &length, &error))
        {
            gtk_text_buffer_set_text(app->ui->buffer, content, -1);
            g_free(content);

            if (app->filename)
                g_free(app->filename);
            app->filename = g_strdup(filename);

            gchar* title = g_strdup_printf("记事本 - %s", filename);
            gtk_window_set_title(GTK_WINDOW(app->ui->window), title);
            g_free(title);

            notepad_set_modified(app, FALSE); // 设置为未修改状态

            // 更新状态栏信息
            update_cursor_position(app);
            update_line_ending_type(app);
            update_encoding_type(app);
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

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);

    GError* error = NULL;
    if (g_file_set_contents(app->filename, text, -1, &error))
        notepad_set_modified(app, FALSE); // 保存成功，设置为未修改状态
    else
    {
        gchar* error_message = g_strdup_printf("无法保存文件 \"%s\":\n%s", app->filename, error->message);
        show_error_dialog(GTK_WINDOW(app->ui->window), "保存文件失败", error_message);
        g_free(error_message);
        g_error_free(error);
    }
    g_free(text);
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

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
        gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);

        GError* error = NULL;
        if (g_file_set_contents(filename, text, -1, &error))
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
        g_free(text);
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
