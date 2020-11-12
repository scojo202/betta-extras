/*
 * b-video-window.c :
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

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <b-plot.h>
#include <b-extras.h>
#include "b-video-window.h"

struct _BVideoWindow
{
  GtkApplicationWindow base;
  ArvCamera *camera;
  GtkBox *video_window_box1;
  GtkBox *video_widget_bottom_box;
  unsigned int run : 1;
  unsigned int setting_marker : 1;
  unsigned int show_active : 1;
  unsigned int setting_active : 1;
  unsigned int adjusting_active : 1;
  unsigned int near_left : 1;
  unsigned int near_right : 1;
  unsigned int near_top : 1;
  unsigned int near_bottom : 1;
  unsigned int logscale : 1;
  unsigned int flipud : 1;
  unsigned int region_changed : 1;
  gchar *colormap_name;
  BColorMap *colormap;
  gchar *zscale;
  int marker_i;
  int marker_x[5];
  int marker_y[5];
  guint draw_idle;
  cairo_surface_t *surf;
  BArvSource *image;
  int x, y; /* scaled cursor position */
  double scale;
  int *hist;
  GdkCursor *crosshair_cursor, *topleft_cursor, *topright_cursor, *bottomright_cursor, *bottomleft_cursor, *top_cursor, *bottom_cursor, *left_cursor, *right_cursor;

  GSettings *settings;
  GtkHeaderBar *header;
  GtkBox *video_box;
  GtkScrolledWindow *scrolled_window;
  GtkViewport *viewport;
  GtkDrawingArea *draw_area;
  BRateLabel *frame_rate_label;
  BRateLabel *frame_rate_label_draw;
  GtkLabel *cursor_pos_label;
  GtkLabel *z_label;
  GtkComboBoxText *zoom_style_picker;
  GtkSpinButton *zoom_spinbutton;
  BCameraSettingsGrid *settings_grid;

  GtkRadioToolButton *jet_toolbutton, *gray_toolbutton, *auto_toolbutton, *full_toolbutton;

  gulong x_handler, y_handler, width_handler, height_handler;
};

G_DEFINE_TYPE (BVideoWindow, b_video_window, GTK_TYPE_APPLICATION_WINDOW)

static GObjectClass *parent_class = NULL;

/*

detect saturation

*/

#define PROFILE 0

