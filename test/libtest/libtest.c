#include <stdio.h>
#include <stdint.h>

#if defined(WIN32) || defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT int six = 6;

EXPORT void* n = NULL;

EXPORT char str[] = "hello world";

EXPORT uint64_t factorial(int max) {
  int i = max;
  uint64_t result = 1;

  while (i >= 2) {
    result *= i--;
  }

  return result;
}

EXPORT intptr_t factorial_addr = (intptr_t)&factorial;
