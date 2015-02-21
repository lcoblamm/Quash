/*
  The shell will run from here
*/

#include <stdio.h>
#include <stdlib.h>

bool getCommand(char* cmd, char* args, cmdSize);
void execCommand(char* cmd, char* args); 

int main(int argc, char* argv[])
{
  int cmdSize = 256;
  char* cmd = malloc(cmdSize);
  char* args = malloc(cmdSize);

  while (true) {
    memset(cmd, '\0', cmdSize);
    memset(args, '\0', cmdSize);
    printf("> ");
    // read in input   
    getCommand(cmd, args, cmdSize);
    execCommand(cmd, args);
  }
  free(cmd);
  free(args);
}

/*
  Reads input and parses into a command & its arguments
  @param cmd: [out] preallocated array of cmdSize that
    will hold a null-terminated command
  @param arguments: [out] preallocated array of cmdSize 
    that will hold a null-terminated string of all arguments
  @return: true if command was successfully read in, false otherwise
*/
bool getCommand(char* cmd, char* args, cmdSize)
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
	return false;
      }
      // reached end of input, append null
      cmd[index] = '\0';
      return true;
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
  } while (true);
  index = 0;
  bufSize = cmdSize;
  // read in arguments
  do {
    c = getChar();
    if (c == EOF || c == '\n') {
      args[index] = '\0';
      return true;
    }
    args[index] = c;
    
    index++;
    if (index >= bufSize) {
      bufSize += bufSize;
      args = realloc(args, bufSize);
    }
  } while (true);
}

void execCommad(char* cmd, char* args) 
{
  // TODO: fork and execute command in child
}
