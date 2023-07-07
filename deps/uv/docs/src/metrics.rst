
.. _metrics:

Metrics operations
======================

libuv provides a metrics API to track various internal operations of the event
loop.


Data types
----------

.. c:type:: uv_metrics_t

    The struct that contains event loop metrics. It is recommended to retrieve
    these metrics in a :c:type:`uv_prepare_cb` in order to make sure there are
    no inconsistencies with the metrics counters.

    ::

        typedef struct {
            uint64_t loop_count;
            uint64_t events;
            uint64_t events_waiting;
            /* private */
            uint64_t* reserved[13];
        } uv_metrics_t;


Public members
^^^^^^^^^^^^^^

.. c:member:: uint64_t uv_metrics_t.loop_count

    Number of event loop iterations.

.. c:member:: uint64_t uv_metrics_t.events

    Number of events that have been processed by the event handler.

.. c:member:: uint64_t uv_metrics_t.events_waiting

    Number of events that were waiting to be processed when the event provider
    was called.


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

    .. versionadded:: 1.39.0

.. c:function:: int uv_metrics_info(uv_loop_t* loop, uv_metrics_t* metrics)

    Copy the current set of event loop metrics to the ``metrics`` pointer.

    .. versionadded:: 1.45.0
