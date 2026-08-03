#include <assert.h>
#include <unistd.h>

int main() {
  char buf[256] = {0};
  int r = getentropy(buf, 256);
  assert(r == 0);

  for (int i = 0; i < 256; i++) {
    if (buf[i] != 0) {
      return 0;
    }
  }

  // if this ever is reached, we either have a bug or should buy a lottery
  // ticket
  return 1;
}
