/*
 * Copyright (C) 2009 Canonical Ltd.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 * 
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "ui.h"
#include "book-view.h"


enum {
    START_SCAN,
    STOP_SCAN,
    SAVE,
    PRINT,
    QUIT,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };


struct SimpleScanPrivate
{
    GConfClient *client;
    
    GtkBuilder *builder;

    GtkWidget *window;
    GtkWidget *scan_button_label, *continuous_scan_button_label, *page_label;
    GtkWidget *actions_box;
    GtkWidget *device_combo, *mode_combo;
    GtkTreeModel *device_model, *mode_model;
    GtkWidget *replace_pages_check;
    GtkWidget *preview_area;
    GtkWidget *zoom_scale;
    GtkWidget *page_delete_menuitem, *crop_rotate_menuitem;
    BookView *book_view;
    gboolean updating_page_menu;

    gchar *default_file_name;
    gboolean scanning;
};

G_DEFINE_TYPE (SimpleScan, ui, G_TYPE_OBJECT);


static gboolean
find_scan_device (SimpleScan *ui, const char *device, GtkTreeIter *iter)
{
    gboolean have_iter = FALSE;

    if (gtk_tree_model_get_iter_first (ui->priv->device_model, iter)) {
        do {
            gchar *d;
            gtk_tree_model_get (ui->priv->device_model, iter, 0, &d, -1);
            if (strcmp (d, device) == 0)
                have_iter = TRUE;
            g_free (d);
        } while (!have_iter && gtk_tree_model_iter_next (ui->priv->device_model, iter));
    }
    
    return have_iter;
}


void
ui_set_default_file_name (SimpleScan *ui, const gchar *default_file_name)
{
    g_free (ui->priv->default_file_name);
    ui->priv->default_file_name = g_strdup (default_file_name);
}


void
ui_mark_devices_undetected (SimpleScan *ui)
{
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter_first (ui->priv->device_model, &iter)) {
        do {
            gtk_list_store_set (GTK_LIST_STORE (ui->priv->device_model), &iter, 2, FALSE, -1);
        } while (gtk_tree_model_iter_next (ui->priv->device_model, &iter));
    }
}


void
ui_add_scan_device (SimpleScan *ui, const gchar *device, const gchar *label)
{
    GtkTreeIter iter;
    
    if (!find_scan_device (ui, device, &iter)) {
        gtk_list_store_append (GTK_LIST_STORE (ui->priv->device_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (ui->priv->device_model), &iter, 0, device, -1);
    }

    gtk_list_store_set (GTK_LIST_STORE (ui->priv->device_model), &iter, 1, label, 2, TRUE, -1);
    
    /* Select this device if none selected */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (ui->priv->device_combo)) == -1)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->priv->device_combo), &iter);
}


gchar *
ui_get_selected_device (SimpleScan *ui)
{
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (ui->priv->device_combo), &iter)) {
        gchar *device;
        gtk_tree_model_get (ui->priv->device_model, &iter, 0, &device, -1);
        return device;
    }

    return NULL;
}


void
ui_set_selected_device (SimpleScan *ui, const gchar *device)
{
    GtkTreeIter iter;

    /* If doesn't exist add with label set to device name */
    if (!find_scan_device (ui, device, &iter)) {
        gtk_list_store_append (GTK_LIST_STORE (ui->priv->device_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (ui->priv->device_model), &iter, 0, device, 1, device, 2, FALSE, -1);
    }

    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->priv->device_combo), &iter);
}


static void
set_document_hint (SimpleScan *ui, const gchar *document_hint)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first (ui->priv->mode_model, &iter)) {
        do {
            gchar *d;
            gboolean have_match;

            gtk_tree_model_get (ui->priv->mode_model, &iter, 0, &d, -1);
            have_match = strcmp (d, document_hint) == 0;
            g_free (d);

            if (have_match) {
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->priv->mode_combo), &iter);                
                return;
            }
        } while (gtk_tree_model_iter_next (ui->priv->mode_model, &iter));
     }
}


static gchar *
get_document_hint (SimpleScan *ui)
{
    GtkTreeIter iter;
    gchar *mode = NULL;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (ui->priv->mode_combo), &iter))
        gtk_tree_model_get (ui->priv->mode_model, &iter, 0, &mode, -1);
    
    return mode;
}


static gboolean
get_replace_pages (SimpleScan *ui)
{
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui->priv->replace_pages_check));
}


