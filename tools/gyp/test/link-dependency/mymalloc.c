#include <stdlib.h>

// The windows ninja generator is expecting an import library to get generated,
// but it doesn't if there are no exports.
#ifdef _MSC_VER
__declspec(dllexport) void foo() {}
#endif

void *malloc(size_t size) {
  (void)size;
  return (void*)0xdeadbeef;
}
