/* Copyright libuv project contributors. All rights reserved.
 *
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


#include "os390-syscalls.h"
#include <errno.h>
#include <stdlib.h>
#include <search.h>
#include <termios.h>
#include <sys/msg.h>

static struct uv__queue global_epoll_queue;
static uv_mutex_t global_epoll_lock;
static uv_once_t once = UV_ONCE_INIT;

int scandir(const char* maindir, struct dirent*** namelist,
            int (*filter)(const struct dirent*),
            int (*compar)(const struct dirent**,
            const struct dirent **)) {
  struct dirent** nl;
  struct dirent** nl_copy;
  struct dirent* dirent;
  unsigned count;
  size_t allocated;
  DIR* mdir;

  nl = NULL;
  count = 0;
  allocated = 0;
  mdir = opendir(maindir);
  if (!mdir)
    return -1;

  for (;;) {
    dirent = readdir(mdir);
    if (!dirent)
      break;
    if (!filter || filter(dirent)) {
      struct dirent* copy;
      copy = uv__malloc(sizeof(*copy));
      if (!copy)
        goto error;
      memcpy(copy, dirent, sizeof(*copy));

      nl_copy = uv__realloc(nl, sizeof(*copy) * (count + 1));
      if (nl_copy == NULL) {
        uv__free(copy);
        goto error;
      }

      nl = nl_copy;
      nl[count++] = copy;
    }
  }

  qsort(nl, count, sizeof(struct dirent *),
       (int (*)(const void *, const void *)) compar);

  closedir(mdir);

  *namelist = nl;
  return count;

error:
  while (count > 0) {
    dirent = nl[--count];
    uv__free(dirent);
  }
  uv__free(nl);
  closedir(mdir);
  errno = ENOMEM;
  return -1;
}


static unsigned int next_power_of_two(unsigned int val) {
  val -= 1;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  val += 1;
  return val;
}


static void maybe_resize(uv__os390_epoll* lst, unsigned int len) {
  unsigned int newsize;
  unsigned int i;
  struct pollfd* newlst;
  struct pollfd event;

  if (len <= lst->size)
    return;

  if (lst->size == 0)
    event.fd = -1;
  else {
    /* Extract the message queue at the end. */
    event = lst->items[lst->size - 1];
    lst->items[lst->size - 1].fd = -1;
  }

  newsize = next_power_of_two(len);
  newlst = uv__reallocf(lst->items, newsize * sizeof(lst->items[0]));

  if (newlst == NULL)
    abort();
  for (i = lst->size; i < newsize; ++i)
    newlst[i].fd = -1;

  /* Restore the message queue at the end */
  newlst[newsize - 1] = event;

  lst->items = newlst;
  lst->size = newsize;
}


void uv__os390_cleanup(void) {
  msgctl(uv_backend_fd(uv_default_loop()), IPC_RMID, NULL);
}


static void init_message_queue(uv__os390_epoll* lst) {
  struct {
    long int header;
    char body;
  } msg;

  /* initialize message queue */
  lst->msg_queue = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
  if (lst->msg_queue == -1)
    abort();

  /*
     On z/OS, the message queue will be affiliated with the process only
     when a send is performed on it. Once this is done, the system
     can be queried for all message queues belonging to our process id.
  */
  msg.header = 1;
  if (msgsnd(lst->msg_queue, &msg, sizeof(msg.body), 0) != 0)
    abort();

  /* Clean up the dummy message sent above */
  if (msgrcv(lst->msg_queue, &msg, sizeof(msg.body), 0, 0) != sizeof(msg.body))
    abort();
}


static void before_fork(void) {
  uv_mutex_lock(&global_epoll_lock);
}


static void after_fork(void) {
  uv_mutex_unlock(&global_epoll_lock);
}


static void child_fork(void) {
  struct uv__queue* q;
  uv_once_t child_once = UV_ONCE_INIT;

  /* reset once */
  memcpy(&once, &child_once, sizeof(child_once));

  /* reset epoll list */
  while (!uv__queue_empty(&global_epoll_queue)) {
    uv__os390_epoll* lst;
    q = uv__queue_head(&global_epoll_queue);
    uv__queue_remove(q);
    lst = uv__queue_data(q, uv__os390_epoll, member);
    uv__free(lst->items);
    lst->items = NULL;
    lst->size = 0;
  }

  uv_mutex_unlock(&global_epoll_lock);
  uv_mutex_destroy(&global_epoll_lock);
}


static void epoll_init(void) {
  uv__queue_init(&global_epoll_queue);
  if (uv_mutex_init(&global_epoll_lock))
    abort();

  if (pthread_atfork(&before_fork, &after_fork, &child_fork))
    abort();
}