void scan_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
scan_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (ui->priv->scanning) {
        g_signal_emit (G_OBJECT (ui), signals[STOP_SCAN], 0);
    } else {
        gchar *device, *mode;

        device = ui_get_selected_device (ui);
        if (device) {
            mode = get_document_hint (ui);
            g_signal_emit (G_OBJECT (ui), signals[START_SCAN], 0, device, mode,
                           FALSE, get_replace_pages (ui));
            g_free (device);
            g_free (mode);
        }
    }
}


void continuous_scan_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
continuous_scan_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (ui->priv->scanning) {
        g_signal_emit (G_OBJECT (ui), signals[STOP_SCAN], 0);
    } else {
        gchar *device, *mode;

        device = ui_get_selected_device (ui);
        if (device) {
            mode = get_document_hint (ui);
            g_signal_emit (G_OBJECT (ui), signals[START_SCAN], 0, device, mode,
                           TRUE, get_replace_pages (ui));
            g_free (device);
            g_free (mode);
        }
    }
}


static void
page_selected_cb (BookView *view, Page *page, SimpleScan *ui)
{
    char *name = NULL;

    if (page == NULL)
        return;

    ui->priv->updating_page_menu = TRUE;
    
    if (page_has_crop (page)) {
        char *crop_name;

        // FIXME: Make more generic
        crop_name = page_get_named_crop (page);
        if (crop_name) {
            if (strcmp (crop_name, "A4") == 0)
                name = "a4_menuitem";
            else if (strcmp (crop_name, "A5") == 0)
                name = "a5_menuitem";
            else if (strcmp (crop_name, "A6") == 0)
                name = "a6_menuitem";
            else if (strcmp (crop_name, "letter") == 0)
                name = "letter_menuitem";
            else if (strcmp (crop_name, "legal") == 0)
                name = "legal_menuitem";
            else if (strcmp (crop_name, "4x6") == 0)
                name = "4x6_menuitem";
            g_free (crop_name);
        }
        else
            name = "custom_crop_menuitem";
    }
    else
        name = "no_crop_menuitem";

    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_builder_get_object (ui->priv->builder, name)), TRUE);
    
    ui->priv->updating_page_menu = FALSE;
}


void rotate_left_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
rotate_left_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (ui->priv->updating_page_menu)
        return;
    page_rotate_left (book_view_get_selected (ui->priv->book_view));
}


void rotate_right_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
rotate_right_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (ui->priv->updating_page_menu)
        return;
    page_rotate_right (book_view_get_selected (ui->priv->book_view));    
}


static void
set_crop (SimpleScan *ui, const gchar *crop_name)
{
    Page *page;
    
    gtk_widget_set_sensitive (ui->priv->crop_rotate_menuitem, crop_name != NULL);

    if (ui->priv->updating_page_menu)
        return;
    
    page = book_view_get_selected (ui->priv->book_view);
    if (!page)
        return;
    
    if (!crop_name) {
        page_set_no_crop (page);
        return;
    }
    
    page_set_named_crop (page, crop_name);
}


void no_crop_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
no_crop_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, NULL);
}


void custom_crop_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
custom_crop_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "custom");
}


void four_by_six_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
four_by_six_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "4x6");
}

                         
void legal_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
legal_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "legal");
}

                         
void letter_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
letter_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "letter");
}

                         
void a6_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
a6_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "A6");
}


void a5_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
a5_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "A5");
}

                         
void a4_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
a4_menuitem_toggled_cb (GtkWidget *widget, SimpleScan *ui)
{
    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget)))
        set_crop (ui, "A4");
}


void crop_rotate_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
crop_rotate_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui)
{
    Page *page;

    page = book_view_get_selected (ui->priv->book_view);
    if (!page)
        return;
    
    page_rotate_crop (page);
}


void page_delete_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
page_delete_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui)
{
    book_delete_page (book_view_get_book (ui->priv->book_view),
                      book_view_get_selected (ui->priv->book_view));
}


void save_file_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
save_file_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    GtkWidget *dialog;
    gint response;

    /* Title of save dialog */
    dialog = gtk_file_chooser_dialog_new (_("Save As..."),
                                          GTK_WINDOW (ui->priv->window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), ui->priv->default_file_name);
    
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
        gchar *uri;
        
        uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
        g_signal_emit (G_OBJECT (ui), signals[SAVE], 0, uri);

        g_free (uri);
    }
    gtk_widget_destroy (dialog);
}


static void
draw_page (GtkPrintOperation *operation,
           GtkPrintContext   *print_context,
           gint               page_number,
           SimpleScan                *ui)
{
    cairo_t *context;

    context = gtk_print_context_get_cairo_context (print_context);

    g_signal_emit (G_OBJECT (ui), signals[PRINT], 0, context);

    //For some reason can't destroy until job complete
    //cairo_destroy (context);
}


