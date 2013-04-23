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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <net/if.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define HAVE_IFADDRS_H 1
#ifdef __UCLIBC__
# if __UCLIBC_MAJOR__ < 0 || __UCLIBC_MINOR__ < 9 || __UCLIBC_SUBLEVEL__ < 32
#  undef HAVE_IFADDRS_H
# endif
#endif
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif

#undef NANOSEC
#define NANOSEC ((uint64_t) 1e9)

/* This is rather annoying: CLOCK_BOOTTIME lives in <linux/time.h> but we can't
 * include that file because it conflicts with <time.h>. We'll just have to
 * define it ourselves.
 */
#ifndef CLOCK_BOOTTIME
# define CLOCK_BOOTTIME 7
#endif

static int read_models(unsigned int numcpus, uv_cpu_info_t* ci);
static int read_times(unsigned int numcpus, uv_cpu_info_t* ci);
static void read_speeds(unsigned int numcpus, uv_cpu_info_t* ci);
static unsigned long read_cpufreq(unsigned int cpunum);


int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  int fd;

  fd = uv__epoll_create1(UV__EPOLL_CLOEXEC);

  /* epoll_create1() can fail either because it's not implemented (old kernel)
   * or because it doesn't understand the EPOLL_CLOEXEC flag.
   */
  if (fd == -1 && (errno == ENOSYS || errno == EINVAL)) {
    fd = uv__epoll_create(256);

    if (fd != -1)
      uv__cloexec(fd, 1);
  }

  loop->backend_fd = fd;
  loop->inotify_fd = -1;
  loop->inotify_watchers = NULL;

  if (fd == -1)
    return -1;

  return 0;
}


