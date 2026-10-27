#include "notepad.h"
#include "file_operations.h"
#include <string.h>
#include <glib/gstdio.h>

static void flush_styles(void)
{
    for (int i = 0; i < 20; ++i)
    {
        while (gtk_events_pending())
        {
            gtk_main_iteration();
        }
        g_usleep(10000);
    }
}

static gboolean change_dialog_size(gpointer data)
{
    (void)data;
    GList* windows = gtk_window_list_toplevels();
    gboolean changed = FALSE;
    for (GList* item = windows; item; item = item->next)
    {
        if (GTK_IS_DIALOG(item->data))
        {
            GtkWidget* spin = g_object_get_data(G_OBJECT(item->data), "font-size");
            if (spin)
            {
                GtkWidget* family = g_object_get_data(G_OBJECT(item->data), "font-family");
                GtkWidget* face = g_object_get_data(G_OBJECT(item->data), "font-face");
                GtkWidget* fallback = g_object_get_data(G_OBJECT(item->data), "font-fallback");
                g_assert_nonnull(fallback);
                g_assert_true(gtk_widget_get_visible(fallback));
                g_assert_nonnull(family);
                g_assert_nonnull(face);
                GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(family));
                GtkTreeIter iter;
                gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
                while (valid)
                {
                    gchar* name = NULL;
                    gtk_tree_model_get(model, &iter, 0, &name, -1);
                    g_assert_cmpint(g_ascii_strcasecmp(name, "monospace"), !=, 0);
                    g_assert_cmpint(g_ascii_strcasecmp(name, "Sans"), !=, 0);
                    g_free(name);
                    valid = gtk_tree_model_iter_next(model, &iter);
                }
                gboolean list_mode = FALSE;
                gtk_widget_style_get(family, "appears-as-list", &list_mode, NULL);
                g_assert_true(list_mode);
                AtkObject* accessible = gtk_combo_box_get_popup_accessible(GTK_COMBO_BOX(family));
                GtkWidget* tree = gtk_accessible_get_widget(GTK_ACCESSIBLE(accessible));
                g_assert_true(GTK_IS_TREE_VIEW(tree));
                g_assert_true(gtk_tree_view_get_fixed_height_mode(GTK_TREE_VIEW(tree)));
                GtkWidget* scroll = gtk_widget_get_ancestor(tree, GTK_TYPE_SCROLLED_WINDOW);
                g_assert_nonnull(scroll);
                GtkPolicyType vertical;
                gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(scroll), NULL, &vertical);
                g_assert_cmpint(vertical, ==, GTK_POLICY_ALWAYS);
                gint64 popup_started = g_get_monotonic_time();
                gtk_combo_box_popup(GTK_COMBO_BOX(family));
                flush_styles();
                GtkWidget* bar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scroll));
                GtkWidget* popup_window = gtk_widget_get_toplevel(tree);
                gint bar_x, bar_y, root_x, root_y;
                gtk_widget_translate_coordinates(bar, popup_window, 0, 0, &bar_x, &bar_y);
                gdk_window_get_origin(gtk_widget_get_window(popup_window), &root_x, &root_y);
                GdkEvent* drag = gdk_event_new(GDK_BUTTON_PRESS);
                drag->button.window = g_object_ref(gtk_widget_get_window(popup_window));
                drag->button.button = GDK_BUTTON_PRIMARY;
                drag->button.x_root = root_x + bar_x + gtk_widget_get_allocated_width(bar) / 2;
                drag->button.y_root = root_y + bar_y + 20;
                gdk_event_set_device(drag, gdk_seat_get_pointer(gdk_display_get_default_seat(
                    gtk_widget_get_display(bar))));
                gtk_widget_event(popup_window, drag);
                GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
                gdouble target = (gtk_adjustment_get_upper(adjustment) -
                    gtk_adjustment_get_page_size(adjustment)) / 2.0;
                gtk_adjustment_set_value(adjustment, target);
                g_assert_cmpfloat(gtk_adjustment_get_value(adjustment), >, 0.0);
                drag->type = GDK_BUTTON_RELEASE;
                drag->button.x_root -= 30;
                drag->button.y_root += 100;
                gtk_widget_event(popup_window, drag);
                gdk_event_free(drag);
                gboolean still_open = FALSE;
                g_object_get(family, "popup-shown", &still_open, NULL);
                g_assert_true(still_open);
                gtk_combo_box_popdown(GTK_COMBO_BOX(family));
                gtk_combo_box_popup(GTK_COMBO_BOX(fallback));
                flush_styles();
                gtk_combo_box_popdown(GTK_COMBO_BOX(fallback));
                if (g_getenv("NOTEPAD_FONT_TIMING"))
                {
                    g_print("font scrollable popups including event flush: %.2f ms\n",
                        (g_get_monotonic_time() - popup_started) / 1000.0);
                }
                gint original_face = gtk_combo_box_get_active(GTK_COMBO_BOX(face));
                gint faces = gtk_tree_model_iter_n_children(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(face)), NULL);
                for (gint i = 0; i < faces; ++i)
                {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(face), i);
                }
                gtk_combo_box_set_active(GTK_COMBO_BOX(face), original_face);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 28);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 29);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 30);
                flush_styles();
                gtk_dialog_response(GTK_DIALOG(item->data), GTK_RESPONSE_OK);
                changed = TRUE;
                break;
            }
        }
    }
    g_list_free(windows);
    return changed ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

