/*
 * b-data-derived.c :
 *
 * Copyright (C) 2016-2017 Scott O. Johnson (scojo202@gmail.com)
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
#include "b-data-derived.h"

/**
 * SECTION: b-data-derived
 * @short_description: Data objects that reflect the outputs of operations.
 *
 * These can change automatically when input data emit changed signals.
 *
 *
 */

enum {
	PROP_0,
	PROP_AUTORUN,
	PROP_INPUT,
	PROP_OPERATION,
	N_PROPERTIES
};

G_DEFINE_INTERFACE(BDerived, b_derived, B_TYPE_DATA)

static void b_derived_default_init(BDerivedInterface * i)
{
  g_object_interface_install_property(i,
    g_param_spec_boolean("autorun", "Auto-run",
            "Whether to run the operation immediately upon input data changing",
            FALSE /* default value */,
            G_PARAM_READWRITE));

  g_object_interface_install_property(i,
    g_param_spec_object("input", "Input data", "The input data", B_TYPE_DATA,
                        G_PARAM_READWRITE));

  g_object_interface_install_property(i,
    g_param_spec_object("operation", "Operation", "The operation",
                        B_TYPE_OPERATION, G_PARAM_READWRITE));
}

/**
 * BDerivedScalar:
 *
 *
 **/

typedef struct {
  BOperation *op;
  BData *input;
  gulong handler;
  unsigned int autorun : 1;
  unsigned int running : 1;	/* is operation currently running? */
  gpointer task_data;
} Derived;

static
void finalize_derived(Derived *d) {
  if(d->handler != 0 && d->input !=NULL) {
    g_signal_handler_disconnect(d->input,d->handler);
  }
  /* unref matrix */
  g_clear_object(&d->input);
  if (d->task_data) {
    BOperationClass *klass = (BOperationClass *) G_OBJECT_GET_CLASS(d->op);
    if (klass->op_data_free)
      klass->op_data_free(d->task_data);
  }
  g_clear_object(&d->op);
}

static gboolean
derived_get_property(Derived *d, guint property_id, GValue * value)
{
  gboolean found = TRUE;

  switch (property_id) {
  case PROP_AUTORUN:
    g_value_set_boolean(value, d->autorun);
    break;
  case PROP_INPUT:
    g_value_set_object(value, d->input);
    break;
  case PROP_OPERATION:
    g_value_set_object(value, d->op);
    break;
  default:
    found = FALSE;
    break;
  }

  return found;
}

static void derived_set_input(Derived *d, BData *new_d)
{
  if(new_d != d->input) {
    if(d->input!=NULL)
      g_signal_handler_disconnect(d->input,d->handler);
    g_clear_object(&d->input);
    d->input = new_d;
    g_object_ref_sink(d->input);
  }
}

/*****************/

struct _BDerivedScalar {
  BScalar base;
  double cache;
  Derived der;
};

static void b_scalar_derived_interface_init(BDerivedInterface * iface)
{

}

G_DEFINE_TYPE_WITH_CODE(BDerivedScalar, b_derived_scalar, B_TYPE_SCALAR,
      G_IMPLEMENT_INTERFACE(B_TYPE_DERIVED, b_scalar_derived_interface_init));

static
void b_derived_scalar_init(BDerivedScalar * self)
{

}

static double scalar_derived_get_value(BScalar * sca)
{
  BDerivedScalar *scas = (BDerivedScalar *) sca;

  g_return_val_if_fail (sca != NULL, NAN);

  BOperationClass *klass = B_OPERATION_GET_CLASS(scas->der.op);

  unsigned int dims[3];

  g_return_val_if_fail(klass->op_size(scas->der.op,scas->der.input, dims)==0,NAN);

  /* call op */
  if (scas->der.task_data == NULL) {
    scas->der.task_data =
      b_operation_create_task_data(scas->der.op, scas->der.input);
  } else {
    b_operation_update_task_data(scas->der.op, scas->der.task_data,
                                  scas->der.input);
  }
  double *dout = klass->op_func(scas->der.task_data);

  return *dout;
}

static void
scalar_op_cb(GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  /* set outputs */
  GTask *task = G_TASK(res);
  g_task_propagate_pointer(task, NULL);
  BDerivedScalar *d = (BDerivedScalar *) user_data;
  d->der.running = FALSE;
  b_data_emit_changed(B_DATA(user_data));
}

