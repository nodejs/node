# Timers

    Stability: 3 - Locked

All of the timer functions are globals.  You do not need to `require()`
this module in order to use them.

## Class: Immediate

This object is created internally and is returned from [`setImmediate`][]. It
can be passed to [`clearImmediate`] in order to cancel the scheduled actions.

## Class: Timeout

This object is created internally and is returned from [`setTimeout`][] and
[`setInterval`][]. It can be passed to [`clearTimeout`][] or [`clearInterval`][]
respectively in order to cancel the scheduled actions.

### ref()

If a `Timeout` was previously `unref()`d, then `ref()` can be called to
explicitly request the `Timeout` hold the program open. If the `Timeout` is
already `ref`d calling `ref` again will have no effect.

Returns the `Timeout`.

### unref()

The `Timeout` returned by [`setTimeout`][] and [`setInterval`][] also has the
method `timeout.unref()` which allows the creation of a `Timeout` that is active
but if it is the only item left in the event loop, it won't keep the program
running. If the `Timeout` is already `unref`d calling `unref` again will have no
effect.

In the case of [`setTimeout`][], `unref` creates a separate underlying timer
handle that will wakeup the event loop, creating too many of these may adversely
effect event loop performance -- use wisely.

Returns the `Timeout`.

## clearImmediate(immediate)

Stops an `Immediate`, as created by [`setImmediate`][], from triggering.

## clearInterval(timeout)

Stops a `Timeout`, as created by [`setInterval`][], from triggering.

## clearTimeout(timeout)

Stops a `Timeout`, as created by [`setTimeout`][], from triggering.

## setImmediate(callback[, arg][, ...])

Schedules "immediate" execution of `callback` after I/O events'
callbacks and before timers (`Timeout`s) set by [`setTimeout`][] and
[`setInterval`][] are triggered. Returns an `Immediate` for possible use with
[`clearImmediate`][]. Additional optional arguments may be passed to the
callback.

Callbacks for immediates are queued in the order in which they were created.
The entire callback queue is processed every event loop iteration. If an
immediate is queued from inside an executing callback, that immediate won't fire
until the next event loop iteration.

If `callback` is not a function `setImmediate()` will throw immediately.

## setInterval(callback, delay[, arg][, ...])

Schedules repeated execution of `callback` every `delay` milliseconds.
Returns a `Timeout` for possible use with [`clearInterval`][]. Additional
optional arguments may be passed to the callback.

To follow browser behavior, when using delays larger than 2147483647
milliseconds (approximately 25 days) or less than 1, Node.js will use 1 as the
`delay`.

If `callback` is not a function `setInterval()` will throw immediately.

## setTimeout(callback, delay[, arg][, ...])

Schedules execution of a one-time `callback` after `delay` milliseconds.
Returns a `Timeout` for possible use with [`clearTimeout`][]. Additional
optional arguments may be passed to the callback.

The callback will likely not be invoked in precisely `delay` milliseconds.
Node.js makes no guarantees about the exact timing of when callbacks will fire,
nor of their ordering. The callback will be called as close as possible to the
time specified.

To follow browser behavior, when using delays larger than 2147483647
milliseconds (approximately 25 days) or less than 1, the timeout is executed
immediately, as if the `delay` was set to 1.

If `callback` is not a function `setTimeout()` will throw immediately.

[`clearImmediate`]: timers.html#timers_clearimmediate_immediateobject
[`clearInterval`]: timers.html#timers_clearinterval_intervalobject
[`clearTimeout`]: timers.html#timers_cleartimeout_timeoutobject
[`setImmediate`]: timers.html#timers_setimmediate_callback_arg
[`setInterval`]: timers.html#timers_setinterval_callback_delay_arg
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_arg
