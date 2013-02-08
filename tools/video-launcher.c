#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <stdbool.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

#include <time.h>

int main(int argc, char* argv[])
{
  FILE *fp= NULL;
  pid_t process_id = 0;
  pid_t sid = 0;

  redisContext *c; 
  redisReply *reply;

  GError *error = NULL;

  //long int i;

  // Create child process
  process_id = fork();
  
  // Indication of fork() failure
  if (process_id < 0)
  {
    printf("fork failed!\n");
    // Return failure in exit status
    exit(EXIT_FAILURE);
  }
  
  // PARENT PROCESS. Need to kill it.
  if (process_id > 0)
  {
    printf("process_id of child process %d \n", process_id);
    // return success in exit status
    exit(EXIT_SUCCESS);
  }

  //unmask the file mode
  umask(0);
  
  //set new session
  sid = setsid();
  if(sid < 0)
  {
    // Return failure
    exit(EXIT_FAILURE);
  }
  
  // Change the current working directory to /tmp.
  if (chdir("/tmp") < 0) {
    exit(EXIT_FAILURE);
  }
  
  // Close stdin. stdout and stderr
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
  // Open a log file in write mode.
  fp = fopen ("Log.txt", "a+");
  
  //Connect to redis
  c = redisConnect("143.54.10.41", 6379); //143.54.10.41
  if (c->err) {
    fprintf(fp, "Error: %s\n", c->errstr);
  }else{
    fprintf(fp, "Connection Made! \n");
  }
  fflush(fp);
  
  reply = redisCommand(c,"PSUBSCRIBE bigbluebutton:meeting:participants");
  freeReplyObject(reply);
  
  while (1)
  {
    // Dont block context switches, let the process sleep for some time
    sleep(1);
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    fprintf (fp, "Current local time and date: %s", asctime (timeinfo) );

    fflush(fp);
    redisGetReply(c,(void**)&reply);
    //fprintf(fp, "%s: %s\n", reply->element[2]->str, reply->element[3]->str);
   
    const gchar *message_json = reply->element[3]->str;

    g_type_init ();

    JsonParser *parser;
    parser = json_parser_new ();
    
    if (!json_parser_load_from_data (parser, message_json, -1, &error)) {
      fprintf(fp, "%s\n", "Erro ao fazer parser da mensagem json");
    }

    JsonNode *root, *node_stream, *node_meeting;
    JsonObject *object;
    root = json_parser_get_root (parser);

    object = json_node_get_object (root);
    node_meeting = json_object_get_member (object, "meetingId");
    node_stream  = json_object_get_member (object, "value");
   
    //TODO: retrieve host value from config
    const char *host = "143.54.10.41"; //"webconferencia.hc.ufmg.br"; //"150.164.192.113";
    const char *meetingId = json_node_get_string(node_meeting); //"0009666694da07ee6363e22df5cdac8e079642eb-1359993137281";
    const char *videoId = json_node_get_string(node_stream);//"640x480185-1359999168732";

    //Get the substring (after equal sign)
    //true,stream=1280x720-1360167989810-1360167685014
    //TODO: should be reviewed
    int size_id = strlen(videoId)-12; 
    char *streamId;
    memcpy( streamId, &videoId[12], size_id);
    streamId[size_id] = '\0';

    //printf("%s %s %s %s\n", host, meetingId, videoId,streamId);
    //fprintf(fp,"%s %s %s %s\n", host, meetingId, videoId, streamId);
    
    pid_t childPID;
    childPID = fork();
   
    if (childPID >= 0) // fork was successful
    {
      if(childPID == 0) // child process
      {
        //TODO: create gss push server
        char *chan = "stream0"; 
        //Launch pipeline
        if (execl("webm", "webm", host, meetingId, streamId, chan, NULL) == -1) 
        {
          fprintf(stderr,"execl Error!");
          exit(1);
        } 
      } 
      else //Parent process
      {
        fprintf(fp, "process_id of gstreamer child process %d \n", childPID);
      }
    }
    else //fork failed
    {
      fprintf(fp, "Fork failed, Gstreamer Pipeline not started\n");
      return 1;
    }
    freeReplyObject(reply);
  }
  fclose(fp);
  return (0);
}
