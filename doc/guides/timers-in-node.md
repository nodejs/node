---
title: Timers in Node.js
layout: docs.hbs
---

# Timers in Node.js and beyond

The Timers module in Node.js contains functions that execute code after a set
period of time. Timers do not need to be imported via `require()`, since
all the methods are available globally to emulate the browser JavaScript API.
To fully understand when timer functions will be executed, it's a good idea to
read up on the the Node.js
[Event Loop](../topics/the-event-loop-timers-and-nexttick).

## Controlling the Time Continuum with Node.js

The Node.js API provides several ways of scheduling code to execute at
some point after the present moment. The functions below may seem familiar,
since they are available in most browsers, but Node.js actually provides
its own implementation of these methods. Timers integrate very closely
with the system, and despite the fact that the API mirrors the browser
API, there are some differences in implementation.

### "When I say so" Execution ~ *`setTimeout()`*

`setTimeout()` can be used to schedule code execution after a designated
amount of milliseconds. This function is similar to
[`window.setTimeout()`](https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout)
from the browser JavaScript API, however a string of code cannot be passed
to be executed.

`setTimeout()` accepts a function to execute as its first argument and the
millisecond delay defined as a number as the second argument. Additional
arguments may also be included and these will be passed on to the function. Here
is an example of that:

```js
function myFunc (arg) {
  console.log('arg was => ' + arg);
}

setTimeout(myFunc, 1500, 'funky');
```

The above function `myFunc()` will execute as close to 1500
milliseconds (or 1.5 seconds) as possible due to the call of `setTimeout()`.

The timeout interval that is set cannot be relied upon to execute after
that *exact* number of milliseconds. This is because other executing code that
blocks or holds onto the event loop will push the execution of the timeout
back. The *only* guarantee is that the timeout will not execute *sooner* than
the declared timeout interval.

`setTimeout()` returns a `Timeout` object that can be used to reference the
timeout that was set. This returned object can be used to cancel the timeout (
see `clearTimeout()` below) as well as change the execution behavior (see
`unref()` below).

### "Right after this" Execution ~ *`setImmediate()`*

`setImmediate()` will execute code at the end of the current event loop cycle.
This code will execute *after* any I/O operations in the current event loop and
*before* any timers scheduled for the next event loop. This code execution
could be thought of as happening "right after this", meaning any code following
the `setImmediate()` function call will execute before the `setImmediate()`
function argument.

The first argument to `setImmediate()` will be the function to execute. Any
subsequent arguments will be passed to the function when it is executed.
Here's an example:

```js
console.log('before immediate');

setImmediate((arg) => {
  console.log(`executing immediate: ${arg}`);
}, 'so immediate');

console.log('after immediate');
```

The above function passed to `setImmediate()` will execute after all runnable
code has executed, and the console output will be:

```shell
before immediate
after immediate
executing immediate: so immediate
```

`setImmediate()` returns and `Immediate` object, which can be used to cancel
the scheduled immediate (see `clearImmediate()` below).

Note: Don't get `setImmediate()` confused with `process.nextTick()`. There are
some major ways they differ. The first is that `process.nextTick()` will run
*before* any `Immediate`s that are set as well as before any scheduled I/O.
The second is that `process.nextTick()` is non-clearable, meaning once
code has been scheduled to execute with `process.nextTick()`, the execution
cannot be stopped, just like with a normal function. Refer to [this guide](../topics/the-event-loop-timers-and-nexttick#processnexttick)
to better understand the operation of `process.nextTick()`.

### "Infinite Loop" Execution ~ *`setInterval()`*

If there is a block of code that should execute multiple times, `setInterval()`
can be used to execute that code. `setInterval()` takes a function
argument that will run an infinite number of times with a given millisecond
delay as the second argument. Just like `setTimeout()`, additional arguments
can be added beyond the delay, and these will be passed on to the function call.
Also like `setTimeout()`, the delay cannot be guaranteed because of operations
that may hold on to the event loop, and therefore should be treated as an
approximate delay. See the below example:

```js
function intervalFunc () {
  console.log('Cant stop me now!');
}

setInterval(intervalFunc, 1500);
```
In the above example, `intervalFunc()` will execute about every 1500
milliseconds, or 1.5 seconds, until it is stopped (see below).

Just like `setTimeout()`, `setInterval()` also returns a `Timeout` object which
can be used to reference and modify the interval that was set.

## Clearing the Future

What can be done if a `Timeout` or `Immediate` object needs to be cancelled?
`setTimeout()`, `setImmediate()`, and `setInterval()` return a timer object
that can be used to reference the set `Timeout` or `Immediate` object.
By passing said object into the respective `clear` function, execution of
that object will be halted completely. The respective functions are
`clearTimeout()`, `clearImmediate()`, and `clearInterval()`. See the example
below for an example of each:

```js
let timeoutObj = setTimeout(() => {
  console.log('timeout beyond time');
}, 1500);

let immediateObj = setImmediate(() => {
  console.log('immediately executing immediate');
});

let intervalObj = setInterval(() => {
  console.log('interviewing the interval');
}, 500);

clearTimeout(timeoutObj);
clearImmediate(immediateObj);
clearInterval(intervalObj);
```

## Leaving Timeouts Behind

Remember that `Timeout` objects are returned by `setTimeout` and `setInterval`.
The `Timeout` object provides two functions intended to augment `Timeout`
behavior with `unref()` and `ref()`. If there is a `Timeout` object scheduled
using a `set` function, `unref()` can be called on that object. This will change
the behavior slightly, and not call the `Timeout` object *if it is the last
code to execute*. The `Timeout` object will not keep the process alive, waiting
to execute.

In similar fashion, a `Timeout` object that has had `unref()` called on it
can remove that behavior by calling `ref()` on that same `Timeout` object,
which will then ensure its execution. Be aware, however, that this does
not *exactly* restore the initial behavior for performance reasons. See
below for examples of both:

```js
let timerObj = setTimeout(() => {
  console.log('will i run?');
});

// if left alone, this statement will keep the above
// timeout from running, since the timeout will be the only
// thing keeping the program from exiting
timerObj.unref();

// we can bring it back to life by calling ref() inside
// an immediate
setImmediate(() => {
  timerObj.ref();
});
```
## Further Down the Event Loop

There's much more to the Event Loop and Timers than this guide
has covered. To learn more about the internals of the Node.js
Event Loop and how Timers operate during execution, check out
this Node.js guide: [The Node.js Event Loop, Timers, and
process.nextTick()](../topics/the-event-loop-timers-and-nexttick).
