/*
  File: quash.c
  Authors: Roxanne Calderon & Lynne Coblammers
  EECS 678 Project 1: Quite a Shell
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int getCommand(char** cmd[], int* numArgs);
int getCommandsFromFile(char*** cmds[], int* numArgs[], int* numCmds);
int splitCommand(char* cmd[], char*** separated[], int* numCmds, char* separator);

int execCommand(char* cmd[], int numArgs, char* envp[]); 
int execSimpleCommand(char* cmd[], char* envp[]);
int execPipedCommand(char** cmdSet[], int numCmds, char* envp[]);
int execRedirectedCommand(char* cmd[], int numArgs, char redirectSym, char* envp[]);
int execBackgroundCommand(char* cmd[], char* envp[]);
int execQuashFromFile(char* argv[], int argc, char* envp[]);

int cd(char* args[]);
int jobs();
int set(char* args[]);

void exitChildHandler(int signal, siginfo_t* info, void* ctx);
int killCMD(char** args); 
void preventProgramKill(int signal);
void allowProgramKill(int signal); 

struct job {
	int pid;
	int jobid;
	char* bgcommand; 
	int finishedFlag; 
} ;
struct job jobArray[1000]; 
int jobCount = 0; 

// used for blocking signals
sigset_t mask;
sigset_t oldMask;

int main(int argc, char* argv[], char* envp[])
{
  // set up signal mask
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  if (!isatty((fileno(stdin)))) {
    // input has been redirected (input not from terminal)
    int ret = execQuashFromFile(argv, argc, envp);
    return 0;
  }

  int numArgs = 4;
  int ret = 0;
  char cwd[1024]; 

  while (1) {
    if (getcwd(cwd, sizeof(cwd)) != 0) {
      printf("%s", cwd);
      printf(" > "); 
    }	
    else {
	   perror("getcwd error: "); 
    }
    
    // read in input 
    char** cmd = malloc(numArgs * sizeof(char*));
    if (!cmd) {
      fprintf(stderr, "\nAllocation error\n, Error:%d\n", errno);
      return -1;
    }
    if (getCommand(&cmd, &numArgs) != 0) {
      // error getting command
      continue;
    }

    // determine command type
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
      // free memory and exit
      int i;
      for (i = 0; i < numArgs; ++i) {
        free(cmd[i]);
        i++;
      }
      free(cmd);
      return 0;
    }
    else if (strcmp(cmd[0], "cd") == 0) {
      ret = cd(cmd);
    }
    else if (strcmp(cmd[0], "jobs") == 0) {
      ret = jobs();
    }
    else if (strcmp(cmd[0], "set") == 0) {
      ret = set(cmd);
    }
    else if (strcmp(cmd[0], "kill") == 0) {
      ret = killCMD(cmd);
    }
    else {
      ret = execCommand(cmd, numArgs, envp);
    }

    // free memory before getting next command
    int i;
    for (i = 0; i < numArgs; ++i) {
      free(cmd[i]);
      i++;
    }
    free(cmd);
  }
}

/*
  Executes quash with input from file of commands
  @param argv: arguments passed into main
  @param argc: number of arugments
  @param envp: environment variables
  @return: 0 for success, non-zero otherwise
*/
int execQuashFromFile(char* argv[], int argc, char* envp[])
{
  int ret = 0;
  int numCmds = 1;
  int* numArgs = malloc(numCmds * sizeof(int));
  if (!numArgs) {
    fprintf(stderr, "\nAllocation error\n, Error:%d\n", errno);
    return -1;
  }
  memset(numArgs, '\0', numCmds);
  char*** cmds = malloc(numCmds * sizeof(char**));
  if (!cmds) {
    fprintf(stderr, "\nAllocation error\n, Error:%d\n", errno);
    return -1;
  }
  memset(cmds, '\0', numCmds);

  // read in input
  if (getCommandsFromFile(&cmds, &numArgs, &numCmds) != 0) {
    // error getting command
    return -1;
  }

  int i;
  for(i = 0; i < numCmds; ++i) {
    char** currCmd = cmds[i];
    // determine command type
    if (strcmp(currCmd[0], "exit") == 0 || strcmp(currCmd[0], "quit") == 0) {
      // free memory and exit
      int j = 0;
      while (currCmd[j] != 0) {
        free(currCmd[j]);
        j++;
      }
      free(currCmd);
      return 0;
    }
    else if (strcmp(currCmd[0], "cd") == 0) {
      ret = cd(currCmd);
    }
    else if (strcmp(currCmd[0], "jobs") == 0) {
      ret = jobs();
    }
    else if (strcmp(currCmd[0], "set") == 0) {
      ret = set(currCmd);
    }
    else {
      ret = execCommand(currCmd, numArgs[i], envp);
    }
  }

  for (i = 0; i < numCmds; ++i) {
    int j = 0;
    for (; j < numArgs[i]; ++j) {
      free(cmds[i][j]);
      j++;
    }
    free(cmds[i]);
    i++;
  }
  free(cmds);

  return 0;
}

