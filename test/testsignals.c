#include <math.h>
#include <y-extras.h>

int i = 0;
YMatrix *a;
YDerivedVector *b;
YDerivedVector *c;
YOperation *s;
YOperation *f;
GMutex mutex;

#define THREAD 1

static void
on_changed(YData *data, gpointer user_data)
{

  //double q = y_vector_get_value(Y_VECTOR(data),0);
  const double *p = y_vector_get_values(Y_VECTOR(data));
  //g_message("data: %f",q);
  g_message("data: %f",p[i]);

}

static gboolean
emit(gpointer user_data)
{
  YData *data = (YData *) user_data;
  g_message("emit");
  g_mutex_lock(&mutex);
  y_data_emit_changed(data);
  g_mutex_unlock(&mutex);

  return FALSE;
}

static gpointer
feeder_thread(gpointer data)
{
  int j;
  while(i<10000) {
    g_mutex_lock(&mutex);
    double *data = y_val_matrix_get_array(Y_VAL_MATRIX(a));
    for(j=0;j<y_matrix_get_rows(a)*y_matrix_get_columns(a);j++)
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
  for(j=0;j<y_matrix_get_rows(a)*y_matrix_get_columns(a);j++)
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
  a = Y_MATRIX(y_val_matrix_new_alloc(700,500));
  s = y_slice_operation_new(SLICE_ROW, 100, 1);
  f = y_fft_operation_new (FFT_MAG);
  b = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(a),s));
  c = Y_DERIVED_VECTOR(y_derived_vector_new(Y_DATA(b),f));

  g_idle_add(start,NULL);

  GMainLoop *loop = g_main_loop_new(NULL,FALSE);
  g_main_loop_run(loop);
  return 0;
}
