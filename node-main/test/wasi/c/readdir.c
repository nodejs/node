#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  DIR* dir;
  struct dirent* entry;
  char* platform;
  int cnt;
  int has_d_type;

  platform = getenv("NODE_PLATFORM");
  assert(platform != NULL);

  dir = opendir("/sandbox");
  assert(dir != NULL);

  cnt = 0;
  errno = 0;
  while (NULL != (entry = readdir(dir))) {
    if (strcmp(entry->d_name, "input.txt") == 0 ||
        strcmp(entry->d_name, "input2.txt") == 0 ||
        strcmp(entry->d_name, "notadir") == 0) {
        assert(entry->d_type == DT_REG);
    } else if (strcmp(entry->d_name, "subdir") == 0) {
        assert(entry->d_type == DT_DIR);
    } else {
      assert("unexpected file");
    }

    cnt++;
  }

  assert(errno == 0);
  assert(cnt == 4);
  closedir(dir);
  return 0;
}
