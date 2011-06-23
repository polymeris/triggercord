/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011 Andras Salamon <andras.salamon@melda.info>

    based on:

    PK-Remote: Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include "pslr.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

/* ----------------------------------------------------------------------- */

#define MAX_BUFFERS 9

static struct {
    char *autosave_path;
} plugin_config;

/* ----------------------------------------------------------------------- */

void shutter_press(GtkWidget *widget);
static void focus_button_clicked_cb(GtkButton *widget);

static void green_button_clicked_cb(GtkButton *widget);
static void ae_lock_button_toggled_cb(GtkToggleButton *widget);

static gboolean added_quit(gpointer data);
static void my_atexit(void);

static void menu_quit_activate_cb(GtkMenuItem *item, gpointer user_data);
static void menu_about_activate_cb(GtkMenuItem *item, gpointer user_data);

static void menu_buffer_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data);
static void menu_settings_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data);
static void menu_histogram_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data);

static gboolean bufferwindow_delete_event_cb(GtkWidget *window, GdkEvent *event, gpointer user_data);
static gboolean settings_window_delete_event_cb(GtkWidget *window, GdkEvent *event, gpointer user_data);
static gboolean histogram_window_delete_event_cb(GtkWidget *window, GdkEvent *event, gpointer user_data);

gboolean main_drawing_area_motion_notify_event_cb(GtkWidget *drawing_area, 
                                                  GdkEventMotion *event, 
                                                  gpointer user_data);

gboolean main_drawing_area_button_press_event_cb(GtkWidget *drawing_area, 
                                                GdkEventButton *event, 
                                                gpointer user_data);


gboolean da_expose(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData);
gboolean bufferwindow_expose_event_cb(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData);
gboolean histogram_drawing_area_expose_event_cb(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData);
gboolean da_buttonpress(GtkWidget *widget);
void plugin_quit(GtkWidget *widget);
int common_init(void);
void init_preview_area(void);
void set_preview_icon(int n, GdkPixbuf *pBuf);

gboolean error_dialog_close(GtkWidget *widget);

void preview_save_as(GtkButton *widget);
void preview_delete_button_clicked_cb(GtkButton *widget);

void preview_icon_view_selection_changed_cb(GtkIconView *widget);

void error_message(const gchar *message);

static gchar* shutter_scale_format_value_cb(GtkScale *scale, gdouble value);
static gchar* aperture_scale_format_value_cb(GtkScale *scale, gdouble value);
static gchar* iso_scale_format_value_cb(GtkScale *scale, gdouble value);
static gchar* ec_scale_format_value_cb(GtkScale *scale, gdouble value);

static void aperture_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void shutter_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void iso_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void ec_scale_value_changed_cb(GtkScale *scale, gpointer user_data);

static void jpeg_sharpness_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void jpeg_contrast_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void jpeg_saturation_scale_value_changed_cb(GtkScale *scale, gpointer user_data);
static void jpeg_hue_scale_value_changed_cb(GtkScale *scale, gpointer user_data);

static void jpeg_resolution_combo_changed_cb(GtkCombo *combo, gpointer user_data);
static void jpeg_quality_combo_changed_cb(GtkCombo *combo, gpointer user_data);
static void jpeg_image_mode_combo_changed_cb(GtkCombo *combo, gpointer user_data);

static void file_format_combo_changed_cb(GtkCombo *combo, gpointer user_data);

static void user_mode_combo_changed_cb(GtkCombo *combo, gpointer user_data);

static void auto_save_folder_button_clicked_cb(GtkButton *widget);
static void auto_folder_entry_changed_cb(GtkEntry *widget);

static gboolean status_poll(gpointer data);
static void update_preview_area(int buffer);
static void update_main_area(int buffer);

static void init_controls(pslr_status *st_new, pslr_status *st_old);
static bool auto_save_check(int format, int buffer);
static void manage_camera_buffers(pslr_status *st_new, pslr_status *st_old);

static void which_shutter_table(pslr_status *st, const pslr_rational_t **table, int *steps);
static void which_iso_table(pslr_status *st, const int **table, int *steps);
static void which_ec_table(pslr_status *st, const int **table, int *steps);
static bool is_inside(int rect_x, int rect_y, int rect_w, int rect_h, int px, int py);
static user_file_format file_format(pslr_status *st);

static void save_buffer(int bufno, const char *filename);

/* ----------------------------------------------------------------------- */

#define AF_FAR_LEFT   132
#define AF_LEFT       223
#define AF_CENTER     319
#define AF_RIGHT      415
#define AF_FAR_RIGHT  505      // 283

#define AF_TOP        (149+27) // 84
#define AF_MID        (213+27)
#define AF_BOTTOM     (276+27) // 156

#define AF_CROSS_W    9
#define AF_CROSS_H    10

#define AF_CENTER_W   15
#define AF_CENTER_H   15

#define AF_LINE_W     7
#define AF_LINE_H     21

/* Order of array corresponsds to AF point bitmask, see
 * PSLR_AF_POINT_* defines. */
static struct {
    int x;
    int y;
    int w;
    int h;
} af_points[] = {
    { AF_LEFT-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_FAR_LEFT - (AF_LINE_W/2), AF_MID - (AF_LINE_H/2) - 1, AF_LINE_W, AF_LINE_H },
    { AF_LEFT-(AF_CROSS_W/2), AF_MID-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CENTER_W/2), AF_MID-(AF_CENTER_H/2) - 1, AF_CENTER_W, AF_CENTER_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_MID-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_FAR_RIGHT - (AF_LINE_W/2), AF_MID - (AF_LINE_H/2) - 1, AF_LINE_W, AF_LINE_H },
    { AF_LEFT-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
 };

static uint32_t focus_indicated_af_points;
static uint32_t select_indicated_af_points;
static uint32_t preselect_indicated_af_points;
static bool preselect_reselect = false;

/* 40-220 confirmed */
/* This is the nominator, the denominator is 10 for all confirmed
 * apertures */
static const int aperture_tbl[] = {
    10, 11, 12, 14, 16, 17, 18,
    20, 22, 24, 25, 28, 32, 35,
    40, 45, 50, 56, 63, 67, 71,
    80, 90, 95, 100, 110, 130, 140,
    160, 180, 190, 200, 220, 250, 280,
    320, 360, 400, 450, 510, 570
};

static const pslr_rational_t shutter_tbl_1_3[] = {
{ 30, 1},{ 25, 1},{ 20, 1},{ 15, 1},{ 13, 1},{ 10, 1},{ 8 , 1},{ 6 , 1},
{ 5 , 1},{ 4 , 1},{ 3 , 1},{ 25, 10},{ 2 , 1},{ 16, 10},{ 13, 10},{ 1 , 1},
{ 8 , 10},{ 6 , 10},{ 5 , 10},{ 4 , 10},{ 3 , 10},{ 1 , 4},{ 1 , 5},{ 1 , 6},
{ 1 , 8},{ 1 , 10},{ 1 , 13},{ 1 , 15},{ 1 , 20},{ 1 , 25},{ 1 , 30},{ 1 , 40},
{ 1 , 50},{ 1 , 60},{ 1 , 80},{ 1 , 100},{ 1 , 125},{ 1 , 160},{ 1 , 200},{ 1 , 250},
{ 1 , 320},{ 1 , 400},{ 1 , 500},{ 1 , 640},{ 1 , 800},{ 1 , 1000},{ 1 , 1250},{ 1 , 1600},
{ 1 , 2000},{ 1 , 2500},{ 1 , 3200},{ 1 , 4000},{1 , 5000}, {1, 6000}, {1, 8000}
};

static const pslr_rational_t shutter_tbl_1_2[] = {
{ 30, 1},{ 20, 1},{ 15, 1},{ 10, 1},{ 8 , 1},{ 6 , 1},
{ 4 , 1},{ 3 , 1},{ 2 , 1},{ 15, 10},{ 1 , 1},
{ 7 , 10},{ 5 , 10},{ 3 , 10},{ 1 , 4},{ 1 , 6},
{ 1 , 8},{ 1 , 10},{ 1 , 15},{ 1 , 20},{ 1 , 30},
{ 1 , 45},{ 1 , 60},{ 1 , 90},{ 1 , 125},{ 1 , 180},{ 1 , 250},
{ 1 , 350},{ 1 , 500},{ 1 , 750},{ 1 , 1000},{ 1 , 1500},
{ 1 , 2000},{ 1 , 3000},{ 1 , 4000}, {1, 6000}, {1, 8000}
};


static const int iso_tbl_1_3[] = {
    100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600
};

static const int iso_tbl_1_2[] = {
    100, 140, 200, 280, 400, 560, 800, 1100, 1600
};