void uv__platform_loop_delete(uv_loop_t* loop) {
  if (loop->inotify_fd == -1) return;
  uv__io_stop(loop, &loop->inotify_read_watcher, UV__POLLIN);
  close(loop->inotify_fd);
  loop->inotify_fd = -1;
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  struct uv__epoll_event events[1024];
  struct uv__epoll_event* pe;
  struct uv__epoll_event e;
  ngx_queue_t* q;
  uv__io_t* w;
  uint64_t base;
  uint64_t diff;
  int nevents;
  int count;
  int nfds;
  int fd;
  int op;
  int i;

  if (loop->nfds == 0) {
    assert(ngx_queue_empty(&loop->watcher_queue));
    return;
  }

  while (!ngx_queue_empty(&loop->watcher_queue)) {
    q = ngx_queue_head(&loop->watcher_queue);
    ngx_queue_remove(q);
    ngx_queue_init(q);

    w = ngx_queue_data(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    e.events = w->pevents;
    e.data = w->fd;

    if (w->events == 0)
      op = UV__EPOLL_CTL_ADD;
    else
      op = UV__EPOLL_CTL_MOD;

    /* XXX Future optimization: do EPOLL_CTL_MOD lazily if we stop watching
     * events, skip the syscall and squelch the events after epoll_wait().
     */
    if (uv__epoll_ctl(loop->backend_fd, op, w->fd, &e)) {
      if (errno != EEXIST)
        abort();

      assert(op == UV__EPOLL_CTL_ADD);

      /* We've reactivated a file descriptor that's been watched before. */
      if (uv__epoll_ctl(loop->backend_fd, UV__EPOLL_CTL_MOD, w->fd, &e))
        abort();
    }

    w->events = w->pevents;
  }

  assert(timeout >= -1);
  base = loop->time;
  count = 48; /* Benchmarks suggest this gives the best throughput. */

  for (;;) {
    nfds = uv__epoll_wait(loop->backend_fd,
                          events,
                          ARRAY_SIZE(events),
                          timeout);

    /* Update loop->time unconditionally. It's tempting to skip the update when
     * timeout == 0 (i.e. non-blocking poll) but there is no guarantee that the
     * operating system didn't reschedule our process while in the syscall.
     */
    SAVE_ERRNO(uv__update_time(loop));

    if (nfds == 0) {
      assert(timeout != -1);
      return;
    }

    if (nfds == -1) {
      if (errno != EINTR)
        abort();

      if (timeout == -1)
        continue;

      if (timeout == 0)
        return;

      /* Interrupted by a signal. Update timeout and poll again. */
      goto update_timeout;
    }

    nevents = 0;

    for (i = 0; i < nfds; i++) {
      pe = events + i;
      fd = pe->data;

      assert(fd >= 0);
      assert((unsigned) fd < loop->nwatchers);

      w = loop->watchers[fd];

      if (w == NULL) {
        /* File descriptor that we've stopped watching, disarm it.
         *
         * Ignore all errors because we may be racing with another thread
         * when the file descriptor is closed.
         */
        uv__epoll_ctl(loop->backend_fd, UV__EPOLL_CTL_DEL, fd, pe);
        continue;
      }

      w->cb(loop, w, pe->events);
      nevents++;
    }

    if (nevents != 0) {
      if (nfds == ARRAY_SIZE(events) && --count != 0) {
        /* Poll for more events but don't block this time. */
        timeout = 0;
        continue;
      }
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


uint64_t uv__hrtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((uint64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
}


void uv_loadavg(double avg[3]) {
  struct sysinfo info;

  if (sysinfo(&info) < 0) return;

  avg[0] = (double) info.loads[0] / 65536.0;
  avg[1] = (double) info.loads[1] / 65536.0;
  avg[2] = (double) info.loads[2] / 65536.0;
}


int uv_exepath(char* buffer, size_t* size) {
  ssize_t n;

  if (!buffer || !size) {
    return -1;
  }

  n = readlink("/proc/self/exe", buffer, *size - 1);
  if (n <= 0) return -1;
  buffer[n] = '\0';
  *size = n;

  return 0;
}


uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}


uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}


uv_err_t uv_resident_set_memory(size_t* rss) {
  char buf[1024];
  const char* s;
  ssize_t n;
  long val;
  int fd;
  int i;

  do
    fd = open("/proc/self/stat", O_RDONLY);
  while (fd == -1 && errno == EINTR);

  if (fd == -1)
    return uv__new_sys_error(errno);

  do
    n = read(fd, buf, sizeof(buf) - 1);
  while (n == -1 && errno == EINTR);

  SAVE_ERRNO(close(fd));
  if (n == -1)
    return uv__new_sys_error(errno);
  buf[n] = '\0';

  s = strchr(buf, ' ');
  if (s == NULL)
    goto err;

  s += 1;
  if (*s != '(')
    goto err;

  s = strchr(s, ')');
  if (s == NULL)
    goto err;

  for (i = 1; i <= 22; i++) {
    s = strchr(s + 1, ' ');
    if (s == NULL)
      goto err;
  }

  errno = 0;
  val = strtol(s, NULL, 10);
  if (errno != 0)
    goto err;
  if (val < 0)
    goto err;

  *rss = val * getpagesize();
  return uv_ok_;

err:
  return uv__new_artificial_error(UV_EINVAL);
}


uv_err_t uv_uptime(double* uptime) {
  static volatile int no_clock_boottime;
  struct timespec now;
  int r;

  /* Try CLOCK_BOOTTIME first, fall back to CLOCK_MONOTONIC if not available
   * (pre-2.6.39 kernels). CLOCK_MONOTONIC doesn't increase when the system
   * is suspended.
   */
  if (no_clock_boottime) {
    retry: r = clock_gettime(CLOCK_MONOTONIC, &now);
  }
  else if ((r = clock_gettime(CLOCK_BOOTTIME, &now)) && errno == EINVAL) {
    no_clock_boottime = 1;
    goto retry;
  }

  if (r)
    return uv__new_sys_error(errno);

  *uptime = now.tv_sec;
  *uptime += (double)now.tv_nsec / 1000000000.0;
  return uv_ok_;
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int numcpus;
  uv_cpu_info_t* ci;

  *cpu_infos = NULL;
  *count = 0;

  numcpus = sysconf(_SC_NPROCESSORS_ONLN);
  assert(numcpus != (unsigned int) -1);
  assert(numcpus != 0);

  ci = calloc(numcpus, sizeof(*ci));
  if (ci == NULL)
    return uv__new_sys_error(ENOMEM);

  if (read_models(numcpus, ci)) {
    SAVE_ERRNO(free(ci));
    return uv__new_sys_error(errno);
  }

  if (read_times(numcpus, ci)) {
    SAVE_ERRNO(free(ci));
    return uv__new_sys_error(errno);
  }

  /* read_models() on x86 also reads the CPU speed from /proc/cpuinfo.
   * We don't check for errors here. Worst case, the field is left zero.
   */
  if (ci[0].speed == 0)
    read_speeds(numcpus, ci);

  *cpu_infos = ci;
  *count = numcpus;

  return uv_ok_;
}


static void read_speeds(unsigned int numcpus, uv_cpu_info_t* ci) {
  unsigned int num;

  for (num = 0; num < numcpus; num++)
    ci[num].speed = read_cpufreq(num) / 1000;
}


/* Also reads the CPU frequency on x86. The other architectures only have
 * a BogoMIPS field, which may not be very accurate.
 */
static int read_models(unsigned int numcpus, uv_cpu_info_t* ci) {
#if defined(__i386__) || defined(__x86_64__)
  static const char model_marker[] = "model name\t: ";
  static const char speed_marker[] = "cpu MHz\t\t: ";
#elif defined(__arm__)
  static const char model_marker[] = "Processor\t: ";
  static const char speed_marker[] = "";
#elif defined(__mips__)
  static const char model_marker[] = "cpu model\t\t: ";
  static const char speed_marker[] = "";
#else
# warning uv_cpu_info() is not supported on this architecture.
  static const char model_marker[] = "";
  static const char speed_marker[] = "";
#endif
  static const char bogus_model[] = "unknown";
  unsigned int model_idx;
  unsigned int speed_idx;
  char buf[1024];
  char* model;
  FILE* fp;
  char* inferred_model;

  fp = fopen("/proc/cpuinfo", "r");
  if (fp == NULL)
    return -1;

  model_idx = 0;
  speed_idx = 0;

  while (fgets(buf, sizeof(buf), fp)) {
    if (model_marker[0] != '\0' &&
        model_idx < numcpus &&
        strncmp(buf, model_marker, sizeof(model_marker) - 1) == 0)
    {
      model = buf + sizeof(model_marker) - 1;
      model = strndup(model, strlen(model) - 1); /* strip newline */
      ci[model_idx++].model = model;
      continue;
    }

    if (speed_marker[0] != '\0' &&
        speed_idx < numcpus &&
        strncmp(buf, speed_marker, sizeof(speed_marker) - 1) == 0)
    {
      ci[speed_idx++].speed = atoi(buf + sizeof(speed_marker) - 1);
      continue;
    }
  }
  fclose(fp);

  /* Now we want to make sure that all the models contain *something*:
   * it's not safe to leave them as null.
   */
  if (model_idx == 0) {
    /* No models at all: fake up the first one. */
    ci[0].model = strndup(bogus_model, sizeof(bogus_model) - 1);
    model_idx = 1;
  }

  /* Not enough models, but we do have at least one.  So we'll just
   * copy the rest down: it might be better to indicate somehow that
   * the remaining ones have been guessed.
   */
  inferred_model = ci[model_idx - 1].model;

  while (model_idx < numcpus) {
    ci[model_idx].model = strndup(inferred_model, strlen(inferred_model));
    model_idx++;
  }

  return 0;
}


static int read_times(unsigned int numcpus, uv_cpu_info_t* ci) {
  unsigned long clock_ticks;
  struct uv_cpu_times_s ts;
  unsigned long user;
  unsigned long nice;
  unsigned long sys;
  unsigned long idle;
  unsigned long dummy;
  unsigned long irq;
  unsigned int num;
  unsigned int len;
  char buf[1024];
  FILE* fp;

  clock_ticks = sysconf(_SC_CLK_TCK);
  assert(clock_ticks != (unsigned long) -1);
  assert(clock_ticks != 0);

  fp = fopen("/proc/stat", "r");
  if (fp == NULL)
    return -1;

  if (!fgets(buf, sizeof(buf), fp))
    abort();

  num = 0;

  while (fgets(buf, sizeof(buf), fp)) {
    if (num >= numcpus)
      break;

    if (strncmp(buf, "cpu", 3))
      break;

    /* skip "cpu<num> " marker */
    {
      unsigned int n = num;
      for (len = sizeof("cpu0"); n /= 10; len++);
      assert(sscanf(buf, "cpu%u ", &n) == 1 && n == num);
    }

    /* Line contains user, nice, system, idle, iowait, irq, softirq, steal,
     * guest, guest_nice but we're only interested in the first four + irq.
     *
     * Don't use %*s to skip fields or %ll to read straight into the uint64_t
     * fields, they're not allowed in C89 mode.
     */
    if (6 != sscanf(buf + len,
                    "%lu %lu %lu %lu %lu %lu",
                    &user,
                    &nice,
                    &sys,
                    &idle,
                    &dummy,
                    &irq))
      abort();

    ts.user = clock_ticks * user;
    ts.nice = clock_ticks * nice;
    ts.sys  = clock_ticks * sys;
    ts.idle = clock_ticks * idle;
    ts.irq  = clock_ticks * irq;
    ci[num++].cpu_times = ts;
  }
  fclose(fp);

  return 0;
}


static unsigned long read_cpufreq(unsigned int cpunum) {
  unsigned long val;
  char buf[1024];
  FILE* fp;

  snprintf(buf,
           sizeof(buf),
           "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq",
           cpunum);

  fp = fopen(buf, "r");
  if (fp == NULL)
    return 0;

  if (fscanf(fp, "%lu", &val) != 1)
    val = 0;

  fclose(fp);

  return val;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
#ifndef HAVE_IFADDRS_H
  return uv__new_artificial_error(UV_ENOSYS);
#else
  struct ifaddrs *addrs, *ent;
  char ip[INET6_ADDRSTRLEN];
  uv_interface_address_t* address;

  if (getifaddrs(&addrs) != 0) {
    return uv__new_sys_error(errno);
  }

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING) ||
        (ent->ifa_addr == NULL) ||
        (ent->ifa_addr->sa_family == PF_PACKET)) {
      continue;
    }

    (*count)++;
  }

  *addresses = (uv_interface_address_t*)
    malloc(*count * sizeof(uv_interface_address_t));
  if (!(*addresses)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    bzero(&ip, sizeof (ip));
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING)) {
      continue;
    }

    if (ent->ifa_addr == NULL) {
      continue;
    }

    /*
     * On Linux getifaddrs returns information related to the raw underlying
     * devices. We're not interested in this information.
     */
    if (ent->ifa_addr->sa_family == PF_PACKET) {
      continue;
    }

    address->name = strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6 *)ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in *)ent->ifa_addr);
    }

    address->is_internal = ent->ifa_flags & IFF_LOOPBACK ? 1 : 0;

    address++;
  }

  freeifaddrs(addrs);

  return uv_ok_;
#endif
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}


void uv__set_process_title(const char* title) {
#if defined(PR_SET_NAME)
  prctl(PR_SET_NAME, title);  /* Only copies first 16 characters. */
#endif
}
