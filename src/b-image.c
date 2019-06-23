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
