/*
 * y-simple-operation.c :
 *
 * Copyright (C) 2017 Scott O. Johnson (scojo202@gmail.com)
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
#include "y-simple-operation.h"

/**
 * SECTION: y-simple-operation
 * @short_description: Operations that apply a function to every element of a vector or matrix.
 *
 * This operation applies a function to every element of an array. The output will be the same size as the input.
 *
 *
 */

struct _YSimpleOperation {
	YOperation base;
	YDoubleToDouble func;
};

G_DEFINE_TYPE(YSimpleOperation, y_simple_operation, Y_TYPE_OPERATION);

static
int simple_size(YOperation * op, YData * input, unsigned int *dims)
{
	g_assert(dims);
	/* output is the same size as input */
	YDataClass *data_class = Y_DATA_GET_CLASS(input);
	char n_dims = data_class->get_sizes (input, dims);
	return (int) n_dims;
}

typedef struct {
	YSimpleOperation sop;
	double *input;
	unsigned int len;
	YMatrixSize size;
	double *output;
} SimpleOpData;

static
gpointer simple_op_create_data(YOperation * op, gpointer data,
				      YData * input)
{
	if (input == NULL)
		return NULL;
	SimpleOpData *d;
	gboolean neu = TRUE;
	if (data == NULL) {
		d = g_new0(SimpleOpData, 1);
	} else {
		neu = FALSE;
		d = (SimpleOpData *) data;
	}
	YSimpleOperation *sop = Y_SIMPLE_OPERATION(op);
	d->sop = *sop;
	if(Y_IS_SCALAR(input)) {
		YScalar *sca = Y_SCALAR(input);
		d->input = g_malloc(sizeof(double));
		*d->input = y_scalar_get_value(sca);
		d->len = 1;
		d->output = g_malloc(sizeof(double));
	}
	else if(Y_IS_VECTOR(input)) {
		YVector *vec = Y_VECTOR(input);
		unsigned int old_len = d->len;
		d->input = y_create_input_array_from_vector(vec, neu, d->len, d->input);
		d->len = y_vector_get_len(vec);
		if (d->len == 0)
			return NULL;
		unsigned int dims[3];
		simple_size(op, input, dims);
		if (d->len != old_len) {
			if (d->output)
				g_free(d->output);
			d->output = g_new0(double, d->len);
		}
		g_assert(d->input);
		g_assert(d->len > 0);
		memcpy(d->input, y_vector_get_values(vec), d->len * sizeof(double));
	}
	else if(Y_IS_MATRIX(input)) {
		YMatrix *mat = Y_MATRIX(input);
		YMatrixSize old_size = d->size;
		d->input = y_create_input_array_from_matrix(mat, neu, d->size, d->input);
		d->size = y_matrix_get_size(mat);
		if (d->size.rows == 0 || d->size.columns == 0)
			return NULL;
		unsigned int dims[3];
		simple_size(op, input, dims);
		if (d->size.rows*d->size.columns != old_size.rows*old_size.rows) {
			if (d->output)
				g_free(d->output);
			d->output = g_new0(double, d->size.rows*d->size.columns);
		}
		d->len = d->size.rows*d->size.columns;
		g_assert(d->input);
		g_assert(d->size.rows*d->size.columns > 0);
		memcpy(d->input, y_matrix_get_values(mat), d->size.rows*d->size.columns * sizeof(double));
	}
	return d;
}

static
void simple_op_data_free(gpointer d)
{
	SimpleOpData *s = (SimpleOpData *) d;
	g_free(s->input);
	g_free(s->output);
	g_free(d);
}

static
gpointer simple_op(gpointer input)
{
	SimpleOpData *d = (SimpleOpData *) input;

	if (d == NULL)
		return NULL;

	int i;
	for (i = 0; i < d->len; i++) {
		d->output[i] = d->sop.func(d->input[i]);
	}

	return d->output;
}

static void y_simple_operation_class_init(YSimpleOperationClass * slice_klass)
{
	YOperationClass *op_klass = (YOperationClass *) slice_klass;
	op_klass->thread_safe = FALSE;
	op_klass->op_size = simple_size;
	op_klass->op_func = simple_op;
	op_klass->op_data = simple_op_create_data;
	op_klass->op_data_free = simple_op_data_free;
}

static void y_simple_operation_init(YSimpleOperation * s)
{
	g_assert(Y_IS_SIMPLE_OPERATION(s));
}

/**
 * y_simple_operation_new: (skip)
 * @func: the function
 *
 * Create a new simple operation.
 *
 * Returns: a #YOperation
 **/
YOperation *y_simple_operation_new(YDoubleToDouble func)
{
	YSimpleOperation *o = g_object_new(Y_TYPE_SIMPLE_OPERATION, NULL);
	o->func = func;

	return Y_OPERATION(o);
}
