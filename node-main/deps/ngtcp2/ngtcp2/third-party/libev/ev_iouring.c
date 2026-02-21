/*
 * libev linux io_uring fd activity backend
 *
 * Copyright (c) 2019-2020 Marc Alexander Lehmann <libev@schmorp.de>
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
 * general notes about linux io_uring:
 *
 * a) it's the best interface I have seen so far. on linux.
 * b) best is not necessarily very good.
 * c) it's better than the aio mess, doesn't suffer from the fork problems
 *    of linux aio or epoll and so on and so on. and you could do event stuff
 *    without any syscalls. what's not to like?
 * d) ok, it's vastly more complex, but that's ok, really.
 * e) why two mmaps instead of one? one would be more space-efficient,
 *    and I can't see what benefit two would have (other than being
 *    somehow resizable/relocatable, but that's apparently not possible).
 * f) hmm, it's practically undebuggable (gdb can't access the memory, and
 *    the bizarre way structure offsets are communicated makes it hard to
 *    just print the ring buffer heads, even *iff* the memory were visible
 *    in gdb. but then, that's also ok, really.
 * g) well, you cannot specify a timeout when waiting for events. no,
 *    seriously, the interface doesn't support a timeout. never seen _that_
 *    before. sure, you can use a timerfd, but that's another syscall
 *    you could have avoided. overall, this bizarre omission smells
 *    like a µ-optimisation by the io_uring author for his personal
 *    applications, to the detriment of everybody else who just wants
 *    an event loop. but, umm, ok, if that's all, it could be worse.
 *    (from what I gather from the author Jens Axboe, it simply didn't
 *    occur to him, and he made good on it by adding an unlimited nuber
 *    of timeouts later :).
 * h) initially there was a hardcoded limit of 4096 outstanding events.
 *    later versions not only bump this to 32k, but also can handle
 *    an unlimited amount of events, so this only affects the batch size.
 * i) unlike linux aio, you *can* register more then the limit
 *    of fd events. while early verisons of io_uring signalled an overflow
 *    and you ended up getting wet. 5.5+ does not do this anymore.
 * j) but, oh my! it had exactly the same bugs as the linux aio backend,
 *    where some undocumented poll combinations just fail. fortunately,
 *    after finally reaching the author, he was more than willing to fix
 *    this probably in 5.6+.
 * k) overall, the *API* itself is, I dare to say, not a total trainwreck.
 *    once the bugs ae fixed (probably in 5.6+), it will be without
 *    competition.
 */

/* TODO: use internal TIMEOUT */
/* TODO: take advantage of single mmap, NODROP etc. */
/* TODO: resize cq/sq size independently */

#include <sys/timerfd.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdint.h>

#define IOURING_INIT_ENTRIES 32

/*****************************************************************************/
/* syscall wrapdadoop - this section has the raw api/abi definitions */

#include <linux/fs.h>
#include <linux/types.h>

/* mostly directly taken from the kernel or documentation */

struct io_uring_sqe
{
  __u8 opcode;
  __u8 flags;
  __u16 ioprio;
  __s32 fd;
  union {
    __u64 off;
    __u64 addr2;
  };
  __u64 addr;
  __u32 len;
  union {
    __kernel_rwf_t rw_flags;
    __u32 fsync_flags;
    __u16 poll_events;
    __u32 sync_range_flags;
    __u32 msg_flags;
    __u32 timeout_flags;
    __u32 accept_flags;
    __u32 cancel_flags;
    __u32 open_flags;
    __u32 statx_flags;
  };
  __u64 user_data;
  union {
    __u16 buf_index;
    __u64 __pad2[3];
  };
};

struct io_uring_cqe
{
  __u64 user_data;
  __s32 res;
  __u32 flags;
};

struct io_sqring_offsets
{
  __u32 head;
  __u32 tail;
  __u32 ring_mask;
  __u32 ring_entries;
  __u32 flags;
  __u32 dropped;
  __u32 array;
  __u32 resv1;
  __u64 resv2;
};

struct io_cqring_offsets
{
  __u32 head;
  __u32 tail;
  __u32 ring_mask;
  __u32 ring_entries;
  __u32 overflow;
  __u32 cqes;
  __u64 resv[2];
};

struct io_uring_params
{
  __u32 sq_entries;
  __u32 cq_entries;
  __u32 flags;
  __u32 sq_thread_cpu;
  __u32 sq_thread_idle;
  __u32 features;
  __u32 resv[4];
  struct io_sqring_offsets sq_off;
  struct io_cqring_offsets cq_off;
};

