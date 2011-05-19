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

#include <kstat.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/loadavg.h>

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


static Handle<Value> data_named(kstat_named_t *knp) {
  Handle<Value> val;

  switch (knp->data_type) {
  case KSTAT_DATA_CHAR:
    val = Number::New(knp->value.c[0]);
    break;
  case KSTAT_DATA_INT32:
    val = Number::New(knp->value.i32);
    break;
  case KSTAT_DATA_UINT32:
    val = Number::New(knp->value.ui32);
    break;
  case KSTAT_DATA_INT64:
    val = Number::New(knp->value.i64);
    break;
  case KSTAT_DATA_UINT64:
    val = Number::New(knp->value.ui64);
    break;
  case KSTAT_DATA_STRING:
    val = String::New(KSTAT_NAMED_STR_PTR(knp));
    break;
  default:
    throw (String::New("unrecognized data type"));
  }

  return (val);
}


int Platform::GetCPUInfo(Local<Array> *cpus) {
  HandleScope scope;
  Local<Object> cpuinfo;
  Local<Object> cputimes;

  int           lookup_instance;
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;

  if ((kc = kstat_open()) == NULL)
    throw "could not open kstat";

  *cpus = Array::New();

  lookup_instance = 0;
  while (ksp = kstat_lookup(kc, (char *)"cpu_info", lookup_instance, NULL)){
    cpuinfo  = Object::New();

    if (kstat_read(kc, ksp, NULL) == -1) {
      /*
       * It is deeply annoying, but some kstats can return errors
       * under otherwise routine conditions.  (ACPI is one
       * offender; there are surely others.)  To prevent these
       * fouled kstats from completely ruining our day, we assign
       * an "error" member to the return value that consists of
       * the strerror().
       */
      cpuinfo->Set(String::New("error"), String::New(strerror(errno)));
      (*cpus)->Set(lookup_instance, cpuinfo);
    } else {
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"clock_MHz");
      cpuinfo->Set(String::New("speed"), data_named(knp));
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"brand");
      cpuinfo->Set(String::New("model"), data_named(knp));
      (*cpus)->Set(lookup_instance, cpuinfo);
    }

    lookup_instance++;
  }

  lookup_instance = 0;
  while (ksp = kstat_lookup(kc, (char *)"cpu", lookup_instance, (char *)"sys")){
    cpuinfo = (*cpus)->Get(lookup_instance)->ToObject();
    cputimes = Object::New();

    if (kstat_read(kc, ksp, NULL) == -1) {
      cputimes->Set(String::New("error"), String::New(strerror(errno)));
      cpuinfo->Set(String::New("times"), cpuinfo);
    } else {
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"cpu_ticks_kernel");
      cputimes->Set(String::New("system"), data_named(knp));
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"cpu_ticks_user");
      cputimes->Set(String::New("user"), data_named(knp));
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"cpu_ticks_idle");
      cputimes->Set(String::New("idle"), data_named(knp));
      knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"intr");
      cputimes->Set(String::New("irq"), data_named(knp));

      cpuinfo->Set(String::New("times"), cputimes);
    }

    lookup_instance++;
  }

  kstat_close(kc);

  return 0;
}


double Platform::GetFreeMemory() {
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;

  double pagesize = static_cast<double>(sysconf(_SC_PAGESIZE));
  ulong_t freemem;

  if((kc = kstat_open()) == NULL)
    throw "could not open kstat";

  ksp = kstat_lookup(kc, (char *)"unix", 0, (char *)"system_pages");

  if(kstat_read(kc, ksp, NULL) == -1){
    throw "could not read kstat";
  }
  else {
    knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"freemem");
    freemem = knp->value.ul;
  }

  kstat_close(kc);

  return static_cast<double>(freemem)*pagesize;
}


double Platform::GetTotalMemory() {
  double pagesize = static_cast<double>(sysconf(_SC_PAGESIZE));
  double pages = static_cast<double>(sysconf(_SC_PHYS_PAGES));

  return pagesize*pages;
}

double Platform::GetUptime() {
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;

  long hz = sysconf(_SC_CLK_TCK);
  ulong_t clk_intr;

  if ((kc = kstat_open()) == NULL)
    throw "could not open kstat";

  ksp = kstat_lookup(kc, (char *)"unix", 0, (char *)"system_misc");

  if (kstat_read(kc, ksp, NULL) == -1) {
    throw "unable to read kstat";
  } else {
    knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"clk_intr");
    clk_intr = knp->value.ul;
  }

  kstat_close(kc);

  return static_cast<double>( clk_intr / hz );
}

int Platform::GetLoadAvg(Local<Array> *loads) {
  HandleScope scope;
  double loadavg[3];

  (void) getloadavg(loadavg, 3);
  (*loads)->Set(0, Number::New(loadavg[LOADAVG_1MIN]));
  (*loads)->Set(1, Number::New(loadavg[LOADAVG_5MIN]));
  (*loads)->Set(2, Number::New(loadavg[LOADAVG_15MIN]));

  return 0;
}


}  // namespace node