/*
  Reads input and parses into a command & its arguments
  @param cmd: [in/out] preallocated char** of size numArgs passed by reference, 
    will hold command and its args on return with NULL as the final value
  @param numArgs: [in/out] number of args in cmd 
  @return: 0 if command was successfully read in, non-zero for error
*/
int getCommand(char** cmd[], int* numArgs)
{
  int cmdLength = 128;
  char* unparsedCmd = malloc(cmdLength * sizeof(char));
  if (!unparsedCmd) {
    fprintf(stderr, "\ngetCommand allocation error, Error:%d\n", errno);
    return -1;
  }
  
  int index = 0;
  int c;
  // read in command
  do {
    c = getchar();
    if (c == EOF || c == '\n') {
      if (index == 0) {
	       // no command was entered
	       return 1;
      }
      // reached end of input, append null
      unparsedCmd[index] = '\0';
      break;
    }
    // add character to command
    unparsedCmd[index] = c;
    
    index++;
    if (index >= cmdLength) {
      // allocate more space for the buffer
      cmdLength += cmdLength;
      unparsedCmd = realloc(unparsedCmd, cmdLength * sizeof(char));
      if (!unparsedCmd) {
        // error in reallocation
        fprintf(stderr, "\ngetCommand allocation error, Error:%d\n", errno);
        return -1;
      }
    }
  } while (1);

  // parse command into invidual arguments
  int argNum = 0;
  char* arg = strtok(unparsedCmd, " ");
  while (arg != 0) {
    // allocate memory for string storage
    (*cmd)[argNum] = malloc((strlen(arg) + 1) * sizeof(char));
    if (!((*cmd)[argNum])) {
      fprintf(stderr, "\ngetCommand allocation error, Error:%d\n", errno);
      return -1;
    }
    memset((*cmd)[argNum], '\0', (strlen(arg) + 1));
    // NOTE: need to copy because unparsedCmd goes out of scope
    strcpy((*cmd)[argNum], arg);
    argNum++;
    if (argNum >= *numArgs) {
      // need to reallocate, double size
      *numArgs *= 2;
      *cmd = realloc(*cmd, (*numArgs) * sizeof(char*));
      if (!(*cmd)) {
        // error in reallocations
        fprintf(stderr, "\ngetCommand allocation error, Error:%d\n", errno);
        return -1;
      }
    }

    arg = strtok(0, " ");
  }
  // add one last null pointer
  (*cmd)[argNum] = 0;

  // set numArgs to be the actual number of arguments (not size of array)
  *numArgs = argNum;
  // shrink command vector to exactly the size needed
  (*cmd) = realloc(*cmd, (argNum + 1) * sizeof(char*));

  free(unparsedCmd);

  return 0;
}

