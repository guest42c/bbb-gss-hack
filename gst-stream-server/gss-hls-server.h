
#ifndef _GSS_HLS_SERVER_H
#define _GSS_HLS_SERVER_H

#include <gst/gst.h>
#include <libsoup/soup.h>

#define N_CHUNKS 10

typedef struct _EwHLSStream EwHLSStream;
typedef struct _EwHLSChunk EwHLSChunk;


#if 0
struct _EwHLSStream {
  EwHLSBundle *bundle;
  GstElement *sink;
  int program_id;
  int bandwidth;
  int width;
  int height;
  const char *codecs;
};

struct _EwHLSChunk {
  int duration;
};

#define EW_TYPE_HLS_BUNDLE \
  (ew_hls_bundle_get_type())
#define EW_HLS_BUNDLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),EW_TYPE_HLS_BUNDLE,EwHLSBundle))
#define EW_HLS_BUNDLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),EW_TYPE_HLS_BUNDLE,EwHLSBundleClass))
#define GST_IS_EW_HLS_BUNDLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),EW_TYPE_HLS_BUNDLE))
#define GST_IS_EW_HLS_BUNDLE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),EW_TYPE_HLS_BUNDLE))

struct _EwHLSBundle
{
  GObject object;

  const char *name;

  gboolean need_index_update;
  SoupBuffer *index_buffer; /* contents of current index file */

  int target_duration; /* max length of a chunk (in seconds) */
  int sequence_number; /* first sequence in index */
  int current_sequence_number;
  gboolean is_encrypted;
  const char *key_uri;
  gboolean have_iv;
  guint32 init_vector[4];

  gboolean at_eos; /* true if sliding window is at the end of the stream */
  
  int n_chunks;
  EwHLSChunk *chunks;
  int n_streams;
  EwHLSStream **streams;
};

struct _EwHLSBundleClass
{
  GObjectClass object_class;

};

GType ew_hls_bundle_get_type (void);

EwHLSBundle * ew_hls_bundle_new (const char *name);

EwStream * ew_hls_bundle_add_stream (EwHLSBundle *bundle, GstElement *sink,
    int bandwidth, int width, int height);
#endif



#endif

