
.. _threadpool:

Threadpool
===========================

libuv provides the notion of an executor for asynchronous work.
An executor runs work off of the loop thread and delivers a notification
to the event loop when the work is finished.

Users can submit work directly to the executor via
:c:func:`uv_executor_queue_work`.

libuv will also convert certain asynchronous requests into work for the
executor when appropriate OS-level facilities are unavailable.
This includes asynchronous file system operations and DNS requests
(getaddrinfo, getnameinfo).
All such internally-generated work is submitted to the public
executor API.

libuv offers a default executor called the threadpool.
This pool maintains a pool of worker threads that consume pending work in
a FIFO manner.
By default there are 4 threads in this pool.
The size can be controlled by setting the ``UV_THREADPOOL_SIZE`` environment
variable to the desired number of threads.

libuv also permits users to replace the default executor with their own
implementation via :c:func:`uv_replace_executor`.
Users may thus define a more sophisticated executor if desired,
e.g. handling I/O and CPU in different pools.

.. note::
    The default executor cannot be overriden after any work is queued.

The executor is global and shared across all event loops.

When a function makes use of the default executor (i.e. when using :c:func:`uv_queue_work`)
libuv preallocates and initializes the maximum number of threads allowed by
``UV_THREADPOOL_SIZE``. This causes a relatively minor memory overhead
(~1MB for 128 threads) but increases the performance of threading at runtime.
The maximum size of the default libuv executor's threadpool is 128.

.. note::
    Even though a global thread pool which is shared across all events
    loops is used, the functions are not thread safe.


Data types
----------

.. c:type:: uv_work_t

    Work request type.

.. c:type:: void (*uv_work_cb)(uv_work_t* req)

    Callback passed to :c:func:`uv_queue_work` which will be run on
    the executor.

.. c:type:: void (*uv_after_work_cb)(uv_work_t* req, int status)

    Callback passed to :c:func:`uv_queue_work` which will be called on the loop
    thread after the work on the executor has been completed or cancelled.
    If the work was cancelled using :c:func:`uv_cancel`,
    then `status` will be ``UV_ECANCELED``.

.. c:type:: uv_executor_t

    Executor type. Use when overriding the default threadpool.
    Zero out objects of this type before use.

.. c:type:: void (*uv_executor_submit_func)(uv_executor_t* executor, uv_work_t* req, uv_work_options_t* opts)

    Called when the executor should handle this request.

.. c:type:: int (*uv_executor_cancel_func)(uv_executor_t* executor, uv_work_t* req)

    Called when someone wants to cancel a previously submitted request.
    Return ``UV_EBUSY`` if you cannot cancel it.

.. seealso:: :c:func:`uv_cancel_t`.

.. c:type:: int uv_replace_executor(uv_executor_t* executor)

    Replace the default libuv executor with this user-defined one.
    Must be called before any work is submitted to the default libuv executor.
    Returns 0 on success.

.. c:type:: uv_work_options_t

    Options for guiding the executor in its policy decisions.

Public members
^^^^^^^^^^^^^^

.. c:member:: void* uv_work_t.executor_data

    Space for arbitrary data. libuv does not use this field.
    This is intended for use by an executor implementation.

.. c:member:: uv_loop_t* uv_work_t.loop

    Loop that started this request and where completion will be reported.
    Readonly.

.. c:member:: uv_work_cb uv_work_t.work_cb

    Executor should invoke this, not on the event loop.

.. seealso:: The :c:type:`uv_req_t` members also apply.

.. c:member:: uv_executor_submit_func uv_executor_t.submit

    Must be non-NULL.

.. c:member:: uv_executor_cancel_func uv_executor_t.cancel

    Can be NULL.
    If NULL, calls to :c:function:`uv_cancel` will return ``UV_ENOSYS``.

.. c:member:: void * uv_executor_t.data

    Space for user-defined arbitrary data. libuv does not use this field.

.. c:member:: uv_work_type uv_work_options_t.type

    Type of request. Readonly.

    ::

        typedef enum {
            UV_WORK_UNKNOWN = 0,
            UV_WORK_FS,
            UV_WORK_DNS,
            UV_WORK_USER_IO,
            UV_WORK_USER_CPU,
            UV_WORK_PRIVATE,
            UV_WORK_MAX = 255
        } uv_work_type;

.. c:member:: int uv_work_options_t.priority

    Suggested for use in user-defined executor.
    Has no effect on libuv's default executor.

.. c:member:: int uv_work_options_t.cancelable

    Boolean.
    If non-zero, it is safe to abort this work while it is being handled
    by a thread (e.g. by pthread_cancel'ing the thread on which it is running).
    In addition, work that has not yet been assigned to a thread can be
    cancelled.

.. c:member:: void * uv_work_options_t.data

    Space for user-defined arbitrary data. libuv does not use this field.


API
---

.. c:function:: int uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb, uv_after_work_cb after_work_cb)

    Calls :c:func:`uv_executor_queue_work` with NULL options.

.. c:function:: int uv_executor_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_options_t* opts, uv_work_cb work_cb, uv_after_work_cb after_work_cb)

    Submits a work request with options to the executor.
    The executor will run the given `work_cb` off of the loop thread.
    Once `work_cb` is completed, `after_work_cb` will be
    called on loop's loop thread.

    The `opts` can guide the executor used by libuv.

    `req` must remain valid until `after_work_cb` is executed.
    `opts` need not remain valid once `uv_executor_queue_work` returns.

    This request can be cancelled with :c:func:`uv_cancel`.

.. c:function:: void uv_executor_return_work(uv_work_t* req)

    An executor should invoke this function once it finishes with a request.
    The effect is to return control over the `req` to libuv.

    This function is thread safe. <-- TODO This seems desirable so the executor workers don't have to centralize returns through the event loop, but thread safety requires locking loop->wq_mutex. I'm having trouble imagining how this could lead to deadlock in a "reasonable" executor implementation, but wanted to discuss.

.. seealso:: The :c:type:`uv_req_t` API functions also apply
             to a :c:type:`uv_work_t`.