/*
  Reads one or more commands from a file separated by new line characters
  @param cmds: [in/out] Preallocated array of string arrays passed in by refernece
  @param numArgs: [in/out] Preallocated array of number of arguments for each command
  @param numCmds: [in/out] Number of total commands read from file
  @return: 0 for success, non zero otherwise
*/
int getCommandsFromFile(char*** cmds[], int* numArgs[], int* numCmds)
{
  int endOfFile = 0;
  int cmdNum = 0;
  while (!endOfFile) {
    int cmdLength = 128;
    char* unparsedCmd = malloc(cmdLength * sizeof(char));
    if (!unparsedCmd) {
      fprintf(stderr, "\nAllocation error, Error:%d\n", errno);
      return -1;
    }
    
    int index = 0;
    int c;
    // read in command
    do {
      c = getchar();
      if (c == EOF) {
        if (index == 0 && cmdNum == 0) {
          // no command in file
          return -1; 
        }
        else if (index == 0) {
          // all commands have been read
          *numCmds = cmdNum;
          return 0;
        }
        unparsedCmd[index] = '\0';
        endOfFile = 1;
        break;
      }
      else if(c == '\n') {
        if (index == 0) {
          // no command was entered
          // start over on reading
          continue;
        }
        // reached end of input, append null
        unparsedCmd[index] = '\0';
        break;
      }
      // add character to command
      unparsedCmd[index] = c;
      
      index++;
      if (index >= cmdLength) {
        // allocate more space for the buffer
        cmdLength += cmdLength;
        unparsedCmd = realloc(unparsedCmd, cmdLength * sizeof(char));
        if (!unparsedCmd) {
          // error in reallocation
          fprintf(stderr, "\nReallocation error, Error:%d\n", errno);
          return -1;
        }
      }
    } while (1);

    // allocate command vector
    int currNumArgs = 2;
    (*cmds)[cmdNum] = malloc(currNumArgs * sizeof(char*));
    if (!(*cmds)[cmdNum]) {
      fprintf(stderr, "\nAllocation error, Error:%d\n", errno);
      return -1;
    }
    memset((*cmds)[cmdNum], '\0', currNumArgs);

    // parse command into invidual arguments
    int argNum = 0;
    char* arg = strtok(unparsedCmd, " ");
    while (arg != 0) {
      // allocate memory for string storage
      (*cmds)[cmdNum][argNum] = malloc((strlen(arg) + 1) * sizeof(char));
      if (!((*cmds)[cmdNum][argNum])) {
        fprintf(stderr, "\nAllocation error, Error:%d\n", errno);
        return -1;
      }
      memset((*cmds)[cmdNum][argNum], '\0', (strlen(arg) + 1));
      // NOTE: need to copy because unparsedCmd goes out of scope
      strcpy((*cmds)[cmdNum][argNum], arg);
      argNum++;
      if (argNum >= currNumArgs) {
        // need to reallocate, double size
        currNumArgs *= 2;
        (*cmds)[cmdNum] = realloc((*cmds)[cmdNum], (currNumArgs) * sizeof(char*));
        if (!(*cmds)[cmdNum]) {
          // error in reallocations
          fprintf(stderr, "\nReallocation error, Error:%d\n", errno);
          return -1;
        }
      }
      arg = strtok(0, " ");
    }
    // add one last null pointer
    (*cmds)[cmdNum][argNum] = 0;
    // set numArgs to be number of arguments for current command
    (*numArgs)[cmdNum] = argNum;
    //shrink command to exactly size needed
    (*cmds)[cmdNum] = realloc((*cmds)[cmdNum], (argNum + 1) * sizeof(char*));

    free(unparsedCmd);
    // check to see if we need to allocate for more commands
    cmdNum++;
    if (cmdNum >= *numCmds) {
      *numCmds *= 2;
      (*cmds) = realloc((*cmds), (*numCmds) * sizeof(char**));
      if (!(*cmds)) {
        // error in reallocations
        fprintf(stderr, "\nReallocation error, Error:%d\n", errno);
        return -1;
      }
      (*numArgs) = realloc((*numArgs), (*numCmds) * sizeof(int));
      if (!(*numArgs)) {
        // error in reallocations
        fprintf(stderr, "\nReallocation error, Error:%d\n", errno);
        return -1;
      }
    }
  }
  *numCmds = cmdNum;
  // shrink command array to exactly size needed
  *cmds = realloc(*cmds, cmdNum * sizeof(char**));
  return 0;
}

