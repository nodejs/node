/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifndef SUNOS_NO_IFADDRS
# include <ifaddrs.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <sys/sockio.h>

#include <sys/loadavg.h>
#include <sys/time.h>
#include <unistd.h>
#include <kstat.h>
#include <fcntl.h>

#include <sys/port.h>
#include <port.h>

#define PORT_FIRED 0x69
#define PORT_UNUSED 0x0
#define PORT_LOADED 0x99
#define PORT_DELETED -1

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


int uv__platform_loop_init(uv_loop_t* loop) {
  int err;
  int fd;

  loop->fs_fd = -1;
  loop->backend_fd = -1;

  fd = port_create();
  if (fd == -1)
    return UV__ERR(errno);

  err = uv__cloexec(fd, 1);
  if (err) {
    uv__close(fd);
    return err;
  }
  loop->backend_fd = fd;

  return 0;
}


void uv__platform_loop_delete(uv_loop_t* loop) {
  if (loop->fs_fd != -1) {
    uv__close(loop->fs_fd);
    loop->fs_fd = -1;
  }

  if (loop->backend_fd != -1) {
    uv__close(loop->backend_fd);
    loop->backend_fd = -1;
  }
}


int uv__io_fork(uv_loop_t* loop) {
#if defined(PORT_SOURCE_FILE)
  if (loop->fs_fd != -1) {
    /* stop the watcher before we blow away its fileno */
    uv__io_stop(loop, &loop->fs_event_watcher, POLLIN);
  }
#endif
  uv__platform_loop_delete(loop);
  return uv__platform_loop_init(loop);
}


void uv__platform_invalidate_fd(uv_loop_t* loop, int fd) {
  struct port_event* events;
  uintptr_t i;
  uintptr_t nfds;

  assert(loop->watchers != NULL);

  events = (struct port_event*) loop->watchers[loop->nwatchers];
  nfds = (uintptr_t) loop->watchers[loop->nwatchers + 1];
  if (events == NULL)
    return;

  /* Invalidate events with same file descriptor */
  for (i = 0; i < nfds; i++)
    if ((int) events[i].portev_object == fd)
      events[i].portev_object = -1;
}


