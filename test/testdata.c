#include <math.h>
#include <fftw3.h>
#include <b-extras.h>

static void
test_range_vectors(void)
{
  BLinearRangeVector *r = B_LINEAR_RANGE_VECTOR(b_linear_range_vector_new(0.0,1.0,10));
  g_assert_cmpuint(10, ==, b_vector_get_len(B_VECTOR(r)));
  g_assert_cmpfloat(0.0, ==, b_linear_range_vector_get_v0(r));
  g_assert_cmpfloat(1.0, ==, b_linear_range_vector_get_dv(r));
  int i;
  for(i=0;i<10;i++) {
    g_assert_cmpfloat(0.0+1.0*i, ==, b_vector_get_value(B_VECTOR(r),i));
  }
  g_autofree gchar *str = b_vector_get_str(B_VECTOR(r),8,"%1.1f");
  g_assert_cmpstr(str,==,"8.0");
  g_assert_true(b_vector_is_varying_uniformly(B_VECTOR(r)));
  double mn, mx;
  b_vector_get_minmax(B_VECTOR(r),&mn,&mx);
  g_assert_cmpfloat(mn, ==, 0.0);
  g_assert_cmpfloat(mx, ==, 9.0);

  BFourierLinearRangeVector *f = B_FOURIER_LINEAR_RANGE_VECTOR(b_fourier_linear_range_vector_new(r));
  g_assert_cmpuint(10/2+1,==,b_vector_get_len(B_VECTOR(f)));
  g_assert_cmpfloat(0.0, ==, b_vector_get_value(B_VECTOR(f),0));
  g_assert_true(b_vector_is_varying_uniformly(B_VECTOR(f)));
  g_object_unref(f);
}

static void
test_ring_vector(void)
{
  BRingVector *r = B_RING_VECTOR(b_ring_vector_new(100, 0, FALSE));
  g_assert_cmpuint(0, ==, b_vector_get_len(B_VECTOR(r)));
  int i;
  for(i=0;i<10;i++) {
    b_ring_vector_append(r,(double)i);
  }
  g_assert_cmpuint(10, ==, b_vector_get_len(B_VECTOR(r)));
  for(i=0;i<10;i++) {
    g_assert_cmpfloat((double)i, ==, b_vector_get_value(B_VECTOR(r),i));
  }
  g_assert_true(b_vector_is_varying_uniformly(B_VECTOR(r)));
  b_ring_vector_set_length(r,5);
  g_assert_cmpuint(5, ==, b_vector_get_len(B_VECTOR(r)));
  g_object_unref(r);
}

static void
test_ring_matrix(void)
{
  BRingMatrix *r = B_RING_MATRIX(b_ring_matrix_new(10,100, 0, FALSE));
  g_assert_cmpuint(0, ==, b_matrix_get_rows(B_MATRIX(r)));
  g_assert_cmpuint(10, ==, b_matrix_get_columns(B_MATRIX(r)));
  int i;
  double vals[10];
  for(i=0;i<10;i++) {
    vals[i]=(double)i;
  }
  for(i=0;i<10;i++) {
    b_ring_matrix_append(r,vals,10);
  }
  g_assert_cmpuint(10, ==, b_matrix_get_rows(B_MATRIX(r)));
  for(i=0;i<10;i++) {
    g_assert_cmpfloat((double)i, ==, b_matrix_get_value(B_MATRIX(r),0,i));
  }
  b_ring_matrix_set_rows(r,5);
  g_assert_cmpuint(5, ==, b_matrix_get_rows(B_MATRIX(r)));
  g_object_unref(r);
}

