/*
 * y-linear-range.c :
 *
 * Copyright (C) 2003-2005 Jody Goldberg (jody@gnome.org)
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

#include "y-linear-range.h"
#include <math.h>

/**
 * SECTION: y-linear-range
 * @short_description: Vector for equally spaced data.
 *
 * A vector y_i = v_0 + i*dv, where i ranges from 0 to n-1.
 */

struct _YLinearRangeVector {
	YVector	 base;
	double v0;
	double dv;
	unsigned n;
};

G_DEFINE_TYPE (YLinearRangeVector, y_linear_range_vector, Y_TYPE_VECTOR);

static GObjectClass *vector_parent_klass;

static YData *
linear_range_vector_dup (YData *src)
{
	YLinearRangeVector *dst = g_object_new (G_OBJECT_TYPE (src), NULL);
	YLinearRangeVector const *src_val = (YLinearRangeVector const *)src;
	dst->v0 = src_val->v0;
	dst->dv = src_val->dv;
	y_linear_range_vector_set_length(dst, src_val->n);
	return Y_DATA (dst);
}

static unsigned int
linear_range_vector_load_len (YVector *vec)
{
	return ((YLinearRangeVector *)vec)->n;
}

#define get_val(d,i) (d->v0+i*(d->dv))

static double *
linear_range_vector_load_values (YVector *vec)
{
	YLinearRangeVector *val = (YLinearRangeVector *)vec;
	int i = val->n;

  g_assert(isfinite(val->v0));
  g_assert(isfinite(val->dv));

	double *values = y_vector_replace_cache(vec,val->n);

	while (i-- > 0) {
		values[i]=get_val(val,i);
	}
	return values;
}

static double
linear_range_vector_get_value (YVector *vec, unsigned i)
{
	YLinearRangeVector const *val = (YLinearRangeVector const *)vec;
	g_return_val_if_fail (val != NULL && i < val->n, NAN);
	return get_val(val,i);
}

static gboolean
linear_range_vector_has_value (YData *dat)
{
	YLinearRangeVector const *val = (YLinearRangeVector const *)dat;
	return (isfinite(val->v0) && isfinite(val->dv));
}

static void
y_linear_range_vector_init(YLinearRangeVector *v) {}

static void
y_linear_range_vector_class_init (YLinearRangeVectorClass *klass)
{
	GObjectClass *gobject_klass = (GObjectClass *) klass;
	YDataClass *ydata_klass = (YDataClass *) klass;
	ydata_klass->has_value = linear_range_vector_has_value;
	YVectorClass *vector_klass = (YVectorClass *) klass;

	vector_parent_klass = g_type_class_peek_parent (gobject_klass);
	ydata_klass->dup	= linear_range_vector_dup;
	vector_klass->load_len    = linear_range_vector_load_len;
	vector_klass->load_values = linear_range_vector_load_values;
	vector_klass->get_value   = linear_range_vector_get_value;
}

/**
 * y_linear_range_vector_set_length :
 * @d: a #YLinearRangeVector
 * @n: length
 *
 * Set the length of @d.
 *
 **/

void y_linear_range_vector_set_length(YLinearRangeVector *d, unsigned int n)
{
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(d));

	if(n!=d->n) {
		d->n = n;
		y_data_emit_changed(Y_DATA(d));
	}
}

/**
 * y_linear_range_vector_set_pars :
 * @d: a #YLinearRangeVector
 * @v0: first value
 * @dv: step size
 *
 * Set the initial value @v0 and step size @dv of @d.
 *
 **/

void y_linear_range_vector_set_pars(YLinearRangeVector *d, double v0, double dv)
{
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(d));
	d->v0 = v0;
	d->dv = dv;
	y_data_emit_changed(Y_DATA(d));
}

/**
 * y_linear_range_vector_set_v0 :
 * @d: a #YLinearRangeVector
 * @v0: first value
 *
 * Set the initial value @v0 of @d.
 *
 **/

void y_linear_range_vector_set_v0(YLinearRangeVector *d, double v0)
{
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(d));
	d->v0 = v0;
	y_data_emit_changed(Y_DATA(d));
}

/**
 * y_linear_range_vector_set_dv :
 * @d: a #YLinearRangeVector
 * @dv: step size
 *
 * Set the step size @dv of @d.
 *
 **/

void y_linear_range_vector_set_dv(YLinearRangeVector *d, double dv)
{
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(d));
	d->dv = dv;
	y_data_emit_changed(Y_DATA(d));
}

double y_linear_range_vector_get_v0(YLinearRangeVector *d)
{
	return d->v0;
}

double y_linear_range_vector_get_dv(YLinearRangeVector *d)
{
	return d->dv;
}

/**
 * y_linear_range_vector_new :
 * @v0: first value
 * @dv: step size
 * @n: length
 *
 * Create a new #YLinearRangeVector.
 *
 * Returns: a new #YLinearRangeVector as a #YData
 **/

