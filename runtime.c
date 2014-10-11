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


/* the pids of the background processes */
bgjobL *bgjobs = NULL;

aliasesT aliases = {0};

//foreground process pid
pid_t fgpid = -1;
// status_p fgstatus;
char * fgcmd;

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

void RunCmdPipe(commandT**, int n);
void RunCmdRedirOut(commandT* cmd, char* file);
void RunCmdRedirIn(commandT* cmd, char* file);
void RunCmdRedir(commandT** cmd);
/************External Declaration*****************************************/

/*bckground job list methods*/

void waitfg();
//move job to the back ground
void add_job(pid_t,const char *,status_p st);
//remove a job from list
pid_t remove_job(pid_t);
//get the next job
pid_t get_next_job(); 

int get_bgjobid_by_pid(pid_t);

bgjobL* get_bgjob_by_pid(pid_t);

bgjobL* get_last_job();

bgjobL* get_bgjob_by_id(int);

// static char * status_to_string(status_p st);

/*background */
static void jobs_func();

static void cd_func(commandT*);

static void bg_func(commandT*);

static void fg_func(commandT*);

static void alias_func(commandT*);

static void unalias_func(commandT*);

static void print_all_aliases();

// -1 means no, >=0 means yes
static int is_alias(char* command);

static char* get_alias(int index);

static void set_alias(commandT* cmd);

static void unset_alias(commandT* cmd);

static char* get_alias_key(commandT* cmd);

