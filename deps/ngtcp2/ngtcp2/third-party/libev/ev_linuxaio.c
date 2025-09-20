/*
 * libev linux aio fd activity backend
 *
 * Copyright (c) 2019 Marc Alexander Lehmann <libev@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

/*
 * general notes about linux aio:
 *
 * a) at first, the linux aio IOCB_CMD_POLL functionality introduced in
 *    4.18 looks too good to be true: both watchers and events can be
 *    batched, and events can even be handled in userspace using
 *    a ring buffer shared with the kernel. watchers can be canceled
 *    regardless of whether the fd has been closed. no problems with fork.
 *    ok, the ring buffer is 200% undocumented (there isn't even a
 *    header file), but otherwise, it's pure bliss!
 * b) ok, watchers are one-shot, so you have to re-arm active ones
 *    on every iteration. so much for syscall-less event handling,
 *    but at least these re-arms can be batched, no big deal, right?
 * c) well, linux as usual: the documentation lies to you: io_submit
 *    sometimes returns EINVAL because the kernel doesn't feel like
 *    handling your poll mask - ttys can be polled for POLLOUT,
 *    POLLOUT|POLLIN, but polling for POLLIN fails. just great,
 *    so we have to fall back to something else (hello, epoll),
 *    but at least the fallback can be slow, because these are
 *    exceptional cases, right?
 * d) hmm, you have to tell the kernel the maximum number of watchers
 *    you want to queue when initialising the aio context. but of
 *    course the real limit is magically calculated in the kernel, and
 *    is often higher then we asked for. so we just have to destroy
 *    the aio context and re-create it a bit larger if we hit the limit.
 *    (starts to remind you of epoll? well, it's a bit more deterministic
 *    and less gambling, but still ugly as hell).
 * e) that's when you find out you can also hit an arbitrary system-wide
 *    limit. or the kernel simply doesn't want to handle your watchers.
 *    what the fuck do we do then? you guessed it, in the middle
 *    of event handling we have to switch to 100% epoll polling. and
 *    that better is as fast as normal epoll polling, so you practically
 *    have to use the normal epoll backend with all its quirks.
 * f) end result of this train wreck: it inherits all the disadvantages
 *    from epoll, while adding a number on its own. why even bother to use
 *    it? because if conditions are right and your fds are supported and you
 *    don't hit a limit, this backend is actually faster, doesn't gamble with
 *    your fds, batches watchers and events and doesn't require costly state
 *    recreates. well, until it does.
 * g) all of this makes this backend use almost twice as much code as epoll.
 *    which in turn uses twice as much code as poll. and that#s not counting
 *    the fact that this backend also depends on the epoll backend, making
 *    it three times as much code as poll, or kqueue.
 * h) bleah. why can't linux just do kqueue. sure kqueue is ugly, but by now
 *    it's clear that whatever linux comes up with is far, far, far worse.
 */

#include <sys/time.h> /* actually linux/time.h, but we must assume they are compatible */
#include <poll.h>
#include <linux/aio_abi.h>

/*****************************************************************************/
/* syscall wrapdadoop - this section has the raw api/abi definitions */

#include <sys/syscall.h> /* no glibc wrappers */

/* aio_abi.h is not versioned in any way, so we cannot test for its existance */
#define IOCB_CMD_POLL 5

/* taken from linux/fs/aio.c. yup, that's a .c file.
 * not only is this totally undocumented, not even the source code
 * can tell you what the future semantics of compat_features and
 * incompat_features are, or what header_length actually is for.
 */
#define AIO_RING_MAGIC                  0xa10a10a1
#define EV_AIO_RING_INCOMPAT_FEATURES   0
struct aio_ring
{
  unsigned id;    /* kernel internal index number */
  unsigned nr;    /* number of io_events */
  unsigned head;  /* Written to by userland or by kernel. */
  unsigned tail;

  unsigned magic;
  unsigned compat_features;
  unsigned incompat_features;
  unsigned header_length;  /* size of aio_ring */

  struct io_event io_events[0];
};

inline_size
int
evsys_io_setup (unsigned nr_events, aio_context_t *ctx_idp)
{
  return ev_syscall2 (SYS_io_setup, nr_events, ctx_idp);
}

