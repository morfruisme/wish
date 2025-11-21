#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "cmd.h"

// #define DEBUG
// enable debug prints
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__);
#else
#define LOG(...)
#endif

const char* error_message = "An error has occurred\n";
#define ERR write(STDERR_FILENO, error_message, 22);
#define THROW_ERR ERR
#define ERR_AND_EXIT { ERR; exit(1); }

int parse_full(char* line, char**** argvs, char*** outfiles);
int parse(const char* line, char*** argv);

int main(int argc, char** argv) {
  // -- interactive / batch file mode
  int interactive = 1;
  FILE* input = stdin;

  if (argc == 1);
  else if (argc == 2 && (input = fopen(argv[1], "r"))) // valid source file
    interactive = 0;
  else
    ERR_AND_EXIT
  
  // -- set up path table
  #ifndef NIX_USER
  char* default_path[] = { "/bin" };
  #else
  char* default_path[] = { "/run/current-system/sw/bin", "/etc/profiles/per-user/"NIX_USER"/bin" };
  #endif

  char** path = copy_null_terminate(sizeof(default_path)/sizeof(char*), default_path);
  if (path == NULL)
    ERR_AND_EXIT

  // -- read line by line
  char* line = NULL;
  size_t size = 0; // unused (size of memory allocated for line)
  ssize_t length;
  
  while (1) {
    if (interactive) printf("wish> ");
    length = getline(&line, &size, input);

    if (length == -1) {
      int eof = feof(input);
      free(line);
      if (input != stdin)
        fclose(input);
      
      if (eof) // end of file
        exit(0);
      else
        ERR_AND_EXIT
    }

    // parse line
    char*** argvs;
    char** outfiles;
    if (parse_full(line, &argvs, &outfiles) == -1) {
      ERR
      continue;
    }

    int n = 0; // number of commands
    for (; argvs[n] != NULL; n++);
    pid_t* pids = malloc(sizeof(pid_t)*n);

    // execute each command
    for (int i = 0; i < n; i++) {
      int m = 0; // number of arguments
      for (; argvs[i][m] != NULL; m++);

      if ((pids[i] = handle_cmd(m, argvs[i], &path, outfiles[i])) == -1)
        ERR
    }

    // wait all commands to exit
    for (int i = 0; i < n; i++)
      if (pids[i] > 0)
        waitpid(pids[i], NULL, 0);
  }
}

// we call command a set of words (arguments) separated by whitespaces
// commands are separated from one another by '&'
//
// return -1 on error with argvs and outfiles set to NULL
// returns 0 on success with: 
//   argvs a null terminated 2 dimensional array argvs where argvs[i][j] is the argument j of command i
//   outfiles a null terminated array where outfiles[i] is either NULL or the path to the outfile for that command

// TODO: memory recovery on error (right now we quit instantly)

int parse_full(char* line, char**** _argvs, char*** _outfiles) {
  int n, i, j, new_arg;
  
  // -- count commands
  n = 1; // at least one command

  for (char* c = line; *c != '\0'; c++)
    if (*c == '&')
      n++;

  char*** argvs = malloc(sizeof(char**)*(n+1));
  argvs[n] = NULL;    // null terminate
  char** outfiles = malloc(sizeof(char*)*(n+1));
  for (int i = 0; i < n; i++)
    outfiles[i] = NULL; // default to no outfile
  outfiles[n] = NULL; // null terminate

  // -- count arguments for each command
  i = 0; // command index
  n = 0; // number of arguments
  new_arg = 1;

  for (char* c = line; ; c++) {
    if (*c == '>') {
      if (n == 0) // syntax error : empty command before '>'
        return -1;
      
      c++;
      for (; isspace((unsigned char)*c); c++); // skip whitespaces before

      n = 0;
      for (; *c != '&' && *c != '\0' && !isspace((unsigned char)*c); c++) {
        if (*c == '>') // syntax error : multiple '>'
          return -1;
        n++;
      }
      if (n == 0) // syntax error : no outfile provided
        return -1;
      outfiles[i] = malloc(sizeof(char*)*(n+1));
      strncpy(outfiles[i], c-n, n);
      outfiles[i][n] = '\0';

      for (; isspace((unsigned char)*c); c++); // skip whitespaces after
      if (*c != '&' && *c != '\0') // syntax error : argument after outfile
        return -1;
    }

    if (*c == '&' || *c == '\0') {
      // if (n == 0) // syntax error : empty command
      //   return -1;
      
      argvs[i] = malloc(sizeof(char*)*(n+1));
      argvs[i][n] = NULL; // null terminate

      // loop termination
      if (*c == '\0')
        break;

      // prepare for next command
      i++;
      n = 0;
      new_arg = 1;
    }
    else if (!isspace((unsigned char)*c)) {
      if (new_arg) {
        n++;
        new_arg = 0;
      }
    }
    else if (!new_arg)
      new_arg = 1;
  }

  // -- fill with arguments
  i = 0; // command index
  j = 0; // argument index
  n = 0; // number of letters
  new_arg = 1;

  for (char* c = line; ; c++) {    
    if (*c == '>' || *c == '&' || *c == '\0' || isspace((unsigned char)*c)) {
      if (!new_arg) {
        argvs[i][j] = malloc(sizeof(char)*(n+1));
        strncpy(argvs[i][j], c-n, n);
        argvs[i][j][n] = '\0';

        LOG("%d:%d %s\n", i, j, argvs[i][j]);
        
        // prepare for next argument
        j++;
        n = 0;
        new_arg = 1;
      }

      // ignore redirection
      if (*c == '>') {
        c++;
        for (; *c != '&' && *c != '\0'; c++);

        LOG("%d:outfile %s\n", i, outfiles[i]);
      }

      // next command
      if (*c == '&') {
        i++;
        j = 0;
      }

      // loop termination
      if (*c == '\0')
        break;
    }
    else {
      if (new_arg)
        new_arg = 0;
      n++;
    }
  }

  LOG("parse end\n");
  *_argvs = argvs;
  *_outfiles = outfiles;
  return 0;
}
