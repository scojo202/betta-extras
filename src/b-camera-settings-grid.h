/*
 * b-camera-settings-grid.h :
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

#pragma once

#include <gtk/gtk.h>
#include <arv.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BCameraSettingsGrid,b_camera_settings_grid,B,CAMERA_SETTINGS_GRID,GtkGrid)

#define B_TYPE_CAMERA_SETTINGS_GRID                  (b_camera_settings_grid_get_type ())

void b_camera_settings_grid_set_camera(BCameraSettingsGrid *grid, ArvCamera *cam);
void b_camera_settings_grid_get_region(BCameraSettingsGrid *ivw, int *x, int *y, int *w, int *h);

G_END_DECLS
