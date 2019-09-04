#include <unistd.h>

int main(void) {
  isatty(1);
  __builtin_wasm_memory_grow(0, 1);
  isatty(1);
  return 0;
}
