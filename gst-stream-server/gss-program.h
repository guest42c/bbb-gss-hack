
#ifndef _GSS_PROGRAM_H
#define _GSS_PROGRAM_H

#include <gst/gst.h>
#include <libsoup/soup.h>

typedef struct _EwProgram EwProgram;
typedef struct _EwStream EwStream;

struct _EwProgram {
  const char *location;

  int n_streams;
  EwStreams *streams;

  gboolean restart;
};

struct _EwStream {
  EwProgram *program;
  //EwHLSBundle *bundle;
  GstElement *sink;
  int program_id;
  int bandwidth;
  int width;
  int height;
  const char *codecs;
};


#define EW_TYPE_SERVER \
  (ew_server_get_type())
#define EW_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),EW_TYPE_SERVER,EwStreamServer))
#define EW_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),EW_TYPE_SERVER,EwStreamServerClass))
#define GST_IS_EW_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),EW_TYPE_SERVER))
#define GST_IS_EW_SERVER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),EW_TYPE_SERVER))

struct _EwStreamServer
{
  GObject object;

  int port;

  int n_programs;
  EwProgram *programs;
};

struct _EwStreamServerClass
{
  GObjectClass object_class;

};

GType ew_server_get_type (void);

EwStreamServer * ew_server_new (const char *name);



#endif