int uv__io_check_fd(uv_loop_t* loop, int fd) {
  if (port_associate(loop->backend_fd, PORT_SOURCE_FD, fd, POLLIN, 0))
    return UV__ERR(errno);

  if (port_dissociate(loop->backend_fd, PORT_SOURCE_FD, fd))
    abort();

  return 0;
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  struct port_event events[1024];
  struct port_event* pe;
  struct timespec spec;
  QUEUE* q;
  uv__io_t* w;
  sigset_t* pset;
  sigset_t set;
  uint64_t base;
  uint64_t diff;
  unsigned int nfds;
  unsigned int i;
  int saved_errno;
  int have_signals;
  int nevents;
  int count;
  int err;
  int fd;

  if (loop->nfds == 0) {
    assert(QUEUE_EMPTY(&loop->watcher_queue));
    return;
  }

  while (!QUEUE_EMPTY(&loop->watcher_queue)) {
    q = QUEUE_HEAD(&loop->watcher_queue);
    QUEUE_REMOVE(q);
    QUEUE_INIT(q);

    w = QUEUE_DATA(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);

    if (port_associate(loop->backend_fd, PORT_SOURCE_FD, w->fd, w->pevents, 0))
      abort();

    w->events = w->pevents;
  }

  pset = NULL;
  if (loop->flags & UV_LOOP_BLOCK_SIGPROF) {
    pset = &set;
    sigemptyset(pset);
    sigaddset(pset, SIGPROF);
  }

  assert(timeout >= -1);
  base = loop->time;
  count = 48; /* Benchmarks suggest this gives the best throughput. */

  for (;;) {
    if (timeout != -1) {
      spec.tv_sec = timeout / 1000;
      spec.tv_nsec = (timeout % 1000) * 1000000;
    }

    /* Work around a kernel bug where nfds is not updated. */
    events[0].portev_source = 0;

    nfds = 1;
    saved_errno = 0;

    if (pset != NULL)
      pthread_sigmask(SIG_BLOCK, pset, NULL);

    err = port_getn(loop->backend_fd,
                    events,
                    ARRAY_SIZE(events),
                    &nfds,
                    timeout == -1 ? NULL : &spec);

    if (pset != NULL)
      pthread_sigmask(SIG_UNBLOCK, pset, NULL);

    if (err) {
      /* Work around another kernel bug: port_getn() may return events even
       * on error.
       */
      if (errno == EINTR || errno == ETIME)
        saved_errno = errno;
      else
        abort();
    }

    /* Update loop->time unconditionally. It's tempting to skip the update when
     * timeout == 0 (i.e. non-blocking poll) but there is no guarantee that the
     * operating system didn't reschedule our process while in the syscall.
     */
    SAVE_ERRNO(uv__update_time(loop));

    if (events[0].portev_source == 0) {
      if (timeout == 0)
        return;

      if (timeout == -1)
        continue;

      goto update_timeout;
    }

    if (nfds == 0) {
      assert(timeout != -1);
      return;
    }

    have_signals = 0;
    nevents = 0;

    assert(loop->watchers != NULL);
    loop->watchers[loop->nwatchers] = (void*) events;
    loop->watchers[loop->nwatchers + 1] = (void*) (uintptr_t) nfds;
    for (i = 0; i < nfds; i++) {
      pe = events + i;
      fd = pe->portev_object;

      /* Skip invalidated events, see uv__platform_invalidate_fd */
      if (fd == -1)
        continue;

      assert(fd >= 0);
      assert((unsigned) fd < loop->nwatchers);

      w = loop->watchers[fd];

      /* File descriptor that we've stopped watching, ignore. */
      if (w == NULL)
        continue;

      /* Run signal watchers last.  This also affects child process watchers
       * because those are implemented in terms of signal watchers.
       */
      if (w == &loop->signal_io_watcher)
        have_signals = 1;
      else
        w->cb(loop, w, pe->portev_events);

      nevents++;

      if (w != loop->watchers[fd])
        continue;  /* Disabled by callback. */

      /* Events Ports operates in oneshot mode, rearm timer on next run. */
      if (w->pevents != 0 && QUEUE_EMPTY(&w->watcher_queue))
        QUEUE_INSERT_TAIL(&loop->watcher_queue, &w->watcher_queue);
    }

    if (have_signals != 0)
      loop->signal_io_watcher.cb(loop, &loop->signal_io_watcher, POLLIN);

    loop->watchers[loop->nwatchers] = NULL;
    loop->watchers[loop->nwatchers + 1] = NULL;

    if (have_signals != 0)
      return;  /* Event loop should cycle now so don't poll again. */

    if (nevents != 0) {
      if (nfds == ARRAY_SIZE(events) && --count != 0) {
        /* Poll for more events but don't block this time. */
        timeout = 0;
        continue;
      }
      return;
    }

    if (saved_errno == ETIME) {
      assert(timeout != -1);
      return;
    }

    if (timeout == 0)
      return;

    if (timeout == -1)
      continue;

update_timeout:
    assert(timeout > 0);

    diff = loop->time - base;
    if (diff >= (uint64_t) timeout)
      return;

    timeout -= diff;
  }
}


uint64_t uv__hrtime(uv_clocktype_t type) {
  return gethrtime();
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 */
int uv_exepath(char* buffer, size_t* size) {
  ssize_t res;
  char buf[128];

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  snprintf(buf, sizeof(buf), "/proc/%lu/path/a.out", (unsigned long) getpid());

  res = *size - 1;
  if (res > 0)
    res = readlink(buf, buffer, res);

  if (res == -1)
    return UV__ERR(errno);

  buffer[res] = '\0';
  *size = res;
  return 0;
}


uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}


uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}


void uv_loadavg(double avg[3]) {
  (void) getloadavg(avg, 3);
}


#if defined(PORT_SOURCE_FILE)

