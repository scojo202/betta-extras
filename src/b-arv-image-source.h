#pragma once

#include <b-data.h>
#include <arv.h>
#include "b-frame.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(BArvImageSource,b_arv_image_source,B,ARV_IMAGE_SOURCE,BData)

#define B_TYPE_ARV_IMAGE_SOURCE (b_arv_image_source_get_type())

BMatrixSize b_arv_image_source_get_size (BArvImageSource *mat);
BImage *b_arv_image_source_get_frame(BArvImageSource *mat);
BImage *b_arv_image_source_copy_frame(BArvImageSource *mat);
guint16 *b_arv_image_source_get_values (BArvImageSource *mat);
guint16 b_arv_image_source_get_value  (BArvImageSource *mat, unsigned int i, unsigned int j);
void b_arv_image_source_get_minmax (BArvImageSource *mat, guint16 *min, guint16 *max);

G_END_DECLS

#endif
