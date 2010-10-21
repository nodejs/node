/*
 * libev simple C++ wrapper classes
 *
 * Copyright (c) 2007,2008,2010 Marc Alexander Lehmann <libev@schmorp.de>
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

#ifndef EVPP_H__
#define EVPP_H__

#ifdef EV_H
# include EV_H
#else
# include "ev.h"
#endif

#ifndef EV_USE_STDEXCEPT
# define EV_USE_STDEXCEPT 1
#endif

#if EV_USE_STDEXCEPT
# include <stdexcept>
#endif

namespace ev {

  typedef ev_tstamp tstamp;

  enum
  {
    UNDEF    = EV_UNDEF,
    NONE     = EV_NONE,
    READ     = EV_READ,
    WRITE    = EV_WRITE,
    TIMEOUT  = EV_TIMEOUT,
    PERIODIC = EV_PERIODIC,
    SIGNAL   = EV_SIGNAL,
    CHILD    = EV_CHILD,
    STAT     = EV_STAT,
    IDLE     = EV_IDLE,
    CHECK    = EV_CHECK,
    PREPARE  = EV_PREPARE,
    FORK     = EV_FORK,
    ASYNC    = EV_ASYNC,
    EMBED    = EV_EMBED,
#   undef ERROR // some systems stupidly #define ERROR
    ERROR    = EV_ERROR
  };

  enum
  {
    AUTO      = EVFLAG_AUTO,
    NOENV     = EVFLAG_NOENV,
    FORKCHECK = EVFLAG_FORKCHECK,

    SELECT    = EVBACKEND_SELECT,
    POLL      = EVBACKEND_POLL,
    EPOLL     = EVBACKEND_EPOLL,
    KQUEUE    = EVBACKEND_KQUEUE,
    DEVPOLL   = EVBACKEND_DEVPOLL,
    PORT      = EVBACKEND_PORT
  };

  enum
  {
    NONBLOCK = EVLOOP_NONBLOCK,
    ONESHOT  = EVLOOP_ONESHOT
  };

  enum how_t
  {
    ONE = EVUNLOOP_ONE,
    ALL = EVUNLOOP_ALL
  };

  struct bad_loop
#if EV_USE_STDEXCEPT
  : std::runtime_error
#endif
  {
#if EV_USE_STDEXCEPT
    bad_loop ()
    : std::runtime_error ("libev event loop cannot be initialized, bad value of LIBEV_FLAGS?")
    {
    }
#endif
  };

#ifdef EV_AX
#  undef EV_AX
#endif

#ifdef EV_AX_
#  undef EV_AX_
#endif

#if EV_MULTIPLICITY
#  define EV_AX  raw_loop
#  define EV_AX_ raw_loop,
#else
#  define EV_AX
#  define EV_AX_
#endif

  struct loop_ref
  {
    loop_ref (EV_P) throw ()
#if EV_MULTIPLICITY
    : EV_AX (EV_A)
#endif
    {
    }

    bool operator == (const loop_ref &other) const throw ()
    {
#if EV_MULTIPLICITY
      return EV_AX == other.EV_AX;
#else
      return true;
#endif
    }

    bool operator != (const loop_ref &other) const throw ()
    {
#if EV_MULTIPLICITY
      return ! (*this == other);
#else
      return false;
#endif
    }

#if EV_MULTIPLICITY
    bool operator == (const EV_P) const throw ()
    {
      return this->EV_AX == EV_A;
    }

    bool operator != (const EV_P) const throw ()
    {
      return (*this == EV_A);
    }

    operator struct ev_loop * () const throw ()
    {
      return EV_AX;
    }

    operator const struct ev_loop * () const throw ()
    {
      return EV_AX;
    }

    bool is_default () const throw ()
    {
      return EV_AX == ev_default_loop (0);
    }
#endif

    void loop (int flags = 0)
    {
      ev_loop (EV_AX_ flags);
    }

    void unloop (how_t how = ONE) throw ()
    {
      ev_unloop (EV_AX_ how);
    }

    void post_fork () throw ()
    {
#if EV_MULTIPLICITY
      ev_loop_fork (EV_AX);
#else
      ev_default_fork ();
#endif
    }

    unsigned int backend () const throw ()
    {
      return ev_backend (EV_AX);
    }

    tstamp now () const throw ()
    {
      return ev_now (EV_AX);
    }

    void ref () throw ()
    {
      ev_ref (EV_AX);
    }

    void unref () throw ()
    {
      ev_unref (EV_AX);
    }

#if EV_FEATURE_API
    unsigned int iteration () const throw ()
    {
      return ev_iteration (EV_AX);
    }

    unsigned int depth () const throw ()
    {
      return ev_depth (EV_AX);
    }

    void set_io_collect_interval (tstamp interval) throw ()
    {
      ev_set_io_collect_interval (EV_AX_ interval);
    }

    void set_timeout_collect_interval (tstamp interval) throw ()
    {
      ev_set_timeout_collect_interval (EV_AX_ interval);
    }
#endif

    // function callback
    void once (int fd, int events, tstamp timeout, void (*cb)(int, void *), void *arg = 0) throw ()
    {
      ev_once (EV_AX_ fd, events, timeout, cb, arg);
    }

    // method callback
    template<class K, void (K::*method)(int)>
    void once (int fd, int events, tstamp timeout, K *object) throw ()
    {
      once (fd, events, timeout, method_thunk<K, method>, object);
    }

    // default method == operator ()
    template<class K>
    void once (int fd, int events, tstamp timeout, K *object) throw ()
    {
      once (fd, events, timeout, method_thunk<K, &K::operator ()>, object);
    }

    template<class K, void (K::*method)(int)>
    static void method_thunk (int revents, void *arg)
    {
      static_cast<K *>(arg)->*method
        (revents);
    }

    // no-argument method callback
    template<class K, void (K::*method)()>
    void once (int fd, int events, tstamp timeout, K *object) throw ()
    {
      once (fd, events, timeout, method_noargs_thunk<K, method>, object);
    }

    template<class K, void (K::*method)()>
    static void method_noargs_thunk (int revents, void *arg)
    {
      static_cast<K *>(arg)->*method
        ();
    }

    // simpler function callback
    template<void (*cb)(int)>
    void once (int fd, int events, tstamp timeout) throw ()
    {
      once (fd, events, timeout, simpler_func_thunk<cb>);
    }

    template<void (*cb)(int)>
    static void simpler_func_thunk (int revents, void *arg)
    {
      (*cb)
        (revents);
    }

    // simplest function callback
    template<void (*cb)()>
    void once (int fd, int events, tstamp timeout) throw ()
    {
      once (fd, events, timeout, simplest_func_thunk<cb>);
    }

    template<void (*cb)()>
    static void simplest_func_thunk (int revents, void *arg)
    {
      (*cb)
        ();
    }

    void feed_fd_event (int fd, int revents) throw ()
    {
      ev_feed_fd_event (EV_AX_ fd, revents);
    }

    void feed_signal_event (int signum) throw ()
    {
      ev_feed_signal_event (EV_AX_ signum);
    }

#if EV_MULTIPLICITY
    struct ev_loop* EV_AX;
#endif

  };

#if EV_MULTIPLICITY
  struct dynamic_loop : loop_ref
  {

    dynamic_loop (unsigned int flags = AUTO) throw (bad_loop)
    : loop_ref (ev_loop_new (flags))
    {
      if (!EV_AX)
        throw bad_loop ();
    }

    ~dynamic_loop () throw ()
    {
      ev_loop_destroy (EV_AX);
      EV_AX = 0;
    }

  private:

    dynamic_loop (const dynamic_loop &);

    dynamic_loop & operator= (const dynamic_loop &);

  };
#endif

  struct default_loop : loop_ref
  {
    default_loop (unsigned int flags = AUTO) throw (bad_loop)
#if EV_MULTIPLICITY
    : loop_ref (ev_default_loop (flags))
#endif
    {
      if (
#if EV_MULTIPLICITY
          !EV_AX
#else
          !ev_default_loop (flags)
#endif
      )
        throw bad_loop ();
    }

    ~default_loop () throw ()
    {
      ev_default_destroy ();
    }

  private:
    default_loop (const default_loop &);
    default_loop &operator = (const default_loop &);
  };

  inline loop_ref get_default_loop () throw ()
  {
#if EV_MULTIPLICITY
    return ev_default_loop (0);
#else
    return loop_ref ();
#endif
  }

#undef EV_AX
#undef EV_AX_

#undef EV_PX
#undef EV_PX_
#if EV_MULTIPLICITY
#  define EV_PX  loop_ref EV_A
#  define EV_PX_ loop_ref EV_A_
#else
#  define EV_PX
#  define EV_PX_
#endif

  template<class ev_watcher, class watcher>
  struct base : ev_watcher
  {
    #if EV_MULTIPLICITY
      EV_PX;

      // loop set
      void set (EV_P) throw ()
      {
        this->EV_A = EV_A;
      }
    #endif

    base (EV_PX) throw ()
    #if EV_MULTIPLICITY
      : EV_A (EV_A)
    #endif
    {
      ev_init (this, 0);
    }

    void set_ (const void *data, void (*cb)(EV_P_ ev_watcher *w, int revents)) throw ()
    {
      this->data = (void *)data;
      ev_set_cb (static_cast<ev_watcher *>(this), cb);
    }

    // function callback
    template<void (*function)(watcher &w, int)>
    void set (void *data = 0) throw ()
    {
      set_ (data, function_thunk<function>);
    }

    template<void (*function)(watcher &w, int)>
    static void function_thunk (EV_P_ ev_watcher *w, int revents)
    {
      function
        (*static_cast<watcher *>(w), revents);
    }

    // method callback
    template<class K, void (K::*method)(watcher &w, int)>
    void set (K *object) throw ()
    {
      set_ (object, method_thunk<K, method>);
    }

    // default method == operator ()
    template<class K>
    void set (K *object) throw ()
    {
      set_ (object, method_thunk<K, &K::operator ()>);
    }

    template<class K, void (K::*method)(watcher &w, int)>
    static void method_thunk (EV_P_ ev_watcher *w, int revents)
    {
      (static_cast<K *>(w->data)->*method)
        (*static_cast<watcher *>(w), revents);
    }

    // no-argument callback
    template<class K, void (K::*method)()>
    void set (K *object) throw ()
    {
      set_ (object, method_noargs_thunk<K, method>);
    }

    template<class K, void (K::*method)()>
    static void method_noargs_thunk (EV_P_ ev_watcher *w, int revents)
    {
      (static_cast<K *>(w->data)->*method)
        ();
    }

    void operator ()(int events = EV_UNDEF)
    {
      return
        ev_cb (static_cast<ev_watcher *>(this))
          (static_cast<ev_watcher *>(this), events);
    }

    bool is_active () const throw ()
    {
      return ev_is_active (static_cast<const ev_watcher *>(this));
    }

    bool is_pending () const throw ()
    {
      return ev_is_pending (static_cast<const ev_watcher *>(this));
    }

    void feed_event (int revents) throw ()
    {
      ev_feed_event (EV_A_ static_cast<const ev_watcher *>(this), revents);
    }
  };

  inline tstamp now () throw ()
  {
    return ev_time ();
  }

  inline void delay (tstamp interval) throw ()
  {
    ev_sleep (interval);
  }

  inline int version_major () throw ()
  {
    return ev_version_major ();
  }

  inline int version_minor () throw ()
  {
    return ev_version_minor ();
  }

  inline unsigned int supported_backends () throw ()
  {
    return ev_supported_backends ();
  }

  inline unsigned int recommended_backends () throw ()
  {
    return ev_recommended_backends ();
  }

  inline unsigned int embeddable_backends () throw ()
  {
    return ev_embeddable_backends ();
  }

  inline void set_allocator (void *(*cb)(void *ptr, long size)) throw ()
  {
    ev_set_allocator (cb);
  }

  inline void set_syserr_cb (void (*cb)(const char *msg)) throw ()
  {
    ev_set_syserr_cb (cb);
  }

  #if EV_MULTIPLICITY
    #define EV_CONSTRUCT(cppstem,cstem)	                                                \
      (EV_PX = get_default_loop ()) throw ()                                            \
        : base<ev_ ## cstem, cppstem> (EV_A)                                            \
      {                                                                                 \
      }
  #else
    #define EV_CONSTRUCT(cppstem,cstem)                                                 \
      () throw ()                                                                       \
      {                                                                                 \
      }
  #endif

  /* using a template here would require quite a bit more lines,
   * so a macro solution was chosen */
  #define EV_BEGIN_WATCHER(cppstem,cstem)	                                        \
                                                                                        \
  struct cppstem : base<ev_ ## cstem, cppstem>                                          \
  {                                                                                     \
    void start () throw ()                                                              \
    {                                                                                   \
      ev_ ## cstem ## _start (EV_A_ static_cast<ev_ ## cstem *>(this));                 \
    }                                                                                   \
                                                                                        \
    void stop () throw ()                                                               \
    {                                                                                   \
      ev_ ## cstem ## _stop (EV_A_ static_cast<ev_ ## cstem *>(this));                  \
    }                                                                                   \
                                                                                        \
    cppstem EV_CONSTRUCT(cppstem,cstem)                                                 \
                                                                                        \
    ~cppstem () throw ()                                                                \
    {                                                                                   \
      stop ();                                                                          \
    }                                                                                   \
                                                                                        \
    using base<ev_ ## cstem, cppstem>::set;                                             \
                                                                                        \
  private:                                                                              \
                                                                                        \
    cppstem (const cppstem &o);                                                         \
                                                                                        \
    cppstem &operator =(const cppstem &o);                                              \
                                                                                        \
  public:

  #define EV_END_WATCHER(cppstem,cstem)	                                                \
  };

  EV_BEGIN_WATCHER (io, io)
    void set (int fd, int events) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_io_set (static_cast<ev_io *>(this), fd, events);
      if (active) start ();
    }

    void set (int events) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_io_set (static_cast<ev_io *>(this), fd, events);
      if (active) start ();
    }

    void start (int fd, int events) throw ()
    {
      set (fd, events);
      start ();
    }
  EV_END_WATCHER (io, io)

  EV_BEGIN_WATCHER (timer, timer)
    void set (ev_tstamp after, ev_tstamp repeat = 0.) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_timer_set (static_cast<ev_timer *>(this), after, repeat);
      if (active) start ();
    }

    void start (ev_tstamp after, ev_tstamp repeat = 0.) throw ()
    {
      set (after, repeat);
      start ();
    }

    void again () throw ()
    {
      ev_timer_again (EV_A_ static_cast<ev_timer *>(this));
    }

    ev_tstamp remaining ()
    {
      return ev_timer_remaining (EV_A_ static_cast<ev_timer *>(this));
    }
  EV_END_WATCHER (timer, timer)

  #if EV_PERIODIC_ENABLE
  EV_BEGIN_WATCHER (periodic, periodic)
    void set (ev_tstamp at, ev_tstamp interval = 0.) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_periodic_set (static_cast<ev_periodic *>(this), at, interval, 0);
      if (active) start ();
    }

    void start (ev_tstamp at, ev_tstamp interval = 0.) throw ()
    {
      set (at, interval);
      start ();
    }

    void again () throw ()
    {
      ev_periodic_again (EV_A_ static_cast<ev_periodic *>(this));
    }
  EV_END_WATCHER (periodic, periodic)
  #endif

  #if EV_SIGNAL_ENABLE
  EV_BEGIN_WATCHER (sig, signal)
    void set (int signum) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_signal_set (static_cast<ev_signal *>(this), signum);
      if (active) start ();
    }

    void start (int signum) throw ()
    {
      set (signum);
      start ();
    }
  EV_END_WATCHER (sig, signal)
  #endif

  #if EV_CHILD_ENABLE
  EV_BEGIN_WATCHER (child, child)
    void set (int pid, int trace = 0) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_child_set (static_cast<ev_child *>(this), pid, trace);
      if (active) start ();
    }

    void start (int pid, int trace = 0) throw ()
    {
      set (pid, trace);
      start ();
    }
  EV_END_WATCHER (child, child)
  #endif

  #if EV_STAT_ENABLE
  EV_BEGIN_WATCHER (stat, stat)
    void set (const char *path, ev_tstamp interval = 0.) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_stat_set (static_cast<ev_stat *>(this), path, interval);
      if (active) start ();
    }

    void start (const char *path, ev_tstamp interval = 0.) throw ()
    {
      stop ();
      set (path, interval);
      start ();
    }

    void update () throw ()
    {
      ev_stat_stat (EV_A_ static_cast<ev_stat *>(this));
    }
  EV_END_WATCHER (stat, stat)
  #endif

  #if EV_IDLE_ENABLE
  EV_BEGIN_WATCHER (idle, idle)
    void set () throw () { }
  EV_END_WATCHER (idle, idle)
  #endif

  #if EV_PREPARE_ENABLE
  EV_BEGIN_WATCHER (prepare, prepare)
    void set () throw () { }
  EV_END_WATCHER (prepare, prepare)
  #endif

  #if EV_CHECK_ENABLE
  EV_BEGIN_WATCHER (check, check)
    void set () throw () { }
  EV_END_WATCHER (check, check)
  #endif

  #if EV_EMBED_ENABLE
  EV_BEGIN_WATCHER (embed, embed)
    void set (struct ev_loop *embedded_loop) throw ()
    {
      int active = is_active ();
      if (active) stop ();
      ev_embed_set (static_cast<ev_embed *>(this), embedded_loop);
      if (active) start ();
    }

    void start (struct ev_loop *embedded_loop) throw ()
    {
      set (embedded_loop);
      start ();
    }

    void sweep ()
    {
      ev_embed_sweep (EV_A_ static_cast<ev_embed *>(this));
    }
  EV_END_WATCHER (embed, embed)
  #endif

  #if EV_FORK_ENABLE
  EV_BEGIN_WATCHER (fork, fork)
    void set () throw () { }
  EV_END_WATCHER (fork, fork)
  #endif

  #if EV_ASYNC_ENABLE
  EV_BEGIN_WATCHER (async, async)
    void send () throw ()
    {
      ev_async_send (EV_A_ static_cast<ev_async *>(this));
    }

    bool async_pending () throw ()
    {
      return ev_async_pending (static_cast<ev_async *>(this));
    }
  EV_END_WATCHER (async, async)
  #endif

  #undef EV_PX
  #undef EV_PX_
  #undef EV_CONSTRUCT
  #undef EV_BEGIN_WATCHER
  #undef EV_END_WATCHER
}

#endif

