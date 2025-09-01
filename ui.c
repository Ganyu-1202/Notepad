//
// Created by ganyu on 2025/8/16.
//

#include "ui.h"
#include "file_operations.h"
#include "output_exception.h"
#include <stdlib.h>
#include <string.h>

// 撤销/重做相关函数
void push_undo_action(NotepadApp* app, UndoType type, gint position, const gchar* text)
{
    if (!app->ui->recording_changes) return;

    UndoAction* action = (UndoAction*)malloc(sizeof(UndoAction));
    action->type = type;
    action->position = position;
    action->text = g_strdup(text);
    action->next = app->ui->undo_stack;
    app->ui->undo_stack = action;

    // 清空重做栈
    clear_undo_stack(&app->ui->redo_stack);
}

void clear_undo_stack(UndoAction** stack)
{
    while (*stack)
    {
        UndoAction* action = *stack;
        *stack = action->next;
        g_free(action->text);
        free(action);
    }
}

void on_text_insert(GtkTextBuffer* buffer, GtkTextIter* location, gchar* text, gint len, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    if (app->ui->recording_changes)
    {
        gint position = gtk_text_iter_get_offset(location);
        gchar* text_copy = g_strndup(text, len);
        push_undo_action(app, UNDO_DELETE, position, text_copy);
        g_free(text_copy);
    }
}

void on_text_delete(GtkTextBuffer* buffer, GtkTextIter* start, GtkTextIter* end, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    if (app->ui->recording_changes)
    {
        gint position = gtk_text_iter_get_offset(start);
        gchar* deleted_text = gtk_text_buffer_get_text(buffer, start, end, FALSE);
        push_undo_action(app, UNDO_INSERT, position, deleted_text);
        g_free(deleted_text);
    }
}

void on_cursor_moved(GtkTextBuffer* buffer, GParamSpec* pspec, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    update_cursor_position(app);
}

void setup_main_window(NotepadApp* app)
{
    app->ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->ui->window), "记事本");
    gtk_window_set_default_size(GTK_WINDOW(app->ui->window), 1080, 720);
    gtk_window_set_position(GTK_WINDOW(app->ui->window), GTK_WIN_POS_CENTER);

    // 创建加速器组
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app->ui->window), accel_group);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->ui->window), vbox);

    // 创建工具栏（菜单栏）
    GtkWidget* menu_bar = create_menu_bar(app, accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    const gchar* icon_path = "../notepad.png";
    gtk_window_set_icon_from_file(GTK_WINDOW(app->ui->window), icon_path, NULL);

    // 创建查找和替换栏，默认不可见
    app->ui->find_replace_bar = create_find_replace_bar(app);
    gtk_box_pack_start(GTK_BOX(vbox), app->ui->find_replace_bar, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(app->ui->find_replace_bar, TRUE);
    app->ui->find_replace_visible = FALSE;

    // 创建文本编辑区域
    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    app->ui->text_view = gtk_text_view_new();
    app->ui->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->ui->text_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->ui->text_view), GTK_WRAP_WORD);

    // 为文本视图添加内边距
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);

    gtk_container_add(GTK_CONTAINER(scrolled_window), app->ui->text_view);

    // 创建状态栏
    app->ui->status_bar = create_status_bar(app);
    gtk_box_pack_start(GTK_BOX(vbox), app->ui->status_bar, FALSE, FALSE, 0);

    // 连接信号
    g_signal_connect(app->ui->buffer, "changed", G_CALLBACK(on_text_changed), app);
    g_signal_connect(app->ui->buffer, "notify::cursor-position", G_CALLBACK(on_cursor_moved), app);
    g_signal_connect(app->ui->buffer, "insert-text", G_CALLBACK(on_text_insert), app);
    g_signal_connect(app->ui->buffer, "delete-range", G_CALLBACK(on_text_delete), app);
    g_signal_connect(app->ui->window, "delete-event", G_CALLBACK(on_window_delete), app);
    g_signal_connect(app->ui->window, "destroy", G_CALLBACK(on_quit), NULL);

    // 初始化状态栏信息
    update_cursor_position(app);
    update_line_ending_type(app);
    update_encoding_type(app);
}

