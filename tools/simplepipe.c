#include <gst/gst.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  FILE *fp = NULL;
  fp = fopen ("/tmp/livelog.txt", "a+");

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
      "' ! decodebin name=demux ! vp8enc ! webmmux streamable=true name=mux ! shout2send ip=localhost port=8080 mount=stream1";
  char *pipe = calloc (strlen (result) + strlen (start) + strlen (end) + 1,
      sizeof (char));

  strcat (pipe, start);
  strcat (pipe, result);
  strcat (pipe, end);

  fprintf (fp, "Pipeline: %s\n", pipe);
//rtmpsrc location='rtmp://143.54.31.81/video/430cd080c4cef065a1f03d3fb69a8ec23eeac5ce-1356804012139/160x1201329-1356804097953 live=1' ! decodebin name=demux ! vp8enc ! webmmux streamable=true name=mux ! shout2send ip=192.168.1.108 port=8080 mount=stream1


  /* Build the pipeline */
  pipeline = gst_parse_launch (pipe, NULL);

  fprintf (fp, "SUCCESSFULY LAUNCHED\n");
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
