#include "core/notepad.h"
#include "ui/font_picker.h"
#include <string.h>

typedef struct
{
    GtkWidget* family;
    GtkWidget* face;
    GtkWidget* fallback;
    GtkWidget* size;
    GtkWidget* preview;
    GPtrArray* families;
    GPtrArray* faces;
    bool updating;
    guint preview_source;
} FontDialog;

static void trace_font_time(const char* phase, gint64 start)
{
    if (g_getenv("NOTEPAD_FONT_TIMING"))
    {
        g_print("font %s: %.2f ms\n", phase, (g_get_monotonic_time() - start) / 1000.0);
    }
}

static bool is_virtual_family(const char* name)
{
    return g_ascii_strcasecmp(name, "Sans") == 0 ||
        g_ascii_strcasecmp(name, "Serif") == 0 ||
        g_ascii_strcasecmp(name, "Monospace") == 0 ||
        g_ascii_strcasecmp(name, "System-ui") == 0;
}

static gint compare_families(gconstpointer left, gconstpointer right)
{
    return g_utf8_collate(pango_font_family_get_name(*(PangoFontFamily* const*)left),
        pango_font_family_get_name(*(PangoFontFamily* const*)right));
}

static PangoFontDescription* selected_font(FontDialog* ui)
{
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(ui->face));
    PangoFontDescription* desc;
    if (index >= 0 && (guint)index < ui->faces->len)
    {
        desc = pango_font_face_describe(g_ptr_array_index(ui->faces, index));
    }
    else
    {
        desc = pango_font_description_new();
        gchar* family = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ui->family));
        pango_font_description_set_family(desc, family ? family : "Microsoft YaHei");
        g_free(family);
    }
    pango_font_description_set_size(desc,
        (gint)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ui->size)) * PANGO_SCALE));
    return desc;
}

static void render_preview(FontDialog* ui)
{
    if (ui->updating)
    {
        return;
    }
    gint64 started = g_get_monotonic_time();
    PangoFontDescription* desc = selected_font(ui);
    gchar* primary = pango_font_description_to_string(desc);
    gchar* fallback = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ui->fallback));
    /* Use the same renderer as the editor without changing the document. */
    NotepadUI preview_ui = {0};
    NotepadApp preview_app = {0};
    preview_ui.text_view = ui->preview;
    preview_app.ui = &preview_ui;
    apply_font_with_fallback(&preview_app, primary, fallback ? fallback : "Microsoft YaHei");
    pango_font_description_free(desc);
    g_free(primary);
    g_free(fallback);
    trace_font_time("preview", started);
}

static gboolean render_pending_preview(gpointer data)
{
    FontDialog* ui = data;
    ui->preview_source = 0;
    render_preview(ui);
    return G_SOURCE_REMOVE;
}

static void update_preview(GtkWidget* widget, gpointer data)
{
    (void)widget;
    FontDialog* ui = data;
    if (ui->updating)
    {
        return;
    }
    if (ui->preview_source)
    {
        g_source_remove(ui->preview_source);
    }
    ui->preview_source = g_timeout_add(100, render_pending_preview, ui);
}

static void populate_faces(FontDialog* ui, const PangoFontDescription* preferred)
{
    ui->updating = true;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(ui->face));
    g_ptr_array_set_size(ui->faces, 0);
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(ui->family));
    gint selected = 0;
    if (index >= 0 && (guint)index < ui->families->len)
    {
        PangoFontFace** faces;
        gint count;
        pango_font_family_list_faces(g_ptr_array_index(ui->families, index), &faces, &count);
        for (gint i = 0; i < count; ++i)
        {
            /* Synthetic condensed/expanded faces can be advertised but unloadable on Windows. */
            if (pango_font_face_is_synthesized(faces[i]))
            {
                continue;
            }
            PangoFontDescription* desc = pango_font_face_describe(faces[i]);
            if (preferred && pango_font_description_get_weight(desc) == pango_font_description_get_weight(preferred) &&
                pango_font_description_get_style(desc) == pango_font_description_get_style(preferred) &&
                pango_font_description_get_stretch(desc) == pango_font_description_get_stretch(preferred))
            {
                selected = (gint)ui->faces->len;
            }
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->face), pango_font_face_get_face_name(faces[i]));
            g_ptr_array_add(ui->faces, g_object_ref(faces[i]));
            pango_font_description_free(desc);
        }
        g_free(faces);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->face), ui->faces->len ? selected : -1);
    gtk_widget_set_sensitive(ui->face, ui->faces->len > 0);
    ui->updating = false;
}

