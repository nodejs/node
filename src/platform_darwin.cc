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

#include <mach/task.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <limits.h> /* PATH_MAX */

#include <unistd.h>  // sysconf
#include <sys/param.h>
#include <sys/sysctl.h>
#include <time.h>

namespace node {

using namespace v8;

static char *process_title;

char** Platform::SetupArgs(int argc, char *argv[]) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


// Platform::SetProcessTitle implemented in platform_darwin_proctitle.cc
}  // namespace node
#include "platform_darwin_proctitle.cc"
namespace node {


const char* Platform::GetProcessTitle(int *len) {
  if (process_title) {
    *len = strlen(process_title);
    return process_title;
  }
  *len = 0;
  return NULL;
}

// Researched by Tim Becker and Michael Knight
// http://blog.kuriositaet.de/?p=257
int Platform::GetMemory(size_t *rss, size_t *vsize) {
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  int r = task_info(mach_task_self(),
                    TASK_BASIC_INFO,
                    (task_info_t)&t_info,
                    &t_info_count);

  if (r != KERN_SUCCESS) return -1;

  *rss = t_info.resident_size;
  *vsize  = t_info.virtual_size;

  return 0;
}


int Platform::GetExecutablePath(char* buffer, size_t* size) {
  uint32_t usize = *size;
  int result = _NSGetExecutablePath(buffer, &usize);
  if (result) return result;

  char *path = new char[2*PATH_MAX];

  char *fullpath = realpath(buffer, path);
  if (fullpath == NULL) {
    delete [] path;
    return -1;
  }
  strncpy(buffer, fullpath, *size);
  delete [] fullpath;
  *size = strlen(buffer);
  return 0;
}

int Platform::GetCPUInfo(Local<Array> *cpus) {
  Local<Object> cpuinfo;
  Local<Object> cputimes;
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks);
  char model[512];
  uint64_t cpuspeed;
  size_t size;

  size = sizeof(model);
  if (sysctlbyname("hw.model", &model, &size, NULL, 0) < 0) {
    return -1;
  }
  size = sizeof(cpuspeed);
  if (sysctlbyname("hw.cpufrequency", &cpuspeed, &size, NULL, 0) < 0) {
    return -1;
  }

  natural_t numcpus;
  mach_msg_type_number_t count;
  processor_cpu_load_info_data_t *info;
  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numcpus,
                          reinterpret_cast<processor_info_array_t*>(&info),
                          &count) != KERN_SUCCESS) {
    return -1;
  }
  *cpus = Array::New(numcpus);
  for (int i = 0; i < numcpus; i++) {
    cpuinfo = Object::New();
    cputimes = Object::New();
    cputimes->Set(String::New("user"),
                  Number::New((uint64_t)(info[i].cpu_ticks[0]) * multiplier));
    cputimes->Set(String::New("nice"),
                  Number::New((uint64_t)(info[i].cpu_ticks[3]) * multiplier));
    cputimes->Set(String::New("sys"),
                  Number::New((uint64_t)(info[i].cpu_ticks[1]) * multiplier));
    cputimes->Set(String::New("idle"),
                  Number::New((uint64_t)(info[i].cpu_ticks[2]) * multiplier));
    cputimes->Set(String::New("irq"), Number::New(0));

    cpuinfo->Set(String::New("model"), String::New(model));
    cpuinfo->Set(String::New("speed"), Number::New(cpuspeed/1000000));

    cpuinfo->Set(String::New("times"), cputimes);
    (*cpus)->Set(i, cpuinfo);
  }
  vm_deallocate(mach_task_self(), (vm_address_t)info, count);

  return 0;
}

double Platform::GetFreeMemory() {
  double pagesize = static_cast<double>(sysconf(_SC_PAGESIZE));
  vm_statistics_data_t info;
  mach_msg_type_number_t count = sizeof(info) / sizeof(integer_t);

  if (host_statistics(mach_host_self(), HOST_VM_INFO,
                      (host_info_t)&info, &count) != KERN_SUCCESS) {
    return -1;
  }

  return (static_cast<double>(info.free_count)) * pagesize;
}

double Platform::GetTotalMemory() {
  uint64_t info;
  static int which[] = {CTL_HW, HW_MEMSIZE};
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