GtkWidget* create_menu_bar(NotepadApp* app, GtkAccelGroup* accel_group)
{
    GtkWidget* menu_bar = gtk_menu_bar_new();

    // 文件菜单
    GtkWidget* file_menu = gtk_menu_new();
    GtkWidget* file_item = gtk_menu_item_new_with_mnemonic("文件(_F)");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);

    GtkWidget* new_item = gtk_menu_item_new_with_label("新建");
    GtkWidget* open_item = gtk_menu_item_new_with_label("打开");
    GtkWidget* save_item = gtk_menu_item_new_with_label("保存");
    GtkWidget* save_as_item = gtk_menu_item_new_with_label("另存为");
    GtkWidget* separator1 = gtk_separator_menu_item_new();
    GtkWidget* exit_item = gtk_menu_item_new_with_label("退出");

    // 编辑菜单
    GtkWidget* edit_item = gtk_menu_item_new_with_mnemonic("编辑(_E)");
    GtkWidget* edit_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);

    GtkWidget* revoke_item = gtk_menu_item_new_with_label("撤销");
    GtkWidget* redo_item = gtk_menu_item_new_with_label("重做");
    GtkWidget* separator2 = gtk_separator_menu_item_new();
    GtkWidget* find_and_replace_item = gtk_menu_item_new_with_label("查找和替换");
    GtkWidget* goto_item = gtk_menu_item_new_with_label("转到行");
    GtkWidget* separator3 = gtk_separator_menu_item_new();
    GtkWidget* select_all_item = gtk_menu_item_new_with_label("全选");

    // 视图菜单
    GtkWidget* view_item = gtk_menu_item_new_with_mnemonic("视图(_V)");
    GtkWidget* view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);

    GtkWidget* word_wrap_item = gtk_check_menu_item_new_with_label("自动换行");
    GtkWidget* font_item = gtk_menu_item_new_with_label("字体");
    GtkWidget* background_settings_item = gtk_menu_item_new_with_label("背景设置");

    // 帮助菜单
    GtkWidget* help_item = gtk_menu_item_new_with_mnemonic("帮助(_H)");
    GtkWidget* help_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);

    GtkWidget* about_item = gtk_menu_item_new_with_label("关于记事本");

    // 添加快捷键
    gtk_widget_add_accelerator(new_item, "activate", accel_group,
                               GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(open_item, "activate", accel_group,
                               GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(save_item, "activate", accel_group,
                               GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(revoke_item, "activate", accel_group,
                               GDK_KEY_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(redo_item, "activate", accel_group,
                               GDK_KEY_y, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(find_and_replace_item, "activate", accel_group,
                               GDK_KEY_f, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(goto_item, "activate", accel_group,
                               GDK_KEY_g, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(select_all_item, "activate", accel_group,
                               GDK_KEY_a, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(word_wrap_item, "activate", accel_group,
                               GDK_KEY_z, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    // 连接文件菜单信号
    g_signal_connect(new_item, "activate", G_CALLBACK(on_new_file), app);
    g_signal_connect(open_item, "activate", G_CALLBACK(on_open_file), app);
    g_signal_connect(save_item, "activate", G_CALLBACK(on_save_file), app);
    g_signal_connect(save_as_item, "activate", G_CALLBACK(on_save_as_file), app);
    g_signal_connect(exit_item, "activate", G_CALLBACK(on_exit), app);

    // 连接编辑菜单信号
    g_signal_connect(revoke_item, "activate", G_CALLBACK(on_revoke), app);
    g_signal_connect(redo_item, "activate", G_CALLBACK(on_redo), app);
    g_signal_connect(find_and_replace_item, "activate", G_CALLBACK(on_find_replace), app);
    g_signal_connect(goto_item, "activate", G_CALLBACK(on_goto_line), app);
    g_signal_connect(select_all_item, "activate", G_CALLBACK(on_select_all), app);
    g_signal_connect(word_wrap_item, "toggled", G_CALLBACK(on_word_wrap_toggle), app);
    g_signal_connect(font_item, "activate", G_CALLBACK(on_font_selection), app);
    g_signal_connect(background_settings_item, "activate", G_CALLBACK(on_background_settings), app);
    g_signal_connect(about_item, "activate", G_CALLBACK(on_about), app);

    // 添加到菜单
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_as_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator1);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), revoke_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), redo_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), separator2);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), find_and_replace_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), goto_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), separator3);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), select_all_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), word_wrap_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), font_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), background_settings_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), edit_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), view_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_item);

    return menu_bar;
}

GtkWidget* create_status_bar(NotepadApp* app)
{
    GtkWidget* status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(status_bar, -1, 25);

    // 添加填充空间
    GtkWidget* filler = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(status_bar), filler, TRUE, TRUE, 0);

    // 光标位置标签
    app->ui->cursor_label = gtk_label_new("行: 1, 列: 1");
    gtk_widget_set_size_request(app->ui->cursor_label, 120, -1);
    gtk_widget_set_halign(app->ui->cursor_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->ui->cursor_label, 10);
    gtk_widget_set_margin_end(app->ui->cursor_label, 10);

    // 行分隔符类型标签
    app->ui->line_ending_label = gtk_label_new("CRLF");
    gtk_widget_set_size_request(app->ui->line_ending_label, 60, -1);
    gtk_widget_set_halign(app->ui->line_ending_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(app->ui->line_ending_label, 5);
    gtk_widget_set_margin_end(app->ui->line_ending_label, 5);

    // 字符编码类型标签
    app->ui->encoding_label = gtk_label_new("UTF-8");
    gtk_widget_set_size_request(app->ui->encoding_label, 80, -1);
    gtk_widget_set_halign(app->ui->encoding_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(app->ui->encoding_label, 5);
    gtk_widget_set_margin_end(app->ui->encoding_label, 10);

    // 添加分隔符
    GtkWidget* separator1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    GtkWidget* separator2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

    // 将标签添加到状态栏
    gtk_box_pack_start(GTK_BOX(status_bar), app->ui->cursor_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar), separator1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar), app->ui->line_ending_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar), separator2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar), app->ui->encoding_label, FALSE, FALSE, 0);

    return status_bar;
}

