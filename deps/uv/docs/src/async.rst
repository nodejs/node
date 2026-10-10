
.. _async:

:c:type:`uv_async_t` --- Async handle
=====================================

Async handles allow the user to "wakeup" the event loop and get a callback
called from another thread.

.. note::

    :c:func:`uv_async_send` and the :c:type:`uv_async_cb` invocation are
    sequentially consistent (seq_cst) operations for a given async handle: all
    memory accesses (reads and writes) made before :c:func:`uv_async_send` are
    visible to that callback.

.. warning::
    libuv will coalesce calls to :c:func:`uv_async_send`, that is, not every
    call to it will yield an execution of the callback. For example: if
    :c:func:`uv_async_send` is called 5 times in a row before the callback is
    called, the callback will only be called once. If :c:func:`uv_async_send`
    is called again after the callback was called, it will be called again.
    However, since it is sequentially consistent, the values read or written in
    that callback will always be the same (or newer) as those read or written
    by the other thread.

.. versionchanged:: 1.53.0
    :c:func:`uv_async_send` and :c:type:`uv_async_cb` are sequentially
    consistent. Prior to this version, any case where libuv might coalesce
    calls probably requires a full `seq_cst` fence before it for correctness.


Data types
----------

.. c:type:: uv_async_t

    Async handle type.

.. c:type:: void (*uv_async_cb)(uv_async_t* handle)

    Type definition for callback passed to :c:func:`uv_async_init`.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_async_init(uv_loop_t* loop, uv_async_t* async, uv_async_cb async_cb)

    Initialize the handle. A NULL callback is allowed.

    :returns: 0 on success, or an error code < 0 on failure.

    .. note::
        Unlike other handle initialization  functions, it immediately starts the handle.

.. c:function:: int uv_async_send(uv_async_t* async)

    Wake up the event loop and call the async handle's callback.

    :returns: 0 on success, or an error code < 0 on failure.

    .. note::
        It's safe to call this function from any thread. The callback will be called on the
        loop thread.

    .. note::
        :c:func:`uv_async_send` is `async-signal-safe <https://man7.org/linux/man-pages/man7/signal-safety.7.html>`_.
        It's safe to call this function from a signal handler.

    .. note::
        This is a full memory fence with respect to other calls and callbacks
        using this same async handle, and so it will order all operations
        around this call and the corresponding callback on the sending thread
        and receiving thread.

.. seealso::
    The :c:type:`uv_handle_t` API functions also apply.
