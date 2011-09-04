/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include <assert.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


/*
 * Subclass of uv_handle_t. Used for integration of c-ares.
 */
struct uv_ares_action_s {
  UV_HANDLE_FIELDS
  struct uv_req_s ares_req;
  SOCKET sock;
  int read;
  int write;
};


/* default timeout per socket request if ares does not specify value */
/* use 20 sec */
#define ARES_TIMEOUT_MS            20000


/* thread pool callback when socket is signalled */
static void CALLBACK uv_ares_socksignal_tp(void* parameter,
    BOOLEAN timerfired) {
  WSANETWORKEVENTS network_events;
  uv_ares_task_t* sockhandle;
  uv_ares_action_t* selhandle;
  uv_req_t* uv_ares_req;
  uv_loop_t* loop;

  assert(parameter != NULL);

  if (parameter != NULL) {
    sockhandle = (uv_ares_task_t*) parameter;
    loop = sockhandle->loop;

    /* clear socket status for this event */
    /* do not fail if error, thread may run after socket close */
    /* The code assumes that c-ares will write all pending data in the */
    /* callback, unless the socket would block. We can clear the state here */
    /* to avoid unecessary signals. */
    WSAEnumNetworkEvents(sockhandle->sock,
                         sockhandle->h_event,
                         &network_events);

    /* setup new handle */
    selhandle = (uv_ares_action_t*)malloc(sizeof(uv_ares_action_t));
    if (selhandle == NULL) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }
    selhandle->type = UV_ARES_EVENT;
    selhandle->close_cb = NULL;
    selhandle->data = sockhandle->data;
    selhandle->sock = sockhandle->sock;
    selhandle->read =
        (network_events.lNetworkEvents & (FD_READ | FD_CONNECT)) ? 1 : 0;
    selhandle->write =
        (network_events.lNetworkEvents & (FD_WRITE | FD_CONNECT)) ? 1 : 0;

    uv_ares_req = &selhandle->ares_req;
    uv_req_init(loop, uv_ares_req);
    uv_ares_req->type = UV_ARES_EVENT_REQ;
    uv_ares_req->data = selhandle;

    /* post ares needs to called */
    POST_COMPLETION_FOR_REQ(loop, uv_ares_req);
  }
}


