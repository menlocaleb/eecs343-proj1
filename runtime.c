/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

typedef struct bgjob_l {
  pid_t pid;
  char * cmd;
  int id;
  // int status;
  struct bgjob_l* next;
  
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;

/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/************External Declaration*****************************************/

/*bckground job list methods*/
//move job to the back ground
static void insert_job(pid_t,const char *);
//remove a job from list
static pid_t remove_job(pid_t);
//get the next job
static pid_t get_next_job(); 

/*background */
static void jobs_func();

static void cd_func(commandT*);

static void bg_func(commandT*);

static void fg_func(commandT*);

static void alias_func(commandT*);

static void unalias_func(commandT*);

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  // printf("!!!%s\n",cmd[0]->cmdline );
  int i;
  total_task = n;
  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    printf("%s\n","bulitin bitch!" );
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{
  // TODO



}

void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{

  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }


  else {
    printf("%s: command not found\n", cmd->argv[0]);
    // printf("Number of arguments (not counting command):%d\n", cmd->argc-1);
    // int i;
    // for (i=0;i<cmd->argc;i++) {
    //    printf("Arg %d: %s", i, cmd->argv[i]);
    // }
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    // at this point buf holds one path from list of paths in PATH
    strcat(buf, "/"); // appends "/" to buf
    strcat(buf,cmd->argv[0]); // buf is now path/command
    
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static void Exec(commandT* cmd, bool forceFork)
{

  // printf("comand: %s\n", cmd->name);
  if (forceFork)
  {
      pid_t rc = fork();

    if(rc < 0 ) { // fork failed
      fprintf(stderr,"fork failed\n");
      exit(1);
    }
    else if(rc == 0){ //child process

      //if this is to run in background, set a new session
      if(cmd->bg == 1) {
        if(setpgid(0,0)== -1) perror("setsid error");
      }
      execv(cmd->name,cmd->argv);
    }
    else{ //parent will have to wait for a child to terminate
      
      //if it's to run foreground
      if(cmd -> bg == 0){
        waitpid(rc, NULL, 0);
      }

      //if it's to run background
      else{
        insert_job(rc, cmd->argv[0]);
      }      
    }
  }
//no fork
  else{
    execv(cmd->name,cmd->argv);
  }
  
  
}

static bool IsBuiltIn(char* cmd)
{
  if(!strcmp(cmd, "cd")
    ||!strcmp(cmd, "fg")
    ||!strcmp(cmd, "bg")
    ||!strcmp(cmd, "alias")
    ||!strcmp(cmd, "jobs")
    ||!strcmp(cmd, "unalias"))
    return TRUE;
  return FALSE;

}


static void RunBuiltInCmd(commandT* cmd)
{
  // cd cmd
  if(!strcmp(cmd->argv[0],"cd")) {
    cd_func(cmd);
  }

  //jobs cmd
  else if(!strcmp(cmd->argv[0],"jobs")){
    printf("%s\n", cmd->argv[0] );
    jobs_func();
  }

  //fg cmd
  else if(!strcmp(cmd->argv[0],"fg")){
    fg_func(cmd);
  }

  //bg cmd
  else if(!strcmp(cmd->argv[0],"bg")){
    
    bg_func(cmd);
  }

  //alias cmd
  else if(!strcmp(cmd->argv[0],"alias")){
    alias_func(cmd);
  }

  //unalias cmd
  else {
    unalias_func(cmd);
  }

}

void CheckJobs()
{
}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

static void insert_job(pid_t pid, const char * cmd){

  // printf("inserting\n");
  bgjobL* to_insert = malloc(sizeof(bgjobL));

  to_insert->cmd = (char*)malloc(500*sizeof(char));
  strcpy(to_insert->cmd,cmd);

  // printf("!!!inserting %s+%d\n", to_insert->cmd, pid);

  if (!bgjobs)
  {
    // printf("emptylist !\n");
    to_insert->id = 1;
    bgjobs = to_insert;

  }
  //job -> NULL
  else if(!bgjobs->next) {
    // printf("one in list\n");
    to_insert->id = 2;
    bgjobs->next = to_insert;
  }

  else{
    // printf("many in list\n");
    int id = 2;
    bgjobL* iteration = bgjobs;
    while(iteration->next){
      iteration = iteration->next;
      id++;
      // printf("%d\n^^s^", id);
    }
    to_insert->id = id;
    iteration->next = to_insert;
  }
}

static pid_t remove_job(pid_t pid){

  if (!bgjobs) return -1;

  //JOB->NULL
  else if(!bgjobs->next) {
    if(bgjobs->pid == pid){
      bgjobs = NULL;
      return pid;
    }
    else return -1;
  }

  else{
    /*     bgjobs
             |
    //head->job->job->...->NULL
    */
    bgjobL* iteration = bgjobs;
    bgjobL* head = malloc(sizeof(bgjobL));
    head->pid = -1;
    head->next = bgjobs;
    bgjobL* previous = head;

    /*     bgjobs
             |
    //head->job->job->...->NULL
        |    |
       pre  iter

    */
    while(iteration){

      if(iteration->pid == pid){
        previous->next = iteration->next;
        bgjobs = head->next;
        free(iteration);
        return pid;
      }

      previous = iteration;
      iteration = iteration->next;

    }
    bgjobs = head->next;
    
    return -1;
  }
}
static void jobs_func(){
  //NULL
  if(!bgjobs) {
    printf("No jobs to show\n");
    return;
  }

  bgjobL* head = malloc(sizeof(bgjobL));
  head->next = bgjobs;

  bgjobL * iteration = bgjobs;
  // if(iteration == NULL) printf("####");

  // printf("[%d]   %s+\n", iteration->id, iteration->cmd);
  while(iteration){
    printf("[%d]   %s+\n", iteration->id, iteration->cmd);
    iteration = iteration->next;
  }

}


static void cd_func(commandT* cmd){
  printf("%s\n not implemented", cmd->argv[0]);
}

static void bg_func(commandT* cmd){
  printf("%s\n not implemented", cmd->argv[0]);
}

static void fg_func(commandT* cmd){
  printf("%s\n not implemented", cmd->argv[0]);
}

static void alias_func(commandT* cmd){
  printf("%s\n not implemented", cmd->argv[0]);
}

static void unalias_func(commandT* cmd){
  printf("%s\n not implemented", cmd->argv[0]);
}