/*
  Splits command into several command vectors around separator provided
  @param cmd: [in] command to remove pipes from
  @param separated: [out] array of command vectors
  @param numCmds: [out] number of separate commands
  @param separator: [in] symbol to separate commands by (e.g. |, <, >)
  @return: 0 for success, non-zero otherwise
*/
int splitCommand(char* cmd[], char*** separated[], int* numCmds, char* separator)
{
  int lastIndex = 0; // keeps track of beginning of last command copied
  int index = 0; // keeps track of current place in original command
  int cmdVector = 0; 
  (*numCmds) = 0;
  // read each argument of command
  while (cmd[index] != 0) {
    if (strcmp(cmd[index], separator) == 0) {
      // allocate memory for command
      (*separated)[cmdVector] = malloc(((index - lastIndex) + 1) * sizeof(char*));
      if (!((*separated)[cmdVector])) {
        fprintf(stderr, "\nsplitCommand allocation error, Error:%d\n", errno);
        return -1;
      }
      memset((*separated)[cmdVector], '\0', ((index - lastIndex) + 1));

      int arg = 0;
      // copy each string up to pipe into command vector of separated
      for(; lastIndex < index; ++lastIndex, ++arg) {
        // allocate memory for string in command
        (*separated)[cmdVector][arg] = malloc((strlen(cmd[lastIndex]) + 1) * sizeof(char));
        if (!((*separated)[cmdVector][arg])) {
          fprintf(stderr, "\nsplitCommand allocation error, Error:%d\n", errno);
          return -1;
        }
        memset((*separated)[cmdVector][arg], '\0', (strlen(cmd[lastIndex]) + 1));
        // copy string 
        strcpy((*separated)[cmdVector][arg], cmd[lastIndex]);
      }
      (*separated)[cmdVector][arg] = 0;
      lastIndex = index + 1;
      cmdVector++;
      (*numCmds)++;
    }
    index++;
  }
  // copy last command
  // allocate memory for command
  (*separated)[cmdVector] = malloc((index - lastIndex + 1) * sizeof(char*));
  if (!((*separated)[cmdVector])) {
    fprintf(stderr, "\nsplitCommand allocation error, Error:%d\n", errno);
    return -1;
  }
  memset((*separated)[cmdVector], '\0', (index - lastIndex + 1));

  int arg = 0;
  // copy each string up to pipe into command vector of separated
  for(; lastIndex < index; ++lastIndex, ++arg) {
    // allocate memory for string in command
    (*separated)[cmdVector][arg] = malloc((strlen(cmd[lastIndex]) + 1) * sizeof(char));
    if (!((*separated)[cmdVector][arg])) {
      fprintf(stderr, "\nsplitCommand allocation error, Error:%d\n", errno);
      return -1;
    }
    memset((*separated)[cmdVector][arg], '\0', (strlen(cmd[lastIndex]) + 1));
    // copy string 
    strcpy((*separated)[cmdVector][arg], cmd[lastIndex]);
  }
  (*separated)[cmdVector][arg] = 0;
  (*numCmds)++;
  return 0;
}

