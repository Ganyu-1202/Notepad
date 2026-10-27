#include "font_picker.h"

/* The combo is a non-visible selection model; only explicit row activation commits. */
static gboolean filter_font(GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
    GtkWidget* search = data;
    const gchar* query = gtk_entry_get_text(GTK_ENTRY(search));
    if (!*query)
    {
        return TRUE;
    }
    gchar* name = NULL;
    gtk_tree_model_get(model, iter, 0, &name, -1);
    gchar* folded_name = g_utf8_casefold(name ? name : "", -1);
    gchar* folded_query = g_utf8_casefold(query, -1);
    gboolean visible = g_strstr_len(folded_name, -1, folded_query) != NULL;
    g_free(name);
    g_free(folded_name);
    g_free(folded_query);
    return visible;
}

static void search_changed(GtkEditable* entry, gpointer data)
{
    GtkWidget* picker = data;
    GtkTreeModelFilter* filter = g_object_get_data(G_OBJECT(picker), "filter");
    gtk_tree_model_filter_refilter(filter);
    GtkWidget* empty = g_object_get_data(G_OBJECT(picker), "empty");
    gtk_widget_set_visible(empty, gtk_tree_model_iter_n_children(GTK_TREE_MODEL(filter), NULL) == 0);
    GtkWidget* tree = g_object_get_data(G_OBJECT(picker), "tree");
    GtkWidget* scroll = gtk_widget_get_ancestor(tree, GTK_TYPE_SCROLLED_WINDOW);
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll)), 0);
    (void)entry;
}

static void mark_selected(GtkTreeViewColumn* column, GtkCellRenderer* renderer,
                          GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
    GtkWidget* combo = data;
    GtkTreeIter child;
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child, iter);
    GtkTreePath* path = gtk_tree_model_get_path(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)), &child);
    gint* indices = gtk_tree_path_get_indices(path);
    gboolean selected = indices && indices[0] == gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    g_object_set(renderer, "text", selected ? "✓" : "", NULL);
    gtk_tree_path_free(path);
    (void)column;
}

static void row_activated(GtkTreeView* tree, GtkTreePath* path, GtkTreeViewColumn* column, gpointer data)
{
    GtkWidget* picker = data;
    GtkTreeModelFilter* filter = GTK_TREE_MODEL_FILTER(gtk_tree_view_get_model(tree));
    GtkTreePath* child = gtk_tree_model_filter_convert_path_to_child_path(filter, path);
    if (child)
    {
        GtkWidget* combo = g_object_get_data(G_OBJECT(picker), "combo");
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), gtk_tree_path_get_indices(child)[0]);
        gtk_tree_path_free(child);
        gtk_popover_popdown(GTK_POPOVER(g_object_get_data(G_OBJECT(picker), "popover")));
    }
    (void)column;
}

static void selection_changed(GtkComboBox* combo, gpointer data)
{
    GtkWidget* picker = data;
    gchar* text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(picker), "label")), text ? text : "选择字体");
    gtk_widget_set_tooltip_text(picker, text);
    gtk_widget_queue_draw(g_object_get_data(G_OBJECT(picker), "tree"));
    g_free(text);
}

static void popup_shown(GtkWidget* popover, gpointer data)
{
    GtkWidget* picker = data;
    GtkWidget* search = g_object_get_data(G_OBJECT(picker), "search");
    gtk_entry_set_text(GTK_ENTRY(search), "");
    gtk_widget_grab_focus(search);
    GtkWidget* combo = g_object_get_data(G_OBJECT(picker), "combo");
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (index >= 0)
    {
        GtkTreePath* child = gtk_tree_path_new_from_indices(index, -1);
        GtkTreeModelFilter* filter = g_object_get_data(G_OBJECT(picker), "filter");
        GtkTreePath* path = gtk_tree_model_filter_convert_child_path_to_path(filter, child);
        if (path)
        {
            GtkTreeView* tree = g_object_get_data(G_OBJECT(picker), "tree");
            gtk_tree_view_set_cursor(tree, path, NULL, FALSE);
            gtk_tree_view_scroll_to_cell(tree, path, NULL, TRUE, 0.5, 0);
            gtk_tree_path_free(path);
        }
        gtk_tree_path_free(child);
    }
    (void)popover;
}

