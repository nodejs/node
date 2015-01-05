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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <libgen.h>

#include <sys/protosw.h>
#include <libperfstat.h>
#include <sys/proc.h>
#include <sys/procfs.h>

#include <sys/poll.h>

#include <sys/pollset.h>
#include <ctype.h>
#include <sys/ahafs_evProds.h>

#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <limits.h>
#include <strings.h>
#include <sys/vnode.h>

#define RDWR_BUF_SIZE   4096
#define EQ(a,b)         (strcmp(a,b) == 0)

int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  loop->fs_fd = -1;

  /* Passing maxfd of -1 should mean the limit is determined
   * by the user's ulimit or the global limit as per the doc */
  loop->backend_fd = pollset_create(-1);

  if (loop->backend_fd == -1)
    return -1;

  return 0;
}


void uv__platform_loop_delete(uv_loop_t* loop) {
  if (loop->fs_fd != -1) {
    uv__close(loop->fs_fd);
    loop->fs_fd = -1;
  }

  if (loop->backend_fd != -1) {
    pollset_destroy(loop->backend_fd);
    loop->backend_fd = -1;
  }
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  struct pollfd events[1024];
  struct pollfd pqry;
  struct pollfd* pe;
  struct poll_ctl pc;
  QUEUE* q;
  uv__io_t* w;
  uint64_t base;
  uint64_t diff;
  int nevents;
  int count;
  int nfds;
  int i;
  int rc;
  int add_failed;

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
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    pc.events = w->pevents;
    pc.fd = w->fd;

    add_failed = 0;
    if (w->events == 0) {
      pc.cmd = PS_ADD;
      if (pollset_ctl(loop->backend_fd, &pc, 1)) {
        if (errno != EINVAL) {
          assert(0 && "Failed to add file descriptor (pc.fd) to pollset");
          abort();
        }
        /* Check if the fd is already in the pollset */
        pqry.fd = pc.fd;
        rc = pollset_query(loop->backend_fd, &pqry);
        switch (rc) {
        case -1: 
          assert(0 && "Failed to query pollset for file descriptor");
          abort();
        case 0:
          assert(0 && "Pollset does not contain file descriptor");
          abort();
        }
        /* If we got here then the pollset already contained the file descriptor even though
         * we didn't think it should. This probably shouldn't happen, but we can continue. */
        add_failed = 1;
      }
    }
    if (w->events != 0 || add_failed) {
      /* Modify, potentially removing events -- need to delete then add.
       * Could maybe mod if we knew for sure no events are removed, but
       * content of w->events is handled above as not reliable (falls back)
       * so may require a pollset_query() which would have to be pretty cheap
       * compared to a PS_DELETE to be worth optimizing. Alternatively, could
       * lazily remove events, squelching them in the mean time. */
      pc.cmd = PS_DELETE;
      if (pollset_ctl(loop->backend_fd, &pc, 1)) {
        assert(0 && "Failed to delete file descriptor (pc.fd) from pollset");
        abort();
      }
      pc.cmd = PS_ADD;
      if (pollset_ctl(loop->backend_fd, &pc, 1)) {
        assert(0 && "Failed to add file descriptor (pc.fd) to pollset");
        abort();
      }
    }

    w->events = w->pevents;
  }

  assert(timeout >= -1);
  base = loop->time;
  count = 48; /* Benchmarks suggest this gives the best throughput. */

  for (;;) {
    nfds = pollset_poll(loop->backend_fd,
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
      if (errno != EINTR) {
        abort();
      }

      if (timeout == -1)
        continue;

      if (timeout == 0)
        return;

      /* Interrupted by a signal. Update timeout and poll again. */
      goto update_timeout;
    }

    nevents = 0;

    assert(loop->watchers != NULL);
    loop->watchers[loop->nwatchers] = (void*) events;
    loop->watchers[loop->nwatchers + 1] = (void*) (uintptr_t) nfds;

    for (i = 0; i < nfds; i++) {
      pe = events + i;
      pc.cmd = PS_DELETE;
      pc.fd = pe->fd;

      /* Skip invalidated events, see uv__platform_invalidate_fd */
      if (pc.fd == -1)
        continue;

      assert(pc.fd >= 0);
      assert((unsigned) pc.fd < loop->nwatchers);

      w = loop->watchers[pc.fd];

      if (w == NULL) {
        /* File descriptor that we've stopped watching, disarm it.
         *
         * Ignore all errors because we may be racing with another thread
         * when the file descriptor is closed.
         */
        pollset_ctl(loop->backend_fd, &pc, 1);
        continue;
      }

      w->cb(loop, w, pe->revents);
      nevents++;
    }

    loop->watchers[loop->nwatchers] = NULL;
    loop->watchers[loop->nwatchers + 1] = NULL;

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


uint64_t uv__hrtime(uv_clocktype_t type) {
  uint64_t G = 1000000000;
  timebasestruct_t t;
  read_wall_time(&t, TIMEBASE_SZ);
  time_base_to_time(&t, TIMEBASE_SZ);
  return (uint64_t) t.tb_high * G + t.tb_low;
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 * There is no direct way of getting the exe path in AIX - either through /procfs
 * or through some libc APIs. The below approach is to parse the argv[0]'s pattern
 * and use it in conjunction with PATH environment variable to craft one.
 */
int uv_exepath(char* buffer, size_t* size) {
  ssize_t res;
  char cwd[PATH_MAX], cwdl[PATH_MAX];
  char symlink[PATH_MAX], temp_buffer[PATH_MAX];
  char pp[64];
  struct psinfo ps;
  int fd;
  char **argv;

  if ((buffer == NULL) || (size == NULL))
    return -EINVAL;

  snprintf(pp, sizeof(pp), "/proc/%lu/psinfo", (unsigned long) getpid());

  fd = open(pp, O_RDONLY);
  if (fd < 0)
    return fd;

  res = read(fd, &ps, sizeof(ps));
  uv__close(fd);
  if (res < 0)
    return res;

  if (ps.pr_argv == 0)
    return -EINVAL;

  argv = (char **) *((char ***) (intptr_t) ps.pr_argv);

  if ((argv == NULL) || (argv[0] == NULL))
    return -EINVAL;

  /*
   * Three possibilities for argv[0]:
   * i) an absolute path such as: /home/user/myprojects/nodejs/node
   * ii) a relative path such as: ./node or ./myprojects/nodejs/node
   * iii) a bare filename such as "node", after exporting PATH variable 
   *     to its location.
   */

  /* case #1, absolute path. */
  if (argv[0][0] == '/') {
    snprintf(symlink, PATH_MAX-1, "%s", argv[0]);

    /* This could or could not be a symlink. */
    res = readlink(symlink, temp_buffer, PATH_MAX-1);

    /* if readlink fails, it is a normal file just copy symlink to the 
     * output buffer.
     */
    if (res < 0) {
      assert(*size > strlen(symlink));
      strcpy(buffer, symlink);

    /* If it is a link, the resolved filename is again a relative path,
     * make it absolute. 
     */
    } else {
      assert(*size > (strlen(symlink) + 1 + strlen(temp_buffer)));
      snprintf(buffer, *size-1, "%s/%s", dirname(symlink), temp_buffer);
    }
    *size = strlen(buffer);
    return 0;

  /* case #2, relative path with usage of '.' */
  } else if (argv[0][0] == '.') {
    char *relative = strchr(argv[0], '/');
    if (relative == NULL)
      return -EINVAL;

    /* Get the current working directory to resolve the relative path. */
    snprintf(cwd, PATH_MAX-1, "/proc/%lu/cwd", (unsigned long) getpid());

    /* This is always a symlink, resolve it. */
    res = readlink(cwd, cwdl, sizeof(cwdl) - 1);
    if (res < 0)
      return -errno;

    snprintf(symlink, PATH_MAX-1, "%s%s", cwdl, relative + 1);

    res = readlink(symlink, temp_buffer, PATH_MAX-1);
    if (res < 0) {
      assert(*size > strlen(symlink));
      strcpy(buffer, symlink);
    } else {
      assert(*size > (strlen(symlink) + 1 + strlen(temp_buffer)));
      snprintf(buffer, *size-1, "%s/%s", dirname(symlink), temp_buffer);
    }
    *size = strlen(buffer);
    return 0;

  /* case #3, relative path without usage of '.', such as invocations in Node test suite. */
  } else if (strchr(argv[0], '/') != NULL) {
    /* Get the current working directory to resolve the relative path. */
    snprintf(cwd, PATH_MAX-1, "/proc/%lu/cwd", (unsigned long) getpid());

    /* This is always a symlink, resolve it. */
    res = readlink(cwd, cwdl, sizeof(cwdl) - 1);
    if (res < 0)
      return -errno;

    snprintf(symlink, PATH_MAX-1, "%s%s", cwdl, argv[0]);

    res = readlink(symlink, temp_buffer, PATH_MAX-1);
    if (res < 0) {
      assert(*size > strlen(symlink));
      strcpy(buffer, symlink);
    } else {
      assert(*size > (strlen(symlink) + 1 + strlen(temp_buffer)));
      snprintf(buffer, *size-1, "%s/%s", dirname(symlink), temp_buffer);
    }
    *size = strlen(buffer);
    return 0;
  /* Usage of absolute filename with location exported in PATH */
  } else {
    char clonedpath[8192];  /* assume 8k buffer will fit PATH */
    char *token = NULL;
    struct stat statstruct;

    /* Get the paths. */
    char *path = getenv("PATH");
    if(sizeof(clonedpath) <= strlen(path))
      return -EINVAL;

    /* Get a local copy. */
    strcpy(clonedpath, path);

    /* Tokenize. */
    token = strtok(clonedpath, ":");

    /* Get current working directory. (may be required in the loop). */
    snprintf(cwd, PATH_MAX-1, "/proc/%lu/cwd", (unsigned long) getpid());
    res = readlink(cwd, cwdl, sizeof(cwdl) - 1);
    if (res < 0)
      return -errno;
    /* Run through the tokens, append our executable file name with each,
     * and see which one succeeds. Exit on first match. */
    while(token != NULL) {
      if (token[0] == '.') {
        /* Path contains a token relative to current directory. */
        char *relative = strchr(token, '/');
        if (relative != NULL)
          /* A path which is not current directory. */
          snprintf(symlink, PATH_MAX-1, "%s%s/%s", cwdl, relative+1, ps.pr_fname);
        else
          snprintf(symlink, PATH_MAX-1, "%s%s", cwdl, ps.pr_fname);
        if (stat(symlink, &statstruct) != -1) {
          /* File exists. Resolve if it is a link. */
          res = readlink(symlink, temp_buffer, PATH_MAX-1);
          if (res < 0) {
            assert(*size > strlen(symlink));
            strcpy(buffer, symlink);
          } else {
            assert(*size > (strlen(symlink) + 1 + strlen(temp_buffer)));
            snprintf(buffer, *size-1, "%s/%s", dirname(symlink), temp_buffer);
          }
          *size = strlen(buffer);
          return 0;
        }

        /* Absolute path names. */
      } else {
        snprintf(symlink, PATH_MAX-1, "%s/%s", token, ps.pr_fname);
        if (stat(symlink, &statstruct) != -1) {
          res = readlink(symlink, temp_buffer, PATH_MAX-1);
          if (res < 0) {
            assert(*size > strlen(symlink));
            strcpy(buffer, symlink);
          } else {
            assert(*size > (strlen(symlink) + 1 + strlen(temp_buffer)));
            snprintf(buffer, *size-1, "%s/%s", dirname(symlink), temp_buffer);
          }
          *size = strlen(buffer);
          return 0;
        }
      }
      token = strtok(NULL, ":");
    }
    /* Out of tokens (path entries), and no match found */
    return -EINVAL;
  }
}


uint64_t uv_get_free_memory(void) {
  perfstat_memory_total_t mem_total;
  int result = perfstat_memory_total(NULL, &mem_total, sizeof(mem_total), 1);
  if (result == -1) {
    return 0;
  }
  return mem_total.real_free * 4096;
}


uint64_t uv_get_total_memory(void) {
  perfstat_memory_total_t mem_total;
  int result = perfstat_memory_total(NULL, &mem_total, sizeof(mem_total), 1);
  if (result == -1) {
    return 0;
  }
  return mem_total.real_total * 4096;
}


void uv_loadavg(double avg[3]) {
  perfstat_cpu_total_t ps_total;
  int result = perfstat_cpu_total(NULL, &ps_total, sizeof(ps_total), 1);
  if (result == -1) {
    avg[0] = 0.; avg[1] = 0.; avg[2] = 0.;
    return;
  }
  avg[0] = ps_total.loadavg[0] / (double)(1 << SBITS);
  avg[1] = ps_total.loadavg[1] / (double)(1 << SBITS);
  avg[2] = ps_total.loadavg[2] / (double)(1 << SBITS);
}


static char *uv__rawname(char *cp) {
  static char rawbuf[FILENAME_MAX+1];
  char *dp = rindex(cp, '/');

  if (dp == 0)
    return 0;

  *dp = 0;
  strcpy(rawbuf, cp);
  *dp = '/';
  strcat(rawbuf, "/r");
  strcat(rawbuf, dp+1);
  return rawbuf;
}


/* 
 * Determine whether given pathname is a directory
 * Returns 0 if the path is a directory, -1 if not
 *
 * Note: Opportunity here for more detailed error information but
 *       that requires changing callers of this function as well
 */
static int uv__path_is_a_directory(char* filename) {
  struct stat statbuf;

  if (stat(filename, &statbuf) < 0)
    return -1;  /* failed: not a directory, assume it is a file */

  if (statbuf.st_type == VDIR)
    return 0;

  return -1;
}


/* 
 * Check whether AHAFS is mounted.
 * Returns 0 if AHAFS is mounted, or an error code < 0 on failure
 */
static int uv__is_ahafs_mounted(void){
  int rv, i = 2;
  struct vmount *p;
  int size_multiplier = 10;
  size_t siz = sizeof(struct vmount)*size_multiplier;
  struct vmount *vmt;
  const char *dev = "/aha";
  char *obj, *stub;

  p = malloc(siz);
  if (p == NULL)
    return -errno;

  /* Retrieve all mounted filesystems */
  rv = mntctl(MCTL_QUERY, siz, (char*)p);
  if (rv < 0)
    return -errno;
  if (rv == 0) {
    /* buffer was not large enough, reallocate to correct size */
    siz = *(int*)p;
    free(p);
    p = malloc(siz);
    if (p == NULL)
      return -errno;
    rv = mntctl(MCTL_QUERY, siz, (char*)p);
    if (rv < 0)
      return -errno;
  }

  /* Look for dev in filesystems mount info */
  for(vmt = p, i = 0; i < rv; i++) {
    obj = vmt2dataptr(vmt, VMT_OBJECT);     /* device */
    stub = vmt2dataptr(vmt, VMT_STUB);      /* mount point */

    if (EQ(obj, dev) || EQ(uv__rawname(obj), dev) || EQ(stub, dev)) {
      free(p);  /* Found a match */
      return 0;
    }
    vmt = (struct vmount *) ((char *) vmt + vmt->vmt_length);
  }

  /* /aha is required for monitoring filesystem changes */
  return -1;
}

/*
 * Recursive call to mkdir() to create intermediate folders, if any
 * Returns code from mkdir call
 */
static int uv__makedir_p(const char *dir) {
  char tmp[256];
  char *p = NULL;
  size_t len;
  int err;

  snprintf(tmp, sizeof(tmp),"%s",dir);
  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      err = mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if(err != 0)
        return err;
      *p = '/';
    }
  }
  return mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/* 
 * Creates necessary subdirectories in the AIX Event Infrastructure
 * file system for monitoring the object specified.
 * Returns code from mkdir call
 */
static int uv__make_subdirs_p(const char *filename) {
  char cmd[2048];
  char *p;
  int rc = 0;

  /* Strip off the monitor file name */
  p = strrchr(filename, '/');

  if (p == NULL)
    return 0;

  if (uv__path_is_a_directory((char*)filename) == 0) {
    sprintf(cmd, "/aha/fs/modDir.monFactory");
  } else {
    sprintf(cmd, "/aha/fs/modFile.monFactory");
  }

  strncat(cmd, filename, (p - filename));
  rc = uv__makedir_p(cmd);

  if (rc == -1 && errno != EEXIST){
    return -errno;
  }

  return rc;
}


/*
 * Checks if /aha is mounted, then proceeds to set up the monitoring
 * objects for the specified file.
 * Returns 0 on success, or an error code < 0 on failure
 */
static int uv__setup_ahafs(const char* filename, int *fd) {
  int rc = 0;
  char mon_file_write_string[RDWR_BUF_SIZE];
  char mon_file[PATH_MAX];
  int file_is_directory = 0; /* -1 == NO, 0 == YES  */

  /* Create monitor file name for object */
  file_is_directory = uv__path_is_a_directory((char*)filename);

  if (file_is_directory == 0)
    sprintf(mon_file, "/aha/fs/modDir.monFactory");
  else
    sprintf(mon_file, "/aha/fs/modFile.monFactory");

  if ((strlen(mon_file) + strlen(filename) + 5) > PATH_MAX)
    return -ENAMETOOLONG;

  /* Make the necessary subdirectories for the monitor file */
  rc = uv__make_subdirs_p(filename);
  if (rc == -1 && errno != EEXIST)
    return rc;

  strcat(mon_file, filename);
  strcat(mon_file, ".mon");

  *fd = 0; errno = 0;

  /* Open the monitor file, creating it if necessary */
  *fd = open(mon_file, O_CREAT|O_RDWR);
  if (*fd < 0)
    return -errno;

  /* Write out the monitoring specifications.
   * In this case, we are monitoring for a state change event type
   *    CHANGED=YES
   * We will be waiting in select call, rather than a read:
   *    WAIT_TYPE=WAIT_IN_SELECT
   * We only want minimal information for files:
   *      INFO_LVL=1
   * For directories, we want more information to track what file
   * caused the change
   *      INFO_LVL=2
   */

  if (file_is_directory == 0)
    sprintf(mon_file_write_string, "CHANGED=YES;WAIT_TYPE=WAIT_IN_SELECT;INFO_LVL=2");
  else
    sprintf(mon_file_write_string, "CHANGED=YES;WAIT_TYPE=WAIT_IN_SELECT;INFO_LVL=1");

  rc = write(*fd, mon_file_write_string, strlen(mon_file_write_string)+1);
  if (rc < 0)
    return -errno;

  return 0;
}

/*
 * Skips a specified number of lines in the buffer passed in.
 * Walks the buffer pointed to by p and attempts to skip n lines.
 * Returns the total number of lines skipped
 */
static int uv__skip_lines(char **p, int n) {
  int lines = 0;

  while(n > 0) {
    *p = strchr(*p, '\n');
    if (!p)
      return lines;

    (*p)++;
    n--;
    lines++;
  }
  return lines;
}


/*
 * Parse the event occurrence data to figure out what event just occurred
 * and take proper action.
 * 
 * The buf is a pointer to the buffer containing the event occurrence data
 * Returns 0 on success, -1 if unrecoverable error in parsing
 *
 */
static int uv__parse_data(char *buf, int *events, uv_fs_event_t* handle) {
  int    evp_rc, i;
  char   *p;
  char   filename[PATH_MAX]; /* To be used when handling directories */

  p = buf;
  *events = 0;

  /* Clean the filename buffer*/
  for(i = 0; i < PATH_MAX; i++) {
    filename[i] = 0;
  }
  i = 0;

  /* Check for BUF_WRAP */
  if (strncmp(buf, "BUF_WRAP", strlen("BUF_WRAP")) == 0) {
    assert(0 && "Buffer wrap detected, Some event occurrences lost!");
    return 0;
  }

  /* Since we are using the default buffer size (4K), and have specified
   * INFO_LVL=1, we won't see any EVENT_OVERFLOW conditions.  Applications
   * should check for this keyword if they are using an INFO_LVL of 2 or
   * higher, and have a buffer size of <= 4K
   */

  /* Skip to RC_FROM_EVPROD */
  if (uv__skip_lines(&p, 9) != 9)
    return -1;

  if (sscanf(p, "RC_FROM_EVPROD=%d\nEND_EVENT_DATA", &evp_rc) == 1) {
    if (uv__path_is_a_directory(handle->path) == 0) { /* Directory */
      if (evp_rc == AHAFS_MODDIR_UNMOUNT || evp_rc == AHAFS_MODDIR_REMOVE_SELF) {
        /* The directory is no longer available for monitoring */
        *events = UV_RENAME;
        handle->dir_filename = NULL;
      } else {
        /* A file was added/removed inside the directory */
        *events = UV_CHANGE;

        /* Get the EVPROD_INFO */
        if (uv__skip_lines(&p, 1) != 1)
          return -1;

        /* Scan out the name of the file that triggered the event*/
        if (sscanf(p, "BEGIN_EVPROD_INFO\n%sEND_EVPROD_INFO", filename) == 1) {
          handle->dir_filename = strdup((const char*)&filename);
        } else
          return -1;
        }
    } else { /* Regular File */
      if (evp_rc == AHAFS_MODFILE_RENAME)
        *events = UV_RENAME;
      else
        *events = UV_CHANGE;
    }
  }
  else
    return -1;

  return 0;
}


/* This is the internal callback */
static void uv__ahafs_event(uv_loop_t* loop, uv__io_t* event_watch, unsigned int fflags) {
  char   result_data[RDWR_BUF_SIZE];
  int bytes, rc = 0;
  uv_fs_event_t* handle;
  int events = 0;
  int  i = 0;
  char fname[PATH_MAX];
  char *p;

  handle = container_of(event_watch, uv_fs_event_t, event_watcher);

  /* Clean all the buffers*/
  for(i = 0; i < PATH_MAX; i++) {
    fname[i] = 0;
  }
  i = 0;

  /* At this point, we assume that polling has been done on the
   * file descriptor, so we can just read the AHAFS event occurrence
   * data and parse its results without having to block anything
   */
  bytes = pread(event_watch->fd, result_data, RDWR_BUF_SIZE, 0);

  assert((bytes <= 0) && "uv__ahafs_event - Error reading monitor file");

  /* Parse the data */
  if(bytes > 0)
    rc = uv__parse_data(result_data, &events, handle);

  /* For directory changes, the name of the files that triggered the change
   * are never absolute pathnames
   */
  if (uv__path_is_a_directory(handle->path) == 0) {
    p = handle->dir_filename;
    while(*p != NULL){
      fname[i]= *p;
      i++;
      p++;
    }
  } else {
    /* For file changes, figure out whether filename is absolute or not */
    if (handle->path[0] == '/') {
      p = strrchr(handle->path, '/');
      p++;

      while(*p != NULL) {
        fname[i]= *p;
        i++;
        p++;
      }
    }
  }

  /* Unrecoverable error */
  if (rc == -1)
    return;
  else /* Call the actual JavaScript callback function */
    handle->cb(handle, (const char*)&fname, events, 0);
}


int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  return 0;
}


