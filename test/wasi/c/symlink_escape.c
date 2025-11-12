#include <assert.h>
#include <errno.h>
#include <stdio.h>

int main() {
  FILE* file = fopen("/sandbox/subdir/outside.txt", "r");
  assert(file == NULL);
  assert(errno == ENOTCAPABLE);
}