static const int iso_tbl_1[] = {
    100, 200, 400, 800, 1600, 3200, 6400
};

static const int ec_tbl_1_3[] = {
    -30, -27, -23, -20, -17, -13, -10, -7, -3, 0, 3, 7, 10, 13, 17, 20, 23, 27, 30
};

static const int ec_tbl_1_2[]= {
    -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 30
};

/*
static const struct { uint32_t id; const char *name; } lens_id[] = {
    { 0xfa, "smc DA 50-200" },
    { 0xfc, "smc DA 18-55" },
    { 0x1c, "smc FA 35" }
    };*/

/* ----------------------------------------------------------------------- */

static pslr_handle_t camhandle;
static GladeXML *xml;
static GtkStatusbar *statusbar;
static guint sbar_connect_ctx;
static guint sbar_download_ctx;

#ifdef DEBUG
#define DEBUG_DEFAULT true
#else
#define DEBUG_DEFAULT false
#endif

static bool debug = DEBUG_DEFAULT;

int common_init(void)
{
    GtkWidget *widget;

    atexit(my_atexit);

    gtk_init(0, 0);

    gtk_quit_add(0, added_quit, 0);

    DPRINT("Create glade xml\n");

    if (debug) {
        xml = glade_xml_new("pktriggercord.glade", NULL, NULL);
    } else {
        xml = glade_xml_new(DATADIR "/pktriggercord.glade", NULL, NULL);
    }

    init_preview_area();

    widget = glade_xml_get_widget(xml, "mainwindow");
    glade_xml_signal_connect(xml, "shutter_press", G_CALLBACK(shutter_press));
    glade_xml_signal_connect(xml, "focus_button_clicked_cb", 
                             G_CALLBACK(focus_button_clicked_cb));

    glade_xml_signal_connect(xml, "green_button_clicked_cb", 
                             G_CALLBACK(green_button_clicked_cb));
    glade_xml_signal_connect(xml, "ae_lock_button_toggled_cb", 
                             G_CALLBACK(ae_lock_button_toggled_cb));

    glade_xml_signal_connect(xml, "plugin_quit", G_CALLBACK(plugin_quit));

    glade_xml_signal_connect(xml, "main_drawing_area_expose_event_cb", G_CALLBACK(da_expose));
    glade_xml_signal_connect(xml, "main_drawing_area_button_press_event_cb", 
                             G_CALLBACK(main_drawing_area_button_press_event_cb));
    glade_xml_signal_connect(xml, "main_drawing_area_motion_notify_event_cb", 
                             G_CALLBACK(main_drawing_area_motion_notify_event_cb));

    glade_xml_signal_connect(xml, "bufferwindow_expose_event_cb", G_CALLBACK(bufferwindow_expose_event_cb));

    glade_xml_signal_connect(xml, "histogram_drawing_area_expose_event_cb", G_CALLBACK(histogram_drawing_area_expose_event_cb));

    glade_xml_signal_connect(xml, "mainwindow_expose", G_CALLBACK(da_expose));

    glade_xml_signal_connect(xml, "error_dialog_close", G_CALLBACK(error_dialog_close));

    glade_xml_signal_connect(xml, "preview_save_as", G_CALLBACK(preview_save_as));
    glade_xml_signal_connect(xml, "preview_delete_button_clicked_cb", G_CALLBACK(preview_delete_button_clicked_cb));

    glade_xml_signal_connect(xml, "preview_icon_view_selection_changed_cb", 
                             G_CALLBACK(preview_icon_view_selection_changed_cb));


    glade_xml_signal_connect(xml, "file_format_combo_changed_cb", 
                             G_CALLBACK(file_format_combo_changed_cb));

    glade_xml_signal_connect(xml, "user_mode_combo_changed_cb", 
                             G_CALLBACK(user_mode_combo_changed_cb));

    //glade_xml_signal_connect(xml, "preview_save_as_save", G_CALLBACK(preview_save_as_save));
    //glade_xml_signal_connect(xml, "preview_save_as_cancel", G_CALLBACK(preview_save_as_cancel));

    glade_xml_signal_connect(xml, "shutter_scale_format_value_cb", G_CALLBACK(shutter_scale_format_value_cb));
    glade_xml_signal_connect(xml, "aperture_scale_format_value_cb", G_CALLBACK(aperture_scale_format_value_cb));

    glade_xml_signal_connect(xml, "iso_scale_format_value_cb", G_CALLBACK(iso_scale_format_value_cb));
    glade_xml_signal_connect(xml, "ec_scale_format_value_cb", G_CALLBACK(ec_scale_format_value_cb));


    glade_xml_signal_connect(xml, "shutter_scale_value_changed_cb", G_CALLBACK(shutter_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "aperture_scale_value_changed_cb", G_CALLBACK(aperture_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "iso_scale_value_changed_cb", G_CALLBACK(iso_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "ec_scale_value_changed_cb", G_CALLBACK(ec_scale_value_changed_cb));

    glade_xml_signal_connect(xml, "jpeg_resolution_combo_changed_cb", G_CALLBACK(jpeg_resolution_combo_changed_cb));
    glade_xml_signal_connect(xml, "jpeg_quality_combo_changed_cb", G_CALLBACK(jpeg_quality_combo_changed_cb));
    glade_xml_signal_connect(xml, "jpeg_image_mode_combo_changed_cb", G_CALLBACK(jpeg_image_mode_combo_changed_cb));

    glade_xml_signal_connect(xml, "jpeg_sharpness_scale_value_changed_cb", G_CALLBACK(jpeg_sharpness_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "jpeg_saturation_scale_value_changed_cb", G_CALLBACK(jpeg_saturation_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "jpeg_contrast_scale_value_changed_cb", G_CALLBACK(jpeg_contrast_scale_value_changed_cb));
    glade_xml_signal_connect(xml, "jpeg_hue_scale_value_changed_cb", G_CALLBACK(jpeg_hue_scale_value_changed_cb));

    glade_xml_signal_connect(xml, "menu_quit_activate_cb", G_CALLBACK(menu_quit_activate_cb));
    glade_xml_signal_connect(xml, "menu_about_activate_cb", G_CALLBACK(menu_about_activate_cb));
    glade_xml_signal_connect(xml, "menu_buffer_window_toggled_cb", G_CALLBACK(menu_buffer_window_toggled_cb));
    glade_xml_signal_connect(xml, "menu_settings_window_toggled_cb", 
                             G_CALLBACK(menu_settings_window_toggled_cb));
    glade_xml_signal_connect(xml, "menu_histogram_window_toggled_cb", 
                             G_CALLBACK(menu_histogram_window_toggled_cb));

    glade_xml_signal_connect(xml, "bufferwindow_delete_event_cb", G_CALLBACK(bufferwindow_delete_event_cb));
    glade_xml_signal_connect(xml, "settings_window_delete_event_cb", G_CALLBACK(settings_window_delete_event_cb));
    glade_xml_signal_connect(xml, "histogram_window_delete_event_cb", G_CALLBACK(histogram_window_delete_event_cb));

    glade_xml_signal_connect(xml, "auto_save_folder_button_clicked_cb", 
                             G_CALLBACK(auto_save_folder_button_clicked_cb));

    glade_xml_signal_connect(xml, "auto_folder_entry_changed_cb", 
                             G_CALLBACK(auto_folder_entry_changed_cb));

    statusbar = GTK_STATUSBAR(glade_xml_get_widget(xml, "statusbar1"));
    sbar_connect_ctx = gtk_statusbar_get_context_id(statusbar, "connect");
    sbar_download_ctx = gtk_statusbar_get_context_id(statusbar, "download");

    gtk_statusbar_push(statusbar, sbar_connect_ctx, "No camera connected.");

    gdk_window_set_events(widget->window, GDK_ALL_EVENTS_MASK);

    GtkComboBox *pw = (GtkComboBox*)glade_xml_get_widget(xml, "file_format_combo");

    int i;
    for (i = 0; i<sizeof(file_formats) / sizeof (file_formats[0]); i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(pw), file_formats[i].file_format_name);                 	
    }

    init_controls(NULL, NULL);

    g_timeout_add(1000, status_poll, 0);

    gtk_widget_show(widget);

    gtk_main();
    return 0;
}

static int get_jpeg_property_shift() {
    return (pslr_get_model_jpeg_property_levels( camhandle )-1) / 2;
}

void shutter_speed_table_init(pslr_status *st) {
    // check valid shutter speeds for the camera
    int max_valid_shutter_speed_index=0;
    int i;
    const pslr_rational_t *tbl;
    int steps;
    which_shutter_table( st, &tbl, &steps);
    for(i=0;  i<steps; i++) {
	if( tbl[i].nom == 1 &&
	    tbl[i].denom <= pslr_get_model_fastest_shutter_speed(camhandle)) {
	    max_valid_shutter_speed_index = i;
	}
    }
    GtkWidget *pw;
    pw = glade_xml_get_widget(xml, "shutter_scale");
    //printf("range 0-%f\n", (float) sizeof(shutter_tbl)/sizeof(shutter_tbl[0]));
    gtk_range_set_range(GTK_RANGE(pw), 0.0, max_valid_shutter_speed_index);
    gtk_range_set_increments(GTK_RANGE(pw), 1.0, 1.0);
}

void camera_specific_init() {
    gtk_range_set_range( GTK_RANGE(glade_xml_get_widget(xml, "jpeg_hue_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    gtk_range_set_range( GTK_RANGE(glade_xml_get_widget(xml, "jpeg_sharpness_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    gtk_range_set_range( GTK_RANGE(glade_xml_get_widget(xml, "jpeg_saturation_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    gtk_range_set_range( GTK_RANGE(glade_xml_get_widget(xml, "jpeg_contrast_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    int *resolutions = pslr_get_model_jpeg_resolutions( camhandle );
    int resindex=0;
    gchar buf[256];
    while( resindex < MAX_RESOLUTION_SIZE ) {
	sprintf( buf, "%dM", resolutions[resindex]); 
	gtk_combo_box_insert_text( GTK_COMBO_BOX(glade_xml_get_widget(xml, "jpeg_resolution_combo")), resindex, buf);
	++resindex;
    }
}   

static void init_controls(pslr_status *st_new, pslr_status *st_old)
{
    GtkWidget *pw;
    int min_ap=-1, max_ap=-1, current_ap = -1;
    int current_iso = -1;
    int idx;
    int i;
    bool disable = false;

    if (st_new == NULL)
        disable = true;

    /* Aperture scale */
    pw = glade_xml_get_widget(xml, "aperture_scale");

    if (st_new) {
        for (i=0; i<sizeof(aperture_tbl)/sizeof(aperture_tbl[0]); i++) {
            if (st_new->lens_min_aperture.nom == aperture_tbl[i])
                min_ap = i;
            if (st_new->lens_max_aperture.nom == aperture_tbl[i])
                max_ap = i;
            if (st_new->set_aperture.nom == aperture_tbl[i])
                current_ap = i;
        }

        gtk_range_set_increments(GTK_RANGE(pw), 1.0, 1.0);

        if (min_ap >= 0 && max_ap >= 0 && current_ap >= 0) {
            //printf("range %f-%f (%f)\n", (float) min_ap, (float)max_ap, (float)current_ap);
            gtk_range_set_range(GTK_RANGE(pw), min_ap, max_ap);
            gtk_range_set_value(GTK_RANGE(pw), current_ap);
            gtk_widget_set_sensitive(pw, TRUE);
        }
    }

    if (disable || min_ap < 0 || max_ap < 0 || current_ap < 0) {
        gtk_widget_set_sensitive(pw, FALSE);
    } else {
        gtk_widget_set_sensitive(pw, TRUE);
    }

    /* Shutter speed scale */
    pw = glade_xml_get_widget(xml, "shutter_scale");
    if (st_new) {
        idx = -1;
	const pslr_rational_t *tbl = 0;
        int steps = 0;    
        which_shutter_table( st_new, &tbl, &steps );
        for (i=0; i<steps; i++) {
            if (st_new->set_shutter_speed.nom == tbl[i].nom
                && st_new->set_shutter_speed.denom == tbl[i].denom) {
                idx = i;
	    }
        }
        if (idx >= 0) {
            gtk_range_set_value(GTK_RANGE(pw), idx);
        }
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* ISO scale */

    pw = glade_xml_get_widget(xml, "iso_scale");
    if (st_new) {

        const int *tbl = 0;
        int steps = 0;
        which_iso_table(st_new, &tbl, &steps);
        gtk_range_set_range(GTK_RANGE(pw), 0.0, (gdouble) (steps-1));
        for (i=0; i<steps; i++) {
            if (tbl[i] >= st_new->fixed_iso) {
                current_iso = i;
                break;
            }
        }
        if (current_iso >= 0) {
            //printf("set value for ISO slider, new = %d old = %d\n",
            //       st_new->set_iso, st_old ? st_old->set_iso : -1);
            gtk_range_set_value(GTK_RANGE(pw), current_iso);
        }
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* EC scale */
    pw = glade_xml_get_widget(xml, "ec_scale");
    if (st_new) {
        const int *tbl;
        int steps;
	which_ec_table( st_new, &tbl, &steps);
        idx = -1;
        for (i=0; i<steps; i++) {
            if (tbl[i] == st_new->ec.nom)
                idx = i;
        }
        if (!st_old || st_old->custom_ev_steps != st_new->custom_ev_steps)
            gtk_range_set_range(GTK_RANGE(pw), 0.0, steps-1);
        if (!st_old || st_old->ec.nom != st_new->ec.nom 
            || st_old->ec.denom != st_new->ec.denom)
            gtk_range_set_value(GTK_RANGE(pw), idx);
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* JPEG contrast */
    pw = glade_xml_get_widget(xml, "jpeg_contrast_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), st_new->jpeg_contrast - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* JPEG hue */
    pw = glade_xml_get_widget(xml, "jpeg_hue_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), st_new->jpeg_hue - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);


    /* JPEG saturation */
    pw = glade_xml_get_widget(xml, "jpeg_saturation_scale");
    if (st_new)
        gtk_range_set_value(GTK_RANGE(pw), st_new->jpeg_saturation - get_jpeg_property_shift());

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* JPEG sharpness */
    pw = glade_xml_get_widget(xml, "jpeg_sharpness_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), st_new->jpeg_sharpness - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* JPEG quality */
    pw = glade_xml_get_widget(xml, "jpeg_quality_combo");
    if (st_new)
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->jpeg_quality);

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* JPEG resolution */
    pw = glade_xml_get_widget(xml, "jpeg_resolution_combo");
    if (st_new)
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->jpeg_resolution);

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* Image mode */
    pw = glade_xml_get_widget(xml, "jpeg_image_mode_combo");
    if (st_new)
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->jpeg_image_mode);

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* USER mode selector */
    pw = glade_xml_get_widget(xml, "user_mode_combo");
    if (st_new) {
        if (!st_old || st_old->exposure_mode != st_new->exposure_mode) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->exposure_mode);
	}
    }

    gtk_widget_set_sensitive(pw, st_new != NULL && st_new->user_mode_flag);
    

    /* Format selector */
    pw = glade_xml_get_widget(xml, "file_format_combo");
    if (st_new) {
        int val = file_format(st_new);
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), val);        
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);

    /* Buttons */
    pw = glade_xml_get_widget(xml, "shutter_button");
    gtk_widget_set_sensitive(pw, st_new != NULL);
    pw = glade_xml_get_widget(xml, "focus_button");
    gtk_widget_set_sensitive(pw, st_new != NULL);
    pw = glade_xml_get_widget(xml, "quick_gimp_button");
    gtk_widget_set_sensitive(pw, st_new != NULL);
    pw = glade_xml_get_widget(xml, "green_button");
    gtk_widget_set_sensitive(pw, st_new != NULL);

    pw = glade_xml_get_widget(xml, "ae_lock_button");
    if (st_new) {
        bool lock;
        lock = (st_new->light_meter_flags & PSLR_LIGHT_METER_AE_LOCK) != 0;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw), lock);
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static pslr_status cam_status[2];
static pslr_status *status_new = NULL;
static pslr_status *status_old = NULL;

static gboolean status_poll(gpointer data)
{
    GtkWidget *pw;
    gchar buf[256];
    pslr_status *tmp;
    int ret;
    static bool status_poll_inhibit = false;

    if (status_poll_inhibit)
        return TRUE;

    /* Do not recursively status poll */
    status_poll_inhibit = true;

    if (!camhandle) {
        camhandle = pslr_init();
        if (camhandle) {
            /* Try to reconnect */
            gtk_statusbar_pop(statusbar, sbar_connect_ctx);
            gtk_statusbar_push(statusbar, sbar_connect_ctx, "Connecting...");

            /* Allow the message to be seen */
            while (gtk_events_pending())
                gtk_main_iteration();

            /* Connect */
            pslr_connect(camhandle);
        }

        gtk_statusbar_pop(statusbar, sbar_connect_ctx);
        if (camhandle) {
            camera_specific_init();
            const char *name;
            name = pslr_camera_name(camhandle);
            snprintf(buf, sizeof(buf), "Connected: %s", name);
            buf[sizeof(buf)-1] = '\0';
            gtk_statusbar_push(statusbar, sbar_connect_ctx, buf);
        } else {
            gtk_statusbar_push(statusbar, sbar_connect_ctx, "No camera connected.");
        }
        status_poll_inhibit = false;
        return TRUE;
    }

    tmp = status_new;
    status_new = status_old;
    status_old = tmp;

    if (status_new == NULL) {
        if (status_old == NULL)
            status_new = &cam_status[0];
        else
            status_new = &cam_status[1];
    }

    ret = pslr_get_status(camhandle, status_new);
    shutter_speed_table_init( status_new );
    if (ret != PSLR_OK) {
        if (ret == PSLR_DEVICE_ERROR) {
            /* Camera disconnected */
            camhandle = NULL;
        }
        DPRINT("pslr_get_status: %d\n", ret);
        status_new = NULL;
    }

    /* aperture label */
    pw = glade_xml_get_widget(xml, "label_aperture");
    if (status_new && status_new->current_aperture.denom) {
        float aper = (float)status_new->current_aperture.nom / (float)status_new->current_aperture.denom;
        sprintf(buf, "f/%.1f", aper);
        gtk_label_set_text(GTK_LABEL(pw), buf);
    }

    /* shutter speed label */
    if (status_new && status_new->current_shutter_speed.denom) {
        if (status_new->current_shutter_speed.nom == 1) {
            sprintf(buf, "1/%ds", status_new->current_shutter_speed.denom);
        } else if (status_new->current_shutter_speed.denom == 1) {
            sprintf(buf, "%ds", status_new->current_shutter_speed.nom);
        } else {
            sprintf(buf, "%.1fs", (float)status_new->current_shutter_speed.nom / (float) status_new->current_shutter_speed.denom);
        }
        pw = glade_xml_get_widget(xml, "label_shutter");
        gtk_label_set_text(GTK_LABEL(pw), buf);

    }

    /* ISO label */
    if (status_new) {
        sprintf(buf, "ISO %d", status_new->current_iso);
        pw = glade_xml_get_widget(xml, "label_iso");
        gtk_label_set_text(GTK_LABEL(pw), buf);
    }

    /* EV label */
    if (status_new && status_new->current_aperture.denom 
        && status_new->current_shutter_speed.denom) {
        float ev, a, s;
        a = (float)status_new->current_aperture.nom/(float)status_new->current_aperture.denom;
        s = (float)status_new->current_shutter_speed.nom/(float)status_new->current_shutter_speed.denom;

        ev = log(a*a/s)/M_LN2 - log(status_new->current_iso/100)/M_LN2;
    
        sprintf(buf, "<span size=\"xx-large\">%.2f</span>", ev);
        pw = glade_xml_get_widget(xml, "label_ev");
        gtk_label_set_markup(GTK_LABEL(pw), buf);
    }

    /* Zoom label */
    if (status_new && status_new->zoom.denom) {
        pw = glade_xml_get_widget(xml, "label_zoom");
        sprintf(buf, "%d mm", status_new->zoom.nom / status_new->zoom.denom);
        gtk_label_set_text(GTK_LABEL(pw), buf);
    }

    /* Focus label */
    if (status_new) {
        pw = glade_xml_get_widget(xml, "label_focus");
        sprintf(buf, "focus: %d", status_new->focus);
        gtk_label_set_text(GTK_LABEL(pw), buf);
    }

    /* Other controls */
    init_controls(status_new, status_old);

    /* AF point indicators */
    pw = glade_xml_get_widget(xml, "main_drawing_area");
    if (status_new) {
        if (!status_old || status_old->focused_af_point != status_new->focused_af_point) {
            /* Change in focused AF points */
            focus_indicated_af_points |= status_new->focused_af_point;
            gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);
        } else {
            /* Same as previous check, stop indicating */
            focus_indicated_af_points = 0;
            gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);
        } 

        if (!status_old || status_old->selected_af_point != status_new->selected_af_point) {
            /* Change in selected AF points */
            select_indicated_af_points = status_new->selected_af_point;
            gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);
        } else {
            /* Same as previous check, stop indicating */
            select_indicated_af_points = 0;
            gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);
        }
        preselect_indicated_af_points = 0;
        preselect_reselect = false;
    }
    
    /* Camera buffer checks */
    manage_camera_buffers(status_new, status_old);
    //printf("end poll\n");

    status_poll_inhibit = false;
    return TRUE;
}

static void manage_camera_buffers(pslr_status *st_new, pslr_status *st_old)
{
    uint32_t new_pictures;
    bool deleted;
    int new_picture;
    int format;
    int i;

    if (!st_new) {
        for (i=0; i<MAX_BUFFERS; i++) {
            set_preview_icon(i, NULL);
        }
        return;
    }

    if (st_old && st_new->bufmask == st_old->bufmask)
        return;

    if (st_old)
        new_pictures = (st_new->bufmask ^ st_old->bufmask) & st_new->bufmask;
    else
        new_pictures = st_new->bufmask;

    if (!new_pictures)
        return;

    /* Show the newest picture in the main area */

    for (new_picture=MAX_BUFFERS; new_picture>=0; --new_picture) {
        if (new_pictures & (1<<new_picture))
            break;
    }
    if (new_picture >= 0) {
        update_main_area(new_picture);
    }

    format = file_format(st_new);

    /* auto-save check buffers */
    for (i=0; i<MAX_BUFFERS; i++) {
        if (new_pictures & (1<<i)) {
            deleted = auto_save_check(format, i);
            if (deleted)
                new_pictures &= ~(1<<i);
        }
    }

    /* Update buffer window for buffers that were not deleted by
     * auto_save_check */
    for (i=0; i<MAX_BUFFERS; i++) {
        if (new_pictures & (1<<i))
            update_preview_area(i);
    }
}

static void auto_save_folder_button_clicked_cb(GtkButton *widget)
{
    GtkWidget *pw;
    char *filename;
    int res;

    pw = glade_xml_get_widget(xml, "auto_save_folder_dialog");
    res = gtk_dialog_run(GTK_DIALOG(pw));
    DPRINT("Run folder dialog -> %d\n", res);
    gtk_widget_hide(pw);

    if (res == 1) {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pw));
        DPRINT("Selected path: %s\n", filename);
        if (plugin_config.autosave_path)
            g_free(plugin_config.autosave_path);
        plugin_config.autosave_path = g_strdup(filename);
        pw = glade_xml_get_widget(xml, "auto_folder_entry");
        gtk_entry_set_text(GTK_ENTRY(pw), plugin_config.autosave_path);
    } else {
        DPRINT("Cancelled.\n");
    }
}

static void auto_folder_entry_changed_cb(GtkEntry *widget)
{
    DPRINT("Auto folder changed to %s\n", gtk_entry_get_text(widget));
    if (plugin_config.autosave_path)
        g_free(plugin_config.autosave_path);
    plugin_config.autosave_path = g_strdup(gtk_entry_get_text(widget));
}

static bool auto_save_check(int format, int buffer)
{
    GtkWidget *pw;
    gboolean autosave;
    gboolean autodelete;
    const gchar *filebase;
    gint counter;
    int ret;
    GtkSpinButton *spin;
    char *old_path = NULL;
    GtkProgressBar *pbar;
    bool deleted = false;
    char filename[256];

    pw = glade_xml_get_widget(xml, "auto_save_check");
    autosave = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    if (!autosave)
        return false;

    pbar = GTK_PROGRESS_BAR(glade_xml_get_widget(xml, "download_progress"));

    pw = glade_xml_get_widget(xml, "auto_save_check");
    autodelete = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    spin = GTK_SPIN_BUTTON(glade_xml_get_widget(xml, "auto_name_spin"));
    counter = gtk_spin_button_get_value_as_int(spin);
    DPRINT("Counter = %d\n", counter);
    pw = glade_xml_get_widget(xml, "auto_name_entry");
    filebase = gtk_entry_get_text(GTK_ENTRY(pw));

    /* Change to auto-save path */
    if (plugin_config.autosave_path) {
        old_path = getcwd(NULL, 0);
        if (chdir(plugin_config.autosave_path) == -1) {
            char msg[256];
            
            snprintf(msg, sizeof(msg), "Could not save in folder %s: %s", 
                     plugin_config.autosave_path, strerror(errno));
            error_message(msg);
            free(old_path);
            return false;
        }
    }

    gtk_statusbar_push(statusbar, sbar_download_ctx, "Auto-saving");
    while (gtk_events_pending())
        gtk_main_iteration();

    snprintf(filename, sizeof(filename), "%s%04d.%s", filebase, counter, file_formats[format].extension);
    DPRINT("Save buffer %d\n", buffer);
    gtk_progress_bar_set_text(pbar, filename);
    save_buffer(buffer, filename);
    gtk_progress_bar_set_text(pbar, NULL);

    if (autodelete) {
        int retry;
        pslr_status st;
        /* Init bufmask to 1's so that we don't see buffer as deleted
         * if we never got a good status. */
        st.bufmask = ~0;
        DPRINT("Delete buffer %d\n", buffer);
        for (retry = 0; retry < 5; retry++)  {
            ret = pslr_delete_buffer(camhandle, buffer);
            if (ret == PSLR_OK)
                break;
            DPRINT("Could not delete buffer %d: %d\n", buffer, ret);
            usleep(100000);
        }
        for (retry=0; retry<5; retry++) {
            pslr_get_status(camhandle, &st);
            if ((st.bufmask & (1<<buffer))==0)
                break;
            DPRINT("Buffer not gone - wait\n");
        }
        set_preview_icon(buffer, NULL);
        if ((st.bufmask & (1<<buffer)) == 0) {
            deleted = true;
        }
    }
    counter++;
    DPRINT("Set counter -> %d\n", counter);
    gtk_spin_button_set_value(spin, counter);

    if (old_path) {
        chdir(old_path);
        free(old_path);
    }

    gtk_statusbar_pop(statusbar, sbar_download_ctx);

    //printf("auto_save_check done\n");
    return deleted;
}

bool buf_updated = false;
GdkPixbuf *pMainPixbuf = NULL;
//static GdkPixbuf *pThumbPixbuf[MAX_BUFFERS];

static void update_main_area(int buffer)
{
    GError *pError;
    uint8_t *pImage;
    uint32_t imageSize;
    int r;
    GdkPixbuf *pixBuf;
    GtkWidget *pw;

    gtk_statusbar_push(statusbar, sbar_download_ctx, "Getting preview");
    while (gtk_events_pending())
        gtk_main_iteration();

    pError = NULL;
    DPRINT("Trying to read buffer\n");
    r = pslr_get_buffer(camhandle, buffer, PSLR_BUF_PREVIEW, 4, &pImage, &imageSize);
    if (r != PSLR_OK) {
        printf("Could not get buffer data\n");
        goto the_end;
    }

    GInputStream *ginput = g_memory_input_stream_new_from_data (pImage, imageSize, NULL);
    pixBuf = gdk_pixbuf_new_from_stream( ginput, NULL, &pError);
    if (!pixBuf) {
        printf("No pixbuf from loader.\n");
        goto the_end;
    }
    g_object_ref(pixBuf);
    pError = NULL;
    pMainPixbuf = pixBuf;
    
    pw = glade_xml_get_widget(xml, "histogram_drawing_area");
    gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);

  the_end:
    gtk_statusbar_pop(statusbar, sbar_download_ctx);

}

static void update_preview_area(int buffer)
{
    GError *pError;
    uint8_t *pImage;
    uint32_t imageSize;
    int r;
    GdkPixbuf *pixBuf;
    GtkWidget *pw;

    pw = glade_xml_get_widget(xml, "test_draw_area");
    
    gtk_statusbar_push(statusbar, sbar_download_ctx, "Getting thumbnails");
    while (gtk_events_pending())
        gtk_main_iteration();

    DPRINT("buffer %d has new contents\n", buffer);
    pError = NULL;

    DPRINT("Trying to get thumbnail\n");
    r = pslr_get_buffer(camhandle, buffer, PSLR_BUF_THUMBNAIL, 4, &pImage, &imageSize);
    if (r != PSLR_OK) {
        printf("Could not get buffer data\n");
        goto the_end;
    }
    DPRINT("got %d bytes at %p\n", imageSize, pImage);
    GInputStream *ginput = g_memory_input_stream_new_from_data (pImage, imageSize, NULL);

    pixBuf = gdk_pixbuf_new_from_stream( ginput, NULL, &pError);
    if (!pixBuf) {
        printf("No pixbuf from loader.\n");
        goto the_end;
    }
    g_object_ref(pixBuf);
    set_preview_icon(buffer, pixBuf);
    pError = NULL;
  the_end:
    gtk_statusbar_pop(statusbar, sbar_download_ctx);
}

static void menu_quit_activate_cb(GtkMenuItem *item, gpointer user_data)
{
    DPRINT("menu quit.\n");
    gtk_main_quit();
}

static void menu_about_activate_cb(GtkMenuItem *item, gpointer user_data)
{
    GtkWidget *pw;
    DPRINT("menu about.\n");
    pw = glade_xml_get_widget(xml, "about_dialog");
    gtk_about_dialog_set_version( GTK_ABOUT_DIALOG(pw), VERSION);
    gtk_dialog_run(GTK_DIALOG(pw));
    gtk_widget_hide(pw);
}

static void menu_buffer_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data)
{
    guint checked;
    GtkWidget *pw;
    pw = glade_xml_get_widget(xml, "bufferwindow");
    checked = item->active;
    DPRINT("buffer window %d.\n", checked);
    if (checked) {
        gtk_widget_show(pw);
    } else {
        gtk_widget_hide(pw);
    }
}

static void menu_settings_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data)
{
    guint checked;
    GtkWidget *pw;
    pw = glade_xml_get_widget(xml, "settings_window");
    checked = item->active;
    DPRINT("settings window %d.\n", checked);
    if (checked) {
        gtk_widget_show(pw);
    } else {
        gtk_widget_hide(pw);
    }
}

