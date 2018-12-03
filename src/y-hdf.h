/*
 * y-hdf.h :
 *
 * Copyright (C) 2016 Scott O. Johnson (scojo202@gmail.com)
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

#ifndef Y_HDF_H
#define Y_HDF_H

#include <hdf5.h>
#include <hdf5_hl.h>
#include <y-data-class.h>
#include <y-data-simple.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(YFile,y_file,Y,FILE,GObject)

#define Y_TYPE_FILE  (y_file_get_type ())

YFile * y_file_open_for_writing(const gchar * filename, gboolean overwrite, GError **err);
YFile * y_file_open_for_reading(const gchar *filename, GError **err);
hid_t y_file_get_handle(YFile *f);
void y_file_attach_data(YFile *f, const gchar *data_name, YData *d);

hid_t y_hdf5_create_group(hid_t id, const gchar *name);
#define y_hdf5_close_group(id) H5Gclose(id);

void y_data_attach_h5(YData *d, hid_t group_id, const gchar *data_name);
//YData *y_data_from_h5(hid_t group_id, const gchar *data_name);

void y_vector_attach_h5 (YVector *v, hid_t group_id, const gchar *data_name);
void y_vector_attach_attr_h5 (YVector *v, hid_t group_id, const gchar *obj_name, const gchar *attr_name);

void y_matrix_attach_h5 (YMatrix *m, hid_t group_id, const gchar *data_name);

YData *y_vector_from_h5 (hid_t group_id, const gchar *data_name);
YData *y_matrix_from_h5 (hid_t group_id, const gchar *data_name);
void y_val_vector_replace_h5 (YValVector *v, hid_t group_id, const gchar *data_name);

G_END_DECLS

#endif