enum
{
  PROP_0,
  PROP_IMAGE,
  PROP_COLORMAP,
  PROP_COLORMAPNAME,
  PROP_ZSCALE,
  PROP_LOGSCALE,
  PROP_CAMERA,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static
const gchar * action_get_string_state (GActionGroup *g, const gchar *name)
{
  GVariant *gv = g_action_group_get_action_state (g, name);
  const gchar *retval = g_variant_get_string(gv,NULL);
  g_variant_unref(gv);
  return retval;
}

static void
window_close_activate (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  gtk_window_close(user_data);
}

static void
window_change_fullscreen_state (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
  if (g_variant_get_boolean (state))
    gtk_window_fullscreen (user_data);
  else
    gtk_window_unfullscreen (user_data);

  g_simple_action_set_state (action, state);
}

static
void replace_surf(BVideoWindow *widget) {
  BMatrixSize size = b_arv_source_get_size(widget->image);
  int image_width = size.columns;
  int image_height = size.rows;

  guint16 *cam_data = b_arv_source_get_values(widget->image);

  if(cam_data==NULL) return;
  if(image_width==0 || image_height==0) return;

  /* update surf */

  if(G_UNLIKELY(widget->surf == NULL)) {
    widget->surf = cairo_image_surface_create ( CAIRO_FORMAT_RGB24,
                                                         image_width,
                                                         image_height);
  }
  if(G_UNLIKELY(widget->hist == NULL)) {
    widget->hist = malloc(sizeof(int)*65536);
  }
  guint16 dmax = 1;
  memset(widget->hist,0,sizeof(int)*65536);
  int i;
  for(i=0;i<image_height*image_width;i++) {
    widget->hist[cam_data[i]]++;
    if(cam_data[i]>dmax) dmax = cam_data[i];
  }

  int surf_height = cairo_image_surface_get_height(widget->surf);
  int surf_width = cairo_image_surface_get_width(widget->surf);

  if(surf_height != image_height || surf_width != image_width) {
    cairo_surface_destroy(widget->surf);
    g_message("replace surf, %d %d", image_width, image_height);
    widget->surf = cairo_image_surface_create ( CAIRO_FORMAT_RGB24,
                                                         image_width,
                                                         image_height);
    surf_height = image_height;
    surf_width = image_width;
    //frame_resize(widget->frame,image_height, image_width);
  }

  unsigned char *buffer = cairo_image_surface_get_data(widget->surf);

  if(widget->x<image_width && widget->y<image_height && widget->x>0 && widget->y>0) {
    gchar buff[40];
    sprintf(buff,"%d",cam_data[widget->x+image_width*widget->y]);
    gtk_label_set_text(widget->z_label,buff);
  }
  else {
    gtk_label_set_text(widget->z_label,"");
  }

  const gchar *scaling = action_get_string_state (G_ACTION_GROUP(widget),"zscale");

#if PROFILE
  GTimer *t = g_timer_new();
#endif

  if(!strcmp(scaling,"full")) {
    dmax=4095;
  }

  gboolean logscale = widget->logscale;

  const gchar *pal = action_get_string_state (G_ACTION_GROUP(widget),"colormapname");

  float idmax;
  if(dmax>0)
    idmax = logscale ? 1.0/logf((float)dmax) : 1.0/((float) dmax);
  else
    idmax = 0.0;

  if(!strcmp(pal,"gray")) {
      unsigned char lut[dmax];
      lut[0] = 0;
      for(i=1;i<=dmax;i++) {
        float d = logscale ? log((float)i) : ((float) i);
        lut[i] = (unsigned char) (255.0*d*idmax);
      }
      for(i=0;i<surf_height*surf_width;i++) {
        buffer[4*i]=lut[cam_data[i]];
        buffer[4*i+1]=lut[cam_data[i]];
        buffer[4*i+2]=lut[cam_data[i]];
      }
  }
  else if (!strcmp(pal,"jet")) {
      /* create lookup tables here */
      guint32 lut[dmax];
      unsigned char *clut = (unsigned char *) lut;
      clut[0]=0;
      clut[1]=0;
      clut[2]=0;
      for(i=1;i<=dmax;i++) {
        float d = logscale ? logf((float)i) : ((float) i);
        float q = d*idmax;
        guint32 u = b_color_map_get_map(widget->colormap,q);
        UINT_TO_RGB(u,&clut[4*i+2],&clut[4*i+1],&clut[4*i])
      }
      guint32 *ibuffer = (guint32*) buffer;
      for(i=0;i<surf_height*surf_width;i++) {
        int j=cam_data[i];
        ibuffer[i]=lut[j];
      }
  }
#if PROFILE
  double te = g_timer_elapsed(t,NULL);
  g_message("video fill buffer: %f ms",te*1000);
  g_timer_destroy(t);
#endif
  g_free(cam_data);
}

static
gboolean draw_timer (gpointer user_data)
{
  BVideoWindow *widget = B_VIDEO_WINDOW(user_data);
  GtkDrawingArea *draw_area = widget->draw_area;

  replace_surf(widget);

  gtk_widget_queue_draw(GTK_WIDGET(draw_area));

  return FALSE;
}

static gboolean
frame_ready(BData *data, gpointer user_data)
{
  BVideoWindow *widget = B_VIDEO_WINDOW(user_data);
  g_assert(widget!=NULL);
  //g_return_val_if_fail(widget->frame_rate_label!=NULL,FALSE);
  b_rate_label_update(widget->frame_rate_label);
  if(!widget->draw_idle) {
    widget->draw_idle = g_idle_add(draw_timer, widget);
  }
  else {
    //g_message("video-widget: skipped draw");
  }
  return FALSE;
}

gboolean
draw_callback (GtkWidget *draw_area, cairo_t *cr, gpointer user_data)
{
  int width, height;

  BVideoWindow *widget = B_VIDEO_WINDOW(user_data);

  if(widget->surf == NULL) return FALSE;

#if PROFILE
  GTimer *t = g_timer_new();
#endif

  const gchar *zoom_style = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget->zoom_style_picker));
  width = gtk_widget_get_allocated_width (draw_area);
  height = gtk_widget_get_allocated_height (draw_area);

  int surf_height = cairo_image_surface_get_height(widget->surf);
  int surf_width = cairo_image_surface_get_width(widget->surf);

  const int hist_width = 50;

  if(!strcmp(zoom_style,"with-window")) {
    double scalex = ((double)width-hist_width)/(surf_width);
    double scaley = ((double)height)/surf_height;

    widget->scale = MIN(scalex,scaley);

    gtk_spin_button_set_value(widget->zoom_spinbutton, widget->scale*100);
  }
  else if(!strcmp(zoom_style,"manual")) {
    /* handle manual zooming */
    widget->scale = gtk_spin_button_get_value(widget->zoom_spinbutton)/100.0;
    gtk_widget_set_size_request(GTK_WIDGET(draw_area),surf_width*widget->scale+hist_width,surf_height*widget->scale);

  }
  /* draw colorbar/histogram */

  int hist_height=surf_height*widget->scale;
  /* resample histogram */
  const int sat=4095; /* value corresponding to very top of histogram colorbar */
  int hmax=sat; /* this is the actual maximum value in the histogram */

  while(widget->hist[hmax]==0 && hmax>0) {
    hmax--;
  }

  double bin_size = (((double) sat)/hist_height); /* number of bins, each of which corresponds to a pixel */
  double rhist[hist_height];
  memset(rhist,0,sizeof(double)*hist_height);
  int i;
  int j=0;
  double rmax=0;
  for(i=0;i<sat;i++) {
    rhist[j]+=widget->hist[i];
    if(i>bin_size*(j+1)) {
      if(rhist[j]>0) {
        rhist[j]=log(rhist[j]);
        if(rhist[j]>rmax) {
          rmax=rhist[j];
        }
      }
      j++;
    }
  }

  cairo_save(cr);

  cairo_surface_t *hist_surf = cairo_image_surface_create ( CAIRO_FORMAT_RGB24,
                                                         hist_width,
                                                         hist_height);
  unsigned char *hist_buffer = cairo_image_surface_get_data(hist_surf);

  const gchar *scaling = action_get_string_state (G_ACTION_GROUP(widget),"zscale");
  if(!strcmp(scaling,"full")) {
    hmax=sat;
  }

  const gchar *pal = action_get_string_state (G_ACTION_GROUP(widget),"colormapname");

  gboolean grey=FALSE;
  if(!strcmp(pal,"gray")) {
    grey=TRUE;
  }

  hist_buffer[0]=0;
  hist_buffer[1]=0;
  hist_buffer[2]=0;
  for(i=1;i<hist_height;i++) {
    float q=((float)(hist_height-1-i)/hist_height);
    q*=((float)sat)/hmax;
    if(widget->logscale) {
      q=logf(hist_height-i)/logf(hmax);
      q*=((float)sat)/hmax;
    }
    if(q>1.0) q=1.0;
    if(grey) {
      for(j=0;j<hist_width;j++) {
        hist_buffer[4*(hist_width*i+j)]=255.0*q;
        hist_buffer[4*(hist_width*i+j)+1]=255.0*q;
        hist_buffer[4*(hist_width*i+j)+2]=255.0*q;
      }
    }
    else {
      for(j=0;j<hist_width;j++) {
        guint32 u = b_color_map_get_map(widget->colormap,q);
        UINT_TO_RGB(u,&hist_buffer[4*(hist_width*i+j)+2],&hist_buffer[4*(hist_width*i+j)+1],&hist_buffer[4*(hist_width*i+j)])
      }
    }
  }

