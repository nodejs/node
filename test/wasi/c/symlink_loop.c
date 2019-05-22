#include <assert.h>
#include <errno.h>
#include <stdio.h>

int main() {
  FILE* file = fopen("/sandbox/subdir1/loop1", "r");
  assert(file == NULL);
  assert(errno == ELOOP);
}