GtkWidget* create_find_replace_bar(NotepadApp* app)
{
    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(outer_box, -1, 40);

    // 创建内部容器用于居中布局
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(bar, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(bar, 5);
    gtk_widget_set_margin_bottom(bar, 5);

    // 查找标签和输入框
    GtkWidget* find_label = gtk_label_new("查找:");
    app->ui->find_entry = gtk_entry_new();
    gtk_widget_set_size_request(app->ui->find_entry, 150, -1);

    // 替换标签和输入框
    GtkWidget* replace_label = gtk_label_new("替换:");
    app->ui->replace_entry = gtk_entry_new();
    gtk_widget_set_size_request(app->ui->replace_entry, 150, -1);

    // 按钮
    GtkWidget* find_next_button = gtk_button_new_with_label("查找下一个");
    GtkWidget* replace_button = gtk_button_new_with_label("替换");
    GtkWidget* replace_all_button = gtk_button_new_with_label("全部替换");
    GtkWidget* close_button = gtk_button_new_with_label("关闭");

    // 区分大小写 复选框
    GtkWidget* case_sensitive_check = gtk_check_button_new_with_label("区分大小写");
    gtk_box_pack_start(GTK_BOX(bar), case_sensitive_check, FALSE, FALSE, 0);
    app->ui->case_sensitive_check = case_sensitive_check;

    // 连接信号
    g_signal_connect(find_next_button, "clicked", G_CALLBACK(on_find_next), app);
    g_signal_connect(replace_button, "clicked", G_CALLBACK(on_replace), app);
    g_signal_connect(replace_all_button, "clicked", G_CALLBACK(on_replace_all), app);
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_close_find_replace), app);

    // 添加到内部容器
    gtk_box_pack_start(GTK_BOX(bar), find_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), app->ui->find_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), replace_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), app->ui->replace_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), find_next_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), replace_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), replace_all_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), close_button, FALSE, FALSE, 0);

    // 将内部容器添加到外部容器中居中
    gtk_box_pack_start(GTK_BOX(outer_box), bar, TRUE, FALSE, 0);

    return outer_box;
}

// 编辑功能实现
void on_revoke(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    if (!app->ui->undo_stack) return;

    UndoAction* action = app->ui->undo_stack;
    app->ui->undo_stack = action->next;

    // 暂停记录变化
    app->ui->recording_changes = FALSE;

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_offset(app->ui->buffer, &iter, action->position);

    if (action->type == UNDO_INSERT)
    {
        gtk_text_buffer_insert(app->ui->buffer, &iter, action->text, -1);
    }
    else
    { // UNDO_DELETE
        GtkTextIter end_iter = iter;
        gtk_text_iter_forward_chars(&end_iter, g_utf8_strlen(action->text, -1));
        gtk_text_buffer_delete(app->ui->buffer, &iter, &end_iter);
    }

    // 移动到重做栈
    action->next = app->ui->redo_stack;
    app->ui->redo_stack = action;

    // 恢复记录变化
    app->ui->recording_changes = TRUE;
}

void on_redo(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    if (!app->ui->redo_stack) return;

    UndoAction* action = app->ui->redo_stack;
    app->ui->redo_stack = action->next;

    // 暂停记录变化
    app->ui->recording_changes = FALSE;

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_offset(app->ui->buffer, &iter, action->position);

    if (action->type == UNDO_INSERT)
    {
        gtk_text_buffer_insert(app->ui->buffer, &iter, action->text, -1);
    }
    else
    { // UNDO_DELETE
        GtkTextIter end_iter = iter;
        gtk_text_iter_forward_chars(&end_iter, g_utf8_strlen(action->text, -1));
        gtk_text_buffer_delete(app->ui->buffer, &iter, &end_iter);
    }

    // 移动到撤销栈
    action->next = app->ui->undo_stack;
    app->ui->undo_stack = action;

    // 恢复记录变化
    app->ui->recording_changes = TRUE;
}

void on_find_replace(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    if (app->ui->find_replace_visible)
    {
        gtk_widget_hide(app->ui->find_replace_bar);
        app->ui->find_replace_visible = FALSE;
        gtk_widget_grab_focus(app->ui->text_view);
    }
    else
    {
        gtk_widget_set_no_show_all(app->ui->find_replace_bar, FALSE);
        gtk_widget_show_all(app->ui->find_replace_bar);
        app->ui->find_replace_visible = TRUE;
        gtk_widget_grab_focus(app->ui->find_entry);
    }
}

void on_goto_line(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    GtkWidget* dialog = gtk_dialog_new_with_buttons("转到行",
                                                    GTK_WINDOW(app->ui->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    "转到", GTK_RESPONSE_OK,
                                                    NULL);

    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(hbox, 10);
    gtk_widget_set_margin_end(hbox, 10);
    gtk_widget_set_margin_top(hbox, 10);
    gtk_widget_set_margin_bottom(hbox, 10);

    GtkWidget* label = gtk_label_new("行号:");
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content_area), hbox);

    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        const gchar* text = gtk_entry_get_text(GTK_ENTRY(entry));
        char* endptr;
        long line_number = strtol(text, &endptr, 10);

        if (*endptr == '\0' && line_number > 0)
        {
            // 获取总行数
            GtkTextIter end_iter;
            gtk_text_buffer_get_end_iter(app->ui->buffer, &end_iter);
            gint total_lines = gtk_text_iter_get_line(&end_iter) + 1;

            if (line_number <= total_lines)
            {
                GtkTextIter iter;
                gtk_text_buffer_get_iter_at_line(app->ui->buffer, &iter, line_number - 1);
                gtk_text_buffer_place_cursor(app->ui->buffer, &iter);
                gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->ui->text_view), &iter, 0.0, FALSE, 0.0, 0.0);
            }
            else
            {
                gchar* error_msg = g_strdup_printf("行号超出范围。文档共有 %d 行。", total_lines);
                show_error_dialog(GTK_WINDOW(app->ui->window), "无效行号", error_msg);
                g_free(error_msg);
            }
        }
        else
        {
            show_error_dialog(GTK_WINDOW(app->ui->window), "无效输入", "请输入有效的行号。");
        }
    }
    gtk_widget_destroy(dialog);
}

