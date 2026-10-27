#include "notepad.h"

/* A selection ending at the next line's start does not include that line.
 * With no selection, frame the caret's logical line. */
gboolean get_framed_line_range(NotepadApp* app, int* first, int* last)
{
    GtkTextIter start, end;
    if (gtk_text_buffer_get_selection_bounds(app->ui->buffer, &start, &end))
    {
        if (gtk_text_iter_starts_line(&end))
        {
            gtk_text_iter_backward_char(&end);
        }
    }
    else
    {
        gtk_text_buffer_get_iter_at_mark(app->ui->buffer, &start,
            gtk_text_buffer_get_insert(app->ui->buffer));
        end = start;
    }
    *first = gtk_text_iter_get_line(&start);
    *last = gtk_text_iter_get_line(&end);
    return app->ui->frame_selected_lines;
}

static void set_decoration_color(NotepadApp* app, cairo_t* cr)
{
    GdkRGBA color;
    gtk_style_context_get_color(gtk_widget_get_style_context(app->ui->text_view),
        GTK_STATE_FLAG_NORMAL, &color);
    gdk_cairo_set_source_rgba(cr, &color);
}

static PangoLayout* create_line_number_layout(NotepadApp* app, const gchar* text)
{
    PangoLayout* layout = gtk_widget_create_pango_layout(app->ui->text_view, text);
    PangoFontDescription* font = NULL;
    gtk_style_context_get(gtk_widget_get_style_context(app->ui->text_view),
        GTK_STATE_FLAG_NORMAL, "font", &font, NULL);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    return layout;
}

static gboolean draw_line_numbers(GtkWidget* widget, cairo_t* cr, gpointer data)
{
    NotepadApp* app = data;
    GtkTextView* view = GTK_TEXT_VIEW(app->ui->text_view);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);
    GtkTextIter iter;
    gtk_text_view_get_line_at_y(view, &iter, visible.y, NULL);
    PangoLayout* layout = create_line_number_layout(app, NULL);
    set_decoration_color(app, cr);
    do
    {
        gint y, height;
        gtk_text_view_get_line_yrange(view, &iter, &y, &height);
        if (y > visible.y + visible.height)
        {
            break;
        }
        gchar number[32];
        g_snprintf(number, sizeof(number), "%d", gtk_text_iter_get_line(&iter) + 1);
        pango_layout_set_text(layout, number, -1);
        gint width;
        pango_layout_get_pixel_size(layout, &width, NULL);
        gint window_y;
        gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_WIDGET, 0, y, NULL, &window_y);
        cairo_move_to(cr, gtk_widget_get_allocated_width(widget) - width - 8, window_y);
        pango_cairo_show_layout(cr, layout);
    }
    while (gtk_text_iter_forward_line(&iter));
    g_object_unref(layout);
    return FALSE;
}

static gboolean draw_line_frames(GtkWidget* widget, cairo_t* cr, gpointer data)
{
    NotepadApp* app = data;
    int first, last;
    if (!get_framed_line_range(app, &first, &last))
    {
        return FALSE;
    }
    GtkTextView* view = GTK_TEXT_VIEW(widget);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);
    GtkTextIter iter;
    gtk_text_view_get_line_at_y(view, &iter, visible.y, NULL);
    if (gtk_text_iter_get_line(&iter) < first)
    {
        gtk_text_buffer_get_iter_at_line(app->ui->buffer, &iter, first);
    }
    set_decoration_color(app, cr);
    cairo_set_line_width(cr, 1.0);
    do
    {
        gint y, height;
        gtk_text_view_get_line_yrange(view, &iter, &y, &height);
        if (gtk_text_iter_get_line(&iter) > last || y > visible.y + visible.height)
        {
            break;
        }
        gint window_y;
        gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_WIDGET, 0, y, NULL, &window_y);
        cairo_rectangle(cr, 0.5, window_y + 0.5,
            MAX(1, gtk_widget_get_allocated_width(widget) - 1), MAX(1, height - 1));
        cairo_stroke(cr);
    }
    while (gtk_text_iter_forward_line(&iter));
    return FALSE;
}

void refresh_editor_decorations(NotepadApp* app)
{
    if (!app->ui->line_numbers)
    {
        return;
    }
    gchar* widest = g_strdup_printf("%d", gtk_text_buffer_get_line_count(app->ui->buffer));
    PangoLayout* layout = create_line_number_layout(app, widest);
    gint width;
    pango_layout_get_pixel_size(layout, &width, NULL);
    gtk_widget_set_size_request(app->ui->line_numbers, MAX(36, width + 16), -1);
    g_object_unref(layout);
    g_free(widest);
    gtk_widget_queue_draw(app->ui->line_numbers);
    gtk_widget_queue_draw(app->ui->text_view);
}

static void on_editor_scroll(GtkAdjustment* adjustment, gpointer data)
{
    (void)adjustment;
    refresh_editor_decorations(data);
}

static void on_editor_mark(GtkTextBuffer* buffer, GtkTextIter* iter, GtkTextMark* mark, gpointer data)
{
    (void)buffer;
    (void)iter;
    (void)mark;
    refresh_editor_decorations(data);
}

static void on_editor_style_updated(GtkWidget* widget, gpointer data)
{
    (void)widget;
    refresh_editor_decorations(data);
}

void setup_editor_decorations(NotepadApp* app)
{
    g_signal_connect(app->ui->text_view, "style-updated", G_CALLBACK(on_editor_style_updated), app);
    g_signal_connect(app->ui->line_numbers, "draw", G_CALLBACK(draw_line_numbers), app);
    g_signal_connect_after(app->ui->text_view, "draw", G_CALLBACK(draw_line_frames), app);
    g_signal_connect(app->ui->buffer, "mark-set", G_CALLBACK(on_editor_mark), app);
    GtkAdjustment* adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(app->ui->text_view));
    g_signal_connect(adjustment, "value-changed", G_CALLBACK(on_editor_scroll), app);
    refresh_editor_decorations(app);
}

void on_line_numbers_toggled(GtkCheckMenuItem* item, gpointer data)
{
    NotepadApp* app = data;
    app->ui->show_line_numbers = gtk_check_menu_item_get_active(item);
    if (app->ui->line_numbers)
    {
        gtk_widget_set_visible(app->ui->line_numbers, app->ui->show_line_numbers);
        refresh_editor_decorations(app);
    }
}

void on_line_frame_toggled(GtkCheckMenuItem* item, gpointer data)
{
    NotepadApp* app = data;
    app->ui->frame_selected_lines = gtk_check_menu_item_get_active(item);
    refresh_editor_decorations(app);
}
