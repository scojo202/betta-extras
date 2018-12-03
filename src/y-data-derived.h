/*
 * y-data-derived.h :
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

#ifndef DATA_DERIVED_H
#define DATA_DERIVED_H

#include <y-data-class.h>
#include <y-operation.h>

G_BEGIN_DECLS

#define Y_TYPE_DERIVED (y_derived_get_type ())

G_DECLARE_INTERFACE (YDerived, y_derived, Y, DERIVED, YData)

struct _YDerivedInterface
{
	GTypeInterface parent;

	void (*force_recalculate) (YDerived *self);
};

G_DECLARE_FINAL_TYPE(YDerivedScalar,y_derived_scalar,Y,DERIVED_SCALAR,YScalar)

#define Y_TYPE_DERIVED_SCALAR  (y_derived_scalar_get_type ())

YData	*y_derived_scalar_new      (YData *input, YOperation *op);

G_DECLARE_FINAL_TYPE(YDerivedVector,y_derived_vector,Y,DERIVED_VECTOR,YVector)

#define Y_TYPE_DERIVED_VECTOR  (y_derived_vector_get_type ())

YData	*y_derived_vector_new      (YData *input, YOperation *op);

G_DECLARE_FINAL_TYPE(YDerivedMatrix,y_derived_matrix,Y,DERIVED_MATRIX,YMatrix)

#define Y_TYPE_DERIVED_MATRIX  (y_derived_matrix_get_type ())

YData	*y_derived_matrix_new      (YData *input, YOperation *op);

G_END_DECLS

#endif
