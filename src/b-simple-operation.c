/*
 * b-simple-operation.c :
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
#include "b-simple-operation.h"

/**
 * SECTION: b-simple-operation
 * @short_description: Operations that apply a function to every element of a vector or matrix.
 *
 * This operation applies a function to every element of an array. The output will be the same size as the input.
 *
 *
 */

struct _BSimpleOperation {
  BOperation base;
  BDoubleToDouble func;
};

G_DEFINE_TYPE(BSimpleOperation, b_simple_operation, B_TYPE_OPERATION);

static
int simple_size(BOperation * op, BData * input, unsigned int *dims)
{
  g_assert(dims);
  /* output is the same size as input */
  BDataClass *data_class = B_DATA_GET_CLASS(input);
  char n_dims = data_class->get_sizes (input, dims);
  return (int) n_dims;
}

typedef struct {
  BSimpleOperation sop;
  double *input;
  unsigned int len;
  BMatrixSize size;
  double *output;
} SimpleOpData;

static
gpointer simple_op_create_data(BOperation * op, gpointer data, BData * input)
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
  BSimpleOperation *sop = B_SIMPLE_OPERATION(op);
  d->sop = *sop;
  if(B_IS_SCALAR(input)) {
    BScalar *sca = B_SCALAR(input);
    if(neu) {
      d->input = g_malloc(sizeof(double));
      d->len = 1;
      d->output = g_malloc(sizeof(double));
    }
    *d->input = b_scalar_get_value(sca);
  }
  else if(B_IS_VECTOR(input)) {
    BVector *vec = B_VECTOR(input);
    unsigned int old_len = d->len;
    d->input = b_create_input_array_from_vector(vec, neu, d->len, d->input);
    d->len = b_vector_get_len(vec);
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
    memcpy(d->input, b_vector_get_values(vec), d->len * sizeof(double));
  }
  else if(B_IS_MATRIX(input)) {
    BMatrix *mat = B_MATRIX(input);
    BMatrixSize old_size = d->size;
    d->input = b_create_input_array_from_matrix(mat, neu, d->size, d->input);
    d->size = b_matrix_get_size(mat);
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
    memcpy(d->input, b_matrix_get_values(mat), d->size.rows*d->size.columns * sizeof(double));
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

static void b_simple_operation_class_init(BSimpleOperationClass * slice_klass)
{
  BOperationClass *op_klass = (BOperationClass *) slice_klass;
  op_klass->thread_safe = FALSE;
  op_klass->op_size = simple_size;
  op_klass->op_func = simple_op;
  op_klass->op_data = simple_op_create_data;
  op_klass->op_data_free = simple_op_data_free;
}

static void b_simple_operation_init(BSimpleOperation * s)
{
}

/**
 * b_simple_operation_new: (skip)
 * @func: the function
 *
 * Create a new simple operation.
 *
 * Returns: a #BOperation
 **/
BOperation *b_simple_operation_new(BDoubleToDouble func)
{
  BSimpleOperation *o = g_object_new(B_TYPE_SIMPLE_OPERATION, NULL);
  o->func = func;

  return B_OPERATION(o);
}
