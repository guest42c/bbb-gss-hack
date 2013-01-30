#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>


int main(int argc, char* argv[])
{
  FILE *fp= NULL;
  pid_t process_id = 0;
  pid_t sid = 0;

  redisContext *c; 
  redisReply *reply;
  long int i;

  // Create child process
  process_id = fork();
  
  // Indication of fork() failure
  if (process_id < 0)
  {
    printf("fork failed!\n");
    // Return failure in exit status
    exit(1);
  }
  
  // PARENT PROCESS. Need to kill it.
  if (process_id > 0)
  {
    printf("process_id of child process %d \n", process_id);
    // return success in exit status
    exit(0);
  }
  
  //unmask the file mode
  umask(0);
  
  //set new session
  sid = setsid();
  if(sid < 0)
  {
    // Return failure
    exit(1);
  }
  
  // Change the current working directory to /tmp.
  chdir("/tmp");
  
  // Close stdin. stdout and stderr
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
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
    freeReplyObject(reply);
  }
  fclose(fp);
  return (0);
}