uv__os390_epoll* epoll_create1(int flags) {
  uv__os390_epoll* lst;

  lst = uv__malloc(sizeof(*lst));
  if (lst != NULL) {
    /* initialize list */
    lst->size = 0;
    lst->items = NULL;
    init_message_queue(lst);
    maybe_resize(lst, 1);
    lst->items[lst->size - 1].fd = lst->msg_queue;
    lst->items[lst->size - 1].events = POLLIN;
    lst->items[lst->size - 1].revents = 0;
    uv_once(&once, epoll_init);
    uv_mutex_lock(&global_epoll_lock);
    uv__queue_insert_tail(&global_epoll_queue, &lst->member);
    uv_mutex_unlock(&global_epoll_lock);
  }

  return lst;
}


int epoll_ctl(uv__os390_epoll* lst,
              int op,
              int fd,
              struct epoll_event *event) {
  uv_mutex_lock(&global_epoll_lock);

  if (op == EPOLL_CTL_DEL) {
    if (fd >= lst->size || lst->items[fd].fd == -1) {
      uv_mutex_unlock(&global_epoll_lock);
      errno = ENOENT;
      return -1;
    }
    lst->items[fd].fd = -1;
  } else if (op == EPOLL_CTL_ADD) {

    /* Resizing to 'fd + 1' would expand the list to contain at least
     * 'fd'. But we need to guarantee that the last index on the list 
     * is reserved for the message queue. So specify 'fd + 2' instead.
     */
    maybe_resize(lst, fd + 2);
    if (lst->items[fd].fd != -1) {
      uv_mutex_unlock(&global_epoll_lock);
      errno = EEXIST;
      return -1;
    }
    lst->items[fd].fd = fd;
    lst->items[fd].events = event->events;
    lst->items[fd].revents = 0;
  } else if (op == EPOLL_CTL_MOD) {
    if (fd >= lst->size - 1 || lst->items[fd].fd == -1) {
      uv_mutex_unlock(&global_epoll_lock);
      errno = ENOENT;
      return -1;
    }
    lst->items[fd].events = event->events;
    lst->items[fd].revents = 0;
  } else
    abort();

  uv_mutex_unlock(&global_epoll_lock);
  return 0;
}

#define EP_MAX_PFDS (ULONG_MAX / sizeof(struct pollfd))
#define EP_MAX_EVENTS (INT_MAX / sizeof(struct epoll_event))

int epoll_wait(uv__os390_epoll* lst, struct epoll_event* events,
               int maxevents, int timeout) {
  nmsgsfds_t size;
  struct pollfd* pfds;
  int pollret;
  int pollfdret;
  int pollmsgret;
  int reventcount;
  int nevents;
  struct pollfd msg_fd;
  int i;

  if (!lst || !lst->items || !events) {
    errno = EFAULT;
    return -1;
  }

  if (lst->size > EP_MAX_PFDS) {
    errno = EINVAL;
    return -1;
  }

  if (maxevents <= 0 || maxevents > EP_MAX_EVENTS) {
    errno = EINVAL;
    return -1;
  }

  assert(lst->size > 0);
  _SET_FDS_MSGS(size, 1, lst->size - 1);
  pfds = lst->items;
  pollret = poll(pfds, size, timeout);
  if (pollret <= 0)
    return pollret;

  pollfdret = _NFDS(pollret);
  pollmsgret = _NMSGS(pollret);

  reventcount = 0;
  nevents = 0;
  msg_fd = pfds[lst->size - 1]; /* message queue is always last entry */
  maxevents = maxevents - pollmsgret; /* allow spot for message queue */
  for (i = 0;
       i < lst->size - 1 &&
       nevents < maxevents &&
       reventcount < pollfdret; ++i) {
    struct epoll_event ev;
    struct pollfd* pfd;

    pfd = &pfds[i];
    if (pfd->fd == -1 || pfd->revents == 0)
      continue;

    ev.fd = pfd->fd;
    ev.events = pfd->revents;
    ev.is_msg = 0;

    reventcount++;
    events[nevents++] = ev;
  }

  if (pollmsgret > 0 && msg_fd.revents != 0 && msg_fd.fd != -1) {
    struct epoll_event ev;
    ev.fd = msg_fd.fd;
    ev.events = msg_fd.revents;
    ev.is_msg = 1;
    events[nevents++] = ev;
  }

  return nevents;
}