static void menu_histogram_window_toggled_cb(GtkCheckMenuItem *item, gpointer user_data)
{
    guint checked;
    GtkWidget *pw;
    pw = glade_xml_get_widget(xml, "histogram_window");
    checked = item->active;
    DPRINT("histogram window %d.\n", checked);
    if (checked) {
        gtk_widget_show(pw);
    } else {
        gtk_widget_hide(pw);
    }
}

gboolean da_expose(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData)
{
    GtkWidget *pw;
    GdkGC *gc_focus, *gc_sel, *gc_presel;
    GdkColor red = { 0, 65535, 0, 0 };
    GdkColor green = { 0, 0, 65535, 0 };
    GdkColor yellow = { 0, 65535, 65535, 0 };
    int i;
    gboolean fill;

    //printf("Expose event for widget %p\n", widget);
    pw = glade_xml_get_widget(xml, "main_drawing_area");

    if (pMainPixbuf)
        gdk_pixbuf_render_to_drawable(pMainPixbuf, pw->window, pw->style->fg_gc[GTK_WIDGET_STATE (pw)], 0, 0, 0, 0, 640, 480, GDK_RGB_DITHER_NONE, 0, 0);

    gc_focus = gdk_gc_new(pw->window);
    gc_sel = gdk_gc_new(pw->window);
    gc_presel = gdk_gc_new(pw->window);
    gdk_gc_copy(gc_focus, pw->style->fg_gc[GTK_WIDGET_STATE (pw)]);
    gdk_gc_copy(gc_sel, pw->style->fg_gc[GTK_WIDGET_STATE (pw)]);
    gdk_gc_copy(gc_presel, pw->style->fg_gc[GTK_WIDGET_STATE (pw)]);

    gdk_gc_set_rgb_fg_color(gc_focus, &red);
    gdk_gc_set_rgb_fg_color(gc_sel, &green);
    gdk_gc_set_rgb_fg_color(gc_presel, &yellow);

    for (i=0; i<sizeof(af_points)/sizeof(af_points[0]); i++) {
        GdkGC *the_gc;
        the_gc = select_indicated_af_points & (1<<i) ? gc_sel : gc_focus;
        if (preselect_indicated_af_points & (1<<i)) {
            /* The user clicked an AF point and this should be
             * indicated. If it's the same as the current AF point
             * selected in the camera (indicated bu
             * preselect_reselect), it should be green. If not, it
             * means we're waiting for camera confirmation and it
             * should be yellow. */
            if (preselect_reselect)
                the_gc = gc_sel;
            else
                the_gc = gc_presel;
        }
        fill = focus_indicated_af_points & (1<<i) ? TRUE : FALSE;
        gdk_draw_rectangle(pw->window, the_gc, fill, 
                           af_points[i].x, af_points[i].y,
                           af_points[i].w, af_points[i].h);
    }

    gdk_gc_destroy(gc_focus);
    gdk_gc_destroy(gc_sel);
    gdk_gc_destroy(gc_presel);

    return FALSE;
}

