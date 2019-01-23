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

#include <data/b-data-class.h>
#include <b-operation.h>

G_BEGIN_DECLS

#define B_TYPE_DERIVED (b_derived_get_type ())

G_DECLARE_INTERFACE (BDerived, b_derived, B, DERIVED, BData)

struct _BDerivedInterface
{
	GTypeInterface parent;

	void (*force_recalculate) (BDerived *self);
};

G_DECLARE_FINAL_TYPE(BDerivedScalar,b_derived_scalar,B,DERIVED_SCALAR,BScalar)

#define B_TYPE_DERIVED_SCALAR  (b_derived_scalar_get_type ())

BData	*b_derived_scalar_new      (BData *input, BOperation *op);

G_DECLARE_FINAL_TYPE(BDerivedVector,b_derived_vector,B,DERIVED_VECTOR,BVector)

#define B_TYPE_DERIVED_VECTOR  (b_derived_vector_get_type ())

BData	*b_derived_vector_new      (BData *input, BOperation *op);

G_DECLARE_FINAL_TYPE(BDerivedMatrix,b_derived_matrix,B,DERIVED_MATRIX,BMatrix)

#define B_TYPE_DERIVED_MATRIX  (b_derived_matrix_get_type ())

BData	*b_derived_matrix_new      (BData *input, BOperation *op);

G_END_DECLS

#endif