#define IORING_SETUP_CQSIZE 0x00000008

#define IORING_OP_POLL_ADD        6
#define IORING_OP_POLL_REMOVE     7
#define IORING_OP_TIMEOUT        11
#define IORING_OP_TIMEOUT_REMOVE 12

/* relative or absolute, reference clock is CLOCK_MONOTONIC */
struct iouring_kernel_timespec
{
  int64_t tv_sec;
  long long tv_nsec;
};

#define IORING_TIMEOUT_ABS 0x00000001

#define IORING_ENTER_GETEVENTS 0x01

#define IORING_OFF_SQ_RING 0x00000000ULL
#define IORING_OFF_CQ_RING 0x08000000ULL
#define IORING_OFF_SQES	   0x10000000ULL

#define IORING_FEAT_SINGLE_MMAP   0x00000001
#define IORING_FEAT_NODROP        0x00000002
#define IORING_FEAT_SUBMIT_STABLE 0x00000004

inline_size
int
evsys_io_uring_setup (unsigned entries, struct io_uring_params *params)
{
  return ev_syscall2 (SYS_io_uring_setup, entries, params);
}

inline_size
int
evsys_io_uring_enter (int fd, unsigned to_submit, unsigned min_complete, unsigned flags, const sigset_t *sig, size_t sigsz)
{
  return ev_syscall6 (SYS_io_uring_enter, fd, to_submit, min_complete, flags, sig, sigsz);
}

/*****************************************************************************/
/* actual backed implementation */

/* we hope that volatile will make the compiler access this variables only once */
#define EV_SQ_VAR(name) *(volatile unsigned *)((char *)iouring_sq_ring + iouring_sq_ ## name)
#define EV_CQ_VAR(name) *(volatile unsigned *)((char *)iouring_cq_ring + iouring_cq_ ## name)

/* the index array */
#define EV_SQ_ARRAY     ((unsigned *)((char *)iouring_sq_ring + iouring_sq_array))

/* the submit/completion queue entries */
#define EV_SQES         ((struct io_uring_sqe *)         iouring_sqes)
#define EV_CQES         ((struct io_uring_cqe *)((char *)iouring_cq_ring + iouring_cq_cqes))

inline_speed
int
iouring_enter (EV_P_ ev_tstamp timeout)
{
  int res;

  EV_RELEASE_CB;

  res = evsys_io_uring_enter (iouring_fd, iouring_to_submit, 1,
                              timeout > EV_TS_CONST (0.) ? IORING_ENTER_GETEVENTS : 0, 0, 0);

  assert (("libev: io_uring_enter did not consume all sqes", (res < 0 || res == iouring_to_submit)));

  iouring_to_submit = 0;

  EV_ACQUIRE_CB;

  return res;
}

/* TODO: can we move things around so we don't need this forward-reference? */
static void
iouring_poll (EV_P_ ev_tstamp timeout);

static
struct io_uring_sqe *
iouring_sqe_get (EV_P)
{
  unsigned tail;
  
  for (;;)
    {
      tail = EV_SQ_VAR (tail);

      if (ecb_expect_true (tail + 1 - EV_SQ_VAR (head) <= EV_SQ_VAR (ring_entries)))
        break; /* whats the problem, we have free sqes */

      /* queue full, need to flush and possibly handle some events */

#if EV_FEATURE_CODE
      /* first we ask the kernel nicely, most often this frees up some sqes */
      int res = iouring_enter (EV_A_ EV_TS_CONST (0.));

      ECB_MEMORY_FENCE_ACQUIRE; /* better safe than sorry */

      if (res >= 0)
        continue; /* yes, it worked, try again */
#endif

      /* some problem, possibly EBUSY - do the full poll and let it handle any issues */

      iouring_poll (EV_A_ EV_TS_CONST (0.));
      /* iouring_poll should have done ECB_MEMORY_FENCE_ACQUIRE for us */
    }

  /*assert (("libev: io_uring queue full after flush", tail + 1 - EV_SQ_VAR (head) <= EV_SQ_VAR (ring_entries)));*/

  return EV_SQES + (tail & EV_SQ_VAR (ring_mask));
}