#if PROFILE
  double te1 = g_timer_elapsed(t,NULL);
  g_message("video prepare: %f ms",te1*1000);
#endif

  cairo_translate(cr,surf_width*widget->scale,0);

  cairo_set_source_surface (cr, hist_surf, 0.0, 0.0);
  cairo_paint(cr);
  cairo_restore(cr);
  cairo_save(cr);

  cairo_set_source_rgb(cr,0.0,0.0,0.0);
  cairo_translate(cr,surf_width*widget->scale,0);

  cairo_scale(cr,hist_width,1.0);
  cairo_line_to(cr, 1, 0);
  cairo_move_to(cr, 1, hist_height);
  for(i=0;i<hist_height;i++) {
    cairo_line_to(cr, rhist[i]/rmax, hist_height-1-i);
  }
  cairo_line_to(cr, 1, 0);
  cairo_close_path(cr);
  cairo_fill(cr);

  cairo_surface_destroy(hist_surf);

  cairo_restore(cr);

  int m = widget->flipud ? -1 : 1;
  cairo_scale(cr,widget->scale,m*widget->scale);
  if(widget->flipud)
    cairo_translate(cr,0,-surf_height);

  cairo_save(cr);

  cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

  cairo_set_source_surface (cr, widget->surf, 0.0, 0.0);

  cairo_paint(cr);