gboolean main_drawing_area_motion_notify_event_cb(GtkWidget *drawing_area, 
                                                  GdkEventMotion *event, 
                                                  gpointer user_data)
{
    int x = rint(event->x);
    int y = rint(event->y);
    int i;

    //printf("Pointer motion widget=%p\n", drawing_area);

    for (i=0; i<sizeof(af_points)/sizeof(af_points[0]); i++) {
        if (is_inside(af_points[i].x, af_points[i].y, af_points[i].w, af_points[i].h,
                      x, y)) {
            //printf("hover: af point %d\n", i);
        }
    }

    return TRUE;
}

gboolean main_drawing_area_button_press_event_cb(GtkWidget *drawing_area, 
                                                GdkEventButton *event, 
                                                gpointer user_data)
{
    int x = rint(event->x);
    int y = rint(event->y);
    int i;
    int ret;
    GtkWidget *pw;

    /* Don't care about clicks on AF points if no camera is connected. */
    if (!camhandle)
        return TRUE;

    /* Click one at a time please */
    if (preselect_indicated_af_points)
        return TRUE;

    for (i=0; i<sizeof(af_points)/sizeof(af_points[0]); i++) {
        if (is_inside(af_points[i].x, af_points[i].y, af_points[i].w, af_points[i].h,
                      x, y)) {
            //printf("click: af point %d\n", i);

            preselect_indicated_af_points = 1<<i;
            if (status_new && status_new->selected_af_point == preselect_indicated_af_points) {
                preselect_reselect = true;
            } else {
                preselect_reselect = false;
            }
            pw = glade_xml_get_widget(xml, "main_drawing_area");
            gdk_window_invalidate_rect(pw->window, &pw->allocation, FALSE);            
            while (gtk_events_pending())
                gtk_main_iteration();
            if (status_new && status_new->af_point_select == PSLR_AF_POINT_SEL_SELECT) {
                ret = pslr_select_af_point(camhandle, 1 << i);
                if (ret != PSLR_OK)
                    DPRINT("Could not select AF point %d\n", i);
            }
            break;
        }
    }
    return TRUE;
}