static void family_changed(GtkWidget* widget, gpointer data)
{
    (void)widget;
    FontDialog* ui = data;
    populate_faces(ui, NULL);
    update_preview(NULL, ui);
}

static GtkWidget* create_font_combo(void)
{
    GtkWidget* combo = gtk_combo_box_text_new();
    /* List mode uses a scrolled tree instead of creating a menu item per font. */
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "combobox { -GtkComboBox-appears-as-list: true; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(combo),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    /* Fixed cells avoid shaping every installed font name just to find a width. */
    PangoLayout* sample = gtk_widget_create_pango_layout(combo, "Ag国");
    gint height;
    pango_layout_get_pixel_size(sample, NULL, &height);
    g_object_unref(sample);
    GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));
    for (GList* item = cells; item; item = item->next)
    {
        gtk_cell_renderer_set_fixed_size(GTK_CELL_RENDERER(item->data), 260, MAX(24, height + 8));
        g_object_set(item->data, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    }
    g_list_free(cells);
    gtk_widget_set_size_request(combo, 280, -1);
    gtk_combo_box_set_popup_fixed_width(GTK_COMBO_BOX(combo), TRUE);
    return combo;
}

static void attach_field(GtkGrid* grid, const gchar* title, GtkWidget* field, gint row)
{
    GtkWidget* label = gtk_label_new_with_mnemonic(title);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), field);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(field, TRUE);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, field, 1, row, 1, 1);
}

