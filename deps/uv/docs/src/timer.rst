
.. _timer:

:c:type:`uv_timer_t` --- Timer handle
=====================================

Timer handles are used to schedule callbacks to be called in the future.


Data types
----------

.. c:type:: uv_timer_t

    Timer handle type.

.. c:type:: void (*uv_timer_cb)(uv_timer_t* handle)

    Type definition for callback passed to :c:func:`uv_timer_start`.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle)

    Initialize the handle.

.. c:function:: int uv_timer_start(uv_timer_t* handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat)

    Start the timer. `timeout` and `repeat` are in milliseconds.

    If `timeout` is zero, the callback fires on the next event loop iteration.
    If `repeat` is non-zero, the callback fires first after `timeout`
    milliseconds and then repeatedly after `repeat` milliseconds.

    .. note::
        Does not update the event loop's concept of "now". See :c:func:`uv_update_time` for more information.

.. c:function:: int uv_timer_stop(uv_timer_t* handle)

    Stop the timer, the callback will not be called anymore.

.. c:function:: int uv_timer_again(uv_timer_t* handle)

    Stop the timer, and if it is repeating restart it using the repeat value
    as the timeout. If the timer has never been started before it returns
    UV_EINVAL.

.. c:function:: void uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat)

    Set the repeat interval value in milliseconds. The timer will be scheduled
    to run on the given interval, regardless of the callback execution
    duration, and will follow normal timer semantics in the case of a
    time-slice overrun.

    For example, if a 50ms repeating timer first runs for 17ms, it will be
    scheduled to run again 33ms later. If other tasks consume more than the
    33ms following the first timer callback, then the callback will run as soon
    as possible.

    .. note::
        If the repeat value is set from a timer callback it does not immediately take effect.
        If the timer was non-repeating before, it will have been stopped. If it was repeating,
        then the old repeat value will have been used to schedule the next timeout.

.. c:function:: uint64_t uv_timer_get_repeat(const uv_timer_t* handle)

    Get the timer repeat value.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