static void scalar_on_input_changed(BData * data, gpointer user_data)
{
  g_return_if_fail(B_IS_DATA(data));
  g_return_if_fail(B_IS_DERIVED_SCALAR(user_data));
  BDerivedScalar *d = B_DERIVED_SCALAR(user_data);
  if (!d->der.autorun) {
    b_data_emit_changed(B_DATA(d));
  } else {
    if (d->der.running)
      return;
    d->der.running = TRUE;
    BOperationClass *klass = B_OPERATION_GET_CLASS(d->der.op);
    if (klass->thread_safe) {
      /* get task data, run in a thread */
      b_operation_update_task_data(d->der.op, d->der.task_data, data);
      b_operation_run_task(d->der.op, d->der.task_data, scalar_op_cb, d);
    } else {
      /* load new values into the cache */
      d->cache = scalar_derived_get_value(B_SCALAR(d));
      d->der.running = FALSE;
      b_data_emit_changed(B_DATA(d));
    }
  }
}

static void
scalar_on_op_changed(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  BDerivedScalar *d = B_DERIVED_SCALAR(user_data);
  b_data_emit_changed(B_DATA(d));
}

static void scalar_derived_finalize(GObject * obj)
{
  BDerivedScalar *vec = (BDerivedScalar *) obj;
  finalize_derived(&vec->der);

  GObjectClass *obj_class = G_OBJECT_CLASS(b_derived_scalar_parent_class);

  obj_class->finalize(obj);
}