static char* get_alias_value(commandT* cmd);

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  // printf("!!!%s\n",cmd[0]->cmdline );
  int i;
  total_task = n;
  // printf("tasks:%d\n", total_task);


  // check if first command is an alias
  int aliasIndex = is_alias(cmd[0]->argv[0]);
  if (aliasIndex != -1) {
    free(cmd[0]->argv[0]);
    char* aliasValue = get_alias(aliasIndex);
    int arg0Len = strlen(aliasValue);
    int arg1Len = 0;

    char* spaceIndex = strchr(aliasValue, ' ');
    if (spaceIndex != NULL) {
       arg0Len = spaceIndex - aliasValue;
       arg1Len = strlen(aliasValue) - arg0Len -1; // -1 for space
    }
    
    cmd[0]->argv[0] = malloc(sizeof(char) + arg0Len + 1);
    strncpy(cmd[0]->argv[0], aliasValue, arg0Len);
    cmd[0]->argv[arg0Len] = '\0';

    // hack to accomodate aliases with 2 arguments 
    if (arg1Len > 0) {
      cmd[0]->argv[1] = malloc(sizeof(char) + arg1Len + 1);
      strcpy(cmd[0]->argv[1], spaceIndex+1);
      cmd[0]->argc = cmd[0]->argc + 1;
    }
  }

  if(n == 1){
    if (cmd[0]->is_redirect_in && cmd[0]->is_redirect_out){
      RunCmdRedir(cmd);
    }
    else if (cmd[0]->is_redirect_in){
      RunCmdRedirIn(cmd[0], cmd[0]->redirect_in);
    }
    else if (cmd[0]->is_redirect_out){
      RunCmdRedirOut(cmd[0], cmd[0]->redirect_out);
    }
    else {
      RunCmdFork(cmd[0], TRUE);
    }
  }

    
  else{
    RunCmdPipe(cmd,n);
    for(i = 0; i < n-1; i++){
      ReleaseCmdT(&cmd[i]);
    }
      
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{

}


void RunCmdPipe(commandT** cmd, int n){
    int pd[2] = {-1,-1};
    int i;
    int out = -1;
    pid_t rc;
    for (i = 0; i < n; i++){
      
      if(pipe(pd)<0){
        perror("Error");
        exit(EXIT_FAILURE);
      };
      rc = fork();
      if (!rc){
        //redirect STDIN
        dup2(out,0);
        //close the other end
        close(pd[0]);
        //redirect STDOUT
        if( i+1!= n) dup2(pd[1], 1);
        
        RunCmdFork(cmd[i], FALSE);
        exit(EXIT_FAILURE);
      }
      else{
        wait(NULL);
        //store the pipe read end for next process
        out = pd[0];
        close(pd[1]);
      }
        
  }   
}

void RunCmdRedirOut(commandT* cmd, char* file_o)
{
  int output = dup(1); //need to save the stdout to restore
  int fd_out = open(file_o, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd_out == -1)
  {
    return;
    perror("File");
  }
  dup2(fd_out,1);
  RunCmdFork(cmd, TRUE);
  //restore
  dup2(output,1);
  close(fd_out);
}

void RunCmdRedirIn(commandT* cmd, char* file_i)
{
  int input = dup(0);
  int fd_in = open(file_i, O_RDONLY);
  if (fd_in == -1)
  {
    return;
  }
  dup2(fd_in, 0);
  RunCmdFork(cmd, TRUE);
  //restore
  dup2(input, 0);
  close(fd_in);
}

void RunCmdRedir(commandT** cmd){
  int input = dup(0);
  int output = dup(1);
  int fileIn = open(cmd[0]->redirect_in, O_RDONLY);
  int fileOut =  open(cmd[0]->redirect_out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  dup2(fileIn, 0);
  dup2(fileOut, 1);
  RunCmdFork(cmd[0], TRUE);
  dup2(input, 0);
  dup2(output, 1);
  close(fileIn);
  close(fileOut);
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
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask,SIGCHLD);

  // printf("comand: %s\n", cmd->name);

  sigprocmask(SIG_BLOCK,&mask,NULL);
  if (forceFork)
  {
      pid_t rc = fork();

    if(rc < 0 ) { // fork failed
      fprintf(stderr,"fork failed\n");
      exit(1);
    }
    else if(rc == 0){ //child process

      if(setpgid(0,0)== -1) perror("setsid error");
      execv(cmd->name,cmd->argv);
      exit(0);
    }
    else{ //parent will have to wait for a child to terminate
      
      //if it's to run foreground
      if(cmd -> bg == 0){
        // fgstatus = RUNNING;
        fgpid = rc;
        fgcmd = cmd->cmdline;
        sigprocmask(SIG_UNBLOCK,&mask,NULL);
        // waitpid(rc, NULL, 0);
        waitfg();
        
      }

      //if it's to run background
      else{
        sigprocmask(SIG_UNBLOCK,&mask,NULL);        
        add_job(rc, cmd->cmdline, RUNNING);
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
    // printf("%s\n", cmd->argv[0] );
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
  if (!bgjobs)
  {
    return;
  }

  bgjobL* iteration = bgjobs;
  while(iteration){
    if(iteration->bg_status == DONE) {
      // pid_t id_removed = iteration->id;
      // status_p status_removed = DONE;
      // char * cmd_removed = iteration->cmd;

      remove_job(iteration->pid);

      char str[200];
      sprintf(str,"[%d]   Done             %s\n", iteration->id, iteration-> cmd);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if(none<0) printf("wrong\n");
      // free(str);
      
    }
    iteration = iteration->next;
  }
  return;
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

void add_job(pid_t pid, const char * cmd, status_p st){

  // printf("inserting\n");
  bgjobL* to_insert = malloc(sizeof(bgjobL));

  to_insert->cmd = (char*)malloc(500*sizeof(char));
  strcpy(to_insert->cmd,cmd);
  to_insert->bg_status = st;
  to_insert->pid = pid;
  bgjobL * last =get_last_job();

  to_insert->id = last ? ((last->id)+1) : 1 ;
  // printf("to to_insert%s %d",to_insert->cmd,to_insert->id);

  // printf("!!!inserting %s+%d\n", to_insert->cmd, pid);

  if (!bgjobs)
  {
    // printf("emptylist !\n");
    bgjobs = to_insert;

  }
  //job -> NULL
  else if(!bgjobs->next) {
    // printf("one in list\n");
    bgjobs->next = to_insert;
  }

  //job->jobg->...->NULL
  else{
    // printf("many in list\n");
    bgjobL* iteration = bgjobs;
    while(iteration->next){
      iteration = iteration->next;
    }
    iteration->next = to_insert;
  }
}

pid_t remove_job(pid_t pid){

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

bgjobL* get_bgjob_by_pid(pid_t pid){

  if (!bgjobs)
  {
    return NULL;
  }


  bgjobL* iteration = bgjobs;
  while(iteration){
    if(iteration->pid == pid) return iteration;
    iteration= iteration->next;
  }

  return NULL;
}

int get_bgjobid_by_pid(pid_t pid){
  bgjobL * result = get_bgjob_by_pid(pid);
  if(!result) return -1;
  else{
    return result->id;
  }
}

bgjobL * get_last_job(){
  if (!bgjobs)
  {
    return NULL;
  }

  bgjobL* iteration = bgjobs;
  while(iteration){
    if(!iteration->next) break;
    iteration= iteration->next;
  }
  return iteration;
}

bgjobL* get_bgjob_by_id(int id){
  if (!bgjobs)
  {
    return NULL;
  }

  bgjobL* iteration = bgjobs;
  while(iteration){
    if(iteration->id == id) break;
    iteration= iteration->next;
  }

  return iteration;
}

/*builtin functions*/
static void jobs_func(){
  //NULL
  if(!bgjobs) {
    // printf("No jobs to show\n");
    return;
  }

  bgjobL* head = malloc(sizeof(bgjobL));
  head->next = bgjobs;

  bgjobL * iteration = bgjobs;
  
  while(iteration){
    if(iteration->bg_status == DONE) {
      pid_t id_removed = iteration->id;
      // status_p status_removed = DONE;
      char * cmd_removed = iteration->cmd;

      remove_job(iteration->pid);
      char str[200];
      sprintf(str,"[%d]   Done             %s\n", id_removed, cmd_removed);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if(none<0) printf("wrong\n");
    }
    if (iteration->bg_status == RUNNING)
    {
      char *command = (char *) malloc(500 * sizeof(char));
      strcpy(command, iteration->cmd);
      if (command[strlen(command) - 1] != '&') strcat(command, " &");

      char str[200];
      sprintf(str,"[%d]   Running             %s\n", iteration->id, command);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if(none<0) printf("wrong\n");
      // printf("[%d]                        %s\n", iteration->id, command);
      fflush(stdout);
      free(command);
    }
    else if (iteration->bg_status == SUSPENDED)
    {
      char str[200];
      sprintf(str,"[%d]   Stopped             %s\n", iteration->id, iteration->cmd);
      
      int none = write(STDOUT_FILENO, str, strlen(str));
      if(none<0) printf("wrong\n");
      fflush(stdout);
    }
    // if(iteration->bg_status == DONE) {
    //   pid_t id_removed = iteration->id;
    //   status_p status_removed = DONE;
    //   char * cmd_removed = iteration->cmd;

    //   remove_job(iteration->pid);
    //   printf("[%d] [%s] %s \n", id_removed, status_to_string(status_removed),cmd_removed);
    // }
    // else{
    //   printf("[%d] [%d] [%s] %s \n", iteration->id,iteration->pid, status_to_string(iteration->bg_status),iteration->cmd);
    // }
    
    iteration = iteration->next;
  }

}

//bg function resume the job in the background
static void cd_func(commandT* cmd){
  int res;
  if(cmd->argc > 1){
    res =chdir(cmd->argv[1]);
  }
  else{
    res = chdir(getenv("HOME"));
  }
  if(res) printf("error cd\n");
}

static void bg_func(commandT* cmd){
  pid_t id = atoi(cmd->argv[1]);

  bgjobL * target = get_bgjob_by_id(id);
  if(!target) {
    // printf("bg: %d: no such job\n",id);
    return;
  }
  else{
    if(target->bg_status == RUNNING){
      // printf("bg: %d: already in the background\n",id);
    }
    else if(target->bg_status == DONE){
      // printf("bg: %d: no such job\n",id);
    }

    else if(target->bg_status == SUSPENDED){
      // printf("cont-b%d\n", kill(-(target->pid), SIGCONT));
      kill(-(target->pid), SIGCONT);
      target->bg_status = RUNNING;
    }
  }
  return;
}


static void fg_func(commandT* cmd){
  pid_t id = atoi(cmd->argv[1]);

  bgjobL * target = get_bgjob_by_id(id);
  //no such job
  if(!target) {
    // printf("fg: %d: no such job\n",id);
    return;
  }


  else{
    //the job is already done
    if(target->bg_status == DONE){
      fgpid = -1;
      // fgstatus = DONE;
      // printf("fg: job has terminated\n");
    }


    else{
      fgpid = target->pid;
      // fgstatus = target -> bg_status;
      // printf("cont_f%d\n", kill(-fgpid, SIGCONT));
      kill(-fgpid, SIGCONT);
      waitfg();
    }
  }
  return;
}

static void alias_func(commandT* cmd){
  if (cmd->argc < 2) {
    print_all_aliases();
  } else {
    set_alias(cmd);
  }
}

static void unalias_func(commandT* cmd){
  if (cmd->argc < 2) {
    //printf("Must list an alias to remove: unalias <aliasName>\n");
    return;
  }
  unset_alias(cmd);
}

static void print_all_aliases(){
  if (aliases.numOfAliases == 0) {
    //printf("No aliases defined.\n");
  } else {
    //printf("%d aliases defined.\n", aliases.numOfAliases);
    int i;
    for (i=0; i<aliases.numOfAliases; i++) {
	printf("alias %s='%s'\n", aliases.keys[i], aliases.values[i]);
    }
  }
}

// -1 means no, >=0 means yes
static int is_alias(char* command) {
  //printf("%d aliases, checking %s\n", aliases.numOfAliases, command);
  int i;
  for (i=0; i<aliases.numOfAliases; i++) {
    //printf("%s is an alias\n", aliases.keys[i]);
    if(strcmp(command,aliases.keys[i]) == 0) {
      return i;
    } 
  }
  return -1;
}

static char* get_alias(int index) {
  if (index >= 0 && index < aliases.numOfAliases) {
    return aliases.values[index];
  }
  return "";
}

static void set_alias(commandT* cmd) {
  char* aliasFormula = cmd->argv[1];
  char* equalSign = strchr(aliasFormula, '=');
  if (equalSign == NULL) {
    return;
  }

  char* key = get_alias_key(cmd); 
  if (key == NULL) {
    // error
    return;
  }
  char* value = get_alias_value(cmd);
  if (value == NULL) {
    // error 
  }
  aliases.keys[aliases.numOfAliases] = key;

  aliases.values[aliases.numOfAliases] = value;

  aliases.numOfAliases = aliases.numOfAliases + 1;
  //printf("%s aliased to %s\n", aliases.keys[aliases.numOfAliases-1], aliases.values[aliases.numOfAliases-1]);
}

static void unset_alias(commandT* cmd) {
  int index = is_alias(cmd->argv[1]);
  if (index == -1) {
    return;
  }
  
  free(aliases.keys[index]);
  free(aliases.values[index]);
  int i;
  for (i=index; i < aliases.numOfAliases -1; i++) {
    aliases.keys[i] = aliases.keys[i+1];
    aliases.values[i] = aliases.values[i+1];
  } 
  aliases.numOfAliases = aliases.numOfAliases - 1;
}

static char* get_alias_key(commandT* cmd) {
  char* aliasFormula = cmd->argv[1];
  char* equalSign = strchr(aliasFormula, '=');
  if (equalSign == NULL) {
    return NULL;
  }
  int index = equalSign - aliasFormula;

  int lenOfAlias = index + 1;
  char* key = malloc(sizeof(char) * lenOfAlias);
  strncpy(key, aliasFormula, index);
  key[lenOfAlias-1] = '\0';
 
  return key;
}

static char* get_alias_value(commandT* cmd) {
  //printf("%d argc\n", cmd->argc);
  int i;
  for (i=0; i< cmd->argc;i++) {
    //printf("%s\n", cmd->argv[i]);
  }
  char* aliasFormula = cmd->argv[1];
  char* quotePtr = strchr(aliasFormula, '\'');
  if (quotePtr == NULL) {
    return NULL;
  }
  int index = quotePtr - aliasFormula;

  int len = strlen(aliasFormula) - (index + 1); 


  char* value = malloc(sizeof(char) * len + 1); // + 1 for space then null char
  strncpy(value, quotePtr+1, len);
  value[len-1] = '\0';

  return value;
}


// static char * status_to_string(status_p st){
//   switch (st) 
//    {
//       case RUNNING: return "RUNNING";
//       case DONE: return "DONE";
//       case STOPPED: return "STOPPED";
//       case SUSPENDED: return "SUSPENDED";
//       default: return "N/A";
//    }
// }

void waitfg(){
  while(fgpid > 0){
    sleep(1);
  }
}
