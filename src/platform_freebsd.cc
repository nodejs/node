#include "node.h"
#include "platform.h"

#include <stdlib.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <string.h>
#include <paths.h>
#include <fcntl.h>
#include <unistd.h>


namespace node {
static char *process_title;

char** OS::SetupArgs(int argc, char *argv[]) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


void OS::SetProcessTitle(char *title) {
  if (process_title) free(process_title);
  process_title = strdup(title);
  setproctitle(title);
}

const char* OS::GetProcessTitle(int *len) {
  if (process_title) {
    *len = strlen(process_title);
    return process_title;
  }
  *len = 0;
  return NULL;
}

int OS::GetMemory(size_t *rss, size_t *vsize) {
  kvm_t *kd = NULL;
  struct kinfo_proc *kinfo = NULL;
  pid_t pid;
  int nprocs;
  size_t page_size = getpagesize();

  pid = getpid();

  kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, "kvm_open");
  if (kd == NULL) goto error;

  kinfo = kvm_getprocs(kd, KERN_PROC_PID, pid, &nprocs);
  if (kinfo == NULL) goto error;

  *rss = kinfo->ki_rssize * page_size;
  *vsize = kinfo->ki_size;

  kvm_close(kd);

  return 0;

error:
  if (kd) kvm_close(kd);
  return -1;
}


int OS::GetExecutablePath(char* buffer, size_t* size) {
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  if (sysctl(mib, 4, buffer, size, NULL, 0) == -1) {
    return -1;
  }
  *size-=1;
  return 0;
}

}  // namespace node