/* periodically call ares to check for timeouts */
static void uv_ares_poll(uv_timer_t* handle, int status) {
  uv_loop_t* loop = handle->loop;
  if (loop->ares_chan != NULL && loop->ares_active_sockets > 0) {
    ares_process_fd(loop->ares_chan, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
  }
}


/* callback from ares when socket operation is started */
static void uv_ares_sockstate_cb(void *data, ares_socket_t sock, int read,
    int write) {
  /* look to see if we have a handle for this socket in our list */
  uv_loop_t* loop = (uv_loop_t*) data;
  uv_ares_task_t* uv_handle_ares = uv_find_ares_handle(loop, sock);

  int timeoutms = 0;

  if (read == 0 && write == 0) {
    /* if read and write are 0, cleanup existing data */
    /* The code assumes that c-ares does a callback with read = 0 and */
    /* write = 0 when the socket is closed. After we recieve this we stop */
    /* monitoring the socket. */
    if (uv_handle_ares != NULL) {
      uv_req_t* uv_ares_req;

      uv_handle_ares->h_close_event = CreateEvent(NULL, FALSE, FALSE, NULL);
      /* remove Wait */
      if (uv_handle_ares->h_wait) {
        UnregisterWaitEx(uv_handle_ares->h_wait,
                         uv_handle_ares->h_close_event);
        uv_handle_ares->h_wait = NULL;
      }

      /* detach socket from the event */
      WSAEventSelect(sock, NULL, 0);
      if (uv_handle_ares->h_event != WSA_INVALID_EVENT) {
        WSACloseEvent(uv_handle_ares->h_event);
        uv_handle_ares->h_event = WSA_INVALID_EVENT;
      }
      /* remove handle from list */
      uv_remove_ares_handle(uv_handle_ares);

      /* Post request to cleanup the Task */
      uv_ares_req = &uv_handle_ares->ares_req;
      uv_req_init(loop, uv_ares_req);
      uv_ares_req->type = UV_ARES_CLEANUP_REQ;
      uv_ares_req->data = uv_handle_ares;

      /* post ares done with socket - finish cleanup when all threads done. */
      POST_COMPLETION_FOR_REQ(loop, uv_ares_req);
    } else {
      assert(0);
      uv_fatal_error(ERROR_INVALID_DATA, "ares_SockStateCB");
    }
  } else {
    if (uv_handle_ares == NULL) {
      /* setup new handle */
      /* The code assumes that c-ares will call us when it has an open socket.
        We need to call into c-ares when there is something to read,
        or when it becomes writable. */
      uv_handle_ares = (uv_ares_task_t*)malloc(sizeof(uv_ares_task_t));
      if (uv_handle_ares == NULL) {
        uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
      }
      uv_handle_ares->type = UV_ARES_TASK;
      uv_handle_ares->close_cb = NULL;
      uv_handle_ares->data = loop;
      uv_handle_ares->sock = sock;
      uv_handle_ares->h_wait = NULL;
      uv_handle_ares->flags = 0;

      /* create an event to wait on socket signal */
      uv_handle_ares->h_event = WSACreateEvent();
      if (uv_handle_ares->h_event == WSA_INVALID_EVENT) {
        uv_fatal_error(WSAGetLastError(), "WSACreateEvent");
      }

      /* tie event to socket */
      if (SOCKET_ERROR == WSAEventSelect(sock,
                                         uv_handle_ares->h_event,
                                         FD_READ | FD_WRITE | FD_CONNECT)) {
        uv_fatal_error(WSAGetLastError(), "WSAEventSelect");
      }

      /* add handle to list */
      uv_add_ares_handle(loop, uv_handle_ares);
      uv_ref(loop);

      /*
       * we have a single polling timer for all ares sockets.
       * This is preferred to using ares_timeout. See ares_timeout.c warning.
       * if timer is not running start it, and keep socket count
       */
      if (loop->ares_active_sockets == 0) {
        uv_timer_init(loop, &loop->ares_polling_timer);
        uv_timer_start(&loop->ares_polling_timer, uv_ares_poll, 1000L, 1000L);
      }
      loop->ares_active_sockets++;

      /* specify thread pool function to call when event is signaled */
      if (RegisterWaitForSingleObject(&uv_handle_ares->h_wait,
                                  uv_handle_ares->h_event,
                                  uv_ares_socksignal_tp,
                                  (void*)uv_handle_ares,
                                  INFINITE,
                                  WT_EXECUTEINWAITTHREAD) == 0) {
        uv_fatal_error(GetLastError(), "RegisterWaitForSingleObject");
      }
    } else {
      /* found existing handle.  */
      assert(uv_handle_ares->type == UV_ARES_TASK);
      assert(uv_handle_ares->data != NULL);
      assert(uv_handle_ares->h_event != WSA_INVALID_EVENT);
    }
  }
}


/* called via uv_poll when ares completion port signaled */
void uv_process_ares_event_req(uv_loop_t* loop, uv_ares_action_t* handle,
    uv_req_t* req) {
  ares_process_fd(loop->ares_chan,
                  handle->read ? handle->sock : INVALID_SOCKET,
                  handle->write ?  handle->sock : INVALID_SOCKET);

  /* release handle for select here  */
  free(handle);
}


/* called via uv_poll when ares is finished with socket */
void uv_process_ares_cleanup_req(uv_loop_t* loop, uv_ares_task_t* handle,
    uv_req_t* req) {
  /* check for event complete without waiting */
  unsigned int signaled = WaitForSingleObject(handle->h_close_event, 0);

  if (signaled != WAIT_TIMEOUT) {
    uv_unref(loop);

    /* close event handle and free uv handle memory */
    CloseHandle(handle->h_close_event);
    free(handle);

    /* decrement active count. if it becomes 0 stop polling */
    if (loop->ares_active_sockets > 0) {
      loop->ares_active_sockets--;
      if (loop->ares_active_sockets == 0) {
        uv_close((uv_handle_t*) &loop->ares_polling_timer, NULL);
      }
    }
  } else {
    /* stil busy - repost and try again */
    POST_COMPLETION_FOR_REQ(loop, req);
  }
}


/* set ares SOCK_STATE callback to our handler */
int uv_ares_init_options(uv_loop_t* loop,
                         ares_channel *channelptr,
                         struct ares_options *options,
                         int optmask) {
  int rc;

  /* only allow single init at a time */
  if (loop->ares_chan != NULL) {
    return UV_EALREADY;
  }

  /* set our callback as an option */
  options->sock_state_cb = uv_ares_sockstate_cb;
  options->sock_state_cb_data = loop;
  optmask |= ARES_OPT_SOCK_STATE_CB;

  /* We do the call to ares_init_option for caller. */
  rc = ares_init_options(channelptr, options, optmask);

  /* if success, save channel */
  if (rc == ARES_SUCCESS) {
    loop->ares_chan = *channelptr;
  }

  return rc;
}


/* release memory */
void uv_ares_destroy(uv_loop_t* loop, ares_channel channel) {
  /* only allow destroy if did init */
  if (loop->ares_chan != NULL) {
    ares_destroy(channel);
    loop->ares_chan = NULL;
  }
}
