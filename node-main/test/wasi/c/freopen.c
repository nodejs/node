#include <assert.h>
#include <stdio.h>

int main() {
  FILE* file_orig = fopen("/sandbox/input.txt", "r");
  assert(file_orig != NULL);
  FILE* file_new = freopen("/sandbox/input2.txt", "r", file_orig);
  assert(file_new != NULL);

  int c = fgetc(file_new);
  while (c != EOF) {
    int wrote = fputc((char)c, stdout);
    assert(wrote != EOF);
    c = fgetc(file_new);
  }
}
