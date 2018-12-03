#include <math.h>
#include <y-extras.h>

static void
test_range_vectors(void)
{
  YLinearRangeVector *r = Y_LINEAR_RANGE_VECTOR(y_linear_range_vector_new(0.0,1.0,10));
  g_assert_cmpuint(10, ==, y_vector_get_len(Y_VECTOR(r)));
  g_assert_cmpfloat(0.0, ==, y_linear_range_vector_get_v0(r));
  g_assert_cmpfloat(1.0, ==, y_linear_range_vector_get_dv(r));
  int i;
  for(i=0;i<10;i++) {
    g_assert_cmpfloat(0.0+1.0*i, ==, y_vector_get_value(Y_VECTOR(r),i));
  }
  g_assert_cmpstr(y_vector_get_str(Y_VECTOR(r),8,"%1.1f"),==,"8.0");
  g_assert_true(y_vector_is_varying_uniformly(Y_VECTOR(r)));
  double mn, mx;
  y_vector_get_minmax(Y_VECTOR(r),&mn,&mx);
  g_assert_cmpfloat(mn, ==, 0.0);
  g_assert_cmpfloat(mx, ==, 9.0);

  YFourierLinearRangeVector *f = Y_FOURIER_LINEAR_RANGE_VECTOR(y_fourier_linear_range_vector_new(r));
  g_assert_cmpuint(10/2+1,==,y_vector_get_len(Y_VECTOR(f)));
  g_assert_cmpfloat(0.0, ==, y_vector_get_value(Y_VECTOR(f),0));
  g_assert_true(y_vector_is_varying_uniformly(Y_VECTOR(f)));
  g_object_unref(f);
}

static void
test_ring_vector(void)
{
  YRingVector *r = Y_RING_VECTOR(y_ring_vector_new(100, 0, FALSE));
  g_assert_cmpuint(0, ==, y_vector_get_len(Y_VECTOR(r)));
  int i;
  for(i=0;i<10;i++) {
    y_ring_vector_append(r,(double)i);
  }
  g_assert_cmpuint(10, ==, y_vector_get_len(Y_VECTOR(r)));
  for(i=0;i<10;i++) {
    g_assert_cmpfloat((double)i, ==, y_vector_get_value(Y_VECTOR(r),i));
  }
  g_assert_true(y_vector_is_varying_uniformly(Y_VECTOR(r)));
  y_ring_vector_set_length(r,5);
  g_assert_cmpuint(5, ==, y_vector_get_len(Y_VECTOR(r)));
  g_object_unref(r);
}

static void
test_ring_matrix(void)
{
  YRingMatrix *r = Y_RING_MATRIX(y_ring_matrix_new(10,100, 0, FALSE));
  g_assert_cmpuint(0, ==, y_matrix_get_rows(Y_MATRIX(r)));
  g_assert_cmpuint(10, ==, y_matrix_get_columns(Y_MATRIX(r)));
  int i;
  double vals[10];
  for(i=0;i<10;i++) {
    vals[i]=(double)i;
  }
  for(i=0;i<10;i++) {
    y_ring_matrix_append(r,vals,10);
  }
  g_assert_cmpuint(10, ==, y_matrix_get_rows(Y_MATRIX(r)));
  for(i=0;i<10;i++) {
    g_assert_cmpfloat((double)i, ==, y_matrix_get_value(Y_MATRIX(r),0,i));
  }
  y_ring_matrix_set_rows(r,5);
  g_assert_cmpuint(5, ==, y_matrix_get_rows(Y_MATRIX(r)));
  g_object_unref(r);
}