static gboolean edit_background_transparency(gpointer data)
{
    GList* windows = gtk_window_list_toplevels();
    for (GList* item = windows; item; item = item->next)
    {
        GtkWidget* spin = g_object_get_data(G_OBJECT(item->data), "background-transparency");
        if (!spin)
        {
            continue;
        }
        GtkWidget* scale = g_object_get_data(G_OBJECT(item->data), "background-transparency-scale");
        GtkWidget* theme = g_object_get_data(G_OBJECT(item->data), "background-default");
        GtkWidget* color = g_object_get_data(G_OBJECT(item->data), "background-color");
        GtkWidget* image = g_object_get_data(G_OBJECT(item->data), "background-image");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(theme), TRUE);
        g_assert_false(gtk_widget_is_sensitive(spin));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(image), TRUE);
        g_assert_true(gtk_widget_is_sensitive(spin));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(color), TRUE);
        g_assert_true(gtk_widget_is_sensitive(spin));
        gtk_range_set_value(GTK_RANGE(scale), 25);
        g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin)), ==, 25);
        gtk_entry_set_text(GTK_ENTRY(spin), "50");
        gtk_spin_button_update(GTK_SPIN_BUTTON(spin));
        g_assert_cmpfloat(gtk_range_get_value(GTK_RANGE(scale)), ==, 50.0);
        GdkRGBA red;
        gdk_rgba_parse(&red, "#ff0000");
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(g_object_get_data(
            G_OBJECT(item->data), "background-color-button")), &red);
        gtk_dialog_response(GTK_DIALOG(item->data), GPOINTER_TO_INT(data));
        g_list_free(windows);
        return G_SOURCE_REMOVE;
    }
    g_list_free(windows);
    return G_SOURCE_CONTINUE;
}