/*
  Determines type of command to execute and calls approriated executor
  @param cmd: cmd with args to execute
  @param numArgs: number of arguments in command (including command itself)
  @param envp: array of environment variables to pass to command
  @return: 0 for success, non-zero for failure
*/
int execCommand(char* cmd[], int numArgs, char* envp[])
{
  // search command for pipes & redirects
  int redirectInFlag = 0;
  int redirectOutFlag = 0;
  int pipeFlag = 0;
  int bgFlag = 0; 
  int i = 0;
  while (cmd[i] != 0) {
    if (strcmp(cmd[i], "|") == 0) {
      pipeFlag = 1;
    }
    else if (strcmp(cmd[i], "<") == 0) {
      redirectInFlag = 1;
    }
    else if (strcmp(cmd[i], ">") == 0) {
      redirectOutFlag = 1;
    }
    else if (strcmp(cmd[i], "&") == 0) {
      bgFlag = 1;
    }
    i++;
  }

  int ret = 0;
  // we have access to numArgs here and this will be portable
  if (bgFlag) {
    // replace final & with null
    //cmd[numArgs - 1] = 0;
    char** newCmd = malloc(numArgs * sizeof(char*));
    memcpy(newCmd, cmd, (numArgs - 1));
    newCmd[numArgs - 1] = 0;
    ret = execBackgroundCommand(newCmd, envp);
    free(newCmd);
  } 
  else if (redirectInFlag || redirectOutFlag) {
    if (redirectInFlag == 1) {
      ret = execRedirectedCommand(cmd, numArgs, '<', envp);
    }
    else {
      // redirecting out
      ret = execRedirectedCommand(cmd, numArgs, '>', envp);
    }
  }
  else if (pipeFlag) {
    // parse into pieces between pipes
    char*** unpipedCmds = malloc(numArgs * sizeof(char**));
    if (!unpipedCmds) {
      fprintf(stderr, "\nAllocation error\n, Error:%d\n", errno);
      return -1;
    }
    memset(unpipedCmds, '\0', numArgs * sizeof(char**));
    int numCmds;
    if (splitCommand(cmd, &unpipedCmds, &numCmds, "|") != 0) {
      fprintf(stderr, "\nError in splitCommand\n");
      return -1;
    }

    // call execPipedCommand with array of commands and number of commands
    ret = execPipedCommand(unpipedCmds, numCmds, envp);

    // free up memory
    int i = 0;
    while (unpipedCmds[i] != 0) {
      int j = 0;
      while (unpipedCmds[i][j] != 0) {
        free(unpipedCmds[i][j]);
        j++;
      }
      free(unpipedCmds[i]);
      i++;
    }
    free(unpipedCmds);
  }
  else {
    ret = execSimpleCommand(cmd, envp);
  }
  return ret;
}

/*
  Executes command without pipes, redirection, or background instruction
  @param cmd: cmd with args to execute
  @param envp: environment variables
  @return: 0 for success, non-zero otherwise
*/
int execSimpleCommand(char* cmd[], char* envp[])
{
  //prevent control-c from killing quash
  signal(SIGINT, preventProgramKill);	 
  int status;
  pid_t pid;
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "\nError forking child. Error:%d\n", errno);
    exit(EXIT_FAILURE);
  }
  if (pid == 0) {
    // child process
    #ifdef __linux__
    if (execvpe(cmd[0], cmd, envp) < 0) {
    #endif
    #ifdef __APPLE__
    if (execvP(cmd[0], getenv("PATH"), cmd) < 0) {
    #endif
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd[0]);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd[0], errno);
      }
      exit(EXIT_FAILURE);
    }
    exit(0);
  }
  else {
    // parent process
    if (waitpid(pid, &status, 0) < 0) {
      fprintf(stderr, "\nError in child process %d. Error#%d\n", pid, errno);
      signal(SIGINT, allowProgramKill);
      return 1;
    }
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == EXIT_FAILURE) {
        return 2;
      }
    }
    //control c can terminate entire quash again
    signal(SIGINT, allowProgramKill);
    return 0;
  }
}