inline_size
int
evsys_io_destroy (aio_context_t ctx_id)
{
  return ev_syscall1 (SYS_io_destroy, ctx_id);
}

inline_size
int
evsys_io_submit (aio_context_t ctx_id, long nr, struct iocb *cbp[])
{
  return ev_syscall3 (SYS_io_submit, ctx_id, nr, cbp);
}

inline_size
int
evsys_io_cancel (aio_context_t ctx_id, struct iocb *cbp, struct io_event *result)
{
  return ev_syscall3 (SYS_io_cancel, ctx_id, cbp, result);
}

inline_size
int
evsys_io_getevents (aio_context_t ctx_id, long min_nr, long nr, struct io_event *events, struct timespec *timeout)
{
  return ev_syscall5 (SYS_io_getevents, ctx_id, min_nr, nr, events, timeout);
}

/*****************************************************************************/
/* actual backed implementation */

ecb_cold
static int
linuxaio_nr_events (EV_P)
{
  /* we start with 16 iocbs and incraese from there
   * that's tiny, but the kernel has a rather low system-wide
   * limit that can be reached quickly, so let's be parsimonious
   * with this resource.
   * Rest assured, the kernel generously rounds up small and big numbers
   * in different ways (but doesn't seem to charge you for it).
   * The 15 here is because the kernel usually has a power of two as aio-max-nr,
   * and this helps to take advantage of that limit.
   */

  /* we try to fill 4kB pages exactly.
   * the ring buffer header is 32 bytes, every io event is 32 bytes.
   * the kernel takes the io requests number, doubles it, adds 2
   * and adds the ring buffer.
   * the way we use this is by starting low, and then roughly doubling the
   * size each time we hit a limit.
   */

  int requests   = 15 << linuxaio_iteration;
  int one_page   =  (4096
                    / sizeof (struct io_event)    ) / 2; /* how many fit into one page */
  int first_page = ((4096 - sizeof (struct aio_ring))
                    / sizeof (struct io_event) - 2) / 2; /* how many fit into the first page */

  /* if everything fits into one page, use count exactly */
  if (requests > first_page)
    /* otherwise, round down to full pages and add the first page */
    requests = requests / one_page * one_page + first_page;

  return requests;
}

/* we use out own wrapper structure in case we ever want to do something "clever" */
typedef struct aniocb
{
  struct iocb io;
  /*int inuse;*/
} *ANIOCBP;

inline_size
void
linuxaio_array_needsize_iocbp (ANIOCBP *base, int offset, int count)
{
  while (count--)
    {
      /* TODO: quite the overhead to allocate every iocb separately, maybe use our own allocator? */
      ANIOCBP iocb = (ANIOCBP)ev_malloc (sizeof (*iocb));

      /* full zero initialise is probably not required at the moment, but
       * this is not well documented, so we better do it.
       */
      memset (iocb, 0, sizeof (*iocb));

      iocb->io.aio_lio_opcode = IOCB_CMD_POLL;
      iocb->io.aio_fildes     = offset;

      base [offset++] = iocb;
    }
}

ecb_cold
static void
linuxaio_free_iocbp (EV_P)
{
  while (linuxaio_iocbpmax--)
    ev_free (linuxaio_iocbps [linuxaio_iocbpmax]);

  linuxaio_iocbpmax = 0; /* next resize will completely reallocate the array, at some overhead */
}

