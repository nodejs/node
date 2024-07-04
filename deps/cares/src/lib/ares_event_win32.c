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

/* Uses an anonymous union */
#if defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wc11-extensions"
#endif

#include "ares_private.h"
#include "ares_event.h"
#include "ares_event_win32.h"
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#if defined(USE_WINSOCK)

/* IMPLEMENTATION NOTES
 * ====================
 *
 * This implementation uses some undocumented functionality within Windows for
 * monitoring sockets. The Ancillary Function Driver (AFD) is the low level
 * implementation that Winsock2 sits on top of.  Winsock2 unfortunately does
 * not expose the equivalent of epoll() or kqueue(), but it is possible to
 * access AFD directly and use along with IOCP to simulate the functionality.
 * We want to use IOCP if possible as it gives us the ability to monitor more
 * than just sockets (WSAPoll is not an option), and perform arbitrary callbacks
 * which means we can hook in non-socket related events.
 *
 * The information for this implementation was gathered from "wepoll" and
 * "libuv" which both use slight variants on this, but this implementation
 * doesn't directly follow either methodology.
 *
 * Initialization:
 *   1. Dynamically load the NtDeviceIoControlFile and NtCancelIoFileEx internal
 *      symbols from ntdll.dll.  These functions are used to submit the AFD POLL
 *      request and to cancel a prior request, respectively.
 *   2. Create an IO Completion Port base handle via CreateIoCompletionPort()
 *      that all socket events will be delivered through.
 *   3. Create a callback to be used to be able to interrupt waiting for IOCP
 *      events, this may be called for allowing enqueuing of additional socket
 *      events or removing socket events. PostQueuedCompletionStatus() is the
 *      obvious choice.  Use the same container structure as used with a Socket
 *      but tagged indicating it is not as the CompletionKey (important!).
 *
 * Socket Add:
 *   1. Create/Allocate a container for holding metadata about a socket:
 *      - SOCKET base_socket;
 *      - SOCKET peer_socket;
 *      - OVERLAPPED overlapped; -- Used by AFD POLL
 *      - AFD_POLL_INFO afd_poll_info; -- Used by AFD POLL
 *   2. Call WSAIoctl(..., SIO_BASE_HANDLE, ...) to unwrap the SOCKET and get
 *      the "base socket" we can use for polling.  It appears this may fail so
 *      we should call WSAIoctl(..., SIO_BSP_HANDLE_POLL, ...) as a fallback.
 *   3. The SOCKET handle we have is most likely not capable of supporting
 *      OVERLAPPED, and we need to have a way to unbind a socket from IOCP
 *      (which is done via a simple closesocket()) so we need to duplicate the
 *      "base socket" using WSADuplicateSocketW() followed by
 *      WSASocketW(..., WSA_FLAG_OVERLAPPED) to create this "peer socket" for
 *      submitting AFD POLL requests.
 *   4. Bind to IOCP using CreateIoCompletionPort() referencing the "peer
 *      socket" and the base IOCP handle from "Initialization".  Use the
 *      pointer to the socket container as the "CompletionKey" which will be
 *      returned when an event occurs.
 *   5. Submit AFD POLL request (see "AFD POLL Request" section)
 *
 * Socket Delete:
 *   1. Call "AFD Poll Cancel" (see Section of same name)
 *   2. If a cancel was requested (not bypassed due to no events, etc), tag the
 *      "container" for the socket as pending delete, and when the next IOCP
 *      event for the socket is dequeued, cleanup.
 *   3. Otherwise, call closesocket(peer_socket) then free() the container
 *      which will officially delete it.
 *   NOTE: Deferring delete may be completely unnecessary.  In theory closing
 *         the peer_socket() should guarantee no additional events will be
 *         delivered.  But maybe if there's a pending event that hasn't been
 *         read yet but already trigggered it would be an issue, so this is
 *         "safer" unless we can prove its not necessary.
 *
 * Socket Modify:
 *   1. Call "AFD Poll Cancel" (see Section of same name)
 *   2. If a cancel was not enqueued because there is no pending request,
 *      submit AFD POLL request (see "AFD POLL Request" section), otherwise
 *      defer until next socket event.
 *
 * Event Wait:
 *   1. Call GetQueuedCompletionStatusEx() with the base IOCP handle, a
 *      stack allocated array of OVERLAPPED_ENTRY's, and an appropriate
 *      timeout.
 *   2. Iterate across returned events, the CompletionKey is a pointer to the
 *      container registered with CreateIoCompletionPort() or
 *      PostQueuedCompletionStatus()
 *   3. If object indicates it is pending delete, go ahead and
 *      closesocket(peer_socket) and free() the container. Go to the next event.
 *   4. Submit AFD POLL Request (see "AFD POLL Request"). We must re-enable
 *      the request each time we receive a response, it is not persistent.
 *   5. Notify of any events received as indicated in the AFD_POLL_INFO
 *      Handles[0].Events (NOTE: check NumberOfHandles first, make sure it is
 *      > 0, otherwise we might not have events such as if our last request
 *      was cancelled).
 *
 * AFD Poll Request:
 *   1. Initialize the AFD_POLL_INFO structure:
 *      Exclusive         = TRUE; // Auto cancel duplicates for same socket
 *      NumberOfHandles   = 1;
 *      Timeout.QuadPart  = LLONG_MAX;
 *      Handles[0].Handle = (HANDLE)base_socket;
 *      Handles[0].Status = 0;
 *      Handles[0].Events = ... set as appropriate AFD_POLL_RECEIVE, etc;
 *   2. Zero out the OVERLAPPED structure
 *   3. Create an IO_STATUS_BLOCK pointer (iosb) and set it to the address of
 *      the OVERLAPPED "Internal" member.
 *   4. Set the "Status" member of IO_STATUS_BLOCK to STATUS_PENDING
 *   5. Call
 *      NtDeviceIoControlFile((HANDLE)peer_socket, NULL, NULL, &overlapped,
 *                            iosb, IOCTL_AFD_POLL
 *                            &afd_poll_info, sizeof(afd_poll_info),
 *                            &afd_poll_info, sizeof(afd_poll_info));
 *   NOTE: Its not clear to me if the IO_STATUS_BLOCK pointing to OVERLAPPED
 *         is for efficiency or if its a requirement for AFD.  This is what
 *         libuv does, so I'm doing it here too.
 *
 * AFD Poll Cancel:
 *   1. Check to see if the IO_STATUS_BLOCK "Status" member for the socket
 *      is still STATUS_PENDING, if not, no cancel request is necessary.
 *   2. Call
 *      NtCancelIoFileEx((HANDLE)peer_socket, iosb, &temp_iosb);
 *
 *
 * References:
 *   - https://github.com/piscisaureus/wepoll/
 *   - https://github.com/libuv/libuv/
 */