int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* filename,
                      unsigned int flags) {
  int  fd, rc, i = 0, res = 0;
  char cwd[PATH_MAX];
  char absolute_path[PATH_MAX];
  char fname[PATH_MAX];
  char *p;

  /* Clean all the buffers*/
  for(i = 0; i < PATH_MAX; i++) {
    cwd[i] = 0;
    absolute_path[i] = 0;
    fname[i] = 0;
  }
  i = 0;

  /* Figure out whether filename is absolute or not */
  if (filename[0] == '/') {
    /* We have absolute pathname, create the relative pathname*/
    sprintf(absolute_path, filename);
    p = strrchr(filename, '/');
    p++;
  } else {
    if (filename[0] == '.' && filename[1] == '/') {
      /* We have a relative pathname, compose the absolute pathname */
      sprintf(fname, filename);
      snprintf(cwd, PATH_MAX-1, "/proc/%lu/cwd", (unsigned long) getpid());
      res = readlink(cwd, absolute_path, sizeof(absolute_path) - 1);
      if (res < 0)
        return res;
      p = strrchr(absolute_path, '/');
      p++;
      p++;
    } else {
      /* We have a relative pathname, compose the absolute pathname */
      sprintf(fname, filename);
      snprintf(cwd, PATH_MAX-1, "/proc/%lu/cwd", (unsigned long) getpid());
      res = readlink(cwd, absolute_path, sizeof(absolute_path) - 1);
      if (res < 0)
        return res;
      p = strrchr(absolute_path, '/');
      p++;
    }
    /* Copy to filename buffer */
    while(filename[i] != NULL) {
      *p = filename[i];
      i++;
      p++;
    }
  }

  if (uv__is_ahafs_mounted() < 0)  /* /aha checks failed */
    return UV_ENOSYS;

  /* Setup ahafs */
  rc = uv__setup_ahafs((const char *)absolute_path, &fd);
  if (rc != 0)
    return rc;

  /* Setup/Initialize all the libuv routines */
  uv__handle_start(handle);
  uv__io_init(&handle->event_watcher, uv__ahafs_event, fd);
  handle->path = strdup((const char*)&absolute_path);
  handle->cb = cb;

  uv__io_start(handle->loop, &handle->event_watcher, UV__POLLIN);

  return 0;
}