static int uv__fs_event_rearm(uv_fs_event_t *handle) {
  if (handle->fd == -1)
    return UV_EBADF;

  if (port_associate(handle->loop->fs_fd,
                     PORT_SOURCE_FILE,
                     (uintptr_t) &handle->fo,
                     FILE_ATTRIB | FILE_MODIFIED,
                     handle) == -1) {
    return UV__ERR(errno);
  }
  handle->fd = PORT_LOADED;

  return 0;
}


static void uv__fs_event_read(uv_loop_t* loop,
                              uv__io_t* w,
                              unsigned int revents) {
  uv_fs_event_t *handle = NULL;
  timespec_t timeout;
  port_event_t pe;
  int events;
  int r;

  (void) w;
  (void) revents;

  do {
    uint_t n = 1;

    /*
     * Note that our use of port_getn() here (and not port_get()) is deliberate:
     * there is a bug in event ports (Sun bug 6456558) whereby a zeroed timeout
     * causes port_get() to return success instead of ETIME when there aren't
     * actually any events (!); by using port_getn() in lieu of port_get(),
     * we can at least workaround the bug by checking for zero returned events
     * and treating it as we would ETIME.
     */
    do {
      memset(&timeout, 0, sizeof timeout);
      r = port_getn(loop->fs_fd, &pe, 1, &n, &timeout);
    }
    while (r == -1 && errno == EINTR);

    if ((r == -1 && errno == ETIME) || n == 0)
      break;

    handle = (uv_fs_event_t*) pe.portev_user;
    assert((r == 0) && "unexpected port_get() error");

    events = 0;
    if (pe.portev_events & (FILE_ATTRIB | FILE_MODIFIED))
      events |= UV_CHANGE;
    if (pe.portev_events & ~(FILE_ATTRIB | FILE_MODIFIED))
      events |= UV_RENAME;
    assert(events != 0);
    handle->fd = PORT_FIRED;
    handle->cb(handle, NULL, events, 0);

    if (handle->fd != PORT_DELETED) {
      r = uv__fs_event_rearm(handle);
      if (r != 0)
        handle->cb(handle, NULL, 0, r);
    }
  }
  while (handle->fd != PORT_DELETED);
}


int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  return 0;
}


int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* path,
                      unsigned int flags) {
  int portfd;
  int first_run;
  int err;

  if (uv__is_active(handle))
    return UV_EINVAL;

  first_run = 0;
  if (handle->loop->fs_fd == -1) {
    portfd = port_create();
    if (portfd == -1)
      return UV__ERR(errno);
    handle->loop->fs_fd = portfd;
    first_run = 1;
  }

  uv__handle_start(handle);
  handle->path = uv__strdup(path);
  handle->fd = PORT_UNUSED;
  handle->cb = cb;

  memset(&handle->fo, 0, sizeof handle->fo);
  handle->fo.fo_name = handle->path;
  err = uv__fs_event_rearm(handle);
  if (err != 0) {
    uv_fs_event_stop(handle);
    return err;
  }

  if (first_run) {
    uv__io_init(&handle->loop->fs_event_watcher, uv__fs_event_read, portfd);
    uv__io_start(handle->loop, &handle->loop->fs_event_watcher, POLLIN);
  }

  return 0;
}


int uv_fs_event_stop(uv_fs_event_t* handle) {
  if (!uv__is_active(handle))
    return 0;

  if (handle->fd == PORT_FIRED || handle->fd == PORT_LOADED) {
    port_dissociate(handle->loop->fs_fd,
                    PORT_SOURCE_FILE,
                    (uintptr_t) &handle->fo);
  }

  handle->fd = PORT_DELETED;
  uv__free(handle->path);
  handle->path = NULL;
  handle->fo.fo_name = NULL;
  uv__handle_stop(handle);

  return 0;
}

void uv__fs_event_close(uv_fs_event_t* handle) {
  uv_fs_event_stop(handle);
}

#else /* !defined(PORT_SOURCE_FILE) */

int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  return UV_ENOSYS;
}


int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* filename,
                      unsigned int flags) {
  return UV_ENOSYS;
}


