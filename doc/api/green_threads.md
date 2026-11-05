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
a time, in order, but not necessarily on the same OS thread. Other than
through [CPU time limits][], green threads are not preempted: a task that
never returns keeps one pool thread busy until the green thread is
terminated. Other green threads keep running on the remaining pool threads.

Timers belong to the message or evaluation that created them, or to the
initial script, and their callbacks run on the same OS thread as that task.

## Resource limits

Each green thread can have its own CPU time and memory limits, which are set
with the `resourceLimits` option of the [`GreenThread`][] constructor. A green
thread that reaches a limit is stopped, without affecting any other thread.

### CPU time limits

`maxTaskCpuTimeMs` limits the CPU time of each task, and `maxCpuTimeMs` that of
all tasks of the green thread together. A task includes the microtasks that
run after it, such as promise reactions. Time spent waiting, for example for
a timer, does not count.

When a task reaches either limit, it is interrupted, even if it never yields:
the green thread emits an [`'error'`][] event with an
[`ERR_GREEN_THREAD_CPU_LIMIT`][] error and exits. Limits are enforced within
about a millisecond, and by the time a task is interrupted it may have used
slightly more CPU time than allowed.

```cjs
const { GreenThread } = require('node:green_threads');

const greenThread = new GreenThread('for (;;);', {
  resourceLimits: { maxTaskCpuTimeMs: 50 },
});
greenThread.on('error', (error) => {
  console.log(error.code);  // Prints: ERR_GREEN_THREAD_CPU_LIMIT
});
```

### Memory limits

`maxMemoryMb` limits the memory used by the JavaScript heap and by
`ArrayBuffer`s together. An `ArrayBuffer` allocation that would exceed the
limit throws a `RangeError`, which the green thread can catch. To leave room
for the heap to grow until the next garbage collection, such allocations fail
once the heap and `ArrayBuffer`s together would use more than 15/16 of the
limit. When the heap
together with the `ArrayBuffer`s still in use exceeds the limit, the green
thread emits an [`'error'`][] event with an [`ERR_GREEN_THREAD_OUT_OF_MEMORY`][]
error and exits. This is checked after each garbage collection, so memory use
may briefly exceed the limit.

`maxOldGenerationSizeMb` and `maxYoungGenerationSizeMb` limit only the heap,
like the [`Worker`][] options of the same names.

### Green threads that don't stop

Resource limits and [`greenThread.terminate()`][] interrupt the running
JavaScript code. In rare cases, V8 compiles code in a way that no longer
handles interrupts, so the green thread keeps running. Node.js then stops
waiting for it after one second: the green thread emits the [`'error'`][]
event for the limit it reached, if any, and the [`'exit'`][] event with exit
code `1`, and the process emits a `GreenThreadWarning`. The green thread is
abandoned: it never runs again, and the memory it uses is not freed until the
process exits.

Node.js then stops the pool thread that was running it as soon as that thread
runs JavaScript code, so that it can run other green threads again. Where that
isn't possible, or doesn't succeed within 10 seconds, Node.js aborts the
process. Stopping pool threads is only supported on Linux on x64 and arm64.

## `greenThreads.setWarmPoolSize(size)`

<!-- YAML
added: REPLACEME
-->

* `size` {integer} An integer between `0` and `1024`.
* Returns: {Promise} Fulfilled with `undefined` once the pool holds `size`
  isolates that are ready to use.

Sets how many green threads are prepared ahead of time, for the whole process.
**Default:** `0`.

```cjs
const { GreenThread, setWarmPoolSize } = require('node:green_threads');

(async () => {
  await setWarmPoolSize(16);
  // These start without having to create an isolate.
  const greenThreads = Array.from({ length: 16 }, () => new GreenThread(''));
})();
```

Most of the time it takes to start a green thread is spent creating its V8
isolate. Isolates in the warm pool are created in the background, when pool
threads have nothing else to do, so green threads that use them start almost
immediately. Each isolate is used by one green thread only: the pool is
refilled after a green thread takes one. Isolates in the warm pool use memory,
about 3 MB each.

Only green threads without the `maxMemoryMb`, `maxOldGenerationSizeMb` and
`maxYoungGenerationSizeMb` options, and in their default isolate group, use the
warm pool.

## Isolate groups

