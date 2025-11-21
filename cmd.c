#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "cmd.h"

// prototype for built-in commands implemetation
#define DECLARE_CMD(name) int cmd_##name(int argc, char** argv, char*** path)
DECLARE_CMD(exit);
DECLARE_CMD(cd);
DECLARE_CMD(path);

// table of builtin-in commands name and their associated implementation
typedef int (*cmd_t)(int, char**, char***);
struct cmd { const char* name; const cmd_t func; };
const struct cmd cmds[] = {
  { "exit", cmd_exit },
  { "cd",   cmd_cd   },
  { "path", cmd_path },
};
#define CMDS_SIZE sizeof(cmds)/sizeof(struct cmd)

int handle_cmd(int argc, char** argv, char*** path, const char* outfile) {
    // -- match built-in command
    for (int i = 0; i < CMDS_SIZE; i++){
      if (strcmp(argv[0], cmds[i].name) == 0){
        if (outfile != NULL) return -1; // ??? pq pas
        return cmds[i].func(argc-1, argv+1, path);
      }
    }

    // -- else search the binary in path
    char* bin; // holds full path to the binary once found
    char** tmp = *path; // non null

    for (; *tmp != NULL; tmp++) { // null terminated
      // format <path>/<name>\0
      size_t len = strlen(*tmp) + 1 + strlen(argv[0]) + 1;
      bin = malloc(sizeof(char)*len);
      if (!bin) return -1;
      sprintf(bin, "%s/%s", *tmp, argv[0]);
 
      if (access(bin, X_OK) == 0) break;
      free(bin);
    }

    if (*tmp == NULL) return -1; // binary not found

    // -- execute the binary in child process
    pid_t pid = fork();
    if (pid < 0) { // error
      free(bin);
      return -1;
    }
    else if (pid == 0) { // child process
      // _exit(1) on error to not disturb parent process
      if (outfile) {
        int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) _exit(1);
        // redirect stdout and stderr to the output file
        if (dup2(fd, STDOUT_FILENO) < 0) _exit(1);
        if (dup2(fd, STDERR_FILENO) < 0) _exit(1);
        close(fd);
      }
      execv(bin, argv);
      _exit(1);
    }
    else // parent process
      waitpid(pid, NULL, 0);

    free(bin);
    return 0;
}

DECLARE_CMD(exit) {
  if (argc != 0)
    return -1;
  exit(0);
}

DECLARE_CMD(cd) {
  if (argc != 1 || chdir(argv[0]) == -1)
    return -1;
  return 0;
}

DECLARE_CMD(path) {
  // -- generate new path
  char** new_path = copy_null_terminate(argc, argv);
  if (new_path == NULL) return -1;
  
  // -- free old path
  for (char** tmp = *path; *tmp != NULL; tmp++) // null terminated
    free(*tmp);
  free(*path);
  
  // -- set new path
  *path = new_path;
  return 0;
}

// copy old array into a null terminated heap allocated new one
char** copy_null_terminate(int n, char** old) {
  char** new = malloc(sizeof(char*)*(n+1));
  if (new == NULL) return NULL;

  for (int i = 0; i < n; i++) {
    new[i] = malloc(sizeof(char)*(strlen(old[i])+1));
    if (new[i] == NULL) {
      // free previous allocations
      for (int j = 0; j <= i; j++)
        free(new[j]);
      free(new);
      return NULL;
    }
    else
      strcpy(new[i], old[i]);
  }
  new[n] = NULL; // null terminate

  return new;
}

// char** merge_null_terminate(int n, char** a, int m, char** b) {
//   char** new = malloc(sizeof(char*)*(n+m+1));
//   if (new == NULL) return NULL;

//   for (int i = 0; i < n; i++) {
//     new[i] = malloc(sizeof(char)*(strlen(a[i])+1));
//     if (new[i] == NULL) {
//       // free previous allocations
//       for (int j = 0; j <= i; j++)
//         free(new[j]);
//       free(new);
//       return NULL;
//     }
//     else
//       strcpy(new[i], a[i]);
//   }

//   for (int i = 0; i < m; i++) {
//     new[n+i] = malloc(sizeof(char)*(strlen(b[i])+1));
//     if (new[n+i] == NULL) {
//       // free previous allocations
//       for (int j = 0; j <= n+i; j++)
//         free(new[j]);
//       free(new);
//       return NULL;
//     }
//     else
//       strcpy(new[n+i], b[i]);
//   }

//   new[n+m] = NULL; // null terminate
//   return new;
// }
