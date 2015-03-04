/*
  The shell will run from here
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int getCommand(char*** cmd, int* numArgs);
int unpipeCommand(char** cmd, char*** unpiped[], int* numCmds);
int execCommand(char** cmd, char** envp, int bgFlag); 
int execPipedCommand(char*** cmdSet, int numCmds, char**envp);
int cd(char** args);
int jobs();
int set(char** args);

int main(int argc, char* argv[], char** envp)
{
  int numArgs = 1;
  char cwd[1024]; 
  int ret = 0;
  int backgroundFlag = 0; 
  
  while (1) {
    if(getcwd(cwd, sizeof(cwd)) != 0) {
      printf(cwd);
      printf(" > "); 
    }	
    else {
	   perror("getcwd error: "); 
    }
    
    // read in input 
    char** cmd = malloc(numArgs * sizeof(char*));
    ret = getCommand(&cmd, &numArgs);

    if (ret != 0) {
      // error running command
      continue;
    }

    // need to find way to remove final argument to this case. Might move to get command. 
    //char* background = strchr(cmd[numArgs - 1], '&'); 
    
    char* background = strchr(cmd[0], '&');
    if (background != 0) {
		  backgroundFlag = 1; 
		  cmd[0]++;
    } 
    else backgroundFlag = 0; 

    // search command for pipe 
    int pipeFlag = 0;
    int i = 0;
    while (cmd[i] != 0) {
      if (strcmp(cmd[i], "|") == 0) {
        pipeFlag = 1;
      }
      i++;
    }
    
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
      // free memory and exit
      int i = 0;
      while (cmd[i] != 0) {
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
    else if (pipeFlag == 1) {
      // parse into pieces between pipes
      char*** unpipedCmds = malloc(numArgs * sizeof(char**));
      int numCmds;
      ret = unpipeCommand(cmd, &unpipedCmds, &numCmds);
      // call execPipedCommand with array of commands and number of commands
      ret = execPipedCommand(unpipedCmds, numCmds, envp);
      // TODO: free unpipedCmds
    }
    else {
      execCommand(cmd, envp, backgroundFlag);
    }

    // free memory before getting next command
    i = 0;
    while (cmd[i] != 0) {
      free(cmd[i]);
      i++;
    }
    free(cmd);
  }
}

/*
  Reads input and parses into a command & its arguments
  @param cmd: [in/out] preallocated char** of size numArgs, 
    will hold command and its args on return with NULL as the final value
  @param numArgs: [in] number of args in cmd 
  @return: 0 if command was successfully read in, non-zero for error
*/
int getCommand(char*** cmd, int* numArgs)
{
  int cmdLength = 128;
  char* unparsedCmd = malloc(cmdLength * sizeof(char));
  if (!unparsedCmd) {
    fprintf(stderr, "getCommand allocation error\n");
    return 2;
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
        fprintf(stderr, "getCommand allocation error\n");
        return 2;
      }
    }
  } while (1);

  // parse command into invidual arguments
  int argNum = 0;
  char* arg = strtok(unparsedCmd, " ");
  while (arg != 0) {
    // allocate memory for string storage
    (*cmd)[argNum] = malloc((strlen(arg) + 1) * sizeof(char));
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
        fprintf(stderr, "getCommand allocation error\n");
        return 2;
      }
    }

    arg = strtok(0, " ");
  }
  // add one last null pointer
  (*cmd)[argNum] = 0;

  free(unparsedCmd);

  return 0;
}

/*
  Removes pipes from command and splits into separate commands
  @param cmd: [in] command to remove pipes from
  @param unpiped: [out] array of command vectors
  @param numCmds [out] number of separate commands
  @return: 0 for success
*/
int unpipeCommand(char** cmd, char*** unpiped[], int* numCmds)
{
  int lastIndex = 0;
  int index = 0;
  int unpipedIndex = 0;
  (*numCmds) = 0;
  // read each argument of command
  while (cmd[index] != 0) {
    if (strcmp(cmd[index], "|") == 0) {
      // allocate memory for command
      (*unpiped)[unpipedIndex] = malloc(((index - lastIndex) + 1) * sizeof(char*));
      memset((*unpiped)[unpipedIndex], '\0', ((index - lastIndex) + 1));

      int unpipedStrIdx = 0;
      // copy each string up to pipe into command vector of unpiped
      for(; lastIndex < index; ++lastIndex, ++unpipedStrIdx) {
        // allocate memory for string in command
        (*unpiped)[unpipedIndex][unpipedStrIdx] = malloc((strlen(cmd[lastIndex]) + 1) * sizeof(char));
        memset((*unpiped)[unpipedIndex][unpipedStrIdx], '\0', (strlen(cmd[lastIndex]) + 1));
        // copy string 
        strcpy((*unpiped)[unpipedIndex][unpipedStrIdx], cmd[lastIndex]);
      }
      (*unpiped)[unpipedIndex][unpipedStrIdx] = 0;
      lastIndex = index + 1;
      unpipedIndex++;
      (*numCmds)++;
    }
    index++;
  }
  // copy last command
  // allocate memory for command
  (*unpiped)[unpipedIndex] = malloc((index - lastIndex + 1) * sizeof(char*));
  memset((*unpiped)[unpipedIndex], '\0', (index - lastIndex + 1));

  int unpipedStrIdx = 0;
  // copy each string up to pipe into command vector of unpiped
  for(; lastIndex < index; ++lastIndex, ++unpipedStrIdx) {
    // allocate memory for string in command
    (*unpiped)[unpipedIndex][unpipedStrIdx] = malloc((strlen(cmd[lastIndex]) + 1) * sizeof(char));
    memset((*unpiped)[unpipedIndex][unpipedStrIdx], '\0', (strlen(cmd[lastIndex]) + 1));
    // copy string 
    strcpy((*unpiped)[unpipedIndex][unpipedStrIdx], cmd[lastIndex]);
  }
  (*unpiped)[unpipedIndex][unpipedStrIdx] = 0;
  (*numCmds)++;
  return 0;
}

