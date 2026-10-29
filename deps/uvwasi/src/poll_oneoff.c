#include "uv.h"
#include "poll_oneoff.h"
#include "uv_mapping.h"
#include "uvwasi_alloc.h"


static void poll_cb(uv_poll_t* handle, int status, int events) {
  struct uvwasi_poll_oneoff_state_t* state;
  struct uvwasi__poll_fdevent_t* event;

  uv_poll_stop(handle);
  event = uv_handle_get_data((uv_handle_t*) handle);
  event->revents = events;

  if (status != 0)
    event->error = UVWASI_EIO;

  state = uv_loop_get_data(handle->loop);
  state->result++;
}


static void timeout_cb(uv_timer_t* handle) {
  struct uvwasi_poll_oneoff_state_t* state;
  uvwasi_size_t i;

  state = uv_loop_get_data(handle->loop);

  for (i = 0; i < state->handle_cnt; i++)
    uv_poll_stop(&state->poll_handles[i]);
}


uvwasi_errno_t uvwasi__poll_oneoff_state_init(
                                      uvwasi_t* uvwasi,
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_size_t max_fds
                                    ) {
  uvwasi_errno_t err;
  int r;

  if (uvwasi == NULL || state == NULL)
    return UVWASI_EINVAL;

  state->uvwasi = NULL;
  state->timeout = 0;
  state->has_timer = 0;
  state->fdevents = NULL;
  state->poll_handles = NULL;
  state->max_fds = 0;
  state->fdevent_cnt = 0;
  state->handle_cnt = 0;
  state->result = 0;

  r = uv_loop_init(&state->loop);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  if (max_fds > 0) {
    state->fdevents = uvwasi__calloc(uvwasi,
                                     max_fds,
                                     sizeof(*state->fdevents));
    if (state->fdevents == NULL) {
      err = UVWASI_ENOMEM;
      goto error_exit;
    }

    state->poll_handles = uvwasi__calloc(uvwasi,
                                         max_fds,
                                         sizeof(*state->poll_handles));
    if (state->poll_handles == NULL) {
      err = UVWASI_ENOMEM;
      goto error_exit;
    }
  }

  uv_loop_set_data(&state->loop, (void*) state);
  state->uvwasi = uvwasi;
  state->max_fds = max_fds;

  return UVWASI_ESUCCESS;

error_exit:
  uv_loop_close(&state->loop);
  uvwasi__free(state->uvwasi, state->fdevents);
  uvwasi__free(state->uvwasi, state->poll_handles);
  return err;
}


