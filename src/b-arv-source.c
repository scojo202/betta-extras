/*
 * b-arv-source.c :
 *
 * Copyright (C) 2019 Scott O. Johnson (scojo202@gmail.com)
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

#include <b-data.h>
#include <string.h>
#include "b-arv-source.h"

struct _BArvSource {
  BData parent;
  ArvCamera *camera;
  ArvStream *stream;
  BImage *frame;
  guint16 *data;
  guint32 ncol;
  guint32 nrow;
  guint64 timestamp;
  GMutex dmut;
  gulong handler;
};

enum
{
  PROP_0,
  PROP_CAMERA,
  PROP_STREAM,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BArvSource, b_arv_source, B_TYPE_DATA)

static void
arv_source_emit_changed (BData *data)
{
  BArvSource *image = (BArvSource *) data;
  if(image->frame == NULL) {
    image->frame = b_image_new(2, image->nrow, image->ncol);
  }
  g_assert(image->frame->nrow == image->nrow);
  g_assert(image->frame->ncol == image->ncol);
  g_mutex_lock(&image->dmut);
  memcpy(image->frame->data,image->data,sizeof(guint16)*image->nrow*image->ncol);
  image->frame->timestamp = image->timestamp;
  g_mutex_unlock(&image->dmut);
}

static gboolean
emit_changed(gpointer data)
{
  BData *image = B_DATA(data);
  b_data_emit_changed(B_DATA(image));
  return FALSE;
}

/* following called from a different thread! */
static
void frame_ready(ArvStream *stream, gpointer user_data) {
  g_return_if_fail(user_data !=NULL);
  BArvSource *image = B_ARV_SOURCE(user_data);
  ArvBuffer *buffer = arv_stream_try_pop_buffer (stream);
  if (buffer != NULL) {
    /* Image processing here */
    if(arv_buffer_get_status(buffer)!=ARV_BUFFER_STATUS_SUCCESS) {
      //g_message("got status %d",arv_buffer_get_status(buffer));
      arv_stream_push_buffer (stream, buffer);
      return;
    }
    if(arv_buffer_get_payload_type(buffer)==ARV_BUFFER_PAYLOAD_TYPE_IMAGE) {
      //ArvPixelFormat format = arv_buffer_get_image_pixel_format(buffer);
      size_t size;
      const guint16 *b = arv_buffer_get_data(buffer, &size);
      int i;
      g_mutex_lock(&image->dmut);
      if((arv_buffer_get_image_width(buffer)!=image->ncol) || (arv_buffer_get_image_height(buffer)!=image->nrow))
        {
          image->nrow = arv_buffer_get_image_height(buffer);
          image->ncol = arv_buffer_get_image_width(buffer);
          if(image->data!=NULL)
            g_free(image->data);
          image->data = g_malloc0(sizeof(guint16)*image->nrow*image->ncol);
          image->frame = b_image_new(2,image->nrow, image->ncol);
        }
      for(i=0;i<image->nrow*image->ncol;i++) {
        image->data[i]=(guint16) b[i];
      }
      image->frame->num = arv_buffer_get_frame_id(buffer);
      image->timestamp = arv_buffer_get_timestamp(buffer);
      g_mutex_unlock(&image->dmut);
      g_idle_add(emit_changed,image);
    }
  }
  arv_stream_push_buffer (stream, buffer);
}

static void
arv_source_finalize(GObject *object)
{
  BArvSource *im = B_ARV_SOURCE (object);

  arv_stream_set_emit_signals(im->stream, FALSE);

  g_signal_handler_disconnect(im->stream,im->handler);
  g_object_unref(im->stream);
}

