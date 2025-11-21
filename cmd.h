#ifndef CMDH
#define CMDH

int handle_cmd(int argc, char** argv, char*** path, const char *outfile);
char** copy_null_terminated(int n, char** old);

#endif
