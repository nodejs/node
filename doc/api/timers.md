# Timers

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

The `timer` module exposes a global API for scheduling functions to
be called at some future period of time. Because the timer functions are
globals, there is no need to call `require('timers')` to use the API.

The timer functions within Node.js implement a similar API as the timers API
provided by Web Browsers but use a different internal implementation that is
built around the Node.js [Event Loop][].

## Class: `Immediate`

This object is created internally and is returned from [`setImmediate()`][]. It
can be passed to [`clearImmediate()`][] in order to cancel the scheduled
actions.

By default, when an immediate is scheduled, the Node.js event loop will continue
running as long as the immediate is active. The `Immediate` object returned by
[`setImmediate()`][] exports both `immediate.ref()` and `immediate.unref()`
functions that can be used to control this default behavior.

### `immediate.hasRef()`
<!-- YAML
added: v11.0.0
-->

* Returns: {boolean}

If true, the `Immediate` object will keep the Node.js event loop active.

### `immediate.ref()`
<!-- YAML
added: v9.7.0
-->

* Returns: {Immediate} a reference to `immediate`

When called, requests that the Node.js event loop *not* exit so long as the
`Immediate` is active. Calling `immediate.ref()` multiple times will have no
effect.

By default, all `Immediate` objects are "ref'ed", making it normally unnecessary
to call `immediate.ref()` unless `immediate.unref()` had been called previously.

### `immediate.unref()`
<!-- YAML
added: v9.7.0
-->

* Returns: {Immediate} a reference to `immediate`

When called, the active `Immediate` object will not require the Node.js event
loop to remain active. If there is no other activity keeping the event loop
running, the process may exit before the `Immediate` object's callback is
invoked. Calling `immediate.unref()` multiple times will have no effect.

## Class: `Timeout`

This object is created internally and is returned from [`setTimeout()`][] and
[`setInterval()`][]. It can be passed to either [`clearTimeout()`][] or
[`clearInterval()`][] in order to cancel the scheduled actions.

By default, when a timer is scheduled using either [`setTimeout()`][] or
[`setInterval()`][], the Node.js event loop will continue running as long as the
timer is active. Each of the `Timeout` objects returned by these functions
export both `timeout.ref()` and `timeout.unref()` functions that can be used to
control this default behavior.

### `timeout.hasRef()`
<!-- YAML
added: v11.0.0
-->

* Returns: {boolean}

If true, the `Timeout` object will keep the Node.js event loop active.

### `timeout.ref()`
<!-- YAML
added: v0.9.1
-->

* Returns: {Timeout} a reference to `timeout`

When called, requests that the Node.js event loop *not* exit so long as the
`Timeout` is active. Calling `timeout.ref()` multiple times will have no effect.

By default, all `Timeout` objects are "ref'ed", making it normally unnecessary
to call `timeout.ref()` unless `timeout.unref()` had been called previously.

### `timeout.refresh()`
<!-- YAML
added: v10.2.0
-->

* Returns: {Timeout} a reference to `timeout`

Sets the timer's start time to the current time, and reschedules the timer to
call its callback at the previously specified duration adjusted to the current
time. This is useful for refreshing a timer without allocating a new
JavaScript object.

Using this on a timer that has already called its callback will reactivate the
timer.

### `timeout.unref()`
<!-- YAML
added: v0.9.1
-->

* Returns: {Timeout} a reference to `timeout`

When called, the active `Timeout` object will not require the Node.js event loop
to remain active. If there is no other activity keeping the event loop running,
the process may exit before the `Timeout` object's callback is invoked. Calling
`timeout.unref()` multiple times will have no effect.

Calling `timeout.unref()` creates an internal timer that will wake the Node.js
event loop. Creating too many of these can adversely impact performance
of the Node.js application.

## Scheduling Timers

A timer in Node.js is an internal construct that calls a given function after
a certain period of time. When a timer's function is called varies depending on
which method was used to create the timer and what other work the Node.js
event loop is doing.

### `setImmediate(callback[, ...args])`
<!-- YAML
added: v0.9.1
-->

* `callback` {Function} The function to call at the end of this turn of
  the Node.js [Event Loop][]
* `...args` {any} Optional arguments to pass when the `callback` is called.
* Returns: {Immediate} for use with [`clearImmediate()`][]

Schedules the "immediate" execution of the `callback` after I/O events'
callbacks.