void print_button_clicked_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
print_button_clicked_cb (GtkWidget *widget, SimpleScan *ui)
{
    GtkPrintOperation *print;
    GtkPrintOperationResult result;
    GError *error = NULL;
    
    print = gtk_print_operation_new ();
    gtk_print_operation_set_n_pages (print, 1);
    gtk_print_operation_set_use_full_page (print, TRUE);
    // FIXME: Auto portrait, landscape
    g_signal_connect (print, "draw-page", G_CALLBACK (draw_page), ui);

    result = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                      GTK_WINDOW (ui->priv->window), &error);

    g_object_unref (print);
}


static void
load_device_cache (SimpleScan *ui)
{
    gchar *filename;
    GKeyFile *key_file;
    gboolean result;
    GError *error = NULL;
    
    filename = g_build_filename (g_get_user_cache_dir (), "simple-scan", "device_cache", NULL);
    
    key_file = g_key_file_new ();
    result = g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);
    if (error) {
        if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning ("Error loading device cache file: %s", error->message);
        g_error_free (error);
        error = NULL;
    }
    if (result) {
        gchar **groups, **group_iter;

        groups = g_key_file_get_groups (key_file, NULL);
        for (group_iter = groups; *group_iter; group_iter++) {
            gchar *label, *device;

            label = *group_iter;
            device = g_key_file_get_value (key_file, label, "device", &error);
            if (error) {
                g_warning ("Error getting device name for label '%s': %s", label, error->message);
                g_error_free (error);
                error = NULL;
            }
            
            if (device)
                ui_add_scan_device (ui, device, label);

            g_free (device);
        }

        g_strfreev (groups);
    }

    g_free (filename);
    g_key_file_free (key_file);
}


static void
save_device_cache (SimpleScan *ui)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_debug ("Saving device cache");

    model = ui->priv->device_model;
    if (gtk_tree_model_get_iter_first (model, &iter)) {
        GKeyFile *key_file;
        gchar *data;
        gsize data_length;
        GError *error = NULL;

        key_file = g_key_file_new ();
        do {
            gchar *name, *label;
            gboolean detected;
            
            gtk_tree_model_get (model, &iter, 0, &name, 1, &label, 2, &detected, -1);
            
            if (detected) {
                g_debug ("Storing device '%s' in cache", name);
                g_key_file_set_value (key_file, label, "device", name);
            }

            g_free (name);
            g_free (label);
        } while (gtk_tree_model_iter_next (model, &iter));
        
        data = g_key_file_to_data (key_file, &data_length, &error);
        if (data) {
            gchar *dir, *filename;
            GFile *file;
            GFileOutputStream *stream;
            GError *error = NULL;

            dir = g_build_filename (g_get_user_cache_dir (), "simple-scan", NULL);
            g_mkdir_with_parents (dir, 0700);
            filename = g_build_filename (dir, "device_cache", NULL);

            file = g_file_new_for_path (filename);
            stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
            if (error) {
                g_warning ("Error writing device cache: %s", error->message);
                g_error_free (error);
                error = NULL;
            }
            if (stream) {
                g_output_stream_write_all (G_OUTPUT_STREAM (stream), data, data_length, NULL, NULL, &error);
                if (error) {
                    g_warning ("Error writing device cache: %s", error->message);
                    g_error_free (error);
                    error = NULL;
                }
                g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
            }
            g_free (data);

            g_free (filename);
            g_free (dir);        
        }

        g_key_file_free (key_file);
    }
}


void about_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
about_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui)
{
    const gchar *authors[] = { "Robert Ancell <robert.ancell@canonical.com>", NULL };

    /* The license this software is under (GPL3+) */
    const char *license = _("This program is free software: you can redistribute it and/or modify\n"
                            "it under the terms of the GNU General Public License as published by\n"
                            "the Free Software Foundation, either version 3 of the License, or\n"
                            "(at your option) any later version.\n"
                            "\n"
                            "This program is distributed in the hope that it will be useful,\n"
                            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                            "GNU General Public License for more details.\n"
                            "\n"
                            "You should have received a copy of the GNU General Public License\n"
                            "along with this program.  If not, see <http://www.gnu.org/licenses/>.");

    /* Title of about dialog */
    const char *title = _("About Simple Scan");

    /* Description of program */
    const char *description = _("Simple document scanning tool");

    gtk_show_about_dialog (GTK_WINDOW (ui->priv->window),
                           "title", title,
                           "program-name", "Simple Scan",
                           "version", VERSION,
                           "comments", description,
                           "authors", authors,
                           "website", "https://launchpad.net/simple-scan",
                           "copyright", "Copyright (C) 2009 Canonical Ltd.",
                           "license", license,
                           NULL);
}


