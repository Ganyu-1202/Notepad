#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

// 前向声明
typedef struct NotepadApp NotepadApp;

// 撤销操作类型
typedef enum
{
    UNDO_INSERT,
    UNDO_DELETE
} UndoType;

// 撤销操作结构
typedef struct UndoAction
{
    UndoType type;
    gint position;
    gchar* text;
    struct UndoAction* next;
} UndoAction;

typedef struct NotepadUI
{
    GtkWidget* window;
    GtkWidget* text_view;
    GtkTextBuffer* buffer;
    GtkWidget* status_bar;
    GtkWidget* cursor_label;
    GtkWidget* line_ending_label;
    GtkWidget* encoding_label;
    GtkWidget* find_replace_bar;
    GtkWidget* find_entry;
    GtkWidget* replace_entry;
    gboolean find_replace_visible;

    // 撤销/重做相关
    UndoAction* undo_stack;
    UndoAction* redo_stack;
    gboolean recording_changes;

    // 字体设置
    gchar* primary_font;    // 首要字体
    gchar* fallback_font;   // 备选字体
} NotepadUI;

extern void setup_main_window(NotepadApp* app);

extern GtkWidget* create_menu_bar(NotepadApp* app, GtkAccelGroup* accel_group);

extern GtkWidget* create_status_bar(NotepadApp* app);

extern GtkWidget* create_find_replace_bar(NotepadApp* app);

extern void on_quit(GtkWidget* widget, gpointer data);

extern void on_text_changed(GtkTextBuffer* buffer, gpointer data);

extern void on_cursor_moved(GtkTextBuffer* buffer, GParamSpec* pspec, gpointer data);

extern gboolean on_window_delete(GtkWidget* widget, GdkEvent* event, gpointer data);

// 编辑功能
extern void on_revoke(GtkWidget* widget, gpointer data);

extern void on_redo(GtkWidget* widget, gpointer data);

extern void on_find_replace(GtkWidget* widget, gpointer data);

extern void on_goto_line(GtkWidget* widget, gpointer data);

extern void on_select_all(GtkWidget* widget, gpointer data);

extern void on_word_wrap_toggle(GtkWidget* widget, gpointer data);

// 查找替换相关
extern void on_find_next(GtkWidget* widget, gpointer data);

extern void on_replace(GtkWidget* widget, gpointer data);

extern void on_replace_all(GtkWidget* widget, gpointer data);

extern void on_close_find_replace(GtkWidget* widget, gpointer data);

// 撤销/重做相关
extern void push_undo_action(NotepadApp* app, UndoType type, gint position, const gchar* text);

extern void clear_undo_stack(UndoAction** stack);

extern void on_text_insert(GtkTextBuffer* buffer, GtkTextIter* location, gchar* text, gint len, gpointer data);

extern void on_text_delete(GtkTextBuffer* buffer, GtkTextIter* start, GtkTextIter* end, gpointer data);

// 字体设置相关
extern void on_font_selection(GtkWidget* widget, gpointer data);

extern void update_font_preview(GtkWidget* preview_text, GtkWidget* primary_button, GtkWidget* fallback_button);

extern void apply_font_with_fallback(NotepadApp* app, const gchar* primary_font, const gchar* fallback_font);

extern void on_font_preview_update(GtkWidget* font_button, gpointer data);

extern void validate_font_application(NotepadApp* app, const gchar* primary_font, const gchar* fallback_font);

// 背景设置相关
extern void on_background_settings(GtkWidget* widget, gpointer data);
extern void on_browse_image_file(GtkWidget* widget, gpointer data);
extern void on_color_radio_toggled(GtkWidget* widget, gpointer data);
extern void on_image_radio_toggled(GtkWidget* widget, gpointer data);
extern void apply_background_color(NotepadApp* app, const GdkRGBA* color);
extern void apply_background_image(NotepadApp* app, const gchar* image_path, gdouble opacity);

// 关于对话框
extern void on_about(GtkWidget* widget, gpointer data);

#endif // UI_H
