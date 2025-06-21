#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define BASE_DIR "/tmp"
#define OUTPUT_DIR BASE_DIR "/testdir"
#define PATH OUTPUT_DIR "/output.txt"
#define SIZE 500

int main(void) {
  struct timespec times[2];
  struct stat st;
  int fd;
  int ret;
  off_t pos;

  (void)st;
  ret = mkdir(OUTPUT_DIR, 0755);
  assert(ret == 0);

  fd = open(PATH, O_CREAT | O_WRONLY, 0666);
  assert(fd != -1);

  pos = lseek(fd, SIZE - 1, SEEK_SET);
  assert(pos == SIZE - 1);

  ret = (int)write(fd, "", 1);
  assert(ret == 1);

  ret = fstat(fd, &st);
  assert(ret == 0);
  assert(st.st_size == SIZE);

  times[0].tv_sec = 4;
  times[0].tv_nsec = 0;
  times[1].tv_sec = 9;
  times[1].tv_nsec = 0;
  assert(0 == futimens(fd, times));
  assert(0 == fstat(fd, &st));
  assert(4 == st.st_atime);
  assert(9 == st.st_mtime);

  ret = close(fd);
  assert(ret == 0);

  ret = access(PATH, R_OK);
  assert(ret == 0);

  ret = stat(PATH, &st);
  assert(ret == 0);
  assert(st.st_size == SIZE);

  ret = unlink(PATH);
  assert(ret == 0);

  ret = stat(PATH, &st);
  assert(ret == -1);

  ret = stat(OUTPUT_DIR, &st);
  assert(ret == 0);
  assert(S_ISDIR(st.st_mode));

  ret = rmdir(OUTPUT_DIR);
  assert(ret == 0);

  ret = stat(OUTPUT_DIR, &st);
  assert(ret == -1);

  return 0;
}
