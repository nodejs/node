/*
 * loop member variable declarations
 *
 * Copyright (c) 2007,2008,2009,2010,2011,2012,2013,2019 Marc Alexander Lehmann <libev@schmorp.de>
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

#define VARx(type,name) VAR(name, type name)

VARx(ev_tstamp, now_floor) /* last time we refreshed rt_time */
VARx(ev_tstamp, mn_now)    /* monotonic clock "now" */
VARx(ev_tstamp, rtmn_diff) /* difference realtime - monotonic time */

/* for reverse feeding of events */
VARx(W *, rfeeds)
VARx(int, rfeedmax)
VARx(int, rfeedcnt)

VAR (pendings, ANPENDING *pendings [NUMPRI])
VAR (pendingmax, int pendingmax [NUMPRI])
VAR (pendingcnt, int pendingcnt [NUMPRI])
VARx(int, pendingpri) /* highest priority currently pending */
VARx(ev_prepare, pending_w) /* dummy pending watcher */

VARx(ev_tstamp, io_blocktime)
VARx(ev_tstamp, timeout_blocktime)

VARx(int, backend)
VARx(int, activecnt) /* total number of active events ("refcount") */
VARx(EV_ATOMIC_T, loop_done)  /* signal by ev_break */

VARx(int, backend_fd)
VARx(ev_tstamp, backend_mintime) /* assumed typical timer resolution */
VAR (backend_modify, void (*backend_modify)(EV_P_ int fd, int oev, int nev))
VAR (backend_poll  , void (*backend_poll)(EV_P_ ev_tstamp timeout))

VARx(ANFD *, anfds)
VARx(int, anfdmax)

VAR (evpipe, int evpipe [2])
VARx(ev_io, pipe_w)
VARx(EV_ATOMIC_T, pipe_write_wanted)
VARx(EV_ATOMIC_T, pipe_write_skipped)

#if !defined(_WIN32) || EV_GENWRAP
VARx(pid_t, curpid)
#endif

VARx(char, postfork)  /* true if we need to recreate kernel state after fork */

#if EV_USE_SELECT || EV_GENWRAP
VARx(void *, vec_ri)
VARx(void *, vec_ro)
VARx(void *, vec_wi)
VARx(void *, vec_wo)
#if defined(_WIN32) || EV_GENWRAP
VARx(void *, vec_eo)
#endif
VARx(int, vec_max)
#endif

#if EV_USE_POLL || EV_GENWRAP
VARx(struct pollfd *, polls)
VARx(int, pollmax)
VARx(int, pollcnt)
VARx(int *, pollidxs) /* maps fds into structure indices */
VARx(int, pollidxmax)
#endif

#if EV_USE_EPOLL || EV_GENWRAP
VARx(struct epoll_event *, epoll_events)
VARx(int, epoll_eventmax)
VARx(int *, epoll_eperms)
VARx(int, epoll_epermcnt)
VARx(int, epoll_epermmax)
#endif

#if EV_USE_LINUXAIO || EV_GENWRAP
VARx(aio_context_t, linuxaio_ctx)
VARx(int, linuxaio_iteration)
VARx(struct aniocb **, linuxaio_iocbps)
VARx(int, linuxaio_iocbpmax)
VARx(struct iocb **, linuxaio_submits)
VARx(int, linuxaio_submitcnt)
VARx(int, linuxaio_submitmax)
VARx(ev_io, linuxaio_epoll_w)
#endif

#if EV_USE_IOURING || EV_GENWRAP
VARx(int, iouring_fd)
VARx(unsigned, iouring_to_submit);
VARx(int, iouring_entries)
VARx(int, iouring_max_entries)
VARx(void *, iouring_sq_ring)
VARx(void *, iouring_cq_ring)
VARx(void *, iouring_sqes)
VARx(uint32_t, iouring_sq_ring_size)
VARx(uint32_t, iouring_cq_ring_size)
VARx(uint32_t, iouring_sqes_size)
VARx(uint32_t, iouring_sq_head)
VARx(uint32_t, iouring_sq_tail)
VARx(uint32_t, iouring_sq_ring_mask)
VARx(uint32_t, iouring_sq_ring_entries)
VARx(uint32_t, iouring_sq_flags)
VARx(uint32_t, iouring_sq_dropped)
VARx(uint32_t, iouring_sq_array)
VARx(uint32_t, iouring_cq_head)
VARx(uint32_t, iouring_cq_tail)
VARx(uint32_t, iouring_cq_ring_mask)
VARx(uint32_t, iouring_cq_ring_entries)
VARx(uint32_t, iouring_cq_overflow)
VARx(uint32_t, iouring_cq_cqes)
VARx(ev_tstamp, iouring_tfd_to)
VARx(int, iouring_tfd)
VARx(ev_io, iouring_tfd_w)
#endif

