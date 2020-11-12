/*
 * b-camera-settings-grid.c :
 *
 * Copyright (C) 2019 Scott O. Johnson (scojo202@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <string.h>
#include "b-camera-settings-grid.h"

struct _BCameraSettingsGrid
{
  GtkGrid base;
  ArvCamera *cam;
  BArvSource *source;
  GtkSpinButton *exposure_sb;
  GtkSpinButton *frame_rate_sb;
  GtkAdjustment *x_adjustment;
  GtkAdjustment *y_adjustment;
  GtkAdjustment *width_adjustment;
  GtkAdjustment *height_adjustment;
  GtkToggleButton *external_trigger_button;
  GtkButton *set_region_button, *use_full_region_button;
  gulong expo_handler, fr_handler, t_handler;
};

G_DEFINE_TYPE (BCameraSettingsGrid, b_camera_settings_grid, GTK_TYPE_GRID)

static void
b_camera_settings_grid_class_init (BCameraSettingsGridClass *klass)
{
  //GObjectClass *object_class = G_OBJECT_CLASS (klass);

  //object_class->finalize = grid_finalize;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/com/github/scojo202/betta/b-camera-settings-grid.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, exposure_sb);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, frame_rate_sb);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, external_trigger_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, set_region_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, use_full_region_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, x_adjustment);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, y_adjustment);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, width_adjustment);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BCameraSettingsGrid, height_adjustment);
}

static void
on_expo_changed(GtkSpinButton *sb, gpointer user_data)
{
  double val = gtk_spin_button_get_value(sb);
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  arv_camera_set_exposure_time(g->cam,val,NULL);
}

static void
on_fr_changed(GtkSpinButton *sb, gpointer user_data)
{
  double val = gtk_spin_button_get_value(sb);
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  arv_camera_set_frame_rate(g->cam,val,NULL);
}

static void
on_ext_trigger_toggled(GtkToggleButton *tb, gpointer user_data)
{
  gboolean a = gtk_toggle_button_get_active(tb);

  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  if (a) {
    arv_camera_stop_acquisition(g->cam,NULL);
    arv_camera_set_trigger_source(g->cam,"Line1",NULL);
    arv_camera_set_trigger(g->cam,"Line1",NULL);
    arv_camera_start_acquisition(g->cam,NULL);
  }
  else {
    arv_camera_stop_acquisition(g->cam,NULL);
    arv_camera_set_trigger_source(g->cam,"Freerun",NULL);
    arv_camera_set_trigger(g->cam,"Freerun",NULL);
    arv_camera_set_frame_rate (g->cam, 5.0,NULL);
    arv_camera_start_acquisition(g->cam,NULL);
  }
}

static void
on_set_region_clicked (GtkButton *b,
                 gpointer       user_data)
{
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  
  gint swidth, sheight;
  arv_camera_get_sensor_size(g->cam, &swidth, &sheight,NULL);
  
  int active_x0;
  int active_y0;
  int active_width;
  int active_height;
  
  b_camera_settings_grid_get_region(g,&active_x0,&active_y0,&active_width,&active_height);
    
  active_width = MIN(active_width,swidth-active_x0);
  active_height = MIN(active_height,sheight-active_y0);

  arv_camera_stop_acquisition(g->cam,NULL);

  arv_camera_set_region (g->cam, active_x0, active_y0, active_width, active_height,NULL);

  b_arv_source_create_stream(g->source);

  arv_camera_start_acquisition(g->cam,NULL);
}

static void
on_full_region_clicked (GtkButton *b,
                        gpointer   user_data)
{
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);

  gint swidth, sheight;
  arv_camera_get_sensor_size(g->cam, &swidth, &sheight,NULL);

  arv_camera_stop_acquisition(g->cam,NULL);

  arv_camera_set_region (g->cam, 0, 0, swidth, sheight,NULL);

  b_arv_source_create_stream(g->source);

  arv_camera_start_acquisition(g->cam,NULL);
}

static void
b_camera_settings_grid_init(BCameraSettingsGrid *obj)
{
  gtk_widget_init_template (GTK_WIDGET (obj));


}

void b_camera_settings_grid_set_camera(BCameraSettingsGrid *g, ArvCamera *cam)
{
  g_return_if_fail(B_IS_CAMERA_SETTINGS_GRID(g));
  g_return_if_fail(ARV_IS_CAMERA(cam));
  g->cam = g_object_ref(cam);

  /* find exposure time limits and current exposure time */
  double emin,emax;
  arv_camera_get_exposure_time_bounds(g->cam,&emin,&emax,NULL);

  double e = arv_camera_get_exposure_time(g->cam,NULL);
  /* emax is typically something ridiculous -- default to 1 s */
  /* TODO: use log scale for exposure time */
  double step = (1e6-emin)/100;

  GtkAdjustment *adj = gtk_spin_button_get_adjustment(g->exposure_sb);
  gtk_adjustment_configure(adj,e,emin,1e6,step,step*10,step*10);
  g->expo_handler = g_signal_connect(g->exposure_sb,"value-changed",G_CALLBACK(on_expo_changed),g);

  /* find frame rate limits and current frame rate */
  arv_camera_get_frame_rate_bounds(g->cam,&emin,&emax,NULL);
  e = arv_camera_get_frame_rate(g->cam,NULL);
  step = (emax-emin)/100;
  adj = gtk_spin_button_get_adjustment(g->frame_rate_sb);
  gtk_adjustment_configure(adj,e,emin,emax,step,step*10,step*10);
  g->fr_handler = g_signal_connect(g->frame_rate_sb,"value-changed",G_CALLBACK(on_fr_changed),g);

  /* trigger source */
  const gchar *source = arv_camera_get_trigger_source(g->cam,NULL);
  if(!strcmp(source,"Freerun"))
    gtk_toggle_button_set_active(g->external_trigger_button,FALSE);
  else
    gtk_toggle_button_set_active(g->external_trigger_button,TRUE);

  g->t_handler = g_signal_connect(g->external_trigger_button,"toggled", G_CALLBACK(on_ext_trigger_toggled), g);
  
  /* set values and limits for region adjustments */
  gint swidth, sheight;
  arv_camera_get_sensor_size(g->cam, &swidth, &sheight,NULL);
  gint x,y,width,height;
  arv_camera_get_region(g->cam,&x,&y,&width,&height,NULL);
  gtk_adjustment_configure(g->x_adjustment,x,0,swidth,1,10,0);
  gtk_adjustment_configure(g->y_adjustment,y,0,sheight,1,10,0);
  gtk_adjustment_configure(g->width_adjustment,width,1,swidth,1,10,0);
  gtk_adjustment_configure(g->height_adjustment,height,1,sheight,1,10,0);

  g_signal_connect(g->set_region_button, "clicked", G_CALLBACK (on_set_region_clicked), g);
  g_signal_connect(g->use_full_region_button, "clicked", G_CALLBACK (on_full_region_clicked), g);
}

