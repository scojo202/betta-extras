/*
 * y-data-derived.c :
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
#include "y-data-derived.h"

/**
 * SECTION: y-data-derived
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

G_DEFINE_INTERFACE(YDerived, y_derived, Y_TYPE_DATA)

static void y_derived_default_init(YDerivedInterface * i)
{
	g_object_interface_install_property(i,
					    g_param_spec_boolean("autorun",
								 "Auto-run",
								 "Whether to run the operation immediately upon input data changing",
								 FALSE
								 /* default value */
								 ,
								 G_PARAM_READWRITE));

	g_object_interface_install_property(i,
					    g_param_spec_object("input",
								"Input data",
								"The input data",
								Y_TYPE_DATA,
								G_PARAM_READWRITE));
	g_object_interface_install_property(i,
					    g_param_spec_object("operation",
								"Operation",
								"The operation",
								Y_TYPE_OPERATION,
								G_PARAM_READWRITE));
}

static GParamSpec *scalar_properties[N_PROPERTIES] = { NULL, };

/**
 * YDerivedScalar:
 *
 *
 **/

typedef struct {
	YOperation *op;
	YData *input;
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
	if(d->input!=NULL) {
		g_object_unref(d->input);
	}
	if (d->task_data) {
		YOperationClass *klass =
        (YOperationClass *) G_OBJECT_GET_CLASS(d->op);
		if (klass->op_data_free) {
			klass->op_data_free(d->task_data);
		}
	}
	if (d->op) {
		g_object_unref(d->op);
	}
}

static gboolean
derived_get_property(Derived *d,
			      guint property_id,
			      GValue * value)
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

struct _YDerivedScalar {
	YScalar base;
	double cache;
	Derived der;
};

static void y_scalar_derived_interface_init(YDerivedInterface * iface)
{

}

G_DEFINE_TYPE_WITH_CODE(YDerivedScalar, y_derived_scalar, Y_TYPE_SCALAR,
			G_IMPLEMENT_INTERFACE(Y_TYPE_DERIVED,
					      y_scalar_derived_interface_init));

static
void y_derived_scalar_init(YDerivedScalar * self)
{

}

