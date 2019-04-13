#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
  gpointer data;
  guchar bits;
  guint32 ncol;
  guint32 nrow;
  gint32 num;			/* an index, e.g. to denote a buffer */
  guint64 timestamp;		/* time stamp */
} BImage;

BImage *b_image_new(guchar bits, guint32 nrow, guint32 ncol);
BImage *b_image_copy(const BImage * f);
void b_image_free(BImage * f);

G_END_DECLS