void on_select_all(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gtk_text_buffer_select_range(app->ui->buffer, &start, &end);
}

// 查找替换功能
void on_find_next(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    const gchar* search_text = gtk_entry_get_text(GTK_ENTRY(app->ui->find_entry));
    gboolean case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->ui->case_sensitive_check));
    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY;

    if (!case_sensitive)
        flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;

    if (g_utf8_strlen(search_text, -1) == 0)
    {
        show_info_dialog(GTK_WINDOW(app->ui->window), "查找", "请输入要查找的内容。");
        return;
    }

    GtkTextIter start, match_start, match_end;
    GtkTextMark* insert_mark = gtk_text_buffer_get_insert(app->ui->buffer);
    gtk_text_buffer_get_iter_at_mark(app->ui->buffer, &start, insert_mark);

    // 如果有选中文本，从选中区域结束位置开始搜索
    if (gtk_text_buffer_get_selection_bounds(app->ui->buffer, NULL, &start))
    {
        // 从选中区域结束位置开始
    }

    // 从当前位置向前搜索
    if (gtk_text_iter_forward_search(&start, search_text, flags,
                                     &match_start, &match_end, NULL))
    {
        gtk_text_buffer_select_range(app->ui->buffer, &match_start, &match_end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->ui->text_view), &match_start, 0.0, FALSE, 0.0, 0.0);
    }
    else
    {
        // 从文档开头重新搜索
        gtk_text_buffer_get_start_iter(app->ui->buffer, &start);
        if (gtk_text_iter_forward_search(&start, search_text, flags,
                                         &match_start, &match_end, NULL))
        {
            gtk_text_buffer_select_range(app->ui->buffer, &match_start, &match_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->ui->text_view), &match_start, 0.0, FALSE, 0.0, 0.0);
        }
        else
        {
            show_info_dialog(GTK_WINDOW(app->ui->window), "查找", "找不到匹配项。");
        }
    }
}

void on_replace(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    const gchar* search_text = gtk_entry_get_text(GTK_ENTRY(app->ui->find_entry));
    const gchar* replace_text = gtk_entry_get_text(GTK_ENTRY(app->ui->replace_entry));

    GtkTextIter start, end;
    if (gtk_text_buffer_get_selection_bounds(app->ui->buffer, &start, &end))
    {
        gchar* selected_text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);
        if (g_strcmp0(selected_text, search_text) == 0)
        {
            gtk_text_buffer_delete(app->ui->buffer, &start, &end);
            gtk_text_buffer_insert(app->ui->buffer, &start, replace_text, -1);
        }
        g_free(selected_text);
    }

    on_find_next(widget, data);
}

void on_replace_all(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    const gchar* search_text = gtk_entry_get_text(GTK_ENTRY(app->ui->find_entry));
    const gchar* replace_text = gtk_entry_get_text(GTK_ENTRY(app->ui->replace_entry));

    if (g_utf8_strlen(search_text, -1) == 0)
    {
        show_info_dialog(GTK_WINDOW(app->ui->window), "替换", "请输入要查找的内容。");
        return;
    }

    gint count = 0;
    size_t search_len = strlen(search_text);

    // 暂停撤销记录
    app->ui->recording_changes = FALSE;

    // 获取所有文本
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);

    // 使用字符串替换避免迭代器问题
    GString* new_text = g_string_new("");
    gchar* pos = text;
    gchar* found;

    while ((found = strstr(pos, search_text)) != NULL)
    {
        // 添加找到位置之前的文本
        g_string_append_len(new_text, pos, (gssize)(found - pos));
        // 添加替换文本
        g_string_append(new_text, replace_text);
        // 移动到下一个搜索位置
        pos = found + search_len;
        count++;
    }
    // 添加剩余文本
    g_string_append(new_text, pos);

    // 替换整个缓冲区内容
    gtk_text_buffer_set_text(app->ui->buffer, new_text->str, -1);

    g_string_free(new_text, TRUE);
    g_free(text);

    // 恢复撤销记录
    app->ui->recording_changes = TRUE;

    gchar* message = g_strdup_printf("已替换 %d 个匹配项", count);
    show_info_dialog(GTK_WINDOW(app->ui->window), "替换完成", message);
    g_free(message);
}

void on_close_find_replace(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    gtk_widget_hide(app->ui->find_replace_bar);
    gtk_widget_set_no_show_all(app->ui->find_replace_bar, TRUE);
    app->ui->find_replace_visible = FALSE;
    gtk_widget_grab_focus(app->ui->text_view);
}

