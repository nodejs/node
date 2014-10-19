# Timers

    Stability: 5 - Locked

All of the timer functions are globals.  You do not need to `require()`
this module in order to use them.

## setTimeout(callback, delay[, arg][, ...])

To schedule execution of a one-time `callback` after `delay` milliseconds. Returns a
`timeoutObject` for possible use with `clearTimeout()`. Optionally you can
also pass arguments to the callback.

It is important to note that your callback will probably not be called in exactly
`delay` milliseconds - Node.js makes no guarantees about the exact timing of when
the callback will fire, nor of the ordering things will fire in. The callback will
be called as close as possible to the time specified.

## clearTimeout(timeoutObject)

Prevents a timeout from triggering.

## setInterval(callback, delay[, arg][, ...])

To schedule the repeated execution of `callback` every `delay` milliseconds.
Returns a `intervalObject` for possible use with `clearInterval()`. Optionally
you can also pass arguments to the callback.

## clearInterval(intervalObject)

Stops an interval from triggering.

## unref()

The opaque value returned by `setTimeout` and `setInterval` also has the method
`timer.unref()` which will allow you to create a timer that is active but if
it is the only item left in the event loop won't keep the program running.
If the timer is already `unref`d calling `unref` again will have no effect.

In the case of `setTimeout` when you `unref` you create a separate timer that
will wakeup the event loop, creating too many of these may adversely effect
event loop performance -- use wisely.

## ref()

If you had previously `unref()`d a timer you can call `ref()` to explicitly
request the timer hold the program open. If the timer is already `ref`d calling
`ref` again will have no effect.

## setImmediate(callback[, arg][, ...])

To schedule the "immediate" execution of `callback` after I/O events
callbacks and before `setTimeout` and `setInterval` . Returns an
`immediateObject` for possible use with `clearImmediate()`. Optionally you
can also pass arguments to the callback.

Callbacks for immediates are queued in the order in which they were created.
The entire callback queue is processed every event loop iteration. If you queue
an immediate from inside an executing callback, that immediate won't fire
until the next event loop iteration.

## clearImmediate(immediateObject)

Stops an immediate from triggering.
