# Timers

    Stability: 3 - Locked

All of the timer functions are globals.  You do not need to `require()`
this module in order to use them.

## clearImmediate(immediateObject)
<!-- YAML
added: v0.9.1
-->

Stops an `immediateObject`, as created by [`setImmediate`][], from triggering.

## clearInterval(intervalObject)
<!-- YAML
added: v0.0.1
-->

Stops an `intervalObject`, as created by [`setInterval`][], from triggering.

## clearTimeout(timeoutObject)
<!-- YAML
added: v0.0.1
-->

Prevents a `timeoutObject`, as created by [`setTimeout`][], from triggering.

## ref()
<!-- YAML
added: v0.9.1
-->

If a timer was previously `unref()`d, then `ref()` can be called to explicitly
request the timer hold the program open. If the timer is already `ref`d calling
`ref` again will have no effect.

Returns the timer.

## setImmediate(callback[, arg][, ...])
<!-- YAML
added: v0.9.1
-->

Schedules "immediate" execution of `callback` after I/O events'
callbacks and before timers set by [`setTimeout`][] and [`setInterval`][] are
triggered. Returns an `immediateObject` for possible use with
[`clearImmediate`][]. Additional optional arguments may be passed to the
callback.

Callbacks for immediates are queued in the order in which they were created.
The entire callback queue is processed every event loop iteration. If an
immediate is queued from inside an executing callback, that immediate won't fire
until the next event loop iteration.

## setInterval(callback, delay[, arg][, ...])
<!-- YAML
added: v0.0.1
-->

Schedules repeated execution of `callback` every `delay` milliseconds.
Returns a `intervalObject` for possible use with [`clearInterval`][]. Additional
optional arguments may be passed to the callback.

To follow browser behavior, when using delays larger than 2147483647
milliseconds (approximately 25 days) or less than 1, Node.js will use 1 as the
`delay`.

## setTimeout(callback, delay[, arg][, ...])
<!-- YAML
added: v0.0.1
-->

Schedules execution of a one-time `callback` after `delay` milliseconds.
Returns a `timeoutObject` for possible use with [`clearTimeout`][]. Additional
optional arguments may be passed to the callback.

The callback will likely not be invoked in precisely `delay` milliseconds.
Node.js makes no guarantees about the exact timing of when callbacks will fire,
nor of their ordering. The callback will be called as close as possible to the
time specified.

To follow browser behavior, when using delays larger than 2147483647
milliseconds (approximately 25 days) or less than 1, the timeout is executed
immediately, as if the `delay` was set to 1.

## unref()
<!-- YAML
added: v0.9.1
-->

The opaque value returned by [`setTimeout`][] and [`setInterval`][] also has the
method `timer.unref()` which allows the creation of a timer that is active but
if it is the only item left in the event loop, it won't keep the program
running. If the timer is already `unref`d calling `unref` again will have no
effect.

In the case of [`setTimeout`][], `unref` creates a separate timer that will
wakeup the event loop, creating too many of these may adversely effect event
loop performance -- use wisely.

Returns the timer.

[`clearImmediate`]: timers.html#timers_clearimmediate_immediateobject
[`clearInterval`]: timers.html#timers_clearinterval_intervalobject
[`clearTimeout`]: timers.html#timers_cleartimeout_timeoutobject
[`setImmediate`]: timers.html#timers_setimmediate_callback_arg
[`setInterval`]: timers.html#timers_setinterval_callback_delay_arg
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_arg
