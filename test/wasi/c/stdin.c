// STDIN: hello world
// STDOUT: hello world

#include <stdio.h>

int main(void) {
  char x[32];

  if (fgets(x, sizeof x, stdin) == NULL) {
    return ferror(stdin);
  }
  if (fputs(x, stdout) == EOF) {
    return ferror(stdout);
  }
  return 0;
}