//when quash is execing a command, it cannot be killed.
void preventProgramKill(int signal)
{
	printf("\n");
} 

//when program is outside of execing a command, it can be killed
void allowProgramKill(int signal)
{
	printf("\n");
	exit(0); 
} 

/*
  Executes command containing one or more pipes
  @param cmdSet: array of command vectors to execute
  @param numCmds: number of total piped commands
  @param envp: environment variables
  @return: 0 for success, non-zero otherwise
*/
int execPipedCommand(char** cmdSet[], int numCmds, char* envp[])
{
  signal(SIGINT, preventProgramKill);	 
  int status;
  int numPipes = numCmds - 1;
  pid_t pids[numCmds];
  // create all pipes
  int pipefds[numPipes * 2];
  int i = 0;
  for (; i < numPipes; ++i) {
    if (pipe(pipefds + (i * 2)) < 0) {
      fprintf(stderr, "\nError creating pipe %d. Error:%d\n", (i * 2), errno);
      return -1;
    }
  }

  // fork all child processes
  int j = 0;
  for (; j < numCmds; ++j) {
    pids[j] = fork();
    if (pids[j] < 0) {
      fprintf(stderr, "\nError forking child %d. Error:%d\n", j, errno);
      exit(EXIT_FAILURE);
    }
    else if (pids[j] == 0) {
      // if not first command, set up input pipe
      if (j != 0) {
        if (dup2(pipefds[(j - 1) * 2], STDIN_FILENO) < 0) {
          fprintf(stderr, "\nError setting stdin to pipe %d. Error:%d\n", ((j - 1) * 2), errno);
          exit(EXIT_FAILURE);
        }
      }
      // if not last command, set up output pipe
      if (j != numCmds - 1) {
        if (dup2(pipefds[(j * 2) + 1], STDOUT_FILENO) < 0) {
          fprintf(stderr, "\nError setting stdout to pipe %d. Error:%d\n", ((j * 2) + 1), errno);
          exit(EXIT_FAILURE);
        }
      }
      // close all pipes
      i = 0;
      for (; i < numPipes * 2; ++i) {
        close(pipefds[i]);

      }
      // execute command
      #ifdef __linux__
      if (execvpe(cmdSet[j][0], cmdSet[j], envp) < 0) {
      #elif __APPLE__
      if (execvP(cmdSet[j][0], getenv("PATH"), cmdSet[j]) < 0) {
      #endif
        if (errno == 2) {
          fprintf(stderr, "\n%s not found.\n", cmdSet[j][0]);
        }
        else {
          fprintf(stderr, "\nError execing %s. Error#%d\n", cmdSet[j][0], errno);
        }
        exit(EXIT_FAILURE);
      }
      printf("Process %d exiting", (j + 1));
    signal(SIGINT, allowProgramKill);
      exit(0);
    }
  }

  // close all pipes
  i = 0;
  for (; i < numPipes * 2; ++i) {
    close(pipefds[i]);
  }

  // wait for all children
  i = 0;
  for (; i < numCmds; ++i) {
    if (waitpid(pids[i], &status, 0) < 0) {
      fprintf(stderr, "\nError in child process %d. Error#%d\n", pids[i], errno);
      signal(SIGINT, allowProgramKill);
      return -1;
    }
  }
    signal(SIGINT, allowProgramKill);
  return 0;
}

/*
  Executes command with redirect
  @param cmd: command to execute, including redirect & file
  @param numArgs: number of arguments in command (including command itself)
  @param redirectSym: either < or >
  @param envp: environment variables
  @return: 0 for success, non-zero otherwise
*/
int execRedirectedCommand(char* cmd[], int numArgs, char redirectSym, char* envp[])
{
  int status;
  int fd;
  pid_t pid;
  signal(SIGINT, preventProgramKill);	 
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "\nError creating pipe. Error:%d\n", errno);
    return -1;
  }

  if (pid == 0) {
    // child process
    // redirect input or output as needed
    if (redirectSym == '<') {
      fd = open(cmd[numArgs - 1], O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (fd < 0) {
        fprintf(stderr, "\nError opening %s. Error#%d\n", cmd[numArgs - 1], errno);
        exit(EXIT_FAILURE);
      }
      if (dup2(fd, STDIN_FILENO) < 0) {
        fprintf(stderr, "\nError resetting stdin to %s. Error#%d\n", cmd[numArgs -1], errno);
        exit(EXIT_FAILURE);
      }
    }
    else {
      fd = open(cmd[numArgs - 1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (fd < 0) {
        fprintf(stderr, "\nError opening %s. Error#%d\n", cmd[numArgs - 1], errno);
        exit(EXIT_FAILURE);
      }
      if (dup2(fd, STDOUT_FILENO) < 0) {
        fprintf(stderr, "\nError resetting stdout to %s. Error#%d\n", cmd[numArgs -1], errno);
        exit(EXIT_FAILURE);
      }
    }
    close(fd);
    // copy command up to redirect
    cmd = realloc(cmd, (numArgs - 1) * sizeof(char*));
    cmd[numArgs - 2] = 0;

    #ifdef __linux__
    if (execvpe(cmd[0], cmd, envp) < 0) {
    #endif
    #ifdef __APPLE__
    if (execvP(cmd[0], getenv("PATH"), cmd) < 0) {
    #endif
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd[0]);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd[0], errno);
      }
      exit(EXIT_FAILURE);
    }
    exit(0);
  }
  else {
    // parent process
    if (waitpid(pid, &status, 0) == -1) {
      fprintf(stderr, "\nError in child process %d. Error#%d\n", pid, errno);
      return -1;
    }
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == EXIT_FAILURE) {
        return -1;
      }
    }
      signal(SIGINT, allowProgramKill);
    return 0;
  }
}