inline_size
struct io_uring_sqe *
iouring_sqe_submit (EV_P_ struct io_uring_sqe *sqe)
{
  unsigned idx = sqe - EV_SQES;

  EV_SQ_ARRAY [idx] = idx;
  ECB_MEMORY_FENCE_RELEASE;
  ++EV_SQ_VAR (tail);
  /*ECB_MEMORY_FENCE_RELEASE; /* for the time being we assume this is not needed */
  ++iouring_to_submit;
}

/*****************************************************************************/

/* when the timerfd expires we simply note the fact,
 * as the purpose of the timerfd is to wake us up, nothing else.
 * the next iteration should re-set it.
 */
static void
iouring_tfd_cb (EV_P_ struct ev_io *w, int revents)
{
  iouring_tfd_to = EV_TSTAMP_HUGE;
}

/* called for full and partial cleanup */
ecb_cold
static int
iouring_internal_destroy (EV_P)
{
  close (iouring_tfd);
  close (iouring_fd);

  if (iouring_sq_ring != MAP_FAILED) munmap (iouring_sq_ring, iouring_sq_ring_size);
  if (iouring_cq_ring != MAP_FAILED) munmap (iouring_cq_ring, iouring_cq_ring_size);
  if (iouring_sqes    != MAP_FAILED) munmap (iouring_sqes   , iouring_sqes_size   );

  if (ev_is_active (&iouring_tfd_w))
    {
      ev_ref (EV_A);
      ev_io_stop (EV_A_ &iouring_tfd_w);
    }
}

ecb_cold
static int
iouring_internal_init (EV_P)
{
  struct io_uring_params params = { 0 };

  iouring_to_submit = 0;

  iouring_tfd     = -1;
  iouring_sq_ring = MAP_FAILED;
  iouring_cq_ring = MAP_FAILED;
  iouring_sqes    = MAP_FAILED;

  if (!have_monotonic) /* cannot really happen, but what if11 */
    return -1;

  for (;;)
    {
      iouring_fd = evsys_io_uring_setup (iouring_entries, &params);

      if (iouring_fd >= 0)
        break; /* yippie */

      if (errno != EINVAL)
        return -1; /* we failed */

#if TODO
      if ((~params.features) & (IORING_FEAT_NODROP | IORING_FEATURE_SINGLE_MMAP | IORING_FEAT_SUBMIT_STABLE))
        return -1; /* we require the above features */
#endif

      /* EINVAL: lots of possible reasons, but maybe
       * it is because we hit the unqueryable hardcoded size limit
       */

      /* we hit the limit already, give up */
      if (iouring_max_entries)
        return -1;

      /* first time we hit EINVAL? assume we hit the limit, so go back and retry */
      iouring_entries >>= 1;
      iouring_max_entries = iouring_entries;
    }

  iouring_sq_ring_size = params.sq_off.array + params.sq_entries * sizeof (unsigned);
  iouring_cq_ring_size = params.cq_off.cqes  + params.cq_entries * sizeof (struct io_uring_cqe);
  iouring_sqes_size    =                       params.sq_entries * sizeof (struct io_uring_sqe);

  iouring_sq_ring = mmap (0, iouring_sq_ring_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, iouring_fd, IORING_OFF_SQ_RING);
  iouring_cq_ring = mmap (0, iouring_cq_ring_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, iouring_fd, IORING_OFF_CQ_RING);
  iouring_sqes    = mmap (0, iouring_sqes_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, iouring_fd, IORING_OFF_SQES);

  if (iouring_sq_ring == MAP_FAILED || iouring_cq_ring == MAP_FAILED || iouring_sqes == MAP_FAILED)
    return -1;

  iouring_sq_head         = params.sq_off.head;
  iouring_sq_tail         = params.sq_off.tail;
  iouring_sq_ring_mask    = params.sq_off.ring_mask;
  iouring_sq_ring_entries = params.sq_off.ring_entries;
  iouring_sq_flags        = params.sq_off.flags;
  iouring_sq_dropped      = params.sq_off.dropped;
  iouring_sq_array        = params.sq_off.array;

  iouring_cq_head         = params.cq_off.head;
  iouring_cq_tail         = params.cq_off.tail;
  iouring_cq_ring_mask    = params.cq_off.ring_mask;
  iouring_cq_ring_entries = params.cq_off.ring_entries;
  iouring_cq_overflow     = params.cq_off.overflow;
  iouring_cq_cqes         = params.cq_off.cqes;

  iouring_tfd = timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);

  if (iouring_tfd < 0)
    return iouring_tfd;

  iouring_tfd_to = EV_TSTAMP_HUGE;

  return 0;
}