static void test_document_features(NotepadApp* app)
{
    GtkWidget* toggle = gtk_check_menu_item_new();
    g_object_ref_sink(toggle);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle), TRUE);
    on_line_numbers_toggled(GTK_CHECK_MENU_ITEM(toggle), app);
    on_line_frame_toggled(GTK_CHECK_MENU_ITEM(toggle), app);
    g_assert_true(gtk_widget_get_visible(app->ui->line_numbers));
    apply_font_with_fallback(app, "Arial 12", "SimSun 12");
    flush_styles();
    gint narrow, wide;
    gtk_widget_get_size_request(app->ui->line_numbers, &narrow, NULL);
    apply_font_with_fallback(app, "Arial Bold 72", "SimSun 12");
    flush_styles();
    gtk_widget_get_size_request(app->ui->line_numbers, &wide, NULL);
    g_assert_cmpint(wide, >, narrow);
    apply_font_with_fallback(app, app->ui->primary_font, app->ui->fallback_font);
    gtk_text_buffer_set_text(app->ui->buffer, "first\nsecond\nthird", -1);
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_line(app->ui->buffer, &start, 0);
    gtk_text_buffer_get_iter_at_line(app->ui->buffer, &end, 2);
    gtk_text_buffer_select_range(app->ui->buffer, &start, &end);
    int first, last;
    g_assert_true(get_framed_line_range(app, &first, &last));
    g_assert_cmpint(first, ==, 0);
    g_assert_cmpint(last, ==, 1);
    gtk_text_buffer_place_cursor(app->ui->buffer, &end);
    get_framed_line_range(app, &first, &last);
    g_assert_cmpint(first, ==, 2);
    g_assert_cmpint(last, ==, 2);
    flush_styles();
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1080, 720);
    cairo_t* cr = cairo_create(surface);
    gtk_widget_draw(app->ui->window, cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle), FALSE);
    on_line_numbers_toggled(GTK_CHECK_MENU_ITEM(toggle), app);
    on_line_frame_toggled(GTK_CHECK_MENU_ITEM(toggle), app);
    g_assert_false(gtk_widget_get_visible(app->ui->line_numbers));
    g_object_unref(toggle);

    gchar* path = NULL;
    int fd = g_file_open_tmp("notepad-encoding-XXXXXX", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    g_close(fd, NULL);
    const char* sample = "中文 Test 😀\n第二行";
    for (int encoding = ENCODING_UTF8; encoding <= ENCODING_GB18030; ++encoding)
    {
        gtk_text_buffer_set_text(app->ui->buffer, sample, -1);
        app->encoding = (TextEncoding)encoding;
        GError* error = NULL;
        g_assert_true(save_document(app, path, &error));
        g_assert_no_error(error);
        gchar* before;
        gsize before_length;
        g_assert_true(g_file_get_contents(path, &before, &before_length, &error));
        g_assert_true(load_document(app, path, &error));
        g_assert_no_error(error);
        g_assert_cmpint(app->encoding, ==, encoding);
        gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
        gchar* decoded = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);
        g_assert_cmpstr(decoded, ==, sample);
        g_free(decoded);
        g_assert_true(save_document(app, path, &error));
        gchar* after;
        gsize after_length;
        g_assert_true(g_file_get_contents(path, &after, &after_length, &error));
        g_assert_cmpmem(before, before_length, after, after_length);
        g_free(before);
        g_free(after);
    }
    const char invalid[] = { (char)0xff, (char)0xfe, (char)0x41 };
    g_assert_true(g_file_set_contents(path, invalid, sizeof(invalid), NULL));
    GError* error = NULL;
    g_assert_false(load_document(app, path, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gchar* unchanged = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);
    g_assert_cmpstr(unchanged, ==, sample);
    g_free(unchanged);
    notepad_set_modified(app, false);
    on_new_file(NULL, app);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(app->ui->encoding_label)), ==, "UTF-8");
    g_assert_false(app->is_modified);
    g_remove(path);
    g_free(path);
}

