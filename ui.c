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
    GtkWidget* overlay = gtk_overlay_new();
    app->ui->background_area = gtk_drawing_area_new();
    gtk_widget_set_name(app->ui->background_area, "notepad-background");
    gtk_container_add(GTK_CONTAINER(overlay), app->ui->background_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), scrolled_window);
    GtkWidget* editor_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    app->ui->line_numbers = gtk_drawing_area_new();
    gtk_widget_set_no_show_all(app->ui->line_numbers, TRUE);
    gtk_box_pack_start(GTK_BOX(editor_box), app->ui->line_numbers, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(editor_box), overlay, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), editor_box, TRUE, TRUE, 0);
    g_signal_connect(app->ui->background_area, "draw", G_CALLBACK(draw_editor_background), app);

    app->ui->text_view = gtk_text_view_new();
    gtk_widget_set_name(app->ui->text_view, "notepad-editor");
    app->ui->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->ui->text_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->ui->text_view), GTK_WRAP_WORD);

    // 为文本视图添加内边距
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app->ui->text_view), 10);

    gtk_container_add(GTK_CONTAINER(scrolled_window), app->ui->text_view);

    setup_editor_decorations(app);

    // 创建状态栏
    app->ui->status_bar = create_status_bar(app);
    gtk_box_pack_start(GTK_BOX(vbox), app->ui->status_bar, FALSE, FALSE, 0);

    // 连接信号
    g_signal_connect(app->ui->text_view, "key-press-event", G_CALLBACK(on_editor_tab_key), app);
    g_signal_connect(app->ui->buffer, "changed", G_CALLBACK(on_text_changed), app);
    g_signal_connect(app->ui->buffer, "notify::cursor-position", G_CALLBACK(on_cursor_moved), app);
    g_signal_connect(app->ui->buffer, "insert-text", G_CALLBACK(on_text_insert), app);
    g_signal_connect(app->ui->buffer, "delete-range", G_CALLBACK(on_text_delete), app);
    g_signal_connect(app->ui->window, "delete-event", G_CALLBACK(on_window_delete), app);
    g_signal_connect(app->ui->window, "destroy", G_CALLBACK(on_quit), NULL);

    apply_font_with_fallback(app, app->ui->primary_font, app->ui->fallback_font);
    apply_appearance(app);

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
    GtkWidget* tab_settings_item = gtk_menu_item_new_with_label("制表符设置");
    g_signal_connect(tab_settings_item, "activate", G_CALLBACK(on_tab_settings), app);

    // 视图菜单
    GtkWidget* view_item = gtk_menu_item_new_with_mnemonic("视图(_V)");
    GtkWidget* view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);

    GtkWidget* word_wrap_item = gtk_check_menu_item_new_with_label("自动换行");
    GtkWidget* font_item = gtk_menu_item_new_with_label("字体");
    GtkWidget* numbers_item = gtk_check_menu_item_new_with_label("显示行号");
    GtkWidget* frame_item = gtk_check_menu_item_new_with_label("框选中行");
    gtk_widget_set_tooltip_text(frame_item, "框出选区覆盖的逻辑行；无选区时框出光标所在行");
    g_signal_connect(numbers_item, "toggled", G_CALLBACK(on_line_numbers_toggled), app);
    g_signal_connect(frame_item, "toggled", G_CALLBACK(on_line_frame_toggled), app);
    GtkWidget* background_settings_item = gtk_menu_item_new_with_label("背景设置");
    GtkWidget* theme_item = gtk_menu_item_new_with_label("外观模式");
    GtkWidget* theme_menu = gtk_menu_new();
    GtkWidget* light_item = gtk_radio_menu_item_new_with_label(NULL, "浅色");
    GtkWidget* dark_item = gtk_radio_menu_item_new_with_label_from_widget(
        GTK_RADIO_MENU_ITEM(light_item), "深色");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(theme_item), theme_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(theme_menu), light_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(theme_menu), dark_item);
    g_object_set_data(G_OBJECT(dark_item), "dark-mode", GINT_TO_POINTER(TRUE));
    g_signal_connect(light_item, "toggled", G_CALLBACK(on_theme_toggled), app);
    g_signal_connect(dark_item, "toggled", G_CALLBACK(on_theme_toggled), app);


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
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), tab_settings_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), word_wrap_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), numbers_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), frame_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), font_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), background_settings_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), theme_item);

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

    // 栈内保存下一次需要执行的操作。
    action->type = action->type == UNDO_INSERT ? UNDO_DELETE : UNDO_INSERT;
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

    action->type = action->type == UNDO_INSERT ? UNDO_DELETE : UNDO_INSERT;
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

