
.. _prepare:

:c:type:`uv_prepare_t` --- Prepare handle
=========================================

Prepare handles will run the given callback once per loop iteration, right
before polling for i/o.


Data types
----------

.. c:type:: uv_prepare_t

    Prepare handle type.

.. c:type:: void (*uv_prepare_cb)(uv_prepare_t* handle)

    Type definition for callback passed to :c:func:`uv_prepare_start`.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_prepare_init(uv_loop_t* loop, uv_prepare_t* prepare)

    Initialize the handle. This function always succeeds.

    :returns: 0

.. c:function:: int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb)

    Start the handle with the given callback. This function always succeeds,
    except when `cb` is `NULL`.

    :returns: 0 on success, or `UV_EINVAL` when `cb == NULL`.

.. c:function:: int uv_prepare_stop(uv_prepare_t* prepare)

    Stop the handle, the callback will no longer be called.
    This function always succeeds.

    :returns: 0

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
