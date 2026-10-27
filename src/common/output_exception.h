//
// Created by ganyu on 2025/8/16.
//

#ifndef OUTPUT_EXCEPTION_H
#define OUTPUT_EXCEPTION_H

#include <gtk/gtk.h>

typedef enum ExceptionType
{
    EXCEPTION_NONE,
    EXCEPTION_INFO,
    EXCEPTION_WARNING,
    EXCEPTION_ERROR,
    EXCEPTION_QUESTION
} ExceptionType;

// 显示信息对话框
extern void show_info_dialog(GtkWindow* parent, const gchar* title, const gchar* message);

// 显示警告对话框
extern void show_warning_dialog(GtkWindow* parent, const gchar* title, const gchar* message);

// 显示错误对话框
extern void show_error_dialog(GtkWindow* parent, const gchar* title, const gchar* message);

// 显示确认对话框
extern gboolean show_confirm_dialog(GtkWindow* parent, const gchar* title, const gchar* message);

// 通用对话框函数
extern void show_message_dialog(GtkWindow* parent, ExceptionType type,
                               const gchar* title, const gchar* message);

// 输出异常信息（内部逻辑使用标准类型）
extern void output_exception_message(ExceptionType type, const char* message);

#endif // OUTPUT_EXCEPTION_H