YData *
y_linear_range_vector_new (double v0, double dv, unsigned n)
{
	YLinearRangeVector *res = g_object_new (Y_TYPE_LINEAR_RANGE_VECTOR, NULL);
	res->v0 = v0;
	res->dv = dv;
	y_linear_range_vector_set_length(res,n);
	return Y_DATA (res);
}

/******************************************************************/

struct _YFourierLinearRangeVector {
	YVector     base;
	YLinearRangeVector *range;
	unsigned int n;
	gboolean inverse;
};

G_DEFINE_TYPE (YFourierLinearRangeVector, y_fourier_linear_range_vector, Y_TYPE_VECTOR);

static void
fourier_linear_range_vector_finalize (GObject *obj)
{
	YFourierLinearRangeVector *vec = (YFourierLinearRangeVector *)obj;

	g_signal_handlers_disconnect_by_data(vec->range, obj);
	g_object_unref(vec->range);

	(*vector_parent_klass->finalize) (obj);
}

static YData *
fourier_linear_range_vector_dup (YData *src)
{
	YFourierLinearRangeVector *dst = g_object_new (G_OBJECT_TYPE (src), NULL);
	YFourierLinearRangeVector const *src_val = (YFourierLinearRangeVector const *)src;
	dst->range = g_object_ref(src_val->range);
	return Y_DATA (dst);
}

static unsigned int
fourier_linear_range_vector_load_len (YVector *vec)
{
	YFourierLinearRangeVector *f = (YFourierLinearRangeVector *) vec;
	return f->range->n/2 + 1;
}

static double *
fourier_linear_range_vector_load_values (YVector *vec)
{
	YFourierLinearRangeVector *val = (YFourierLinearRangeVector *)vec;
	YLinearRangeVector *range = val->range;
	int i = range->n/2 + 1;

	g_assert(isfinite(range->v0));
	g_assert(isfinite(range->dv));
	if(range->n ==0 )
		return NULL;

	double *values = y_vector_replace_cache(vec,range->n/2+1);

	double df = 1./range->n/range->dv;
	if(val->inverse) {
		df *= 2*M_PI;
	}

	while (i-- > 0) {
		values[i]=i*df;
	}
	return values;
}

static double
fourier_linear_range_vector_get_value (YVector *vec, unsigned i)
{
	YFourierLinearRangeVector const *val = (YFourierLinearRangeVector const *)vec;
	YLinearRangeVector *range = val->range;

	double df = 1./range->n/range->dv;
	if(val->inverse) {
		df *= 2*M_PI;
	}
	return i*df;
}

static gboolean
fourier_linear_range_vector_has_value (YData *dat)
{
	YFourierLinearRangeVector const *val = (YFourierLinearRangeVector const *)dat;
	return linear_range_vector_has_value(Y_DATA(val->range));
}

static void
y_fourier_linear_range_vector_init(YFourierLinearRangeVector *v) {}

static void
y_fourier_linear_range_vector_class_init (YFourierLinearRangeVectorClass *klass)
{
	GObjectClass *gobject_klass = (GObjectClass *) klass;
	YDataClass *ydata_klass = (YDataClass *) klass;
	ydata_klass->has_value = fourier_linear_range_vector_has_value;
	YVectorClass *vector_klass = (YVectorClass *) klass;

	vector_parent_klass = g_type_class_peek_parent (gobject_klass);
	gobject_klass->finalize = fourier_linear_range_vector_finalize;
	ydata_klass->dup    = fourier_linear_range_vector_dup;
	vector_klass->load_len    = fourier_linear_range_vector_load_len;
	vector_klass->load_values = fourier_linear_range_vector_load_values;
	vector_klass->get_value   = fourier_linear_range_vector_get_value;
}

static void
on_range_changed (YData *d, gpointer user_data)
{
	YData *dat = Y_DATA(user_data);
	YFourierLinearRangeVector *res = Y_FOURIER_LINEAR_RANGE_VECTOR(user_data);
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(res->range));
	g_assert(d == Y_DATA(res->range));
	if(res->n != res->range->n/2 + 1) {
		res->n = res->range->n/2 + 1;
	}
	y_data_emit_changed(dat);
}

void
y_fourier_linear_range_vector_set_inverse(YFourierLinearRangeVector *v, gboolean val)
{
	if(v->inverse!=val) {
		v->inverse = val;
		y_data_emit_changed(Y_DATA(v));
	}
}

/**
 * y_fourier_linear_range_vector_new :
 * @v: real space range
 *
 * Create a new #YFourierLinearRangeVector connected to @v.
 *
 * Returns: a new #YFourierLinearRangeVector as a #YData
 **/

YData *
y_fourier_linear_range_vector_new (YLinearRangeVector *v)
{
	YFourierLinearRangeVector *res = g_object_new (Y_TYPE_FOURIER_LINEAR_RANGE_VECTOR, NULL);
	g_assert(Y_IS_LINEAR_RANGE_VECTOR(v));
	res->range = g_object_ref_sink(v);
	res->n = res->range->n/2 + 1;
	g_signal_connect_after(res->range,"changed",G_CALLBACK(on_range_changed),res);
	return Y_DATA (res);
}