When Node.js is built with `--experimental-enable-pointer-compression`, V8
limits the heaps of all isolates in an isolate group to 4 GB together. Isolates
in different groups are independent of each other: each group has its own
pointer compression cage and its own copy of the read-only part of the heap.
[`IsolateGroup.supported`][] tells whether this build of Node.js supports
creating isolate groups.

Where it does, each green thread gets an isolate group of its own by default,
so that green threads can't use up the heap space of the main thread or of
each other. Isolates in groups that only green threads use also start from
V8's own startup snapshot, which is smaller than the one Node.js uses, so they
use less memory. The `isolateGroup` option of the [`GreenThread`][] constructor
selects the group instead:

* `'new'`: a group of its own. The default where isolate groups are supported.
* `'default'`: V8's default group, which the main thread and workers are in
  unless they have groups of their own. The default where isolate groups are
  not supported.
* An [`IsolateGroup`][]: a group that green threads share.

```cjs
const { GreenThread, IsolateGroup } = require('node:green_threads');

if (IsolateGroup.supported) {
  const group = new IsolateGroup();
  // Both green threads together can use at most 4 GB of heap.
  const a = new GreenThread('', { isolateGroup: group });
  const b = new GreenThread('', { isolateGroup: group });
}
```

## Class: `IsolateGroup`

<!-- YAML
added: REPLACEME
-->

A V8 isolate group that green threads can share. See [Isolate groups][].

### `new IsolateGroup()`

<!-- YAML
added: REPLACEME
-->

Creates an isolate group. Throws an
[`ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED`][] error if this build of Node.js
doesn't support isolate groups.

### Static property: `IsolateGroup.supported`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Whether this build of Node.js supports isolate groups.

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
  * `isolateGroup` {string|IsolateGroup} The isolate group of the green
    thread: `'new'`, `'default'`, or an {IsolateGroup}. See
    [Isolate groups][]. **Default:** `'new'` if this build of Node.js supports
    isolate groups, `'default'` otherwise.
  * `resourceLimits` {Object}
    * `maxOldGenerationSizeMb` {number} The maximum size of the main heap, in
      MB. `0` uses the V8 default. **Default:** `0`.
    * `maxYoungGenerationSizeMb` {number} The maximum size of the heap space
      for recently created objects, in MB. `0` uses the V8 default.
      **Default:** `0`.
    * `maxMemoryMb` {number} The maximum size of the heap and `ArrayBuffer`s
      together, in MB. Also limits the main heap, unless
      `maxOldGenerationSizeMb` is set. `0` means no limit. **Default:** `0`.
    * `maxCpuTimeMs` {number} The maximum CPU time of all tasks together, in
      milliseconds. `0` means no limit. **Default:** `0`.
    * `maxTaskCpuTimeMs` {number} The maximum CPU time of each task, in
      milliseconds. `0` means no limit. **Default:** `0`.

Starts a green thread that runs `source`. See [Resource limits][] for what
happens when a limit is reached.

### Event: `'error'`

<!-- YAML
added: REPLACEME
-->

* `error` {any}

Emitted when the green thread throws an uncaught exception, has an unhandled
promise rejection, or reaches a [resource limit][Resource limits]. The green
thread then exits.

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

[CPU time limits]: #cpu-time-limits
[Isolate groups]: #isolate-groups
[Resource limits]: #resource-limits
[`'error'`]: #event-error
[`'exit'`]: #event-exit
[`'message'`]: #event-message
[`--experimental-green-threads`]: cli.md#--experimental-green-threads
[`ERR_GREEN_THREAD_CPU_LIMIT`]: errors.md#err_green_thread_cpu_limit
[`ERR_GREEN_THREAD_EXITED`]: errors.md#err_green_thread_exited
[`ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED`]: errors.md#err_green_thread_isolate_groups_unsupported
[`ERR_GREEN_THREAD_OUT_OF_MEMORY`]: errors.md#err_green_thread_out_of_memory
[`GreenThread`]: #new-greenthreadsource-options
[`IsolateGroup.supported`]: #static-property-isolategroupsupported
[`IsolateGroup`]: #class-isolategroup
[`Worker`]: worker_threads.md#class-worker
[`greenThread.postMessage()`]: #greenthreadpostmessagevalue
[`greenThread.terminate()`]: #greenthreadterminate