static bool is_inside(int rect_x, int rect_y, int rect_w, int rect_h, int px, int py)
{
    if (px < rect_x)
        return false;
    if (py < rect_y)
        return false;
    if (px >= rect_x + rect_w)
        return false;
    if (py >= rect_y + rect_h)
        return false;
    return true;
}

gboolean da_buttonpress(GtkWidget *widget)
{
    GtkWidget *pw;
    DPRINT("Button event for widget %p\n", widget);
    pw = glade_xml_get_widget(xml, "test_draw_area");
    if (pw != widget)
        DPRINT("pw != widget\n");
    gdk_draw_arc (pw->window,
                  pw->style->fg_gc[GTK_WIDGET_STATE (pw)],
                  TRUE,
                  0, 0, pw->allocation.width, pw->allocation.height,
                  0, 64 * 360);
    return FALSE;
}

gboolean bufferwindow_expose_event_cb(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData)
{
    DPRINT("bufferwindow expose\n");
    return FALSE;
}

gboolean histogram_drawing_area_expose_event_cb(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData)
{
    DPRINT("histogram window expose\n");
    GtkWidget *pw;
    guchar *pixels;
    uint32_t histogram[256][3];
    int x, y;
    uint32_t scale;
    GdkGC *gc;
    int pitch;
    int hist_w, hist_h;
    int wx1, wy1, wx2, wy2;

    const GdkColor hist_colors[] = {
        { 0, 65535, 0, 0 },
        { 0, 0, 65535, 0 },
        { 0, 0, 0, 65535 }
    };

    pw = glade_xml_get_widget(xml, "histogram_drawing_area");
    if (pw != widget)
        DPRINT("pw != widget\n");

    if (!pMainPixbuf)
        return TRUE;

    hist_w = pw->allocation.width;
    hist_h = pw->allocation.height / 3;
    g_assert (gdk_pixbuf_get_colorspace (pMainPixbuf) == GDK_COLORSPACE_RGB);
    g_assert (gdk_pixbuf_get_bits_per_sample (pMainPixbuf) == 8);
    g_assert (!gdk_pixbuf_get_has_alpha (pMainPixbuf));
    g_assert (gdk_pixbuf_get_n_channels(pMainPixbuf) == 3);

    g_assert (gdk_pixbuf_get_width(pMainPixbuf) == 640);
    g_assert (gdk_pixbuf_get_height(pMainPixbuf) == 480);

    pixels = gdk_pixbuf_get_pixels(pMainPixbuf);
    pitch = gdk_pixbuf_get_rowstride(pMainPixbuf);

    memset(histogram, 0, sizeof(histogram));

    for (y=26; y<428; y++) {
        for (x=0; x<640; x++) {
            uint8_t r = pixels[y*pitch+x*3+0];
            uint8_t g = pixels[y*pitch+x*3+1];
            uint8_t b = pixels[y*pitch+x*3+2];
            histogram[r][0]++;
            histogram[g][1]++;
            histogram[b][2]++;
        }
    }

    scale = 0;
    for (y=0; y<3; y++) {
        for (x=0; x<256; x++) {
            if (histogram[x][y] > scale)
                scale = histogram[x][y];
        }
    }

    gc = gdk_gc_new(pw->window);
    gdk_gc_copy(gc, pw->style->fg_gc[GTK_WIDGET_STATE (pw)]);
    
    for (y=0; y<3; y++) {
        gdk_gc_set_rgb_fg_color(gc, &hist_colors[y]);
        for (x=0; x<256; x++) {
            wx1 = hist_w*x / 256;
            wx2 = hist_w*(x+1) / 256;
            int yval = histogram[x][y] * hist_h / scale;
            wy1 = (hist_h * y) + hist_h - yval;
            wy2 = (hist_h * y) + hist_h;
            gdk_draw_rectangle(pw->window, gc, TRUE, wx1, wy1, wx2-wx1, wy2-wy1);
        }
    }

    return FALSE;
}

