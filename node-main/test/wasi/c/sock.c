#include <sys/socket.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

// TODO(mhdawson): Update once sock_accept is implemented in uvwasi
int main(void) {
  int fd = 0 ;
  socklen_t addrlen = 0;
  int flags = 0;
  int ret = accept(10, NULL, &addrlen);
  assert(ret == -1);
  assert(errno == EBADF);

  return 0;
}
