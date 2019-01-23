#include <math.h>
#include <b-data.h>
#include <b-extras.h>

BData *d1, *d2, *d3;

#define DATA_COUNT 20000

#define A 1e-19

static void
build_scalar_val (void)
{
  double x, y, z;

  x = 1.4;
  y = 3.4;
  z = 3.2;

  d1 = b_val_scalar_new (x);
  d2 = b_val_scalar_new (y);
  d3 = b_val_scalar_new(z);
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
  d1 = b_val_vector_new (x, DATA_COUNT, g_free);
  d2 = b_val_vector_new (y, DATA_COUNT, g_free);
  d3 = b_val_vector_new_copy (z, DATA_COUNT);
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
  d1 = b_val_matrix_new (x, DATA_COUNT,100, g_free);
  d2 = b_val_matrix_new (y, DATA_COUNT,100, g_free);
  d3 = b_val_matrix_new_copy (z, DATA_COUNT,100);
  g_free(z);
}

static
void on_subdata_changed(BStruct *s, BData *d, gpointer user_data)
{
  g_message("x: %p %p %p",s,d,user_data);
}

int
main (int argc, char *argv[])
{
  BStruct *s = g_object_new(B_TYPE_STRUCT,NULL);

  g_signal_connect(s, "subdata-changed",
       G_CALLBACK(on_subdata_changed), NULL);

  build_scalar_val ();

  b_struct_set_data(s,"scalar_data1",d1);
  b_struct_set_data(s,"scalar_data2",d2);
  g_object_unref(d3);

  build_vector_val ();

  b_struct_set_data(s,"vector_data1",d1);
  b_struct_set_data(s,"vector_data2",d2);
  g_object_unref(d3);

  build_matrix_val ();

  b_struct_set_data(s,"matrix_data1",d1);
  b_struct_set_data(s,"matrix_data2",d2);
  g_object_unref(d3);

  /* test slice */
  BOperation *op1 = b_slice_operation_new(SLICE_ROW,50,10);
  BOperation *op2 = b_slice_operation_new(SLICE_COL,20,10);
  BData *der1 = b_derived_vector_new(d1,op1);
  BData *der2 = b_derived_vector_new(d2,op1);
  BData *der3 = b_derived_vector_new(d2,op2);

  b_struct_set_data(s,"slice1",der1);
  b_struct_set_data(s,"slice2",der2);
  b_struct_set_data(s,"slice3",der3);

  //b_derived_vector_set_autorun(Y_VECTOR_DERIVED(der3),TRUE);
  b_data_emit_changed(d2);

  g_usleep(2000000);

  GError *err = NULL;
  BFile *hfile = b_file_open_for_writing("test.h5", FALSE, &err);
  if(err==NULL) {
    b_data_attach_h5(B_DATA(s),b_file_get_handle(hfile),NULL);
    g_object_unref(hfile);
  }
  else {
    fprintf (stderr, "Error, %s\n", err->message);
  }
  //g_object_unref(s);

  return 0;
}
