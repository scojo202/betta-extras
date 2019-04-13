#include "b-image.h"

static
G_GNUC_MALLOC guint16 * short_array_calloc(size_t n)
{
  g_return_val_if_fail(n > 0, NULL);
  g_return_val_if_fail(n < 50000000, NULL);
  guint16 *array = g_new0(guint16, n);
  return array;
}

/* coordinates: r - row and c - column */

BImage *b_image_new(guchar bits, guint32 r, guint32 c)
{
  BImage *f = g_slice_new0(BImage);
  if (r > 0 && c > 0) {
    if(bits==2)
      f->data = short_array_calloc(r * c);
    f->nrow = r;
    f->ncol = c;
  }
  return f;
}

BImage *b_image_copy(const BImage * f)
{
    BImage *nf = b_image_new(f->bits, f->nrow, f->ncol);
    if(f->bits==2)
      memcpy(nf->data, f->data, sizeof(guint16) * (f->nrow) * (f->ncol));
    nf->num = f->num;
    nf->timestamp = f->timestamp;
    return nf;
}

void b_image_free(BImage * f)
{
    g_return_if_fail(f);
    g_clear_pointer(&f->data,g_free);
    g_slice_free(BImage, f);
}