When multiple calls to `setImmediate()` are made, the `callback` functions are
queued for execution in the order in which they are created. The entire callback
queue is processed every event loop iteration. If an immediate timer is queued
from inside an executing callback, that timer will not be triggered until the
next event loop iteration.

If `callback` is not a function, a [`TypeError`][] will be thrown.

This method has a custom variant for promises that is available using
[`util.promisify()`][]:

```js
const util = require('util');
const setImmediatePromise = util.promisify(setImmediate);

setImmediatePromise('foobar').then((value) => {
  // value === 'foobar' (passing values is optional)
  // This is executed after all I/O callbacks.
});

// Or with async function
async function timerExample() {
  console.log('Before I/O callbacks');
  await setImmediatePromise();
  console.log('After I/O callbacks');
}
timerExample();
```

### `setInterval(callback, delay[, ...args])`
<!-- YAML
added: v0.0.1
-->

* `callback` {Function} The function to call when the timer elapses.
* `delay` {number} The number of milliseconds to wait before calling the
  `callback`.
* `...args` {any} Optional arguments to pass when the `callback` is called.
* Returns: {Timeout} for use with [`clearInterval()`][]

Schedules repeated execution of `callback` every `delay` milliseconds.

When `delay` is larger than `2147483647` or less than `1`, the `delay` will be
set to `1`. Non-integer delays are truncated to an integer.

If `callback` is not a function, a [`TypeError`][] will be thrown.

### `setTimeout(callback, delay[, ...args])`
<!-- YAML
added: v0.0.1
-->

* `callback` {Function} The function to call when the timer elapses.
* `delay` {number} The number of milliseconds to wait before calling the
  `callback`.
* `...args` {any} Optional arguments to pass when the `callback` is called.
* Returns: {Timeout} for use with [`clearTimeout()`][]

Schedules execution of a one-time `callback` after `delay` milliseconds.

The `callback` will likely not be invoked in precisely `delay` milliseconds.
Node.js makes no guarantees about the exact timing of when callbacks will fire,
nor of their ordering. The callback will be called as close as possible to the
time specified.

When `delay` is larger than `2147483647` or less than `1`, the `delay`
will be set to `1`. Non-integer delays are truncated to an integer.

If `callback` is not a function, a [`TypeError`][] will be thrown.

This method has a custom variant for promises that is available using
[`util.promisify()`][]:

```js
const util = require('util');
const setTimeoutPromise = util.promisify(setTimeout);

setTimeoutPromise(40, 'foobar').then((value) => {
  // value === 'foobar' (passing values is optional)
  // This is executed after about 40 milliseconds.
});
```

## Cancelling Timers

The [`setImmediate()`][], [`setInterval()`][], and [`setTimeout()`][] methods
each return objects that represent the scheduled timers. These can be used to
cancel the timer and prevent it from triggering.

It is not possible to cancel timers that were created using the promisified
variants of [`setImmediate()`][], [`setTimeout()`][].

### `clearImmediate(immediate)`
<!-- YAML
added: v0.9.1
-->

* `immediate` {Immediate} An `Immediate` object as returned by
  [`setImmediate()`][].

Cancels an `Immediate` object created by [`setImmediate()`][].

### `clearInterval(timeout)`
<!-- YAML
added: v0.0.1
-->

* `timeout` {Timeout} A `Timeout` object as returned by [`setInterval()`][].

Cancels a `Timeout` object created by [`setInterval()`][].

### `clearTimeout(timeout)`
<!-- YAML
added: v0.0.1
-->

* `timeout` {Timeout} A `Timeout` object as returned by [`setTimeout()`][].

Cancels a `Timeout` object created by [`setTimeout()`][].

[Event Loop]: https://nodejs.org/en/docs/guides/event-loop-timers-and-nexttick/#setimmediate-vs-settimeout
[`TypeError`]: errors.html#errors_class_typeerror
[`clearImmediate()`]: timers.html#timers_clearimmediate_immediate
[`clearInterval()`]: timers.html#timers_clearinterval_timeout
[`clearTimeout()`]: timers.html#timers_cleartimeout_timeout
[`setImmediate()`]: timers.html#timers_setimmediate_callback_args
[`setInterval()`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout()`]: timers.html#timers_settimeout_callback_delay_args
[`util.promisify()`]: util.html#util_util_promisify_original
