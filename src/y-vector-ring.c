/*
 * y-vector-ring.c :
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

#include "y-vector-ring.h"
#include <math.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>

/**
 * SECTION: y-vector-ring
 * @short_description: A vector that grows as scalar values are added, up to a
 * maximum length, after which it throws away the first elements to make room
 * for more.
 *
 * Data class #YRingVector
 *
 */

/**
 * YRingVector:
 *
 * A YVector that grows up to a maximum length @nmax.
 **/

struct _YRingVector {
	YVector base;
	unsigned n;
	unsigned int nmax;
	double *val;
	YScalar *source;
	gulong handler;
	YRingVector *timestamps;
};

G_DEFINE_TYPE(YRingVector, y_ring_vector, Y_TYPE_VECTOR);

static void y_ring_vector_finalize(GObject * obj)
{
	YRingVector *vec = (YRingVector *) obj;
	if (vec->val)
		g_free(vec->val);
	if (vec->source) {
		g_object_unref(vec->source);
		g_signal_handler_disconnect(vec->source, vec->handler);
	}

	GObjectClass *obj_class = G_OBJECT_CLASS(y_ring_vector_parent_class);

	(*obj_class->finalize) (obj);
}

static YData *y_ring_vector_dup(YData * src)
{
	YRingVector *dst = g_object_new(G_OBJECT_TYPE(src), NULL);
	YRingVector const *src_val = (YRingVector const *)src;
	dst->val = g_new0(double, src_val->nmax);
	memcpy(dst->val, src_val->val, src_val->n * sizeof(double));
	dst->n = src_val->n;
	return Y_DATA(dst);
}

static unsigned int y_ring_vector_load_len(YVector * vec)
{
	return ((YRingVector *) vec)->n;
}

static double *y_ring_vector_load_values(YVector * vec)
{
	YRingVector const *val = (YRingVector const *)vec;

	return val->val;
}

static double y_ring_vector_get_value(YVector * vec, unsigned i)
{
	YRingVector const *val = (YRingVector const *)vec;
	g_return_val_if_fail(val != NULL && val->val != NULL
			     && i < val->n, NAN);
	return val->val[i];
}

static double *
y_ring_vector_replace_cache(YVector *vec, unsigned len)
{
	YRingVector const *r = (YRingVector const *)vec;

	if(len!=r->n) {
		g_warning("Trying to replace cache in YRingVector.");
	}
	return r->val;
}

static void y_ring_vector_class_init(YRingVectorClass * val_klass)
{
	YDataClass *ydata_klass = (YDataClass *) val_klass;
	YVectorClass *vector_klass = (YVectorClass *) val_klass;
	GObjectClass *gobject_klass = (GObjectClass *) val_klass;

	gobject_klass->finalize = y_ring_vector_finalize;
	ydata_klass->dup = y_ring_vector_dup;
	vector_klass->load_len = y_ring_vector_load_len;
	vector_klass->load_values = y_ring_vector_load_values;
	vector_klass->get_value = y_ring_vector_get_value;
	vector_klass->replace_cache = y_ring_vector_replace_cache;
}

static void y_ring_vector_init(YRingVector * val)
{
}

/**
 * y_ring_vector_new:
 * @nmax: maximum length of array
 * @n: initial length of array
 * @track_timestamps: whether to save timestamps for each element
 *
 * If @n is not zero, elements are initialized to zero.
 *
 * Returns: a #YData
 *
 **/
YData *y_ring_vector_new(unsigned nmax, unsigned n, gboolean track_timestamps)
{
	YRingVector *res = g_object_new(Y_TYPE_RING_VECTOR, NULL);
	res->val = g_new0(double, nmax);
	res->n = n;
	res->nmax = nmax;
	if(track_timestamps) {
		res->timestamps = Y_RING_VECTOR(g_object_ref_sink(y_ring_vector_new(nmax,n,FALSE)));
	}
	return Y_DATA(res);
}

/**
 * y_ring_vector_append :
 * @d: #YRingVector
 * @val: new value
 *
 * Append a new value to the vector.
 *
 **/
