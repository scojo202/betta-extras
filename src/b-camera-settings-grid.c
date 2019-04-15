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
  GtkSpinButton *exposure_sb;
  GtkSpinButton *frame_rate_sb;
  GtkToggleButton *external_trigger_button;
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
}

static void
on_expo_changed(GtkSpinButton *sb, gpointer user_data)
{
  double val = gtk_spin_button_get_value(sb);
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  arv_camera_set_exposure_time(g->cam,val);
}

static void
on_fr_changed(GtkSpinButton *sb, gpointer user_data)
{
  double val = gtk_spin_button_get_value(sb);
  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  arv_camera_set_frame_rate(g->cam,val);
}

static void
on_ext_trigger_toggled(GtkToggleButton *tb, gpointer user_data)
{
  gboolean a = gtk_toggle_button_get_active(tb);

  BCameraSettingsGrid *g = B_CAMERA_SETTINGS_GRID(user_data);
  if (a) {
    arv_camera_stop_acquisition(g->cam);
    arv_camera_set_trigger_source(g->cam,"Line1");
    arv_camera_set_trigger(g->cam,"Line1");
    arv_camera_start_acquisition(g->cam);
  }
  else {
    arv_camera_stop_acquisition(g->cam);
    arv_camera_set_trigger_source(g->cam,"Freerun");
    arv_camera_set_trigger(g->cam,"Freerun");
    arv_camera_set_frame_rate (g->cam, 5.0);
    arv_camera_start_acquisition(g->cam);
  }
}

static void
b_camera_settings_grid_init(BCameraSettingsGrid *obj)
{
  gtk_widget_init_template (GTK_WIDGET (obj));


}

void b_camera_settings_grid_set_camera(BCameraSettingsGrid *grid, ArvCamera *cam)
{
  grid->cam = g_object_ref(cam);

  /* find exposure time limits and current exposure time */
  double emin,emax;
  arv_camera_get_exposure_time_bounds(grid->cam,&emin,&emax);

  double e = arv_camera_get_exposure_time(grid->cam);
  /* emax is typically something ridiculous -- default to 1 s */
  /* TODO: use log scale for exposure time */
  double step = (1e6-emin)/100;

  GtkAdjustment *adj = gtk_spin_button_get_adjustment(grid->exposure_sb);
  gtk_adjustment_configure(adj,e,emin,1e6,step,step*10,step*10);
  grid->expo_handler = g_signal_connect(grid->exposure_sb,"value-changed",G_CALLBACK(on_expo_changed),grid);

  /* find frame rate limits and current frame rate */
  arv_camera_get_frame_rate_bounds(grid->cam,&emin,&emax);
  e = arv_camera_get_frame_rate(grid->cam);
  step = (emax-emin)/100;
  adj = gtk_spin_button_get_adjustment(grid->frame_rate_sb);
  gtk_adjustment_configure(adj,e,emin,emax,step,step*10,step*10);
  grid->fr_handler = g_signal_connect(grid->frame_rate_sb,"value-changed",G_CALLBACK(on_fr_changed),grid);

  const gchar *source = arv_camera_get_trigger_source(grid->cam);
  if(!strcmp(source,"Freerun"))
    gtk_toggle_button_set_active(grid->external_trigger_button,FALSE);
  else
    gtk_toggle_button_set_active(grid->external_trigger_button,TRUE);

  grid->t_handler = g_signal_connect(grid->external_trigger_button,"toggled", G_CALLBACK(on_ext_trigger_toggled), grid);
}
