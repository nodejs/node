#include <assert.h>
#include <stdio.h>

int main() {
  FILE* file = fopen("/sandbox/input.txt", "r");
  assert(file != NULL);

  char c = fgetc(file);
  while (c != EOF) {
    int wrote = fputc(c, stdout);
    assert(wrote != EOF);
    c = fgetc(file);
  }
}
