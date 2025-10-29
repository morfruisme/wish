#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmd.h"

const char* error_message = "An error has occured\n";
#define THROW_ERR write(STDERR_FILENO, error_message, 21)

int parse(const char* line, char*** argv);

int main(int argc, char** argv) {
  char* line = NULL;
  size_t size; // unused (size of line mem alloc)
  ssize_t length;

  // path table ended by null pointer
  char** path = malloc(sizeof(char*));
  *path = NULL;
  
  while (1) {
    printf("wish> ");
    length = getline(&line, &size, stdin);

    if (length == -1) {
      THROW_ERR;
      free(line);
      return -1;
    }

    char** largv;
    int largc = parse(line, &largv);
    
    if (largc == 0) // no args
      continue;
    if (handle_cmd(largc, largv, &path) == -1)
      THROW_ERR;
    
    for (int i = 0; i < largc; i++)
      free(largv[i]);
    free(largv);
  }
}


// all strings (line and **argv) are null-terminated
// last pointer of *argv is null (for compatibility with execv)
int parse(const char* line, char*** argv) {
  // count the number of args
  int n = 0;
  int new = 1;
  for (int i = 0; line[i] != '\0'; i++) {
    if (!isspace((unsigned int)line[i])) {
      if (new) {
        n++;
        new = 0;
      }
    }
    else if (!new)
      new = 1;
  }

  // fill the args table
  *argv = malloc(sizeof(char**)*n);
  int j = 0;
  int offset = -1;
  for (int i = 0; line[i] != '\0'; i++) {
    if (!isspace((unsigned int)line[i])) {
      if (offset < 0)
        offset = i;
    }
    else if (offset >= 0) {
      (*argv)[j] = malloc(sizeof(char)*(i-offset+1));
      strncpy((*argv)[j], &(line[offset]), i-offset);
      (*argv)[j][i-offset] = '\0';
      j++;
      offset = -1;
    }
  }

  return n;
}