void on_font_selection(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    // 创建字体选择对话框
    GtkWidget* dialog = gtk_dialog_new_with_buttons("字体设置",
                                                    GTK_WINDOW(app->ui->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "确定", GTK_RESPONSE_OK,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    NULL);

    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_size_request(dialog, 450, 350);

    // 创建主容器
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 15);
    gtk_widget_set_margin_end(main_box, 15);
    gtk_widget_set_margin_top(main_box, 15);
    gtk_widget_set_margin_bottom(main_box, 15);

    // 首要字体选择
    GtkWidget* primary_label = gtk_label_new("首要字体:");
    gtk_widget_set_halign(primary_label, GTK_ALIGN_START);

    GtkWidget* primary_font_button = gtk_font_button_new();
    gtk_font_chooser_set_level(GTK_FONT_CHOOSER(primary_font_button),
                               GTK_FONT_CHOOSER_LEVEL_FAMILY | GTK_FONT_CHOOSER_LEVEL_SIZE);

    // 设置当前首要字体
    if (app->ui->primary_font)
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(app->ui->primary_font);
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(primary_font_button), font_desc);
        pango_font_description_free(font_desc);
    }
    else
    {
        // 使用支持中文的默认字体
        PangoFontDescription* default_font = pango_font_description_from_string("Microsoft YaHei 12");
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(primary_font_button), default_font);
        pango_font_description_free(default_font);
    }

    // 备选字体选择
    GtkWidget* fallback_label = gtk_label_new("备选字体:");
    gtk_widget_set_halign(fallback_label, GTK_ALIGN_START);

    GtkWidget* fallback_font_button = gtk_font_button_new();
    gtk_font_chooser_set_level(GTK_FONT_CHOOSER(fallback_font_button),
                               GTK_FONT_CHOOSER_LEVEL_FAMILY | GTK_FONT_CHOOSER_LEVEL_SIZE);

    // 设置当前备选字体
    if (app->ui->fallback_font)
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(app->ui->fallback_font);
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(fallback_font_button), font_desc);
        pango_font_description_free(font_desc);
    }
    else
    {
        // 使用支持中文的备选字体
        PangoFontDescription* default_font = pango_font_description_from_string("SimSun 12");
        gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(fallback_font_button), default_font);
        pango_font_description_free(default_font);
    }

    // 预览文本
    GtkWidget* preview_label = gtk_label_new("预览:");
    gtk_widget_set_halign(preview_label, GTK_ALIGN_START);

    GtkWidget* preview_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(preview_text), FALSE);
    gtk_widget_set_size_request(preview_text, -1, 200);

    GtkTextBuffer* preview_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(preview_text));
    gtk_text_buffer_set_text(preview_buffer,
                             "这是字体预览文本 Font Preview Text\n中文测试 English Test 123456\nAaBbCc 0123456789", -1);

    GtkWidget* preview_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(preview_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(preview_scrolled), preview_text);

    // 添加到容器
    gtk_box_pack_start(GTK_BOX(main_box), primary_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), primary_font_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), fallback_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), fallback_font_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), preview_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), preview_scrolled, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content_area), main_box);

    // 字体变化时更新预览
    g_signal_connect(primary_font_button, "font-set",
                     G_CALLBACK(on_font_preview_update), preview_text);
    g_signal_connect(fallback_font_button, "font-set",
                     G_CALLBACK(on_font_preview_update), preview_text);

    // 初始预览更新
    update_font_preview(preview_text, primary_font_button, fallback_font_button);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        // 获取选择的字体
        PangoFontDescription* primary_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(primary_font_button));
        PangoFontDescription* fallback_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(fallback_font_button));

        gchar* primary_font = pango_font_description_to_string(primary_desc);
        gchar* fallback_font = pango_font_description_to_string(fallback_desc);

        // 保存字体设置
        if (app->ui->primary_font)
        {
            g_free(app->ui->primary_font);
        }
        if (app->ui->fallback_font)
        {
            g_free(app->ui->fallback_font);
        }

        app->ui->primary_font = g_strdup(primary_font);
        app->ui->fallback_font = g_strdup(fallback_font);

        // 应用字体
        apply_font_with_fallback(app, primary_font, fallback_font);

        g_free(primary_font);
        g_free(fallback_font);
        pango_font_description_free(primary_desc);
        pango_font_description_free(fallback_desc);
    }

    gtk_widget_destroy(dialog);
}

// 字体预览更新回调
void on_font_preview_update(GtkWidget* font_button, gpointer data)
{
    GtkWidget* preview_text = (GtkWidget*)data;

    // 获取对话框
    GtkWidget* dialog = gtk_widget_get_toplevel(font_button);

    // 找到两个字体按钮
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GList* children = gtk_container_get_children(GTK_CONTAINER(content_area));
    GtkWidget* main_box = GTK_WIDGET(children->data);
    g_list_free(children);

    children = gtk_container_get_children(GTK_CONTAINER(main_box));
    GtkWidget* primary_font_button = NULL;
    GtkWidget* fallback_font_button = NULL;

    // 遍历找到字体按钮
    for (GList* iter = children; iter != NULL; iter = iter->next)
    {
        if (GTK_IS_FONT_BUTTON(iter->data))
        {
            if (primary_font_button == NULL)
            {
                primary_font_button = GTK_WIDGET(iter->data);
            }
            else
            {
                fallback_font_button = GTK_WIDGET(iter->data);
                break;
            }
        }
    }
    g_list_free(children);

    if (primary_font_button && fallback_font_button)
    {
        update_font_preview(preview_text, primary_font_button, fallback_font_button);
    }
}

