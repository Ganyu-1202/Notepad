#include "core/notepad.h"
#include "common/output_exception.h"

void apply_appearance(NotepadApp* app)
{
    NotepadUI* ui = app->ui;
    const gchar* base = ui->dark_mode ? "#202124" : "#ffffff";
    const gchar* foreground = ui->dark_mode ? "#eeeeee" : "#202124";
    GdkRGBA color;
    gdk_rgba_parse(&color, base);
    if (ui->custom_background && !ui->background_image)
    {
        gdouble opacity = ui->background_opacity;
        color.red = ui->background_color.red * opacity + color.red * (1.0 - opacity);
        color.green = ui->background_color.green * opacity + color.green * (1.0 - opacity);
        color.blue = ui->background_color.blue * opacity + color.blue * (1.0 - opacity);
        /* Match foreground to the selected solid background. */
        gdouble brightness = 0.2126 * color.red + 0.7152 * color.green + 0.0722 * color.blue;
        foreground = brightness < 0.5 ? "#eeeeee" : "#202124";
    }
    gchar* background = gdk_rgba_to_string(&color);
    gchar* css = g_strdup_printf(
        "#notepad-editor, #notepad-editor text { background-color: transparent; "
        "background-image: none; color: %s; caret-color: %s; }"
        "#notepad-editor text selection { background-color: %s; color: #ffffff; }"
        "#notepad-background { background-color: %s; }",
        foreground, foreground, ui->dark_mode ? "#365d87" : "#2468b4", background);
    if (!ui->appearance_provider)
    {
        ui->appearance_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(ui->window),
            GTK_STYLE_PROVIDER(ui->appearance_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    gtk_css_provider_load_from_data(ui->appearance_provider, css, -1, NULL);
    g_free(css);
    g_free(background);
    gtk_widget_queue_draw(ui->background_area);
}

void on_theme_toggled(GtkCheckMenuItem* item, gpointer data)
{
    if (!gtk_check_menu_item_get_active(item))
    {
        return;
    }
    NotepadApp* app = data;
    app->ui->dark_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "dark-mode"));
    /* GTK's theme variant also updates menus, dialogs and controls. */
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
        app->ui->dark_mode, NULL);
    if (app->ui->text_view)
    {
        apply_appearance(app);
    }
}

void reset_background(NotepadApp* app)
{
    app->ui->custom_background = FALSE;
    g_clear_object(&app->ui->background_image);
    g_clear_pointer(&app->ui->background_image_path, g_free);
    app->ui->background_opacity = 1.0;
    apply_appearance(app);
}

gboolean draw_editor_background(GtkWidget* widget, cairo_t* cr, gpointer data)
{
    NotepadApp* app = data;
    gtk_render_background(gtk_widget_get_style_context(widget), cr, 0, 0,
        gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
    GdkPixbuf* image = app->ui->background_image;
    if (!image)
    {
        return FALSE;
    }
    gdouble width = gtk_widget_get_allocated_width(widget);
    gdouble height = gtk_widget_get_allocated_height(widget);
    gdouble image_width = gdk_pixbuf_get_width(image);
    gdouble image_height = gdk_pixbuf_get_height(image);
    gdouble scale = MAX(width / image_width, height / image_height);
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    cairo_translate(cr, (width - image_width * scale) / 2,
        (height - image_height * scale) / 2);
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, image, 0, 0);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint_with_alpha(cr, app->ui->background_opacity);
    cairo_restore(cr);
    return FALSE;
}

void apply_background_color(NotepadApp* app, const GdkRGBA* color)
{
    g_clear_object(&app->ui->background_image);
    g_clear_pointer(&app->ui->background_image_path, g_free);
    app->ui->custom_background = TRUE;
    app->ui->background_color = *color;
    app->ui->background_opacity = CLAMP(color->alpha, 0.0, 1.0);
    app->ui->background_color.alpha = 1.0;
    apply_appearance(app);
}

void apply_background_image(NotepadApp* app, const gchar* image_path, gdouble opacity)
{
    GError* error = NULL;
    GdkPixbuf* image = gdk_pixbuf_new_from_file(image_path, &error);
    if (!image)
    {
        show_error_dialog(GTK_WINDOW(app->ui->window), "无法加载背景图片", error->message);
        g_clear_error(&error);
        return;
    }
    /* Decode before replacing the previous image: failure leaves it intact. */
    gchar* path = g_strdup(image_path);
    g_clear_object(&app->ui->background_image);
    g_free(app->ui->background_image_path);
    app->ui->background_image = image;
    app->ui->background_image_path = path;
    app->ui->background_opacity = CLAMP(opacity, 0.0, 1.0);
    app->ui->custom_background = TRUE;
    apply_appearance(app);
}

void clear_appearance(NotepadApp* app)
{
    if (app->ui->appearance_provider)
    {
        gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(app->ui->appearance_provider));
        g_clear_object(&app->ui->appearance_provider);
    }
    g_clear_object(&app->ui->background_image);
    g_clear_pointer(&app->ui->background_image_path, g_free);
}
