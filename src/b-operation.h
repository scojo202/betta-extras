/*
 * y-operation.h :
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

/*  Object for an operation  */
/*  Operations maintain copies of input data for multithreading */

#ifndef B_OP_H
#define B_OP_H

#include <gio/gio.h>
#include <data/b-data-class.h>

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE(BOperation,b_operation,B,OPERATION,GObject)

#define B_TYPE_OPERATION	(b_operation_get_type ())

/**
 * BOperationClass:
 * @base: base class.
 * @thread_safe: whether the operation can be run in a thread.
 * @op_size: outputs how large the output will be, for a particular instance of input data.
 * @op_func: the function to call for the operation
 * @op_data: allocate data for the operation
 * @op_data_free: a #GDestroyNotify for the operation data
 *
 * Class for BOperation.
 **/

struct _BOperationClass {
  GObjectClass base;
  gboolean thread_safe; /* does this operation keep copies of all data so it can be done in a thread? */
  int (*op_size) (BOperation *op, BData *input, unsigned int *dims);
  gpointer (*op_func) (gpointer data);
  gpointer (*op_data) (BOperation *op, gpointer data, BData *input);
  GDestroyNotify op_data_free;
};

double *b_create_input_array_from_vector(BVector *input, gboolean is_new, unsigned int old_size, double *old_input);
double *b_create_input_array_from_matrix(BMatrix *input, gboolean is_new, BMatrixSize old_size, double *old_input);

BData *b_data_new_from_operation(BOperation *op, BData *input);

GTask * b_operation_get_task(BOperation *op, gpointer user_data, GAsyncReadyCallback cb, gpointer cb_data);
gpointer b_operation_create_task_data(BOperation *op, BData *input);
void b_operation_run_task(BOperation *op, gpointer user_data, GAsyncReadyCallback cb, gpointer cb_data);
void b_operation_update_task_data(BOperation *op, gpointer task_data, BData *input);

G_END_DECLS

#endif