// 更新预览
void update_font_preview(GtkWidget* preview_text, GtkWidget* primary_button, GtkWidget* fallback_button)
{
    PangoFontDescription* primary_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(primary_button));
    PangoFontDescription* fallback_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(fallback_button));

    // 创建字体列表字符串
    gchar* primary_font = pango_font_description_to_string(primary_desc);
    gchar* fallback_font = pango_font_description_to_string(fallback_desc);

    // 创建CSS样式提供者
    GtkCssProvider* css_provider = gtk_css_provider_new();
    gchar* css_data = g_strdup_printf(
        "textview { font-family: \"%s\", \"%s\"; font-size: %dpx; }",
        pango_font_description_get_family(primary_desc),
        pango_font_description_get_family(fallback_desc),
        pango_font_description_get_size(primary_desc) / PANGO_SCALE
    );

    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    GtkStyleContext* style_context = gtk_widget_get_style_context(preview_text);
    gtk_style_context_add_provider(style_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_free(primary_font);
    g_free(fallback_font);
    g_free(css_data);
    pango_font_description_free(primary_desc);
    pango_font_description_free(fallback_desc);
    g_object_unref(css_provider);
}

// 应用字体并设置备选
void apply_font_with_fallback(NotepadApp* app, const gchar* primary_font, const gchar* fallback_font)
{
    // 解析首要字体
    PangoFontDescription* primary_desc = pango_font_description_from_string(primary_font);
    PangoFontDescription* fallback_desc = pango_font_description_from_string(fallback_font);

    // 获取字体族名
    const gchar* primary_family = pango_font_description_get_family(primary_desc);
    const gchar* fallback_family = pango_font_description_get_family(fallback_desc);

    // 创建CSS样式提供者，包含中文字体支持
    GtkCssProvider* css_provider = gtk_css_provider_new();
    gchar* css_data = g_strdup_printf(
        "textview { "
        "font-family: \"%s\", \"%s\", \"Microsoft YaHei\", \"SimSun\", \"Arial Unicode MS\", sans-serif; "
        "font-size: %dpx; "
        "font-weight: %d; "
        "font-style: %s; "
        "}",
        primary_family ? primary_family : "Microsoft YaHei",
        fallback_family ? fallback_family : "SimSun",
        pango_font_description_get_size(primary_desc) / PANGO_SCALE,
        pango_font_description_get_weight(primary_desc),
        pango_font_description_get_style(primary_desc) == PANGO_STYLE_ITALIC ? "italic" : "normal"
    );

    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    // 应用到文本视图
    GtkStyleContext* style_context = gtk_widget_get_style_context(app->ui->text_view);

    // 移除之前的字体样式提供者
    gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(),
                                                 GTK_STYLE_PROVIDER(css_provider));

    gtk_style_context_add_provider(style_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // 验证字体应用结果
    validate_font_application(app, primary_font, fallback_font);

    g_free(css_data);
    pango_font_description_free(primary_desc);
    pango_font_description_free(fallback_desc);
    g_object_unref(css_provider);
}

// 验证字体应用
void validate_font_application(NotepadApp* app, const gchar* primary_font, const gchar* fallback_font)
{
    PangoFontDescription* primary_desc = pango_font_description_from_string(primary_font);
    const gchar* primary_family = pango_font_description_get_family(primary_desc);

    // 获取实际应用的字体
    PangoContext* context = gtk_widget_get_pango_context(app->ui->text_view);
    PangoFontDescription* current_font = pango_context_get_font_description(context);

    gchar* message;
    if (current_font)
    {
        const gchar* current_family = pango_font_description_get_family(current_font);

        if (current_family && primary_family && strcmp(current_family, primary_family) == 0)
        {
            message = g_strdup_printf("已应用首要字体: %s", primary_family);
        }
        else
        {
            PangoFontDescription* fallback_desc = pango_font_description_from_string(fallback_font);
            const gchar* fallback_family = pango_font_description_get_family(fallback_desc);
            message = g_strdup_printf("首要字体不可用，已使用备选字体: %s",
                                      fallback_family ? fallback_family : "默认字体");
            pango_font_description_free(fallback_desc);
            output_exception_message(EXCEPTION_WARNING, message);
        }
    }
    else
    {
        message = g_strdup("字体设置已应用");
    }

    // show_info_dialog(GTK_WINDOW(app->ui->window), "字体设置", message);


    g_free(message);
    pango_font_description_free(primary_desc);
}

void on_word_wrap_toggle(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

    if (active)
    {
        // 使用 GTK_WRAP_WORD_CHAR 来支持所有字符的换行
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->ui->text_view), GTK_WRAP_WORD_CHAR);
    }
    else
    {
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->ui->text_view), GTK_WRAP_NONE);
    }
}

