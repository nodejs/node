
.. _signal:

:c:type:`uv_signal_t` --- Signal handle
=======================================

Signal handles implement Unix style signal handling on a per-event loop bases.

Reception of some signals is emulated on Windows:

* SIGINT is normally delivered when the user presses CTRL+C. However, like
  on Unix, it is not generated when terminal raw mode is enabled.

* SIGBREAK is delivered when the user pressed CTRL + BREAK.

* SIGHUP is generated when the user closes the console window. On SIGHUP the
  program is given approximately 10 seconds to perform cleanup. After that
  Windows will unconditionally terminate it.

* SIGWINCH is raised whenever libuv detects that the console has been
  resized. SIGWINCH is emulated by libuv when the program uses a :c:type:`uv_tty_t`
  handle to write to the console. SIGWINCH may not always be delivered in a
  timely manner; libuv will only detect size changes when the cursor is
  being moved. When a readable :c:type:`uv_tty_t` handle is used in raw mode,
  resizing the console buffer will also trigger a SIGWINCH signal.

Watchers for other signals can be successfully created, but these signals
are never received. These signals are: `SIGILL`, `SIGABRT`, `SIGFPE`, `SIGSEGV`,
`SIGTERM` and `SIGKILL.`

Calls to raise() or abort() to programmatically raise a signal are
not detected by libuv; these will not trigger a signal watcher.

.. note::
    On Linux SIGRT0 and SIGRT1 (signals 32 and 33) are used by the NPTL pthreads library to
    manage threads. Installing watchers for those signals will lead to unpredictable behavior
    and is strongly discouraged. Future versions of libuv may simply reject them.


Data types
----------

.. c:type:: uv_signal_t

    Signal handle type.

.. c:type:: void (*uv_signal_cb)(uv_signal_t* handle, int signum)

    Type definition for callback passed to :c:func:`uv_signal_start`.


Public members
^^^^^^^^^^^^^^

.. c:member:: int uv_signal_t.signum

    Signal being monitored by this handle. Readonly.

.. seealso:: The :c:type:`uv_handle_t` members also apply.


API
---

.. c:function:: int uv_signal_init(uv_loop_t*, uv_signal_t* signal)

    Initialize the handle.

.. c:function:: int uv_signal_start(uv_signal_t* signal, uv_signal_cb cb, int signum)

    Start the handle with the given callback, watching for the given signal.

.. c:function:: int uv_signal_stop(uv_signal_t* signal)

    Stop the handle, the callback will no longer be called.

.. seealso:: The :c:type:`uv_handle_t` API functions also apply.