int uv_fs_event_stop(uv_fs_event_t* handle) {

  if (!uv__is_active(handle))
    return 0;

  uv__io_close(handle->loop, &handle->event_watcher);
  uv__handle_stop(handle);

  if (uv__path_is_a_directory(handle->path) == 0) {
    free(handle->dir_filename);
    handle->dir_filename = NULL;
  }

  free(handle->path);
  handle->path = NULL;
  uv__close(handle->event_watcher.fd);
  handle->event_watcher.fd = -1;

  return 0;
}


void uv__fs_event_close(uv_fs_event_t* handle) {
  uv_fs_event_stop(handle);
}


char** uv_setup_args(int argc, char** argv) {
  return argv;
}


int uv_set_process_title(const char* title) {
  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  if (size > 0) {
    buffer[0] = '\0';
  }
  return 0;
}


int uv_resident_set_memory(size_t* rss) {
  char pp[64];
  psinfo_t psinfo;
  int err;
  int fd;

  snprintf(pp, sizeof(pp), "/proc/%lu/psinfo", (unsigned long) getpid());

  fd = open(pp, O_RDONLY);
  if (fd == -1)
    return -errno;

  /* FIXME(bnoordhuis) Handle EINTR. */
  err = -EINVAL;
  if (read(fd, &psinfo, sizeof(psinfo)) == sizeof(psinfo)) {
    *rss = (size_t)psinfo.pr_rssize * 1024;
    err = 0;
  }
  uv__close(fd);

  return err;
}


