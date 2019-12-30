/*
 * b-image.c :
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
#include <png.h>
#include <string.h>
#include "b-image.h"

static
G_GNUC_MALLOC guchar * char_array_calloc(size_t n)
{
  g_return_val_if_fail(n > 0, NULL);
  g_return_val_if_fail(n < 50000000, NULL);
  guchar *array = g_new0(guchar, n);
  return array;
}

static
G_GNUC_MALLOC guint16 * short_array_calloc(size_t n)
{
  g_return_val_if_fail(n > 0, NULL);
  g_return_val_if_fail(n < 50000000, NULL);
  guint16 *array = g_new0(guint16, n);
  return array;
}

/* coordinates: r - row and c - column */

BImage *b_image_new(guchar bytes, guint32 r, guint32 c)
{
  g_return_val_if_fail(bytes>0, NULL);
  g_return_val_if_fail(bytes<=2, NULL);
  BImage *f = g_slice_new0(BImage);
  f->bytes = bytes;
  if (r > 0 && c > 0) {
    if(bytes==1) {
      f->data = char_array_calloc(r * c);
    }
    if(bytes==2) {
      f->data = short_array_calloc(r * c);
    }
    f->nrow = r;
    f->ncol = c;
  }
  return f;
}

BImage *b_image_copy(const BImage * f)
{
    BImage *nf = b_image_new(f->bytes, f->nrow, f->ncol);
    if(f->bytes==1)
      memcpy(nf->data, f->data, sizeof(guchar) * (f->nrow) * (f->ncol));
    if(f->bytes==2)
      memcpy(nf->data, f->data, sizeof(guint16) * (f->nrow) * (f->ncol));
    nf->num = f->num;
    nf->timestamp = f->timestamp;
    return nf;
}

void b_image_free(BImage * f)
{
    g_return_if_fail(f);
    g_clear_pointer(&f->data,g_free);
    g_slice_free(BImage, f);
}

guint16 b_image_max(const BImage * f, guint32 * c, guint32 * r)
{
  const guint32 nrow = f->nrow;
  const guint32 ncol = f->ncol;
  guint32 i, j, k;
  guint16 m = 0;
  if (r && c) {
    *c = 0;
    *r = 0;
    k = 0;
  }
  if(f->bytes==1) {
    const guchar *data = (guchar *) f->data;
    if (r && c) {
      for (j = 0; j < nrow; j++) {
        for (i = 0; i < ncol; i++) {
          if (data[k] > m) {
            m = data[k];
            *r = j;
            *c = i;
          }
          k++;
        }
      }
    } else {
      for (j = 0; j < nrow * ncol; j++) {
        m = MAX(m, data[j]);
      }
    }
  }
  if(f->bytes==2) {
    const guint16 *data = (guint16 *) f->data;
    if (r && c) {
      for (j = 0; j < nrow; j++) {
        for (i = 0; i < ncol; i++) {
          if (data[k] > m) {
            m = data[k];
            *r = j;
            *c = i;
          }
          k++;
        }
      }
    } else {
      for (j = 0; j < nrow * ncol; j++) {
        m = MAX(m, data[j]);
      }
    }
  }
  return m;
}

typedef struct {
    BImage *f;
    char *filename;
} PNGSaveData;

static
void png_data_free(gpointer task_data)
{
    PNGSaveData *d = (PNGSaveData *) task_data;
    b_image_free(d->f);
    g_free(d->filename);
    g_slice_free(PNGSaveData, d);
}

double *frame_to_double_array(const BImage * f)
{
    int i;
    double *d = g_malloc(sizeof(double) * f->ncol * f->nrow);
    guint16 *id = (guint16 *) f->data;
    for (i = 0; i < f->ncol * f->nrow; i++) {
	d[i] = (double) id[i];
    }
    return d;
}

static
void frame_save_to_png_callback(GObject * obj, GAsyncResult * res,
				gpointer userdata)
{
}

static
void frame_save_thread(GTask * task, gpointer source_object,
		       gpointer task_data, GCancellable * cancel)
{
    PNGSaveData *d = (PNGSaveData *) task_data;
    b_image_save_to_png(d->f, d->filename);
}

void b_image_save_to_png_async(const BImage * f, char *filename)
{
    GTask *task = g_task_new(NULL, NULL, frame_save_to_png_callback, NULL);
    PNGSaveData *d = g_slice_new(PNGSaveData);
    d->f = b_image_copy(f);
    d->filename = g_strdup(filename);
    g_task_set_task_data(task, d, png_data_free);
    g_task_run_in_thread(task, frame_save_thread);
}

void b_image_save_to_png(const BImage * f, char *filename)
{
    FILE *fp = fopen(filename, "wb");
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t x, y;
    png_bytepp row_pointers;

    int image_width = f->ncol;
    int image_height = f->nrow;
    guint16 *cam_data = f->data;

    png_ptr =
	png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
	return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
	png_destroy_write_struct(&png_ptr, NULL);
	return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return;
    }

    png_set_IHDR(png_ptr, info_ptr, image_width, image_height,
		 16,		/* TODO: use 8 bit if appropriate */
		 PNG_COLOR_TYPE_GRAY,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

    row_pointers =
	(png_bytepp) png_malloc(png_ptr,
				image_height * sizeof(png_bytepp));
    int i;
    for (i = 0; i < image_height; i++)
	row_pointers[i] = NULL;

    for (i = 0; i < image_height; i++)
	row_pointers[i] = png_malloc(png_ptr, image_width * 2);

    for (y = 0; y < image_height; ++y) {
	png_bytep row = row_pointers[y];
	for (x = 0; x < image_width; ++x) {
	    guint16 color = cam_data[x + y * image_width];

	    *row++ = (png_byte) (color >> 8);
	    *row++ = (png_byte) (color & 0xFF);
	}
    }

    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    for (y = 0; y < image_height; y++) {
	png_free(png_ptr, row_pointers[y]);
    }
    png_free(png_ptr, row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}
