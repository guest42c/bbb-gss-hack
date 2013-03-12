#include <gst/gst.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

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

  char *start = "rtmpsrc location='";
  char *end =
      "' ! decodebin name=demux ! x264enc bitrate=600 key-int-max=90 ! mpegtsmux ! souphttpclientsink location=http://143.54.10.78:8080/stream1";
  char *pipe = calloc (strlen (result) + strlen (start) + strlen (end) + 1,
      sizeof (char));

  strcat (pipe, start);
  strcat (pipe, result);
  strcat (pipe, end);

  g_print ("Pipe:%s\n", pipe);

  /* Build the pipeline */
  pipeline = gst_parse_launch (pipe, NULL);

  /* Start playing */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);


  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