static gboolean search_key(GtkWidget* search, GdkEventKey* event, gpointer data)
{
    GtkWidget* picker = data;
    GtkTreeView* tree = g_object_get_data(G_OBJECT(picker), "tree");
    if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        GtkTreeModel* model = gtk_tree_view_get_model(tree);
        if (gtk_tree_model_iter_n_children(model, NULL) > 0)
        {
            GtkTreePath* path = gtk_tree_path_new_first();
            gtk_tree_view_set_cursor(tree, path, NULL, FALSE);
            if (event->keyval == GDK_KEY_Down)
            {
                gtk_widget_grab_focus(GTK_WIDGET(tree));
            }
            else
            {
                row_activated(tree, path, NULL, picker);
            }
            gtk_tree_path_free(path);
        }
        return TRUE;
    }
    (void)search;
    return FALSE;
}

GtkWidget* create_searchable_font_picker(GtkWidget* combo)
{
    GtkWidget* picker = gtk_menu_button_new();
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(header), label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(picker), header);
    GtkWidget* popover = gtk_popover_new(picker);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(picker), popover);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_container_add(GTK_CONTAINER(popover), box);
    GtkWidget* search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search), "搜索字体…");
    gtk_box_pack_start(GTK_BOX(box), search, FALSE, FALSE, 0);
    GtkTreeModel* filter = gtk_tree_model_filter_new(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter), filter_font, search, NULL);
    GtkWidget* tree = gtk_tree_view_new_with_model(filter);
    g_object_unref(filter);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);
    GtkCellRenderer* text = gtk_cell_renderer_text_new();
    g_object_set(text, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    PangoLayout* sample = gtk_widget_create_pango_layout(picker, "Ag国");
    gint height;
    pango_layout_get_pixel_size(sample, NULL, &height);
    g_object_unref(sample);
    gtk_cell_renderer_set_fixed_size(text, 240, MAX(28, height + 10));
    GtkTreeViewColumn* name = gtk_tree_view_column_new_with_attributes("字体", text, "text", 0, NULL);
    gtk_tree_view_column_set_sizing(name, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(name, 240);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), name);
    GtkCellRenderer* check = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* selected = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(selected, check, FALSE);
    gtk_tree_view_column_set_cell_data_func(selected, check, mark_selected, combo, NULL);
    gtk_tree_view_column_set_sizing(selected, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(selected, 28);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), selected);
    gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(tree), TRUE);
    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_widget_set_size_request(scroll, 300, 280);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 280);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 280);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
    GtkWidget* empty = gtk_label_new("没有匹配的字体");
    gtk_widget_set_no_show_all(empty, TRUE);
    gtk_box_pack_start(GTK_BOX(box), empty, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(picker), "combo", combo);
    g_object_set_data(G_OBJECT(picker), "popover", popover);
    g_object_set_data(G_OBJECT(picker), "search", search);
    g_object_set_data(G_OBJECT(picker), "filter", filter);
    g_object_set_data(G_OBJECT(picker), "tree", tree);
    g_object_set_data(G_OBJECT(picker), "label", label);
    g_object_set_data(G_OBJECT(picker), "empty", empty);
    g_signal_connect_object(combo, "changed", G_CALLBACK(selection_changed), picker, 0);
    g_signal_connect(search, "changed", G_CALLBACK(search_changed), picker);
    g_signal_connect(search, "key-press-event", G_CALLBACK(search_key), picker);
    g_signal_connect(tree, "row-activated", G_CALLBACK(row_activated), picker);
    g_signal_connect(popover, "show", G_CALLBACK(popup_shown), picker);
    selection_changed(GTK_COMBO_BOX(combo), picker);
    gtk_widget_show_all(box);
    return picker;
}