void shutter_press(GtkWidget *widget)
{
    int r;
    DPRINT("Shutter press.\n");
    r = pslr_shutter(camhandle);
    if (r != PSLR_OK) {
        DPRINT("shutter error\n");
        return;
    }
}

static void focus_button_clicked_cb(GtkButton *widget)
{
    DPRINT("Focus");
    int ret;
    ret = pslr_focus(camhandle);
    if (ret != PSLR_OK) {
        DPRINT("Focus failed: %d\n", ret);
    }
}

static void green_button_clicked_cb(GtkButton *widget)
{
    DPRINT("Green btn");
    int ret;
    ret = pslr_green_button(camhandle);
    if (ret != PSLR_OK) {
        DPRINT("Green button failed: %d\n", ret);
    }
}

static void ae_lock_button_toggled_cb(GtkToggleButton *widget)
{
    DPRINT("AE Lock");
    gboolean active = gtk_toggle_button_get_active(widget);
    int ret;
    gboolean locked;
    if (status_new == NULL)
        return;
    locked = (status_new->light_meter_flags & PSLR_LIGHT_METER_AE_LOCK) != 0;
    if (locked != active) {
        ret = pslr_ae_lock(camhandle, active);
        if (ret != PSLR_OK) {
            DPRINT("AE lock failed: %d\n", ret);
        }
    }
}

static void my_atexit(void)
{
    DPRINT("atexit\n");
}

static gboolean added_quit(gpointer data)
{
    DPRINT("added_quit\n");
    if (camhandle) {
        pslr_disconnect(camhandle);
        pslr_shutdown(camhandle);
        camhandle = 0;
    }
    return FALSE;
}

void plugin_quit(GtkWidget *widget)
{
    DPRINT("Quit plugin.\n");
    gtk_main_quit();
}

static gboolean bufferwindow_delete_event_cb(GtkWidget *window, GdkEvent *event, 
                                             gpointer user_data)
{
    GtkWidget *pw;
    DPRINT("Hide buffer window.\n");
    pw = glade_xml_get_widget(xml, "menu_buffer_window");
    gtk_widget_hide(window);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pw), FALSE);
    return TRUE;
}

static gboolean settings_window_delete_event_cb(GtkWidget *window, GdkEvent *event, 
                                                gpointer user_data)
{
    GtkWidget *pw;
    DPRINT("Hide settings window.\n");
    pw = glade_xml_get_widget(xml, "menu_settings_window");
    gtk_widget_hide(window);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pw), FALSE);
    return TRUE;
}

static gboolean histogram_window_delete_event_cb(GtkWidget *window, GdkEvent *event, 
                                                gpointer user_data)
{
    GtkWidget *pw;
    DPRINT("Hide histogram window.\n");
    pw = glade_xml_get_widget(xml, "menu_histogram_window");
    gtk_widget_hide(window);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pw), FALSE);
    return TRUE;
}


