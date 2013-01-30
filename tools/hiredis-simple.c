#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <hiredis/hiredis.h>

int main(int argc, char* argv[])
{
  FILE *fp= NULL;
  pid_t process_id = 0;
  pid_t sid = 0;
  
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
  
  // Change the current working directory to root.
  chdir("/tmp");
  
  // Close stdin. stdout and stderr
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
  // Open a log file in write mode.
  fp = fopen ("Log.txt", "w+");
  
  //Connect to redis
  redisContext *c = redisConnect("127.0.0.1", 6379);
  if (c->err) {
    fprintf(fp, "Error: %s\n", c->errstr);
  }else{
    fprintf(fp, "Connection Made! \n");
  }
  fflush(fp);
  
  while (1)
  {
    // Dont block context switches, let the process sleep for some time
    sleep(5);
    fprintf(fp, "Logging info...\n");
    fflush(fp);
    // Implement and call some function that does core work for this daemon.
    // Get all keys for testing
    reply = redisCommand(c, "keys %s", "*");
    if ( reply->type == REDIS_REPLY_ERROR ) {
      fprintf(fp, "Error: %s\n", reply->str );
      fflush(fp);
    } else if ( reply->type != REDIS_REPLY_ARRAY ) {
      fprintf(fp, "Unexpected type: %d\n", reply->type );
      fflush(fp);
    } else {
      for ( i=0; i<reply->elements; ++i ) { 
        fprintf(fp, "Result:%lu: %s\n", i, reply->element[i]->str );
        fflush(fp);
      }
    }
    fprintf(fp, "Total Number of Results: %lu\n", i );
    fflush(fp);
    freeReplyObject(reply);
  }
  fclose(fp);
  return (0);
}