typedef struct {
  /* Dynamically loaded symbols */
  NtDeviceIoControlFile_t NtDeviceIoControlFile;
  NtCancelIoFileEx_t      NtCancelIoFileEx;

  /* Implementation details */
  HANDLE                  iocp_handle;
} ares_evsys_win32_t;

typedef struct {
  /*! Pointer to parent event container */
  ares_event_t *event;
  /*! Socket passed in to monitor */
  SOCKET        socket;
  /*! Base socket derived from provided socket */
  SOCKET        base_socket;
  /*! New socket (duplicate base_socket handle) supporting OVERLAPPED operation
   */
  SOCKET        peer_socket;
  /*! Structure for submitting AFD POLL requests (Internals!) */
  AFD_POLL_INFO afd_poll_info;
  /*! Overlapped structure submitted with AFD POLL requests and returned with
   * IOCP results */
  OVERLAPPED    overlapped;
} ares_evsys_win32_eventdata_t;

static void ares_iocpevent_signal(const ares_event_t *event)
{
  ares_event_thread_t *e  = event->e;
  ares_evsys_win32_t  *ew = e->ev_sys_data;

  if (e == NULL) {
    return;
  }

  PostQueuedCompletionStatus(ew->iocp_handle, 0, (ULONG_PTR)event->data, NULL);
}

static void ares_iocpevent_cb(ares_event_thread_t *e, ares_socket_t fd,
                              void *data, ares_event_flags_t flags)
{
  (void)e;
  (void)data;
  (void)fd;
  (void)flags;
}

static ares_event_t *ares_iocpevent_create(ares_event_thread_t *e)
{
  ares_event_t *event = NULL;
  ares_status_t status;

  status =
    ares_event_update(&event, e, ARES_EVENT_FLAG_OTHER, ares_iocpevent_cb,
                      ARES_SOCKET_BAD, NULL, NULL, ares_iocpevent_signal);
  if (status != ARES_SUCCESS) {
    return NULL;
  }

  return event;
}

static void ares_evsys_win32_destroy(ares_event_thread_t *e)
{
  ares_evsys_win32_t *ew = NULL;

  if (e == NULL) {
    return;
  }

  ew = e->ev_sys_data;
  if (ew == NULL) {
    return;
  }

  if (ew->iocp_handle != NULL) {
    CloseHandle(ew->iocp_handle);
  }

  ares_free(ew);
  e->ev_sys_data = NULL;
}

static ares_bool_t ares_evsys_win32_init(ares_event_thread_t *e)
{
  ares_evsys_win32_t *ew = NULL;
  HMODULE             ntdll;

  ew = ares_malloc_zero(sizeof(*ew));
  if (ew == NULL) {
    return ARES_FALSE;
  }

  e->ev_sys_data = ew;

  /* All apps should have ntdll.dll already loaded, so just get a handle to
   * this */
  ntdll = GetModuleHandleA("ntdll.dll");
  if (ntdll == NULL) {
    goto fail;
  }

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
/* Without the (void *) cast we get:
 *  warning: cast between incompatible function types from 'FARPROC' {aka 'long long int (*)()'} to 'NTSTATUS (*)(...)'} [-Wcast-function-type]
 * but with it we get:
 *   warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
 * look unsolvable short of killing the warning.
 */
#endif


  /* Load Internal symbols not typically accessible */
  ew->NtDeviceIoControlFile = (NtDeviceIoControlFile_t)(void *)GetProcAddress(
    ntdll, "NtDeviceIoControlFile");
  ew->NtCancelIoFileEx =
    (NtCancelIoFileEx_t)(void *)GetProcAddress(ntdll, "NtCancelIoFileEx");

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

  if (ew->NtCancelIoFileEx == NULL || ew->NtDeviceIoControlFile == NULL) {
    goto fail;
  }

  ew->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (ew->iocp_handle == NULL) {
    goto fail;
  }

  e->ev_signal = ares_iocpevent_create(e);
  if (e->ev_signal == NULL) {
    goto fail;
  }

  return ARES_TRUE;

fail:
  ares_evsys_win32_destroy(e);
  return ARES_FALSE;
}

static ares_socket_t ares_evsys_win32_basesocket(ares_socket_t socket)
{
  while (1) {
    DWORD         bytes; /* Not used */
    ares_socket_t base_socket = ARES_SOCKET_BAD;
    int           rv;

    rv = WSAIoctl(socket, SIO_BASE_HANDLE, NULL, 0, &base_socket,
                  sizeof(base_socket), &bytes, NULL, NULL);
    if (rv != SOCKET_ERROR && base_socket != ARES_SOCKET_BAD) {
      socket = base_socket;
      break;
    }

    /* If we're here, an error occurred */
    if (GetLastError() == WSAENOTSOCK) {
      /* This is critical, exit */
      return ARES_SOCKET_BAD;
    }

    /* Work around known bug in Komodia based LSPs, use ARES_BSP_HANDLE_POLL
     * to retrieve the underlying socket to then loop and get the base socket:
     *  https://docs.microsoft.com/en-us/windows/win32/winsock/winsock-ioctls
     *  https://www.komodia.com/newwiki/index.php?title=Komodia%27s_Redirector_bug_fixes#Version_2.2.2.6
     */
    base_socket = ARES_SOCKET_BAD;
    rv          = WSAIoctl(socket, SIO_BSP_HANDLE_POLL, NULL, 0, &base_socket,
                           sizeof(base_socket), &bytes, NULL, NULL);

    if (rv != SOCKET_ERROR && base_socket != ARES_SOCKET_BAD &&
        base_socket != socket) {
      socket = base_socket;
      continue; /* loop! */
    }

    return ARES_SOCKET_BAD;
  }

  return socket;
}

static ares_bool_t ares_evsys_win32_afd_enqueue(ares_event_t      *event,
                                                ares_event_flags_t flags)
{
  ares_event_thread_t          *e  = event->e;
  ares_evsys_win32_t           *ew = e->ev_sys_data;
  ares_evsys_win32_eventdata_t *ed = event->data;
  NTSTATUS                      status;
  IO_STATUS_BLOCK              *iosb_ptr;

  if (e == NULL || ed == NULL || ew == NULL) {
    return ARES_FALSE;
  }

  /* Enqueue AFD Poll */
  ed->afd_poll_info.Exclusive         = TRUE;
  ed->afd_poll_info.NumberOfHandles   = 1;
  ed->afd_poll_info.Timeout.QuadPart  = LLONG_MAX;
  ed->afd_poll_info.Handles[0].Handle = (HANDLE)ed->base_socket;
  ed->afd_poll_info.Handles[0].Status = 0;
  ed->afd_poll_info.Handles[0].Events = 0;

  if (flags & ARES_EVENT_FLAG_READ) {
    ed->afd_poll_info.Handles[0].Events |=
      (AFD_POLL_RECEIVE | AFD_POLL_DISCONNECT | AFD_POLL_ACCEPT |
       AFD_POLL_ABORT);
  }
  if (flags & ARES_EVENT_FLAG_WRITE) {
    ed->afd_poll_info.Handles[0].Events |=
      (AFD_POLL_SEND | AFD_POLL_CONNECT_FAIL);
  }
  if (flags == 0) {
    ed->afd_poll_info.Handles[0].Events |= AFD_POLL_DISCONNECT;
  }

  memset(&ed->overlapped, 0, sizeof(ed->overlapped));
  iosb_ptr         = (IO_STATUS_BLOCK *)&ed->overlapped.Internal;
  iosb_ptr->Status = STATUS_PENDING;

  status = ew->NtDeviceIoControlFile(
    (HANDLE)ed->peer_socket, NULL, NULL, &ed->overlapped, iosb_ptr,
    IOCTL_AFD_POLL, &ed->afd_poll_info, sizeof(ed->afd_poll_info),
    &ed->afd_poll_info, sizeof(ed->afd_poll_info));
  if (status != STATUS_SUCCESS && status != STATUS_PENDING) {
    return ARES_FALSE;
  }

  return ARES_TRUE;
}