#if PROFILE
  double te2 = g_timer_elapsed(t,NULL);
  g_message("video surf draw: %f ms",te2*1000);
#endif

  cairo_restore(cr);

  double r[5] = {1,1,0,1,0};
  double g[5] = {1,0,1,1,1};
  double b[5] = {1,1,1,0,0};

  for(i=0;i<5;i++) {
    if(widget->marker_x[i] > 0) {
      cairo_arc(cr,widget->marker_x[i],widget->marker_y[i],6,0,2*M_PI);
      cairo_close_path(cr);
      cairo_set_source_rgba(cr,r[i],g[i],b[i],0.5);
      cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr,0,0,0,0.5);
      cairo_stroke(cr);
    }
  }
  /* draw white dashed active area rectangle */
  if(widget->show_active) {
    BArvSource *image = widget->image;
    int active_x0, active_y0, active_width, active_height;
    g_object_get(image, "active-x0", &active_x0, "active-y0", &active_y0, "active_width", &active_width, "active_height", &active_height, NULL);
    cairo_set_source_rgba(cr,1.0,1.0,1.0,0.75);
    const double d[2]={5.0,5.0};
    cairo_set_dash(cr,d,2,0.0);
    cairo_move_to(cr, active_x0,active_y0);
    cairo_line_to(cr,active_x0,active_y0+active_height);
    cairo_line_to(cr,active_x0+active_width,active_y0+active_height);
    cairo_line_to(cr,active_x0+active_width,active_y0);
    cairo_line_to(cr,active_x0,active_y0);
    cairo_stroke(cr);
  }

  /* draw yellow dashed active area rectangle */
  if(b_camera_settings_grid_get_region_modified(widget->settings_grid)) {
    int active_x0, active_y0, active_width, active_height;

    b_camera_settings_grid_get_region(widget->settings_grid, &active_x0, &active_y0, &active_width, &active_height);

    active_width = MIN(active_width,surf_width-active_x0);
    active_height = MIN(active_height,surf_height-active_y0);

    cairo_set_source_rgba(cr,1.0,1.0,0.0,1.0);
    const double d[2]={5.0,5.0};
    cairo_set_dash(cr,d,2,0.0);
    cairo_move_to(cr, active_x0,active_y0);
    cairo_line_to(cr,active_x0,active_y0+active_height);
    cairo_line_to(cr,active_x0+active_width,active_y0+active_height);
    cairo_line_to(cr,active_x0+active_width,active_y0);
    cairo_line_to(cr,active_x0,active_y0);
    cairo_stroke(cr);
  }

  if(widget->frame_rate_label_draw) {
    b_rate_label_update(widget->frame_rate_label_draw);
  }
  widget->draw_idle = 0;

#if PROFILE
  double te = g_timer_elapsed(t,NULL);
  g_message("video draw: %f ms",te*1000);
  g_timer_destroy(t);
#endif

  return FALSE;
}

static
void video_widget_finalize(GObject *obj)
{
  BVideoWindow *widget = B_VIDEO_WINDOW(obj);

  g_message("video widget finalize");

  g_object_unref(widget->scrolled_window);
  g_object_unref(widget->draw_area);
  //frame_free(widget->frame);

  if (parent_class->finalize)
    parent_class->finalize (obj);
}

