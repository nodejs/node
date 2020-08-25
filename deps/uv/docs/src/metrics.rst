
.. _metrics:

Metrics operations
======================

libuv provides a metrics API to track the amount of time the event loop has
spent idle in the kernel's event provider.

API
---

.. c:function:: uint64_t uv_metrics_idle_time(uv_loop_t* loop)

    Retrieve the amount of time the event loop has been idle in the kernel's
    event provider (e.g. ``epoll_wait``). The call is thread safe.

    The return value is the accumulated time spent idle in the kernel's event
    provider starting from when the :c:type:`uv_loop_t` was configured to
    collect the idle time.

    .. note::
        The event loop will not begin accumulating the event provider's idle
        time until calling :c:type:`uv_loop_configure` with
        :c:type:`UV_METRICS_IDLE_TIME`.
