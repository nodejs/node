# Green threads

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- source_link=lib/green_threads.js -->

The `node:green_threads` module runs JavaScript in lightweight, isolated
threads of execution. It is available only when the
[`--experimental-green-threads`][] flag is used.

```mjs
import { GreenThread } from 'node:green_threads';
```

```cjs
const { GreenThread } = require('node:green_threads');
```

This module is only available under the `node:` scheme. Loading it emits an
`ExperimentalWarning`.

Each green thread has its own V8 isolate, so it has its own heap and garbage
collector, and it can be terminated without affecting any other thread.
Unlike a [`Worker`][], a green thread does not get its own OS thread, event
loop, or Node.js environment. Green threads are scheduled onto a small,
process-wide pool of OS threads, which makes them much cheaper to create.

A green thread only has the JavaScript built-ins defined by ECMAScript, plus
the following globals:

* `postMessage(value)`: Sends a structured clone of `value` to the host. The
  host receives it as a [`'message'`][] event.
* `onmessage`: Set this to a function to receive messages sent with
  [`greenThread.postMessage()`][]. The function is called with an object
  whose `data` property holds the message.
* `close()`: Stops the green thread once the current task has completed. The
  exit code is `0`.
* `queueMicrotask(callback)`
* `setTimeout(callback, delay)`, `clearTimeout(id)`,
  `setInterval(callback, delay)`, `clearInterval(id)`. Extra arguments are not
  passed to `callback`.
* `console.log()`, `console.info()`, `console.warn()`, `console.error()`,
  `console.debug()`: Forwarded to the host's `console`. Arguments that can't
  be cloned are forwarded as strings.

Green threads have no access to `require()`, `import`, `process`, or any
other Node.js API.

```cjs
const { GreenThread } = require('node:green_threads');

const greenThread = new GreenThread(`
  onmessage = ({ data }) => postMessage(data * 2);
`);
greenThread.on('message', (value) => {
  console.log(value);  // Prints: 42
  greenThread.terminate();
});
greenThread.postMessage(21);
```

An uncaught exception or unhandled promise rejection inside a green thread
emits an [`'error'`][] event on the host, followed by an [`'exit'`][] event
with exit code `1`.

## Scheduling

A green thread is not tied to an OS thread. Each message, evaluation, and
timer callback is a separate task, and the tasks of a green thread run one at
a time, in order, but not necessarily on the same OS thread. Green threads are
not preempted: a task that never returns keeps one pool thread busy until the
green thread is terminated. Other green threads keep running on the remaining
pool threads.

Timers belong to the message or evaluation that created them, or to the
initial script, and their callbacks run on the same OS thread as that task.

## Class: `GreenThread`

<!-- YAML
added: REPLACEME
-->

* Extends: {EventEmitter}

### `new GreenThread(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {string} The script to run in the green thread.
* `options` {Object}
  * `filename` {string} The file name used in stack traces.
    **Default:** `'green-thread.js'`.
  * `resourceLimits` {Object}
    * `maxOldGenerationSizeMb` {number} The maximum size of the main heap, in
      MB. `0` uses the V8 default. **Default:** `0`.
    * `maxYoungGenerationSizeMb` {number} The maximum size of the heap space
      for recently created objects, in MB. `0` uses the V8 default.
      **Default:** `0`.

Starts a green thread that runs `source`. If the heap limit is reached, the
green thread emits an [`'error'`][] event with an
[`ERR_GREEN_THREAD_OUT_OF_MEMORY`][] error and exits.

### Event: `'error'`

<!-- YAML
added: REPLACEME
-->

* `error` {any}

Emitted when the green thread throws an uncaught exception, has an unhandled
promise rejection, or runs out of memory. The green thread then exits.

### Event: `'exit'`

<!-- YAML
added: REPLACEME
-->

* `exitCode` {integer}

Emitted once the green thread has stopped. The exit code is `0` if the green
thread called `close()`, and `1` otherwise.

### Event: `'message'`

<!-- YAML
added: REPLACEME
-->

* `value` {any}

Emitted when the green thread calls `postMessage()`.

### `greenThread.evaluate(source[, options])`

<!-- YAML
added: REPLACEME
-->

* `source` {string}
* `options` {Object}
  * `filename` {string} The file name used in stack traces.
    **Default:** `'green-thread-eval.js'`.
* Returns: {Promise}

Runs `source` as a script in the green thread. The promise is fulfilled with a
structured clone of the script's completion value. If that value is a promise,
the green thread waits for it to settle first. The promise is rejected if the
script throws, or if the green thread exits first, with an
[`ERR_GREEN_THREAD_EXITED`][] error.

Exceptions thrown by `source` do not stop the green thread.

```cjs
const { GreenThread } = require('node:green_threads');

const greenThread = new GreenThread('globalThis.counter = 0;');
(async () => {
  console.log(await greenThread.evaluate('++counter'));  // Prints: 1
  await greenThread.terminate();
})();
```

### `greenThread.exitCode`

<!-- YAML
added: REPLACEME
-->

* Type: {integer|undefined}

The exit code, or `undefined` while the green thread is running.

### `greenThread.postMessage(value)`

<!-- YAML
added: REPLACEME
-->

* `value` {any}

Sends a structured clone of `value` to the green thread's `onmessage` handler.
Messages are dropped if `onmessage` is not set. Calling this after the green
thread has exited has no effect.

### `greenThread.ref()`

<!-- YAML
added: REPLACEME
-->

* Returns: {GreenThread}

Undoes a previous `unref()` call. This is the default: a running green thread
keeps the event loop alive.

### `greenThread.terminate()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Promise} Fulfilled with the exit code.

Stops the green thread as soon as possible, interrupting any JavaScript that
is running.

### `greenThread.unref()`

<!-- YAML
added: REPLACEME
-->

* Returns: {GreenThread}

Allows the process to exit while the green thread is still running. Any
green threads that are still running when the process exits are terminated.

### `greenThread[Symbol.asyncDispose]()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Promise}

Calls [`greenThread.terminate()`][].

[`'error'`]: #event-error
[`'exit'`]: #event-exit
[`'message'`]: #event-message
[`--experimental-green-threads`]: cli.md#--experimental-green-threads
[`ERR_GREEN_THREAD_EXITED`]: errors.md#err_green_thread_exited
[`ERR_GREEN_THREAD_OUT_OF_MEMORY`]: errors.md#err_green_thread_out_of_memory
[`Worker`]: worker_threads.md#class-worker
[`greenThread.postMessage()`]: #greenthreadpostmessagevalue
[`greenThread.terminate()`]: #greenthreadterminate
