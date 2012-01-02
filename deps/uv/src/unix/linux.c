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

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#undef NANOSEC
#define NANOSEC 1000000000

#undef HAVE_INOTIFY_INIT
#undef HAVE_INOTIFY_INIT1
#undef HAVE_INOTIFY_ADD_WATCH
#undef HAVE_INOTIFY_RM_WATCH

#if __NR_inotify_init
# define HAVE_INOTIFY_INIT 1
#endif
#if __NR_inotify_init1
# define HAVE_INOTIFY_INIT1 1
#endif
#if __NR_inotify_add_watch
# define HAVE_INOTIFY_ADD_WATCH 1
#endif
#if __NR_inotify_rm_watch
# define HAVE_INOTIFY_RM_WATCH 1
#endif

#if HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1
# undef IN_ACCESS
# undef IN_MODIFY
# undef IN_ATTRIB
# undef IN_CLOSE_WRITE
# undef IN_CLOSE_NOWRITE
# undef IN_OPEN
# undef IN_MOVED_FROM
# undef IN_MOVED_TO
# undef IN_CREATE
# undef IN_DELETE
# undef IN_DELETE_SELF
# undef IN_MOVE_SELF
# define IN_ACCESS         0x001
# define IN_MODIFY         0x002
# define IN_ATTRIB         0x004
# define IN_CLOSE_WRITE    0x008
# define IN_CLOSE_NOWRITE  0x010
# define IN_OPEN           0x020
# define IN_MOVED_FROM     0x040
# define IN_MOVED_TO       0x080
# define IN_CREATE         0x100
# define IN_DELETE         0x200
# define IN_DELETE_SELF    0x400
# define IN_MOVE_SELF      0x800
struct inotify_event {
  int32_t wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  /* char name[0]; */
};
#endif /* HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */

#undef IN_CLOEXEC
#undef IN_NONBLOCK

#if HAVE_INOTIFY_INIT1
# define IN_CLOEXEC O_CLOEXEC
# define IN_NONBLOCK O_NONBLOCK
#endif /* HAVE_INOTIFY_INIT1 */

#if HAVE_INOTIFY_INIT
inline static int inotify_init(void) {
  return syscall(__NR_inotify_init);
}
#endif /* HAVE_INOTIFY_INIT */

#if HAVE_INOTIFY_INIT1
inline static int inotify_init1(int flags) {
  return syscall(__NR_inotify_init1, flags);
}
#endif /* HAVE_INOTIFY_INIT1 */

#if HAVE_INOTIFY_ADD_WATCH
inline static int inotify_add_watch(int fd, const char* path, uint32_t mask) {
  return syscall(__NR_inotify_add_watch, fd, path, mask);
}
#endif /* HAVE_INOTIFY_ADD_WATCH */

#if HAVE_INOTIFY_RM_WATCH
inline static int inotify_rm_watch(int fd, uint32_t wd) {
  return syscall(__NR_inotify_rm_watch, fd, wd);
}
#endif /* HAVE_INOTIFY_RM_WATCH */


static char buf[MAXPATHLEN + 1];

static struct {
  char *str;
  size_t len;
} process_title;


/* Don't look aghast, this is exactly how glibc's basename() works. */
static char* basename_r(const char* path) {
  char* s = strrchr(path, '/');
  return s ? (s + 1) : (char*)path;
}


/*
 * There's probably some way to get time from Linux than gettimeofday(). What
 * it is, I don't know.
 */
uint64_t uv_hrtime() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * NANOSEC + ts.tv_nsec);
}


void uv_loadavg(double avg[3]) {
  struct sysinfo info;

  if (sysinfo(&info) < 0) return;

  avg[0] = (double) info.loads[0] / 65536.0;
  avg[1] = (double) info.loads[1] / 65536.0;
  avg[2] = (double) info.loads[2] / 65536.0;
}


int uv_exepath(char* buffer, size_t* size) {
  if (!buffer || !size) {
    return -1;
  }

  *size = readlink("/proc/self/exe", buffer, *size - 1);
  if (*size <= 0) return -1;
  buffer[*size] = '\0';
  return 0;
}


uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}


uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}


char** uv_setup_args(int argc, char** argv) {
  char **new_argv;
  char **new_env;
  size_t size;
  int envc;
  char *s;
  int i;

  for (envc = 0; environ[envc]; envc++);

  s = envc ? environ[envc - 1] : argv[argc - 1];

  process_title.str = argv[0];
  process_title.len = s + strlen(s) + 1 - argv[0];

  size = process_title.len;
  size += (argc + 1) * sizeof(char **);
  size += (envc + 1) * sizeof(char **);

  if ((s = (char *) malloc(size)) == NULL) {
    process_title.str = NULL;
    process_title.len = 0;
    return argv;
  }

  new_argv = (char **) s;
  new_env = new_argv + argc + 1;
  s = (char *) (new_env + envc + 1);
  memcpy(s, process_title.str, process_title.len);

  for (i = 0; i < argc; i++)
    new_argv[i] = s + (argv[i] - argv[0]);
  new_argv[argc] = NULL;

  s += environ[0] - argv[0];

  for (i = 0; i < envc; i++)
    new_env[i] = s + (environ[i] - environ[0]);
  new_env[envc] = NULL;

  environ = new_env;
  return new_argv;
}


