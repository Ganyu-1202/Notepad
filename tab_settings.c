#include "notepad.h"

static void on_spaces_mode_toggled(GtkToggleButton* button, gpointer data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_toggle_button_get_active(button));
}

void on_tab_settings(GtkWidget* widget, gpointer data)
{
    (void)widget;
    NotepadApp* app = data;
    GtkWidget* dialog = gtk_dialog_new_with_buttons("制表符设置", GTK_WINDOW(app->ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "取消", GTK_RESPONSE_CANCEL, "确定", GTK_RESPONSE_OK, NULL);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), box);
    GtkWidget* tab = gtk_radio_button_new_with_label(NULL, "按 Tab 插入制表符（\\t）");
    GtkWidget* spaces = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tab),
        "按 Tab 插入固定数量的空格");
    gtk_box_pack_start(GTK_BOX(box), tab, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), spaces, FALSE, FALSE, 0);
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* count = gtk_spin_button_new_with_range(1, 32, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(count), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count), app->ui->tab_space_count);
    gtk_box_pack_start(GTK_BOX(row), gtk_label_new("空格数量："), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), count, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(count, "每次按 Tab 插入 1–32 个空格，不改变文档中已有的制表符");
    g_signal_connect(spaces, "toggled", G_CALLBACK(on_spaces_mode_toggled), row);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->ui->tab_inserts_spaces ? spaces : tab), TRUE);
    on_spaces_mode_toggled(GTK_TOGGLE_BUTTON(spaces), row);
    g_object_set_data(G_OBJECT(dialog), "tab-spaces", spaces);
    g_object_set_data(G_OBJECT(dialog), "tab-count", count);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        gtk_spin_button_update(GTK_SPIN_BUTTON(count));
        app->ui->tab_inserts_spaces = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spaces));
        app->ui->tab_space_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count));
    }
    gtk_widget_destroy(dialog);
}

gboolean on_editor_tab_key(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
    NotepadApp* app = data;
    GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
    if (event->keyval != GDK_KEY_Tab || (event->state & modifiers) != 0 ||
        !gtk_text_view_get_editable(GTK_TEXT_VIEW(widget)))
    {
        return FALSE;
    }
    if (!app->ui->tab_inserts_spaces)
    {
        /* Keep GTK's native tab insertion, selection replacement and IM handling. */
        return FALSE;
    }
    int count = CLAMP(app->ui->tab_space_count, 1, 32);
    gchar spaces[33];
    for (int i = 0; i < count; ++i)
    {
        spaces[i] = ' ';
    }
    spaces[count] = '\0';
    gtk_text_buffer_begin_user_action(app->ui->buffer);
    gtk_text_buffer_delete_selection(app->ui->buffer, TRUE, TRUE);
    gtk_text_buffer_insert_interactive_at_cursor(app->ui->buffer, spaces, count, TRUE);
    gtk_text_buffer_end_user_action(app->ui->buffer);
    return TRUE;
}
