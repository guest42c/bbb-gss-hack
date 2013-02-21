#include <gst/gst.h>
#include <string.h>

typedef struct _CustomData
{
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
} CustomData;

void
on_pad_added (GstElement * src_element, GstPad * pad,   // dynamic source pad
    gpointer target_element)
{
  GstElement *target = (GstElement *) target_element;
  GstPad *sinkpad = gst_element_get_static_pad (target, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
}

static void
cb_message (GstBus * bus, GstMessage * msg, CustomData * data)
{

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    case GST_MESSAGE_BUFFERING:{
      gint percent = 0;

      /* If the stream is live, we do not care about buffering. */
      if (data->is_live)
        break;

      gst_message_parse_buffering (msg, &percent);
      printf ("Buffering (%3d%%)\r", percent);
      g_print ("Buffering (%3d%%)\r", percent);
      /* Wait until buffering is complete before start/resume playing */
      if (percent < 100)
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      else
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:
      /* Get a new clock */
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    default:
      /* Unhandled message */
      break;
  }
}

int
main (int argc, char *argv[])
{
  GstElement *pipeline, *source, *decode, *enc, *mux, *sink, *shout_sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  CustomData data;

  FILE *fp = NULL;
  fp = fopen ("/tmp/livelog.txt", "a+");

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));

  /* Build the pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  source = gst_element_factory_make ("rtmpsrc", "source");
  decode = gst_element_factory_make ("decodebin", "decode");
  enc = gst_element_factory_make ("vp8enc", "enc");
  mux = gst_element_factory_make ("webmmux", "mux");
  //sink = gst_element_factory_make ("filesink", "sink");
  shout_sink = gst_element_factory_make ("shout2send", "shout_sink");
  if (!pipeline || !source || !decode || !enc || !mux || !shout_sink) {
    //if (!pipeline || !source || !decode || !enc || !mux || !sink) {
    g_printerr ("One or more elements could not be created. Exiting.\n");
    return -1;
  }

  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, decode, enc, mux, shout_sink,
      NULL);
  //gst_bin_add_many (GST_BIN (pipeline), source, decode, enc, mux, sink, NULL);

  if (gst_element_link (source, decode) != TRUE) {
    g_printerr ("Elements source and decode could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  g_signal_connect (decode, "pad-added", G_CALLBACK (on_pad_added), enc);

  if (gst_element_link (enc, mux) != TRUE) {
    g_printerr ("Elements enc and mux could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  if (gst_element_link (mux, shout_sink) != TRUE) {
    //if (gst_element_link (mux, sink) != TRUE ) {
    g_printerr ("Elements mux and sink could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }


  char *host = argv[1];
  char *conferenceId = argv[2];
  char *streamId = argv[3];
  char *chan = argv[4];
  char *protocol = "rtmp://";
  char *video = "/video/";
  char *slash = "/";
  char *live = " live=1";
  char *result = calloc (strlen (protocol) + strlen (video) + strlen (slash)
      + strlen (live) + strlen (conferenceId) + strlen (streamId) +
      strlen (host)
      + 1, sizeof (char));
  strcat (result, protocol);
  strcat (result, host);
  strcat (result, video);
  strcat (result, conferenceId);
  strcat (result, slash);
  strcat (result, streamId);
  strcat (result, live);

  //fprintf(fp, "%s %s %s %s\n",host,conferenceId,streamId,chan);
  fprintf (fp, "%s\n", result);

  printf ("%s\n", result);
  //"rtmp://150.164.192.113/video/0009666694da07ee6363e22df5cdac8e079642eb-1359993137281/640x480185-1359999168732 live=1"
  /* Modify the source's properties */

  g_object_set (source, "location", result, NULL);
  g_object_set (mux, "streamable", TRUE, NULL);
  //g_object_set (sink, "location", "/tmp/live.webm", NULL);
  g_object_set (shout_sink, "ip", "localhost", NULL);
  g_object_set (shout_sink, "port", 8080, NULL);
  g_object_set (shout_sink, "mount", chan, NULL);

  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  main_loop = g_main_loop_new (NULL, FALSE);
  data.loop = main_loop;
  data.pipeline = pipeline;

  bus = gst_element_get_bus (pipeline);

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (cb_message), &data);

  fprintf (fp, "Gstreamer Main Loop Started\n");
  g_main_loop_run (main_loop);

  /* Free resources */
  g_main_loop_unref (main_loop);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;

}
