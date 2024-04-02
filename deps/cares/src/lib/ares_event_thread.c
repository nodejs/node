/* MIT License
 *
 * Copyright (c) 2024 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"
#include "ares_event.h"

static void ares_event_destroy_cb(void *arg)
{
  ares_event_t *event = arg;
  if (event == NULL) {
    return;
  }

  /* Unregister from the event thread if it was registered with one */
  if (event->e) {
    const ares_event_thread_t *e = event->e;
    e->ev_sys->event_del(event);
    event->e = NULL;
  }

  if (event->free_data_cb && event->data) {
    event->free_data_cb(event->data);
  }

  ares_free(event);
}

/* See if a pending update already exists. We don't want to enqueue multiple
 * updates for the same event handle. Right now this is O(n) based on number
 * of updates already enqueued.  In the future, it might make sense to make
 * this O(1) with a hashtable.
 * NOTE: in some cases a delete then re-add of the same fd, but really pointing
 *       to a different destination can happen due to a quick close of a
 *       connection then creation of a new one.  So we need to look at the
 *       flags and ignore any delete events when finding a match since we
 *       need to process the delete always, it can't be combined with other
 *       updates. */
static ares_event_t *ares_event_update_find(ares_event_thread_t *e,
                                            ares_socket_t fd, const void *data)
{
  ares__llist_node_t *node;

  for (node = ares__llist_node_first(e->ev_updates); node != NULL;
       node = ares__llist_node_next(node)) {
    ares_event_t *ev = ares__llist_node_val(node);

    if (fd != ARES_SOCKET_BAD && fd == ev->fd && ev->flags != 0) {
      return ev;
    }

    if (fd == ARES_SOCKET_BAD && ev->fd == ARES_SOCKET_BAD &&
        data == ev->data && ev->flags != 0) {
      return ev;
    }
  }

  return NULL;
}

ares_status_t ares_event_update(ares_event_t **event, ares_event_thread_t *e,
                                ares_event_flags_t flags, ares_event_cb_t cb,
                                ares_socket_t fd, void *data,
                                ares_event_free_data_t free_data_cb,
                                ares_event_signal_cb_t signal_cb)
{
  ares_event_t *ev = NULL;

  if (e == NULL || cb == NULL) {
    return ARES_EFORMERR;
  }

  if (event != NULL) {
    *event = NULL;
  }

  /* Validate flags */
  if (fd == ARES_SOCKET_BAD) {
    if (flags & (ARES_EVENT_FLAG_READ | ARES_EVENT_FLAG_WRITE)) {
      return ARES_EFORMERR;
    }
    if (!(flags & ARES_EVENT_FLAG_OTHER)) {
      return ARES_EFORMERR;
    }
  } else {
    if (flags & ARES_EVENT_FLAG_OTHER) {
      return ARES_EFORMERR;
    }
  }

  /* That's all the validation we can really do */

  /* See if we have a queued update already */
  ev = ares_event_update_find(e, fd, data);
  if (ev == NULL) {
    /* Allocate a new one */
    ev = ares_malloc_zero(sizeof(*ev));
    if (ev == NULL) {
      return ARES_ENOMEM;
    }

    if (ares__llist_insert_last(e->ev_updates, ev) == NULL) {
      ares_free(ev);
      return ARES_ENOMEM;
    }
  }

  ev->flags = flags;
  ev->fd    = fd;
  if (ev->cb == NULL) {
    ev->cb = cb;
  }
  if (ev->data == NULL) {
    ev->data = data;
  }
  if (ev->free_data_cb == NULL) {
    ev->free_data_cb = free_data_cb;
  }
  if (ev->signal_cb == NULL) {
    ev->signal_cb = signal_cb;
  }

  if (event != NULL) {
    *event = ev;
  }

  return ARES_SUCCESS;
}

static void ares_event_signal(const ares_event_t *event)
{
  if (event == NULL || event->signal_cb == NULL) {
    return;
  }
  event->signal_cb(event);
}

static void ares_event_thread_wake(const ares_event_thread_t *e)
{
  if (e == NULL) {
    return;
  }

  ares_event_signal(e->ev_signal);
}