#if EV_USE_KQUEUE || EV_GENWRAP
VARx(pid_t, kqueue_fd_pid)
VARx(struct kevent *, kqueue_changes)
VARx(int, kqueue_changemax)
VARx(int, kqueue_changecnt)
VARx(struct kevent *, kqueue_events)
VARx(int, kqueue_eventmax)
#endif

#if EV_USE_PORT || EV_GENWRAP
VARx(struct port_event *, port_events)
VARx(int, port_eventmax)
#endif

#if EV_USE_IOCP || EV_GENWRAP
VARx(HANDLE, iocp)
#endif

VARx(int *, fdchanges)
VARx(int, fdchangemax)
VARx(int, fdchangecnt)

VARx(ANHE *, timers)
VARx(int, timermax)
VARx(int, timercnt)

#if EV_PERIODIC_ENABLE || EV_GENWRAP
VARx(ANHE *, periodics)
VARx(int, periodicmax)
VARx(int, periodiccnt)
#endif

#if EV_IDLE_ENABLE || EV_GENWRAP
VAR (idles, ev_idle **idles [NUMPRI])
VAR (idlemax, int idlemax [NUMPRI])
VAR (idlecnt, int idlecnt [NUMPRI])
#endif
VARx(int, idleall) /* total number */

VARx(struct ev_prepare **, prepares)
VARx(int, preparemax)
VARx(int, preparecnt)

VARx(struct ev_check **, checks)
VARx(int, checkmax)
VARx(int, checkcnt)

#if EV_FORK_ENABLE || EV_GENWRAP
VARx(struct ev_fork **, forks)
VARx(int, forkmax)
VARx(int, forkcnt)
#endif

#if EV_CLEANUP_ENABLE || EV_GENWRAP
VARx(struct ev_cleanup **, cleanups)
VARx(int, cleanupmax)
VARx(int, cleanupcnt)
#endif

#if EV_ASYNC_ENABLE || EV_GENWRAP
VARx(EV_ATOMIC_T, async_pending)
VARx(struct ev_async **, asyncs)
VARx(int, asyncmax)
VARx(int, asynccnt)
#endif

#if EV_USE_INOTIFY || EV_GENWRAP
VARx(int, fs_fd)
VARx(ev_io, fs_w)
VARx(char, fs_2625) /* whether we are running in linux 2.6.25 or newer */
VAR (fs_hash, ANFS fs_hash [EV_INOTIFY_HASHSIZE])
#endif

VARx(EV_ATOMIC_T, sig_pending)
#if EV_USE_SIGNALFD || EV_GENWRAP
VARx(int, sigfd)
VARx(ev_io, sigfd_w)
VARx(sigset_t, sigfd_set)
#endif

#if EV_USE_TIMERFD || EV_GENWRAP
VARx(int, timerfd) /* timerfd for time jump detection */
VARx(ev_io, timerfd_w)
#endif

VARx(unsigned int, origflags) /* original loop flags */

#if EV_FEATURE_API || EV_GENWRAP
VARx(unsigned int, loop_count) /* total number of loop iterations/blocks */
VARx(unsigned int, loop_depth) /* #ev_run enters - #ev_run leaves */

VARx(void *, userdata)
/* C++ doesn't support the ev_loop_callback typedef here. stinks. */
VAR (release_cb, void (*release_cb)(EV_P) EV_NOEXCEPT)
VAR (acquire_cb, void (*acquire_cb)(EV_P) EV_NOEXCEPT)
VAR (invoke_cb , ev_loop_callback invoke_cb)
#endif

#undef VARx

