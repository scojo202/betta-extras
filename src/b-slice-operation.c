/*
 * y-data-vector-slice.c :
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

#include <memory.h>
#include <math.h>
#include "b-slice-operation.h"
#include "data/b-struct.h"

/**
 * SECTION: b-slice-operation
 * @short_description: Operation that slices matrices or vectors, outputting vectors or scalars, respectively.
 *
 * This operation slices elements out of an array, yielding an array with one less dimension.
 *
 *
 */

enum {
	SLICE_PROP_0,
	SLICE_PROP_INDEX,
	SLICE_PROP_TYPE,
	SLICE_PROP_WIDTH,
	SLICE_PROP_MEAN
};

struct _BSliceOperation {
	BOperation base;
	int index;
	guchar type;
	int width;
	int index2;
	int width2;
	gboolean mean;
};

G_DEFINE_TYPE(BSliceOperation, b_slice_operation, B_TYPE_OPERATION);

static void
slice_operation_set_property(GObject * gobject, guint param_id,
			       GValue const *value, GParamSpec * pspec)
{
	BSliceOperation *sop = B_SLICE_OPERATION(gobject);

	switch (param_id) {
	case SLICE_PROP_INDEX:
		sop->index = g_value_get_int(value);
		break;
	case SLICE_PROP_TYPE:
		sop->type = g_value_get_int(value);
		break;
	case SLICE_PROP_WIDTH:
		sop->width = g_value_get_int(value);
		break;
	case SLICE_PROP_MEAN:
		sop->mean = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		return;		/* NOTE : RETURN */
	}
}

static void
slice_operation_get_property(GObject * gobject, guint param_id,
			       GValue * value, GParamSpec * pspec)
{
	BSliceOperation *sop = B_SLICE_OPERATION(gobject);

	switch (param_id) {
	case SLICE_PROP_INDEX:
		g_value_set_int(value, sop->index);
		break;
	case SLICE_PROP_TYPE:
		g_value_set_int(value, sop->type);
		break;
	case SLICE_PROP_WIDTH:
		g_value_set_int(value, sop->width);
		break;
	case SLICE_PROP_MEAN:
		g_value_set_boolean(value, sop->mean);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		return;		/* NOTE : RETURN */
	}
}

static
int slice_size(BOperation * op, BData * input, unsigned int *dims)
{
	int n_dims = 0;
	g_assert(dims);
	BSliceOperation *sop = B_SLICE_OPERATION(op);

	g_assert(!B_IS_SCALAR(input));
	g_assert(!B_IS_STRUCT(input));

	if (B_IS_VECTOR(input)) {
		if (sop->type == SLICE_ELEMENT
		    || sop->type == SLICE_SUMELEMENTS) {
			dims[0] = 1;
		} else {
			g_warning
			    ("Only SLICE_ELEMENT supported for vector input.");
		}
		return n_dims;
	}

	BMatrix *mat = B_MATRIX(input);

	if ((sop->type == SLICE_ROW) || (sop->type == SLICE_SUMROWS)) {
		dims[0] = b_matrix_get_columns(mat);
		dims[1] = 1;
		n_dims = 1;
	} else if ((sop->type == SLICE_COL) || (sop->type == SLICE_SUMCOLS)) {
		dims[0] = b_matrix_get_rows(mat);
		dims[1] = 1;
		n_dims = 1;
	} else {
		dims[0] = 0;
		dims[1] = 0;
		n_dims = 1;
	}
	return n_dims;
}

typedef struct {
	BSliceOperation sop;
	GType input_type;
	double *input;
	BMatrixSize size;
	double *output;
	unsigned int output_len;
} SliceOpData;

static
gpointer vector_slice_op_create_data(BOperation * op, gpointer data,
				     BData * input)
{
	if (input == NULL)
		return NULL;
	SliceOpData *d;
	gboolean neu = TRUE;
	if (data == NULL) {
		d = g_new0(SliceOpData, 1);
	} else {
		neu = FALSE;
		d = (SliceOpData *) data;
	}
	BSliceOperation *sop = B_SLICE_OPERATION(op);
	d->sop = *sop;
	if (B_IS_VECTOR(input)) {
		BVector *vec = B_VECTOR(input);
		d->input_type = B_TYPE_VECTOR;
		d->input =
		    b_create_input_array_from_vector(vec, neu, d->size.columns,
						     d->input);
		d->size.columns = b_vector_get_len(vec);
		d->size.rows = 0; /* special case for an input vector */
		if (d->output_len != 1) {
			g_clear_pointer(&d->output,g_free);
			d->output = g_new0(double, 1);
			d->output_len = 1;
		}
		return d;
	}
	BMatrix *mat = B_MATRIX(input);
	d->input_type = B_TYPE_MATRIX;
	d->input =
	    b_create_input_array_from_matrix(mat, neu, d->size, d->input);
	d->size = b_matrix_get_size(mat);
	unsigned int dims[2];
	slice_size(op, input, dims);
	if (d->output_len != dims[0]) {
    g_clear_pointer(&d->output,g_free);
    d->output = g_try_new0(double, dims[0]);
    if (d->output)
      d->output_len = dims[0];
    else
      d->output_len = 0;
  }
  return d;
}

static
void vector_slice_op_data_free(gpointer d)
{
	SliceOpData *s = (SliceOpData *) d;
  g_clear_pointer(&s->input,g_free);
  g_clear_pointer(&s->output,g_free);
	g_free(d);
}

