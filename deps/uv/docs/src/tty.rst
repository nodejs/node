
.. _tty:

:c:type:`uv_tty_t` --- TTY handle
=================================

TTY handles represent a stream for the console.

:c:type:`uv_tty_t` is a 'subclass' of :c:type:`uv_stream_t`.


Data types
----------

.. c:type:: uv_tty_t

    TTY handle type.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_stream_t` members also apply.


API
---

.. c:function:: int uv_tty_init(uv_loop_t*, uv_tty_t*, uv_file fd, int readable)

    Initialize a new TTY stream with the given file descriptor. Usually the
    file descriptor will be:

    * 0 = stdin
    * 1 = stdout
    * 2 = stderr

    `readable`, specifies if you plan on calling :c:func:`uv_read_start` with
    this stream. stdin is readable, stdout is not.

    .. note::
        TTY streams which are not readable have blocking writes.

.. c:function:: int uv_tty_set_mode(uv_tty_t*, int mode)

    Set the TTY mode. 0 for normal, 1 for raw.

.. c:function:: int uv_tty_reset_mode(void)

    To be called when the program exits. Resets TTY settings to default
    values for the next process to take over.

    This function is async signal-safe on Unix platforms but can fail with error
    code ``UV_EBUSY`` if you call it when execution is inside
    :c:func:`uv_tty_set_mode`.

.. c:function:: int uv_tty_get_winsize(uv_tty_t*, int* width, int* height)

    Gets the current Window size. On success it returns 0.

.. seealso:: The :c:type:`uv_stream_t` API functions also apply.
