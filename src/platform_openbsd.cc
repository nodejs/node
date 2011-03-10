// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "platform.h"

#include <v8.h>

#include <stdlib.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/dkstat.h>
#include <uvm/uvm_param.h>
#include <string.h>
#include <paths.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <stdio.h>
namespace node {

using namespace v8;

static char *process_title;

char** Platform::SetupArgs(int argc, char *argv[]) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


void Platform::SetProcessTitle(char *title) {
  if (process_title) free(process_title);
  process_title = strdup(title);
  setproctitle(title);
}

const char* Platform::GetProcessTitle(int *len) {
  if (process_title) {
    *len = strlen(process_title);
    return process_title;
  }
  *len = 0;
  return NULL;
}

int Platform::GetMemory(size_t *rss, size_t *vsize) {
  kvm_t *kd = NULL;
  struct kinfo_proc2 *kinfo = NULL;
  pid_t pid;
  int nprocs, max_size = sizeof(struct kinfo_proc2);
  size_t page_size = getpagesize();

  pid = getpid();

  kd = kvm_open(NULL, _PATH_MEM, NULL, O_RDONLY, "kvm_open");
  if (kd == NULL) goto error;

  kinfo = kvm_getproc2(kd, KERN_PROC_PID, pid, max_size, &nprocs);
  if (kinfo == NULL) goto error;

  *rss = kinfo->p_vm_rssize * page_size;
  *vsize = kinfo->p_uru_ixrss;

  kvm_close(kd);

  return 0;

error:
  if (kd) kvm_close(kd);
  return -1;
}


int Platform::GetExecutablePath(char* buffer, size_t* size) {
  *size = 0;
  return -1;
}

int Platform::GetCPUInfo(Local<Array> *cpus) {
  Local<Object> cpuinfo;
  Local<Object> cputimes;
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed;
  uint64_t info[CPUSTATES];
  char model[512];
  int numcpus = 1;
  static int which[] = {CTL_HW, HW_MODEL, NULL};
  size_t size;

  size = sizeof(model);
  if (sysctl(which, 2, &model, &size, NULL, 0) < 0) {
    return -1;
  }
  which[1] = HW_NCPU;
  size = sizeof(numcpus);
  if (sysctl(which, 2, &numcpus, &size, NULL, 0) < 0) {
    return -1;
  }

  *cpus = Array::New(numcpus);

  which[1] = HW_CPUSPEED;
  size = sizeof(cpuspeed);
  if (sysctl(which, 2, &cpuspeed, &size, NULL, 0) < 0) {
    return -1;
  }
  size = sizeof(info);
  which[0] = CTL_KERN;
  which[1] = KERN_CPTIME2;
  for (int i = 0; i < numcpus; i++) {
    which[2] = i;
    size = sizeof(info);
    if (sysctl(which, 3, &info, &size, NULL, 0) < 0) {
      return -1;
    }
    cpuinfo = Object::New();
    cputimes = Object::New();
    cputimes->Set(String::New("user"),
                  Number::New((uint64_t)(info[CP_USER]) * multiplier));
    cputimes->Set(String::New("nice"),
                  Number::New((uint64_t)(info[CP_NICE]) * multiplier));
    cputimes->Set(String::New("sys"),
                  Number::New((uint64_t)(info[CP_SYS]) * multiplier));
    cputimes->Set(String::New("idle"),
                  Number::New((uint64_t)(info[CP_IDLE]) * multiplier));
    cputimes->Set(String::New("irq"),
                  Number::New((uint64_t)(info[CP_INTR]) * multiplier));

    cpuinfo->Set(String::New("model"), String::New(model));
    cpuinfo->Set(String::New("speed"), Number::New(cpuspeed));

    cpuinfo->Set(String::New("times"), cputimes);
    (*cpus)->Set(i, cpuinfo);
  }
  return 0;
}

double Platform::GetFreeMemory() {
  double pagesize = static_cast<double>(sysconf(_SC_PAGESIZE));
  struct uvmexp info;
  size_t size = sizeof(info);
  static int which[] = {CTL_VM, VM_UVMEXP};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return static_cast<double>(info.free) * pagesize;
}

double Platform::GetTotalMemory() {
#if defined(HW_PHYSMEM64)
  uint64_t info;
  static int which[] = {CTL_HW, HW_PHYSMEM64};
#else
  unsigned int info;
  static int which[] = {CTL_HW, HW_PHYSMEM};
#endif
  size_t size = sizeof(info);

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return static_cast<double>(info);
}

double Platform::GetUptime() {
  time_t now;
  struct timeval info;
  size_t size = sizeof(info);
  static int which[] = {CTL_KERN, KERN_BOOTTIME};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }
  now = time(NULL);

  return static_cast<double>(now - info.tv_sec);
}

int Platform::GetLoadAvg(Local<Array> *loads) {
  struct loadavg info;
  size_t size = sizeof(info);
  static int which[] = {CTL_VM, VM_LOADAVG};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }
  (*loads)->Set(0, Number::New(static_cast<double>(info.ldavg[0])
                               / static_cast<double>(info.fscale)));
  (*loads)->Set(1, Number::New(static_cast<double>(info.ldavg[1])
                               / static_cast<double>(info.fscale)));
  (*loads)->Set(2, Number::New(static_cast<double>(info.ldavg[2])
                               / static_cast<double>(info.fscale)));

  return 0;
}

}  // namespace node