static void
test_property_scalar(void)
{
  BOperation *op = b_slice_operation_new(SLICE_ROW, 50, 1);
  BPropertyScalar *s = b_property_scalar_new(G_OBJECT(op),"index");
  g_assert_cmpfloat(50.0, ==, b_scalar_get_value(B_SCALAR(s)));
  g_object_set(op,"index",30,NULL);
  g_assert_cmpfloat(30.0, ==, b_scalar_get_value(B_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_scalar_slice(void)
{
  BOperation *op = b_slice_operation_new(SLICE_ROW, 50, 1);
  BData *v = b_val_vector_new_alloc(100);
  double *d = b_val_vector_get_array(B_VAL_VECTOR(v));
  for (int i=0;i<100;i++) {
    d[i]=(double)i;
  }
  BDerivedScalar *s = B_DERIVED_SCALAR(b_derived_scalar_new(B_DATA(v),op));
  g_assert_cmpfloat(50.0, ==, b_scalar_get_value(B_SCALAR(s)));
  d[50]=137.0;
  b_data_emit_changed(v);
  g_assert_cmpfloat(137.0, ==, b_scalar_get_value(B_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_scalar_simple(void)
{
  BOperation *op = b_simple_operation_new(log);
  BData *v = b_val_scalar_new(10.0);
  BDerivedScalar *s = B_DERIVED_SCALAR(b_derived_scalar_new(B_DATA(v),op));
  g_assert_cmpfloat(log(10.0), ==, b_scalar_get_value(B_SCALAR(s)));
  b_val_scalar_set_val(B_VAL_SCALAR(v),137.0);
  g_assert_cmpfloat(log(137.0), ==, b_scalar_get_value(B_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_vector_simple(void)
{
  BOperation *op = b_simple_operation_new(log);
  BData *input = b_val_vector_new_alloc(100);
  double *d = b_val_vector_get_array(B_VAL_VECTOR(input));
  for (int i=1;i<101;i++) {
    d[i-1]=(double)i;
  }
  BDerivedVector *v = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(input),op));
  g_assert_cmpuint(100,==,b_vector_get_len(B_VECTOR(v)));
  g_assert_cmpfloat(log(50), ==, b_vector_get_value(B_VECTOR(v),50-1));
  d[50-1]=137.0;
  b_data_emit_changed(input);
  g_assert_cmpfloat(log(137.0), ==, b_vector_get_value(B_VECTOR(v),50-1));
  g_object_unref(v);
}

static void
test_derived_vector_FFT_mag(void)
{
  BOperation *op = b_fft_operation_new(FFT_MAG);
  BData *input = b_val_vector_new_alloc(100);
  double *d = b_val_vector_get_array(B_VAL_VECTOR(input));
  for (int i=0;i<100;i++) {
    d[i]=1.0;
  }
  BDerivedVector *v = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(input),op));
  g_assert_cmpuint(100/2+1,==,b_vector_get_len(B_VECTOR(v)));
  g_assert_cmpfloat(0.0, ==, b_vector_get_value(B_VECTOR(v),2));
  g_object_unref(v);
}

static void
test_derived_vector_FFT_phase(void)
{
  BOperation *op = b_fft_operation_new(FFT_PHASE);
  BData *input = b_val_vector_new_alloc(100);
  double *d = b_val_vector_get_array(B_VAL_VECTOR(input));
  for (int i=0;i<100;i++) {
    d[i]=1.0;
  }
  BDerivedVector *v = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(input),op));
  g_assert_cmpuint(100/2+1,==,b_vector_get_len(B_VECTOR(v)));
  g_assert_cmpfloat(0.0, ==, b_vector_get_value(B_VECTOR(v),1));
  g_object_unref(v);
}

static void
test_derived_vector_slice(void)
{
  BOperation *op = b_slice_operation_new(SLICE_ROW, 50, 1);
  BData *m = b_val_matrix_new_alloc(100,100);
  double *d = b_val_matrix_get_array(B_VAL_MATRIX(m));
  for (int i=0;i<100*100;i++) {
    d[i]=(double)i;
  }
  BDerivedVector *v = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(m),op));
  g_assert_cmpuint(100,==,b_vector_get_len(B_VECTOR(v)));
  g_assert_cmpfloat(50*100+50, ==, b_vector_get_value(B_VECTOR(v),50));
  d[50*100+50]=137.0;
  b_data_emit_changed(m);
  g_assert_cmpfloat(137.0, ==, b_vector_get_value(B_VECTOR(v),50));
  g_object_unref(v);
}

static void
test_derived_vector_subset(void)
{
  BOperation *op = g_object_new(B_TYPE_SUBSET_OPERATION,"start1",5,"length1",20,NULL);
  BData *m = b_val_vector_new_alloc(100);
  double *d = b_val_vector_get_array(B_VAL_VECTOR(m));
  for (int i=0;i<100;i++) {
    d[i]=(double)i;
  }
  BDerivedVector *v = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(m),op));
  g_assert_cmpuint(20,==,b_vector_get_len(B_VECTOR(v)));
  g_assert_cmpfloat(5.0, ==, b_vector_get_value(B_VECTOR(v),0));
  g_object_unref(v);
}