int uv_fs_event_stop(uv_fs_event_t* handle) {
  return UV_ENOSYS;
}


void uv__fs_event_close(uv_fs_event_t* handle) {
  UNREACHABLE();
}

#endif /* defined(PORT_SOURCE_FILE) */


int uv_resident_set_memory(size_t* rss) {
  psinfo_t psinfo;
  int err;
  int fd;

  fd = open("/proc/self/psinfo", O_RDONLY);
  if (fd == -1)
    return UV__ERR(errno);

  /* FIXME(bnoordhuis) Handle EINTR. */
  err = UV_EINVAL;
  if (read(fd, &psinfo, sizeof(psinfo)) == sizeof(psinfo)) {
    *rss = (size_t)psinfo.pr_rssize * 1024;
    err = 0;
  }
  uv__close(fd);

  return err;
}


int uv_uptime(double* uptime) {
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;

  long hz = sysconf(_SC_CLK_TCK);

  kc = kstat_open();
  if (kc == NULL)
    return UV_EPERM;

  ksp = kstat_lookup(kc, (char*) "unix", 0, (char*) "system_misc");
  if (kstat_read(kc, ksp, NULL) == -1) {
    *uptime = -1;
  } else {
    knp = (kstat_named_t*)  kstat_data_lookup(ksp, (char*) "clk_intr");
    *uptime = knp->value.ul / hz;
  }
  kstat_close(kc);

  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  int           lookup_instance;
  kstat_ctl_t   *kc;
  kstat_t       *ksp;
  kstat_named_t *knp;
  uv_cpu_info_t* cpu_info;

  kc = kstat_open();
  if (kc == NULL)
    return UV_EPERM;

  /* Get count of cpus */
  lookup_instance = 0;
  while ((ksp = kstat_lookup(kc, (char*) "cpu_info", lookup_instance, NULL))) {
    lookup_instance++;
  }

  *cpu_infos = uv__malloc(lookup_instance * sizeof(**cpu_infos));
  if (!(*cpu_infos)) {
    kstat_close(kc);
    return UV_ENOMEM;
  }

  *count = lookup_instance;

  cpu_info = *cpu_infos;
  lookup_instance = 0;
  while ((ksp = kstat_lookup(kc, (char*) "cpu_info", lookup_instance, NULL))) {
    if (kstat_read(kc, ksp, NULL) == -1) {
      cpu_info->speed = 0;
      cpu_info->model = NULL;
    } else {
      knp = kstat_data_lookup(ksp, (char*) "clock_MHz");
      assert(knp->data_type == KSTAT_DATA_INT32 ||
             knp->data_type == KSTAT_DATA_INT64);
      cpu_info->speed = (knp->data_type == KSTAT_DATA_INT32) ? knp->value.i32
                                                             : knp->value.i64;

      knp = kstat_data_lookup(ksp, (char*) "brand");
      assert(knp->data_type == KSTAT_DATA_STRING);
      cpu_info->model = uv__strdup(KSTAT_NAMED_STR_PTR(knp));
    }

    lookup_instance++;
    cpu_info++;
  }

  cpu_info = *cpu_infos;
  lookup_instance = 0;
  for (;;) {
    ksp = kstat_lookup(kc, (char*) "cpu", lookup_instance, (char*) "sys");

    if (ksp == NULL)
      break;

    if (kstat_read(kc, ksp, NULL) == -1) {
      cpu_info->cpu_times.user = 0;
      cpu_info->cpu_times.nice = 0;
      cpu_info->cpu_times.sys = 0;
      cpu_info->cpu_times.idle = 0;
      cpu_info->cpu_times.irq = 0;
    } else {
      knp = kstat_data_lookup(ksp, (char*) "cpu_ticks_user");
      assert(knp->data_type == KSTAT_DATA_UINT64);
      cpu_info->cpu_times.user = knp->value.ui64;

      knp = kstat_data_lookup(ksp, (char*) "cpu_ticks_kernel");
      assert(knp->data_type == KSTAT_DATA_UINT64);
      cpu_info->cpu_times.sys = knp->value.ui64;

      knp = kstat_data_lookup(ksp, (char*) "cpu_ticks_idle");
      assert(knp->data_type == KSTAT_DATA_UINT64);
      cpu_info->cpu_times.idle = knp->value.ui64;

      knp = kstat_data_lookup(ksp, (char*) "intr");
      assert(knp->data_type == KSTAT_DATA_UINT64);
      cpu_info->cpu_times.irq = knp->value.ui64;
      cpu_info->cpu_times.nice = 0;
    }

    lookup_instance++;
    cpu_info++;
  }

  kstat_close(kc);

  return 0;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(cpu_infos[i].model);
  }

  uv__free(cpu_infos);
}

