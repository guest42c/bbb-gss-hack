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
  GstElement *pipeline, *source, *decode, *vrate, *filter, *enc, *mux, *sink;
  GstBus *bus;
  GstCaps *filtercaps;
  GstMessage *msg;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  CustomData data;

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
  //fprintf (fp, "%s\n", result);
  //fflush(fp);
  //printf ("%s\n", result);
  //"rtmp://150.164.192.113/video/0009666694da07ee6363e22df5cdac8e079642eb-1359993137281/640x480185-1359999168732 live=1"

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));

  /* Build the pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  source = gst_element_factory_make ("rtmpsrc", "source");
  decode = gst_element_factory_make ("decodebin", "decode");
  vrate = gst_element_factory_make ("videorate", "video-rate");
  filter = gst_element_factory_make ("capsfilter", "filter");
  enc = gst_element_factory_make ("x264enc", "enc");
  mux = gst_element_factory_make ("mpegtsmux", "mux");
  sink = gst_element_factory_make ("souphttpclientsink", "sink");
  if (!pipeline || !source || !decode || !vrate || !filter || !enc || !mux
      || !sink) {
    g_printerr ("One or more elements could not be created. Exiting.\n");
    //fprintf(fp, "One or more elements could not be created.\n");
    return -1;
  }

  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, decode, vrate, filter, enc, mux,
      sink, NULL);

  //fprintf(fp, "Binded elements...\n");
  //fflush(fp);
  filtercaps = gst_caps_new_simple ("video/x-raw",
      //"width", G_TYPE_INT, 640,
      //"height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 30000, 1001, NULL);

  if (gst_element_link (source, decode) != TRUE) {
    g_printerr ("Elements source and decode could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  g_signal_connect (decode, "pad-added", G_CALLBACK (on_pad_added), vrate);

  if (gst_element_link (vrate, filter) != TRUE) {
    g_printerr ("Elements vrate and filter could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  if (gst_element_link (filter, enc) != TRUE) {
    g_printerr ("Elements filter and enc could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  if (gst_element_link (enc, mux) != TRUE) {
    g_printerr ("Elements enc and mux could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  if (gst_element_link (mux, sink) != TRUE) {
    g_printerr ("Elements mux and sink could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }
  //fprintf(fp, "All elements linked..\n");
  //fflush(fp);
  /* Modify the source's properties */

  char *server = "http://localhost:8080/";
  char *location = calloc (strlen (server) + strlen (chan) + 1, sizeof (char));
  strcat (location, server);
  strcat (location, chan);

  g_object_set (source, "location", result, NULL);
  ///g_object_set (enc, "tune", "zerolatency", NULL);
  //g_object_set (enc, "profile", "baseline", NULL);
  g_object_set (enc, "sync-lookahead", 0, NULL);
  //g_object_set (enc, "pass", "cbr", NULL);
  g_object_set (enc, "rc-lookahead", 0, NULL);
  g_object_set (enc, "bitrate", 600, NULL);
  g_object_set (enc, "key-int-max", 4000, NULL);
  g_object_set (sink, "location", location, NULL);

  g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);
  gst_caps_unref (filtercaps);

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

  g_main_loop_run (main_loop);

  /* Free resources */
  g_main_loop_unref (main_loop);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;

}