static void
linuxaio_modify (EV_P_ int fd, int oev, int nev)
{
  array_needsize (ANIOCBP, linuxaio_iocbps, linuxaio_iocbpmax, fd + 1, linuxaio_array_needsize_iocbp);
  ANIOCBP iocb = linuxaio_iocbps [fd];
  ANFD *anfd = &anfds [fd];

  if (ecb_expect_false (iocb->io.aio_reqprio < 0))
    {
      /* we handed this fd over to epoll, so undo this first */
      /* we do it manually because the optimisations on epoll_modify won't do us any good */
      epoll_ctl (backend_fd, EPOLL_CTL_DEL, fd, 0);
      anfd->emask = 0;
      iocb->io.aio_reqprio = 0;
    }
  else if (ecb_expect_false (iocb->io.aio_buf))
    {
      /* iocb active, so cancel it first before resubmit */
      /* this assumes we only ever get one call per fd per loop iteration */
      for (;;)
        {
          /* on all relevant kernels, io_cancel fails with EINPROGRESS on "success" */
          if (ecb_expect_false (evsys_io_cancel (linuxaio_ctx, &iocb->io, (struct io_event *)0) == 0))
            break;

          if (ecb_expect_true (errno == EINPROGRESS))
            break;

          /* the EINPROGRESS test is for nicer error message. clumsy. */
          if (errno != EINTR)
            {
              assert (("libev: linuxaio unexpected io_cancel failed", errno != EINTR && errno != EINPROGRESS));
              break;
            }
       }

      /* increment generation counter to avoid handling old events */
      ++anfd->egen;
    }

  iocb->io.aio_buf = (nev & EV_READ  ? POLLIN  : 0)
                   | (nev & EV_WRITE ? POLLOUT : 0);

  if (nev)
    {
      iocb->io.aio_data = (uint32_t)fd | ((__u64)(uint32_t)anfd->egen << 32);

      /* queue iocb up for io_submit */
      /* this assumes we only ever get one call per fd per loop iteration */
      ++linuxaio_submitcnt;
      array_needsize (struct iocb *, linuxaio_submits, linuxaio_submitmax, linuxaio_submitcnt, array_needsize_noinit);
      linuxaio_submits [linuxaio_submitcnt - 1] = &iocb->io;
    }
}

static void
linuxaio_epoll_cb (EV_P_ struct ev_io *w, int revents)
{
  epoll_poll (EV_A_ 0);
}

inline_speed
void
linuxaio_fd_rearm (EV_P_ int fd)
{
  anfds [fd].events = 0;
  linuxaio_iocbps [fd]->io.aio_buf = 0;
  fd_change (EV_A_ fd, EV_ANFD_REIFY);
}

