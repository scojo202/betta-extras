/*
 * b-arv-source.h :
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

#include <b-data.h>
#include <arv.h>
#include "b-image.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BArvSource,b_arv_source,B,ARV_SOURCE,BData)

#define B_TYPE_ARV_SOURCE (b_arv_source_get_type())

BMatrixSize b_arv_source_get_size (BArvSource *mat);
BImage *b_arv_source_get_frame(BArvSource *mat);
BImage *b_arv_source_copy_frame(BArvSource *mat);
guint16 *b_arv_source_get_values (BArvSource *mat);
guint16 b_arv_source_get_value  (BArvSource *mat, unsigned int i, unsigned int j);
void b_arv_source_get_minmax (BArvSource *mat, guint16 *min, guint16 *max);
void b_arv_source_create_stream (BArvSource *s);

G_END_DECLS
