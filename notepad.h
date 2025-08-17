//
// Created by ganyu on 2025/8/16.
//

#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <gtk/gtk.h>
#include "ui.h"

typedef struct NotepadApp
{
    NotepadUI* ui;          // UI组件
    gchar* filename;
    gboolean is_modified;
    gboolean is_saved;
} NotepadApp;

extern NotepadApp* notepad_app_new(void); // 创建 NotepadApp 实例
extern void notepad_app_free(NotepadApp* app); // 释放 NotepadApp 实例
extern void notepad_app_run(NotepadApp* app); // 运行 Notepad 应用
extern void notepad_set_modified(NotepadApp* app, gboolean modified); // 设置修改状态
extern gboolean notepad_check_save_changes(NotepadApp* app); // 检查并提示保存
extern void update_cursor_position(NotepadApp* app);           // 更新光标位置
extern void update_line_ending_type(NotepadApp* app);          // 更新行分隔符类型
extern void update_encoding_type(NotepadApp* app);             // 更新字符集类型

#endif // NOTEPAD_H