#ifdef SUNOS_NO_IFADDRS
int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  return UV_ENOSYS;
}
#else  /* SUNOS_NO_IFADDRS */
/*
 * Inspired By:
 * https://blogs.oracle.com/paulie/entry/retrieving_mac_address_in_solaris
 * http://www.pauliesworld.org/project/getmac.c
 */
static int uv__set_phys_addr(uv_interface_address_t* address,
                             struct ifaddrs* ent) {

  struct sockaddr_dl* sa_addr;
  int sockfd;
  int i;
  struct arpreq arpreq;

  /* This appears to only work as root */
  sa_addr = (struct sockaddr_dl*)(ent->ifa_addr);
  memcpy(address->phys_addr, LLADDR(sa_addr), sizeof(address->phys_addr));
  for (i = 0; i < sizeof(address->phys_addr); i++) {
    if (address->phys_addr[i] != 0)
      return 0;
  }
  memset(&arpreq, 0, sizeof(arpreq));
  if (address->address.address4.sin_family == AF_INET) {
    struct sockaddr_in* sin = ((struct sockaddr_in*)&arpreq.arp_pa);
    sin->sin_addr.s_addr = address->address.address4.sin_addr.s_addr;
  } else if (address->address.address4.sin_family == AF_INET6) {
    struct sockaddr_in6* sin = ((struct sockaddr_in6*)&arpreq.arp_pa);
    memcpy(sin->sin6_addr.s6_addr,
           address->address.address6.sin6_addr.s6_addr,
           sizeof(address->address.address6.sin6_addr.s6_addr));
  } else {
    return 0;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    return UV__ERR(errno);

  if (ioctl(sockfd, SIOCGARP, (char*)&arpreq) == -1) {
    uv__close(sockfd);
    return UV__ERR(errno);
  }
  memcpy(address->phys_addr, arpreq.arp_ha.sa_data, sizeof(address->phys_addr));
  uv__close(sockfd);
  return 0;
}


static int uv__ifaddr_exclude(struct ifaddrs *ent) {
  if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)))
    return 1;
  if (ent->ifa_addr == NULL)
    return 1;
  if (ent->ifa_addr->sa_family != AF_INET &&
      ent->ifa_addr->sa_family != AF_INET6)
    return 1;
  return 0;
}

int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  uv_interface_address_t* address;
  struct ifaddrs* addrs;
  struct ifaddrs* ent;

  if (getifaddrs(&addrs))
    return UV__ERR(errno);

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent))
      continue;
    (*count)++;
  }

  *addresses = uv__malloc(*count * sizeof(**addresses));
  if (!(*addresses)) {
    freeifaddrs(addrs);
    return UV_ENOMEM;
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent))
      continue;

    address->name = uv__strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6*) ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) ent->ifa_addr);
    }

    if (ent->ifa_netmask->sa_family == AF_INET6) {
      address->netmask.netmask6 = *((struct sockaddr_in6*) ent->ifa_netmask);
    } else {
      address->netmask.netmask4 = *((struct sockaddr_in*) ent->ifa_netmask);
    }

    address->is_internal = !!((ent->ifa_flags & IFF_PRIVATE) ||
                           (ent->ifa_flags & IFF_LOOPBACK));

    uv__set_phys_addr(address, ent);
    address++;
  }

  freeifaddrs(addrs);

  return 0;
}
#endif  /* SUNOS_NO_IFADDRS */

void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}
