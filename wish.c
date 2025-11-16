#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmd.c"

const char* error_message = "An error has occurred\n";
#define THROW_ERR write(STDERR_FILENO, error_message, 22)

int parse(const char* line, char*** argv);

int main(int argc, char** argv) {
  char* line = NULL;
  size_t size; // unused (size of line mem alloc)
  ssize_t length;
  FILE *input = stdin;

  if (argc > 2) {
    THROW_ERR;
    exit(1);
  }
  if (argc == 2) {
    input = fopen(argv[1], "r");
    if (!input) {
      THROW_ERR;
      exit(1);
    }
  }

  /* determine interactivity from the input stream (handles pipes/redir too) */
  int interactive = isatty(fileno(input)) ? 1 : 0;

  // path table ended by null pointer
  char** path = malloc(2*sizeof(char*));
  if (!path) { 
    THROW_ERR; 
    exit(1); 
  }
  path[0] = malloc(strlen("/bin") + 1);
  if (!path[0]) {
    free(path); 
    THROW_ERR; 
    exit(1); 
  }
  strcpy(path[0], "/bin");
  path[1] = NULL;
  
  while (1) {
    if (interactive) printf("wish> ");
    length = getline(&line, &size, input);

    if (length == -1) {
      if (feof(input)) {
        free(line);
        if (input != stdin) fclose(input);
        exit(0);
      } else {
        free(line);
        if (input != stdin) fclose(input);
        THROW_ERR;
        exit(1);
      }
    }

    char** largv;
    int largc = parse(line, &largv);

    if (largc == 0) // no args
      continue;

    const char *outfile = NULL;
    int redir_pos = -1;

    // check pour présence de '>' dans largv
    for (int i = 0; i < largc; ++i) {
        if (strcmp(largv[i], ">") == 0) {
            if (redir_pos != -1) { 
              THROW_ERR;
              for (int k = 0; k < largc; k++) free(largv[k]);
              free(largv);
              goto next_iteration;
            }
            redir_pos = i;
        }
    }

    if (redir_pos != -1) {
        // syntaxe: il faut au moins un token avant et un token après, et rien après le nom de fichier 
        if (redir_pos == 0 || redir_pos + 1 >= largc || redir_pos + 2 != largc) {
            THROW_ERR;
            for (int k = 0; k < largc; k++) free(largv[k]);
            free(largv);
            goto next_iteration;
        }
        char *outfile = largv[redir_pos + 1];
        // tronquer argv pour exec : mettre NULL à la place de '>'
        outfile = largv[redir_pos + 1];
        largv[redir_pos] = NULL;
        largc = redir_pos;
        // passer outfile à handle_cmd (voir option ci‑dessous)
    }
        
    if (handle_cmd(largc, largv, &path, outfile) == -1)
      THROW_ERR;
    
    for (int i = 0; i < largc; i++)
      free(largv[i]);

    // free the outfile string stored at position redir_pos+1
    if (redir_pos != -1) {
      free(largv[redir_pos + 1]);
    }
    free(largv);
    
    next_iteration:
    continue;
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
  *argv = malloc(sizeof(char**)*(n+1));
  (*argv)[n] = NULL; // null-terminate the argv table
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