static void ares_event_thread_process_fd(ares_event_thread_t *e,
                                         ares_socket_t fd, void *data,
                                         ares_event_flags_t flags)
{
  (void)data;

  ares_process_fd(e->channel,
                  (flags & ARES_EVENT_FLAG_READ) ? fd : ARES_SOCKET_BAD,
                  (flags & ARES_EVENT_FLAG_WRITE) ? fd : ARES_SOCKET_BAD);
}

static void ares_event_thread_sockstate_cb(void *data, ares_socket_t socket_fd,
                                           int readable, int writable)
{
  ares_event_thread_t *e     = data;
  ares_event_flags_t   flags = ARES_EVENT_FLAG_NONE;

  if (readable) {
    flags |= ARES_EVENT_FLAG_READ;
  }

  if (writable) {
    flags |= ARES_EVENT_FLAG_WRITE;
  }

  /* Update channel fd */
  ares__thread_mutex_lock(e->mutex);
  ares_event_update(NULL, e, flags, ares_event_thread_process_fd, socket_fd,
                    NULL, NULL, NULL);

  /* Wake the event thread so it properly enqueues any updates */
  ares_event_thread_wake(e);

  ares__thread_mutex_unlock(e->mutex);
}

static void ares_event_process_updates(ares_event_thread_t *e)
{
  ares__llist_node_t *node;

  /* Iterate across all updates and apply to internal list, removing from update
   * list */
  while ((node = ares__llist_node_first(e->ev_updates)) != NULL) {
    ares_event_t *newev = ares__llist_node_claim(node);
    ares_event_t *oldev =
      ares__htable_asvp_get_direct(e->ev_handles, newev->fd);

    /* Adding new */
    if (oldev == NULL) {
      newev->e = e;
      /* Don't try to add a new event if all flags are cleared, that's basically
       * someone trying to delete something already deleted.  Also if it fails
       * to add, cleanup. */
      if (newev->flags == ARES_EVENT_FLAG_NONE ||
          !e->ev_sys->event_add(newev)) {
        newev->e = NULL;
        ares_event_destroy_cb(newev);
      } else {
        ares__htable_asvp_insert(e->ev_handles, newev->fd, newev);
      }
      continue;
    }

    /* Removal request */
    if (newev->flags == ARES_EVENT_FLAG_NONE) {
      /* the callback for the removal will call e->ev_sys->event_del(e, event)
       */
      ares__htable_asvp_remove(e->ev_handles, newev->fd);
      ares_free(newev);
      continue;
    }

    /* Modify request -- only flags can be changed */
    e->ev_sys->event_mod(oldev, newev->flags);
    oldev->flags = newev->flags;
    ares_free(newev);
  }
}

