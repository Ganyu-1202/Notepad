//
// Created by ganyu on 2025/8/16.
//

#include "notepad.h"
#include "ui.h"
#include <stdlib.h>
#include "file_operations.h"
#include "output_exception.h"

// 构造函数
NotepadApp* notepad_app_new(void)
{
    NotepadApp* app = (NotepadApp*)malloc(sizeof(NotepadApp));
    app->ui = (NotepadUI*)malloc(sizeof(NotepadUI));
    app->filename = NULL;
    app->is_modified = FALSE;
    app->is_saved = TRUE;

    // 初始化UI属性
    app->ui->window = NULL;
    app->ui->text_view = NULL;
    app->ui->buffer = NULL;
    app->ui->status_bar = NULL;
    app->ui->cursor_label = NULL;
    app->ui->line_ending_label = NULL;
    app->ui->encoding_label = NULL;
    app->ui->find_replace_bar = NULL;
    app->ui->find_entry = NULL;
    app->ui->replace_entry = NULL;
    app->ui->find_replace_visible = FALSE;

    // 初始化撤销/重做相关字段
    app->ui->undo_stack = NULL;
    app->ui->redo_stack = NULL;
    app->ui->recording_changes = TRUE;

    // 初始化字体设置 - 设置支持中文的字体
    app->ui->primary_font = g_strdup("Microsoft YaHei 12");
    app->ui->fallback_font = g_strdup("SimSun 12");

    return app;
}

// 析构函数
void notepad_app_free(NotepadApp* app)
{
    if (app)
    {
        if (app->filename)
        {
            g_free(app->filename);
        }
        if (app->ui)
        {
            // 释放字体设置
            if (app->ui->primary_font)
            {
                g_free(app->ui->primary_font);
            }
            if (app->ui->fallback_font)
            {
                g_free(app->ui->fallback_font);
            }

            // 清理撤销/重做栈
            clear_undo_stack(&app->ui->undo_stack);
            clear_undo_stack(&app->ui->redo_stack);
            free(app->ui);
        }
        free(app);
    }
}

void notepad_app_run(NotepadApp* app)
{
    setup_main_window(app);
    gtk_widget_show_all(app->ui->window);
    gtk_main();
}

void notepad_set_modified(NotepadApp* app, gboolean modified)
{
    app->is_modified = modified;

    // 更新状态栏和窗口标题
    const gchar* current_title = gtk_window_get_title(GTK_WINDOW(app->ui->window));
    gchar* new_title;

    if (modified && !g_str_has_suffix(current_title, "*"))
    {
        new_title = g_strdup_printf("%s*", current_title);
        gtk_window_set_title(GTK_WINDOW(app->ui->window), new_title);
        g_free(new_title);
    }
    else if (!modified && g_str_has_suffix(current_title, "*"))
    {
        gchar* base_title = g_strndup(current_title, strlen(current_title) - 1);
        gtk_window_set_title(GTK_WINDOW(app->ui->window), base_title);
        g_free(base_title);
    }
}

gboolean notepad_check_save_changes(NotepadApp* app)
{
    if (!app->is_modified)
        return TRUE; // 没有修改，直接返回

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(app->ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "文件已修改，是否保存更改？");
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           "保存", GTK_RESPONSE_YES,
                           "不保存", GTK_RESPONSE_NO,
                           "取消", GTK_RESPONSE_CANCEL,
                           NULL);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    switch (response)
    {
        case GTK_RESPONSE_YES:
            // 用户选择保存
            if (app->filename)
            {
                GtkTextIter start, end;
                gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
                gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);
                gboolean saved = g_file_set_contents(app->filename, text, -1, NULL);
                g_free(text);

                if (saved)
                {
                    notepad_set_modified(app, FALSE);
                    return TRUE; // 保存成功
                }
                gtk_widget_show_all(app->ui->window); // 显示错误信息
                return FALSE; // 保存失败
            }
            else
            {
                // 新文件，需要另存
                on_save_as_file(NULL, app);
                return !app->is_modified;
            }
        case GTK_RESPONSE_NO:
            return TRUE; // 不保存，直接返回
        case GTK_RESPONSE_CANCEL:
        default:
            return FALSE; // 取消操作
    }
}

void update_cursor_position(NotepadApp* app)
{
    if (!app->ui->cursor_label)
        return;

    GtkTextIter iter;
    GtkTextMark* mark = gtk_text_buffer_get_insert(app->ui->buffer);
    gtk_text_buffer_get_iter_at_mark(app->ui->buffer, &iter, mark);

    gint line = gtk_text_iter_get_line(&iter) + 1;  // 行号从1开始
    gint column = gtk_text_iter_get_line_offset(&iter) + 1;  // 列号从1开始

    gchar* position_text = g_strdup_printf("行: %d, 列: %d", line, column);
    gtk_label_set_text(GTK_LABEL(app->ui->cursor_label), position_text);
    g_free(position_text);
}

void update_line_ending_type(NotepadApp* app)
{
    if (!app->ui->line_ending_label)
        return;

    // 获取文本内容分析行分隔符
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);

    const gchar* line_ending = "CRLF";  // 默认值

    if (text && strlen(text) > 0)
    {
        if (strstr(text, "\r\n"))
            line_ending = "CRLF";  // Windows
        else if (strstr(text, "\n"))
            line_ending = "LF";    // Unix/Linux
        else if (strstr(text, "\r"))
            line_ending = "CR";    // Mac (经典)
        else
            line_ending = "[unknown]";
    }

    gtk_label_set_text(GTK_LABEL(app->ui->line_ending_label), line_ending);
    g_free(text);
}

void update_encoding_type(NotepadApp* app)
{
    if (!app->ui->encoding_label)
        return;

    // 检测当前文件的字符编码
    const gchar* encoding = "UTF-8";  // 默认编码

    if (app->filename)
    {
        // 简单的编码检测，实际项目中可能需要更复杂的检测逻辑
        gchar* content;
        gsize length;
        if (g_file_get_contents(app->filename, &content, &length, NULL))
        {
            // 检查UTF-8 BOM
            if (length >= 3 &&
                (guchar)content[0] == 0xEF &&
                (guchar)content[1] == 0xBB &&
                (guchar)content[2] == 0xBF)
            {
                encoding = "UTF-8 BOM";
            }
            else if (g_utf8_validate(content, (gssize)length, NULL))
            {
                encoding = "UTF-8";
            }
            else
            {
                // 简单判断，可能是其他编码
                encoding = "ANSI";
            }
            g_free(content);
        }
    }

    gtk_label_set_text(GTK_LABEL(app->ui->encoding_label), encoding);
}
