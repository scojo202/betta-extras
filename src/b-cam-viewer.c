#include <stdlib.h>
#include <memory.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <arv.h>
#include <b-extras.h>
#include <math.h>
#include <glib/gstdio.h>

typedef GtkApplication BCamViewerApp;
typedef GtkApplicationClass BCamViewerAppClass;

BCamViewerApp *app;
GSettings *app_settings;

static ArvCamera *camera = NULL;
BArvSource *image = NULL;

GtkWindow *video_window = NULL;

GType b_cam_viewer_app_get_type (void);

G_DEFINE_TYPE (BCamViewerApp, b_cam_viewer_app, GTK_TYPE_APPLICATION)

static void
control_lost_cb (ArvGvDevice *gv_device)
{
    /* Control of the device is lost. Display a message and force application exit */
    g_info("Control lost");
}

static void
b_cam_viewer_app_finalize (GObject * object)
{
  G_OBJECT_CLASS (b_cam_viewer_app_parent_class)->finalize (object);
}

static void
b_cam_viewer_app_init (BCamViewerApp * app)
{
}

static void
preferences_activated (GSimpleAction * action,
		       GVariant * parameter, gpointer user_data)
{
  //show_preferences_window ();
}

static void
quit_activated (GSimpleAction * action,
		GVariant * parameter, gpointer user_data)
{
  gtk_widget_destroy(GTK_WIDGET(video_window));
  g_object_unref(video_window);

  g_application_quit (G_APPLICATION (app));
}

static
void on_video_window_destroy(GtkWidget *self, gpointer userdata)
{
  self = NULL;
  g_application_quit(G_APPLICATION(app));
}

static GActionEntry app_entries[] = {
  //{ "about", about_activated, NULL, NULL, NULL },
  {"preferences", preferences_activated, NULL, NULL, NULL},
  //{"refresh", refresh_activated, NULL, NULL, NULL},
  {"quit", quit_activated, NULL, NULL, NULL},
};

static void
b_cam_viewer_app_startup (GApplication * application)
{
  G_APPLICATION_CLASS (b_cam_viewer_app_parent_class)->startup
    (application);

  g_action_map_add_action_entries (G_ACTION_MAP (application), app_entries,
				   G_N_ELEMENTS (app_entries), application);

  int i;

  arv_update_device_list();
  unsigned int n_dev = arv_get_n_devices();

  if(n_dev==0) {
    g_message("No cameras found");
    g_application_quit(G_APPLICATION(app));
  }

  GtkListStore *list = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeIter iter;
  for(i=0;i<n_dev;i++) {
    gtk_list_store_append (list, &iter);
    gtk_list_store_set (list, &iter,
                          0, i,
                          1, arv_get_device_id(i),
                          2, "Untitled",
                          -1);
  }

  GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("Camera ID", renderer, "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW(tv), column);

  GtkWidget *d = gtk_dialog_new_with_buttons("Select camera", NULL, GTK_DIALOG_DESTROY_WITH_PARENT, "OK", GTK_RESPONSE_ACCEPT, "Quit", GTK_RESPONSE_REJECT, NULL);

  GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG(d));

  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tv));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_container_add(GTK_CONTAINER(content_area), tv);
  gtk_widget_show_all(content_area);
  gint response = gtk_dialog_run(GTK_DIALOG(d));
  if(response == GTK_RESPONSE_REJECT || response == GTK_RESPONSE_DELETE_EVENT) {
    g_application_quit(G_APPLICATION(app));
    return;
  }

  GtkTreeModel *model = GTK_TREE_MODEL(list);
  gchar *name;

  if(gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 1, &name, -1);
  }
  else {
    g_application_quit(G_APPLICATION(app));
    return;
  }

  gtk_widget_destroy(d);

  app_settings = g_settings_new ("com.github.scojo202.bcamviewer");
  int mtu = g_settings_get_int(app_settings,"packet-size");

  camera = arv_camera_new(name);

  if(camera!=NULL) {
    //fr = arv_camera_get_frame_rate(camera);

    arv_camera_set_pixel_format(camera,ARV_PIXEL_FORMAT_MONO_12);

    gint width, height;
    arv_camera_get_sensor_size(camera, &width, &height);

    //arv_camera_set_region (camera, 0, 0, 400, 300);

    arv_camera_gv_set_packet_size (camera, mtu);
    //guint mps = arv_camera_gv_auto_packet_size(camera);
    //g_message("max packet size %u",mps);
  }
  else{
    g_message("Can't find a real camera. Exiting.");
    exit(1);
  }
}

static void
b_cam_viewer_app_shutdown (GApplication * application)
{
  if(camera)
    arv_camera_stop_acquisition (camera);
  arv_shutdown();

  G_APPLICATION_CLASS (b_cam_viewer_app_parent_class)->shutdown
    (application);
}

static void
b_cam_viewer_app_activate (GApplication * appl)
{
  if(camera==NULL)
    return;
  video_window = g_object_new(B_TYPE_VIDEO_WINDOW,"camera",camera, NULL);

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (video_window));

  gtk_widget_show_all (GTK_WIDGET (video_window));

  g_info ("Starting up camera program.");

  if(camera!=NULL) {
  /* Connect the control-lost signal */
            g_signal_connect (arv_camera_get_device (camera), "control-lost",
                      G_CALLBACK (control_lost_cb), NULL);
  }

  image = g_object_new(B_TYPE_ARV_SOURCE,"camera", camera, NULL);

  arv_camera_set_frame_rate (camera, 10.0);
  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS);

  /* Start the video stream */
  arv_camera_start_acquisition (camera);

  /* should quit if this window is closed */
  g_signal_connect (G_OBJECT (video_window), "destroy", G_CALLBACK(on_video_window_destroy), NULL);

  g_object_set(video_window,"image", image, NULL);
}

static void
b_cam_viewer_app_class_init (BCamViewerAppClass * class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  application_class->startup = b_cam_viewer_app_startup;
  application_class->shutdown = b_cam_viewer_app_shutdown;
  application_class->activate = b_cam_viewer_app_activate;

  object_class->finalize = b_cam_viewer_app_finalize;
}

int
main (int argc, char *argv[])
{

  int status;

  g_set_application_name ("BCamViewerApp");

  app = g_object_new (b_cam_viewer_app_get_type (),
		      "application-id", "com.github.scojo202.bcamviewer",
		      "register-session", TRUE, NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  return status;
}