void y_ring_vector_append(YRingVector * d, double val)
{
	g_assert(Y_IS_RING_VECTOR(d));
	unsigned int l = MIN(d->nmax, y_vector_get_len(Y_VECTOR(d)));
	double *frames = d->val;
	if (l < d->nmax) {
		frames[l] = val;
		y_ring_vector_set_length(d, l + 1);
	}
	else if (l == d->nmax) {
		memmove(frames, &frames[1], (l - 1) * sizeof(double));
		frames[l - 1] = val;
	}
	else {
		return;
	}
	if(d->timestamps) {
		y_ring_vector_append(d->timestamps,((double)g_get_real_time())/1e6);
	}
	y_data_emit_changed(Y_DATA(d));
}

/**
 * y_ring_vector_append_array :
 * @d: #YRingVector
 * @arr: (array length=len): array
 * @len: array length
 *
 * Append a new array of values @arr to the vector.
 *
 **/
void y_ring_vector_append_array(YRingVector * d, double *arr, int len)
{
	g_assert(Y_IS_RING_VECTOR(d));
	g_assert(arr);
	g_assert(len>=0);
	unsigned int l = MIN(d->nmax, y_vector_get_len(Y_VECTOR(d)));
	double *frames = d->val;
	int i;
	double now = ((double)g_get_real_time())/1e6;
	if (l + len < d->nmax) {
		for (i = 0; i < len; i++) {
			frames[i + l] = arr[i];
			if(d->timestamps) {
				y_ring_vector_append(d->timestamps,now);
			}
		}
		y_ring_vector_set_length(d, l + len);
	}
	/*else {
	   memmove(frames, &frames[1], (l-1)*sizeof(double));
	   frames[l-1]=val;
	   } */
	else
		return;
	y_data_emit_changed(Y_DATA(d));
}

static void on_source_changed(YData * data, gpointer user_data)
{
	YRingVector *d = Y_RING_VECTOR(user_data);
	YScalar *source = Y_SCALAR(data);
	y_ring_vector_append(d, y_scalar_get_value(source));
}

/**
 * y_ring_vector_set_source :
 * @d: #YRingVector
 * @source: a #YScalar
 *
 * Set a source for the #YRingVector. When the source emits a "changed" signal,
 * a new value will be appended to the vector.
 **/
void y_ring_vector_set_source(YRingVector * d, YScalar * source)
{
	g_assert(Y_IS_RING_VECTOR(d));
	g_assert(Y_IS_SCALAR(source));
	if (d->source) {
		g_object_unref(d->source);
		g_signal_handler_disconnect(d->source, d->handler);
	}
	if (Y_IS_SCALAR(source)) {
		d->source = g_object_ref_sink(source);
	} else if (source == NULL) {
		d->source = NULL;
		return;
	}
	d->handler =
	    g_signal_connect_after(source, "changed",
				   G_CALLBACK(on_source_changed), d);
}

/**
 * y_ring_vector_set_length :
 * @d: #YRingVector
 * @newlength: new length of array
 *
 * Set the current length of the #YRingVector to a new value. If the new
 * length is longer than the previous length, tailing elements are set to
 * zero.
 **/
void y_ring_vector_set_length(YRingVector * d, unsigned newlength)
{
	g_assert(Y_IS_RING_VECTOR(d));
	if (newlength <= d->nmax) {
		d->n = newlength;
		y_data_emit_changed(Y_DATA(d));
		if(d->timestamps) {
			y_ring_vector_set_length(d->timestamps,newlength);
		}
	}
	/* TODO: set tailing elements to zero */
}

/**
 * y_ring_vector_get_timestamps :
 * @d: #YRingVector
 *
 * Get timestamps for when each element was added.
 *
 * Returns: (transfer none): The timestamps.
 **/

YRingVector *y_ring_vector_get_timestamps(YRingVector *d)
{
	g_assert(Y_IS_RING_VECTOR(d));
	return d->timestamps;
}

/********************************************************************/

/**
 * YRingMatrix:
 *
 * A YMatrix that grows up to a maximum height @rmax.
 **/

