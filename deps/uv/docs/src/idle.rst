
.. _idle:

:c:type:`uv_idle_t` --- Idle handle
===================================

Idle handles will run the given callback once per loop iteration, right
before the :c:type:`uv_prepare_t` handles.

.. note::
    The notable difference with prepare handles is that when there are active idle handles,
    the loop will perform a zero timeout poll instead of blocking for i/o.

.. warning::
    Despite the name, idle handles will get their callbacks called on every loop iteration,
    not when the loop is actually "idle".


Data types
----------

.. c:type:: uv_idle_t

    Idle handle type.

.. c:type:: void (*uv_idle_cb)(uv_idle_t* handle)

    Type definition for callback passed to :c:func:`uv_idle_start`.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_idle_init(uv_loop_t*, uv_idle_t* idle)

    Initialize the handle.

.. c:function:: int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb)

    Start the handle with the given callback.

.. c:function:: int uv_idle_stop(uv_idle_t* idle)

    Stop the handle, the callback will no longer be called.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
