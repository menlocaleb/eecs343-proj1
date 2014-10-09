/***************************************************************************
 *  Title: MySimpleShell 
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: tsh.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/
extern pid_t fgpid;
// extern status_p fgstatus;
extern char * fgcmd;

/************Function Prototypes******************************************/
extern pid_t remove_job(pid_t);
extern bgjobL* get_bgjob_by_pid(pid_t);
extern void add_job(pid_t pid, const char * cmd, status_p st);
extern pid_t waitpid(pid_t pid, int* status, int options);
/* handles SIGINT and SIGSTOP signals */	
static void sig(int);
/* handles SIGCHLD signals */
static void child_handler();
static void stop_handler();
extern bgjobL* get_last_job();

/************External Declaration*****************************************/

/**************Implementation***********************************************/

int main (int argc, char *argv[])
{
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*)*BUFSIZE);

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR) PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR) PrintPError("SIGTSTP");
  if (signal(SIGCHLD, sig) == SIG_ERR) PrintPError("SIGTCHLD");

  
  while (!forceExit) /* repeat forever */
  {
    // char *cur_dir = get_current_dir_name();
    printf("!prompt:%d",fgpid);
    /* read command line */
    // printf("@:%s\n",cmdLine);
    getCommandLine(&cmdLine, BUFSIZE);
    // printf("@@@:%s\n",cmdLine);
    if(strcmp(cmdLine, "exit") == 0)
    {
      forceExit=TRUE;
      continue;
    }
    
    fgpid = -1;
    /* checks the status of background jobs */
    CheckJobs();

    /* interpret command and line
     * includes executing of commands */
    Interpret(cmdLine);

    fgpid = -1;

  }

  /* shell termination */
  free(cmdLine);
  return 0;
} /* end main */

static void sig(int signo)
{
  //child terminating signal caught
  if (signo == SIGCHLD)
  {
    // printf("child terminate!!!!\n");
    child_handler();
  }
  if(signo == SIGTSTP){
    stop_handler();
  }
  // ctrl+c caught
  if (signo == SIGINT){
    if (fgpid != -1)
    {
      kill(-fgpid, SIGINT);
    }
  }
}

static void child_handler(){
  pid_t childpid;
  int status;
  //catching the terminated pid
  childpid = waitpid(-1, &status, WUNTRACED | WNOHANG);

  // if the caught pid is foreground
  if (fgpid == childpid){
    remove_job(fgpid);
    fgpid =-1;
    // fgstatus = DONE;
    return;
  }

  else{
    bgjobL* target = get_bgjob_by_pid(childpid);
    if(!target) {return;}
    else {
      // if()
      if (WIFSTOPPED(status)){
        // printf("SUSPENDED!\n");
        target->bg_status = SUSPENDED;
      }
      else{
        // printf("it's done\n");
        target->bg_status = DONE;
      }
    }
  }

  //if the caught pid is bachground
  return;
}

static void stop_handler(){
  if(fgpid==-1){
    // printf("never enter\n");
    return;
  }
  //something is running on foreground
  else{
    bgjobL * target =  get_bgjob_by_pid(fgpid);
    if(!target){
	bgjobL* lastJob = get_last_job();
  // if(!lastJob) printf("null\n");
      
      char str[200];
      sprintf(str,"[%d]   %s         %s\n", 
        lastJob ? lastJob->id + 1 : 1, "Stopped", fgcmd);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if (none < 0) return;
      add_job(fgpid, fgcmd, SUSPENDED);
    }
    else{
      // printf("got it\n");
      // printf("!!\n");
      target ->bg_status = SUSPENDED;
      char str[200];
      sprintf(str,"[%d]    %s         %s\n", 
        target->id, "Stopped", target->cmd);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if (none<0) return;
	// printf("[%d]    %s         %s\n", target->id, "Stopped", target->cmd);
    }
    kill(-fgpid, SIGTSTP);
    fgpid = -1;
    fflush(stdout);
  }
}
