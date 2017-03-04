# Timers

> Stability: 2 - Stable

The `timer` module exposes a global API for scheduling functions to
be called at some future period of time. Because the timer functions are
globals, there is no need to call `require('timers')` to use the API.

The timer functions within Node.js implement a similar API as the timers API
provided by Web Browsers but use a different internal implementation that is
built around [the Node.js Event Loop][].

## Class: Immediate

This object is created internally and is returned from [`setImmediate()`][]. It
can be passed to [`clearImmediate()`][] in order to cancel the scheduled
actions.

## Class: Timeout

This object is created internally and is returned from [`setTimeout()`][] and
[`setInterval()`][]. It can be passed to [`clearTimeout()`][] or
[`clearInterval()`][] (respectively) in order to cancel the scheduled actions.

By default, when a timer is scheduled using either [`setTimeout()`][] or
[`setInterval()`][], the Node.js event loop will continue running as long as the
timer is active. Each of the `Timeout` objects returned by these functions
export both `timeout.ref()` and `timeout.unref()` functions that can be used to
control this default behavior.

### timeout.ref()
<!-- YAML
added: v0.9.1
-->

When called, requests that the Node.js event loop *not* exit so long as the
`Timeout` is active. Calling `timeout.ref()` multiple times will have no effect.

*Note*: By default, all `Timeout` objects are "ref'd", making it normally
unnecessary to call `timeout.ref()` unless `timeout.unref()` had been called
previously.

Returns a reference to the `Timeout`.

### timeout.unref()
<!-- YAML
added: v0.9.1
-->

When called, the active `Timeout` object will not require the Node.js event loop
to remain active. If there is no other activity keeping the event loop running,
the process may exit before the `Timeout` object's callback is invoked. Calling
`timeout.unref()` multiple times will have no effect.

*Note*: Calling `timeout.unref()` creates an internal timer that will wake the
Node.js event loop. Creating too many of these can adversely impact performance
of the Node.js application.

Returns a reference to the `Timeout`.

## Scheduling Timers

A timer in Node.js is an internal construct that calls a given function after
a certain period of time. When a timer's function is called varies depending on
which method was used to create the timer and what other work the Node.js
event loop is doing.

### setImmediate(callback[, ...args])
<!-- YAML
added: v0.9.1
-->

* `callback` {Function} The function to call at the end of this turn of
  [the Node.js Event Loop]
* `...args` {any} Optional arguments to pass when the `callback` is called.

Schedules the "immediate" execution of the `callback` after I/O events'
callbacks and before timers created using [`setTimeout()`][] and
[`setInterval()`][] are triggered. Returns an `Immediate` for use with
[`clearImmediate()`][].

When multiple calls to `setImmediate()` are made, the `callback` functions are
queued for execution in the order in which they are created. The entire callback
queue is processed every event loop iteration. If an immediate timer is queued
from inside an executing callback, that timer will not be triggered until the
next event loop iteration.

If `callback` is not a function, a [`TypeError`][] will be thrown.

### setInterval(callback, delay[, ...args])
<!-- YAML
added: v0.0.1
-->

* `callback` {Function} The function to call when the timer elapses.
* `delay` {number} The number of milliseconds to wait before calling the
  `callback`.
* `...args` {any} Optional arguments to pass when the `callback` is called.

Schedules repeated execution of `callback` every `delay` milliseconds.
Returns a `Timeout` for use with [`clearInterval()`][].

When `delay` is larger than `2147483647` or less than `1`, the `delay` will be
set to `1`.

If `callback` is not a function, a [`TypeError`][] will be thrown.

### setTimeout(callback, delay[, ...args])
<!-- YAML
added: v0.0.1
-->

* `callback` {Function} The function to call when the timer elapses.
* `delay` {number} The number of milliseconds to wait before calling the
  `callback`.
* `...args` {any} Optional arguments to pass when the `callback` is called.

Schedules execution of a one-time `callback` after `delay` milliseconds.
Returns a `Timeout` for use with [`clearTimeout()`][].

The `callback` will likely not be invoked in precisely `delay` milliseconds.
Node.js makes no guarantees about the exact timing of when callbacks will fire,
nor of their ordering. The callback will be called as close as possible to the
time specified.

*Note*: When `delay` is larger than `2147483647` or less than `1`, the `delay`
will be set to `1`.

If `callback` is not a function, a [`TypeError`][] will be thrown.

## Cancelling Timers

The [`setImmediate()`][], [`setInterval()`][], and [`setTimeout()`][] methods
each return objects that represent the scheduled timers. These can be used to
cancel the timer and prevent it from triggering.

### clearImmediate(immediate)
<!-- YAML
added: v0.9.1
-->

* `immediate` {Immediate} An `Immediate` object as returned by
  [`setImmediate()`][].

Cancels an `Immediate` object created by [`setImmediate()`][].

### clearInterval(timeout)
<!-- YAML
added: v0.0.1
-->

* `timeout` {Timeout} A `Timeout` object as returned by [`setInterval()`][].

Cancels a `Timeout` object created by [`setInterval()`][].

### clearTimeout(timeout)
<!-- YAML
added: v0.0.1
-->

* `timeout` {Timeout} A `Timeout` object as returned by [`setTimeout()`][].

Cancels a `Timeout` object created by [`setTimeout()`][].


[the Node.js Event Loop]: https://nodejs.org/en/docs/guides/event-loop-timers-and-nexttick
[`TypeError`]: errors.html#errors_class_typeerror
[`clearImmediate()`]: timers.html#timers_clearimmediate_immediate
[`clearInterval()`]: timers.html#timers_clearinterval_timeout
[`clearTimeout()`]: timers.html#timers_cleartimeout_timeout
[`setImmediate()`]: timers.html#timers_setimmediate_callback_args
[`setInterval()`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout()`]: timers.html#timers_settimeout_callback_delay_args