uv_err_t uv_set_process_title(const char* title) {
  /* No need to terminate, last char is always '\0'. */
  if (process_title.len)
    strncpy(process_title.str, title, process_title.len - 1);

  return uv_ok_;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  if (process_title.str) {
    strncpy(buffer, process_title.str, size);
  } else {
    if (size > 0) {
      buffer[0] = '\0';
    }
  }

  return uv_ok_;
}


uv_err_t uv_resident_set_memory(size_t* rss) {
  FILE* f;
  int itmp;
  char ctmp;
  unsigned int utmp;
  size_t page_size = getpagesize();
  char *cbuf;
  int foundExeEnd;

  f = fopen("/proc/self/stat", "r");
  if (!f) return uv__new_sys_error(errno);

  /* PID */
  if (fscanf(f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* Exec file */
  cbuf = buf;
  foundExeEnd = 0;
  if (fscanf (f, "%c", cbuf++) == 0) goto error;
  while (1) {
    if (fscanf(f, "%c", cbuf) == 0) goto error;
    if (*cbuf == ')') {
      foundExeEnd = 1;
    } else if (foundExeEnd && *cbuf == ' ') {
      *cbuf = 0;
      break;
    }

    cbuf++;
  }
  /* State */
  if (fscanf (f, "%c ", &ctmp) == 0) goto error; /* coverity[secure_coding] */
  /* Parent process */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* Process group */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* Session id */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* TTY */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* TTY owner process group */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* Flags */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Minor faults (no memory page) */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Minor faults, children */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Major faults (memory page faults) */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Major faults, children */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* utime */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* stime */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* utime, children */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* stime, children */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* jiffies remaining in current time slice */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* 'nice' value */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */
  /* jiffies until next timeout */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* jiffies until next SIGALRM */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* start time (jiffies since system boot) */
  if (fscanf (f, "%d ", &itmp) == 0) goto error; /* coverity[secure_coding] */

  /* Virtual memory size */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */

  /* Resident set size */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  *rss = (size_t) utmp * page_size;

  /* rlim */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Start of text */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* End of text */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */
  /* Start of stack */
  if (fscanf (f, "%u ", &utmp) == 0) goto error; /* coverity[secure_coding] */

  fclose (f);
  return uv_ok_;

error:
  fclose (f);
  return uv__new_sys_error(errno);
}


uv_err_t uv_uptime(double* uptime) {
#ifdef CLOCK_MONOTONIC
  struct timespec now;
  if (0 == clock_gettime(CLOCK_MONOTONIC, &now)) {
    *uptime = now.tv_sec;
    *uptime += (double)now.tv_nsec / 1000000000.0;
    return uv_ok_;
  }
  return uv__new_sys_error(errno);
#else
  struct sysinfo info;
  if (sysinfo(&info) < 0) {
    return uv__new_sys_error(errno);
  }
  *uptime = (double)info.uptime;
  return uv_ok_;
#endif
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed;
  int numcpus = 0, i = 0;
  unsigned long ticks_user, ticks_sys, ticks_idle, ticks_nice, ticks_intr;
  char line[512], speedPath[256], model[512];
  FILE *fpStat = fopen("/proc/stat", "r");
  FILE *fpModel = fopen("/proc/cpuinfo", "r");
  FILE *fpSpeed;
  uv_cpu_info_t* cpu_info;

  if (fpModel) {
    while (fgets(line, 511, fpModel) != NULL) {
      if (strncmp(line, "model name", 10) == 0) {
        numcpus++;
        if (numcpus == 1) {
          char *p = strchr(line, ':') + 2;
          strcpy(model, p);
          model[strlen(model)-1] = 0;
        }
      } else if (strncmp(line, "cpu MHz", 7) == 0) {
        if (numcpus == 1) {
          sscanf(line, "%*s %*s : %u", &cpuspeed);
        }
      }
    }
    fclose(fpModel);
  }

  *cpu_infos = (uv_cpu_info_t*)malloc(numcpus * sizeof(uv_cpu_info_t));
  if (!(*cpu_infos)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  *count = numcpus;

  cpu_info = *cpu_infos;

  if (fpStat) {
    while (fgets(line, 511, fpStat) != NULL) {
      if (strncmp(line, "cpu ", 4) == 0) {
        continue;
      } else if (strncmp(line, "cpu", 3) != 0) {
        break;
      }

      sscanf(line, "%*s %lu %lu %lu %lu %*s %lu",
             &ticks_user, &ticks_nice, &ticks_sys, &ticks_idle, &ticks_intr);
      snprintf(speedPath, sizeof(speedPath),
               "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", i);

      fpSpeed = fopen(speedPath, "r");

      if (fpSpeed) {
        if (fgets(line, 511, fpSpeed) != NULL) {
          sscanf(line, "%u", &cpuspeed);
          cpuspeed /= 1000;
        }
        fclose(fpSpeed);
      }

      cpu_info->cpu_times.user = ticks_user * multiplier;
      cpu_info->cpu_times.nice = ticks_nice * multiplier;
      cpu_info->cpu_times.sys = ticks_sys * multiplier;
      cpu_info->cpu_times.idle = ticks_idle * multiplier;
      cpu_info->cpu_times.irq = ticks_intr * multiplier;

      cpu_info->model = strdup(model);
      cpu_info->speed = cpuspeed;

      cpu_info++;
    }
    fclose(fpStat);
  }

  return uv_ok_;
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
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}

#if HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1

static int new_inotify_fd(void) {
#if HAVE_INOTIFY_INIT1
  return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#else
  int fd;

  if ((fd = inotify_init()) == -1)
    return -1;

  if (uv__cloexec(fd, 1) || uv__nonblock(fd, 1)) {
    SAVE_ERRNO(uv__close(fd));
    fd = -1;
  }

  return fd;
#endif
}


static void uv__inotify_read(EV_P_ ev_io* w, int revents) {
  struct inotify_event* e;
  uv_fs_event_t* handle;
  const char* filename;
  ssize_t size;
  int events;
  char *p;
  /* needs to be large enough for sizeof(inotify_event) + strlen(filename) */
  char buf[4096];

  handle = container_of(w, uv_fs_event_t, read_watcher);

  do {
    do {
      size = read(handle->fd, buf, sizeof buf);
    }
    while (size == -1 && errno == EINTR);

    if (size == -1) {
      assert(errno == EAGAIN || errno == EWOULDBLOCK);
      break;
    }

    assert(size > 0); /* pre-2.6.21 thing, size=0 == read buffer too small */

    /* Now we have one or more inotify_event structs. */
    for (p = buf; p < buf + size; p += sizeof(*e) + e->len) {
      e = (void*)p;

      events = 0;
      if (e->mask & (IN_ATTRIB|IN_MODIFY))
        events |= UV_CHANGE;
      if (e->mask & ~(IN_ATTRIB|IN_MODIFY))
        events |= UV_RENAME;

      /* inotify does not return the filename when monitoring a single file
       * for modifications. Repurpose the filename for API compatibility.
       * I'm not convinced this is a good thing, maybe it should go.
       */
      filename = e->len ? (const char*) (e + 1) : basename_r(handle->filename);

      handle->cb(handle, filename, events, 0);

      if (handle->fd == -1)
        break;
    }
  }
  while (handle->fd != -1); /* handle might've been closed by callback */
}


int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  int events;
  int fd;

  loop->counters.fs_event_init++;

  /* We don't support any flags yet. */
  assert(!flags);

  /*
   * TODO share a single inotify fd across the event loop?
   * We'll run into fs.inotify.max_user_instances if we
   * keep creating new inotify fds.
   */
  if ((fd = new_inotify_fd()) == -1) {
    uv__set_sys_error(loop, errno);
    return -1;
  }

  events = IN_ATTRIB
         | IN_CREATE
         | IN_MODIFY
         | IN_DELETE
         | IN_DELETE_SELF
         | IN_MOVED_FROM
         | IN_MOVED_TO;

  if (inotify_add_watch(fd, filename, events) == -1) {
    uv__set_sys_error(loop, errno);
    uv__close(fd);
    return -1;
  }

  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  handle->filename = strdup(filename); /* this should go! */
  handle->cb = cb;
  handle->fd = fd;

  ev_io_init(&handle->read_watcher, uv__inotify_read, fd, EV_READ);
  ev_io_start(loop->ev, &handle->read_watcher);
  ev_unref(loop->ev);

  return 0;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  ev_ref(handle->loop->ev);
  ev_io_stop(handle->loop->ev, &handle->read_watcher);
  uv__close(handle->fd);
  handle->fd = -1;
  free(handle->filename);
  handle->filename = NULL;
}

#else /* !HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */

int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  loop->counters.fs_event_init++;
  uv__set_sys_error(loop, ENOSYS);
  return -1;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  assert(0 && "unreachable");
  abort();
}

#endif /* HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */
