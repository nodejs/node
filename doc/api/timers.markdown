## Timers

### setTimeout(callback, delay, [arg], [...])

To schedule execution of `callback` after `delay` milliseconds. Returns a
`timeoutId` for possible use with `clearTimeout()`. Optionally, you can
also pass arguments to the callback.

### clearTimeout(timeoutId)

Prevents a timeout from triggering.

### setInterval(callback, delay, [arg], [...])

To schedule the repeated execution of `callback` every `delay` milliseconds.
Returns a `intervalId` for possible use with `clearInterval()`. Optionally,
you can also pass arguments to the callback.

### clearInterval(intervalId)

Stops a interval from triggering.
