/*
 * b-hdf.h :
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

#pragma once

#include <hdf5.h>
#include <hdf5_hl.h>
#include <data/b-data-class.h>
#include <data/b-data-simple.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BFile,b_file,B,FILE,GObject)

#define B_TYPE_FILE  (b_file_get_type ())

BFile * b_file_open_for_writing(const gchar * filename, gboolean overwrite, GError **err);
BFile * b_file_open_for_reading(const gchar *filename, GError **err);
hid_t b_file_get_handle(BFile *f);
void b_file_attach_data(BFile *f, const gchar *data_name, BData *d);

hid_t b_hdf5_create_group(hid_t id, const gchar *name);
#define b_hdf5_close_group(id) H5Gclose(id);

void b_data_attach_h5(BData *d, hid_t group_id, const gchar *data_name);
//BData *b_data_from_h5(hid_t group_id, const gchar *data_name);

void b_vector_attach_h5 (BVector *v, hid_t group_id, const gchar *data_name);
void b_vector_attach_attr_h5 (BVector *v, hid_t group_id, const gchar *obj_name, const gchar *attr_name);

void b_matrix_attach_h5 (BMatrix *m, hid_t group_id, const gchar *data_name);

BData *b_vector_from_h5 (hid_t group_id, const gchar *data_name);
BData *b_matrix_from_h5 (hid_t group_id, const gchar *data_name);
void b_val_vector_replace_h5 (BValVector *v, hid_t group_id, const gchar *data_name);

G_END_DECLS
