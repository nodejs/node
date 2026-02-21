#include <assert.h>
#include <string.h>

int main(int argc, char** argv) {
  assert(argc == 3);
  assert(0 == strcmp(argv[0], "foo"));
  assert(0 == strcmp(argv[1], "-bar"));
  assert(0 == strcmp(argv[2], "--baz=value"));
  return 0;
}
