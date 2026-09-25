#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

#define OLD "/sandbox/input.txt"
#define NEW "/tmp/output.txt"

int main() {
  struct stat st_old;
  struct stat st_new;

  assert(0 == stat(OLD, &st_old));
  assert(0 == link(OLD, NEW));
  assert(0 == stat(NEW, &st_new));
  assert(st_old.st_ino == st_new.st_ino);
  return 0;
}
