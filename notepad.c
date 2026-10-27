//
// Created by ganyu on 2025/8/16.
//

#include "notepad.h"
#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include "file_operations.h"
#include "output_exception.h"

// 构造函数
NotepadApp* notepad_app_new(void)
{
    NotepadApp* app = (NotepadApp*)malloc(sizeof(NotepadApp));
    app->ui = (NotepadUI*)calloc(1, sizeof(NotepadUI));
    app->filename = NULL;
    app->is_modified = false;
    app->is_saved = true;
    app->encoding = ENCODING_UTF8;
    app->encoding_inferred = false;

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
    app->ui->tab_space_count = 4;

    // 初始化字体设置 - 设置支持中文的字体
    app->ui->primary_font = g_strdup("Microsoft YaHei 12");
    app->ui->fallback_font = g_strdup("SimSun 12");
    app->ui->background_opacity = 1.0;
    gdk_rgba_parse(&app->ui->background_color, "#ffffff");

    return app;
}

// 析构函数
void notepad_app_free(NotepadApp* app)
{
    if (app)
    {
        if (app->filename)
        {
            free(app->filename);
        }
        if (app->ui)
        {
            clear_appearance(app);

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

void notepad_set_modified(NotepadApp* app, bool modified)
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

bool notepad_check_save_changes(NotepadApp* app)
{
    if (!app->is_modified)
        return true; // 没有修改，直接返回

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
                GError* error = NULL;
                if (save_document(app, app->filename, &error))
                {
                    return true;
                }
                show_error_dialog(GTK_WINDOW(app->ui->window), "保存失败", error->message);
                g_clear_error(&error);
                return false;
            }
            else
            {
                // 新文件，需要另存
                on_save_as_file(NULL, app);
                return !app->is_modified;
            }
        case GTK_RESPONSE_NO:
            return true; // 不保存，直接返回
        case GTK_RESPONSE_CANCEL:
        default:
            return false; // 取消操作
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

    const char* names[] =
    {
        "UTF-8", "UTF-8 BOM", "UTF-16 LE BOM", "UTF-16 BE BOM",
        "UTF-32 LE BOM", "UTF-32 BE BOM", "GB18030"
    };
    gtk_label_set_text(GTK_LABEL(app->ui->encoding_label), app->encoding_inferred ?
        "GB18030（推测）" : names[app->encoding]);
    gtk_widget_set_tooltip_text(app->ui->encoding_label, app->encoding_inferred ?
        "无 BOM 且不是有效 UTF-8，按 GB18030 解码；无标记编码无法完全可靠地自动识别" :
        "当前文档的读写编码；保存时保留此编码及 BOM");
}
