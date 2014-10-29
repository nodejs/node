
.. _errors:

Error handling
==============

In libuv errors are negative numbered constants. As a rule of thumb, whenever
there is a status parameter, or an API functions returns an integer, a negative
number will imply an error.

.. note::
    Implementation detail: on Unix error codes are the negated `errno` (or `-errno`), while on
    Windows they are defined by libuv to arbitrary negative numbers.


Error constants
---------------

.. c:macro:: UV_E2BIG

    argument list too long

.. c:macro:: UV_EACCES

    permission denied

.. c:macro:: UV_EADDRINUSE

    address already in use

.. c:macro:: UV_EADDRNOTAVAIL

    address not available

.. c:macro:: UV_EAFNOSUPPORT

    address family not supported

.. c:macro:: UV_EAGAIN

    resource temporarily unavailable

.. c:macro:: UV_EAI_ADDRFAMILY

    address family not supported

.. c:macro:: UV_EAI_AGAIN

    temporary failure

.. c:macro:: UV_EAI_BADFLAGS

    bad ai_flags value

.. c:macro:: UV_EAI_BADHINTS

    invalid value for hints

.. c:macro:: UV_EAI_CANCELED

    request canceled

.. c:macro:: UV_EAI_FAIL

    permanent failure

.. c:macro:: UV_EAI_FAMILY

    ai_family not supported

.. c:macro:: UV_EAI_MEMORY

    out of memory

.. c:macro:: UV_EAI_NODATA

    no address

.. c:macro:: UV_EAI_NONAME

    unknown node or service

.. c:macro:: UV_EAI_OVERFLOW

    argument buffer overflow

.. c:macro:: UV_EAI_PROTOCOL

    resolved protocol is unknown

.. c:macro:: UV_EAI_SERVICE

    service not available for socket type

.. c:macro:: UV_EAI_SOCKTYPE

    socket type not supported

.. c:macro:: UV_EALREADY

    connection already in progress

.. c:macro:: UV_EBADF

    bad file descriptor

.. c:macro:: UV_EBUSY

    resource busy or locked

.. c:macro:: UV_ECANCELED

    operation canceled

.. c:macro:: UV_ECHARSET

    invalid Unicode character

.. c:macro:: UV_ECONNABORTED

    software caused connection abort

.. c:macro:: UV_ECONNREFUSED

    connection refused

.. c:macro:: UV_ECONNRESET

    connection reset by peer

.. c:macro:: UV_EDESTADDRREQ

    destination address required

.. c:macro:: UV_EEXIST

    file already exists

.. c:macro:: UV_EFAULT

    bad address in system call argument

.. c:macro:: UV_EFBIG

    file too large

.. c:macro:: UV_EHOSTUNREACH

    host is unreachable

.. c:macro:: UV_EINTR

    interrupted system call

.. c:macro:: UV_EINVAL

    invalid argument

.. c:macro:: UV_EIO

    i/o error

.. c:macro:: UV_EISCONN

    socket is already connected

.. c:macro:: UV_EISDIR

    illegal operation on a directory

.. c:macro:: UV_ELOOP

    too many symbolic links encountered

.. c:macro:: UV_EMFILE

    too many open files

.. c:macro:: UV_EMSGSIZE

    message too long

.. c:macro:: UV_ENAMETOOLONG

    name too long

.. c:macro:: UV_ENETDOWN

    network is down

.. c:macro:: UV_ENETUNREACH

    network is unreachable

.. c:macro:: UV_ENFILE

    file table overflow

.. c:macro:: UV_ENOBUFS

    no buffer space available

.. c:macro:: UV_ENODEV

    no such device

.. c:macro:: UV_ENOENT

    no such file or directory

.. c:macro:: UV_ENOMEM

    not enough memory

.. c:macro:: UV_ENONET

    machine is not on the network

.. c:macro:: UV_ENOPROTOOPT

    protocol not available

.. c:macro:: UV_ENOSPC

    no space left on device

.. c:macro:: UV_ENOSYS

    function not implemented

.. c:macro:: UV_ENOTCONN

    socket is not connected

.. c:macro:: UV_ENOTDIR

    not a directory

.. c:macro:: UV_ENOTEMPTY

    directory not empty

.. c:macro:: UV_ENOTSOCK

    socket operation on non-socket

.. c:macro:: UV_ENOTSUP

    operation not supported on socket

.. c:macro:: UV_EPERM

    operation not permitted

.. c:macro:: UV_EPIPE

    broken pipe

.. c:macro:: UV_EPROTO

    protocol error

.. c:macro:: UV_EPROTONOSUPPORT

    protocol not supported

.. c:macro:: UV_EPROTOTYPE

    protocol wrong type for socket

.. c:macro:: UV_ERANGE

    result too large

.. c:macro:: UV_EROFS

    read-only file system

.. c:macro:: UV_ESHUTDOWN

    cannot send after transport endpoint shutdown

.. c:macro:: UV_ESPIPE

    invalid seek

.. c:macro:: UV_ESRCH

    no such process

.. c:macro:: UV_ETIMEDOUT

    connection timed out

.. c:macro:: UV_ETXTBSY

    text file is busy

.. c:macro:: UV_EXDEV

    cross-device link not permitted

.. c:macro:: UV_UNKNOWN

    unknown error

.. c:macro:: UV_EOF

    end of file

.. c:macro:: UV_ENXIO

    no such device or address

.. c:macro:: UV_EMLINK

    too many links


API
---

.. c:function:: const char* uv_strerror(int err)

    Returns the error message for the given error code.

.. c:function:: const char* uv_err_name(int err)

    Returns the error name for the given error code.