void on_background_settings(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    // 创建背景设置对话框
    GtkWidget* dialog = gtk_dialog_new_with_buttons("背景设置",
                                                    GTK_WINDOW(app->ui->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "确定", GTK_RESPONSE_OK,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 400);

    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    // 背景类型选择
    GtkWidget* type_frame = gtk_frame_new("背景类型");
    gtk_box_pack_start(GTK_BOX(vbox), type_frame, FALSE, FALSE, 0);

    GtkWidget* type_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(type_box), 10);
    gtk_container_add(GTK_CONTAINER(type_frame), type_box);

    // 单选按钮组
    GtkWidget* color_radio = gtk_radio_button_new_with_label(NULL, "纯色背景");
    GtkWidget* image_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(color_radio), "图片背景");

    gtk_box_pack_start(GTK_BOX(type_box), color_radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), image_radio, FALSE, FALSE, 0);

    // 纯色设置区域
    GtkWidget* color_frame = gtk_frame_new("颜色设置");
    gtk_box_pack_start(GTK_BOX(vbox), color_frame, FALSE, FALSE, 0);

    GtkWidget* color_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(color_box), 10);
    gtk_container_add(GTK_CONTAINER(color_frame), color_box);

    GtkWidget* color_label = gtk_label_new("背景颜色:");
    GtkWidget* color_button = gtk_color_button_new();
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(color_button), TRUE);

    // 设置默认颜色为白色
    GdkRGBA white = {1.0, 1.0, 1.0, 1.0};
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button), &white);

    gtk_box_pack_start(GTK_BOX(color_box), color_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(color_box), color_button, FALSE, FALSE, 0);

    // 图片设置区域
    GtkWidget* image_frame = gtk_frame_new("图片设置");
    gtk_box_pack_start(GTK_BOX(vbox), image_frame, FALSE, FALSE, 0);

    GtkWidget* image_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(image_box), 10);
    gtk_container_add(GTK_CONTAINER(image_frame), image_box);

    GtkWidget* file_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* file_label = gtk_label_new("图片文件:");
    GtkWidget* file_entry = gtk_entry_new();
    GtkWidget* browse_button = gtk_button_new_with_label("浏览...");

    gtk_box_pack_start(GTK_BOX(file_box), file_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(file_box), file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(file_box), browse_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(image_box), file_box, FALSE, FALSE, 0);

    // 透明度设置
    GtkWidget* opacity_frame = gtk_frame_new("透明度设置");
    gtk_box_pack_start(GTK_BOX(vbox), opacity_frame, FALSE, FALSE, 0);

    GtkWidget* opacity_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(opacity_box), 10);
    gtk_container_add(GTK_CONTAINER(opacity_frame), opacity_box);

    GtkWidget* opacity_label = gtk_label_new("透明度:");
    GtkWidget* opacity_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_range_set_value(GTK_RANGE(opacity_scale), 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(opacity_scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(opacity_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits(GTK_SCALE(opacity_scale), 2);

    gtk_box_pack_start(GTK_BOX(opacity_box), opacity_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(opacity_box), opacity_scale, TRUE, TRUE, 0);

    // 连接信号处理函数
    g_signal_connect(browse_button, "clicked", G_CALLBACK(on_browse_image_file), file_entry);
    g_signal_connect(color_radio, "toggled", G_CALLBACK(on_color_radio_toggled), image_frame);

    // 设置初始状态：纯色背景选中，图片设置区域禁用
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(color_radio), TRUE);
    gtk_widget_set_sensitive(image_frame, FALSE);

    gtk_widget_show_all(dialog);

    // 运行对话框
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK)
    {
        // 修复：完善确定按钮逻辑
        gboolean use_image = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(image_radio));
        gdouble opacity = gtk_range_get_value(GTK_RANGE(opacity_scale));

        if (use_image)
        {
            // 应用图片背景
            const gchar* image_path = gtk_entry_get_text(GTK_ENTRY(file_entry));
            if (image_path && strlen(image_path) > 0)
            {
                apply_background_image(app, image_path, opacity);
            }
            else
            {
                show_error_dialog(GTK_WINDOW(app->ui->window), "错误", "请选择图片文件");
            }
        }
        else
        {
            // 应用纯色背景
            GdkRGBA color;
            gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), &color);
            apply_background_color(app, &color);
        }
    }

    gtk_widget_destroy(dialog);
}

void on_color_radio_toggled(GtkWidget* widget, gpointer data)
{
    GtkWidget* image_frame = (GtkWidget*)data;
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    // 纯色选中时，禁用图片设置
    gtk_widget_set_sensitive(image_frame, !active);
}

void on_image_radio_toggled(GtkWidget* widget, gpointer data)
{
    GtkWidget* color_frame = (GtkWidget*)data;
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (active)
    {
        gtk_widget_set_sensitive(color_frame, FALSE);
    }
    else
    {
        gtk_widget_set_sensitive(color_frame, TRUE);
    }
}

// 浏览图片文件
void on_browse_image_file(GtkWidget* widget, gpointer data)
{
    GtkWidget* file_entry = (GtkWidget*)data;
    GtkWidget* parent = gtk_widget_get_toplevel(widget);

    GtkWidget* file_dialog = gtk_file_chooser_dialog_new("选择背景图片",
                                                         GTK_WINDOW(parent),
                                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                                         "取消", GTK_RESPONSE_CANCEL,
                                                         "选择", GTK_RESPONSE_ACCEPT,
                                                         NULL);

    // 添加图片文件过滤器
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "图片文件");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_add_pattern(filter, "*.gif");
    gtk_file_filter_add_pattern(filter, "*.bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(file_dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_dialog));
        gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(file_dialog);
}

