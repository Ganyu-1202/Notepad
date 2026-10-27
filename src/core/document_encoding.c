#include "core/file_operations.h"
#include <string.h>

static const char* encoding_charset(TextEncoding encoding)
{
    switch (encoding)
    {
        case ENCODING_UTF16_LE:
            return "UTF-16LE";
        case ENCODING_UTF16_BE:
            return "UTF-16BE";
        case ENCODING_UTF32_LE:
            return "UTF-32LE";
        case ENCODING_UTF32_BE:
            return "UTF-32BE";
        case ENCODING_GB18030:
            return "GB18030";
        default:
            return "UTF-8";
    }
}

gboolean load_document(NotepadApp* app, const gchar* filename, GError** error)
{
    gchar* bytes = NULL;
    gsize length = 0;
    if (!g_file_get_contents(filename, &bytes, &length, error))
    {
        return FALSE;
    }
    TextEncoding encoding = ENCODING_UTF8;
    size_t skip = 0;
    bool inferred = false;
    if (length >= 4 && memcmp(bytes, "\xff\xfe\x00\x00", 4) == 0)
    {
        encoding = ENCODING_UTF32_LE;
        skip = 4;
    }
    else if (length >= 4 && memcmp(bytes, "\x00\x00\xfe\xff", 4) == 0)
    {
        encoding = ENCODING_UTF32_BE;
        skip = 4;
    }
    else if (length >= 3 && memcmp(bytes, "\xef\xbb\xbf", 3) == 0)
    {
        encoding = ENCODING_UTF8_BOM;
        skip = 3;
    }
    else if (length >= 2 && memcmp(bytes, "\xff\xfe", 2) == 0)
    {
        encoding = ENCODING_UTF16_LE;
        skip = 2;
    }
    else if (length >= 2 && memcmp(bytes, "\xfe\xff", 2) == 0)
    {
        encoding = ENCODING_UTF16_BE;
        skip = 2;
    }
    else if (!g_utf8_validate(bytes, length, NULL))
    {
        encoding = ENCODING_GB18030;
        inferred = true;
    }
    gsize read = 0, written = 0;
    gchar* text = g_convert(bytes + skip, length - skip, "UTF-8",
        encoding_charset(encoding), &read, &written, error);
    g_free(bytes);
    if (!text)
    {
        return FALSE;
    }
    if (read != length - skip || written > G_MAXINT || memchr(text, '\0', written) ||
        !g_utf8_validate(text, written, NULL))
    {
        g_set_error_literal(error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
            "文件包含不完整编码、NUL 字符或超出编辑器容量，无法作为文本打开");
        g_free(text);
        return FALSE;
    }
    /* Do not replace the document until decoding and validation succeed. */
    app->ui->recording_changes = FALSE;
    gtk_text_buffer_set_text(app->ui->buffer, text, (gint)written);
    app->ui->recording_changes = TRUE;
    clear_undo_stack(&app->ui->undo_stack);
    clear_undo_stack(&app->ui->redo_stack);
    g_free(text);
    app->encoding = encoding;
    app->encoding_inferred = inferred;
    gchar* path = g_strdup(filename);
    g_free(app->filename);
    app->filename = path;
    gchar* title = g_strdup_printf("记事本 - %s", app->filename);
    gtk_window_set_title(GTK_WINDOW(app->ui->window), title);
    g_free(title);
    notepad_set_modified(app, false);
    update_encoding_type(app);
    update_line_ending_type(app);
    return TRUE;
}

gboolean save_document(NotepadApp* app, const gchar* filename, GError** error)
{
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->ui->buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(app->ui->buffer, &start, &end, FALSE);
    gsize written = 0;
    gchar* bytes = g_convert(text, -1, encoding_charset(app->encoding), "UTF-8",
        NULL, &written, error);
    g_free(text);
    if (!bytes)
    {
        return FALSE;
    }
    const char* bom = "";
    size_t bom_length = 0;
    switch (app->encoding)
    {
        case ENCODING_UTF8_BOM:
            bom = "\xef\xbb\xbf";
            bom_length = 3;
            break;
        case ENCODING_UTF16_LE:
            bom = "\xff\xfe";
            bom_length = 2;
            break;
        case ENCODING_UTF16_BE:
            bom = "\xfe\xff";
            bom_length = 2;
            break;
        case ENCODING_UTF32_LE:
            bom = "\xff\xfe\x00\x00";
            bom_length = 4;
            break;
        case ENCODING_UTF32_BE:
            bom = "\x00\x00\xfe\xff";
            bom_length = 4;
            break;
        default:
            break;
    }
    GString* output = g_string_new_len(bom, bom_length);
    g_string_append_len(output, bytes, written);
    gboolean saved = g_file_set_contents(filename, output->str, output->len, error);
    g_string_free(output, TRUE);
    g_free(bytes);
    if (saved)
    {
        app->encoding_inferred = false;
        notepad_set_modified(app, false);
        update_encoding_type(app);
    }
    return saved;
}