static void *ares_event_thread(void *arg)
{
  ares_event_thread_t *e = arg;
  ares__thread_mutex_lock(e->mutex);

  while (e->isup) {
    struct timeval        tv;
    const struct timeval *tvout;
    unsigned long         timeout_ms = 0; /* 0 = unlimited */

    tvout = ares_timeout(e->channel, NULL, &tv);
    if (tvout != NULL) {
      timeout_ms =
        (unsigned long)((tvout->tv_sec * 1000) + (tvout->tv_usec / 1000) + 1);
    }

    ares_event_process_updates(e);

    /* Don't hold a mutex while waiting on events */
    ares__thread_mutex_unlock(e->mutex);
    e->ev_sys->wait(e, timeout_ms);

    /* Each iteration should do timeout processing */
    if (e->isup) {
      ares_process_fd(e->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    }

    /* Relock before we loop again */
    ares__thread_mutex_lock(e->mutex);
  }

  ares__thread_mutex_unlock(e->mutex);
  return NULL;
}

static void ares_event_thread_destroy_int(ares_event_thread_t *e)
{
  ares__llist_node_t *node;

  /* Wake thread and tell it to shutdown if it exists */
  ares__thread_mutex_lock(e->mutex);
  if (e->isup) {
    e->isup = ARES_FALSE;
    ares_event_thread_wake(e);
  }
  ares__thread_mutex_unlock(e->mutex);

  /* Wait for thread to shutdown */
  if (e->thread) {
    ares__thread_join(e->thread, NULL);
    e->thread = NULL;
  }

  /* Manually free any updates that weren't processed */
  while ((node = ares__llist_node_first(e->ev_updates)) != NULL) {
    ares_event_destroy_cb(ares__llist_node_claim(node));
  }
  ares__llist_destroy(e->ev_updates);
  e->ev_updates = NULL;

  ares__htable_asvp_destroy(e->ev_handles);
  e->ev_handles = NULL;

  if (e->ev_sys->destroy) {
    e->ev_sys->destroy(e);
  }

  ares__thread_mutex_destroy(e->mutex);
  e->mutex = NULL;

  ares_free(e);
}

void ares_event_thread_destroy(ares_channel_t *channel)
{
  ares_event_thread_t *e = channel->sock_state_cb_data;

  if (e == NULL) {
    return;
  }

  ares_event_thread_destroy_int(e);
}

static const ares_event_sys_t *ares_event_fetch_sys(ares_evsys_t evsys)
{
  switch (evsys) {
    case ARES_EVSYS_WIN32:
#if defined(_WIN32)
      return &ares_evsys_win32;
#else
      return NULL;
#endif

    case ARES_EVSYS_EPOLL:
#if defined(HAVE_EPOLL)
      return &ares_evsys_epoll;
#else
      return NULL;
#endif

    case ARES_EVSYS_KQUEUE:
#if defined(HAVE_KQUEUE)
      return &ares_evsys_kqueue;
#else
      return NULL;
#endif

    case ARES_EVSYS_POLL:
#if defined(HAVE_POLL)
      return &ares_evsys_poll;
#else
      return NULL;
#endif

    case ARES_EVSYS_SELECT:
#if defined(HAVE_PIPE)
      return &ares_evsys_select;
#else
      return NULL;
#endif

    /* case ARES_EVSYS_DEFAULT: */
    default:
#if defined(_WIN32)
      return &ares_evsys_win32;
#elif defined(HAVE_KQUEUE)
      return &ares_evsys_kqueue;
#elif defined(HAVE_EPOLL)
      return &ares_evsys_epoll;
#elif defined(HAVE_POLL)
      return &ares_evsys_poll;
#elif defined(HAVE_PIPE)
      return &ares_evsys_select;
#else
      break;
#endif
  }

  return NULL;
}

ares_status_t ares_event_thread_init(ares_channel_t *channel)
{
  ares_event_thread_t *e;

  e = ares_malloc_zero(sizeof(*e));
  if (e == NULL) {
    return ARES_ENOMEM;
  }

  e->mutex = ares__thread_mutex_create();
  if (e->mutex == NULL) {
    ares_event_thread_destroy_int(e);
    return ARES_ENOMEM;
  }

  e->ev_updates = ares__llist_create(NULL);
  if (e->ev_updates == NULL) {
    ares_event_thread_destroy_int(e);
    return ARES_ENOMEM;
  }

  e->ev_handles = ares__htable_asvp_create(ares_event_destroy_cb);
  if (e->ev_handles == NULL) {
    ares_event_thread_destroy_int(e);
    return ARES_ENOMEM;
  }

  e->channel = channel;
  e->isup    = ARES_TRUE;
  e->ev_sys  = ares_event_fetch_sys(channel->evsys);
  if (e->ev_sys == NULL) {
    ares_event_thread_destroy_int(e);
    return ARES_ENOTIMP;
  }

  channel->sock_state_cb      = ares_event_thread_sockstate_cb;
  channel->sock_state_cb_data = e;

  if (!e->ev_sys->init(e)) {
    ares_event_thread_destroy_int(e);
    channel->sock_state_cb      = NULL;
    channel->sock_state_cb_data = NULL;
    return ARES_ESERVFAIL;
  }

  /* Before starting the thread, process any possible events the initialization
   * might have enqueued as we may actually depend on these being valid
   * immediately upon return, which may mean before the thread is fully spawned
   * and processed the list itself. We don't want any sort of race conditions
   * (like the event system wake handle itself). */
  ares_event_process_updates(e);

  /* Start thread */
  if (ares__thread_create(&e->thread, ares_event_thread, e) != ARES_SUCCESS) {
    ares_event_thread_destroy_int(e);
    channel->sock_state_cb      = NULL;
    channel->sock_state_cb_data = NULL;
    return ARES_ESERVFAIL;
  }

  return ARES_SUCCESS;
}