void on_font_selection(GtkWidget* widget, gpointer data)
{
    (void)widget;
    NotepadApp* app = data;
    gint64 started = g_get_monotonic_time();
    FontDialog ui = {0};
    GtkWidget* dialog = gtk_dialog_new_with_buttons("字体设置", GTK_WINDOW(app->ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "取消", GTK_RESPONSE_CANCEL, "确定", GTK_RESPONSE_OK, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 560);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), box);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    ui.family = create_font_combo();
    ui.face = create_font_combo();
    ui.fallback = create_font_combo();
    ui.size = gtk_spin_button_new_with_range(6, 144, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(ui.size), TRUE);
    g_object_ref_sink(ui.family);
    g_object_ref_sink(ui.fallback);
    GtkWidget* family_picker = create_searchable_font_picker(ui.family);
    GtkWidget* fallback_picker = create_searchable_font_picker(ui.fallback);
    attach_field(GTK_GRID(grid), "字体(_F)", family_picker, 0);
    attach_field(GTK_GRID(grid), "字形(_S)", ui.face, 1);
    attach_field(GTK_GRID(grid), "字号（磅）(_Z)", ui.size, 2);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
    attach_field(GTK_GRID(grid), "备用字体(_B)", fallback_picker, 3);
    gtk_widget_set_tooltip_text(ui.fallback, "仅在主字体缺少字符时使用，字号与主字体相同");
    GtkWidget* fallback_hint = gtk_label_new("备用字体仅在主字体缺少字符时使用。");
    gtk_widget_set_halign(fallback_hint, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), fallback_hint, 1, 4, 1, 1);
    ui.preview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ui.preview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(ui.preview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ui.preview), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(ui.preview), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(ui.preview), 12);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.preview)),
        "中文字体预览 · Font Preview\n春风又绿江南岸\nAaBbCc 0123456789 !?", -1);
    GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll, -1, 180);
    gtk_container_add(GTK_CONTAINER(scroll), ui.preview);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
    PangoFontFamily** families;
    gint count;
    pango_context_list_families(gtk_widget_get_pango_context(app->ui->text_view), &families, &count);
    ui.families = g_ptr_array_new_with_free_func(g_object_unref);
    ui.faces = g_ptr_array_new_with_free_func(g_object_unref);
    for (gint i = 0; i < count; ++i)
    {
        if (!is_virtual_family(pango_font_family_get_name(families[i])))
        {
            g_ptr_array_add(ui.families, g_object_ref(families[i]));
        }
    }
    g_free(families);
    g_ptr_array_sort(ui.families, compare_families);
    trace_font_time("enumerate", started);
    PangoFontDescription* current = pango_font_description_from_string(app->ui->primary_font);
    PangoFontDescription* fallback = pango_font_description_from_string(app->ui->fallback_font);
    gint primary_index = 0, fallback_index = 0;
    for (guint i = 0; i < ui.families->len; ++i)
    {
        const gchar* name = pango_font_family_get_name(g_ptr_array_index(ui.families, i));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui.family), name);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui.fallback), name);
        if (g_strcmp0(name, pango_font_description_get_family(current)) == 0)
        {
            primary_index = (gint)i;
        }
        if (g_strcmp0(name, pango_font_description_get_family(fallback)) == 0)
        {
            fallback_index = (gint)i;
        }
    }
    trace_font_time("populate", started);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui.family), primary_index);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui.fallback), fallback_index);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui.size),
        (gdouble)pango_font_description_get_size(current) / PANGO_SCALE);
    populate_faces(&ui, current);
    g_signal_connect(ui.family, "changed", G_CALLBACK(family_changed), &ui);
    g_signal_connect(ui.face, "changed", G_CALLBACK(update_preview), &ui);
    g_signal_connect(ui.fallback, "changed", G_CALLBACK(update_preview), &ui);
    g_signal_connect(ui.size, "value-changed", G_CALLBACK(update_preview), &ui);
    g_object_set_data(G_OBJECT(dialog), "font-size", ui.size);
    g_object_set_data(G_OBJECT(dialog), "font-family", ui.family);
    g_object_set_data(G_OBJECT(dialog), "font-face", ui.face);
    g_object_set_data(G_OBJECT(dialog), "font-fallback", fallback_picker);
    g_object_set_data(G_OBJECT(dialog), "font-family-picker", family_picker);
    render_preview(&ui);
    trace_font_time("before-show", started);
    gtk_widget_show_all(dialog);

    trace_font_time("show", started);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        gtk_spin_button_update(GTK_SPIN_BUTTON(ui.size));
        PangoFontDescription* chosen = selected_font(&ui);
        gchar* fallback_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ui.fallback));
        pango_font_description_set_family(fallback, fallback_name ? fallback_name : "Microsoft YaHei");
        pango_font_description_set_size(fallback, pango_font_description_get_size(chosen));
        g_free(app->ui->primary_font);
        g_free(app->ui->fallback_font);
        app->ui->primary_font = pango_font_description_to_string(chosen);
        app->ui->fallback_font = pango_font_description_to_string(fallback);
        apply_font_with_fallback(app, app->ui->primary_font, app->ui->fallback_font);
        pango_font_description_free(chosen);
        g_free(fallback_name);
    }
    if (ui.preview_source)
    {
        g_source_remove(ui.preview_source);
    }
    gtk_widget_destroy(dialog);
    pango_font_description_free(current);
    pango_font_description_free(fallback);
    g_object_unref(ui.family);
    g_object_unref(ui.fallback);
    g_ptr_array_unref(ui.faces);
    g_ptr_array_unref(ui.families);
}