static ares_bool_t ares_evsys_win32_afd_cancel(ares_evsys_win32_eventdata_t *ed)
{
  IO_STATUS_BLOCK    *iosb_ptr;
  IO_STATUS_BLOCK     cancel_iosb;
  ares_evsys_win32_t *ew;
  NTSTATUS            status;

  /* Detached due to destroy */
  if (ed->event == NULL) {
    return ARES_FALSE;
  }

  iosb_ptr = (IO_STATUS_BLOCK *)&ed->overlapped.Internal;
  /* Not pending, nothing to do */
  if (iosb_ptr->Status != STATUS_PENDING) {
    return ARES_FALSE;
  }

  ew = ed->event->e->ev_sys_data;
  status =
    ew->NtCancelIoFileEx((HANDLE)ed->peer_socket, iosb_ptr, &cancel_iosb);

  /* NtCancelIoFileEx() may return STATUS_NOT_FOUND if the operation completed
   * just before calling NtCancelIoFileEx(), but we have not yet received the
   * notifiction (but it should be queued for the next IOCP event).  */
  if (status == STATUS_SUCCESS || status == STATUS_NOT_FOUND) {
    return ARES_TRUE;
  }

  return ARES_FALSE;
}

static void ares_evsys_win32_eventdata_destroy(ares_evsys_win32_eventdata_t *ed)
{
  if (ed == NULL) {
    return;
  }

  if (ed->peer_socket != ARES_SOCKET_BAD) {
    closesocket(ed->peer_socket);
  }

  ares_free(ed);
}

static ares_bool_t ares_evsys_win32_event_add(ares_event_t *event)
{
  ares_event_thread_t          *e  = event->e;
  ares_evsys_win32_t           *ew = e->ev_sys_data;
  ares_evsys_win32_eventdata_t *ed;
  WSAPROTOCOL_INFOW             protocol_info;

  ed              = ares_malloc_zero(sizeof(*ed));
  ed->event       = event;
  ed->socket      = event->fd;
  ed->base_socket = ARES_SOCKET_BAD;
  ed->peer_socket = ARES_SOCKET_BAD;

  /* Likely a signal event, not something we will directly handle.  We create
   * the ares_evsys_win32_eventdata_t as the placeholder to use as the
   * IOCP Completion Key */
  if (ed->socket == ARES_SOCKET_BAD) {
    event->data = ed;
    return ARES_TRUE;
  }

  ed->base_socket = ares_evsys_win32_basesocket(ed->socket);
  if (ed->base_socket == ARES_SOCKET_BAD) {
    ares_evsys_win32_eventdata_destroy(ed);
    return ARES_FALSE;
  }

  /* Create a peer socket that supports OVERLAPPED so we can use IOCP on the
   * socket handle */
  if (WSADuplicateSocketW(ed->base_socket, GetCurrentProcessId(),
                          &protocol_info) != 0) {
    ares_evsys_win32_eventdata_destroy(ed);
    return ARES_FALSE;
  }

  ed->peer_socket =
    WSASocketW(protocol_info.iAddressFamily, protocol_info.iSocketType,
               protocol_info.iProtocol, &protocol_info, 0, WSA_FLAG_OVERLAPPED);
  if (ed->peer_socket == ARES_SOCKET_BAD) {
    ares_evsys_win32_eventdata_destroy(ed);
    return ARES_FALSE;
  }

  SetHandleInformation((HANDLE)ed->peer_socket, HANDLE_FLAG_INHERIT, 0);

  if (CreateIoCompletionPort((HANDLE)ed->peer_socket, ew->iocp_handle,
                             (ULONG_PTR)ed, 0) == NULL) {
    ares_evsys_win32_eventdata_destroy(ed);
    return ARES_FALSE;
  }

  event->data = ed;

  if (!ares_evsys_win32_afd_enqueue(event, event->flags)) {
    event->data = NULL;
    ares_evsys_win32_eventdata_destroy(ed);
    return ARES_FALSE;
  }

  return ARES_TRUE;
}

