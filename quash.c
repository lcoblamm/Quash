/*
  The shell will run from here
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int getCommand(char* cmd, char* args, int cmdSize);
int execCommand(char* cmd, char* args); 
int cd(char* args);
int jobs();
int set(char* args);

int main(int argc, char* argv[])
{
  int cmdSize = 256;
  char* cmd = malloc(cmdSize);
  char* args = malloc(cmdSize);

  while (1) {
    memset(cmd, '\0', cmdSize);
    memset(args, '\0', cmdSize);
    printf("> ");
    // read in input   
    if (getCommand(cmd, args, cmdSize) != 0) {
      // error running command
      continue;
    }
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
      break;
    }
    if (strcmp(cmd, "cd") == 0) {
      cd(args);
      continue;
    }
    if (strcmp(cmd, "jobs") == 0) {
      jobs();
      continue;
    }
    // run command
    execCommand(cmd, args);
  }
  free(cmd);
  free(args);

  return 0;
}

/*
  Reads input and parses into a command & its arguments
  @param cmd: [out] preallocated array of cmdSize that
    will hold a null-terminated command
  @param arguments: [out] preallocated array of cmdSize 
    that will hold a null-terminated string of all arguments
  @return: 0 if command was successfully read in, -1 otherwise
*/
int getCommand(char* cmd, char* args, int cmdSize)
{
  int index = 0;
  int c;
  int bufSize = cmdSize;
  // read in command
  do {
    c = getchar();
    if (c == EOF || c == '\n') {
      if (index == 0) {
	// no command was entered
	return -1;
      }
      // reached end of input, append null
      cmd[index] = '\0';
      return 0;
    }
    if (c == ' ') {
      // reached end of command, insert null
      // and start reading arguments
      cmd[index] = '\0';
      break;
    }
    cmd[index] = c;
    
    index++;
    if (index >= bufSize) {
      // allocate more space for the buffer
      bufSize += bufSize;
      cmd = realloc(cmd, bufSize);
    }
  } while (1);

  index = 0;
  bufSize = cmdSize;
  // read in arguments
  do {
    c = getchar();
    if (c == EOF || c == '\n') {
      args[index] = '\0';
      return 0;
    }
    args[index] = c;
    
    index++;
    if (index >= bufSize) {
      bufSize += bufSize;
      args = realloc(args, bufSize);
    }
  } while (1);
}

/*
  Executes command by starting child process
  @param cmd: cmd to execute (null terminated)
  @param args: all args to pass to command (null terminated)
*/
int execCommand(char* cmd, char* args) 
{
  int status;
  pid_t pid;

  pid = fork();
  if (pid == 0) {
    // child process
    if (execlp(cmd, cmd, args, (char*) 0) < 0) {
      if (errno == 2) {
        fprintf(stderr, "\n%s not found.\n", cmd);
      }
      else {
        fprintf(stderr, "\nError execing %s. Error#%d\n", cmd, errno);
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
int cd(char* args) {
  if(args[1] == NULL){
    fprintf(stderr, "Error: Expected argument to \"cd\"\n"); 
  } 
  else{
     chdir(args[1]); 
  } 
  return 0;
}

int jobs() {
  // TODO: fill in code to print jobs
  return 0;
}

int set(char* args) {
  // TODO: fill in code to set up HOME/PATH variables
  return 0;
}
