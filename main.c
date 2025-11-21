#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "cmd.h"

const char* error_message = "An error has occurred\n";
#define ERR write(STDERR_FILENO, error_message, 22);
#define THROW_ERR ERR
#define DO_ERR_AND_EXIT(stuff) { stuff ERR; exit(1); }
#define ERR_AND_EXIT DO_ERR_AND_EXIT()

int parse(const char* line, char*** argv);

int main(int argc, char** argv) {
  // -- interactive / batch file mode
  int interactive = 1;
  FILE* input = stdin;

  if (argc == 1);
  else if (argc == 2 && (input = fopen(argv[1], "r"))) // valid source file
    interactive = 0;
  else ERR_AND_EXIT
  
  // -- set up path table
  #ifndef NIX_USER
  char* default_path[] = { "/bin" };
  #else
  char* default_path[] = { "/run/current-system/sw/bin", "/etc/profiles/per-user/"NIX_USER"/bin" };
  #endif

  char** path = copy_null_terminated(sizeof(default_path)/sizeof(char*), default_path);
  if (path == NULL) ERR_AND_EXIT

  // -- read line by line
  char* line = NULL;
  size_t size = 0; // unused (size of memory allocated for line)
  ssize_t length;
  
  while (1) {
    if (interactive) printf("wish> ");
    length = getline(&line, &size, input);

    if (length == -1) {
      if (feof(input)) {
        free(line);
        if (input != stdin) fclose(input);
        exit(0);
      } else DO_ERR_AND_EXIT(
        free(line);
        if (input != stdin) fclose(input);
      )
    }

    char** slargv;
    int slargc = parse(line, &slargv);

    if (slargc == 0){ // no args
      free(slargv);
      continue;
    }

    // collect background PIDs launched for this input line
    int *bg_pids = NULL;
    int bg_count = 0;

    int start = 0;
    while (start < slargc) {
      
      int end = start;
      while (end < slargc && strcmp(slargv[end], "&") != 0) end++;
      int seg_len = end - start; // number of tokens in this segment

      // empty segment (e.g. leading '&' or '&&') -> error
      if (seg_len == 0) {
        //THROW_ERR; //erreur dans test 16, commenter pour le passer
        for (int k = 0; k < slargc; ++k) free(slargv[k]);
        free(slargv);
        goto next_iteration;
      }

      // duplicate tokens for this segment into largv (safe for bg)
      char **largv = malloc(sizeof(char*) * (seg_len + 1));
      if (!largv) {
        THROW_ERR;
        for (int k = 0; k < slargc; ++k) free(slargv[k]);
        free(slargv);
        goto next_iteration;
      }
      for (int i = 0; i < seg_len; ++i) {
        largv[i] = slargv[start + i];
      }
      largv[seg_len] = NULL;

      const char *outfile = NULL;
      int redir_pos = -1;

      // check pour présence de '>' dans largv
      for (int i = 0; i < seg_len; ++i) {
          if (strcmp(largv[i], ">") == 0) {
              if (redir_pos != -1) { 
                THROW_ERR;
                free(largv);
                for (int k = 0; k < slargc; ++k) free(slargv[k]);
                free(slargv);
                goto next_iteration;
              }
              redir_pos = i;
          }
      }

      if (redir_pos != -1) {
          // syntaxe: il faut au moins un token avant et un token après, et rien après le nom de fichier 
          if (redir_pos == 0 || redir_pos + 1 >= seg_len || redir_pos + 2 != seg_len) {
              THROW_ERR;
              free(largv);
              for (int k = 0; k < seg_len; k++) free(slargv[k]);
              free(slargv);
              goto next_iteration;
          }
          // assign to the outer outfile (do NOT redeclare)
          outfile = largv[redir_pos + 1];
          // tronquer argv pour exec : mettre NULL à la place de '>'
          largv[redir_pos] = NULL;
          seg_len = redir_pos;
      }

      // background? (there is a '&' right after segment if end < slargc)
      int background = (end < slargc && strcmp(slargv[end], "&") == 0) ? 1 : 0;

      if (background) {
        pid_t bg = fork();
        if (bg < 0) {
          THROW_ERR;
        } else if (bg == 0) {
          // child: run the command; it inherits copies of slargv strings
          if (handle_cmd(seg_len, largv, &path, outfile) == -1) _exit(1);
          _exit(0);
        } else {
          // parent: don't wait; record bg pid so we can wait later in batch mode
          int *tmp = realloc(bg_pids, sizeof(int) * (bg_count + 1));
          if (tmp == NULL) {
            // allocation failure: still continue but don't record
          } else {
            bg_pids = tmp;
            bg_pids[bg_count++] = bg;
          }
        }
      } else {
        if (handle_cmd(seg_len, largv, &path, outfile) == -1) {
          THROW_ERR;
        }
      }

      free(largv);

      // advance to next segment (skip the '&' if present)
      start = end + (end < slargc ? 1 : 0);
      
    }

    // cleanup original tokens (we can free slargv now; background children have their own memory)
    for (int k = 0; k < slargc; ++k) free(slargv[k]);
    free(slargv);
    
    // in batch mode (non-interactive) wait for background jobs started on this line
    if (!interactive && bg_count > 0) {
      for (int i = 0; i < bg_count; ++i) {
        waitpid(bg_pids[i], NULL, 0);
      }
    }
    free(bg_pids);
    
    
    next_iteration:
    continue;
  }
}


// all strings (line and **argv) are null-terminated
// last pointer of *argv is null (for compatibility with execv)
int parse(const char* line, char*** argv) {
  // count the number of args
  const char *p = line;
  int n = 0;
  
  while (*p != '\0') {
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    if (*p == '\0') break;
    if (*p == '>' || *p == '&') { 
      n++; 
      p++; 
      continue; 
    }
    // normal token
    n++;
    while (*p != '\0' && !isspace((unsigned char)*p) && *p != '>' && *p != '&') p++;
  }

  // allocate argv table
  *argv = malloc(sizeof(char*)*(n+1));
  if (!*argv) return 0;
  (*argv)[n] = NULL; // null-terminate the argv table
  
  // fill the args table
  p = line;
  int idx = 0;
  while (*p != '\0' && idx < n) {
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    if (*p == '\0') break;

    if (*p == '>' || *p == '&') {
      (*argv)[idx] = malloc(2);
      if (!(*argv)[idx]) {
        for (int k = 0; k < idx; k++) free((*argv)[k]);
        free(*argv);
        *argv = NULL;
        return 0;
      }
      (*argv)[idx][0] = *p;
      (*argv)[idx][1] = '\0';
      idx++;
      p++;
      continue;
    }

    // normal token
    const char *start = p;
    while (*p != '\0' && !isspace((unsigned char)*p) && *p != '>' && *p != '&') p++;
    size_t len = (size_t)(p - start);
    (*argv)[idx] = malloc(len + 1);
    if (!(*argv)[idx]) { /* cleanup on failure */
      for (int k = 0; k < idx; k++) free((*argv)[k]);
      free(*argv);
      *argv = NULL;
      return 0;
    }
    memcpy((*argv)[idx], start, len);
    (*argv)[idx][len] = '\0';
    idx++;
  }


  return n;
}
