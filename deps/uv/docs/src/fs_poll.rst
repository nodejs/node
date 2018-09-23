
.. _fs_poll:

:c:type:`uv_fs_poll_t` --- FS Poll handle
=========================================

FS Poll handles allow the user to monitor a given path for changes. Unlike
:c:type:`uv_fs_event_t`, fs poll handles use `stat` to detect when a file has
changed so they can work on file systems where fs event handles can't.


Data types
----------

.. c:type:: uv_fs_poll_t

    FS Poll handle type.

.. c:type:: void (*uv_fs_poll_cb)(uv_fs_poll_t* handle, int status, const uv_stat_t* prev, const uv_stat_t* curr)

    Callback passed to :c:func:`uv_fs_poll_start` which will be called repeatedly
    after the handle is started, when any change happens to the monitored path.

    The callback is invoked with `status < 0` if `path` does not exist
    or is inaccessible. The watcher is *not* stopped but your callback is
    not called again until something changes (e.g. when the file is created
    or the error reason changes).

    When `status == 0`, the callback receives pointers to the old and new
    :c:type:`uv_stat_t` structs. They are valid for the duration of the
    callback only.


Public members
^^^^^^^^^^^^^^

N/A

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_fs_poll_init(uv_loop_t* loop, uv_fs_poll_t* handle)

    Initialize the handle.

.. c:function:: int uv_fs_poll_start(uv_fs_poll_t* handle, uv_fs_poll_cb poll_cb, const char* path, unsigned int interval)

    Check the file at `path` for changes every `interval` milliseconds.

    .. note::
        For maximum portability, use multi-second intervals. Sub-second intervals will not detect
        all changes on many file systems.

.. c:function:: int uv_fs_poll_stop(uv_fs_poll_t* handle)

    Stop the handle, the callback will no longer be called.

.. c:function:: int uv_fs_poll_getpath(uv_fs_poll_t* handle, char* buffer, size_t* size)

    Get the path being monitored by the handle. The buffer must be preallocated
    by the user. Returns 0 on success or an error code < 0 in case of failure.
    On success, `buffer` will contain the path and `size` its length. If the buffer
    is not big enough UV_ENOBUFS will be returned and len will be set to the
    required size.

    .. versionchanged:: 1.3.0 the returned length no longer includes the terminating null byte,
                        and the buffer is not null terminated.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
