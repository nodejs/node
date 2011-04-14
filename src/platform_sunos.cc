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


#include <unistd.h> /* getpagesize() */
#include <stdlib.h> /* getexecname() */
#include <strings.h> /* strncpy() */


#if (!defined(_LP64)) && (_FILE_OFFSET_BITS - 0 == 64)
#define PROCFS_FILE_OFFSET_BITS_HACK 1
#undef _FILE_OFFSET_BITS
#else
#define PROCFS_FILE_OFFSET_BITS_HACK 0
#endif

#include <procfs.h>

#if (PROCFS_FILE_OFFSET_BITS_HACK - 0 == 1)
#define _FILE_OFFSET_BITS 64
#endif


namespace node {

using namespace v8;

char** Platform::SetupArgs(int argc, char *argv[]) {
  return argv;
}


void Platform::SetProcessTitle(char *title) {
  ;
}


const char* Platform::GetProcessTitle(int *len) {
  *len = 0;
  return NULL;
}


int Platform::GetMemory(size_t *rss, size_t *vsize) {
  pid_t pid = getpid();

  size_t page_size = getpagesize();
  char pidpath[1024];
  sprintf(pidpath, "/proc/%d/psinfo", pid);

  psinfo_t psinfo;
  FILE *f = fopen(pidpath, "r");
  if (!f) return -1;

  if (fread(&psinfo, sizeof(psinfo_t), 1, f) != 1) {
    fclose (f);
    return -1;
  }

  /* XXX correct? */

  *vsize = (size_t) psinfo.pr_size * page_size;
  *rss = (size_t) psinfo.pr_rssize * 1024;

  fclose (f);

  return 0;
}


int Platform::GetExecutablePath(char* buffer, size_t* size) {
  const char *execname = getexecname();
  if (!execname) return -1;
  if (execname[0] == '/') {
    char *result = strncpy(buffer, execname, *size);
    *size = strlen(result);
  } else {
    char *result = getcwd(buffer, *size);
    if (!result) return -1;
    result = strncat(buffer, "/", *size);
    if (!result) return -1;
    result = strncat(buffer, execname, *size);
    if (!result) return -1;
    *size = strlen(result);
  }
  return 0;
}


// TODO: libkstat provides all this info. Need to link it though.


int Platform::GetCPUInfo(Local<Array> *cpus) {
  // http://src.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/cmd/psrinfo/psrinfo.pl
  return 0;
}


double Platform::GetFreeMemory() {
  return 0.0;
}


double Platform::GetTotalMemory() {
  return 0.0;
}


double Platform::GetUptime() {
  // http://munin-monitoring.org/attachment/ticket/419/uptime
  return 0.0;
}


int Platform::GetLoadAvg(Local<Array> *loads) {
  return 0;
}


}  // namespace node