/*
  Executes command as background process
  @param cmd: command to execute with arguments
  @param envp: environment variables
  @return: 0 for success, non-zero otherwise
*/
int execBackgroundCommand(char* cmd[], char* envp[])
{
  int status;
  pid_t pid;

  // create signal handler for child
  struct sigaction act;
  act.sa_sigaction = *exitChildHandler;
  act.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigaction(SIGCHLD, &act, NULL) < 0) {
    fprintf(stderr, "Error in handling child signal: Error%d\n", errno);
  }

  // this will make sure parent sets up jobs even if child finishes before parent is called
  sigprocmask(SIG_BLOCK, &mask, &oldMask);
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "\nError forking child. Error:%d\n", errno);
    exit(EXIT_FAILURE);
  }
  if (pid == 0) {
    // child process
    #ifdef __linux__
    if (execvpe(cmd[0], cmd, envp) < 0) {
    #endif
    #ifdef __APPLE__
    if (execvP(cmd[0], getenv("PATH"), cmd) < 0) {
    #endif
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd[0]);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd[0], errno);
      }
      exit(EXIT_FAILURE);
    }
    exit(0);
  }
  else {
    //create new job for job array with all job information
    struct job newjob;
    newjob.pid = pid; 
    newjob.jobid = jobCount;
    printf("[%d] %d running in background\n", jobCount, pid); 
    newjob.bgcommand = (char*) malloc(100);
    strcpy(newjob.bgcommand, cmd[0]);
    newjob.finishedFlag = 0; 
    jobArray[jobCount] = newjob;
    jobCount++;
    // since jobs have been set up, signals can now unblock
    sigprocmask(SIG_UNBLOCK, &mask, &oldMask);
    while (waitpid(pid, &status, WNOHANG) > 0) {} 
    return 0;
  } 
  
}
  