static void
quit (SimpleScan *ui)
{
    char *device, *document_type;

    save_device_cache (ui);

    device = ui_get_selected_device (ui);
    if (device) {
        gconf_client_set_string(ui->priv->client, "/apps/simple-scan/selected_device", device, NULL);
        g_free (device);
    }

    document_type = get_document_hint (ui);
    gconf_client_set_string(ui->priv->client, "/apps/simple-scan/document_type", document_type, NULL);
    g_free (document_type);

    gconf_client_set_bool(ui->priv->client, "/apps/simple-scan/replace_pages",
                          get_replace_pages (ui), NULL);

    g_signal_emit (G_OBJECT (ui), signals[QUIT], 0);
}


void quit_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui);
G_MODULE_EXPORT
void
quit_menuitem_activate_cb (GtkWidget *widget, SimpleScan *ui)
{
    quit (ui);
}


gboolean window_delete_event_cb (GtkWidget *widget, GdkEvent *event, SimpleScan *ui);
G_MODULE_EXPORT
gboolean
window_delete_event_cb (GtkWidget *widget, GdkEvent *event, SimpleScan *ui)
{
    quit (ui);
    return TRUE;
}


static void
ui_load (SimpleScan *ui)
{
    GtkBuilder *builder;
    GError *error = NULL;
    GtkCellRenderer *renderer;
    gchar *device, *document_type;
    gboolean replace_pages;

    builder = ui->priv->builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, UI_DIR "simple-scan.ui", &error);
    if (error) {
        g_critical ("Unable to load UI: %s\n", error->message);
        ui_show_error (ui,
                       /* Title of dialog when cannot load required files */
                       _("Files missing"),
                       /* Description in dialog when cannot load required files */
                       _("Please check your installation"));
        exit (1);
    }
    gtk_builder_connect_signals (builder, ui);

    ui->priv->window = GTK_WIDGET (gtk_builder_get_object (builder, "simple_scan_window"));
    ui->priv->scan_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "scan_button_label"));
    ui->priv->continuous_scan_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "continuous_scan_button_label"));
    ui->priv->page_label = GTK_WIDGET (gtk_builder_get_object (builder, "page_label"));
    ui->priv->actions_box = GTK_WIDGET (gtk_builder_get_object (builder, "actions_box"));
    ui->priv->device_combo = GTK_WIDGET (gtk_builder_get_object (builder, "device_combo"));
    ui->priv->device_model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->priv->device_combo));
    ui->priv->mode_combo = GTK_WIDGET (gtk_builder_get_object (builder, "mode_combo"));
    ui->priv->mode_model = gtk_combo_box_get_model (GTK_COMBO_BOX (ui->priv->mode_combo));
    ui->priv->replace_pages_check = GTK_WIDGET (gtk_builder_get_object (builder, "replace_pages_check"));
    ui->priv->preview_area = GTK_WIDGET (gtk_builder_get_object (builder, "preview_area"));
    ui->priv->zoom_scale = GTK_WIDGET (gtk_builder_get_object (builder, "zoom_scale"));
    ui->priv->page_delete_menuitem = GTK_WIDGET (gtk_builder_get_object (builder, "page_delete_menuitem"));
    ui->priv->crop_rotate_menuitem = GTK_WIDGET (gtk_builder_get_object (builder, "crop_rotate_menuitem"));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ui->priv->device_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (ui->priv->device_combo), renderer, "text", 1);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ui->priv->mode_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (ui->priv->mode_combo), renderer, "text", 1);
    gtk_combo_box_set_active (GTK_COMBO_BOX (ui->priv->mode_combo), 0);

    /* Load previously detected scanners and select the last used one */
    load_device_cache (ui);
    device = gconf_client_get_string(ui->priv->client, "/apps/simple-scan/selected_device", NULL);
    if (device) {
        GtkTreeIter iter;
        if (find_scan_device (ui, device, &iter))
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (ui->priv->device_combo), &iter);
        g_free (device);
    }
    
    document_type = gconf_client_get_string(ui->priv->client, "/apps/simple-scan/document_type", NULL);
    if (document_type) {
        set_document_hint (ui, document_type);
        g_free (document_type);
    }

    replace_pages = gconf_client_get_bool (ui->priv->client, "/apps/simple-scan/replace_pages", &error);
    if (error)
        g_error_free (error);
    else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->priv->replace_pages_check), replace_pages);
    
    ui->priv->book_view = book_view_new ();
    g_signal_connect (ui->priv->book_view, "page-selected", G_CALLBACK (page_selected_cb), ui);
    book_view_set_widget (ui->priv->book_view, ui->priv->preview_area,
                          GTK_WIDGET (gtk_builder_get_object (builder, "page_menu")));
    gtk_range_set_adjustment (GTK_RANGE (ui->priv->zoom_scale),
                              book_view_get_zoom_adjustment (ui->priv->book_view));
    
    /* Start with a reasonable size */
    gtk_window_resize (GTK_WINDOW (ui->priv->window), 600, 400);
}