static void
linuxaio_parse_events (EV_P_ struct io_event *ev, int nr)
{
  while (nr)
    {
      int fd       = ev->data & 0xffffffff;
      uint32_t gen = ev->data >> 32;
      int res      = ev->res;

      assert (("libev: iocb fd must be in-bounds", fd >= 0 && fd < anfdmax));

      /* only accept events if generation counter matches */
      if (ecb_expect_true (gen == (uint32_t)anfds [fd].egen))
        {
          /* feed events, we do not expect or handle POLLNVAL */
          fd_event (
            EV_A_
            fd,
            (res & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
            | (res & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
          );

          /* linux aio is oneshot: rearm fd. TODO: this does more work than strictly needed */
          linuxaio_fd_rearm (EV_A_ fd);
        }

      --nr;
      ++ev;
    }
}

/* get any events from ring buffer, return true if any were handled */
static int
linuxaio_get_events_from_ring (EV_P)
{
  struct aio_ring *ring = (struct aio_ring *)linuxaio_ctx;
  unsigned head, tail;

  /* the kernel reads and writes both of these variables, */
  /* as a C extension, we assume that volatile use here */
  /* both makes reads atomic and once-only */
  head = *(volatile unsigned *)&ring->head;
  ECB_MEMORY_FENCE_ACQUIRE;
  tail = *(volatile unsigned *)&ring->tail;

  if (head == tail)
    return 0;

  /* parse all available events, but only once, to avoid starvation */
  if (ecb_expect_true (tail > head)) /* normal case around */
    linuxaio_parse_events (EV_A_ ring->io_events + head, tail - head);
  else /* wrapped around */
    {
      linuxaio_parse_events (EV_A_ ring->io_events + head, ring->nr - head);
      linuxaio_parse_events (EV_A_ ring->io_events, tail);
    }

  ECB_MEMORY_FENCE_RELEASE;
  /* as an extension to C, we hope that the volatile will make this atomic and once-only */
  *(volatile unsigned *)&ring->head = tail;

  return 1;
}

inline_size
int
linuxaio_ringbuf_valid (EV_P)
{
  struct aio_ring *ring = (struct aio_ring *)linuxaio_ctx;

  return ecb_expect_true (ring->magic == AIO_RING_MAGIC)
                      && ring->incompat_features == EV_AIO_RING_INCOMPAT_FEATURES
                      && ring->header_length == sizeof (struct aio_ring); /* TODO: or use it to find io_event[0]? */
}

/* read at least one event from kernel, or timeout */
inline_size
void
linuxaio_get_events (EV_P_ ev_tstamp timeout)
{
  struct timespec ts;
  struct io_event ioev[8]; /* 256 octet stack space */
  int want = 1; /* how many events to request */
  int ringbuf_valid = linuxaio_ringbuf_valid (EV_A);

  if (ecb_expect_true (ringbuf_valid))
    {
      /* if the ring buffer has any events, we don't wait or call the kernel at all */
      if (linuxaio_get_events_from_ring (EV_A))
        return;

      /* if the ring buffer is empty, and we don't have a timeout, then don't call the kernel */
      if (!timeout)
        return;
    }
  else
    /* no ringbuffer, request slightly larger batch */
    want = sizeof (ioev) / sizeof (ioev [0]);

  /* no events, so wait for some
   * for fairness reasons, we do this in a loop, to fetch all events
   */
  for (;;)
    {
      int res;

      EV_RELEASE_CB;

      EV_TS_SET (ts, timeout);
      res = evsys_io_getevents (linuxaio_ctx, 1, want, ioev, &ts);

      EV_ACQUIRE_CB;

      if (res < 0)
        if (errno == EINTR)
          /* ignored, retry */;
        else
          ev_syserr ("(libev) linuxaio io_getevents");
      else if (res)
        {
          /* at least one event available, handle them */
          linuxaio_parse_events (EV_A_ ioev, res);

          if (ecb_expect_true (ringbuf_valid))
            {
              /* if we have a ring buffer, handle any remaining events in it */
              linuxaio_get_events_from_ring (EV_A);

              /* at this point, we should have handled all outstanding events */
              break;
            }
          else if (res < want)
            /* otherwise, if there were fewere events than we wanted, we assume there are no more */
            break;
        }
      else
        break; /* no events from the kernel, we are done */

      timeout = EV_TS_CONST (0.); /* only wait in the first iteration */
    }
}

inline_size
int
linuxaio_io_setup (EV_P)
{
  linuxaio_ctx = 0;
  return evsys_io_setup (linuxaio_nr_events (EV_A), &linuxaio_ctx);
}

static void
linuxaio_poll (EV_P_ ev_tstamp timeout)
{
  int submitted;

  /* first phase: submit new iocbs */

  /* io_submit might return less than the requested number of iocbs */
  /* this is, afaics, only because of errors, but we go by the book and use a loop, */
  /* which allows us to pinpoint the erroneous iocb */
  for (submitted = 0; submitted < linuxaio_submitcnt; )
    {
      int res = evsys_io_submit (linuxaio_ctx, linuxaio_submitcnt - submitted, linuxaio_submits + submitted);

      if (ecb_expect_false (res < 0))
        if (errno == EINVAL)
          {
            /* This happens for unsupported fds, officially, but in my testing,
             * also randomly happens for supported fds. We fall back to good old
             * poll() here, under the assumption that this is a very rare case.
             * See https://lore.kernel.org/patchwork/patch/1047453/ to see
             * discussion about such a case (ttys) where polling for POLLIN
             * fails but POLLIN|POLLOUT works.
             */
            struct iocb *iocb = linuxaio_submits [submitted];
            epoll_modify (EV_A_ iocb->aio_fildes, 0, anfds [iocb->aio_fildes].events);
            iocb->aio_reqprio = -1; /* mark iocb as epoll */

            res = 1; /* skip this iocb - another iocb, another chance */
          }
        else if (errno == EAGAIN)
          {
            /* This happens when the ring buffer is full, or some other shit we
             * don't know and isn't documented. Most likely because we have too
             * many requests and linux aio can't be assed to handle them.
             * In this case, we try to allocate a larger ring buffer, freeing
             * ours first. This might fail, in which case we have to fall back to 100%
             * epoll.
             * God, how I hate linux not getting its act together. Ever.
             */
            evsys_io_destroy (linuxaio_ctx);
            linuxaio_submitcnt = 0;

            /* rearm all fds with active iocbs */
            {
              int fd;
	      for (fd = 0; fd < linuxaio_iocbpmax; ++fd)
                if (linuxaio_iocbps [fd]->io.aio_buf)
                  linuxaio_fd_rearm (EV_A_ fd);
            }

            ++linuxaio_iteration;
            if (linuxaio_io_setup (EV_A) < 0)
              {
                /* TODO: rearm all and recreate epoll backend from scratch */
                /* TODO: might be more prudent? */

                /* to bad, we can't get a new aio context, go 100% epoll */
                linuxaio_free_iocbp (EV_A);
                ev_io_stop (EV_A_ &linuxaio_epoll_w);
                ev_ref (EV_A);
                linuxaio_ctx = 0;

                backend        = EVBACKEND_EPOLL;
                backend_modify = epoll_modify;
                backend_poll   = epoll_poll;
              }

            timeout = EV_TS_CONST (0.);
            /* it's easiest to handle this mess in another iteration */
            return;
          }
        else if (errno == EBADF)
          {
            assert (("libev: event loop rejected bad fd", errno != EBADF));
            fd_kill (EV_A_ linuxaio_submits [submitted]->aio_fildes);

            res = 1; /* skip this iocb */
          }
        else if (errno == EINTR) /* not seen in reality, not documented */
          res = 0; /* silently ignore and retry */
        else
          {
            ev_syserr ("(libev) linuxaio io_submit");
            res = 0;
          }

      submitted += res;
    }

  linuxaio_submitcnt = 0;

  /* second phase: fetch and parse events */

  linuxaio_get_events (EV_A_ timeout);
}

inline_size
int
linuxaio_init (EV_P_ int flags)
{
  /* would be great to have a nice test for IOCB_CMD_POLL instead */
  /* also: test some semi-common fd types, such as files and ttys in recommended_backends */
  /* 4.18 introduced IOCB_CMD_POLL, 4.19 made epoll work, and we need that */
  if (ev_linux_version () < 0x041300)
    return 0;

  if (!epoll_init (EV_A_ 0))
    return 0;

  linuxaio_iteration = 0;

  if (linuxaio_io_setup (EV_A) < 0)
    {
      epoll_destroy (EV_A);
      return 0;
    }

  ev_io_init  (&linuxaio_epoll_w, linuxaio_epoll_cb, backend_fd, EV_READ);
  ev_set_priority (&linuxaio_epoll_w, EV_MAXPRI);
  ev_io_start (EV_A_ &linuxaio_epoll_w);
  ev_unref (EV_A); /* watcher should not keep loop alive */

  backend_modify = linuxaio_modify;
  backend_poll   = linuxaio_poll;

  linuxaio_iocbpmax = 0;
  linuxaio_iocbps = 0;

  linuxaio_submits = 0;
  linuxaio_submitmax = 0;
  linuxaio_submitcnt = 0;

  return EVBACKEND_LINUXAIO;
}

inline_size
void
linuxaio_destroy (EV_P)
{
  epoll_destroy (EV_A);
  linuxaio_free_iocbp (EV_A);
  evsys_io_destroy (linuxaio_ctx); /* fails in child, aio context is destroyed */
}

ecb_cold
static void
linuxaio_fork (EV_P)
{
  linuxaio_submitcnt = 0; /* all pointers were invalidated */
  linuxaio_free_iocbp (EV_A); /* this frees all iocbs, which is very heavy-handed */
  evsys_io_destroy (linuxaio_ctx); /* fails in child, aio context is destroyed */

  linuxaio_iteration = 0; /* we start over in the child */

  while (linuxaio_io_setup (EV_A) < 0)
    ev_syserr ("(libev) linuxaio io_setup");

  /* forking epoll should also effectively unregister all fds from the backend */
  epoll_fork (EV_A);
  /* epoll_fork already did this. hopefully */
  /*fd_rearm_all (EV_A);*/

  ev_io_stop  (EV_A_ &linuxaio_epoll_w);
  ev_io_set   (EV_A_ &linuxaio_epoll_w, backend_fd, EV_READ);
  ev_io_start (EV_A_ &linuxaio_epoll_w);
}