struct _YRingMatrix {
	YMatrix base;
	unsigned nr, nc;
	unsigned int rmax;
	double *val;
	YVector *source;
	gulong handler;
	YRingVector *timestamps;
};

G_DEFINE_TYPE(YRingMatrix, y_ring_matrix, Y_TYPE_MATRIX);

static void ring_matrix_finalize(GObject * obj)
{
	YRingMatrix *vec = (YRingMatrix *) obj;
	if (vec->val) {
		g_free(vec->val);
	}
	if (vec->source) {
		g_object_unref(vec->source);
		g_signal_handler_disconnect(vec->source, vec->handler);
	}

	GObjectClass *obj_class = G_OBJECT_CLASS(y_ring_matrix_parent_class);

	(*obj_class->finalize) (obj);
}

static YData *ring_matrix_dup(YData * src)
{
	YRingMatrix *dst = g_object_new(G_OBJECT_TYPE(src), NULL);
	YRingMatrix const *src_val = (YRingMatrix const *)src;
	dst->val = g_new(double, src_val->nc*src_val->rmax);
	memcpy(dst->val, src_val->val, src_val->nc*src_val->nr * sizeof(double));
	dst->nr = src_val->nr;
	dst->nc = src_val->nc;
	return Y_DATA(dst);
}

static YMatrixSize ring_matrix_load_size(YMatrix * mat)
{
	YRingMatrix *ring = (YRingMatrix *) mat;
	YMatrixSize s;
	s.rows = ring->nr;
	s.columns = ring->nc;
	return s;
}

static double *ring_matrix_load_values(YMatrix * vec)
{
	YRingMatrix const *val = (YRingMatrix const *)vec;

	return val->val;
}

static double ring_matrix_get_value(YMatrix * vec, unsigned i, unsigned j)
{
	YRingMatrix const *val = (YRingMatrix const *)vec;
	g_return_val_if_fail(val != NULL && val->val != NULL
                         && i < val->nr && j<val->nc, NAN);
	return val->val[i * val->nc + j];
}

static double *
y_ring_matrix_replace_cache(YMatrix *mat, unsigned len)
{
	YRingMatrix const *r = (YRingMatrix const *)mat;

	if(len!=r->nr*r->nc) {
		g_warning("Trying to replace cache in YRingMatrix.");
	}
	return r->val;
}

static void y_ring_matrix_class_init(YRingMatrixClass * val_klass)
{
	YDataClass *ydata_klass = (YDataClass *) val_klass;
	YMatrixClass *matrix_klass = (YMatrixClass *) val_klass;
	GObjectClass *gobject_klass = (GObjectClass *) val_klass;

	gobject_klass->finalize = ring_matrix_finalize;
	ydata_klass->dup = ring_matrix_dup;
	matrix_klass->load_size = ring_matrix_load_size;
	matrix_klass->load_values = ring_matrix_load_values;
	matrix_klass->get_value = ring_matrix_get_value;
	matrix_klass->replace_cache = y_ring_matrix_replace_cache;
}

static void y_ring_matrix_init(YRingMatrix * val)
{
}

/**
 * y_ring_matrix_new:
 * @c: number of columns
 * @rmax: maximum number of rows
 * @r: initial number of rows
 * @track_timestamps: whether to save timestamps for each element
 *
 * If @r is not zero, elements are initialized to zero.
 *
 * Returns: a #YData
 *
 **/
YData *y_ring_matrix_new(unsigned c, unsigned rmax, unsigned r, gboolean track_timestamps)
{
	YRingMatrix *res = g_object_new(Y_TYPE_RING_MATRIX, NULL);
	res->val = g_new0(double, rmax*c);
	res->nr = r;
	res->nc = c;
	res->rmax = rmax;
	if(track_timestamps) {
		res->timestamps = Y_RING_VECTOR(g_object_ref_sink(y_ring_vector_new(rmax,r,FALSE)));
	}
	return Y_DATA(res);
}

/**
 * y_ring_matrix_append :
 * @d: #YRingMatrix
 * @values: (array length=len): array
 * @len: array length
 *
 * Append a new row to the matrix.
 *
 **/
