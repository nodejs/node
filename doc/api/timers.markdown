# Timers

    Stability: 5 - Locked

All of the timer functions are globals.  You do not need to `require()`
this module in order to use them.

## setTimeout(callback, delay, [arg], [...])

To schedule execution of a one-time `callback` after `delay` milliseconds. Returns a
`timeoutId` for possible use with `clearTimeout()`. Optionally you can
also pass arguments to the callback.

It is important to note that your callback will probably not be called in exactly
`delay` milliseconds - Node.js makes no guarantees about the exact timing of when
the callback will fire, nor of the ordering things will fire in. The callback will
be called as close as possible to the time specified.

## clearTimeout(timeoutId)

Prevents a timeout from triggering.

## setInterval(callback, delay, [arg], [...])

To schedule the repeated execution of `callback` every `delay` milliseconds.
Returns a `intervalId` for possible use with `clearInterval()`. Optionally
you can also pass arguments to the callback.

## clearInterval(intervalId)

Stops a interval from triggering.
