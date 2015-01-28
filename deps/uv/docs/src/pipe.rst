
.. _pipe:

:c:type:`uv_pipe_t` --- Pipe handle
===================================

Pipe handles provide an abstraction over local domain sockets on Unix and named
pipes on Windows.

:c:type:`uv_pipe_t` is a 'subclass' of :c:type:`uv_stream_t`.


Data types
----------

.. c:type:: uv_pipe_t

    Pipe handle type.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_stream_t` members also apply.


API
---

.. c:function:: int uv_pipe_init(uv_loop_t*, uv_pipe_t* handle, int ipc)

    Initialize a pipe handle. The `ipc` argument is a boolean to indicate if
    this pipe will be used for handle passing between processes.

.. c:function:: int uv_pipe_open(uv_pipe_t*, uv_file file)

    Open an existing file descriptor or HANDLE as a pipe.

    .. versionchanged:: 1.2.1 the file descriptor is set to non-blocking mode.

.. c:function:: int uv_pipe_bind(uv_pipe_t* handle, const char* name)

    Bind the pipe to a file path (Unix) or a name (Windows).

    .. note::
        Paths on Unix get truncated to ``sizeof(sockaddr_un.sun_path)`` bytes, typically between
        92 and 108 bytes.

.. c:function:: void uv_pipe_connect(uv_connect_t* req, uv_pipe_t* handle, const char* name, uv_connect_cb cb)

    Connect to the Unix domain socket or the named pipe.

    .. note::
        Paths on Unix get truncated to ``sizeof(sockaddr_un.sun_path)`` bytes, typically between
        92 and 108 bytes.

.. c:function:: int uv_pipe_getsockname(const uv_pipe_t* handle, char* buffer, size_t* size)

    Get the name of the Unix domain socket or the named pipe.

    A preallocated buffer must be provided. The size parameter holds the length
    of the buffer and it's set to the number of bytes written to the buffer on
    output. If the buffer is not big enough ``UV_ENOBUFS`` will be returned and
    len will contain the required size.

    .. versionchanged:: 1.3.0 the returned length no longer includes the terminating null byte,
                        and the buffer is not null terminated.

.. c:function:: int uv_pipe_getpeername(const uv_pipe_t* handle, char* buffer, size_t* size)

    Get the name of the Unix domain socket or the named pipe to which the handle
    is connected.

    A preallocated buffer must be provided. The size parameter holds the length
    of the buffer and it's set to the number of bytes written to the buffer on
    output. If the buffer is not big enough ``UV_ENOBUFS`` will be returned and
    len will contain the required size.

    .. versionadded:: 1.3.0

.. c:function:: void uv_pipe_pending_instances(uv_pipe_t* handle, int count)

    Set the number of pending pipe instance handles when the pipe server is
    waiting for connections.

    .. note::
        This setting applies to Windows only.

.. c:function:: int uv_pipe_pending_count(uv_pipe_t* handle)
.. c:function:: uv_handle_type uv_pipe_pending_type(uv_pipe_t* handle)

    Used to receive handles over IPC pipes.

    First - call :c:func:`uv_pipe_pending_count`, if it's > 0 then initialize
    a handle of the given `type`, returned by :c:func:`uv_pipe_pending_type`
    and call ``uv_accept(pipe, handle)``.

.. seealso:: The :c:type:`uv_stream_t` API functions also apply.
