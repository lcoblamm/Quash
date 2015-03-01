/*
  The shell will run from here
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int getCommand(char*** cmd, int numArgs);
int execCommand(char** cmd); 
int cd(char** args);
int jobs();
int set(char** args);

int main(int argc, char* argv[])
{
  int numArgs = 1;
  char** cmd;
  char cwd[1024]; 
  
  while (1) {
    cmd = malloc(numArgs * sizeof(char*));
  
    if(getcwd(cwd, sizeof(cwd)) != NULL){
      printf(cwd);
      printf(" > "); 
    }	
    else
	   perror("getcwd error"); 
    
    // read in input   
    if (getCommand(&cmd, numArgs) != 0) {
      // error running command
      continue;
    }
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
      free(cmd);
      return 0;
    }
    if (strcmp(cmd[0], "cd") == 0) {
      cd(cmd);
      free(cmd);
      continue;
    }
    if (strcmp(cmd[0], "jobs") == 0) {
      jobs();
      free(cmd);
      continue;
    }
    // run command
    execCommand(cmd);
    free(cmd);
  }
}

/*
  Reads input and parses into a command & its arguments
  @param cmd: [in/out] preallocated char** of size numArgs, 
    will hold command and its args on return with NULL as the final value
  @param numArgs: [in] number of args in cmd 
  @return: 0 if command was successfully read in, -1 otherwise
*/
int getCommand(char*** cmd, int numArgs)
{
  int cmdLength = 128;
  char* unparsedCmd = malloc(cmdLength * sizeof(char));
  if (!unparsedCmd) {
    // Error in allocation
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
	       return -1;
      }
      // reached end of input, append null
      unparsedCmd[index] = '\0';
      break;
    }
    unparsedCmd[index] = c;
    
    index++;
    if (index >= cmdLength) {
      // allocate more space for the buffer
      cmdLength += cmdLength;
      unparsedCmd = realloc(unparsedCmd, cmdLength * sizeof(char));
      if (!unparsedCmd) {
        // error in reallocation
        return -1;
      }
    }
  } while (1);

  // parse command into invidual arguments
  int argNum = 0;
  char* arg = strtok(unparsedCmd, " ");
  while (arg != NULL) {
    (*cmd)[argNum] = arg;
    argNum++;

    if (argNum >= numArgs) {
      // need to reallocate, double size
      numArgs *= 2;
      *cmd = realloc(*cmd, numArgs * sizeof(char*));
      if (!(*cmd)) {
        // error in reallocations
        return -1;
      }
    }

    arg = strtok(NULL, " ");
  }
  // add one last null pointer
  (*cmd)[argNum] = NULL;

  free(unparsedCmd);
  return 0;
}

/*
  Executes command by starting child process
  @param cmd: cmd with args to execute
  @param numArgs: number of elements in cmd
*/
int execCommand(char** cmd) 
{
  int status;
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    // child process
    if (execvp(cmd[0], cmd) < 0) {
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd[0]);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd[0], errno);
      }
      return -1;
    }
    return 0;
  }
  else {
    // parent process
    if (waitpid(pid, &status, 0) == -1) {
      fprintf(stderr, "\nError in child process %d. Error#%d\n", pid, errno);
      return -1;
    }
  }
  return 0;
}

//i think this changes it i am not sure. 
int cd(char** args) {
  if(args[1] == '\0'){
    fprintf(stderr, "Error: Expected argument to \"cd\"\n"); 
  } 
  else{ 
	  if(chdir(args[1])!= 0){
	     printf("Error: Incorrect Path Name"); 
	  }
    } 
  
  return 0;
}

int jobs() {
  // TODO: fill in code to print jobs
  return 0;
}

int set(char** args) {
  // TODO: fill in code to set up HOME/PATH variables
  return 0;
}
