//
// Created by ganyu on 2025/8/16.
//

#include "output_exception.h"
#include <string.h>

static void on_copy_message(GtkButton* button, gpointer user_data)
{
    const gchar* message = (const gchar*)user_data;
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, message, -1);
}

static GtkWidget* create_copyable_dialog(GtkWindow* parent, GtkMessageType type,
                                         const gchar* title, const gchar* message)
{
    GtkWidget* dialog = gtk_message_dialog_new(parent,
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               type,
                                               GTK_BUTTONS_NONE,
                                               "%s", message);

    if (title)
        gtk_window_set_title(GTK_WINDOW(dialog), title);

    // 添加复制按钮
    GtkWidget* copy_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "复制消息", GTK_BUTTONS_NONE);
    g_signal_connect(copy_button, "clicked", G_CALLBACK(on_copy_message), (gpointer)g_strdup(message));

    // 添加关闭按钮
    gtk_dialog_add_button(GTK_DIALOG(dialog), "关闭", GTK_RESPONSE_CLOSE);

    // 设置消息文本可选择
    GtkWidget* message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog));
    GList* children = gtk_container_get_children(GTK_CONTAINER(message_area));

    for (GList* iter = children; iter != NULL; iter = iter->next)
    {
        if (GTK_IS_LABEL(iter->data))
        {
            gtk_label_set_selectable(GTK_LABEL(iter->data), TRUE);
        }
    }
    g_list_free(children);

    return dialog;
}

void show_info_dialog(GtkWindow* parent, const gchar* title, const gchar* message)
{
    GtkWidget* diialog = create_copyable_dialog(parent, GTK_MESSAGE_INFO, title, message);
    gtk_dialog_run(GTK_DIALOG(diialog));
    gtk_widget_destroy(diialog);
}

void show_warning_dialog(GtkWindow* parent, const gchar* title, const gchar* message)
{
    GtkWidget* dialog = create_copyable_dialog(parent, GTK_MESSAGE_WARNING, title, message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void show_error_dialog(GtkWindow* parent, const gchar* title, const gchar* message)
{
    GtkWidget* dialog = create_copyable_dialog(parent, GTK_MESSAGE_ERROR, title, message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean show_confirm_dialog(GtkWindow* parent, const gchar* title, const gchar* message)
{
    GtkWidget* dialog = gtk_message_dialog_new(parent,
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               "%s", message);

    if (title)
    {
        gtk_window_set_title(GTK_WINDOW(dialog), title);
    }

    // 添加复制按钮
    GtkWidget* copy_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "复制消息", GTK_RESPONSE_NONE);
    g_signal_connect(copy_button, "clicked", G_CALLBACK(on_copy_message), (gpointer)g_strdup(message));

    // 添加是/否按钮
    gtk_dialog_add_button(GTK_DIALOG(dialog), "否", GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "是", GTK_RESPONSE_YES);

    // 设置消息文本可选择
    GtkWidget* message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog));
    GList* children = gtk_container_get_children(GTK_CONTAINER(message_area));

    for (GList* iter = children; iter != NULL; iter = iter->next)
    {
        if (GTK_IS_LABEL(iter->data))
        {
            gtk_label_set_selectable(GTK_LABEL(iter->data), TRUE);
        }
    }
    g_list_free(children);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return (response == GTK_RESPONSE_YES);
}

void show_message_dialog(GtkWindow* parent, ExceptionType type,
                         const gchar* title, const gchar* message)
{
    switch (type)
    {
        case EXCEPTION_INFO:
            show_info_dialog(parent, title, message);
            break;
        case EXCEPTION_WARNING:
            show_warning_dialog(parent, title, message);
            break;
        case EXCEPTION_ERROR:
            show_error_dialog(parent, title, message);
            break;
        case EXCEPTION_QUESTION:
            show_confirm_dialog(parent, title, message);
            break;
        default:
            // 默认处理为信息对话框
            show_info_dialog(parent, title, message);
            break;
    }
}

void output_exception_message(ExceptionType type, const gchar* message)
{
    switch (type)
    {
        case EXCEPTION_NONE:
            puts(message);
            break;
        case EXCEPTION_INFO:
            puts(strcat("[INFO] ", message));
            break;
        case EXCEPTION_WARNING:
            puts(strcat("[WARNING] ", message));
            break;
        case EXCEPTION_ERROR:
            puts(strcat("[ERROR] ", message));
            break;
        case EXCEPTION_QUESTION:
            puts(strcat("[QUESTION] ", message));
            break;
        default:
            puts(strcat("[UNKNOWN] ", message));
            break;
    }
}