static void
y_scalar_derived_set_property(GObject * object, guint property_id,
                              const GValue * value, GParamSpec * pspec)
{
  BDerivedScalar *s = B_DERIVED_SCALAR(object);

  switch (property_id) {
  case PROP_AUTORUN:
    s->der.autorun = g_value_get_boolean(value);
    break;
  case PROP_INPUT:
    derived_set_input(&s->der, g_value_get_object(value));
    if(s->der.input) {
      g_signal_connect(s->der.input, "changed",
                     G_CALLBACK(scalar_on_input_changed), s);
      b_data_emit_changed(B_DATA(s));
    }
    break;
  case PROP_OPERATION:
    s->der.op = g_value_get_object(value);
    /* listen to "notify" from op for property changes */
    g_signal_connect(s->der.op, "notify", G_CALLBACK(scalar_on_op_changed), s);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
y_scalar_derived_get_property(GObject * object, guint property_id,
                              GValue * value, GParamSpec * pspec)
{
  BDerivedScalar *s = B_DERIVED_SCALAR(object);

  gboolean found = derived_get_property(&s->der,property_id,value);
  if(found) {
    return;
  }

  switch (property_id) {
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static
void b_derived_scalar_class_init(BDerivedScalarClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  BScalarClass *scalar_class = (BScalarClass *) klass;

  gobject_class->finalize = scalar_derived_finalize;
  gobject_class->set_property = y_scalar_derived_set_property;
  gobject_class->get_property = y_scalar_derived_get_property;

  scalar_class->get_value = scalar_derived_get_value;

  g_object_class_override_property(gobject_class, PROP_AUTORUN, "autorun");
  g_object_class_override_property(gobject_class, PROP_INPUT, "input");
  g_object_class_override_property(gobject_class, PROP_OPERATION, "operation");
}

/**
 * b_derived_scalar_new:
 * @input: an input array
 * @op: an operation
 *
 * Create a new #BDerivedScalar based on an input #BData and a #BOperation.
 *
 * Returns: a #BData
 **/
BData *b_derived_scalar_new(BData * input, BOperation * op)
{
  if (input)
    g_assert(B_IS_DATA(input));
  g_assert(B_IS_OPERATION(op));

  BData *d = g_object_new(B_TYPE_DERIVED_SCALAR, "operation", op, NULL);

  if (B_IS_DATA(d) && B_IS_DATA(input)) {
    g_object_set(d, "input", input, NULL);
  }
  return d;
}

/****************************************************************************/

/**
 * BDerivedVector:
 *
 * Object representing data.
 **/

struct _BDerivedVector {
  BVector base;
  unsigned int currlen;
  Derived der;
};

static void b_derived_vector_interface_init(BDerivedInterface * iface)
{
}

G_DEFINE_TYPE_WITH_CODE(BDerivedVector, b_derived_vector, B_TYPE_VECTOR,
                        G_IMPLEMENT_INTERFACE(B_TYPE_DERIVED,
                                              b_derived_vector_interface_init));

static void vector_derived_finalize(GObject * obj)
{
  BDerivedVector *vec = (BDerivedVector *) obj;
  finalize_derived(&vec->der);

  GObjectClass *obj_class = G_OBJECT_CLASS(b_derived_vector_parent_class);

  obj_class->finalize(obj);
}

static unsigned int vector_derived_load_len(BVector * vec)
{
  BDerivedVector *vecd = (BDerivedVector *) vec;
  g_assert(B_IS_OPERATION(vecd->der.op));
  BOperationClass *klass = (BOperationClass *) G_OBJECT_GET_CLASS(vecd->der.op);

  unsigned int newdim = 0;
  g_assert(klass->op_size);
  if (vecd->der.input) {
    int ndims = klass->op_size(vecd->der.op, vecd->der.input, &newdim);
    g_return_val_if_fail(ndims == 1, 0);
  }

  return newdim;
}

static double *vector_derived_load_values(BVector * vec)
{
  BDerivedVector *vecs = (BDerivedVector *) vec;

  double *v = NULL;

  unsigned int len = b_vector_get_len(vec);

  if (vecs->currlen != len) {
    v=b_vector_replace_cache(vec,len);
    vecs->currlen = len;
  } else {
    v = b_vector_replace_cache(vec,vecs->currlen);
  }
  g_return_val_if_fail (v != NULL, NULL);

  /* call op */
  BOperationClass *klass = B_OPERATION_GET_CLASS(vecs->der.op);
  if (vecs->der.task_data == NULL) {
    vecs->der.task_data =
      b_operation_create_task_data(vecs->der.op, vecs->der.input);
  } else {
    b_operation_update_task_data(vecs->der.op, vecs->der.task_data,
                                 vecs->der.input);
  }
  double *dout = klass->op_func(vecs->der.task_data);
  g_return_val_if_fail (dout != NULL, NULL);
  memcpy(v, dout, len * sizeof(double));

  return v;
}

static double vector_derived_get_value(BVector * vec, unsigned i)
{
  const double *d = b_vector_get_values(vec);	/* fills the cache */
  g_return_val_if_fail(d!=NULL,NAN);
  return d[i];
}

static void
op_cb(GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  /* set outputs */
  GTask *task = G_TASK(res);
  g_task_propagate_pointer(task, NULL);
  BDerivedVector *d = (BDerivedVector *) user_data;
  d->der.running = FALSE;
  b_data_emit_changed(B_DATA(user_data));
}

static void on_input_changed_after(BData * data, gpointer user_data)
{
  g_return_if_fail(B_IS_DATA(data));
  g_return_if_fail(B_IS_DERIVED_VECTOR(user_data));
  BDerivedVector *d = B_DERIVED_VECTOR(user_data);
  /* if shape changed, adjust length */
  /* FIXME: this just loads the length every time */
  vector_derived_load_len(B_VECTOR(d));
  if (!d->der.autorun) {
    b_data_emit_changed(B_DATA(d));
  } else {
    if (d->der.running)
      return;
    d->der.running = TRUE;
    BOperationClass *klass = B_OPERATION_GET_CLASS(d->der.op);
    if (klass->thread_safe) {
      /* get task data, run in a thread */
      b_operation_update_task_data(d->der.op, d->der.task_data, data);
      b_operation_run_task(d->der.op, d->der.task_data, op_cb, d);
    } else {
      /* load new values into the cache */
      vector_derived_load_values(B_VECTOR(d));
      d->der.running = FALSE;
      b_data_emit_changed(B_DATA(d));
    }
  }
}

static void
on_op_changed(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  BDerivedVector *d = B_DERIVED_VECTOR(user_data);
  vector_derived_load_len(B_VECTOR(d));
  b_data_emit_changed(B_DATA(d));
}

static void
b_derived_vector_set_property(GObject * object, guint property_id,
                              const GValue * value, GParamSpec * pspec)
{
  BDerivedVector *v = B_DERIVED_VECTOR(object);
  Derived *d = &v->der;

  switch (property_id) {
  case PROP_AUTORUN:
    d->autorun = g_value_get_boolean(value);
    break;
  case PROP_INPUT:
    derived_set_input(&v->der, g_value_get_object(value));
    if(d->input) {
      d->handler = g_signal_connect(d->input, "changed",
                                    G_CALLBACK(on_input_changed_after), v);
      b_data_emit_changed(B_DATA(v));
    }
    break;
  case PROP_OPERATION:
    d->op = g_value_dup_object(value);
    /* listen to "notify" from op for property changes */
    g_signal_connect(d->op, "notify", G_CALLBACK(on_op_changed), v);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
b_derived_vector_get_property(GObject * object, guint property_id,
                              GValue * value, GParamSpec * pspec)
{
  BDerivedVector *v = B_DERIVED_VECTOR(object);

  gboolean found = derived_get_property(&v->der,property_id,value);
  if(found)
    return;

  switch (property_id) {
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void b_derived_vector_class_init(BDerivedVectorClass * slice_klass)
{
  GObjectClass *gobject_class = (GObjectClass *) slice_klass;
  BVectorClass *vector_klass = (BVectorClass *) gobject_class;

  gobject_class->finalize = vector_derived_finalize;
  gobject_class->set_property = b_derived_vector_set_property;
  gobject_class->get_property = b_derived_vector_get_property;

  vector_klass->load_len = vector_derived_load_len;
  vector_klass->load_values = vector_derived_load_values;
  vector_klass->get_value = vector_derived_get_value;

  g_object_class_override_property(gobject_class, PROP_AUTORUN, "autorun");
  g_object_class_override_property(gobject_class, PROP_INPUT, "input");
  g_object_class_override_property(gobject_class, PROP_OPERATION, "operation");
}

static void b_derived_vector_init(BDerivedVector * der)
{
}

/**
 * b_derived_vector_new:
 * @input: an input array (nullable)
 * @op: an operation
 *
 * Create a new #BDerivedVector based on an input #BData and a #BOperation.
 *
 * Returns: a #BData
 **/
BData *b_derived_vector_new(BData * input, BOperation * op)
{
  if (input)
    g_assert(B_IS_DATA(input));
  g_assert(B_IS_OPERATION(op));

  BData *d = g_object_new(B_TYPE_DERIVED_VECTOR, "operation", op, NULL);

  if (B_IS_DATA(d) && B_IS_DATA(input)) {
    g_object_set(d, "input", input, NULL);
  }
  return d;
}

/****************************************************************************/

/**
 * BDerivedMatrix:
 *
 * Object representing data.
 **/

struct _BDerivedMatrix {
  BMatrix base;
  BMatrixSize currsize;
  Derived der;
  double *cache;
  /* might need access to whether cache is OK */
};

static void b_derived_matrix_interface_init(BDerivedInterface * iface)
{
}

G_DEFINE_TYPE_WITH_CODE(BDerivedMatrix, b_derived_matrix, B_TYPE_MATRIX,
			G_IMPLEMENT_INTERFACE(B_TYPE_DERIVED,
					      b_derived_matrix_interface_init));

static void derived_matrix_finalize(GObject * obj)
{
  BDerivedMatrix *vec = (BDerivedMatrix *) obj;

  finalize_derived(&vec->der);

  GObjectClass *obj_class = G_OBJECT_CLASS(b_derived_matrix_parent_class);

  obj_class->finalize(obj);
}

static BMatrixSize derived_matrix_load_size(BMatrix * vec)
{
  BDerivedMatrix *vecd = (BDerivedMatrix *) vec;
  g_assert(vecd->der.op);
  BOperationClass *klass = (BOperationClass *) G_OBJECT_GET_CLASS(vecd->der.op);
  g_assert(klass);

  unsigned int newdim[2] = {0, 0};
  g_assert(klass->op_size);
  if (vecd->der.input) {
    int ndims = klass->op_size(vecd->der.op, vecd->der.input, newdim);
    g_return_val_if_fail(ndims == 2, *(BMatrixSize *) newdim);
  }

  return *(BMatrixSize *) newdim;
}

static double *derived_matrix_load_values(BMatrix * vec)
{
  BDerivedMatrix *vecs = (BDerivedMatrix *) vec;

  double *v = NULL;

  BMatrixSize size = b_matrix_get_size(vec);

  if (vecs->currsize.rows != size.rows
      || vecs->currsize.columns != size.columns) {
    g_clear_pointer(&vecs->cache,g_free);
    v = g_try_new0(double, size.rows * size.columns);
    vecs->currsize = size;
    vecs->cache = v;
  } else {
    v = vecs->cache;
  }
  g_return_val_if_fail (v != NULL, NULL);

  /* call op */
  BOperationClass *klass = B_OPERATION_GET_CLASS(vecs->der.op);
  if (vecs->der.task_data == NULL) {
    vecs->der.task_data =
      b_operation_create_task_data(vecs->der.op, vecs->der.input);
    } else {
    b_operation_update_task_data(vecs->der.op, vecs->der.task_data,
                                 vecs->der.input);
  }
  double *dout = klass->op_func(vecs->der.task_data);
  g_return_val_if_fail(dout!=NULL,NULL);
  memcpy(v, dout, size.rows * size.columns * sizeof(double));

  return v;
}

static double derived_matrix_get_value(BMatrix * vec, unsigned i, unsigned j)
{
  BMatrixSize size = b_matrix_get_size(vec);
  const double *d = b_matrix_get_values(vec);	/* fills the cache */
  g_return_val_if_fail(d!=NULL,NAN);
  return d[i * size.columns + j];
}

static void
op_cb2(GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  /* set outputs */
  GTask *task = G_TASK(res);
  g_task_propagate_pointer(task, NULL);
  BDerivedMatrix *d = (BDerivedMatrix *) user_data;
  d->der.running = FALSE;
  b_data_emit_changed(B_DATA(user_data));
}

static void on_input_changed_after2(BData * data, gpointer user_data)
{
  g_return_if_fail(B_IS_DATA(data));
  g_return_if_fail(B_IS_DERIVED_MATRIX(user_data));
  BDerivedMatrix *d = B_DERIVED_MATRIX(user_data);
  /* if shape changed, adjust length */
  /* FIXME: this just loads the length every time */
  derived_matrix_load_size(B_MATRIX(d));
  if (!d->der.autorun) {
    b_data_emit_changed(B_DATA(d));
  } else {
    if (d->der.running)
      return;
    d->der.running = TRUE;
    BOperationClass *klass = B_OPERATION_GET_CLASS(d->der.op);
    if (klass->thread_safe) {
      /* get task data, run in a thread */
      b_operation_update_task_data(d->der.op, d->der.task_data, data);
      b_operation_run_task(d->der.op, d->der.task_data, op_cb2, d);
    } else {
      /* load new values into the cache */
      derived_matrix_load_values(B_MATRIX(d));
      d->der.running = FALSE;
      b_data_emit_changed(B_DATA(d));
    }
  }
}

static void
on_op_changed2(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  BDerivedMatrix *d = B_DERIVED_MATRIX(user_data);
  derived_matrix_load_size(B_MATRIX(d));
  b_data_emit_changed(B_DATA(d));
}

static void
b_derived_matrix_set_property(GObject * object, guint property_id,
                              const GValue * value, GParamSpec * pspec)
{
  BDerivedMatrix *v = B_DERIVED_MATRIX(object);
  Derived *d = &v->der;

  switch (property_id) {
  case PROP_AUTORUN:
    d->autorun = g_value_get_boolean(value);
    break;
  case PROP_INPUT:
    derived_set_input(&v->der, g_value_get_object(value));
    if(d->input) {
      g_signal_connect(d->input, "changed",
                       G_CALLBACK(on_input_changed_after2), v);
      b_data_emit_changed(B_DATA(v));
    }
    break;
  case PROP_OPERATION:
    d->op = g_value_get_object(value);
    /* listen to "notify" from op for property changes */
    g_signal_connect(d->op, "notify", G_CALLBACK(on_op_changed2), v);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
b_derived_matrix_get_property(GObject * object, guint property_id,
                              GValue * value, GParamSpec * pspec)
{
  BDerivedMatrix *v = B_DERIVED_MATRIX(object);

  gboolean found = derived_get_property(&v->der,property_id,value);
  if(found)
    return;

  switch (property_id) {
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void b_derived_matrix_class_init(BDerivedMatrixClass * slice_klass)
{
  GObjectClass *gobject_class = (GObjectClass *) slice_klass;
  //YDataClass *ydata_klass = (YDataClass *) gobject_class;
  BMatrixClass *matrix_klass = (BMatrixClass *) gobject_class;

  gobject_class->finalize = derived_matrix_finalize;
  gobject_class->set_property = b_derived_matrix_set_property;
  gobject_class->get_property = b_derived_matrix_get_property;

  //ydata_klass->dup      = data_vector_slice_dup;
  matrix_klass->load_size = derived_matrix_load_size;
  matrix_klass->load_values = derived_matrix_load_values;
  matrix_klass->get_value = derived_matrix_get_value;

  g_object_class_override_property(gobject_class, PROP_AUTORUN, "autorun");
  g_object_class_override_property(gobject_class, PROP_INPUT, "input");
  g_object_class_override_property(gobject_class, PROP_OPERATION, "operation");
}

static void b_derived_matrix_init(BDerivedMatrix * der)
{
}

/**
 * b_derived_matrix_new:
 * @input: an input array (nullable)
 * @op: an operation
 *
 * Create a new #BDerivedMatrix based on an input #BData and a #BOperation.
 *
 * Returns: a #BData
 **/
BData *b_derived_matrix_new(BData * input, BOperation * op)
{
  if (input)
    g_assert(B_IS_DATA(input));
  g_assert(B_IS_OPERATION(op));

  BData *d = g_object_new(B_TYPE_DERIVED_MATRIX, "operation", op, NULL);

  if (B_IS_DATA(d) && B_IS_DATA(input)) {
    g_object_set(d, "input", input, NULL);
  }
  return d;
}
