#include <gst/gst.h>
 
void on_pad_added (GstElement *src_element,
           GstPad *pad, // dynamic source pad
                 gpointer target_element)
{
    GstElement *target = (GstElement*) target_element;
    GstPad *sinkpad = gst_element_get_static_pad (target, "sink");
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);
}

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *decode, *enc, *mux, *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);
   
  /* Build the pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  source = gst_element_factory_make ("rtmpsrc", "source");
  decode = gst_element_factory_make ("decodebin", "decode");
  enc = gst_element_factory_make ("vp8enc", "enc");
  mux = gst_element_factory_make ("webmmux", "mux");
  sink = gst_element_factory_make ("filesink", "sink");

  if (!pipeline || !source || !decode || !enc || !mux || !sink) {
    g_printerr ("One or more elements could not be created. Exiting.\n");
    return -1; 
  }

  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, decode, enc, mux, sink, NULL);

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
  if (gst_element_link (mux, sink) != TRUE ) {
    g_printerr ("Elements mux and sink could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }

  /* Modify the source's properties */
  g_object_set (source, "location", "rtmp://150.164.192.113/video/b9f88ecdb171d436578f02890d0f3af563961967-1359930538236/160x120176-1359930548129 live=1", NULL);
  g_object_set (mux, "streamable", TRUE, NULL);
  g_object_set (sink, "location", "live.webm", NULL);
  
  /* Start playing */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /*
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  }*/
   
  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
   
  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