void error_message(const gchar *message)
{
    GtkWidget *pw;

    pw = glade_xml_get_widget(xml, "error_message");
    gtk_label_set_markup(GTK_LABEL(pw), message);
    pw = glade_xml_get_widget(xml, "errordialog");
    gtk_dialog_run(GTK_DIALOG(pw));
    //gtk_widget_show(pw);
}


gboolean error_dialog_close(GtkWidget *widget)
{
    GtkWidget *pw;
    DPRINT("close event.\n");
    pw = glade_xml_get_widget(xml, "errordialog");
    gtk_widget_hide(pw);
    return FALSE;
}

static GtkListStore *list_store;

void init_preview_area(void)
{
    GtkWidget *pw;
    GtkTreeIter iter;
    gint i;

    DPRINT("init_preview_area\n");

    pw = glade_xml_get_widget(xml, "preview_icon_view");

    list_store = gtk_list_store_new (1,
                                     GDK_TYPE_PIXBUF);

    for (i = 0; i < MAX_BUFFERS; i++) {
//        gchar *some_data;
//        char buf[256];
//        sprintf(buf, "Icon%d", i);
//        some_data = buf;

        /* Add a new row to the model */
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            0, NULL,
                            -1);
    }

    gtk_icon_view_set_model(GTK_ICON_VIEW(pw), GTK_TREE_MODEL(list_store));
}

void set_preview_icon(int n, GdkPixbuf *pBuf)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    DPRINT("set_preview_icon\n");
    path = gtk_tree_path_new_from_indices (n, -1);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store),
                             &iter,
                             path);
    gtk_tree_path_free (path);
    gtk_list_store_set (list_store, &iter, 0, pBuf, -1);
}

static gchar* shutter_scale_format_value_cb(GtkScale *scale, gdouble value)
{
    int idx = rint(value);
    //printf("shutter value: %f\n", value);

    const pslr_rational_t *tbl = 0;
    int steps = 0;    
    if (!status_new) {
	return g_strdup_printf("(%f)", value);
    }
    which_shutter_table(status_new, &tbl, &steps);

    if(idx >= 0 && idx < steps) {
        int n = tbl[idx].nom;
        int d = tbl[idx].denom;
        if (n == 1) {
            return g_strdup_printf("1/%d", d);
        } else if (d == 1) {
            return g_strdup_printf("%d\"", n);
        } else {
            return g_strdup_printf("%.1f\"", (float)n/(float)d);
	}
    } else {
        return g_strdup_printf("(%f)", value);
    }
}

static gchar* aperture_scale_format_value_cb(GtkScale *scale, gdouble value)
{
    int idx = rint(value);
    //printf("aperture value: %f\n", value);
    if(idx < sizeof(aperture_tbl)/sizeof(aperture_tbl[0])) {
        return g_strdup_printf("f/%.1f", aperture_tbl[idx]/10.0);
    } else {
        return g_strdup_printf("(%f)", value);
    }
}

static gchar* iso_scale_format_value_cb(GtkScale *scale, gdouble value)
{
    int i = rint(value);
    const int *tbl = 0;
    int steps = 0;
    if (status_new) {
        which_iso_table(status_new, &tbl, &steps);
        if (i >= 0 && i < steps) {
            return g_strdup_printf("%d", tbl[i]);
        }
    }
    return g_strdup_printf("(%d)", i);
}

static gchar* ec_scale_format_value_cb(GtkScale *scale, gdouble value)
{
    int i = rint(value);
    const int *tbl;
    int steps;
    if (status_new) {
        which_ec_table(status_new, &tbl, &steps);
        if (i >= 0 && i < steps) {
            return g_strdup_printf("%.1f", tbl[i]/10.0);
        } 
    }
    return g_strdup_printf("(%f)", value);
}

static void aperture_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    gdouble a;
    pslr_rational_t value;
    int idx;
    int ret;
    a = gtk_range_get_value(GTK_RANGE(scale));
    idx = rint(a);
    assert(idx >= 0);
    assert(idx < sizeof(aperture_tbl)/sizeof(aperture_tbl[0]));
    value.nom = aperture_tbl[idx];
    value.denom = 10;
    DPRINT("aperture->%d/%d\n", value.nom, value.denom);
    ret = pslr_set_aperture(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set aperture failed: %d\n", ret);
    }
}

static void shutter_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    gdouble a;
    pslr_rational_t value;
    int ret;
    int idx;
    const pslr_rational_t *tbl;
    int steps;

    if (!status_new) {
        return;
    }
    a = gtk_range_get_value(GTK_RANGE(scale));
    idx = rint(a);
    which_shutter_table(status_new, &tbl, &steps);
    assert(idx >= 0);
    assert(idx < steps);
    value = tbl[idx];
    DPRINT("shutter->%d/%d\n", value.nom, value.denom);
    ret = pslr_set_shutter(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set shutter failed: %d\n", ret);
    }
}

static void iso_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    gdouble a;
    int idx;
    int ret;
    const int *tbl;
    int steps;

    if (!status_new) {
        return;
    }

    a = gtk_range_get_value(GTK_RANGE(scale));
    idx = rint(a);
    which_iso_table(status_new, &tbl, &steps);
    assert(idx >= 0);
    assert(idx <= steps);
    DPRINT("cam iso = %d\n", status_new->fixed_iso);
    DPRINT("iso->%d\n", tbl[idx]);
    /* If the known camera iso is same as new slider value, then the
     * slider change came from the camera (via init_controls), not
     * user change, and we should NOT send any new value to the
     * camera; for example the Fn menu will be exited if we do. */
    if (status_new->fixed_iso != tbl[idx]) {
        ret = pslr_set_iso(camhandle, tbl[idx], 0, 0);
        if (ret != PSLR_OK) {
            DPRINT("Set ISO failed: %d\n", ret);
        }
    }
}

static void ec_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    gdouble a;
    pslr_rational_t new_ec;
    int ret;
    int idx;
    const int *tbl;
    int steps;
    if (!status_new)
        return;

    a = gtk_range_get_value(GTK_RANGE(scale));
    which_ec_table(status_new, &tbl, &steps);
    idx = rint(a);
    DPRINT("EC->%d\n", idx);
    assert(idx >= 0);
    assert(idx < steps);
    new_ec.nom = tbl[idx];
    new_ec.denom = 10;
    if (status_new == NULL)
        return;
    if (status_new->ec.nom != new_ec.nom || status_new->ec.denom != new_ec.denom) {
        ret = pslr_set_ec(camhandle, new_ec);
        if (ret != PSLR_OK) {
            DPRINT("Set EC failed: %d\n", ret);
        }
    }
}

static void jpeg_resolution_combo_changed_cb(GtkCombo *combo, gpointer user_data)
{
    int ret;
    char *val = gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
    int megapixel = atoi( val );
    DPRINT("jpeg res active->%d\n", megapixel);
    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if (status_new == NULL || pslr_get_jpeg_resolution(camhandle, status_new->jpeg_resolution) != megapixel) {
        ret = pslr_set_jpeg_resolution(camhandle, megapixel);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG resolution failed.\n");
        }
    }
}

static void jpeg_quality_combo_changed_cb(GtkCombo *combo, gpointer user_data)
{
  pslr_jpeg_quality_t val = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    int ret;
    DPRINT("jpeg qual active->%d\n", val);
    assert(val >= 0);
    assert(val < PSLR_JPEG_QUALITY_MAX);

    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if (status_new == NULL || status_new->jpeg_quality != val) {
        ret = pslr_set_jpeg_quality(camhandle, val);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG quality failed.\n");
        }
    }
}

static void jpeg_image_mode_combo_changed_cb(GtkCombo *combo, gpointer user_data)
{
    pslr_jpeg_image_mode_t val = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    int ret;
    assert(val >= 0);
    assert(val < PSLR_JPEG_IMAGE_MODE_MAX);
    DPRINT("jpeg image_mode active->%d\n", val);
    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if (status_new == NULL || status_new->jpeg_image_mode != val) {
        ret = pslr_set_jpeg_image_mode(camhandle, val);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG image mode failed.\n");
        }
    }
}

static void jpeg_sharpness_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    int value = rint(gtk_range_get_value(GTK_RANGE(scale)));
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_sharpness(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG sharpness failed.\n");
    }
}

static void jpeg_contrast_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    int value = rint(gtk_range_get_value(GTK_RANGE(scale)));
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_contrast(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG contrast failed.\n");
    }
}

static void jpeg_hue_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    int value = rint(gtk_range_get_value(GTK_RANGE(scale)));
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_hue(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG hue failed.\n");
    }
}


