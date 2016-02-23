
.. _poll:

:c:type:`uv_poll_t` --- Poll handle
===================================

Poll handles are used to watch file descriptors for readability and
writability, similar to the purpose of :man:`poll(2)`.

The purpose of poll handles is to enable integrating external libraries that
rely on the event loop to signal it about the socket status changes, like
c-ares or libssh2. Using uv_poll_t for any other purpose is not recommended;
:c:type:`uv_tcp_t`, :c:type:`uv_udp_t`, etc. provide an implementation that is faster and
more scalable than what can be achieved with :c:type:`uv_poll_t`, especially on
Windows.

It is possible that poll handles occasionally signal that a file descriptor is
readable or writable even when it isn't. The user should therefore always
be prepared to handle EAGAIN or equivalent when it attempts to read from or
write to the fd.

It is not okay to have multiple active poll handles for the same socket, this
can cause libuv to busyloop or otherwise malfunction.

The user should not close a file descriptor while it is being polled by an
active poll handle. This can cause the handle to report an error,
but it might also start polling another socket. However the fd can be safely
closed immediately after a call to :c:func:`uv_poll_stop` or :c:func:`uv_close`.

.. note::
    On windows only sockets can be polled with poll handles. On Unix any file
    descriptor that would be accepted by :man:`poll(2)` can be used.


Data types
----------

.. c:type:: uv_poll_t

    Poll handle type.

.. c:type:: void (*uv_poll_cb)(uv_poll_t* handle, int status, int events)

    Type definition for callback passed to :c:func:`uv_poll_start`.

.. c:type:: uv_poll_event

    Poll event types

    ::

        enum uv_poll_event {
            UV_READABLE = 1,
            UV_WRITABLE = 2
        };


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd)

    Initialize the handle using a file descriptor.

    .. versionchanged:: 1.2.2 the file descriptor is set to non-blocking mode.

.. c:function:: int uv_poll_init_socket(uv_loop_t* loop, uv_poll_t* handle, uv_os_sock_t socket)

    Initialize the handle using a socket descriptor. On Unix this is identical
    to :c:func:`uv_poll_init`. On windows it takes a SOCKET handle.

    .. versionchanged:: 1.2.2 the socket is set to non-blocking mode.

.. c:function:: int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb)

    Starts polling the file descriptor. `events` is a bitmask consisting made up
    of UV_READABLE and UV_WRITABLE. As soon as an event is detected the callback
    will be called with `status` set to 0, and the detected events set on the
    `events` field.

    If an error happens while polling, `status` will be < 0 and corresponds
    with one of the UV_E* error codes (see :ref:`errors`). The user should
    not close the socket while the handle is active. If the user does that
    anyway, the callback *may* be called reporting an error status, but this
    is **not** guaranteed.

    .. note::
        Calling :c:func:`uv_poll_start` on a handle that is already active is fine. Doing so
        will update the events mask that is being watched for.

.. c:function:: int uv_poll_stop(uv_poll_t* poll)

    Stop polling the file descriptor, the callback will no longer be called.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