void b_camera_settings_grid_set_source(BCameraSettingsGrid *g,
                                       BArvSource          *s)
{
  g_return_if_fail(B_IS_CAMERA_SETTINGS_GRID(g));
  g->source = g_object_ref(s);
}

void b_camera_settings_grid_get_region(BCameraSettingsGrid *g, int *x, int *y, int *w, int *h)
{
  g_return_if_fail(B_IS_CAMERA_SETTINGS_GRID(g));
  
  int active_x0 = (int) gtk_adjustment_get_value(g->x_adjustment);
  int active_y0 = (int) gtk_adjustment_get_value(g->y_adjustment);
  int active_width = (int) gtk_adjustment_get_value(g->width_adjustment);
  int active_height = (int) gtk_adjustment_get_value(g->height_adjustment);
  
  if(x)
    *x = active_x0;
  if(y)
    *y = active_y0;
  if(w)
    *w = active_width;
  if(h)
    *h = active_height;
}

gboolean b_camera_settings_grid_get_region_modified(BCameraSettingsGrid *g)
{
  int active_x0 = (int) gtk_adjustment_get_value(g->x_adjustment);
  int active_y0 = (int) gtk_adjustment_get_value(g->y_adjustment);
  int active_width = (int) gtk_adjustment_get_value(g->width_adjustment);
  int active_height = (int) gtk_adjustment_get_value(g->height_adjustment);

  gint x,y,width,height;
  arv_camera_get_region(g->cam,&x,&y,&width,&height,NULL);

  return ((x!=active_x0) || (y!=active_y0) || (width!=active_width) || (height!=active_height));
}