static void jpeg_saturation_scale_value_changed_cb(GtkScale *scale, gpointer user_data)
{
    int value = rint(gtk_range_get_value(GTK_RANGE(scale)));
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_saturation(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG saturation failed.\n");
    }
}

void preview_icon_view_selection_changed_cb(GtkIconView *icon_view)
{
    GList *l;
    guint len;
    GtkWidget *pw;
    gboolean en;

    /* If nothing is selected, disable the action buttons. Otherwise
     * enable them. */

    l = gtk_icon_view_get_selected_items(icon_view);
    len = g_list_length(l);
    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
    en = len > 0;

    pw = glade_xml_get_widget(xml, "preview_save_as_button");
    gtk_widget_set_sensitive(pw, en);
//    pw = glade_xml_get_widget(xml, "preview_gimp_button");
//    gtk_widget_set_sensitive(pw, en);
    pw = glade_xml_get_widget(xml, "preview_delete_button");
    gtk_widget_set_sensitive(pw, en);
}

/*
 * Save the indicated buffer using the current UI file format
 * settings.  Updates the progress bar periodically & runs the GTK
 * main loop to show it.
 */
static void save_buffer(int bufno, const char *filename)
{
    int r;
    int fd;
    GtkWidget *pw;
    int quality;
    int resolution;
    int filefmt;
    pslr_buffer_type imagetype;
    uint8_t buf[65536];
    uint32_t length;
    uint32_t current;

    pw = glade_xml_get_widget(xml, "jpeg_quality_combo");
    quality = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));
    pw = glade_xml_get_widget(xml, "jpeg_resolution_combo");
    resolution = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));
    pw = glade_xml_get_widget(xml, "file_format_combo");
    filefmt = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));

    if (filefmt == USER_FILE_FORMAT_PEF) {
      imagetype = PSLR_BUF_PEF;
    } else if (filefmt == USER_FILE_FORMAT_DNG) {
      imagetype = PSLR_BUF_DNG;
    } else {
      imagetype = pslr_get_jpeg_buffer_type( camhandle, quality );
    }
    DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, resolution);

    r = pslr_buffer_open(camhandle, bufno, imagetype, resolution);
    if (r != PSLR_OK) {
        DPRINT("Could not open buffer: %d\n", r);
        return;
    }

    length = pslr_buffer_get_size(camhandle);
    current = 0;

    fd = open(filename, FILE_ACCESS, 0664);
    if (fd == -1) {
        perror("could not open target");
        return;
    }

    pw = glade_xml_get_widget(xml, "download_progress");

    while (1) {
        uint32_t bytes;
        bytes = pslr_buffer_read(camhandle, buf, sizeof(buf));
        //printf("Read %d bytes\n", bytes);
        if (bytes == 0)
            break;
        write(fd, buf, bytes);
        current += bytes;
        gtk_progress_bar_update(GTK_PROGRESS_BAR(pw), (gdouble) current / (gdouble) length);
        /* process pending events */
        while (gtk_events_pending())
            gtk_main_iteration();
    }
    close(fd);
    pslr_buffer_close(camhandle);
}

void preview_save_as(GtkButton *widget)
{
    GtkWidget *pw, *icon_view;
    GList *l, *i;
    GtkProgressBar *pbar;
    DPRINT("preview save as\n");
    icon_view = glade_xml_get_widget(xml, "preview_icon_view");
    l = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    pbar = (GtkProgressBar *) glade_xml_get_widget(xml, "download_progress");
    for (i=g_list_first(l); i; i=g_list_next(i)) {
        GtkTreePath *p;
        int d, *pi;
        int res;
        p = i->data;
        d = gtk_tree_path_get_depth(p);
        DPRINT("Tree depth = %d\n", d);
        pi = gtk_tree_path_get_indices(p);
        DPRINT("Selected item = %d\n", *pi);

        pw = glade_xml_get_widget(xml, "save_as_dialog");

        res = gtk_dialog_run(GTK_DIALOG(pw));
        DPRINT("Run dialog -> %d\n", res);
        gtk_widget_hide(pw);
        if (res > 0) {
            char *filename;
            filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pw));
            DPRINT("Save to: %s\n", filename);
            gtk_progress_bar_set_text(pbar, filename);
            save_buffer(*pi, filename);
            gtk_progress_bar_set_text(pbar, NULL);
        }
    }

    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
}

void preview_delete_button_clicked_cb(GtkButton *widget)
{
    GtkWidget *icon_view;
    GList *l, *i;
    int ret;
    int retry;
    pslr_status st;

    DPRINT("preview delete\n");

    icon_view = glade_xml_get_widget(xml, "preview_icon_view");
    l = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    for (i=g_list_first(l); i; i=g_list_next(i)) {
        GtkTreePath *p;
        int d, *pi;
        p = i->data;
        d = gtk_tree_path_get_depth(p);
        DPRINT("Tree depth = %d\n", d);
        pi = gtk_tree_path_get_indices(p);
        DPRINT("Selected item = %d\n", *pi);

        // needed? : g_object_unref(thumbpixbufs[i])?
        set_preview_icon(*pi, NULL);

        ret = pslr_delete_buffer(camhandle, *pi);
        if (ret != PSLR_OK)
            DPRINT("Could not delete buffer %d: %d\n", *pi, ret);
        for (retry=0; retry<5; retry++) {
            pslr_get_status(camhandle, &st);
            if ((st.bufmask & (1<<*pi))==0)
                break;
            DPRINT("Buffer not gone - retry\n");
        }
    }

    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
}

static void file_format_combo_changed_cb(GtkCombo *combo, gpointer user_data)
{
    int val = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    switch (val) {
    case USER_FILE_FORMAT_PEF:
        pslr_set_image_format(camhandle, PSLR_IMAGE_FORMAT_RAW);
        pslr_set_raw_format(camhandle, PSLR_RAW_FORMAT_PEF);
        break;
    case USER_FILE_FORMAT_DNG:
        pslr_set_image_format(camhandle, PSLR_IMAGE_FORMAT_RAW);
        pslr_set_raw_format(camhandle, PSLR_RAW_FORMAT_DNG);
        break;
    case USER_FILE_FORMAT_JPEG:
        pslr_set_image_format(camhandle, PSLR_IMAGE_FORMAT_JPEG);
        break;
    default:
        return;
    }
}

static void user_mode_combo_changed_cb(GtkCombo *combo, gpointer user_data)
{
    pslr_exposure_mode_t val = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (!status_new)
        return;
    assert(val >= 0);
    assert(val < PSLR_EXPOSURE_MODE_MAX);

    pslr_set_exposure_mode(camhandle, val);
}

/* ----------------------------------------------------------------------- */

static void which_iso_table(pslr_status *st, const int **table, int *steps)
{
    if (st->custom_sensitivity_steps == PSLR_CUSTOM_SENSITIVITY_STEPS_1EV) { 
        *table = iso_tbl_1;
        *steps = sizeof(iso_tbl_1)/sizeof(iso_tbl_1[0]);
    }else if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = iso_tbl_1_2;
        *steps = sizeof(iso_tbl_1_2)/sizeof(iso_tbl_1_2[0]);
    } else {
        *table = iso_tbl_1_3;
        *steps = sizeof(iso_tbl_1_3)/sizeof(iso_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}

static void which_ec_table(pslr_status *st, const int **table, int *steps)
{
    if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = ec_tbl_1_2;
        *steps = sizeof(ec_tbl_1_2)/sizeof(ec_tbl_1_2[0]);
    } else {
        *table = ec_tbl_1_3;
        *steps = sizeof(ec_tbl_1_3)/sizeof(ec_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}

static void which_shutter_table(pslr_status *st, const pslr_rational_t **table, int *steps)
{
    if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = shutter_tbl_1_2;
        *steps = sizeof(shutter_tbl_1_2)/sizeof(shutter_tbl_1_2[0]);
    } else {
        *table = shutter_tbl_1_3;
        *steps = sizeof(shutter_tbl_1_3)/sizeof(shutter_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}


static user_file_format file_format(pslr_status *st)
{
    int rawfmt = st->raw_format;
    int imgfmt = st->image_format;
    if (imgfmt == PSLR_IMAGE_FORMAT_JPEG) {
        return USER_FILE_FORMAT_JPEG;
    } else {
        if (rawfmt == PSLR_RAW_FORMAT_PEF) {
            return USER_FILE_FORMAT_PEF;
        } else {
            return USER_FILE_FORMAT_DNG;
	}
    }
}

int main()
{
    int r;
    r = common_init();
    if (r < 0) {
        printf("Could not initialize device.\n");
    }
    return 0;
}
