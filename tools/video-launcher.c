#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <gst/gst.h>
#include <stdbool.h>

int main(int argc, char* argv[])
{
  FILE *fp= NULL;
  pid_t process_id = 0;
  pid_t sid = 0;

  redisContext *c; 
  redisReply *reply;
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
  //close(STDIN_FILENO);
  //close(STDOUT_FILENO);
  //close(STDERR_FILENO);
  
  // Open a log file in write mode.
  fp = fopen ("Log.txt", "a+");
  
  //Connect to redis
  c = redisConnect("127.0.0.1", 6379);
  if (c->err) {
    fprintf(fp, "Error: %s\n", c->errstr);
  }else{
    fprintf(fp, "Connection Made! \n");
  }
  fflush(fp);
  
  reply = redisCommand(c,"PSUBSCRIBE bigbluebutton:%s","*");
  freeReplyObject(reply);
  
  while (1)
  {
    // Dont block context switches, let the process sleep for some time
    sleep(1);
    fprintf(fp, "Logging info...\n");
    fflush(fp);
    redisGetReply(c,(void**)&reply);
    fprintf(fp, "%s: %s\n", reply->element[2]->str, reply->element[3]->str);

    //TODO: retrieve values from redis
    char *host = "webconferencia.hc.ufmg.br"; //"150.164.192.113";
    char *streamId = "0009666694da07ee6363e22df5cdac8e079642eb-1359993137281";
    char *videoId = "640x480185-1359999168732";

    printf("%s\n", host);

    pid_t childPID;
    childPID = fork();
   
    if (childPID >= 0) // fork was successful
    {
      if(childPID == 0) // child process
      {
        //TODO: create gss push server
        char *chan = "stream0"; 
        //Launch pipeline
        if (execl("/home/guilherme/workspace/gst-streaming-server/tools/webm", "webm", host, streamId, videoId, chan, NULL) == -1) 
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