ecb_cold
static void
iouring_fork (EV_P)
{
  iouring_internal_destroy (EV_A);

  while (iouring_internal_init (EV_A) < 0)
    ev_syserr ("(libev) io_uring_setup");

  fd_rearm_all (EV_A);

  ev_io_stop  (EV_A_ &iouring_tfd_w);
  ev_io_set   (EV_A_ &iouring_tfd_w, iouring_tfd, EV_READ);
  ev_io_start (EV_A_ &iouring_tfd_w);
}

/*****************************************************************************/

static void
iouring_modify (EV_P_ int fd, int oev, int nev)
{
  if (oev)
    {
      /* we assume the sqe's are all "properly" initialised */
      struct io_uring_sqe *sqe = iouring_sqe_get (EV_A);
      sqe->opcode    = IORING_OP_POLL_REMOVE;
      sqe->fd        = fd;
      /* Jens Axboe notified me that user_data is not what is documented, but is
       * some kind of unique ID that has to match, otherwise the request cannot
       * be removed. Since we don't *really* have that, we pass in the old
       * generation counter - if that fails, too bad, it will hopefully be removed
       * at close time and then be ignored. */
      sqe->addr      = (uint32_t)fd | ((__u64)(uint32_t)anfds [fd].egen << 32);
      sqe->user_data = (uint64_t)-1;
      iouring_sqe_submit (EV_A_ sqe);

      /* increment generation counter to avoid handling old events */
      ++anfds [fd].egen;
    }

  if (nev)
    {
      struct io_uring_sqe *sqe = iouring_sqe_get (EV_A);
      sqe->opcode      = IORING_OP_POLL_ADD;
      sqe->fd          = fd;
      sqe->addr        = 0;
      sqe->user_data   = (uint32_t)fd | ((__u64)(uint32_t)anfds [fd].egen << 32);
      sqe->poll_events =
        (nev & EV_READ ? POLLIN : 0)
        | (nev & EV_WRITE ? POLLOUT : 0);
      iouring_sqe_submit (EV_A_ sqe);
    }
}

inline_size
void
iouring_tfd_update (EV_P_ ev_tstamp timeout)
{
  ev_tstamp tfd_to = mn_now + timeout;

  /* we assume there will be many iterations per timer change, so
   * we only re-set the timerfd when we have to because its expiry
   * is too late.
   */
  if (ecb_expect_false (tfd_to < iouring_tfd_to))
    {
       struct itimerspec its;

       iouring_tfd_to = tfd_to;
       EV_TS_SET (its.it_interval, 0.);
       EV_TS_SET (its.it_value, tfd_to);

       if (timerfd_settime (iouring_tfd, TFD_TIMER_ABSTIME, &its, 0) < 0)
         assert (("libev: iouring timerfd_settime failed", 0));
    }
}

