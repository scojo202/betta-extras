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

GtkWindow *selection_window;
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

static GActionEntry app_entries[] = {
  //{ "about", about_activated, NULL, NULL, NULL },
  {"preferences", preferences_activated, NULL, NULL, NULL},
  //{"refresh", refresh_activated, NULL, NULL, NULL},
  {"quit", quit_activated, NULL, NULL, NULL},
};

static GtkListStore *cam_list;

static void on_descr_edited (GtkCellRendererText *renderer,
               char                *path,
               char                *new_text,
               gpointer             user_data)
{
  /* retrieve camera ID from list */
  GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
  GtkTreeIter iter;
  gtk_tree_model_get_iter (GTK_TREE_MODEL (cam_list),
                           &iter,
                           treepath);

  gchar *dev_id;
  gtk_tree_model_get (GTK_TREE_MODEL (cam_list),
                      &iter,
                      1,
                      &dev_id,
                      -1);

  const gchar *config_dir = g_get_user_config_dir ();
  gchar *app_path = g_build_path("/",config_dir,"b-cam-viewer",NULL);
  g_mkdir_with_parents(app_path,0755);
  gchar *dev_path = g_build_path("/",app_path,dev_id,NULL);
  FILE *f = fopen(dev_path,"w");
  fwrite(new_text,MIN(64,strlen(new_text)),1,f);
  fclose(f);

  /* set new name in liststore */
  gtk_list_store_set(cam_list,&iter,2,new_text,-1);
}

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

  const gchar *config_dir = g_get_user_config_dir ();
  gchar *app_path = g_build_path("/",config_dir,"b-cam-viewer",NULL);
  g_mkdir_with_parents(app_path,S_IRUSR | S_IWUSR | S_IXUSR);

  cam_list = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeIter iter;
  for(i=0;i<n_dev;i++) {
    gtk_list_store_append (cam_list, &iter);
    const gchar *dev_id = arv_get_device_id(i);

    gchar *dev_path = g_build_path("/",app_path,dev_id,NULL);
    gchar dev_descr[64] = "";
    if(g_file_test(dev_path,G_FILE_TEST_EXISTS)) {
      FILE *f = fopen(dev_path,"r");
      fread(dev_descr,64,1,f);
      fclose(f);
    }

    gtk_list_store_set (cam_list, &iter,
                          0, i,
                          1, arv_get_device_id(i),
                          2, dev_descr,
                          -1);
  }
}

static void
b_cam_viewer_app_shutdown (GApplication * application)
{
  if(camera)
    arv_camera_stop_acquisition (camera,NULL);
  arv_shutdown();

  G_APPLICATION_CLASS (b_cam_viewer_app_parent_class)->shutdown
    (application);
}

static void
on_open_button_clicked(GtkButton *button,
                       gpointer   user_data)
{
  GtkTreeView *tv = (GtkTreeView *) user_data;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tv));

  GtkTreeModel *model = GTK_TREE_MODEL(cam_list);
  GtkTreeIter iter;
  gchar *name;

  if(gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 1, &name, -1);
  }
  else {
    return;
  }

  app_settings = g_settings_new ("com.github.scojo202.bcamviewer");
  int mtu = g_settings_get_int(app_settings,"packet-size");

  camera = arv_camera_new(name,NULL);

  if(camera!=NULL) {
    //fr = arv_camera_get_frame_rate(camera);


    arv_camera_set_pixel_format(camera,ARV_PIXEL_FORMAT_MONO_12, NULL);

    gint width, height;
    arv_camera_get_sensor_size(camera, &width, &height,NULL);

    //arv_camera_set_region (camera, 0, 0, 400, 300);

    arv_camera_gv_set_packet_size (camera, mtu, NULL);
    guint mps = arv_camera_gv_auto_packet_size(camera,NULL);
    g_message("max packet size %u",mps);
  }
  else{
    g_message("Can't find a real camera. Exiting.");
    exit(1);
  }

  video_window = g_object_new(B_TYPE_VIDEO_WINDOW,"camera",camera,
                                                  "application", app, NULL);

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (video_window));

  gtk_window_close(selection_window);
  gtk_widget_destroy(GTK_WIDGET(selection_window));

  gtk_widget_show_all (GTK_WIDGET (video_window));

  g_info ("Starting up camera program.");

  if(camera!=NULL) {
  /* Connect the control-lost signal */
            g_signal_connect (arv_camera_get_device (camera), "control-lost",
                      G_CALLBACK (control_lost_cb), NULL);
  }

  image = g_object_new(B_TYPE_ARV_SOURCE,"camera", camera, NULL);

  arv_camera_set_frame_rate (camera, 10.0, NULL);
  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, NULL);

  /* Start the video stream */
  arv_camera_start_acquisition (camera,NULL);

  g_object_set(video_window,"image", image, NULL);
}

static void
b_cam_viewer_app_activate (GApplication * appl)
{
  GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(cam_list));

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("Camera ID", renderer, "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW(tv), column);

  GtkCellRenderer *name_renderer = gtk_cell_renderer_text_new();
  g_object_set(name_renderer,"editable",TRUE,NULL);
  g_signal_connect(name_renderer,"edited",G_CALLBACK(on_descr_edited),NULL);
  column = gtk_tree_view_column_new_with_attributes("Description", name_renderer, "text", 2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW(tv), column);

  selection_window = GTK_WINDOW(gtk_application_window_new(app));
  gtk_window_set_title(selection_window,"Select camera");

  GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL,0));
  gtk_box_pack_start(box,tv,TRUE,TRUE,0);

  GtkBox *button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0));
  gtk_box_pack_start(box,GTK_WIDGET(button_box),TRUE,TRUE,0);

  GtkWidget *ok_button = gtk_button_new_with_label("Open");
  gtk_box_pack_end(button_box,ok_button,FALSE,FALSE,0);

  g_signal_connect(ok_button,"clicked",G_CALLBACK(on_open_button_clicked),tv);

  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tv));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_container_add(GTK_CONTAINER(selection_window), GTK_WIDGET(box));
  gtk_widget_show_all(GTK_WIDGET(selection_window));
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