static void
test_derived_matrix_simple(void)
{
  BOperation *op = b_simple_operation_new(log);
  BData *input = b_val_matrix_new_alloc(100,100);
  double *d = b_val_matrix_get_array(B_VAL_MATRIX(input));
  for (int i=1;i<101;i++) {
    for(int j=1;j<101;j++) {
      d[i-1+100*(j-1)]=(double)(i+j);
    }
  }
  BDerivedMatrix *v = B_DERIVED_MATRIX(b_derived_matrix_new(B_DATA(input),op));
  g_assert_cmpuint(100,==,b_matrix_get_rows(B_MATRIX(v)));
  g_assert_cmpuint(100,==,b_matrix_get_columns(B_MATRIX(v)));
  g_assert_cmpfloat(log(2), ==, b_matrix_get_value(B_MATRIX(v),0,0));
  g_assert_cmpfloat(log(3), ==, b_matrix_get_value(B_MATRIX(v),0,1));
  g_assert_cmpfloat(log(3), ==, b_matrix_get_value(B_MATRIX(v),1,0));
  d[0]=137.0;
  b_data_emit_changed(input);
  g_assert_cmpfloat(log(137.0), ==, b_matrix_get_value(B_MATRIX(v),0,0));
  g_object_unref(v);
}

static void
test_derived_matrix_subset(void)
{
  BOperation *op = g_object_new(B_TYPE_SUBSET_OPERATION,"start1",5,"length1",20,"start2",5,"length2",25,NULL);
  BData *input = b_val_matrix_new_alloc(100,100);
  double *d = b_val_matrix_get_array(B_VAL_MATRIX(input));
  for (int i=0;i<100;i++) {
    for(int j=0;j<100;j++) {
      d[i+100*j]=(double)(i+j);
    }
  }
  BDerivedMatrix *v = B_DERIVED_MATRIX(b_derived_matrix_new(B_DATA(input),op));
  g_assert_cmpuint(25,==,b_matrix_get_rows(B_MATRIX(v)));
  g_assert_cmpuint(20,==,b_matrix_get_columns(B_MATRIX(v)));
  g_assert_cmpfloat(10.0, ==, b_matrix_get_value(B_MATRIX(v),0,0));
  g_object_unref(v);
}

int
main (int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/BData/range",test_range_vectors);
  g_test_add_func("/BData/ring/vector",test_ring_vector);
  g_test_add_func("/BData/ring/matrix",test_ring_matrix);
  g_test_add_func("/Bdata/property/scalar",test_property_scalar);
  g_test_add_func("/BData/derived/scalar/simple",test_derived_scalar_simple);
  g_test_add_func("/BData/derived/scalar/slice",test_derived_scalar_slice);
  g_test_add_func("/BData/derived/vector/simple",test_derived_vector_simple);
  g_test_add_func("/BData/derived/vector/subset",test_derived_vector_subset);
  g_test_add_func("/BData/derived/vector/FFT/mag",test_derived_vector_FFT_mag);
  g_test_add_func("/BData/derived/vector/FFT/phase",test_derived_vector_FFT_phase);
  g_test_add_func("/BData/derived/vector/slice",test_derived_vector_slice);
  g_test_add_func("/BData/derived/matrix/simple",test_derived_matrix_simple);
  g_test_add_func("/BData/derived/matrix/subset",test_derived_matrix_subset);
  int retval = g_test_run();
  fftw_cleanup();
  return retval;
}