int epoll_file_close(int fd) {
  struct uv__queue* q;

  uv_once(&once, epoll_init);
  uv_mutex_lock(&global_epoll_lock);
  uv__queue_foreach(q, &global_epoll_queue) {
    uv__os390_epoll* lst;

    lst = uv__queue_data(q, uv__os390_epoll, member);
    if (fd < lst->size && lst->items != NULL && lst->items[fd].fd != -1)
      lst->items[fd].fd = -1;
  }

  uv_mutex_unlock(&global_epoll_lock);
  return 0;
}

void epoll_queue_close(uv__os390_epoll* lst) {
  /* Remove epoll instance from global queue */
  uv_mutex_lock(&global_epoll_lock);
  uv__queue_remove(&lst->member);
  uv_mutex_unlock(&global_epoll_lock);

  /* Free resources */
  msgctl(lst->msg_queue, IPC_RMID, NULL);
  lst->msg_queue = -1;
  uv__free(lst->items);
  lst->items = NULL;
}


char* mkdtemp(char* path) {
  static const char* tempchars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static const size_t num_chars = 62;
  static const size_t num_x = 6;
  char *ep, *cp;
  unsigned int tries, i;
  size_t len;
  uint64_t v;
  int fd;
  int retval;
  int saved_errno;

  len = strlen(path);
  ep = path + len;
  if (len < num_x || strncmp(ep - num_x, "XXXXXX", num_x)) {
    errno = EINVAL;
    return NULL;
  }

  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1)
    return NULL;

  tries = TMP_MAX;
  retval = -1;
  do {
    if (read(fd, &v, sizeof(v)) != sizeof(v))
      break;

    cp = ep - num_x;
    for (i = 0; i < num_x; i++) {
      *cp++ = tempchars[v % num_chars];
      v /= num_chars;
    }

    if (mkdir(path, S_IRWXU) == 0) {
      retval = 0;
      break;
    }
    else if (errno != EEXIST)
      break;
  } while (--tries);

  saved_errno = errno;
  uv__close(fd);
  if (tries == 0) {
    errno = EEXIST;
    return NULL;
  }

  if (retval == -1) {
    errno = saved_errno;
    return NULL;
  }

  return path;
}


ssize_t os390_readlink(const char* path, char* buf, size_t len) {
  ssize_t rlen;
  ssize_t vlen;
  ssize_t plen;
  char* delimiter;
  char old_delim;
  char* tmpbuf;
  char realpathstr[PATH_MAX + 1];

  tmpbuf = uv__malloc(len + 1);
  if (tmpbuf == NULL) {
    errno = ENOMEM;
    return -1;
  }

  rlen = readlink(path, tmpbuf, len);
  if (rlen < 0) {
    uv__free(tmpbuf);
    return rlen;
  }

  if (rlen < 3 || strncmp("/$", tmpbuf, 2) != 0) {
    /* Straightforward readlink. */
    memcpy(buf, tmpbuf, rlen);
    uv__free(tmpbuf);
    return rlen;
  }

  /*
   * There is a parmlib variable at the beginning
   * which needs interpretation.
   */
  tmpbuf[rlen] = '\0';
  delimiter = strchr(tmpbuf + 2, '/');
  if (delimiter == NULL)
    /* No slash at the end */
    delimiter = strchr(tmpbuf + 2, '\0');

  /* Read real path of the variable. */
  old_delim = *delimiter;
  *delimiter = '\0';
  if (realpath(tmpbuf, realpathstr) == NULL) {
    uv__free(tmpbuf);
    return -1;
  }

  /* realpathstr is not guaranteed to end with null byte.*/
  realpathstr[PATH_MAX] = '\0';

  /* Reset the delimiter and fill up the buffer. */
  *delimiter = old_delim;
  plen = strlen(delimiter);
  vlen = strlen(realpathstr);
  rlen = plen + vlen;
  if (rlen > len) {
    uv__free(tmpbuf);
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(buf, realpathstr, vlen);
  memcpy(buf + vlen, delimiter, plen);

  /* Done using temporary buffer. */
  uv__free(tmpbuf);

  return rlen;
}


int sem_init(UV_PLATFORM_SEM_T* semid, int pshared, unsigned int value) {
  UNREACHABLE();
}


int sem_destroy(UV_PLATFORM_SEM_T* semid) {
  UNREACHABLE();
}


int sem_post(UV_PLATFORM_SEM_T* semid) {
  UNREACHABLE();
}


int sem_trywait(UV_PLATFORM_SEM_T* semid) {
  UNREACHABLE();
}


int sem_wait(UV_PLATFORM_SEM_T* semid) {
  UNREACHABLE();
}
