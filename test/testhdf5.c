#include <math.h>
#include <y-data.h>
#include <y-extras.h>

YData *d1, *d2, *d3;

#define DATA_COUNT 20000

#define A 1e-19

static void
build_scalar_val (void)
{
  double x, y, z;

  x = 1.4;
  y = 3.4;
  z = 3.2;

  d1 = y_val_scalar_new (x);
  d2 = y_val_scalar_new (y);
  d3 = y_val_scalar_new(z);
}

static void
build_vector_val (void)
{
  gint i;
  double t;
  double *x, *y, *z;

  x=g_malloc(sizeof(double)*DATA_COUNT);
  y=g_malloc(sizeof(double)*DATA_COUNT);
  z=g_malloc(sizeof(double)*DATA_COUNT);
  for (i=0; i<DATA_COUNT; ++i) {
    t = 2*G_PI*i/(double)DATA_COUNT;
    x[i] = 2*sin (4*t)*A;
    y[i] = cos (3*t);
    z[i] = cos (5*t)*A;
  }
  d1 = y_val_vector_new (x, DATA_COUNT, g_free);
  d2 = y_val_vector_new (y, DATA_COUNT, g_free);
  d3 = y_val_vector_new_copy (z, DATA_COUNT);
  g_free(z);
}

static void
build_matrix_val (void)
{
  gint i,j;
  double t;
  double *x, *y, *z;

  x=g_malloc(sizeof(double)*DATA_COUNT*100);
  y=g_malloc(sizeof(double)*DATA_COUNT*100);
  z=g_malloc(sizeof(double)*DATA_COUNT*100);
  for (i=0; i<DATA_COUNT; ++i) {
    for (j=0; j<100; ++j) {
      t = 2*G_PI*(i+j)/(double)DATA_COUNT;
      x[i+j*DATA_COUNT] = 2*sin (4*t)*A;
      y[i+j*DATA_COUNT] = cos (3*t);
      z[i+j*DATA_COUNT] = cos (5*t)*A;
    }
  }
  d1 = y_val_matrix_new (x, DATA_COUNT,100, g_free);
  d2 = y_val_matrix_new (y, DATA_COUNT,100, g_free);
  d3 = y_val_matrix_new_copy (z, DATA_COUNT,100);
  g_free(z);
}

static
void on_subdata_changed(YStruct *s, YData *d, gpointer user_data)
{
  g_message("x: %p %p %p",s,d,user_data);
}

int
main (int argc, char *argv[])
{
  YStruct *s = g_object_new(Y_TYPE_STRUCT,NULL);

  g_signal_connect(s, "subdata-changed",
       G_CALLBACK(on_subdata_changed), NULL);

  build_scalar_val ();

  y_struct_set_data(s,"scalar_data1",d1);
  y_struct_set_data(s,"scalar_data2",d2);
  g_object_unref(d3);

  build_vector_val ();

  y_struct_set_data(s,"vector_data1",d1);
  y_struct_set_data(s,"vector_data2",d2);
  g_object_unref(d3);

  build_matrix_val ();

  y_struct_set_data(s,"matrix_data1",d1);
  y_struct_set_data(s,"matrix_data2",d2);
  g_object_unref(d3);

  /* test slice */
  YOperation *op1 = y_slice_operation_new(SLICE_ROW,50,10);
  YOperation *op2 = y_slice_operation_new(SLICE_COL,20,10);
  YData *der1 = y_derived_vector_new(d1,op1);
  YData *der2 = y_derived_vector_new(d2,op1);
  YData *der3 = y_derived_vector_new(d2,op2);

  y_struct_set_data(s,"slice1",der1);
  y_struct_set_data(s,"slice2",der2);
  y_struct_set_data(s,"slice3",der3);

  //y_derived_vector_set_autorun(Y_VECTOR_DERIVED(der3),TRUE);
  y_data_emit_changed(d2);

  g_usleep(2000000);

  GError *err = NULL;
  YFile *hfile = y_file_open_for_writing("test.h5", FALSE, &err);
  if(err==NULL) {
    y_data_attach_h5(Y_DATA(s),y_file_get_handle(hfile),NULL);
    g_object_unref(hfile);
  }
  else {
    fprintf (stderr, "Error, %s\n", err->message);
  }
  //g_object_unref(s);

  return 0;
}