SimpleScan *
ui_new ()
{
    return g_object_new (SIMPLE_SCAN_TYPE, NULL);
}


void
ui_set_book (SimpleScan *ui, Book *book)
{
    book_view_set_book (ui->priv->book_view, book);
}


void
ui_set_selected_page (SimpleScan *ui, Page *page)
{
    book_view_select_page (ui->priv->book_view, page);
}


Page *
ui_get_selected_page (SimpleScan *ui)
{
    return book_view_get_selected (ui->priv->book_view);
}


void
ui_set_scanning (SimpleScan *ui, gboolean scanning)
{
    ui->priv->scanning = scanning;
    if (ui->priv->scanning) {
        gtk_label_set_label (GTK_LABEL (ui->priv->scan_button_label),
                             /* Label on cancel scan button */
                             _("_Cancel"));
        gtk_label_set_label (GTK_LABEL (ui->priv->continuous_scan_button_label),
                             /* Label on cancel scan button */
                             _("_Cancel"));
    }
    else {
        gtk_label_set_label (GTK_LABEL (ui->priv->scan_button_label),
                             /* Label on scan button */
                             _("_Scan"));
        gtk_label_set_label (GTK_LABEL (ui->priv->continuous_scan_button_label),
                             /* Label on continuous scan button */
                             _("_Continuous Scan"));
    }

    gtk_widget_set_sensitive (ui->priv->page_delete_menuitem, !scanning);
}


void
ui_set_have_scan (SimpleScan *ui, gboolean have_scan)
{
    gtk_widget_set_sensitive (ui->priv->actions_box, have_scan);
}


void
ui_show_error (SimpleScan *ui, const gchar *error_title, const gchar *error_text)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (ui->priv->window),
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", error_title);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", error_text);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}


void
ui_start (SimpleScan *ui)
{
    gtk_widget_show (ui->priv->window);
}


/* Generated with glib-genmarshal */
static void
g_cclosure_user_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN (GClosure     *closure,
                                                             GValue       *return_value G_GNUC_UNUSED,
                                                             guint         n_param_values,
                                                             const GValue *param_values,
                                                             gpointer      invocation_hint G_GNUC_UNUSED,
                                                             gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_STRING_BOOLEAN_BOOLEAN) (gpointer       data1,
                                                                    gconstpointer  arg_1,
                                                                    gconstpointer  arg_2,
                                                                    gboolean       arg_3,
                                                                    gboolean       arg_4,
                                                                    gpointer       data2);
  register GMarshalFunc_VOID__STRING_STRING_BOOLEAN_BOOLEAN callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 5);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_STRING_BOOLEAN_BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_string (param_values + 1),
            g_value_get_string (param_values + 2),
            g_value_get_boolean (param_values + 3),
            g_value_get_boolean (param_values + 4),
            data2);
}


static void
ui_class_init (SimpleScanClass *klass)
{
    signals[START_SCAN] =
        g_signal_new ("start-scan",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (SimpleScanClass, start_scan),
                      NULL, NULL,
                      g_cclosure_user_marshal_VOID__STRING_STRING_BOOLEAN_BOOLEAN,
                      G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    signals[STOP_SCAN] =
        g_signal_new ("stop-scan",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (SimpleScanClass, stop_scan),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[SAVE] =
        g_signal_new ("save",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (SimpleScanClass, save),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[PRINT] =
        g_signal_new ("print",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (SimpleScanClass, print),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[QUIT] =
        g_signal_new ("quit",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (SimpleScanClass, quit),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (SimpleScanPrivate));
}


static void
ui_init (SimpleScan *ui)
{
    ui->priv = G_TYPE_INSTANCE_GET_PRIVATE (ui, SIMPLE_SCAN_TYPE, SimpleScanPrivate);

    ui->priv->client = gconf_client_get_default();
    gconf_client_add_dir(ui->priv->client, "/apps/simple-scan", GCONF_CLIENT_PRELOAD_NONE, NULL);

    ui->priv->default_file_name = g_strdup (_("Scanned Document.pdf"));
    ui->priv->scanning = FALSE;
    ui_load (ui);
}
