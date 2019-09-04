#include <assert.h>
#include <dirent.h>
#include <errno.h>

int main() {
  DIR* dir = opendir("/sandbox/notadir");
  assert(dir == NULL);
  assert(errno == ENOTDIR);

  return 0;
}
