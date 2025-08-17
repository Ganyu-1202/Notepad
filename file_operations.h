//
// Created by ganyu on 2025/8/16.
//

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "notepad.h"

extern void on_new_file(GtkWidget* widget, gpointer data); // 创建新文件
extern void on_open_file(GtkWidget* widget, gpointer data); // 打开文件
extern void on_save_file(GtkWidget* widget, gpointer data); // 保存文件
extern void on_save_as_file(GtkWidget* widget, gpointer data); // 另存为
extern void on_exit(GtkWidget* widget, gpointer data); // 退出应用

#endif // FILE_OPERATIONS_H
