#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>

#include "cmd.h"

#define CMD(name) int cmd_##name(int argc, char** argv, char*** path)
CMD(exit);
CMD(cd);
CMD(path);
CMD(nix_path);
CMD(cwd);
CMD(echo);

typedef int (*cmd_t)(int, char**, char***);
struct cmd { const char* name; const cmd_t func; };
const struct cmd cmds[] = {
  { "exit", cmd_exit },
  { "cd",   cmd_cd   },
  { "path", cmd_path },
  { "nix-path", cmd_nix_path }, // to not have to load path everytime
  // { "cwd",  cmd_cwd  },
  // { "echo", cmd_echo },
};
#define CMDS_SIZE sizeof(cmds)/sizeof(struct cmd)

int handle_cmd(int argc, char** argv, char*** path, const char *outfile) {
    // match built-in command
    for (int i = 0; i < CMDS_SIZE; i++){
      if (strcmp(argv[0], cmds[i].name) == 0){
        if (outfile != NULL) return -1;  
        return cmds[i].func(argc-1, argv+1, path);
      }
    }

    // else search the program in path
    char* bin = NULL;
    if (path == NULL || *path == NULL) return -1;
    char** tmp = *path;

    while (*tmp != NULL) {
      size_t len = strlen(*tmp) + 1 /*slash*/ + strlen(argv[0]) + 1 /*\0*/;
      bin = malloc(len);
      if (!bin) return -1;
      strcpy(bin, *tmp);
      strcat(bin, "/");
      strcat(bin, argv[0]);
      if (access(bin, X_OK) == 0) {
        break;
      }
      free(bin);
      bin = NULL;
      tmp++;
    }

    if (bin == NULL)
      return -1;

    pid_t pid = fork();
    if (pid < 0) {
      free(bin);
      return -1;
    } else if (pid == 0) {
      // child: apply redirection if requested
      if (outfile) {
        int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd < 0) _exit(1);
        if (dup2(fd, STDOUT_FILENO) < 0) _exit(1);
        if (dup2(fd, STDERR_FILENO) < 0) _exit(1);
        close(fd);
      }
      execv(bin, argv);
      _exit(1); /* exec failed */
    } else {
      waitpid(pid, NULL, 0);
    }

    free(bin);
    return 0;
}

CMD(exit) {
  if (argc != 0)
    return -1;
  exit(0);
}

CMD(cd) {
  if (argc != 1 || chdir(argv[0]) == -1)
    return -1;
  return 0;
}

CMD(path) {
  // free old path table if any 
  if (*path != NULL) {
    char** tmp = *path;
    for (; *tmp != NULL; tmp++) {
      free(*tmp);
    }
    free(*path);
    *path = NULL;
  }

  // allocate new table (argc entries + terminating NULL) 
  *path = malloc(sizeof(char*)*(argc+1));
  if (!*path) return -1;

  for (int i = 0; i < argc; i++) {
    (*path)[i] = malloc(sizeof(char)*(strlen(argv[i])+1));
    if (!(*path)[i]) {
      // cleanup already allocated entries
      for (int j = 0; j < i; j++) free((*path)[j]);
      free(*path);
      *path = NULL;
      return -1;
    }
    strcpy((*path)[i], argv[i]);
  }
  (*path)[argc] = NULL;

  return 0;
}

#define USER "fruit"
CMD(nix_path) {
  char** nargv = malloc(sizeof(char*)*(2+argc+1));
  nargv[0] = malloc(sizeof(char)*40);
  strcpy(nargv[0], "/run/current-system/sw/bin");
  nargv[1] = malloc(sizeof(char)*40);
  strcpy(nargv[1], "/etc/profiles/per-user/"USER"/bin");

  for (int i = 0; i < argc; i++) {
    nargv[2+i] = malloc(sizeof(char)*(strlen(argv[i])+1));
    strcpy(nargv[2+i], argv[i]);
  }
  nargv[2+argc] = NULL;

  // pass to cmd_path
  int r = cmd_path(2+argc, nargv, path);
  for (int i = 0; i < 2+argc; i++)
    free(nargv[i]);
  free(nargv);
  return r;
}

CMD(cwd) {
  if (argc != 0)
    return -1;
  char* cwd = getcwd(NULL, 0);
  printf("%s\n", cwd);
  free(cwd);
  return 0;
}

CMD(echo) {
  for (int i = 0; i < argc; i++)
    printf("%s ", argv[i]);
  printf("\n");
  return 0;
}
