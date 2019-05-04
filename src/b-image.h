/*
 * b-image.h :
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

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
  gpointer data;
  guchar bits;
  guint32 ncol;
  guint32 nrow;
  gint32 num;			/* an index, e.g. to denote a buffer */
  guint64 timestamp;		/* time stamp */
} BImage;

BImage *b_image_new(guchar bits, guint32 nrow, guint32 ncol);
BImage *b_image_copy(const BImage * f);
void b_image_free(BImage * f);
guint16 b_image_max(const BImage * f, guint32 * c, guint32 * r);

G_END_DECLS
