#include "ui/zoom.h"
#include "core/notepad.h"

typedef struct
{
    int width;
    int height;
    int margins[4];
    unsigned int border;
    int spacing;
} WidgetDimensions;

static gboolean refresh_menu_layout(gpointer data);
static void refresh_menu_on_show(GtkWidget* menu, gpointer data);

static void scale_widget(GtkWidget* widget, gpointer data)
{
    NotepadApp* app = data;
    double scale = app->ui->zoom_percent / 100.0;
    WidgetDimensions* base = g_object_get_data(G_OBJECT(widget), "notepad-zoom-base");
    if (!base)
    {
        base = g_new0(WidgetDimensions, 1);
        gtk_widget_get_size_request(widget, &base->width, &base->height);
        base->margins[0] = gtk_widget_get_margin_start(widget);
        base->margins[1] = gtk_widget_get_margin_end(widget);
        base->margins[2] = gtk_widget_get_margin_top(widget);
        base->margins[3] = gtk_widget_get_margin_bottom(widget);
        if (GTK_IS_CONTAINER(widget))
        {
            base->border = gtk_container_get_border_width(GTK_CONTAINER(widget));
        }
        if (GTK_IS_BOX(widget))
        {
            base->spacing = gtk_box_get_spacing(GTK_BOX(widget));
        }
        g_object_set_data_full(G_OBJECT(widget), "notepad-zoom-base", base, g_free);
    }
    gtk_widget_set_size_request(widget, base->width < 0 ? -1 : (int)(base->width * scale + 0.5),
        base->height < 0 ? -1 : (int)(base->height * scale + 0.5));
    gtk_widget_set_margin_start(widget, (int)(base->margins[0] * scale + 0.5));
    gtk_widget_set_margin_end(widget, (int)(base->margins[1] * scale + 0.5));
    gtk_widget_set_margin_top(widget, (int)(base->margins[2] * scale + 0.5));
    gtk_widget_set_margin_bottom(widget, (int)(base->margins[3] * scale + 0.5));
    if (GTK_IS_MENU_ITEM(widget))
    {
        GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
        if (submenu)
        {
            scale_widget(submenu, app);
        }
    }
    gtk_widget_reset_style(widget);
    gtk_widget_queue_resize(widget);
    if (GTK_IS_MENU(widget))
    {
        if (!g_object_get_data(G_OBJECT(widget), "notepad-menu-refresh"))
        {
            g_signal_connect(widget, "show", G_CALLBACK(refresh_menu_on_show), NULL);
            g_object_set_data(G_OBJECT(widget), "notepad-menu-refresh", GINT_TO_POINTER(TRUE));
        }
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, refresh_menu_layout, g_object_ref(widget), g_object_unref);
    }
    if (GTK_IS_BOX(widget))
    {
        gtk_box_set_spacing(GTK_BOX(widget), (int)(base->spacing * scale + 0.5));
    }
    if (GTK_IS_CONTAINER(widget))
    {
        gtk_container_set_border_width(GTK_CONTAINER(widget), (guint)(base->border * scale + 0.5));
        gtk_container_forall(GTK_CONTAINER(widget), scale_widget, app);
    }
}

static gboolean on_widget_map(GSignalInvocationHint* hint, guint count,
                              const GValue* values, gpointer data)
{
    GtkWidget* widget = g_value_get_object(&values[0]);
    if (GTK_IS_WINDOW(widget))
    {
        scale_widget(widget, data);
    }
    (void)hint;
    (void)count;
    return TRUE;
}

void initialize_zoom(NotepadApp* app)
{
    app->ui->zoom_percent = 100;
    g_object_get(gtk_settings_get_default(), "gtk-xft-dpi", &app->ui->base_dpi, NULL);
    g_object_get(gtk_settings_get_default(), "gtk-font-name", &app->ui->base_ui_font, NULL);
    gdouble dpi = gdk_screen_get_resolution(gdk_screen_get_default());
    if (dpi <= 0)
    {
        dpi = app->ui->base_dpi > 0 ? app->ui->base_dpi / 1024.0 : 96.0;
    }
    g_object_set_data(G_OBJECT(gtk_settings_get_default()), "notepad-base-dpi",
        GINT_TO_POINTER((gint)(dpi * 1024.0 + 0.5)));
    app->ui->zoom_map_hook = g_signal_add_emission_hook(g_signal_lookup("map", GTK_TYPE_WIDGET),
        0, on_widget_map, app, NULL);
    set_app_zoom(app, 100);
}

void clear_zoom(NotepadApp* app)
{
    if (app->ui->zoom_map_hook)
    {
        g_signal_remove_emission_hook(g_signal_lookup("map", GTK_TYPE_WIDGET), app->ui->zoom_map_hook);
        g_object_set(gtk_settings_get_default(), "gtk-font-name", app->ui->base_ui_font, NULL);
        g_clear_pointer(&app->ui->base_ui_font, g_free);
        GtkCssProvider* provider = g_object_get_data(G_OBJECT(gtk_settings_get_default()), "notepad-menu-zoom");
        if (provider)
        {
            gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider));
            g_object_set_data(G_OBJECT(gtk_settings_get_default()), "notepad-menu-zoom", NULL);
        }
        app->ui->zoom_map_hook = 0;
    }
}