static gboolean
motion_notify (GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
  GdkEventMotion *ev = (GdkEventMotion *) event;
  BVideoWindow *self = B_VIDEO_WINDOW(user_data);
  if(self->surf==NULL) return FALSE;
  self->x = lrint(ev->x/self->scale);
  self->y = lrint(ev->y/self->scale);
  int surf_height = cairo_image_surface_get_height(self->surf);
  int surf_width = cairo_image_surface_get_width(self->surf);
  gchar buff[48];
  if(self->x<surf_width && self->y<surf_height && self->x>0 && self->y>0) {
    sprintf(buff,"(%d, %d)",self->x, self->y);
    gtk_label_set_text(self->cursor_pos_label,buff);
#if 0
    /* if we are setting the active region, check if cursor is near the edge of active region -- if so, set cursor */
    if(self->setting_active) {
      int active_width,active_height,active_y0, active_x0;
      b_arv_source_get_active_area(self->image,&active_width, &active_height, &active_x0, &active_y0);
      if(!self->adjusting_active) {
        self->near_top = abs(self->y-active_y0)<=2;
        self->near_left = abs(self->x-active_x0)<=2;
        self->near_right = abs(self->x-(active_x0+active_width))<=2;
        self->near_bottom = abs(self->y-(active_y0+active_height))<=2;

        if(self->near_top && self->near_left) {
          gdk_window_set_cursor(ev->window,self->topleft_cursor);
        }
        else if (self->near_top && self->near_right) {
          gdk_window_set_cursor(ev->window,self->topright_cursor);
        }
        else if (self->near_bottom && self->near_right) {
          gdk_window_set_cursor(ev->window,self->bottomright_cursor);
        }
        else if (self->near_bottom && self->near_left) {
          gdk_window_set_cursor(ev->window,self->bottomleft_cursor);
        }
        else if (self->near_top) {
          gdk_window_set_cursor(ev->window,self->top_cursor);
        }
        else if (self->near_left) {
          gdk_window_set_cursor(ev->window,self->left_cursor);
        }
        else if (self->near_right) {
          gdk_window_set_cursor(ev->window,self->right_cursor);
        }
        else if (self->near_bottom) {
          gdk_window_set_cursor(ev->window,self->bottom_cursor);
        }
        else {
          gdk_window_set_cursor(ev->window,NULL);
        }
      }
      if(self->adjusting_active) {
        if(self->near_top) {
          active_height+=(active_y0 - self->y);
          active_y0 = self->y;
          g_object_set(self->image,"active-height",active_height,"active-y0",active_y0,NULL);
        }
        if(self->near_left) {
          active_width+=(active_x0 - self->x);
          active_x0 = self->x;
          g_object_set(self->image,"active-width",active_width,"active-x0",active_x0,NULL);
        }
        if(self->near_bottom) {
          active_height = self->y-active_y0;
          g_object_set(self->image,"active-height",active_height,NULL);
        }
        if(self->near_right) {
          active_width = self->x-active_x0;
          g_object_set(self->image,"active-width",active_width,NULL);
        }

        gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
      }
    }
#endif
  }

  return FALSE;
}

static gboolean
button_press (GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
  GdkEventButton *ev = (GdkEventButton *) event;
  BVideoWindow *self = B_VIDEO_WINDOW(user_data);
  if(self->surf==NULL) return FALSE;
  self->x = lrint(ev->x/self->scale);
  self->y = lrint(ev->y/self->scale);
  int surf_height = cairo_image_surface_get_height(self->surf);
  int surf_width = cairo_image_surface_get_width(self->surf);

  if(self->setting_marker) {
    if(self->x<surf_width && self->y<surf_height && self->x>0 && self->y>0) {
      int j = self->marker_i;
      self->marker_x[j] = self->x;
      self->marker_y[j] = self->y;
      self->marker_i++;
      if(self->marker_i>4) self->marker_i=0;
      self->setting_marker = FALSE;
    }
    gdk_window_set_cursor(ev->window,NULL);
  }
  if(self->setting_active) {
    if(self->near_left || self->near_right || self->near_bottom || self->near_top) {
      self->adjusting_active = TRUE;
    }
  }
  return FALSE;
}

static gboolean
button_release (GtkWidget *widget, GdkEvent  *event, gpointer user_data)
{
  BVideoWindow *self = B_VIDEO_WINDOW(user_data);

  if(self->adjusting_active) {
    self->adjusting_active = FALSE;
  }

  return FALSE;
}

