/*
 * b-hdf.c :
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

#include <gio/gio.h>
#include <b-hdf.h>
#include "data/b-struct.h"

#define DEFLATE_LEVEL 5

/**
 * SECTION: b-hdf
 * @short_description: Functions for saving and loading from HDF5 files
 *
 * Utility functions for saving to and loading from HDF5 files.
 *
 **/

struct _BFile {
  GObject	 base;
  hid_t handle;
  gboolean write;
};

G_DEFINE_TYPE (BFile, b_file, G_TYPE_OBJECT);

static
void b_file_finalize (GObject *obj)
{
  BFile *f = (BFile *) obj;
  H5Fclose(f->handle);
}

static
void b_file_class_init(BFileClass *class)
{
  GObjectClass *gobj_class = (GObjectClass *) class;
  gobj_class->finalize = b_file_finalize;
}

static
void b_file_init(BFile *file)
{

}

/**
 * b_file_open_for_writing:
 * @filename: filename
 * @overwrite: whether to overwrite the file if it already exists
 * @err: (nullable): a #GError or %NULL
 *
 * Create an HDF5 file for writing.
 *
 * Returns: (transfer full): The #BFile object.
 **/
BFile * b_file_open_for_writing(const gchar * filename, gboolean overwrite, GError **err)
{
  /* make sure file doesn't already exist */
  GFile *file = g_file_new_for_path(filename);
  gboolean exists = g_file_query_exists(file, NULL);
  g_object_unref(file);
  if (exists) {
    g_set_error(err, G_IO_ERROR, G_IO_ERROR_EXISTS,
                "file already exists: %s", filename);
    if (!overwrite)
      return NULL;
  }
  hid_t hfile = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  BFile *f = g_object_new(B_TYPE_FILE,NULL);
  f->handle = hfile;
  f->write = TRUE;
  return f;
}

/**
 * b_file_open_for_reading:
 * @filename: filename
 * @err: (nullable): a #GError or %NULL
 *
 * Create an HDF5 file to be read.
 *
 * Returns: (transfer full): The #BFile object.
 **/

BFile * b_file_open_for_reading(const gchar *filename, GError **err)
{
  /* make sure file exists */
  GFile *file = g_file_new_for_path(filename);
  gboolean exists = g_file_query_exists(file, NULL);
  g_object_unref(file);
  if (!exists) {
    g_set_error(err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                "file not found: %s", filename);
    return NULL;
  }
  /* make sure file is not corrupted */
  htri_t r = H5Fis_hdf5(filename);
  if(r<=0) {
    g_set_error(err, G_IO_ERROR, G_IO_ERROR_FAILED,
                "file is not HDF5 format: %s", filename);
    return NULL;
  }
  hid_t hfile = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
  BFile *f = g_object_new(B_TYPE_FILE,NULL);
  f->handle = hfile;
  f->write = FALSE;
  return f;
}

/**
 * b_file_get_handle: (skip)
 * @f: a #BFile
 *
 **/
hid_t b_file_get_handle(BFile *f)
{
  return f->handle;
}

/**
 * b_hdf5_create_group: (skip)
 * @id: file handle
 * @name: group name
 *
 * Create an HDF5 group.
 **/

