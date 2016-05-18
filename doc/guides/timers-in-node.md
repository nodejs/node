---
title: Timers in node.js
layout: docs.hbs
---

# Timers in node.js and beyond

Timers are a collection of global functions in node.js that allow you to execute code after a set period of time. To fully understand when timer functions will be executed, it's a good idea to read up on the the node [Event Loop](https://nodesource.com/blog/understanding-the-nodejs-event-loop/).

## Code Time Machine

The node API provides several ways to schedule your code to execute at some point after the present moment.

### "When I say so" Execution ~ *setTimeout()*

Using `setTimeout()`, you can schedule code to execute after a designated amount of milliseconds. You may already be familiar with this function, as it is a part of V8 and thus part of the browser JavaScript API.

`setTimeout()` accepts the code to execute as its first argument and the millisecond delay defined as a number literal as the second argument. Here is an example of that:

```javascript
function myFunc () {
  console.log('funky');
}

setTimeout(myFunc, 1500);
```

The above function `myFunc()` will execute after approximately 1500 milliseconds (or 1.5 seconds) due to the call of `setTimeout()`.

The timeout interval that is set cannot be relied upon to execute after that *exact* number of milliseconds. This is because executing code that blocks or holds onto the event loop will push the execution of your timeout back. You *can* guarantee that your timeout will not execute *sooner* than the declared timeout.

### "Right after this" Execution ~ *setImmediate()*

`setImmediate()` allows you to execute code at the beginning of the next event loop cycle. This code will execute *before* any timers or IO operations. I like to think of this code execution as happening "right after this", meaning any code following the `setImmediate()` function call will execute before the `setImmediate()` function argument. Here's an example:

```javascript
let order = 'before immediate\n';

setImmediate(() => {
  order += 'executing immediate\n';
});

order += 'after immediate\n';

console.log(order);
```

The above function passed to `setImmediate()` will execute after all runnable code has executed, and the console output will be:

```shell
before immediate
after immediate
executing immediate
```

Note: `process.nextTick()` is very similar to `setImmediate()`. The two major differences are that `process.nextTick()` will run *before* any immediates that are set. The second is that `process.nextTick()` is non-clearable, meaning once you've scheduled code to execute with `process.nextTick()` you cannot stop that code, unlike with `setImmediate()`.

### "Deja vu" Execution ~ *setInterval()*

If there is a block of code that you want to execute multiple times, you can use `setInterval()` to execute that code. `setInterval()` takes a function argument that will run and infinite number of times with a given millisecond delay. Just like `setTimeout()`, the delay cannot be guaranteed because of operations that may hold on to the event loop, and therefore should be treated as an approximate delay. See the below example:

```javascript
function intervalFunc () {
  console.log('Cant stop me now!');
}

setInterval(intervalFunc, 1500);
```
In the above example, `intervalFunc()` will execute every 1500 milliseconds, or 1.5 seconds, until it is stopped (see below).

## Master of the Timerverse

What would a Code Time Machine be without the ability to turn it off? `setTimeout()`, `setImmediate()`, and `setInterval()` return a timer object that can be used to reference the set timeout, immediate, or interval object. By passing said objective into the respective `clear` function, execution of that object will be halted completely. The respective functions are `clearTimeout()`, `clearImmediate()`, and `clearInterval()`. See the example below for an example of each:

```javascript
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

## Last Train to Nowhere

The node Timer API provides two functions intended to augment timer behavior with `unref()` and `ref()`. If you have a timer object scheduled using a `set` function, you can call `unref()` on that object. This will change the behavior slightly, and not call the timer object *if it is the last code to execute*. Instead, it will let the program exit cleanly.

In similar fashion, a timer object that has had `unref()` called on it can remove that behavior by calling `ref()` on that same timer object, which will then ensure its execution. See below for examples of both:

```javascript
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
