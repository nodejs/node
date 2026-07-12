#include <assert.h>
#include <stdio.h>
#include <string.h>

static char* message = "hello, file!";

int main() {
  FILE* file = fopen("/tmp/output.txt", "w");
  assert(file != NULL);

  int nwritten = fprintf(file, "%s", message);
  assert(nwritten == strlen(message));
  int r = fclose(file);
  assert(r == 0);
}