int uv_uptime(double* uptime) {
  struct utmp *utmp_buf;
  size_t entries = 0;
  time_t boot_time;

  utmpname(UTMP_FILE);

  setutent();

  while ((utmp_buf = getutent()) != NULL) {
    if (utmp_buf->ut_user[0] && utmp_buf->ut_type == USER_PROCESS)
      ++entries;
    if (utmp_buf->ut_type == BOOT_TIME)
      boot_time = utmp_buf->ut_time;
  }

  endutent();

  if (boot_time == 0)
    return -ENOSYS;

  *uptime = time(NULL) - boot_time;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  uv_cpu_info_t* cpu_info;
  perfstat_cpu_total_t ps_total;
  perfstat_cpu_t* ps_cpus;
  perfstat_id_t cpu_id;
  int result, ncpus, idx = 0;

  result = perfstat_cpu_total(NULL, &ps_total, sizeof(ps_total), 1);
  if (result == -1) {
    return -ENOSYS;
  }

  ncpus = result = perfstat_cpu(NULL, NULL, sizeof(perfstat_cpu_t), 0);
  if (result == -1) {
    return -ENOSYS;
  }

  ps_cpus = (perfstat_cpu_t*) malloc(ncpus * sizeof(perfstat_cpu_t));
  if (!ps_cpus) {
    return -ENOMEM;
  }

  strcpy(cpu_id.name, FIRST_CPU);
  result = perfstat_cpu(&cpu_id, ps_cpus, sizeof(perfstat_cpu_t), ncpus);
  if (result == -1) {
    free(ps_cpus);
    return -ENOSYS;
  }

  *cpu_infos = (uv_cpu_info_t*) malloc(ncpus * sizeof(uv_cpu_info_t));
  if (!*cpu_infos) {
    free(ps_cpus);
    return -ENOMEM;
  }

  *count = ncpus;

  cpu_info = *cpu_infos;
  while (idx < ncpus) {
    cpu_info->speed = (int)(ps_total.processorHZ / 1000000);
    cpu_info->model = strdup(ps_total.description);
    cpu_info->cpu_times.user = ps_cpus[idx].user;
    cpu_info->cpu_times.sys = ps_cpus[idx].sys;
    cpu_info->cpu_times.idle = ps_cpus[idx].idle;
    cpu_info->cpu_times.irq = ps_cpus[idx].wait;
    cpu_info->cpu_times.nice = 0;
    cpu_info++;
    idx++;
  }

  free(ps_cpus);
  return 0;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; ++i) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


int uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
  uv_interface_address_t* address;
  int sockfd, size = 1;
  struct ifconf ifc;
  struct ifreq *ifr, *p, flg;

  *count = 0;

  if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))) {
    return -ENOSYS;
  }

  if (ioctl(sockfd, SIOCGSIZIFCONF, &size) == -1) {
    uv__close(sockfd);
    return -ENOSYS;
  }

  ifc.ifc_req = (struct ifreq*)malloc(size);
  ifc.ifc_len = size;
  if (ioctl(sockfd, SIOCGIFCONF, &ifc) == -1) {
    uv__close(sockfd);
    return -ENOSYS;
  }

#define ADDR_SIZE(p) MAX((p).sa_len, sizeof(p))

  /* Count all up and running ipv4/ipv6 addresses */
  ifr = ifc.ifc_req;
  while ((char*)ifr < (char*)ifc.ifc_req + ifc.ifc_len) {
    p = ifr;
    ifr = (struct ifreq*)
      ((char*)ifr + sizeof(ifr->ifr_name) + ADDR_SIZE(ifr->ifr_addr));

    if (!(p->ifr_addr.sa_family == AF_INET6 ||
          p->ifr_addr.sa_family == AF_INET))
      continue;

    memcpy(flg.ifr_name, p->ifr_name, sizeof(flg.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &flg) == -1) {
      uv__close(sockfd);
      return -ENOSYS;
    }

    if (!(flg.ifr_flags & IFF_UP && flg.ifr_flags & IFF_RUNNING))
      continue;

    (*count)++;
  }

  /* Alloc the return interface structs */
  *addresses = (uv_interface_address_t*)
    malloc(*count * sizeof(uv_interface_address_t));
  if (!(*addresses)) {
    uv__close(sockfd);
    return -ENOMEM;
  }
  address = *addresses;

  ifr = ifc.ifc_req;
  while ((char*)ifr < (char*)ifc.ifc_req + ifc.ifc_len) {
    p = ifr;
    ifr = (struct ifreq*)
      ((char*)ifr + sizeof(ifr->ifr_name) + ADDR_SIZE(ifr->ifr_addr));

    if (!(p->ifr_addr.sa_family == AF_INET6 ||
          p->ifr_addr.sa_family == AF_INET))
      continue;

    memcpy(flg.ifr_name, p->ifr_name, sizeof(flg.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &flg) == -1) {
      uv__close(sockfd);
      return -ENOSYS;
    }

    if (!(flg.ifr_flags & IFF_UP && flg.ifr_flags & IFF_RUNNING))
      continue;

    /* All conditions above must match count loop */

    address->name = strdup(p->ifr_name);

    if (p->ifr_addr.sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6*) &p->ifr_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) &p->ifr_addr);
    }

    /* TODO: Retrieve netmask using SIOCGIFNETMASK ioctl */

    address->is_internal = flg.ifr_flags & IFF_LOOPBACK ? 1 : 0;

    address++;
  }

#undef ADDR_SIZE

  uv__close(sockfd);
  return 0;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; ++i) {
    free(addresses[i].name);
  }

  free(addresses);
}

void uv__platform_invalidate_fd(uv_loop_t* loop, int fd) {
  struct pollfd* events;
  uintptr_t i;
  uintptr_t nfds;

  assert(loop->watchers != NULL);

  events = (struct pollfd*) loop->watchers[loop->nwatchers];
  nfds = (uintptr_t) loop->watchers[loop->nwatchers + 1];
  if (events == NULL)
    return;

  /* Invalidate events with same file descriptor */
  for (i = 0; i < nfds; i++)
    if ((int) events[i].fd == fd)
      events[i].fd = -1;
}
