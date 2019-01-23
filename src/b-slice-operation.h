/*
 * y-data-vector-slice.h :
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

#ifndef DATA_VECTOR_SLICE_H
#define DATA_VECTOR_SLICE_H

#include <data/b-data-class.h>
#include <b-operation.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BSliceOperation,b_slice_operation,B,SLICE_OPERATION,BOperation)

#define B_TYPE_SLICE_OPERATION  (b_slice_operation_get_type ())

enum {
	SLICE_ROW = 0,
	SLICE_COL = 1,
	SLICE_SUMROWS = 2,
	SLICE_SUMCOLS = 3
};

#define SLICE_ELEMENT SLICE_ROW
#define SLICE_SUMELEMENTS SLICE_SUMROWS

BOperation *b_slice_operation_new (int type, int index, int width);
void b_slice_operation_set_pars(BSliceOperation *d, int type, int index,
                                 int width);


G_END_DECLS

#endif
