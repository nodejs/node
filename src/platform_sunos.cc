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
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifdef SUNOS_HAVE_IFADDRS
# include <ifaddrs.h>
#endif



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

double Platform::prog_start_time = Platform::GetUptime();

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


int Platform::GetMemory(size_t *rss) {
  psinfo_t psinfo;
  int fd;

  if ((fd = open("/proc/self/psinfo", O_RDONLY)) < 0)
    return -1;

  if (read(fd, &psinfo, sizeof (psinfo_t)) != sizeof (psinfo_t)) {
    (void) close(fd);
    return -1;
  }

  *rss = (size_t) psinfo.pr_rssize * 1024;
  (void) close(fd);

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
    val = String::New("unrecognized data type");
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
    return -1;

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


double Platform::GetUptimeImpl() {
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;

  long hz = sysconf(_SC_CLK_TCK);
  double clk_intr;

  if ((kc = kstat_open()) == NULL)
    return -1;

  ksp = kstat_lookup(kc, (char *)"unix", 0, (char *)"system_misc");

  if (kstat_read(kc, ksp, NULL) == -1) {
    clk_intr = -1;
  } else {
    knp = (kstat_named_t *) kstat_data_lookup(ksp, (char *)"clk_intr");
    clk_intr = knp->value.ul / hz;
  }

  kstat_close(kc);

  return clk_intr;
}


Handle<Value> Platform::GetInterfaceAddresses() {
  HandleScope scope;

#ifndef SUNOS_HAVE_IFADDRS
  return ThrowException(Exception::Error(String::New(
    "This version of sunos doesn't support getifaddrs")));
#else
  struct ::ifaddrs *addrs, *ent;
  struct ::sockaddr_in *in4;
  struct ::sockaddr_in6 *in6;
  char ip[INET6_ADDRSTRLEN];
  Local<Object> ret, o;
  Local<String> name, ipaddr, family;
  Local<Array> ifarr;

  if (getifaddrs(&addrs) != 0) {
    return ThrowException(ErrnoException(errno, "getifaddrs"));
  }

  ret = Object::New();

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    bzero(&ip, sizeof (ip));
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING)) {
      continue;
    }

    if (ent->ifa_addr == NULL) {
      continue;
    }

    name = String::New(ent->ifa_name);
    if (ret->Has(name)) {
      ifarr = Local<Array>::Cast(ret->Get(name));
    } else {
      ifarr = Array::New();
      ret->Set(name, ifarr);
    }

    if (ent->ifa_addr->sa_family == AF_INET6) {
      in6 = (struct sockaddr_in6 *)ent->ifa_addr;
      inet_ntop(AF_INET6, &(in6->sin6_addr), ip, INET6_ADDRSTRLEN);
      family = String::New("IPv6");
    } else if (ent->ifa_addr->sa_family == AF_INET) {
      in4 = (struct sockaddr_in *)ent->ifa_addr;
      inet_ntop(AF_INET, &(in4->sin_addr), ip, INET6_ADDRSTRLEN);
      family = String::New("IPv4");
    } else {
      (void) strlcpy(ip, "<unknown sa family>", INET6_ADDRSTRLEN);
      family = String::New("<unknown>");
    }

    o = Object::New();
    o->Set(String::New("address"), String::New(ip));
    o->Set(String::New("family"), family);
    o->Set(String::New("internal"), ent->ifa_flags & IFF_PRIVATE || ent->ifa_flags &
	IFF_LOOPBACK ? True() : False());

    ifarr->Set(ifarr->Length(), o);

  }

  freeifaddrs(addrs);

  return scope.Close(ret);

#endif  // SUNOS_HAVE_IFADDRS
}


}  // namespace node