uvwasi_errno_t uvwasi__poll_oneoff_state_cleanup(
                                        struct uvwasi_poll_oneoff_state_t* state
                                      ) {
  struct uvwasi__poll_fdevent_t* event;
  uvwasi_size_t i;
  int r;

  if (state == NULL)
    return UVWASI_EINVAL;

  if (state->has_timer != 0) {
    state->timeout = 0;
    state->has_timer = 0;
    uv_close((uv_handle_t*) &state->timer, NULL);
  }

  for (i = 0; i < state->fdevent_cnt; i++) {
    event = &state->fdevents[i];

    if (event->is_duplicate_fd == 0 && event->wrap != NULL)
      uv_mutex_unlock(&event->wrap->mutex);
  }

  for (i = 0; i < state->handle_cnt; i++)
    uv_close((uv_handle_t*) &state->poll_handles[i], NULL);

  uv_run(&state->loop, UV_RUN_NOWAIT);

  state->max_fds = 0;
  state->fdevent_cnt = 0;
  state->handle_cnt = 0;

  uvwasi__free(state->uvwasi, state->fdevents);
  uvwasi__free(state->uvwasi, state->poll_handles);
  state->fdevents = NULL;
  state->poll_handles = NULL;
  state->uvwasi = NULL;

  r = uv_loop_close(&state->loop);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi__poll_oneoff_state_set_timer(
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_timestamp_t timeout
                                    ) {
  int r;

  if (state == NULL)
    return UVWASI_EINVAL;

  r = uv_timer_init(&state->loop, &state->timer);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  /* Convert WASI timeout from nanoseconds to milliseconds for libuv. */
  state->timeout = timeout / 1000000;
  state->has_timer = 1;
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi__poll_oneoff_state_add_fdevent(
                                      struct uvwasi_poll_oneoff_state_t* state,
                                      uvwasi_subscription_t* subscription
                                    ) {
  struct uvwasi__poll_fdevent_t* event;
  struct uvwasi__poll_fdevent_t* dup;
  uv_poll_t* poll_handle;
  uvwasi_eventtype_t type;
  uvwasi_rights_t rights;
  uvwasi_fd_t fd;
  uvwasi_errno_t err;
  uvwasi_size_t i;
  int r;

  if (state == NULL)
    return UVWASI_EINVAL;

  event = &state->fdevents[state->fdevent_cnt];
  fd = subscription->u.fd_readwrite.fd;
  type = subscription->type;

  if (type == UVWASI_EVENTTYPE_FD_READ) {
    event->events = UV_DISCONNECT | UV_READABLE;
    rights = UVWASI_RIGHT_POLL_FD_READWRITE | UVWASI_RIGHT_FD_READ;
  } else if (type == UVWASI_EVENTTYPE_FD_WRITE) {
    event->events = UV_DISCONNECT | UV_WRITABLE;
    rights = UVWASI_RIGHT_POLL_FD_READWRITE | UVWASI_RIGHT_FD_WRITE;
  } else {
    return UVWASI_EINVAL;
  }

  /* Check if the same file descriptor is already being polled. If so, use the
     wrap and poll handle from the first descriptor. The reasons are that libuv
     does not support polling the same fd more than once at the same time, and
     uvwasi has the fd's mutex locked. */
  event->is_duplicate_fd = 0;
  for (i = 0; i < state->fdevent_cnt; i++) {
    dup = &state->fdevents[i];
    if (dup->wrap->id == fd) {
      event->is_duplicate_fd = 1;
      event->wrap = dup->wrap;
      event->poll_handle = dup->poll_handle;
      err = event->error;
      goto poll_config_done;
    }
  }

  /* Get the file descriptor. If UVWASI_EBADF is returned, continue on, but
     don't do any polling with the handle. */
  err = uvwasi_fd_table_get(state->uvwasi->fds, fd, &event->wrap, rights, 0);
  if (err == UVWASI_EBADF)
    event->wrap = NULL;
  else if (err != UVWASI_ESUCCESS)
    return err;

  if (err == UVWASI_ESUCCESS) {
    /* The fd is valid, so setup the poll handle. */
    poll_handle = &state->poll_handles[state->handle_cnt];
    r = uv_poll_init(&state->loop, poll_handle, event->wrap->fd);

    if (r != 0) {
      /* If uv_poll_init() fails (for example on Windows because only sockets
         are supported), set the error for this event to UVWASI_EBADF, but don't
         do any polling with the handle. */
      uv_mutex_unlock(&event->wrap->mutex);
      return uvwasi__translate_uv_error(r);
    } else {
      r = uv_poll_start(poll_handle,
                        event->events,
                        poll_cb);
      if (r != 0) {
        uv_mutex_unlock(&event->wrap->mutex);
        uv_close((uv_handle_t*) poll_handle, NULL);
        return uvwasi__translate_uv_error(r);
      }

      uv_handle_set_data((uv_handle_t*) poll_handle,
                         (void*) &state->fdevents[state->fdevent_cnt]);
      event->poll_handle = poll_handle;
      state->handle_cnt++;
    }
  }

poll_config_done:
  event->type = type;
  event->userdata = subscription->userdata;
  event->error = err;
  event->revents = 0;
  state->fdevent_cnt++;
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi__poll_oneoff_run(
                                      struct uvwasi_poll_oneoff_state_t* state
                                    ) {
  int r;

  if (state->has_timer == 1) {
    r = uv_timer_start(&state->timer, timeout_cb, state->timeout, 0);
    if (r != 0)
      return uvwasi__translate_uv_error(r);

    if (state->fdevent_cnt > 0)
      uv_unref((uv_handle_t*) &state->timer);
  }

  r = uv_run(&state->loop, UV_RUN_DEFAULT);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  return UVWASI_ESUCCESS;
}