static gboolean refresh_menu_layout(gpointer data)
{
    GtkWidget* menu = data;
    gtk_widget_reset_style(menu);
    gtk_widget_queue_resize(menu);
    GtkRequisition minimum, natural;
    gtk_widget_get_preferred_size(menu, &minimum, &natural);
    GtkWidget* window = gtk_widget_get_toplevel(menu);
    if (GTK_IS_WINDOW(window))
    {
        gtk_container_check_resize(GTK_CONTAINER(window));
    }
    if (gtk_widget_get_mapped(menu))
    {
        gtk_menu_reposition(GTK_MENU(menu));
    }
    return G_SOURCE_REMOVE;
}

static void refresh_menu_on_show(GtkWidget* menu, gpointer data)
{
    (void)data;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, refresh_menu_layout, g_object_ref(menu), g_object_unref);
}

void set_app_zoom(NotepadApp* app, int percent)
{
    if (percent != 85 && percent != 100 && percent != 115 && percent != 130)
    {
        return;
    }
    app->ui->zoom_percent = percent;
    /* Keep native screen DPI intact. Always derive from the startup font,
     * never the font produced by the preceding selection. */
    PangoFontDescription* font = pango_font_description_from_string(app->ui->base_ui_font);
    gdouble size = pango_font_description_get_size(font) * (percent / 100.0);
    if (pango_font_description_get_size_is_absolute(font))
    {
        pango_font_description_set_absolute_size(font, size);
    }
    else
    {
        pango_font_description_set_size(font, (gint)(size + 0.5));
    }
    gchar* scaled_font = pango_font_description_to_string(font);
    g_object_set(gtk_settings_get_default(), "gtk-font-name",
        percent == 100 ? app->ui->base_ui_font : scaled_font, NULL);
    GtkCssProvider* menu_provider = g_object_get_data(G_OBJECT(gtk_settings_get_default()), "notepad-menu-zoom");
    if (!menu_provider)
    {
        menu_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(menu_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_set_data_full(G_OBJECT(gtk_settings_get_default()), "notepad-menu-zoom",
            menu_provider, g_object_unref);
    }
    gchar font_size[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_dtostr(font_size, sizeof(font_size), size / PANGO_SCALE);
    gchar* css = g_strdup_printf(
        "menu, menuitem, menuitem label { font-size: %s%s; }"
        "menu menuitem { min-height: %dpx; padding: %dpx %dpx; }",
        font_size, pango_font_description_get_size_is_absolute(font) ? "px" : "pt",
        (int)(24 * percent / 100.0 + 0.5),
        (int)(4 * percent / 100.0 + 0.5), (int)(8 * percent / 100.0 + 0.5));
    gtk_css_provider_load_from_data(menu_provider, css, -1, NULL);
    g_free(css);
    g_free(scaled_font);
    pango_font_description_free(font);
    GList* windows = gtk_window_list_toplevels();
    for (GList* item = windows; item; item = item->next)
    {
        scale_widget(GTK_WIDGET(item->data), app);
    }
    g_list_free(windows);
    if (app->ui->zoom_items)
    {
        app->ui->zoom_syncing = true;
        GList* items = gtk_container_get_children(GTK_CONTAINER(app->ui->zoom_items));
        for (GList* item = items; item; item = item->next)
        {
            int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item->data), "zoom-value"));
            if (GTK_IS_CHECK_MENU_ITEM(item->data))
            {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->data), value == app->ui->zoom_percent);
            }
        }
        g_list_free(items);
        app->ui->zoom_syncing = false;
    }
    refresh_editor_decorations(app);
}

static void on_zoom_preset(GtkWidget* widget, gpointer data)
{
    int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "zoom-value"));
    NotepadApp* app = data;
    if (!app->ui->zoom_syncing)
    {
        set_app_zoom(app, value);
    }
}

GtkWidget* create_zoom_menu(NotepadApp* app)
{
    GtkWidget* root = gtk_menu_item_new_with_label("缩放");
    GtkWidget* menu = gtk_menu_new();
    app->ui->zoom_items = menu;
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(root), menu);
    const int presets[] = {85, 100, 115, 130};
    const char* labels[] = {"较小", "默认", "较大", "特大"};
    for (size_t i = 0; i < G_N_ELEMENTS(presets); ++i)
    {
        GtkWidget* item = gtk_check_menu_item_new_with_label(labels[i]);
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(item), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), presets[i] == 100);
        g_object_set_data(G_OBJECT(item), "zoom-value", GINT_TO_POINTER(presets[i]));
        g_signal_connect(item, "activate", G_CALLBACK(on_zoom_preset), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    return root;
}