inline_size
void
iouring_process_cqe (EV_P_ struct io_uring_cqe *cqe)
{
  int      fd  = cqe->user_data & 0xffffffffU;
  uint32_t gen = cqe->user_data >> 32;
  int      res = cqe->res;

  /* user_data -1 is a remove that we are not atm. interested in */
  if (cqe->user_data == (uint64_t)-1)
    return;

  assert (("libev: io_uring fd must be in-bounds", fd >= 0 && fd < anfdmax));

  /* documentation lies, of course. the result value is NOT like
   * normal syscalls, but like linux raw syscalls, i.e. negative
   * error numbers. fortunate, as otherwise there would be no way
   * to get error codes at all. still, why not document this?
   */

  /* ignore event if generation doesn't match */
  /* other than skipping removal events, */
  /* this should actually be very rare */
  if (ecb_expect_false (gen != (uint32_t)anfds [fd].egen))
    return;

  if (ecb_expect_false (res < 0))
    {
      /*TODO: EINVAL handling (was something failed with this fd)*/

      if (res == -EBADF)
        {
          assert (("libev: event loop rejected bad fd", res != -EBADF));
          fd_kill (EV_A_ fd);
        }
      else
        {
          errno = -res;
          ev_syserr ("(libev) IORING_OP_POLL_ADD");
        }

      return;
    }

  /* feed events, we do not expect or handle POLLNVAL */
  fd_event (
    EV_A_
    fd,
    (res & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
    | (res & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
  );

  /* io_uring is oneshot, so we need to re-arm the fd next iteration */
  /* this also means we usually have to do at least one syscall per iteration */
  anfds [fd].events = 0;
  fd_change (EV_A_ fd, EV_ANFD_REIFY);
}

/* called when the event queue overflows */
ecb_cold
static void
iouring_overflow (EV_P)
{
  /* we have two options, resize the queue (by tearing down
   * everything and recreating it, or living with it
   * and polling.
   * we implement this by resizing the queue, and, if that fails,
   * we just recreate the state on every failure, which
   * kind of is a very inefficient poll.
   * one danger is, due to the bios toward lower fds,
   * we will only really get events for those, so
   * maybe we need a poll() fallback, after all.
   */
  /*EV_CQ_VAR (overflow) = 0;*/ /* need to do this if we keep the state and poll manually */

  fd_rearm_all (EV_A);

  /* we double the size until we hit the hard-to-probe maximum */
  if (!iouring_max_entries)
    {
      iouring_entries <<= 1;
      iouring_fork (EV_A);
    }
  else
    {
      /* we hit the kernel limit, we should fall back to something else.
       * we can either poll() a few times and hope for the best,
       * poll always, or switch to epoll.
       * TODO: is this necessary with newer kernels?
       */

      iouring_internal_destroy (EV_A);

      /* this should make it so that on return, we don't call any uring functions */
      iouring_to_submit = 0;

      for (;;)
        {
          backend = epoll_init (EV_A_ 0);

          if (backend)
            break;

          ev_syserr ("(libev) iouring switch to epoll");
        }
    }
}

/* handle any events in the completion queue, return true if there were any */
static int
iouring_handle_cq (EV_P)
{
  unsigned head, tail, mask;
  
  head = EV_CQ_VAR (head);
  ECB_MEMORY_FENCE_ACQUIRE;
  tail = EV_CQ_VAR (tail);

  if (head == tail)
    return 0;

  /* it can only overflow if we have events, yes, yes? */
  if (ecb_expect_false (EV_CQ_VAR (overflow)))
    {
      iouring_overflow (EV_A);
      return 1;
    }

  mask = EV_CQ_VAR (ring_mask);

  do
    iouring_process_cqe (EV_A_ &EV_CQES [head++ & mask]);
  while (head != tail);

  EV_CQ_VAR (head) = head;
  ECB_MEMORY_FENCE_RELEASE;

  return 1;
}

static void
iouring_poll (EV_P_ ev_tstamp timeout)
{
  /* if we have events, no need for extra syscalls, but we might have to queue events */
  /* we also clar the timeout if there are outstanding fdchanges */
  /* the latter should only happen if both the sq and cq are full, most likely */
  /* because we have a lot of event sources that immediately complete */
  /* TODO: fdchacngecnt is always 0 because fd_reify does not have two buffers yet */
  if (iouring_handle_cq (EV_A) || fdchangecnt)
    timeout = EV_TS_CONST (0.);
  else
    /* no events, so maybe wait for some */
    iouring_tfd_update (EV_A_ timeout);

  /* only enter the kernel if we have something to submit, or we need to wait */
  if (timeout || iouring_to_submit)
    {
      int res = iouring_enter (EV_A_ timeout);

      if (ecb_expect_false (res < 0))
        if (errno == EINTR)
          /* ignore */;
        else if (errno == EBUSY)
          /* cq full, cannot submit - should be rare because we flush the cq first, so simply ignore */;
        else
          ev_syserr ("(libev) iouring setup");
      else
        iouring_handle_cq (EV_A);
    }
}

inline_size
int
iouring_init (EV_P_ int flags)
{
  iouring_entries     = IOURING_INIT_ENTRIES;
  iouring_max_entries = 0;

  if (iouring_internal_init (EV_A) < 0)
    {
      iouring_internal_destroy (EV_A);
      return 0;
    }

  ev_io_init  (&iouring_tfd_w, iouring_tfd_cb, iouring_tfd, EV_READ);
  ev_set_priority (&iouring_tfd_w, EV_MINPRI);
  ev_io_start (EV_A_ &iouring_tfd_w);
  ev_unref (EV_A); /* watcher should not keep loop alive */

  backend_modify = iouring_modify;
  backend_poll   = iouring_poll;

  return EVBACKEND_IOURING;
}

inline_size
void
iouring_destroy (EV_P)
{
  iouring_internal_destroy (EV_A);
}

