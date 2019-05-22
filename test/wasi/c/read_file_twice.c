// STDOUT: hello from input.txt\nhello from input.txt\n

#include <assert.h>
#include <stdio.h>

int main() {
  for (int i = 0; i < 2; i++) {
    FILE* file = fopen("/sandbox/input.txt", "r");
    assert(file != NULL);

    char c = fgetc(file);
    while (c != EOF) {
      assert(fputc(c, stdout) != EOF);
      c = fgetc(file);
    }
  }
}
