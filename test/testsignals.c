#include <math.h>
#include <b-extras.h>

int i = 0;
BMatrix *a;
BDerivedVector *b;
BDerivedVector *c;
BOperation *s;
BOperation *f;
GMutex mutex;

#define THREAD 1

static void
on_changed(BData *data, gpointer user_data)
{

  //double q = y_vector_get_value(Y_VECTOR(data),0);
  const double *p = b_vector_get_values(B_VECTOR(data));
  //g_message("data: %f",q);
  g_message("data: %f",p[i]);

}

static gboolean
emit(gpointer user_data)
{
  BData *data = (BData *) user_data;
  g_message("emit");
  g_mutex_lock(&mutex);
  b_data_emit_changed(data);
  g_mutex_unlock(&mutex);

  return FALSE;
}

static gpointer
feeder_thread(gpointer data)
{
  int j;
  while(i<10000) {
    g_mutex_lock(&mutex);
    double *data = b_val_matrix_get_array(B_VAL_MATRIX(a));
    for(j=0;j<b_matrix_get_rows(a)*b_matrix_get_columns(a);j++)
      {
        data[j]=g_random_double();
      }
    g_message("filled");
    g_mutex_unlock(&mutex);
    g_idle_add(emit,a);
    g_usleep(10000);
    i++;
  }
  return NULL;
}

#if THREAD
#else
static gboolean
timeout(gpointer user_data)
{
  int j;
  g_mutex_lock(&mutex);
  double *data = y_val_matrix_get_array(a);
  for(j=0;j<b_matrix_get_rows(a)*b_matrix_get_columns(a);j++)
      {
        data[j]=g_random_double();
      }
  g_message("hello?");
  g_mutex_unlock(&mutex);
  y_data_emit_changed(a);
  return TRUE;
}
#endif

static gboolean
start (gpointer user_data)
{
  g_signal_connect_after(c,"changed",G_CALLBACK(on_changed),NULL);

#if THREAD
  g_thread_new("feeder",feeder_thread, NULL);
  //g_thread_join(thread);
#else
  g_timeout_add(100,timeout,NULL);
#endif
  return FALSE;
}

int
main (int argc, char *argv[])
{
  g_mutex_init(&mutex);
  /* make a chain of derived data */
  a = B_MATRIX(b_val_matrix_new_alloc(700,500));
  s = b_slice_operation_new(SLICE_ROW, 100, 1);
  f = b_fft_operation_new (FFT_MAG);
  b = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(a),s));
  c = B_DERIVED_VECTOR(b_derived_vector_new(B_DATA(b),f));

  g_idle_add(start,NULL);

  GMainLoop *loop = g_main_loop_new(NULL,FALSE);
  g_main_loop_run(loop);
  return 0;
}