static void
on_zoom_style_changed (GtkComboBox *widget, gpointer user_data)
{
  BVideoWindow *self = B_VIDEO_WINDOW(user_data);
  const gchar *zoom_style = gtk_combo_box_get_active_id(widget);

  if(!strcmp(zoom_style,"with-window")) {
    if(gtk_container_get_children(GTK_CONTAINER(self->viewport))!=NULL)
      gtk_container_remove(GTK_CONTAINER(self->viewport), GTK_WIDGET(self->draw_area));
    if(gtk_container_get_children(GTK_CONTAINER(self->video_box))!=NULL)
      gtk_container_remove(GTK_CONTAINER(self->video_box),GTK_WIDGET(self->scrolled_window));
    gtk_box_pack_start(self->video_box, GTK_WIDGET(self->draw_area), TRUE, TRUE,0);
    gtk_widget_set_size_request(GTK_WIDGET(self->draw_area),-1,-1);
    gtk_widget_show(GTK_WIDGET(self->draw_area));
  }
  else if (!strcmp(zoom_style,"manual")) {
    if(gtk_container_get_children(GTK_CONTAINER(self->video_box))!=NULL)
      gtk_container_remove(GTK_CONTAINER(self->video_box),GTK_WIDGET(self->draw_area));
    gtk_box_pack_start(self->video_box, GTK_WIDGET(self->scrolled_window), TRUE, TRUE,0);
    gtk_container_add(GTK_CONTAINER(self->viewport),GTK_WIDGET(self->draw_area));
    gtk_widget_show_all(GTK_WIDGET(self->video_box));
  }
}

static void
video_widget_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      if(ivw->image!=NULL) {
        g_object_unref(ivw->image);
        cairo_surface_destroy(ivw->surf);
        ivw->surf=NULL;
      }
      ivw->image = g_value_get_object (value);
      b_camera_settings_grid_set_source(ivw->settings_grid,ivw->image);
      if(ivw->image!=NULL) {
        g_object_ref_sink(ivw->image);
        g_signal_connect (ivw->image, "changed", G_CALLBACK (frame_ready), ivw);
      }
      break;
    case PROP_COLORMAP: {
      ivw->colormap = g_value_dup_object (value);

      break;
    }
    case PROP_COLORMAPNAME: {
      if(ivw->colormap_name)
        g_free (ivw->colormap_name);
      ivw->colormap_name = g_value_dup_string (value);
      break;
    }
    case PROP_CAMERA: {
      ivw->camera = g_value_dup_object (value);
      g_assert(ARV_IS_CAMERA(ivw->camera));
      //gchar buff[512];
      //sprintf(buff,"%s (%s)",arv_camera_get_model_name(ivw->camera),arv_camera_get_device_id(ivw->camera));
      //gtk_header_bar_set_subtitle(ivw->header,buff);

      b_camera_settings_grid_set_camera (ivw->settings_grid,ivw->camera);

      break;
    }
    case PROP_ZSCALE: {
      ivw->zscale = g_value_dup_string (value);
      break;
    }
    case PROP_LOGSCALE:
      ivw->logscale = g_value_get_boolean(value);
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
video_widget_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, ivw->image);
      break;
    case PROP_CAMERA:
      g_value_set_object (value, ivw->camera);
      break;
    case PROP_COLORMAP:
      g_value_set_object (value, ivw->colormap);
      break;
    case PROP_COLORMAPNAME:
      g_value_set_string (value, ivw->colormap_name);
      break;
    case PROP_ZSCALE:
      g_value_set_string (value, ivw->zscale);
      break;
    case PROP_LOGSCALE:
      g_value_set_boolean (value, ivw->logscale);
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static
void video_widget_destroy (GtkWidget *object)
{
  BVideoWindow *widget = B_VIDEO_WINDOW (object);
  cairo_surface_destroy(widget->surf);
  if(widget->image) {
    g_signal_handlers_disconnect_by_data(widget->image, widget);
    g_object_unref(widget->image);
  }
  widget->run=FALSE;
  if(widget->draw_idle)
    g_source_remove(widget->draw_idle);
}

static void
video_widget_constructed (GObject *obj)
{
  BVideoWindow *w = B_VIDEO_WINDOW(obj);

  if(w->image != NULL) {
    b_video_window_on(w);
  }
}