void y_ring_matrix_append(YRingMatrix * d, const double *values, unsigned len)
{
	g_assert(Y_IS_RING_MATRIX(d));
	g_assert(values);
	g_return_if_fail(len<=d->nc);
	unsigned int l = MIN(d->rmax, y_matrix_get_rows(Y_MATRIX(d)));
	double *frames = d->val;
	int k;
	if (l < d->rmax) {
		for(k=0;k<len;k++) {
			frames[l*d->nc+k] = values[k];
		}
		y_ring_matrix_set_rows(d, l + 1);
	}
	else if (l == d->rmax) {
		memmove(frames, &frames[d->nc], (l - 1) * d->nc*sizeof(double));
		for(k=0;k<len;k++) {
			frames[(l-1)*d->nc+k] = values[k];
		}
	}
	else {
			return;
	}
	if(d->timestamps) {
		y_ring_vector_append(d->timestamps,((double)g_get_real_time())/1e6);
	}
	y_data_emit_changed(Y_DATA(d));
}

static void on_vector_source_changed(YData * data, gpointer user_data)
{
	YRingMatrix *d = Y_RING_MATRIX(user_data);
	YVector *source = Y_VECTOR(data);
	y_ring_matrix_append(d, y_vector_get_values(source), y_vector_get_len(source));
}

/**
 * y_ring_matrix_set_source :
 * @d: #YRingMatrix
 * @source: (nullable): a #YVector or %NULL
 *
 * Set a source for the #YRingMatrix. When the source emits a "changed" signal,
 * a new row will be appended to the matrix.
 **/
void y_ring_matrix_set_source(YRingMatrix * d, YVector * source)
{
	g_assert(Y_IS_RING_MATRIX(d));
	g_assert(Y_IS_VECTOR(source) || source==NULL);
	if (d->source) {
		g_object_unref(d->source);
		g_signal_handler_disconnect(d->source, d->handler);
	}
	if (Y_IS_VECTOR(source)) {
		d->source = g_object_ref_sink(source);
	}
	else if (source == NULL) {
		d->source = NULL;
		return;
	}
	d->handler =
		g_signal_connect_after(source, "changed",
                           G_CALLBACK(on_vector_source_changed), d);
}

/**
 * y_ring_matrix_set_rows :
 * @d: #YRingMatrix
 * @r: new number of rows
 *
 * Set the current height of the #YRingMatrix to a new value. If the new
 * height is greater than the previous length, tailing elements are set to
 * zero.
 **/
void y_ring_matrix_set_rows(YRingMatrix * d, unsigned r)
{
	g_assert(Y_IS_RING_MATRIX(d));
	if (r <= d->rmax) {
		d->nr = r;
		y_data_emit_changed(Y_DATA(d));
		if(d->timestamps) {
			y_ring_vector_set_length(d->timestamps,r);
		}
	}
}

/**
 * y_ring_matrix_set_max_rows :
 * @d: #YRingMatrix
 * @rmax: new maximum number of rows
 *
 * Set the maximum height of the #YRingMatrix to a new value.
 **/

void y_ring_matrix_set_max_rows(YRingMatrix *d, unsigned rmax)
{
	g_assert(Y_IS_RING_MATRIX(d));
	if (rmax<d->rmax) { /* don't bother shrinking the array */
		d->rmax = rmax;
		if(d->nr>d->rmax) {
			d->nr=d->rmax;
		}
	}
	else if (rmax>d->rmax) {
		double *a = g_new0(double, rmax*d->nc);
		memcpy(a,d->val,sizeof(double)*d->rmax*d->nc);
		g_free(d->val);
		d->val = a;
		d->rmax = rmax;
	}
	y_data_emit_changed(Y_DATA(d));
}

/**
 * y_ring_matrix_get_timestamps :
 * @d: #YRingMatrix
 *
 * Get timestamps for when each row was added.
 *
 * Returns: (transfer none): The timestamps.
 **/

YRingVector *y_ring_matrix_get_timestamps(YRingMatrix *d)
{
	g_assert(Y_IS_RING_MATRIX(d));
	return d->timestamps;
}
