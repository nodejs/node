
.. _tcp:

:c:type:`uv_tcp_t` --- TCP handle
=================================

TCP handles are used to represent both TCP streams and servers.

:c:type:`uv_tcp_t` is a 'subclass' of :c:type:`uv_stream_t`.


Data types
----------

.. c:type:: uv_tcp_t

    TCP handle type.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_stream_t` members also apply.


API
---

.. c:function:: int uv_tcp_init(uv_loop_t*, uv_tcp_t* handle)

    Initialize the handle.

.. c:function:: int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock)

    Open an existing file descriptor or SOCKET as a TCP handle.

    .. note::
        The user is responsible for setting the file descriptor in
        non-blocking mode.

.. c:function:: int uv_tcp_nodelay(uv_tcp_t* handle, int enable)

    Enable / disable Nagle's algorithm.

.. c:function:: int uv_tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay)

    Enable / disable TCP keep-alive. `delay` is the initial delay in seconds,
    ignored when `enable` is zero.

.. c:function:: int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable)

    Enable / disable simultaneous asynchronous accept requests that are
    queued by the operating system when listening for new TCP connections.

    This setting is used to tune a TCP server for the desired performance.
    Having simultaneous accepts can significantly improve the rate of accepting
    connections (which is why it is enabled by default) but may lead to uneven
    load distribution in multi-process setups.

.. c:function:: int uv_tcp_bind(uv_tcp_t* handle, const struct sockaddr* addr, unsigned int flags)

    Bind the handle to an address and port. `addr` should point to an
    initialized ``struct sockaddr_in`` or ``struct sockaddr_in6``.

    When the port is already taken, you can expect to see an ``UV_EADDRINUSE``
    error from either :c:func:`uv_tcp_bind`, :c:func:`uv_listen` or
    :c:func:`uv_tcp_connect`. That is, a successful call to this function does
    not guarantee that the call to :c:func:`uv_listen` or :c:func:`uv_tcp_connect`
    will succeed as well.

    `flags` con contain ``UV_TCP_IPV6ONLY``, in which case dual-stack support
    is disabled and only IPv6 is used.

.. c:function:: int uv_tcp_getsockname(const uv_tcp_t* handle, struct sockaddr* name, int* namelen)

    Get the current address to which the handle is bound. `addr` must point to
    a valid and big enough chunk of memory, ``struct sockaddr_storage`` is
    recommended for IPv4 and IPv6 support.

.. c:function:: int uv_tcp_getpeername(const uv_tcp_t* handle, struct sockaddr* name, int* namelen)

    Get the address of the peer connected to the handle. `addr` must point to
    a valid and big enough chunk of memory, ``struct sockaddr_storage`` is
    recommended for IPv4 and IPv6 support.

.. c:function:: int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle, const struct sockaddr* addr, uv_connect_cb cb)

    Establish an IPv4 or IPv6 TCP connection. Provide an initialized TCP handle
    and an uninitialized :c:type:`uv_connect_t`. `addr` should point to an
    initialized ``struct sockaddr_in`` or ``struct sockaddr_in6``.

    The callback is made when the connection has been established or when a
    connection error happened.

.. seealso:: The :c:type:`uv_stream_t` API functions also apply.