int main(int argc, char** argv)
{
    gtk_init(&argc, &argv);
    NotepadApp* app = notepad_app_new();
    setup_main_window(app);
    /* Keep the test window off the user's workspace. */
    gtk_window_move(GTK_WINDOW(app->ui->window), -10000, -10000);
    gtk_widget_show_all(app->ui->window);
    apply_font_with_fallback(app, "Arial 12", "SimSun 12");
    flush_styles();
    PangoFontDescription* small = NULL;
    gtk_style_context_get(gtk_widget_get_style_context(app->ui->text_view),
        GTK_STATE_FLAG_NORMAL, "font", &small, NULL);
    apply_font_with_fallback(app, "Arial 24", "SimSun 12");
    flush_styles();
    PangoFontDescription* large = NULL;
    gtk_style_context_get(gtk_widget_get_style_context(app->ui->text_view),
        GTK_STATE_FLAG_NORMAL, "font", &large, NULL);
    g_assert_cmpstr(pango_font_description_get_family(small), ==,
        pango_font_description_get_family(large));
    g_assert_cmpint(pango_font_description_get_size(large), ==,
        2 * pango_font_description_get_size(small));
    pango_font_description_free(small);
    pango_font_description_free(large);

    PangoFontDescription* original_fallback = pango_font_description_from_string(app->ui->fallback_font);
    PangoFontDescription* original = pango_font_description_from_string(app->ui->primary_font);
    g_timeout_add(50, change_dialog_size, NULL);
    on_font_selection(NULL, app);
    PangoFontDescription* selected = pango_font_description_from_string(app->ui->primary_font);
    g_assert_cmpstr(pango_font_description_get_family(original), ==,
        pango_font_description_get_family(selected));
    g_assert_cmpint(pango_font_description_get_size(selected), ==, 30 * PANGO_SCALE);
    PangoFontDescription* selected_fallback = pango_font_description_from_string(app->ui->fallback_font);
    g_assert_cmpstr(pango_font_description_get_family(original_fallback), ==,
        pango_font_description_get_family(selected_fallback));
    pango_font_description_free(original_fallback);
    pango_font_description_free(selected_fallback);
    pango_font_description_free(original);
    pango_font_description_free(selected);

    GtkWidget* dark = gtk_check_menu_item_new();
    g_object_ref_sink(dark);
    g_object_set_data(G_OBJECT(dark), "dark-mode", GINT_TO_POINTER(TRUE));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(dark), TRUE);
    on_theme_toggled(GTK_CHECK_MENU_ITEM(dark), app);
    g_assert_true(app->ui->dark_mode);
    gboolean prefer_dark = FALSE;
    g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    g_assert_true(prefer_dark);
    g_object_set_data(G_OBJECT(dark), "dark-mode", GINT_TO_POINTER(FALSE));
    on_theme_toggled(GTK_CHECK_MENU_ITEM(dark), app);
    g_assert_false(app->ui->dark_mode);
    g_object_unref(dark);

    GdkRGBA color;
    gdk_rgba_parse(&color, "#123456");
    apply_background_color(app, &color);
    g_assert_true(app->ui->custom_background);
    g_assert_true(gdk_rgba_equal(&app->ui->background_color, &color));

    g_timeout_add(50, edit_background_transparency, GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    on_background_settings(NULL, app);
    g_assert_true(gdk_rgba_equal(&app->ui->background_color, &color));
    g_timeout_add(50, edit_background_transparency, GINT_TO_POINTER(GTK_RESPONSE_OK));
    on_background_settings(NULL, app);
    g_assert_cmpfloat(app->ui->background_opacity, ==, 0.5);
    flush_styles();
    cairo_surface_t* solid_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 10, 10);
    cairo_t* solid_cr = cairo_create(solid_surface);
    draw_editor_background(app->ui->background_area, solid_cr, app);
    cairo_surface_flush(solid_surface);
    unsigned char* solid_pixels = cairo_image_surface_get_data(solid_surface);
    g_assert_cmpint(solid_pixels[2], ==, 255);
    g_assert_cmpint(solid_pixels[1], >=, 126);
    g_assert_cmpint(solid_pixels[1], <=, 129);
    g_assert_cmpint(solid_pixels[3], ==, 255);
    cairo_destroy(solid_cr);
    cairo_surface_destroy(solid_surface);

    gchar* path = NULL;
    int fd = g_file_open_tmp("notepad-background-XXXXXX.png", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    g_close(fd, NULL);
    GdkPixbuf* image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 4, 4);
    gdk_pixbuf_fill(image, 0xff0000ff);
    g_assert_true(gdk_pixbuf_save(image, path, "png", NULL, NULL));
    g_object_unref(image);
    apply_background_image(app, path, 0.5);
    g_assert_nonnull(app->ui->background_image);
    g_assert_cmpstr(app->ui->background_image_path, ==, path);
    flush_styles();
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 10, 10);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    draw_editor_background(app->ui->background_area, cr, app);
    cairo_surface_flush(surface);
    unsigned char* pixels = cairo_image_surface_get_data(surface);
    /* Half-opacity red over white: opaque red, half green and blue. */
    g_assert_cmpint(pixels[2], ==, 255);
    g_assert_cmpint(pixels[1], >=, 126);
    g_assert_cmpint(pixels[1], <=, 129);
    g_assert_cmpint(pixels[3], ==, 255);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    GtkWidget* overlay = gtk_widget_get_parent(app->ui->background_area);
    int width = gtk_widget_get_allocated_width(overlay);
    int height = gtk_widget_get_allocated_height(overlay);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(surface);
    gtk_widget_draw(overlay, cr);
    cairo_surface_flush(surface);
    pixels = cairo_image_surface_get_data(surface) + 100 * cairo_image_surface_get_stride(surface) + 100 * 4;
    g_assert_cmpint(pixels[2], ==, 255);
    g_assert_cmpint(pixels[1], >=, 126);
    g_assert_cmpint(pixels[1], <=, 129);
    g_assert_cmpfloat(gtk_widget_get_opacity(app->ui->text_view), ==, 1.0);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    apply_background_color(app, &color);
    g_assert_null(app->ui->background_image);
    g_assert_null(app->ui->background_image_path);
    reset_background(app);
    g_assert_false(app->ui->custom_background);
    g_remove(path);
    g_free(path);
    test_document_features(app);
    /* Do not enter gtk_main; disconnect the application's quit callback. */
    g_signal_handlers_disconnect_by_func(app->ui->window, on_quit, NULL);
    gtk_widget_destroy(app->ui->window);
    notepad_app_free(app);
    g_print("Font size, themes, background color/image/opacity and reset: PASS\n");
    return 0;
}