static void ares_evsys_win32_event_del(ares_event_t *event)
{
  ares_evsys_win32_eventdata_t *ed = event->data;
  ares_event_thread_t          *e  = event->e;

  if (event->fd == ARES_SOCKET_BAD || !e->isup || ed == NULL ||
      !ares_evsys_win32_afd_cancel(ed)) {
    /* Didn't need to enqueue a cancellation, for one of these reasons:
     *  - Not an IOCP socket
     *  - This is during shutdown of the event thread, no more signals can be
     *    delivered.
     *  - It has been determined there is no AFD POLL queued currently for the
     *    socket.
     */
    ares_evsys_win32_eventdata_destroy(ed);
    event->data = NULL;
  } else {
    /* Detach from event, so when the cancel event comes through,
     * it will clean up */
    ed->event   = NULL;
    event->data = NULL;
  }
}

static void ares_evsys_win32_event_mod(ares_event_t      *event,
                                       ares_event_flags_t new_flags)
{
  ares_evsys_win32_eventdata_t *ed = event->data;

  /* Not for us */
  if (event->fd == ARES_SOCKET_BAD || ed == NULL) {
    return;
  }

  /* Try to cancel any current outstanding poll, if one is not running,
   * go ahead and queue it up */
  if (!ares_evsys_win32_afd_cancel(ed)) {
    ares_evsys_win32_afd_enqueue(event, new_flags);
  }
}

static size_t ares_evsys_win32_wait(ares_event_thread_t *e,
                                    unsigned long        timeout_ms)
{
  ares_evsys_win32_t *ew = e->ev_sys_data;
  OVERLAPPED_ENTRY    entries[16];
  ULONG               nentries = sizeof(entries) / sizeof(*entries);
  BOOL                status;
  size_t              i;
  size_t              cnt = 0;

  status = GetQueuedCompletionStatusEx(
    ew->iocp_handle, entries, nentries, &nentries,
    (timeout_ms == 0) ? INFINITE : (DWORD)timeout_ms, FALSE);

  if (!status) {
    return 0;
  }

  for (i = 0; i < (size_t)nentries; i++) {
    ares_event_flags_t            flags = 0;
    ares_evsys_win32_eventdata_t *ed =
      (ares_evsys_win32_eventdata_t *)entries[i].lpCompletionKey;
    ares_event_t *event = ed->event;

    if (ed->socket == ARES_SOCKET_BAD) {
      /* Some sort of signal event */
      flags = ARES_EVENT_FLAG_OTHER;
    } else {
      /* Process events */
      if (ed->afd_poll_info.NumberOfHandles > 0) {
        if (ed->afd_poll_info.Handles[0].Events &
            (AFD_POLL_RECEIVE | AFD_POLL_DISCONNECT | AFD_POLL_ACCEPT |
             AFD_POLL_ABORT)) {
          flags |= ARES_EVENT_FLAG_READ;
        }
        if (ed->afd_poll_info.Handles[0].Events &
            (AFD_POLL_SEND | AFD_POLL_CONNECT_FAIL)) {
          flags |= ARES_EVENT_FLAG_WRITE;
        }

        /* XXX: Handle ed->afd_poll_info.Handles[0].Events &
         * AFD_POLL_LOCAL_CLOSE */
      }

      if (event == NULL) {
        /* This means we need to cleanup the private event data as we've been
         * detached */
        ares_evsys_win32_eventdata_destroy(ed);
      } else {
        /* Re-enqueue so we can get more events on the socket */
        ares_evsys_win32_afd_enqueue(event, event->flags);
      }
    }

    if (event != NULL && flags != 0) {
      cnt++;
      event->cb(e, event->fd, event->data, flags);
    }
  }

  return cnt;
}

const ares_event_sys_t ares_evsys_win32 = { "win32",
                                            ares_evsys_win32_init,
                                            ares_evsys_win32_destroy,
                                            ares_evsys_win32_event_add,
                                            ares_evsys_win32_event_del,
                                            ares_evsys_win32_event_mod,
                                            ares_evsys_win32_wait };
#endif

#if defined(__clang__)
#  pragma GCC diagnostic pop
#endif