// 应用纯色背景
void apply_background_color(NotepadApp* app, const GdkRGBA* color)
{
    GtkCssProvider* css_provider = gtk_css_provider_new();

    // 使用正确的CSS语法设置背景色
    gchar* css_data = g_strdup_printf(
        "textview text { "
        "background-color: rgba(%d, %d, %d, %.2f); "
        "}",
        (int)(color->red * 255),
        (int)(color->green * 255),
        (int)(color->blue * 255),
        color->alpha
    );

    GError* error = NULL;
    if (!gtk_css_provider_load_from_data(css_provider, css_data, -1, &error))
    {
        g_warning("CSS 加载失败: %s", error->message);
        show_error_dialog(GTK_WINDOW(app->ui->window), "错误", "背景颜色设置失败");
        g_error_free(error);
        g_free(css_data);
        g_object_unref(css_provider);
        return;
    }

    GtkStyleContext* style_context = gtk_widget_get_style_context(app->ui->text_view);

    // 应用新的样式
    gtk_style_context_add_provider(style_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_free(css_data);
    g_object_unref(css_provider);
}

// 应用图片背景
void apply_background_image(NotepadApp* app, const gchar* image_path, gdouble opacity)
{
    // 检查文件是否存在
    if (!g_file_test(image_path, G_FILE_TEST_EXISTS))
    {
        show_error_dialog(GTK_WINDOW(app->ui->window), "错误", "图片文件不存在");
        return;
    }

    GtkCssProvider* css_provider = gtk_css_provider_new();

    // 处理文件路径，确保在Windows上正确处理反斜杠
    gchar* escaped_path = g_strescape(image_path, NULL);

    // 使用正确的CSS语法设置背景图片
    gchar* css_data = g_strdup_printf(
        "textview text { "
        "background-image: url('file:///%s'); "
        "background-repeat: no-repeat; "
        "background-position: center center; "
        "background-size: cover; "
        "opacity: %.2f; "
        "}",
        escaped_path, opacity
    );

    GError* error = NULL;
    if (!gtk_css_provider_load_from_data(css_provider, css_data, -1, &error))
    {
        g_warning("CSS 加载失败: %s", error->message);

        // 尝试简化的CSS
        g_free(css_data);
        g_error_free(error);

        css_data = g_strdup_printf(
            "textview { "
            "background: url('file:///%s') center/cover no-repeat; "
            "}",
            escaped_path
        );

        error = NULL;
        if (!gtk_css_provider_load_from_data(css_provider, css_data, -1, &error))
        {
            g_warning("简化CSS也加载失败: %s", error->message);
            show_error_dialog(GTK_WINDOW(app->ui->window), "错误", "背景图片设置失败");
            g_error_free(error);
            g_free(css_data);
            g_free(escaped_path);
            g_object_unref(css_provider);
            return;
        }
    }

    GtkStyleContext* style_context = gtk_widget_get_style_context(app->ui->text_view);

    // 应用新的样式
    gtk_style_context_add_provider(style_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // 显示成功信息
    gchar* success_msg = g_strdup_printf("背景图片已设置为: %s", image_path);
    show_info_dialog(GTK_WINDOW(app->ui->window), "设置成功", success_msg);

    g_free(success_msg);
    g_free(css_data);
    g_free(escaped_path);
    g_object_unref(css_provider);
}

void on_about(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;

    // 创建关于对话框
    GtkWidget* about_dialog = gtk_about_dialog_new();

    // 设置基本信息
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog), "记事本");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dialog), "1.0.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog),
                                   "© 2025 Ganyu-1202. All rights reserved.");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog),
                                  "一个简单而实用的文本编辑器\n支持查找替换、字体设置等功能");

    // 设置许可证信息
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(about_dialog), GTK_LICENSE_MIT_X11);

    // 设置网站信息
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_dialog), "https://github.com/Ganyu-1202/Notepad");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about_dialog), "访问项目主页");

    // 设置作者信息
    const gchar* authors[] = {
        "Ganyu-1202 <ganyu1202@example.com>",
        NULL
    };
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_dialog), authors);

    // 设置文档编写者
    const gchar* documenters[] = {
        "Ganyu-1202",
        NULL
    };
    gtk_about_dialog_set_documenters(GTK_ABOUT_DIALOG(about_dialog), documenters);

    // 设置翻译者（如果有的话）
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about_dialog),
                                            "翻译: Ganyu-1202");

    // 设置logo（可选，如果有应用图标的话）
    // GdkPixbuf* logo = gdk_pixbuf_new_from_file("icon.png", NULL);
    // if (logo) {
    //     gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about_dialog), logo);
    //     g_object_unref(logo);
    // }

    // 设置父窗口
    gtk_window_set_transient_for(GTK_WINDOW(about_dialog), GTK_WINDOW(app->ui->window));
    gtk_window_set_modal(GTK_WINDOW(about_dialog), TRUE);

    // 显示对话框
    gtk_dialog_run(GTK_DIALOG(about_dialog));

    // 销毁对话框
    gtk_widget_destroy(about_dialog);
}

void on_text_changed(GtkTextBuffer* buffer, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    notepad_set_modified(app, TRUE);
}

gboolean on_window_delete(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    NotepadApp* app = (NotepadApp*)data;
    return !notepad_check_save_changes(app);
}

void on_quit(GtkWidget* widget, gpointer data)
{
    gtk_main_quit();
}