static void
b_video_window_class_init (BVideoWindowClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  widget_class->destroy = video_widget_destroy;

  object_class->set_property = video_widget_set_property;
  object_class->get_property = video_widget_get_property;
  object_class->finalize = video_widget_finalize;
  object_class->constructed = video_widget_constructed;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/github/scojo202/betta/b-video-window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, video_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, video_window_box1);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, zoom_style_picker);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, zoom_spinbutton);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, cursor_pos_label);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, z_label);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, jet_toolbutton);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, gray_toolbutton);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, auto_toolbutton);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, full_toolbutton);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), BVideoWindow, video_widget_bottom_box);

  obj_properties[PROP_IMAGE] = g_param_spec_object ("image", "Image", "Image object", B_TYPE_ARV_SOURCE, G_PARAM_READWRITE);
  obj_properties[PROP_CAMERA] = g_param_spec_object ("camera", "Camera", "Camera object", ARV_TYPE_CAMERA, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  obj_properties[PROP_COLORMAP] = g_param_spec_object ("colormap", "Colormap", "Colormap", B_TYPE_COLOR_MAP, G_PARAM_READWRITE);
  obj_properties[PROP_COLORMAPNAME]= g_param_spec_string ("colormapname", "Colormap name", "Colormap name", "jet", G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  obj_properties[PROP_ZSCALE]= g_param_spec_string ("zscale", "Z Scale", "Style for Z scale", "auto", G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  obj_properties[PROP_LOGSCALE]= g_param_spec_boolean ("logscale", "Log scale", "Whether to use a log scale", FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
activate_save (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  BVideoWindow *widget = B_VIDEO_WINDOW(user_data);

  GtkWidget *dialog = gtk_file_chooser_dialog_new("Save PNG",
                                      GTK_WINDOW(widget),
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                      "_Save", GTK_RESPONSE_ACCEPT,
                                      NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "untitled.png");
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    BImage *frame = b_arv_source_get_frame(widget->image);
    b_image_save_to_png_async (frame, filename);
    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

static void
activate_set_markers (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW(user_data);

  ivw->setting_marker = TRUE;
  /* change cursor to a crosshair */
  if(ivw->setting_marker) {
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(ivw)),ivw->crosshair_cursor);
  }
}

static void
activate_clear_markers (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW(user_data);
  int i;
  for(i=0;i<5;i++) {
    ivw->marker_x[i] = -1;
    ivw->marker_y[i] = -1;
  }
  ivw->marker_i = 0;
  /* TODO: if we were setting a marker, undo this */
  ivw->setting_marker = FALSE;
  gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(ivw)),NULL);
}

static void
change_flipud_state (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW(user_data);
  ivw->flipud = g_variant_get_boolean (state);

  g_simple_action_set_state (action, state);
}

static void
change_capture_state (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
  BVideoWindow *ivw = B_VIDEO_WINDOW(user_data);
  gboolean capture = g_variant_get_boolean (state);
  if(capture) {
    arv_camera_start_acquisition(ivw->camera,NULL);
  }
  else {
    arv_camera_stop_acquisition(ivw->camera,NULL);
  }

  g_simple_action_set_state (action, state);
}

static void
action_activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GVariant *state;

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

void simple_action_set_state(GSimpleAction *simple,
                           GVariant *value, gpointer user_data)
{
  g_simple_action_set_state(simple,value);
}

static GActionEntry win_entries[] = {
  { "fullscreen", action_activate_toggle, NULL, "false", window_change_fullscreen_state },
  { "capturing", NULL, NULL, "true", change_capture_state },
  { "save", activate_save, NULL, NULL, NULL},
  { "colormapname", NULL, "s", "'jet'", simple_action_set_state },
  { "zscale", NULL, "s", "'auto'", simple_action_set_state },
  { "flipud", NULL, NULL, "true", change_flipud_state },
  { "set_markers", activate_set_markers, NULL, NULL, NULL},
  { "clear_markers", activate_clear_markers, NULL, NULL, NULL},
  { "close", window_close_activate, NULL, NULL, NULL }
};

static void
b_video_window_init (BVideoWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->colormap = b_color_map_new();
  b_color_map_set_jet(self->colormap);

  self->header = GTK_HEADER_BAR(gtk_header_bar_new());
  gtk_header_bar_set_title(self->header,"Camera viewer");
  gtk_header_bar_set_show_close_button(self->header,TRUE);

  //gtk_window_set_titlebar(GTK_WINDOW(self),GTK_WIDGET(self->header));

  self->settings_grid = g_object_new(B_TYPE_CAMERA_SETTINGS_GRID,NULL);
  gtk_box_pack_end(self->video_window_box1,GTK_WIDGET(self->settings_grid),FALSE,TRUE,0);

  self->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL,NULL));
  //gtk_scrolled_window_set_policy(self->scrolled_window, GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
  g_object_ref(self->scrolled_window); /* because we will sometimes remove it */
  self->draw_area = GTK_DRAWING_AREA(gtk_drawing_area_new());
  g_object_ref(self->draw_area); /* because we will sometimes remove it */

  gtk_widget_add_events(GTK_WIDGET(self->draw_area),GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(self->draw_area, "motion-notify-event", G_CALLBACK(motion_notify), self);
  g_signal_connect(self->draw_area, "button-press-event", G_CALLBACK(button_press), self);
  g_signal_connect(self->draw_area, "button-release-event", G_CALLBACK(button_release), self);

  self->viewport = GTK_VIEWPORT(gtk_viewport_new(NULL,NULL));

  gtk_container_add(GTK_CONTAINER(self->scrolled_window), GTK_WIDGET(self->viewport));

  g_signal_connect(self->zoom_style_picker, "changed", G_CALLBACK(on_zoom_style_changed), self);
  gtk_combo_box_set_active(GTK_COMBO_BOX(self->zoom_style_picker),1);

  self->flipud = FALSE;
  self->surf= NULL;

  self->setting_marker = FALSE;
  int i;
  for(i=0;i<5;i++) {
    self->marker_x[i] = -1;
    self->marker_y[i] = -1;
  }

  gtk_window_set_default_size(GTK_WINDOW(self), 800,600);

  g_action_map_add_action_entries (G_ACTION_MAP (self), win_entries, G_N_ELEMENTS (win_entries), self);
  g_action_map_add_action (G_ACTION_MAP (self), (GAction *) g_property_action_new("logscale", self, "logscale"));
  //g_action_map_add_action (G_ACTION_MAP (self), (GAction *) g_property_action_new("colormapname", self, "colormapname"));

  gtk_actionable_set_detailed_action_name(GTK_ACTIONABLE(self->jet_toolbutton),"win.colormapname::jet");
  gtk_actionable_set_detailed_action_name(GTK_ACTIONABLE(self->gray_toolbutton),"win.colormapname::gray");
  gtk_actionable_set_detailed_action_name(GTK_ACTIONABLE(self->auto_toolbutton),"win.zscale::auto");
  gtk_actionable_set_detailed_action_name(GTK_ACTIONABLE(self->full_toolbutton),"win.zscale::full");

  GdkDisplay *display = gdk_display_get_default();
  self->crosshair_cursor = gdk_cursor_new_for_display(display,GDK_CROSSHAIR);
  self->topleft_cursor = gdk_cursor_new_for_display(display,GDK_TOP_LEFT_CORNER);
  self->topright_cursor = gdk_cursor_new_for_display(display,GDK_TOP_RIGHT_CORNER);
  self->bottomleft_cursor = gdk_cursor_new_for_display(display,GDK_BOTTOM_LEFT_CORNER);
  self->bottomright_cursor = gdk_cursor_new_for_display(display,GDK_BOTTOM_RIGHT_CORNER);
  self->top_cursor = gdk_cursor_new_for_display(display,GDK_TOP_SIDE);
  self->bottom_cursor = gdk_cursor_new_for_display(display,GDK_BOTTOM_SIDE);
  self->left_cursor = gdk_cursor_new_for_display(display,GDK_LEFT_SIDE);
  self->right_cursor = gdk_cursor_new_for_display(display,GDK_RIGHT_SIDE);

  self->frame_rate_label = b_rate_label_new("Capture","fps");
  gtk_box_pack_start(self->video_widget_bottom_box, GTK_WIDGET(self->frame_rate_label), TRUE, TRUE, 0);

  self->frame_rate_label_draw = b_rate_label_new("Draw","fps");
  gtk_box_pack_start(self->video_widget_bottom_box, GTK_WIDGET(self->frame_rate_label_draw), TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (self->draw_area), "draw",
                    G_CALLBACK (draw_callback), self);

  gtk_widget_show_all(GTK_WIDGET(self));
}

void b_video_window_on(BVideoWindow *self)
{
  self->run = TRUE;
}

void b_video_window_attach_settings(BVideoWindow *vw, GSettings *settings)
{
  vw->settings = settings;
  /* load values from settings */

}
