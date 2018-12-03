/*
 * y-subset-operation.c :
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
#include "y-subset-operation.h"
#include "y-struct.h"

/**
 * SECTION: y-subset-operation
 * @short_description: Operation that outputs a subset of a vector or matrix.
 *
 * These output a subset of the input array. The output is smaller in size but has the same number of dimensions.
 *
 *
 */

enum {
	SUBSET_PROP_0,
	SUBSET_PROP_START1,
	SUBSET_PROP_LENGTH1,
	SUBSET_PROP_START2,
	SUBSET_PROP_LENGTH2,
	N_PROPERTIES
};

struct _YSubsetOperation {
	YOperation base;
	int start1, length1, start2, length2;
	/* stride */
};

G_DEFINE_TYPE(YSubsetOperation, y_subset_operation, Y_TYPE_OPERATION);

static void
y_subset_operation_set_property(GObject * gobject, guint param_id,
				GValue const *value, GParamSpec * pspec)
{
	YSubsetOperation *sop = Y_SUBSET_OPERATION(gobject);

	switch (param_id) {
	case SUBSET_PROP_START1:
		sop->start1 = g_value_get_int(value);
		break;
	case SUBSET_PROP_LENGTH1:
		sop->length1 = g_value_get_int(value);
		break;
	case SUBSET_PROP_START2:
		sop->start2 = g_value_get_int(value);
		break;
	case SUBSET_PROP_LENGTH2:
		sop->length2 = g_value_get_int(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		return;		/* NOTE : RETURN */
	}
}

static void
y_subset_operation_get_property(GObject * gobject, guint param_id,
				GValue * value, GParamSpec * pspec)
{
	YSubsetOperation *sop = Y_SUBSET_OPERATION(gobject);

	switch (param_id) {
	case SUBSET_PROP_START1:
		g_value_set_int(value, sop->start1);
		break;
	case SUBSET_PROP_LENGTH1:
		g_value_set_int(value, sop->length1);
		break;
	case SUBSET_PROP_START2:
		g_value_set_int(value, sop->start2);
		break;
	case SUBSET_PROP_LENGTH2:
		g_value_set_int(value, sop->length2);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		return;		/* NOTE : RETURN */
	}
}

static
int subset_size(YOperation * op, YData * input, unsigned int *dims)
{
	int n_dims;
	g_assert(dims);
	YSubsetOperation *sop = Y_SUBSET_OPERATION(op);

	g_assert(!Y_IS_SCALAR(input));
	g_assert(!Y_IS_STRUCT(input));

	if (Y_IS_VECTOR(input)) {
		unsigned int l = y_vector_get_len(Y_VECTOR(input));
		dims[0] =
		    (sop->start1 + sop->length1 >
		     l) ? l - sop->start1 : sop->length1;
		n_dims = 1;
		return n_dims;
	}

	YMatrix *mat = Y_MATRIX(input);

	YMatrixSize size = y_matrix_get_size(Y_MATRIX(mat));
	int real_length1 =
	    (sop->start1 + sop->length1 >
	     size.columns) ? size.columns - sop->start1 : sop->length1;
	int real_length2 =
	    (sop->start2 + sop->length2 >
	     size.rows) ? size.rows - sop->start2 : sop->length2;

	dims[0] = real_length1;
	dims[1] = real_length2;
	n_dims = 2;

	return n_dims;
}

typedef struct {
	YSubsetOperation sop;
	double *input;
	YMatrixSize size;
	double *output;
	YMatrixSize output_size;
} SubsetOpData;

static
gpointer subset_op_create_data(YOperation * op, gpointer data, YData * input)
{
	if (input == NULL)
		return NULL;
	SubsetOpData *d;
	gboolean neu = TRUE;
	if (data == NULL) {
		d = g_new0(SubsetOpData, 1);
	} else {
		neu = FALSE;
		d = (SubsetOpData *) data;
	}
	YSubsetOperation *sop = Y_SUBSET_OPERATION(op);
	d->sop = *sop;
	if (Y_IS_VECTOR(input)) {
		YVector *vec = Y_VECTOR(input);
		d->input =
		    y_create_input_array_from_vector(vec, neu, d->size.rows,
						     d->input);
		if (d->output_size.columns != sop->length1) {
			d->output = g_new(double, sop->length1);
		}
		return d;
	}
	YMatrix *mat = Y_MATRIX(input);
	d->input =
	    y_create_input_array_from_matrix(mat, neu, d->size, d->input);
	d->size = y_matrix_get_size(mat);
	unsigned int dims[2];
	subset_size(op, input, dims);
	if (d->output_size.columns != dims[0] || d->output_size.rows != dims[1]) {
		g_free(d->output);
		d->output = g_new(double, dims[0] * dims[1]);
	}
	return d;
}

static
void subset_op_data_free(gpointer d)
{
	SubsetOpData *s = (SubsetOpData *) d;
	g_free(s->input);
	g_free(s->output);
	g_free(d);
}

static
gpointer subset_op(gpointer input)
{
	SubsetOpData *d = (SubsetOpData *) input;

	if (d == NULL)
		return NULL;

	unsigned int ncol = d->size.columns;
	double *m = d->input;

	double *v = d->output;
	unsigned int i, j;

	if (d->size.rows==0) {
		for (j = 0; j < d->sop.length1; j++) {
			v[j] = m[j + d->sop.start1];
		}
	} else {
		for (j = 0; j < d->sop.length1; j++) {
			for (i = 0; i < d->sop.length2; i++) {
				v[i * d->sop.length1 + j] =
				    m[(i + d->sop.start2) * ncol + j +
				      d->sop.start1];
			}
		}
	}
	return v;
}

static void y_subset_operation_class_init(YSubsetOperationClass * subset_klass)
{
	GObjectClass *gobject_klass = (GObjectClass *) subset_klass;
	gobject_klass->set_property = y_subset_operation_set_property;
	gobject_klass->get_property = y_subset_operation_get_property;
	YOperationClass *op_klass = (YOperationClass *) subset_klass;
	op_klass->thread_safe = TRUE;
	op_klass->op_size = subset_size;
	op_klass->op_func = subset_op;
	op_klass->op_data = subset_op_create_data;
	op_klass->op_data_free = subset_op_data_free;

	g_object_class_install_property(gobject_klass, SUBSET_PROP_START1,
					g_param_spec_int("start1",
							 "Start index #1",
							 "First index", 0,
							 2000000000, 0,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass, SUBSET_PROP_LENGTH1,
					g_param_spec_int("length1", "Length #1",
							 "First length",
							 1, 2000000000, 1,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass, SUBSET_PROP_START2,
					g_param_spec_int("start2",
							 "Start index #2",
							 "Second index", 0,
							 2000000000, 0,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(gobject_klass, SUBSET_PROP_LENGTH2,
					g_param_spec_int("length2", "Length #2",
							 "Second length",
							 1, 2000000000, 1,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void y_subset_operation_init(YSubsetOperation * slice)
{
	g_assert(Y_IS_SUBSET_OPERATION(slice));
	slice->start1 = 0;
	slice->length1 = 1;
	slice->start2 = 0;
	slice->length2 = 1;
}