static
gpointer vector_slice_op(gpointer input)
{
	SliceOpData *d = (SliceOpData *) input;

	if (d == NULL)
		return NULL;

	unsigned int nrow = d->size.rows;
	unsigned int ncol = d->size.columns;
	double *m = d->input;

	double *v = d->output;

	if (d->input_type == B_TYPE_VECTOR) {	/* output will be scalar */
		if (d->sop.type == SLICE_ELEMENT) {
			*v = m[d->sop.index];
		} else if (d->sop.type == SLICE_SUMELEMENTS) {
			unsigned int j;
			int w = d->sop.width;
			int start = d->sop.index - w / 2;
			start = MAX(start, 0);
			int end = d->sop.index + w / 2;
			end = MIN(end, (int)(nrow - 1));
			*v = 0.;
			int n = 0;
			for (j = start; j <= end; j++) {
				*v += m[j];
				n++;
			}
			if (d->sop.mean) {
				*v /= n;
			}
		}
	} else {		/* output will be vector */
		if (d->sop.type == SLICE_ROW) {
			memcpy(v, &m[d->sop.index * ncol],
			       sizeof(double) * ncol);
		} else if (d->sop.type == SLICE_COL) {
			unsigned int j;
			for (j = 0; j < nrow; j++) {
				v[j] = m[d->sop.index + j * ncol];
			}
		} else if (d->sop.type == SLICE_SUMROWS) {
			int w = d->sop.width;
			int start, end;
			if(w==-1) {
				start = 0;
				end = nrow-1;
			}
			else {
				start = d->sop.index - w / 2;
				start = MAX(start, 0);
				end = d->sop.index + w / 2;
				end = MIN(end, (int)(nrow - 1));
			}
			unsigned int j;
			int k;
			for (j = 0; j < ncol; j++) {
				int n = 0;
				v[j] = 0.;
				for (k = start; k <= end; k++) {
					v[j] += m[j + k * ncol];
					n++;
				}
				if (d->sop.mean)
					v[j] /= n;
			}
		} else if (d->sop.type == SLICE_SUMCOLS) {
			int w = d->sop.width;
			int start,end;
			if(w==-1) {
				start=0;
				end=ncol-1;
			}
			else {
				start = d->sop.index - w / 2;
				start = MAX(start, 0);
				end = d->sop.index + w / 2;
				end = MIN(end, (int)(ncol - 1));
			}
			unsigned int j;
			int k;
			for (j = 0; j < nrow; j++) {
				int n = 0;
				v[j] = 0.;
				for (k = start; k <= end; k++) {
					v[j] += m[k + j * ncol];
					n++;
				}
				if (d->sop.mean)
					v[j] /= n;
			}
		}
	}
	return v;
}

static void b_slice_operation_class_init(BSliceOperationClass * slice_klass)
{
	GObjectClass *gobject_klass = (GObjectClass *) slice_klass;
	gobject_klass->set_property = slice_operation_set_property;
	gobject_klass->get_property = slice_operation_get_property;
	BOperationClass *op_klass = (BOperationClass *) slice_klass;
	op_klass->thread_safe = TRUE;
	op_klass->op_size = slice_size;
	op_klass->op_func = vector_slice_op;
	op_klass->op_data = vector_slice_op_create_data;
	op_klass->op_data_free = vector_slice_op_data_free;

	g_object_class_install_property(gobject_klass, SLICE_PROP_INDEX,
					g_param_spec_int("index", "Index",
							 "Index of slice",
							 0, 2000000000, 0,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass, SLICE_PROP_TYPE,
					g_param_spec_int("type", "Type",
							 "Type of slicing operation",
							 SLICE_ROW, 7,
							 SLICE_ROW,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass, SLICE_PROP_WIDTH,
					g_param_spec_int("width", "Width",
							 "Width of slice, if appropriate",
							 -1, 2000000000, 1,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass,
					SLICE_PROP_MEAN,
					g_param_spec_boolean("mean",
							     "average over elements",
							     "Average over elements if TRUE, sum over them if FALSE.",
							     FALSE,
							     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void b_slice_operation_init(BSliceOperation * slice)
{
	g_assert(B_IS_SLICE_OPERATION(slice));
	slice->type = SLICE_ROW;
	slice->width = 1;
}

/**
 * b_slice_operation_new:
 * @type: the type of slice
 * @index: the index of the slice
 * @width: the width over which to sum or average
 *
 * Create a new slice operation.
 *
 * Returns: a #YOperation
 **/
BOperation *b_slice_operation_new(int type, int index, int width)
{
	g_assert(index >= 0);
	g_assert(width >= -1);

	BOperation *o =
	    g_object_new(B_TYPE_SLICE_OPERATION, "type", type, "index", index,
			 "width", width, NULL);

	return o;
}

/**
 * b_slice_operation_set_pars:
 * @d: a #BSliceOperation
 * @type: the type of slice
 * @index: the index of the slice
 * @width: the width over which to sum or average (-1 to sum or average over entire width)
 *
 * Set the parameters of a slice operation.
 **/
void b_slice_operation_set_pars(BSliceOperation * d, int type, int index,
				int width)
{
	g_assert(B_IS_SLICE_OPERATION(d));
	g_assert(index >= 0);
	g_assert(width >= -1);
	if (d->type != type)
		g_object_set(d, "type", type, NULL);
	if (d->index != index)
		g_object_set(d, "index", index, NULL);
	if (d->width != width)
		g_object_set(d, "width", width, NULL);
}