/*
  Executes command by starting child process
  @param cmd: cmd with args to execute
  @param envp: array of environment variables to pass to command
  @return: 0 for success, non-zero for failure
*/
int execCommand(char** cmd, char** envp, int bgFlag) 
{
  if(bgFlag == 0) {
    int status;
    pid_t pid;

    pid = fork();
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
      if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "\nError in child process %d. Error#%d\n", pid, errno);
        return 1;
      }
      if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_FAILURE) {
          return 2;
        }
      }
    }
  }
  return 0;
}


  /*if(bgFlag == 1){
  int status;
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    // child process
	printf("[",jobcount,"] PID running in background");
    #ifdef __linux__
    if (execvpe(cmd[0], cmd, envp) < 0) {
    #elif __APPLE__
    if (execvP(cmd[0], getenv("PATH"), cmd) < 0) {
    #endif
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd[0]);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd[0], errno);
      }
		printf("[",jobcount"]", PID finished", cmd); 
      exit(EXIT_FAILURE);
    }
  }
	
else {
    // parent process	
	// the goal of this is to not wait for the child. Probably needs to be polished. 
	jobCount++;
	jobArray[jobCount].pid = pid;
	jobArray[jobCount].jobid = jobCount;
	jobArray[jobCount].cmd = cmd; 
   while (waitpid(pid_1,NULL, WEXITED|WNOHANG)> 0) { }
 //   if (WIFEXITED(status)) {
   //   if (WEXITSTATUS(status) == EXIT_FAILURE) {
       // return 2;
      }
    }
  } */

/*
  Executes command containing one or more pipes
  @param cmdSet: array of command vectors to execute
  @param numCmds: number of total piped commands
  @param envp: environment variables
  @return: 0 for success
*/
int execPipedCommand(char*** cmdSet, int numCmds, char**envp)
{
  int status;
  int numPipes = numCmds - 1;
  pid_t pids[numCmds];
  // create all pipes
  int pipefds[numPipes * 2];
  int i = 0;
  for (; i < numPipes; ++i) {
    pipe(pipefds + i * 2);
  }

  // fork all child processes
  int j = 0;
  for (; j < numCmds; ++j) {
    printf("Forking process %d\n", j);
    pids[j] = fork();
    if (pids[j] == 0) {
      // if not first command, set up input pipe
      if (j != 0) {
        printf("Reading from pipe %d\n", ((j -1) * 2));
        dup2(pipefds[(j - 1) * 2], STDIN_FILENO);
      }
      // if not last command, set up output pipe
      if (j != numCmds - 1) {
        printf("Writing to pipe %d\n", ((j * 2) + 1));
        dup2(pipefds[(j * 2) - 1], STDOUT_FILENO);
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
    }
  }
  return 0;
}
  
//i think this changes it i am not sure. 
int cd(char** args) 
{
  if(args[1] == '\0'){
    fprintf(stderr, "Error: Expected argument to \"cd\"\n"); 
  } 
  else{ 
	  if(chdir(args[1])!= 0){
	     printf("cd: %s: No such file or directory\n", args[1]); 
	  }
    return 1;
  } 
  return 0;
}

struct job {
	int pid;
	int jobid;
	char* cmd; 
} ;
struct job jobArray[100]; 
int jobCount = 0; 

int jobs() {
	int i;
  	for (i = 0; i < jobCount; i++) {
		printf("[", jobArray[i].jobid, "]", " ", jobArray[i].pid, " ", jobArray[i].cmd, "\n");  
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
int set(char** args) {
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