static void
arv_source_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BArvSource *im = B_ARV_SOURCE (object);

  switch (property_id)
    {
    case PROP_CAMERA:
      im->camera = g_value_dup_object (value);
      b_arv_source_create_stream (im);
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
arv_source_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BArvSource *im= B_ARV_SOURCE (object);

  switch (property_id)
    {
    case PROP_CAMERA:
      g_value_set_object (value, im->camera);
      break;
    case PROP_STREAM:
      g_value_set_object (value, im->stream);
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void b_arv_source_class_init(BArvSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  BDataClass *data_class = (BDataClass *) klass;

  gobject_class->set_property = arv_source_set_property;
  gobject_class->get_property = arv_source_get_property;
  gobject_class->finalize = arv_source_finalize;

  data_class->emit_changed = arv_source_emit_changed;

  obj_properties[PROP_CAMERA] =
    g_param_spec_object ("camera",
                         "Camera",
                         "Camera object",
                         ARV_TYPE_CAMERA,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  obj_properties[PROP_STREAM] =
    g_param_spec_object ("stream",
                         "Aravis Stream",
                         "Aravis Stream to display",
                         ARV_TYPE_STREAM,
                         G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void b_arv_source_init(BArvSource * self)
{
  g_mutex_init(&self->dmut);
}

BImage *b_arv_source_get_frame(BArvSource *mat)
{
  g_return_val_if_fail(B_IS_ARV_SOURCE(mat),NULL);
  return mat->frame;
}

BImage *b_arv_source_copy_frame(BArvSource *image)
{
  g_return_val_if_fail(B_IS_ARV_SOURCE(image),NULL);
  g_mutex_lock(&image->dmut);
  BImage *f2 = b_image_copy(image->frame);
  g_mutex_unlock(&image->dmut);
  return f2;
}

BMatrixSize b_arv_source_get_size    (BArvSource *mat)
{
  BMatrixSize s;

  g_assert(B_IS_ARV_SOURCE(mat));

  s.rows = mat->nrow;
  s.columns = mat->ncol;

  return s;
}

guint16	*b_arv_source_get_values (BArvSource *mat)
{
  g_return_val_if_fail(B_IS_ARV_SOURCE(mat),NULL);
  g_mutex_lock(&mat->dmut);
  guint16 *n = g_memdup(mat->data,sizeof(guint16)*mat->nrow*mat->ncol);
  g_mutex_unlock(&mat->dmut);
  return n;
}

guint16 b_arv_source_get_value  (BArvSource *mat, unsigned i, unsigned j)
{
  g_return_val_if_fail(B_IS_ARV_SOURCE(mat),0);
  return mat->data[mat->ncol*i+j];
}

void b_arv_source_get_minmax (BArvSource *s, guint16 *min, guint16 *max)
{
  g_return_if_fail(B_IS_ARV_SOURCE(s));
  int i;
  guint16 mx=0;
  guint16 mn=65535;
  g_mutex_lock(&s->dmut);
  for(i=0;i<s->nrow*s->ncol;i++) {
    if(s->data[i]>mx) mx = s->data[i];
    if(s->data[i]<mn) mn = s->data[i];
  }
  g_mutex_unlock(&s->dmut);
  if(min) *min = mn;
  if(max) *max = mx;
}

void b_arv_source_create_stream (BArvSource *s)
{
  g_return_if_fail(B_IS_ARV_SOURCE(s));
  int i, payload;
  if(s->stream)
    arv_stream_set_emit_signals(s->stream,FALSE);
  g_clear_object(&s->stream);
  /* Create a new stream object */
  s->stream = arv_camera_create_stream (s->camera, NULL, NULL,NULL);
  if (s->stream != NULL) {
    g_object_set (s->stream,
                  "packet-timeout", 20 * 1000,
                  "frame-retention", 100 * 1000,
                  NULL);
    /* Push 10 buffers in the stream input buffer queue */
    payload = arv_camera_get_payload (s->camera,NULL);
    for (i = 0; i < 10; i++)
      arv_stream_push_buffer (s->stream, arv_buffer_new (payload, NULL));

    /* set up signals */
    s->handler = g_signal_connect (s->stream, "new-buffer", G_CALLBACK (frame_ready), s);
    arv_stream_set_emit_signals (s->stream, TRUE);
  }
  else {
    g_warning ("Can't create stream thread (check if the device is not already used)\n");
  }
}