hid_t b_hdf5_create_group(hid_t id, const gchar * name)
{
  g_return_val_if_fail(id != 0, 0);
  return H5Gcreate(id, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
}

/**
 * b_vector_attach_h5: (skip)
 * @v: #BVector
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Add a vector to an HDF5 group.
 **/

void b_vector_attach_h5(BVector * v, hid_t group_id, const gchar * data_name)
{
  g_return_if_fail(B_IS_VECTOR(v));
  g_return_if_fail(group_id != 0);
  hsize_t dims[1] = { b_vector_get_len(v) };
  if (dims[0] == 0) {
    g_warning("skipping HDF5 save due to zero length vector");
    return;
  }

  hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
  hid_t plist_id = H5Pcreate(H5P_DATASET_CREATE);

  H5Pset_chunk(plist_id, 1, dims);
  H5Pset_deflate(plist_id, DEFLATE_LEVEL);

  hid_t id =
    H5Dcreate2(group_id, data_name, H5T_NATIVE_DOUBLE, dataspace_id,
               H5P_DEFAULT, plist_id, H5P_DEFAULT);
  const double *data = b_vector_get_values(v);
  H5Dwrite(id, H5T_NATIVE_DOUBLE, dataspace_id, dataspace_id, H5P_DEFAULT,
            data);

  H5Sclose(dataspace_id);

  H5Pclose(plist_id);
  H5Dclose(id);
}

/**
 * b_vector_attach_attr_h5: (skip)
 * @v: #BVector
 * @group_id: HDF5 dataset or group
 * @obj_name: name
 * @attr_name: attribute name
 *
 * Add a vector to an HDF5 group as an attribute.
 **/

void b_vector_attach_attr_h5(BVector * v, hid_t group_id,
                             const gchar * obj_name, const gchar * attr_name)
{
  g_return_if_fail(B_IS_VECTOR(v));
  g_return_if_fail(group_id != 0);
  int l = b_vector_get_len(v);
  const double *d = b_vector_get_values(v);
  if (l == 0) {
    g_warning ("skipping HDF5 save to attribute due to zero length vector");
    return;
  }

  H5LTset_attribute_double(group_id, obj_name, attr_name, d, l);
}

/**
 * y_matrix_attach_h5: (skip)
 * @m: #YMatrix
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Add a matrix to an HDF5 group.
 **/
void b_matrix_attach_h5(BMatrix * m, hid_t group_id, const gchar * data_name)
{
  g_return_if_fail(B_IS_MATRIX(m));
  g_return_if_fail(group_id != 0);
  hsize_t dims[2] = { b_matrix_get_rows(m), b_matrix_get_columns(m) };

  hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
  hid_t plist_id = H5Pcreate(H5P_DATASET_CREATE);

  H5Pset_chunk(plist_id, 2, dims);
  H5Pset_deflate(plist_id, DEFLATE_LEVEL);

  hid_t id =
    H5Dcreate2(group_id, data_name, H5T_NATIVE_DOUBLE, dataspace_id,
               H5P_DEFAULT, plist_id, H5P_DEFAULT);
  H5Dwrite(id, H5T_NATIVE_DOUBLE, dataspace_id, dataspace_id, H5P_DEFAULT,
           b_matrix_get_values(m));

  H5Sclose(dataspace_id);

  H5Pclose(plist_id);
  H5Dclose(id);
}

static
void save_func(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *name = (gchar *) key;
  g_message("save %s", name);
  BData *d = B_DATA(value);
  hid_t *subgroup_id = (hid_t *) user_data;
  b_data_attach_h5(d, *subgroup_id, name);
}

/**
 * b_file_attach_data:
 * @f: #BFile
 * @data_name: path
 * @d: #YData
 *
 * Add a YData object to a #BFile.
 **/
void b_file_attach_data(BFile *f, const gchar *data_name, BData *d)
{
  g_return_if_fail(f->write);
  b_data_attach_h5(d,f->handle,data_name);
}

/**
 * y_data_attach_h5: (skip)
 * @d: #YData
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Add a YData object to an HDF5 group.
 **/
void b_data_attach_h5(BData * d, hid_t group_id, const gchar * data_name)
{
  hid_t subgroup_id;
  g_return_if_fail(B_IS_DATA(d));
  g_return_if_fail(group_id != 0);
  char n = b_data_get_n_dimensions(d);
  switch (n) {
  case -1:
    if (data_name)
      subgroup_id =
        H5Gcreate(group_id, data_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    else
      subgroup_id = group_id;
    b_struct_foreach(B_STRUCT(d), save_func, &subgroup_id);
    if (data_name != NULL)
      H5Gclose(subgroup_id);
    break;
  case 0:
    g_warning("scalar save to h5 not implemented");
    break;
  case 1:
    b_vector_attach_h5(B_VECTOR(d), group_id, data_name);
    break;
  case 2:
    b_matrix_attach_h5(B_MATRIX(d), group_id, data_name);
    break;
  default:
    g_warning("number of dimensions %d not supported", n);
    return;
  }
}

/**
 * b_vector_from_h5: (skip)
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Read a vector from an HDF5 group.
 *
 * Returns: (transfer full): The vector.
 **/
BData *b_vector_from_h5(hid_t group_id, const gchar * data_name)
{
  g_return_val_if_fail(group_id != 0, NULL);
  htri_t exists = H5Lexists(group_id, data_name, H5P_DEFAULT);
  if (exists == 0) {
    return NULL;
  }
  hid_t dataset_h5 = H5Dopen(group_id, data_name, H5P_DEFAULT);
  if (dataset_h5 < 0) {
    return NULL;
  }
  hid_t dspace_id = H5Dget_space(dataset_h5);
  int rank;
  hsize_t *current_dims;
  hsize_t *max_dims;
  rank = H5Sget_simple_extent_ndims(dspace_id);
  current_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  max_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  H5Sget_simple_extent_dims(dspace_id, current_dims, max_dims);
  g_return_val_if_fail(rank == 1, NULL);
  g_return_val_if_fail(current_dims[0] > 0, NULL);
  double *d = g_new(double, current_dims[0]);
  H5Dread(dataset_h5, H5T_NATIVE_DOUBLE, H5S_ALL, dspace_id, H5P_DEFAULT, d);
  BData *y = b_val_vector_new(d, current_dims[0], g_free);
  H5Sclose(dspace_id);
  H5Dclose(dataset_h5);
  return y;
}

/**
 * y_matrix_from_h5: (skip)
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Read a matrix from an HDF5 group.
 *
 * Returns: (transfer full): The matrix.
 **/
BData *b_matrix_from_h5(hid_t group_id, const gchar * data_name)
{
  g_return_val_if_fail(group_id != 0, NULL);
  htri_t exists = H5Lexists(group_id, data_name, H5P_DEFAULT);
  if (exists == 0) {
    return NULL;
  }
  hid_t dataset_h5 = H5Dopen(group_id, data_name, H5P_DEFAULT);
  if (dataset_h5 < 0) {
    return NULL;
  }
  hid_t dspace_id = H5Dget_space(dataset_h5);
  int rank;
  hsize_t *current_dims;
  hsize_t *max_dims;
  rank = H5Sget_simple_extent_ndims(dspace_id);
  current_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  max_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  H5Sget_simple_extent_dims(dspace_id, current_dims, max_dims);
  g_return_val_if_fail(rank == 2, NULL);
  g_return_val_if_fail(current_dims[0] > 0, NULL);
  g_return_val_if_fail(current_dims[1] > 0, NULL);
  double *d = g_new(double, current_dims[0] * current_dims[1]);
  H5Dread(dataset_h5, H5T_NATIVE_DOUBLE, H5S_ALL, dspace_id, H5P_DEFAULT, d);
  BData *y =
    b_val_matrix_new(d, current_dims[0], current_dims[1], g_free);
  H5Sclose(dspace_id);
  H5Dclose(dataset_h5);
  return y;
}

/**
 * b_val_vector_replace_h5: (skip)
 * @v: BVectorVal
 * @group_id: HDF5 group
 * @data_name: name
 *
 * Load contents of HDF5 dataset into a vector, replacing its contents.
 **/
void b_val_vector_replace_h5(BValVector * v, hid_t group_id,
                              const gchar * data_name)
{
  g_return_if_fail(group_id != 0);
  htri_t exists = H5Lexists(group_id, data_name, H5P_DEFAULT);
  if (exists == 0) {
    g_warning("H5 dataset doesn't exist");
    return;
  }
  hid_t dataset_h5 = H5Dopen(group_id, data_name, H5P_DEFAULT);
  hid_t dspace_id = H5Dget_space(dataset_h5);
  int rank;
  hsize_t *current_dims;
  hsize_t *max_dims;
  rank = H5Sget_simple_extent_ndims(dspace_id);
  current_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  max_dims = (hsize_t *) g_malloc(rank * sizeof(hsize_t));
  H5Sget_simple_extent_dims(dspace_id, current_dims, max_dims);
  g_return_if_fail(rank == 1);
  g_return_if_fail(current_dims[0] > 0);
  double *d = g_new(double, current_dims[0]);
  H5Dread(dataset_h5, H5T_NATIVE_DOUBLE, H5S_ALL, dspace_id, H5P_DEFAULT, d);
  H5Sclose(dspace_id);
  H5Dclose(dataset_h5);
  b_val_vector_replace_array(v, d, current_dims[0], g_free);
}