static void
test_property_scalar(void)
{
  YOperation *op = y_slice_operation_new(SLICE_ROW, 50, 1);
  YPropertyScalar *s = y_property_scalar_new(G_OBJECT(op),"index");
  g_assert_cmpfloat(50.0, ==, y_scalar_get_value(Y_SCALAR(s)));
  g_object_set(op,"index",30,NULL);
  g_assert_cmpfloat(30.0, ==, y_scalar_get_value(Y_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_scalar_slice(void)
{
  YOperation *op = y_slice_operation_new(SLICE_ROW, 50, 1);
  YData *v = y_val_vector_new_alloc(100);
  double *d = y_val_vector_get_array(Y_VAL_VECTOR(v));
  for (int i=0;i<100;i++) {
    d[i]=(double)i;
  }
  YDerivedScalar *s = Y_DERIVED_SCALAR(y_derived_scalar_new(Y_DATA(v),op));
  g_assert_cmpfloat(50.0, ==, y_scalar_get_value(Y_SCALAR(s)));
  d[50]=137.0;
  y_data_emit_changed(v);
  g_assert_cmpfloat(137.0, ==, y_scalar_get_value(Y_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_scalar_simple(void)
{
  YOperation *op = y_simple_operation_new(log);
  YData *v = y_val_scalar_new(10.0);
  YDerivedScalar *s = Y_DERIVED_SCALAR(y_derived_scalar_new(Y_DATA(v),op));
  g_assert_cmpfloat(log(10.0), ==, y_scalar_get_value(Y_SCALAR(s)));
  y_val_scalar_set_val(Y_VAL_SCALAR(v),137.0);
  g_assert_cmpfloat(log(137.0), ==, y_scalar_get_value(Y_SCALAR(s)));
  g_object_unref(s);
}

static void
test_derived_vector_simple(void)
{
  YOperation *op = y_simple_operation_new(log);
  YData *input = y_val_vector_new_alloc(100);
  double *d = y_val_vector_get_array(Y_VAL_VECTOR(input));
  for (int i=1;i<101;i++) {
    d[i-1]=(double)i;
  }
  YDerivedVector *v = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(input),op));
  g_assert_cmpuint(100,==,y_vector_get_len(Y_VECTOR(v)));
  g_assert_cmpfloat(log(50), ==, y_vector_get_value(Y_VECTOR(v),50-1));
  d[50-1]=137.0;
  y_data_emit_changed(input);
  g_assert_cmpfloat(log(137.0), ==, y_vector_get_value(Y_VECTOR(v),50-1));
  g_object_unref(v);
}

static void
test_derived_vector_FFT_mag(void)
{
  YOperation *op = y_fft_operation_new(FFT_MAG);
  YData *input = y_val_vector_new_alloc(100);
  double *d = y_val_vector_get_array(Y_VAL_VECTOR(input));
  for (int i=0;i<100;i++) {
    d[i]=1.0;
  }
  YDerivedVector *v = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(input),op));
  g_assert_cmpuint(100/2+1,==,y_vector_get_len(Y_VECTOR(v)));
  g_assert_cmpfloat(0.0, ==, y_vector_get_value(Y_VECTOR(v),2));
  g_object_unref(v);
}

static void
test_derived_vector_FFT_phase(void)
{
  YOperation *op = y_fft_operation_new(FFT_PHASE);
  YData *input = y_val_vector_new_alloc(100);
  double *d = y_val_vector_get_array(Y_VAL_VECTOR(input));
  for (int i=0;i<100;i++) {
    d[i]=1.0;
  }
  YDerivedVector *v = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(input),op));
  g_assert_cmpuint(100/2+1,==,y_vector_get_len(Y_VECTOR(v)));
  g_assert_cmpfloat(0.0, ==, y_vector_get_value(Y_VECTOR(v),1));
  g_object_unref(v);
}

static void
test_derived_vector_slice(void)
{
  YOperation *op = y_slice_operation_new(SLICE_ROW, 50, 1);
  YData *m = y_val_matrix_new_alloc(100,100);
  double *d = y_val_matrix_get_array(Y_VAL_MATRIX(m));
  for (int i=0;i<100*100;i++) {
    d[i]=(double)i;
  }
  YDerivedVector *v = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(m),op));
  g_assert_cmpuint(100,==,y_vector_get_len(Y_VECTOR(v)));
  g_assert_cmpfloat(50*100+50, ==, y_vector_get_value(Y_VECTOR(v),50));
  d[50*100+50]=137.0;
  y_data_emit_changed(m);
  g_assert_cmpfloat(137.0, ==, y_vector_get_value(Y_VECTOR(v),50));
  g_object_unref(v);
}

