
.. _dns:

DNS utility functions
=====================

libuv provides asynchronous variants of `getaddrinfo` and `getnameinfo`.


Data types
----------

.. c:type:: uv_getaddrinfo_t

    `getaddrinfo` request type.

.. c:type:: void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t* req, int status, struct addrinfo* res)

    Callback which will be called with the getaddrinfo request result once
    complete. In case it was cancelled, `status` will have a value of
    ``UV_ECANCELED``.

.. c:type:: uv_getnameinfo_t

    `getnameinfo` request type.

.. c:type:: void (*uv_getnameinfo_cb)(uv_getnameinfo_t* req, int status, const char* hostname, const char* service)

    Callback which will be called with the getnameinfo request result once
    complete. In case it was cancelled, `status` will have a value of
    ``UV_ECANCELED``.


Public members
^^^^^^^^^^^^^^

.. c:member:: uv_loop_t* uv_getaddrinfo_t.loop

    Loop that started this getaddrinfo request and where completion will be
    reported. Readonly.

.. c:member:: uv_loop_t* uv_getnameinfo_t.loop

    Loop that started this getnameinfo request and where completion will be
    reported. Readonly.

.. seealso:: The :c:type:`uv_req_t` members also apply.


API
---

.. c:function:: int uv_getaddrinfo(uv_loop_t* loop, uv_getaddrinfo_t* req, uv_getaddrinfo_cb getaddrinfo_cb, const char* node, const char* service, const struct addrinfo* hints)

    Asynchronous ``getaddrinfo(3)``.

    Either node or service may be NULL but not both.

    `hints` is a pointer to a struct addrinfo with additional address type
    constraints, or NULL. Consult `man -s 3 getaddrinfo` for more details.

    Returns 0 on success or an error code < 0 on failure. If successful, the
    callback will get called sometime in the future with the lookup result,
    which is either:

    * status == 0, the res argument points to a valid `struct addrinfo`, or
    * status < 0, the res argument is NULL. See the UV_EAI_* constants.

    Call :c:func:`uv_freeaddrinfo` to free the addrinfo structure.

.. c:function:: void uv_freeaddrinfo(struct addrinfo* ai)

    Free the struct addrinfo. Passing NULL is allowed and is a no-op.

.. c:function:: int uv_getnameinfo(uv_loop_t* loop, uv_getnameinfo_t* req, uv_getnameinfo_cb getnameinfo_cb, const struct sockaddr* addr, int flags)

    Asynchronous ``getnameinfo(3)``.

    Returns 0 on success or an error code < 0 on failure. If successful, the
    callback will get called sometime in the future with the lookup result.
    Consult `man -s 3 getnameinfo` for more details.

.. seealso:: The :c:type:`uv_req_t` API functions also apply.