static double scalar_derived_get_value(YScalar * sca)
{
	YDerivedScalar *scas = (YDerivedScalar *) sca;

	if (sca == NULL)
		return NAN;

	YOperationClass *klass = Y_OPERATION_GET_CLASS(scas->der.op);

	unsigned int dims[3];

	g_return_val_if_fail(klass->op_size(scas->der.op,scas->der.input, dims)==0,NAN);

	/* call op */
	if (scas->der.task_data == NULL) {
		scas->der.task_data =
		    y_operation_create_task_data(scas->der.op, scas->der.input);
	} else {
		y_operation_update_task_data(scas->der.op, scas->der.task_data,
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
	YDerivedScalar *d = (YDerivedScalar *) user_data;
	d->der.running = FALSE;
	y_data_emit_changed(Y_DATA(user_data));
}

static void scalar_on_input_changed(YData * data, gpointer user_data)
{
	YDerivedScalar *d = Y_DERIVED_SCALAR(user_data);
	if (!d->der.autorun) {
		y_data_emit_changed(Y_DATA(d));
	} else {
		if (d->der.running)
			return;
		d->der.running = TRUE;
		YOperationClass *klass = Y_OPERATION_GET_CLASS(d->der.op);
		if (klass->thread_safe) {
			/* get task data, run in a thread */
			y_operation_update_task_data(d->der.op,
						     d->der.task_data, data);
			y_operation_run_task(d->der.op, d->der.task_data,
					     scalar_op_cb, d);
		} else {
			/* load new values into the cache */
			d->cache = scalar_derived_get_value(Y_SCALAR(d));
			d->der.running = FALSE;
			y_data_emit_changed(Y_DATA(d));
		}
	}
}

static void
scalar_on_op_changed(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
	YDerivedScalar *d = Y_DERIVED_SCALAR(user_data);
	y_data_emit_changed(Y_DATA(d));
}

static void scalar_derived_finalize(GObject * obj)
{
	YDerivedScalar *vec = (YDerivedScalar *) obj;
	finalize_derived(&vec->der);

	GObjectClass *obj_class = G_OBJECT_CLASS(y_derived_scalar_parent_class);

	obj_class->finalize(obj);
}

static void
y_scalar_derived_set_property(GObject * object,
			      guint property_id,
			      const GValue * value, GParamSpec * pspec)
{
	YDerivedScalar *s = Y_DERIVED_SCALAR(object);

	switch (property_id) {
	case PROP_AUTORUN:
		s->der.autorun = g_value_get_boolean(value);
		break;
	case PROP_INPUT:
		s->der.input = g_value_get_object(value);
		g_signal_connect(s->der.input, "changed",
				 G_CALLBACK(scalar_on_input_changed), s);
		y_data_emit_changed(Y_DATA(s));
		break;
	case PROP_OPERATION:
		s->der.op = g_value_get_object(value);
		/* listen to "notify" from op for property changes */
		g_signal_connect(s->der.op, "notify",
				 G_CALLBACK(scalar_on_op_changed), s);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
y_scalar_derived_get_property(GObject * object,
			      guint property_id,
			      GValue * value, GParamSpec * pspec)
{
	YDerivedScalar *s = Y_DERIVED_SCALAR(object);

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
void y_derived_scalar_class_init(YDerivedScalarClass * klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;
	YScalarClass *scalar_class = (YScalarClass *) klass;

	gobject_class->finalize = scalar_derived_finalize;
	gobject_class->set_property = y_scalar_derived_set_property;
	gobject_class->get_property = y_scalar_derived_get_property;

	scalar_class->get_value = scalar_derived_get_value;

	scalar_properties[PROP_AUTORUN] = g_param_spec_boolean("autorun",
							       "Auto-run",
							       "Whether to run the operation immediately upon input data changing",
							       FALSE
							       /* default value */
							       ,
							       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	scalar_properties[PROP_INPUT] =
	    g_param_spec_object("input", "Input data", "The input data",
				Y_TYPE_DATA, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	scalar_properties[PROP_OPERATION] =
	    g_param_spec_object("operation", "Operation", "The operation",
				Y_TYPE_OPERATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_override_property(gobject_class, PROP_AUTORUN,
					 "autorun");
	g_object_class_override_property(gobject_class, PROP_INPUT, "input");
	g_object_class_override_property(gobject_class, PROP_OPERATION,
					 "operation");
}

/**
 * y_derived_scalar_new:
 * @input: an input array
 * @op: an operation
 *
 * Create a new #YDerivedScalar based on an input #YData and a #YOperation.
 *
 * Returns: a #YData
 **/
YData *y_derived_scalar_new(YData * input, YOperation * op)
{
	if (input)
		g_assert(Y_IS_DATA(input));
	g_assert(Y_IS_OPERATION(op));

	YData *d = g_object_new(Y_TYPE_DERIVED_SCALAR, "operation", op, NULL);

	YDerivedScalar *vd = (YDerivedScalar *) d;

	if (Y_IS_DATA(d) && Y_IS_DATA(input)) {
		g_object_set(vd, "input", input, NULL);
	}
	return d;
}

/****************************************************************************/

/**
 * YDerivedVector:
 *
 * Object representing data.
 **/

struct _YDerivedVector {
	YVector base;
	unsigned int currlen;
	Derived der;
};

static GParamSpec *vector_properties[N_PROPERTIES] = { NULL, };

static void y_derived_vector_interface_init(YDerivedInterface * iface)
{

}

G_DEFINE_TYPE_WITH_CODE(YDerivedVector, y_derived_vector, Y_TYPE_VECTOR,
			G_IMPLEMENT_INTERFACE(Y_TYPE_DERIVED,
					      y_derived_vector_interface_init));

static void vector_derived_finalize(GObject * obj)
{
	YDerivedVector *vec = (YDerivedVector *) obj;
	finalize_derived(&vec->der);

	GObjectClass *obj_class = G_OBJECT_CLASS(y_derived_vector_parent_class);

	obj_class->finalize(obj);
}

#if 0
static YData *vector_derived_dup(YData const *src)
{
	YDerivedVector *dst = g_object_new(G_OBJECT_TYPE(src), NULL);
	YDerivedVector const *src_val = (YDerivedVector const *)src;
	return Y_DATA(dst);
}
#endif

static unsigned int vector_derived_load_len(YVector * vec)
{
	YDerivedVector *vecd = (YDerivedVector *) vec;
	g_assert(vecd->der.op);
	YOperationClass *klass =
	    (YOperationClass *) G_OBJECT_GET_CLASS(vecd->der.op);
	g_assert(klass);

	unsigned int newdim;
	g_assert(klass->op_size);
	if (vecd->der.input) {
		int ndims =
		    klass->op_size(vecd->der.op, vecd->der.input, &newdim);
		g_assert(ndims == 1);
	} else
		newdim = 0;

	return newdim;
}

static double *vector_derived_load_values(YVector * vec)
{
	YDerivedVector *vecs = (YDerivedVector *) vec;

	double *v = NULL;

	unsigned int len = y_vector_get_len(vec);

	if (vecs->currlen != len) {
		v=y_vector_replace_cache(vec,len);
		vecs->currlen = len;
	} else {
		v = y_vector_replace_cache(vec,vecs->currlen);
	}
	if (v == NULL)
		return NULL;

	/* call op */
	YOperationClass *klass = Y_OPERATION_GET_CLASS(vecs->der.op);
	if (vecs->der.task_data == NULL) {
		vecs->der.task_data =
		    y_operation_create_task_data(vecs->der.op, vecs->der.input);
	} else {
		y_operation_update_task_data(vecs->der.op, vecs->der.task_data,
					     vecs->der.input);
	}
	double *dout = klass->op_func(vecs->der.task_data);
	if (dout == NULL)
		return NULL;
	memcpy(v, dout, len * sizeof(double));

	return v;
}

static double vector_derived_get_value(YVector * vec, unsigned i)
{
	const double *d = y_vector_get_values(vec);	/* fills the cache */
	return d[i];
}

static void
op_cb(GObject * source_object, GAsyncResult * res, gpointer user_data)
{
	/* set outputs */
	GTask *task = G_TASK(res);
	g_task_propagate_pointer(task, NULL);
	YDerivedVector *d = (YDerivedVector *) user_data;
	d->der.running = FALSE;
	y_data_emit_changed(Y_DATA(user_data));
}

static void on_input_changed_after(YData * data, gpointer user_data)
{
	YDerivedVector *d = Y_DERIVED_VECTOR(user_data);
	/* if shape changed, adjust length */
	/* FIXME: this just loads the length every time */
	vector_derived_load_len(Y_VECTOR(d));
	if (!d->der.autorun) {
		y_data_emit_changed(Y_DATA(d));
	} else {
		if (d->der.running)
			return;
		d->der.running = TRUE;
		YOperationClass *klass = Y_OPERATION_GET_CLASS(d->der.op);
		if (klass->thread_safe) {
			/* get task data, run in a thread */
			y_operation_update_task_data(d->der.op,
						     d->der.task_data, data);
			y_operation_run_task(d->der.op, d->der.task_data, op_cb,
					     d);
		} else {
			/* load new values into the cache */
			vector_derived_load_values(Y_VECTOR(d));
			d->der.running = FALSE;
			y_data_emit_changed(Y_DATA(d));
		}
	}
}

static void
on_op_changed(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
	YDerivedVector *d = Y_DERIVED_VECTOR(user_data);
	vector_derived_load_len(Y_VECTOR(d));
	y_data_emit_changed(Y_DATA(d));
}

static void
y_derived_vector_set_property(GObject * object,
			      guint property_id,
			      const GValue * value, GParamSpec * pspec)
{
	YDerivedVector *v = Y_DERIVED_VECTOR(object);
	Derived *d = &v->der;

	switch (property_id) {
	case PROP_AUTORUN:
		d->autorun = g_value_get_boolean(value);
		break;
	case PROP_INPUT:
		/* unref old one */
		if(d->input != NULL) {
		  g_object_unref(d->input);
		}
		d->input = g_value_dup_object(value);
		d->handler = g_signal_connect(d->input, "changed",
				 G_CALLBACK(on_input_changed_after), v);
		y_data_emit_changed(Y_DATA(v));
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
y_derived_vector_get_property(GObject * object,
			      guint property_id,
			      GValue * value, GParamSpec * pspec)
{
	YDerivedVector *v = Y_DERIVED_VECTOR(object);

	gboolean found = derived_get_property(&v->der,property_id,value);
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

static void y_derived_vector_class_init(YDerivedVectorClass * slice_klass)
{
	GObjectClass *gobject_class = (GObjectClass *) slice_klass;
	//YDataClass *ydata_klass = (YDataClass *) gobject_class;
	YVectorClass *vector_klass = (YVectorClass *) gobject_class;

	gobject_class->finalize = vector_derived_finalize;
	gobject_class->set_property = y_derived_vector_set_property;
	gobject_class->get_property = y_derived_vector_get_property;

	//ydata_klass->dup      = data_vector_slice_dup;
	vector_klass->load_len = vector_derived_load_len;
	vector_klass->load_values = vector_derived_load_values;
	vector_klass->get_value = vector_derived_get_value;

	vector_properties[PROP_AUTORUN] = g_param_spec_boolean("autorun",
							       "Auto-run",
							       "Whether to run the operation immediately upon input data changing",
							       FALSE
							       /* default value */
							       ,
							       G_PARAM_READWRITE);
	vector_properties[PROP_INPUT] =
	    g_param_spec_object("input", "Input data", "The input data",
				Y_TYPE_DATA, G_PARAM_READWRITE);
	vector_properties[PROP_OPERATION] =
	    g_param_spec_object("operation", "Operation", "The operation",
				Y_TYPE_OPERATION, G_PARAM_READWRITE);

	g_object_class_override_property(gobject_class, PROP_AUTORUN,
					 "autorun");
	g_object_class_override_property(gobject_class, PROP_INPUT, "input");
	g_object_class_override_property(gobject_class, PROP_OPERATION,
					 "operation");
}

static void y_derived_vector_init(YDerivedVector * der)
{
}

/**
 * y_derived_vector_new:
 * @input: an input array (nullable)
 * @op: an operation
 *
 * Create a new #YDerivedVector based on an input #YData and a #YOperation.
 *
 * Returns: a #YData
 **/
YData *y_derived_vector_new(YData * input, YOperation * op)
{
	if (input)
		g_assert(Y_IS_DATA(input));
	g_assert(Y_IS_OPERATION(op));

	YData *d = g_object_new(Y_TYPE_DERIVED_VECTOR, "operation", op, NULL);

	YDerivedVector *vd = (YDerivedVector *) d;

	if (Y_IS_DATA(d) && Y_IS_DATA(input)) {
		g_object_set(vd, "input", input, NULL);
	}
	return d;
}

/****************************************************************************/

/**
 * YDerivedMatrix:
 *
 * Object representing data.
 **/

struct _YDerivedMatrix {
	YMatrix base;
	YMatrixSize currsize;
	Derived der;
	double *cache;
	/* might need access to whether cache is OK */
};

static GParamSpec *matrix_properties[N_PROPERTIES] = { NULL, };

static void y_derived_matrix_interface_init(YDerivedInterface * iface)
{

}

G_DEFINE_TYPE_WITH_CODE(YDerivedMatrix, y_derived_matrix, Y_TYPE_MATRIX,
			G_IMPLEMENT_INTERFACE(Y_TYPE_DERIVED,
					      y_derived_matrix_interface_init));

static void derived_matrix_finalize(GObject * obj)
{
	YDerivedMatrix *vec = (YDerivedMatrix *) obj;

	finalize_derived(&vec->der);

	GObjectClass *obj_class = G_OBJECT_CLASS(y_derived_matrix_parent_class);

	obj_class->finalize(obj);
}

static YMatrixSize derived_matrix_load_size(YMatrix * vec)
{
	YDerivedMatrix *vecd = (YDerivedMatrix *) vec;
	g_assert(vecd->der.op);
	YOperationClass *klass =
	    (YOperationClass *) G_OBJECT_GET_CLASS(vecd->der.op);
	g_assert(klass);

	unsigned int newdim[2];
	g_assert(klass->op_size);
	if (vecd->der.input) {
		int ndims =
		    klass->op_size(vecd->der.op, vecd->der.input, newdim);
		g_assert(ndims == 2);
	} else {
		newdim[0] = 0;
		newdim[1] = 0;
	}

	YMatrixSize size;
	size.columns = newdim[0];
	size.rows = newdim[1];

	return size;
}

static double *derived_matrix_load_values(YMatrix * vec)
{
	YDerivedMatrix *vecs = (YDerivedMatrix *) vec;

	double *v = NULL;

	YMatrixSize size = y_matrix_get_size(vec);

	//g_message("load values, len is %u",len);

	if (vecs->currsize.rows != size.rows
	    || vecs->currsize.columns != size.columns) {
		if (vecs->cache)
			g_free(vecs->cache);
		v = g_new0(double, size.rows * size.columns);
		vecs->currsize = size;
		vecs->cache = v;
	} else {
		v = vecs->cache;
	}
	if (v == NULL)
		return NULL;

	/* call op */
	YOperationClass *klass = Y_OPERATION_GET_CLASS(vecs->der.op);
	if (vecs->der.task_data == NULL) {
		vecs->der.task_data =
		    y_operation_create_task_data(vecs->der.op, vecs->der.input);
	} else {
		y_operation_update_task_data(vecs->der.op, vecs->der.task_data,
					     vecs->der.input);
	}
	double *dout = klass->op_func(vecs->der.task_data);
	if (dout == NULL)
		return NULL;
	memcpy(v, dout, size.rows * size.columns * sizeof(double));

	return v;
}

static double derived_matrix_get_value(YMatrix * vec, unsigned i, unsigned j)
{
	YMatrixSize size = y_matrix_get_size(vec);
	const double *d = y_matrix_get_values(vec);	/* fills the cache */
	return d[i * size.columns + j];
}

static void
op_cb2(GObject * source_object, GAsyncResult * res, gpointer user_data)
{
	/* set outputs */
	GTask *task = G_TASK(res);
	g_task_propagate_pointer(task, NULL);
	YDerivedMatrix *d = (YDerivedMatrix *) user_data;
	d->der.running = FALSE;
	y_data_emit_changed(Y_DATA(user_data));
}

static void on_input_changed_after2(YData * data, gpointer user_data)
{
	YDerivedMatrix *d = Y_DERIVED_MATRIX(user_data);
	/* if shape changed, adjust length */
	/* FIXME: this just loads the length every time */
	derived_matrix_load_size(Y_MATRIX(d));
	if (!d->der.autorun) {
		y_data_emit_changed(Y_DATA(d));
	} else {
		if (d->der.running)
			return;
		d->der.running = TRUE;
		YOperationClass *klass = Y_OPERATION_GET_CLASS(d->der.op);
		if (klass->thread_safe) {
			/* get task data, run in a thread */
			y_operation_update_task_data(d->der.op,
						     d->der.task_data, data);
			y_operation_run_task(d->der.op, d->der.task_data,
					     op_cb2, d);
		} else {
			/* load new values into the cache */
			derived_matrix_load_values(Y_MATRIX(d));
			d->der.running = FALSE;
			y_data_emit_changed(Y_DATA(d));
		}
	}
}

static void
on_op_changed2(GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
	YDerivedMatrix *d = Y_DERIVED_MATRIX(user_data);
	derived_matrix_load_size(Y_MATRIX(d));
	y_data_emit_changed(Y_DATA(d));
}

static void
y_derived_matrix_set_property(GObject * object,
			      guint property_id,
			      const GValue * value, GParamSpec * pspec)
{
	YDerivedMatrix *v = Y_DERIVED_MATRIX(object);
	Derived *d = &v->der;

	switch (property_id) {
	case PROP_AUTORUN:
		d->autorun = g_value_get_boolean(value);
		break;
	case PROP_INPUT:
		d->input = g_value_get_object(value);
		g_signal_connect(d->input, "changed",
				 G_CALLBACK(on_input_changed_after2), v);
		y_data_emit_changed(Y_DATA(v));
		break;
	case PROP_OPERATION:
		d->op = g_value_get_object(value);
		/* listen to "notify" from op for property changes */
		g_signal_connect(d->op, "notify", G_CALLBACK(on_op_changed2),
				 v);
		break;
	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
y_derived_matrix_get_property(GObject * object,
			      guint property_id,
			      GValue * value, GParamSpec * pspec)
{
	YDerivedMatrix *v = Y_DERIVED_MATRIX(object);

	gboolean found = derived_get_property(&v->der,property_id,value);
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

static void y_derived_matrix_class_init(YDerivedMatrixClass * slice_klass)
{
	GObjectClass *gobject_class = (GObjectClass *) slice_klass;
	//YDataClass *ydata_klass = (YDataClass *) gobject_class;
	YMatrixClass *matrix_klass = (YMatrixClass *) gobject_class;

	gobject_class->finalize = derived_matrix_finalize;
	gobject_class->set_property = y_derived_matrix_set_property;
	gobject_class->get_property = y_derived_matrix_get_property;

	//ydata_klass->dup      = data_vector_slice_dup;
	matrix_klass->load_size = derived_matrix_load_size;
	matrix_klass->load_values = derived_matrix_load_values;
	matrix_klass->get_value = derived_matrix_get_value;

	matrix_properties[PROP_AUTORUN] = g_param_spec_boolean("autorun",
							       "Auto-run",
							       "Whether to run the operation immediately upon input data changing",
							       FALSE
							       /* default value */
							       ,
							       G_PARAM_READWRITE);
	matrix_properties[PROP_INPUT] =
	    g_param_spec_object("input", "Input data", "The input data",
				Y_TYPE_DATA, G_PARAM_READWRITE);
	matrix_properties[PROP_OPERATION] =
	    g_param_spec_object("operation", "Operation", "The operation",
				Y_TYPE_OPERATION, G_PARAM_READWRITE);

	g_object_class_override_property(gobject_class, PROP_AUTORUN,
					 "autorun");
	g_object_class_override_property(gobject_class, PROP_INPUT, "input");
	g_object_class_override_property(gobject_class, PROP_OPERATION,
					 "operation");
}

static void y_derived_matrix_init(YDerivedMatrix * der)
{
}

/**
 * y_derived_matrix_new:
 * @input: an input array (nullable)
 * @op: an operation
 *
 * Create a new #YDerivedMatrix based on an input #YData and a #YOperation.
 *
 * Returns: a #YData
 **/
YData *y_derived_matrix_new(YData * input, YOperation * op)
{
	if (input)
		g_assert(Y_IS_DATA(input));
	g_assert(Y_IS_OPERATION(op));

	YData *d = g_object_new(Y_TYPE_DERIVED_MATRIX, "operation", op, NULL);

	YDerivedMatrix *vd = (YDerivedMatrix *) d;

	if (Y_IS_DATA(d) && Y_IS_DATA(input)) {
		g_object_set(vd, "input", input, NULL);
	}
	return d;
}