/* Each widget owns exactly one font provider. Reloading it also handles size-only edits. */
static void set_widget_font(GtkWidget* widget, const PangoFontDescription* primary,
                            const PangoFontDescription* fallback)
{
    GtkCssProvider* provider = g_object_get_data(G_OBJECT(widget), "notepad-font-provider");
    if (!provider)
    {
        provider = gtk_css_provider_new();
        gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_set_data_full(G_OBJECT(widget), "notepad-font-provider", provider, g_object_unref);
    }
    const gchar* family = pango_font_description_get_family(primary);
    const gchar* fallback_family = pango_font_description_get_family(fallback);
    gchar* escaped_family = g_strescape(family ? family : "sans-serif", NULL);
    gchar* escaped_fallback = g_strescape(fallback_family ? fallback_family : "sans-serif", NULL);
    gchar size[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_dtostr(size, sizeof(size), (gdouble)pango_font_description_get_size(primary) / PANGO_SCALE);
    PangoStyle style = pango_font_description_get_style(primary);
    const gchar* stretches[] =
    {
        "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "normal",
        "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"
    };
    gchar* css = g_strdup_printf(
        "textview { font-family: \"%s\", \"%s\", sans-serif; font-size: %s%s; "
        "font-weight: %d; font-style: %s; font-stretch: %s; }",
        escaped_family, escaped_fallback, size,
        pango_font_description_get_size_is_absolute(primary) ? "px" : "pt",
        CLAMP(((pango_font_description_get_weight(primary) + 50) / 100) * 100, 100, 900),
        style == PANGO_STYLE_ITALIC ? "italic" : style == PANGO_STYLE_OBLIQUE ? "oblique" : "normal",
        stretches[CLAMP(pango_font_description_get_stretch(primary), 0, 8)]);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_free(css);
    g_free(escaped_family);
    g_free(escaped_fallback);
}

void apply_font_with_fallback(NotepadApp* app, const gchar* primary_font, const gchar* fallback_font)
{
    PangoFontDescription* primary = pango_font_description_from_string(primary_font);
    PangoFontDescription* fallback = pango_font_description_from_string(fallback_font);
    set_widget_font(app->ui->text_view, primary, fallback);
    pango_font_description_free(primary);
    pango_font_description_free(fallback);
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

static void background_option_toggled(GtkToggleButton* button, gpointer data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

static void background_default_toggled(GtkToggleButton* button, gpointer data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(data), !gtk_toggle_button_get_active(button));
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
    GtkWidget* default_radio = gtk_radio_button_new_with_label(NULL, "跟随主题");
    GtkWidget* color_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(default_radio), "纯色背景");
    GtkWidget* image_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(color_radio), "图片背景");

    gtk_box_pack_start(GTK_BOX(type_box), default_radio, FALSE, FALSE, 0);
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
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(color_button), FALSE);

    // 设置默认颜色为白色
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button), &app->ui->background_color);

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
    gtk_entry_set_text(GTK_ENTRY(file_entry), app->ui->background_image_path ? app->ui->background_image_path : "");
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

    GtkWidget* opacity_label = gtk_label_new("背景透明度:");
    GtkAdjustment* transparency = gtk_adjustment_new(
        (1.0 - app->ui->background_opacity) * 100.0, 0.0, 100.0, 1.0, 10.0, 0.0);
    GtkWidget* opacity_scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, transparency);
    GtkWidget* opacity_spin = gtk_spin_button_new(transparency, 1.0, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(opacity_spin), TRUE);
    gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(opacity_spin), GTK_UPDATE_IF_VALID);
    gtk_scale_set_draw_value(GTK_SCALE(opacity_scale), FALSE);
    gtk_widget_set_tooltip_text(opacity_scale, "0% 为完全不透明，100% 为完全透明，显示主题底色");
    g_object_set_data(G_OBJECT(dialog), "background-transparency", opacity_spin);
    g_object_set_data(G_OBJECT(dialog), "background-transparency-scale", opacity_scale);
    g_object_set_data(G_OBJECT(dialog), "background-default", default_radio);
    g_object_set_data(G_OBJECT(dialog), "background-color", color_radio);
    g_object_set_data(G_OBJECT(dialog), "background-image", image_radio);
    g_object_set_data(G_OBJECT(dialog), "background-color-button", color_button);

    gtk_box_pack_start(GTK_BOX(opacity_box), opacity_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(opacity_box), opacity_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(opacity_box), opacity_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(opacity_box), gtk_label_new("%"), FALSE, FALSE, 0);

    // 连接信号处理函数
    g_signal_connect(browse_button, "clicked", G_CALLBACK(on_browse_image_file), file_entry);
    g_signal_connect(color_radio, "toggled", G_CALLBACK(background_option_toggled), color_frame);
    g_signal_connect(image_radio, "toggled", G_CALLBACK(background_option_toggled), image_frame);
    g_signal_connect(default_radio, "toggled", G_CALLBACK(background_default_toggled), opacity_frame);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(!app->ui->custom_background ? default_radio :
        app->ui->background_image ? image_radio : color_radio), TRUE);
    background_option_toggled(GTK_TOGGLE_BUTTON(color_radio), color_frame);
    background_option_toggled(GTK_TOGGLE_BUTTON(image_radio), image_frame);
    background_default_toggled(GTK_TOGGLE_BUTTON(default_radio), opacity_frame);

    gtk_widget_show_all(dialog);

    // 运行对话框
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK)
    {
        // 修复：完善确定按钮逻辑
        gboolean use_image = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(image_radio));
        gtk_spin_button_update(GTK_SPIN_BUTTON(opacity_spin));
        gdouble opacity = 1.0 - gtk_adjustment_get_value(transparency) / 100.0;

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(default_radio)))
        {
            reset_background(app);
        }
        else if (use_image)
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
            GdkRGBA color;
            gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), &color);
            color.alpha = opacity;
            apply_background_color(app, &color);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_browse_image_file(GtkWidget* widget, gpointer data)
{
    GtkWidget* dialog = gtk_file_chooser_dialog_new("选择背景图片",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)), GTK_FILE_CHOOSER_ACTION_OPEN,
        "取消", GTK_RESPONSE_CANCEL, "选择", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "图片文件");
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(data), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
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
    refresh_editor_decorations(app);
    update_line_ending_type(app);
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