static void
test_derived_vector_subset(void)
{
  YOperation *op = g_object_new(Y_TYPE_SUBSET_OPERATION,"start1",5,"length1",20,NULL);
  YData *m = y_val_vector_new_alloc(100);
  double *d = y_val_vector_get_array(Y_VAL_VECTOR(m));
  for (int i=0;i<100;i++) {
    d[i]=(double)i;
  }
  YDerivedVector *v = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(m),op));
  g_assert_cmpuint(20,==,y_vector_get_len(Y_VECTOR(v)));
  g_assert_cmpfloat(5.0, ==, y_vector_get_value(Y_VECTOR(v),0));
  g_object_unref(v);
}

static void
test_derived_matrix_simple(void)
{
  YOperation *op = y_simple_operation_new(log);
  YData *input = y_val_matrix_new_alloc(100,100);
  double *d = y_val_matrix_get_array(Y_VAL_MATRIX(input));
  for (int i=1;i<101;i++) {
    for(int j=1;j<101;j++) {
      d[i-1+100*(j-1)]=(double)(i+j);
    }
  }
  YDerivedMatrix *v = Y_DERIVED_MATRIX(y_derived_matrix_new(Y_DATA(input),op));
  g_assert_cmpuint(100,==,y_matrix_get_rows(Y_MATRIX(v)));
  g_assert_cmpuint(100,==,y_matrix_get_columns(Y_MATRIX(v)));
  g_assert_cmpfloat(log(2), ==, y_matrix_get_value(Y_MATRIX(v),0,0));
  g_assert_cmpfloat(log(3), ==, y_matrix_get_value(Y_MATRIX(v),0,1));
  g_assert_cmpfloat(log(3), ==, y_matrix_get_value(Y_MATRIX(v),1,0));
  d[0]=137.0;
  y_data_emit_changed(input);
  g_assert_cmpfloat(log(137.0), ==, y_matrix_get_value(Y_MATRIX(v),0,0));
  g_object_unref(v);
}

static void
test_derived_matrix_subset(void)
{
  YOperation *op = g_object_new(Y_TYPE_SUBSET_OPERATION,"start1",5,"length1",20,"start2",5,"length2",25,NULL);
  YData *input = y_val_matrix_new_alloc(100,100);
  double *d = y_val_matrix_get_array(Y_VAL_MATRIX(input));
  for (int i=0;i<100;i++) {
    for(int j=0;j<100;j++) {
      d[i+100*j]=(double)(i+j);
    }
  }
  YDerivedMatrix *v = Y_DERIVED_MATRIX(y_derived_matrix_new(Y_DATA(input),op));
  g_assert_cmpuint(25,==,y_matrix_get_rows(Y_MATRIX(v)));
  g_assert_cmpuint(20,==,y_matrix_get_columns(Y_MATRIX(v)));
  g_assert_cmpfloat(10.0, ==, y_matrix_get_value(Y_MATRIX(v),0,0));
  g_object_unref(v);
}

int
main (int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/YData/range",test_range_vectors);
  g_test_add_func("/YData/ring/vector",test_ring_vector);
  g_test_add_func("/YData/ring/matrix",test_ring_matrix);
  g_test_add_func("/Ydata/property/scalar",test_property_scalar);
  g_test_add_func("/YData/derived/scalar/simple",test_derived_scalar_simple);
  g_test_add_func("/YData/derived/scalar/slice",test_derived_scalar_slice);
  g_test_add_func("/YData/derived/vector/simple",test_derived_vector_simple);
  g_test_add_func("/YData/derived/vector/subset",test_derived_vector_subset);
  g_test_add_func("/YData/derived/vector/FFT/mag",test_derived_vector_FFT_mag);
  g_test_add_func("/YData/derived/vector/FFT/phase",test_derived_vector_FFT_phase);
  g_test_add_func("/YData/derived/vector/slice",test_derived_vector_slice);
  g_test_add_func("/YData/derived/matrix/simple",test_derived_matrix_simple);
  g_test_add_func("/YData/derived/matrix/subset",test_derived_matrix_subset);
  return g_test_run();
}