/* 
  @param: once background child ends, alert the rest of the program the child has exited. 
*/
void exitChildHandler(int signal, siginfo_t* info, void* ctx)
{
  // find job associated with PID
  pid_t pid = info->si_pid;
  int status;
  int i;
  for (i = 0; i < jobCount; ++i) {
    if (jobArray[i].pid == pid) {
      break;
    }
  }
  if (i < jobCount) {
    // found background job that completed
    printf("[%d] %d finished %s\n", jobArray[i].jobid, pid, jobArray[i].bgcommand); 
    jobArray[i].finishedFlag = 1;
    // free command name
    free(jobArray[i].bgcommand);
  }
}

/* 
  Changes directory
  @param args: command from commandline
  @return: 0 if successful

  Note: if cd is called with no additional
  args, will go to home
*/
int cd(char* args[]) 
{
  if (args[1] == '\0') {
    char* home = getenv("HOME");
	  chdir(home); 
  } 
  else { 
	  if (chdir(args[1])!= 0) {
    	  	printf("cd: %s: No such file or directory\n", args[1]); 
	  }
    return 1;
  } 
  return 0;
}

//scroll through jobs, looking for all commands still active
int jobs() 
{
  int i;
  for (i = 0; i < jobCount; i++) {
    if(jobArray[i].finishedFlag == 0) {
		  printf("[%d] %d %s \n", jobArray[i].jobid, jobArray[i].pid, jobArray[i].bgcommand); 
    }
	}
  return 0;
}

/*
  Prints or sets PATH & HOME variables
  @param args: command from commandline
  @return: 0 if successful

  Note: if set is called with no additional
  args, path and home will be printed
*/
int set(char* args[])
{
  if (args[1] == NULL) {
    // print home & path environment variables
    char* path = getenv("PATH");
    char* home = getenv("HOME");
    printf("PATH:%s\n", path);
    printf("HOME:%s\n", home);
  }
  else {
    // figure out whether setting path or home
    char* variable = strtok(args[1], "=");
    if (variable == NULL) {
      fprintf(stderr, "Usage: set <envVariable>=<newValue>\n");
      return 1;
    }
    else if (strcmp(variable, "PATH") == 0) {
      char* newPath = strtok(NULL, "=");
      if (newPath == NULL) {
        fprintf(stderr, "Usage: set <envVariable>=<newValue>\n");
        return 1;
      }
      setenv(variable, newPath, 1);
    }
    else if (strcmp(variable, "HOME") == 0) {
      char* newHome = strtok(NULL, "=");
      if (newHome == NULL) {
                fprintf(stderr, "Usage: set <envVariable>=<newValue>\n");
        return 1;
      }
      setenv(variable, newHome, 1);
    }
    else {
      fprintf(stderr, "set can be used for PATH or HOME\n");
      return 1;
    }
  }
  return 0;
}

/* 
  Kill Command
  @param args: command from commandline
  @return: 0 if successful
*/
int killCMD(char** args)
{
  //if no arguments, print error
  if (args[1] == NULL) {
    printf("Error: two inputs expected, none recieved \n");
    return 1; 
  }
  else {
    //if one argument, print error
    if (args[2] == NULL) {
      printf("Error: two inputs expected, one recieved \n"); 
      return 1; 
    }
    else {
      //convert args to ints
		  int jobNumber; 
		  sscanf(args[2], "%d", &jobNumber); 
		  int killSig; 
		  sscanf(args[1], "%d", &killSig); 

      //if job select is not empty, proceed
		  if (jobArray[jobNumber].pid != 0) {
        //just a warning about a 0 kill signal
        if (killSig == 0) {
				  printf("Kill signal of 0 will not kill process\n");
        }
        //kill process
        int pidToKill = jobArray[jobNumber].pid; 
        kill(pidToKill,killSig); 
		  }
      //if process does not exist print error
		  else {
        printf("Error: process does not exist \n"); 
        return 1; 
		  }
    }
  }  	
  return 0; 
}
