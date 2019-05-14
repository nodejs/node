
.. _check:

:c:type:`uv_check_t` --- Check handle
=====================================

Check handles will run the given callback once per loop iteration, right
after polling for i/o.


Data types
----------

.. c:type:: uv_check_t

    Check handle type.

.. c:type:: void (*uv_check_cb)(uv_check_t* handle)

    Type definition for callback passed to :c:func:`uv_check_start`.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_check_init(uv_loop_t* loop, uv_check_t* check)

    Initialize the handle.

.. c:function:: int uv_check_start(uv_check_t* check, uv_check_cb cb)

    Start the handle with the given callback.

.. c:function:: int uv_check_stop(uv_check_t* check)

    Stop the handle, the callback will no longer be called.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
