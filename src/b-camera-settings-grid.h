#pragma once

#include <gtk/gtk.h>
#include <arv.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BCameraSettingsGrid,b_camera_settings_grid,B,CAMERA_SETTINGS_GRID,GtkGrid)

#define B_TYPE_CAMERA_SETTINGS_GRID                  (b_camera_settings_grid_get_type ())

void b_camera_settings_grid_set_camera(BCameraSettingsGrid *grid, ArvCamera *cam);

G_END_DECLS
