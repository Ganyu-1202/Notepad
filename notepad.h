//
// Created by ganyu on 2025/8/16.
//

#ifndef NOTEPAD_H
#define NOTEPAD_H


#include <stdbool.h>
#include "ui.h"

typedef enum
{
    ENCODING_UTF8,
    ENCODING_UTF8_BOM,
    ENCODING_UTF16_LE,
    ENCODING_UTF16_BE,
    ENCODING_UTF32_LE,
    ENCODING_UTF32_BE,
    ENCODING_GB18030
} TextEncoding;

typedef struct NotepadApp
{
    NotepadUI* ui;          // UI组件
    char* filename;
    bool is_modified;
    bool is_saved;
    TextEncoding encoding;
    bool encoding_inferred;
} NotepadApp;

extern NotepadApp* notepad_app_new(void); // 创建 NotepadApp 实例
extern void notepad_app_free(NotepadApp* app); // 释放 NotepadApp 实例
extern void notepad_app_run(NotepadApp* app); // 运行 Notepad 应用
extern void notepad_set_modified(NotepadApp* app, bool modified); // 设置修改状态
extern bool notepad_check_save_changes(NotepadApp* app); // 检查并提示保存
extern void update_cursor_position(NotepadApp* app);           // 更新光标位置
extern void update_line_ending_type(NotepadApp* app);          // 更新行分隔符类型
extern void update_encoding_type(NotepadApp* app);             // 更新字符集类型

#endif // NOTEPAD_H
