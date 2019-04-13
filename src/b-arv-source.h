#pragma once

#include <b-data.h>
#include <arv.h>
#include "b-image.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BArvSource,b_arv_source,B,ARV_SOURCE,BData)

#define B_TYPE_ARV_SOURCE (b_arv_source_get_type())

BMatrixSize b_arv_source_get_size (BArvSource *mat);
BImage *b_arv_source_get_frame(BArvSource *mat);
BImage *b_arv_source_copy_frame(BArvSource *mat);
guint16 *b_arv_source_get_values (BArvSource *mat);
guint16 b_arv_source_get_value  (BArvSource *mat, unsigned int i, unsigned int j);
void b_arv_source_get_minmax (BArvSource *mat, guint16 *min, guint16 *max);

G_END_DECLS
