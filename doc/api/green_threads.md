# Green threads

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early development

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

* `postMessage(value[, transfer])`: Sends a structured clone of `value` to the
  host. The host receives it as a [`'message'`][] event. `transfer` is as for
  [`greenThread.postMessage()`][].
* `onmessage`: Set this to a function to receive messages sent with
  [`greenThread.postMessage()`][]. The function is called with an object
  whose `data` property holds the message, and whose `ports` property holds
  the [`MessagePort`][]s that were transferred with it.
* `close()`: Stops the green thread once the current task has completed. The
  exit code is `0`.
* `queueMicrotask(callback)`
* `setTimeout(callback, delay)`, `clearTimeout(id)`,
  `setInterval(callback, delay)`, `clearInterval(id)`. Extra arguments are not
  passed to `callback`.
* `console.log()`, `console.info()`, `console.warn()`, `console.error()`,
  `console.debug()`: Forwarded to the host's `console`. Arguments that can't
  be cloned are forwarded as strings.
* [`EventTarget`][], [`Event`][], [`AbortController`][], [`AbortSignal`][] and
  [`DOMException`][]. There is no DOM tree, so events are only dispatched to
  their target. Errors thrown by event listeners are uncaught errors of the
  green thread.
* [`MessageChannel`][], [`MessagePort`][] and `MessageEvent`. See
  [Message ports][].
* `env`: The `env` option of the [`GreenThread`][] constructor, if it was
  given. See [RPC][].
* [`RpcTarget`][]: See [RPC][].

Green threads have no access to `require()`, `process`, or any other Node.js
API. They can only import the modules that the host supplies. See
[ES modules][].

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

## RPC

A green thread can only use what its host passes it. Besides messages, the
host can give it an `env` object whose functions the green thread can call,
and the green thread can pass functions and objects back. Calls return
promises, which are fulfilled with structured clones of what the call returns.

```cjs
const { GreenThread, RpcTarget } = require('node:green_threads');

class Counter extends RpcTarget {
  #count = 0;
  increment() {
    return ++this.#count;
  }
}

const store = new Map();
const greenThread = new GreenThread(`
  (async () => {
    await env.db.put('greeting', 'hello');
    const counter = await env.newCounter();
    await counter.increment();
    postMessage([await env.db.get('greeting'), await counter.increment()]);
  })();
`, {
  env: {
    db: {
      get: (key) => store.get(key),
      put: (key, value) => { store.set(key, value); },
    },
    newCounter: () => new Counter(),
  },
});
greenThread.on('message', (value) => {
  console.log(value);  // Prints: [ 'hello', 2 ]
  greenThread.terminate();
});
```

These values are not cloned, but passed as stubs that call the side they came
from:

* Functions in the `env` object, at any depth within plain objects and arrays.
* Functions that are passed directly as arguments, or returned.
* [`RpcTarget`][] instances, anywhere in arguments and results. Their stubs
  have the methods of the instance, including inherited ones, but no other
  properties.

A stub that is passed back to the side it came from is the original function
or object again. Each side can call the other's stubs: a green thread can
pass the host a callback, or an `RpcTarget` of its own.

[`AbortSignal`][]s can be passed too, anywhere in arguments and results. The
other side gets an `AbortSignal` that aborts with a clone of the reason when
the original does.

Errors thrown by a call reject its promise with a structured clone of the
error. [`DOMException`][]s keep their class. Values that can't be cloned are
passed as strings.

Once a side no longer uses a stub, and it is garbage collected, or disposed
with `stub[Symbol.dispose]()`, the other side releases the function or object
that it refers to. When a green thread exits, calls that are pending, and
calls to its stubs on the host, are rejected with [`ERR_GREEN_THREAD_EXITED`][].

The result of a call that a green thread makes continues in the invocation
context that made it, so that timers it creates run on the same pool thread,
as if no call had been made.

## Message ports

Green threads have the web's [`MessageChannel`][] and [`MessagePort`][]. Their
ports are the same as those of `node:worker_threads` underneath, so a port can
connect green threads with each other, with their host, and with
[`Worker`][]s. Ports are transferred
with `postMessage()` on either side, or with a port's own `postMessage()`, by
listing them in the transfer list:

```cjs
const { GreenThread } = require('node:green_threads');
const { MessageChannel } = require('node:worker_threads');

const greenThread = new GreenThread(`
  onmessage = ({ ports: [port] }) => {
    port.onmessage = ({ data }) => port.postMessage(data * 2);
  };
`);
const { port1, port2 } = new MessageChannel();
greenThread.postMessage('port', [port2]);
port1.once('message', (value) => {
  console.log(value);  // Prints: 42
  port1.close();
  greenThread.terminate();
});
port1.postMessage(21);
```

In green threads, ports are [`EventTarget`][]s. They emit `'message'` and
`'messageerror'` events, which are `MessageEvent`s, and a `'close'` event once
either port of the channel is closed. As in `node:worker_threads`, a port starts
delivering messages once `start()` is called, `onmessage` is set, or a
`'message'` listener is added. Ports stay open until they are closed or
transferred, or their green thread exits.

Ports can carry what `postMessage()` can, including transferred `ArrayBuffer`s
and other ports, but not [RPC][] stubs. A message from Node.js that a green
thread can't receive, such as one with a {FileHandle} or a
`WebAssembly.Module`, is a `'messageerror'` event whose `data` is a
`DataCloneError`.

## ES modules

With `type: 'module'`, `source` is an ES module. It can import other modules,
which the host supplies, and its exports can be called from the host:

```cjs
const { GreenThread } = require('node:green_threads');

const greenThread = new GreenThread(`
  import { scale } from './scale.js';
  export function resize(width, height) {
    return [scale(width), scale(height)];
  }
`, {
  type: 'module',
  filename: 'file:///app/main.js',
  modules: {
    'file:///app/scale.js': 'export const scale = (x) => x / 2;',
  },
});
greenThread.exports.then(async ({ resize }) => {
  console.log(await resize(640, 480));  // Prints: [ 320, 240 ]
  greenThread.terminate();
});
```

Green threads have no access to the file system, so modules come from the
host:

* Specifiers that are URLs, or start with `/`, `./` or `../`, are resolved as
  URLs, relative to the URL of the importing module. Other specifiers are
  resolved with the `resolve` hook. `node:` URLs can't be imported.
* Modules are taken from `modules`, by URL, and otherwise from the `load`
  hook.

The hooks run on the host's thread, and may be asynchronous. They are called
at most once for each specifier and module:

* `resolve(specifier, context)`: `context` has the `referrer`, the URL of the
  importing module, and the import `attributes`. Returns `{ url }`, where
  `url` is an absolute URL, or `undefined` if the specifier can't be resolved.
* `load(url, context)`: `context` has the import `attributes`. Returns
  `{ source, format }`, where `source` is a string, an `ArrayBuffer` or a
  `TypedArray` containing UTF-8, and `format` is `'module'` (the default) or
  `'json'`, or `undefined` if there is no such module. JSON modules have to be
  imported `with { type: 'json' }`.

What can't be imported is an `ERR_MODULE_NOT_FOUND` error. A green thread
fetches all modules that a module imports before it runs any of them, also
for `import()`. Scripts can use `import()` too. `import.meta.url` is the
module's URL.

[`greenThread.exports`][] is a promise for the exports of the main module.
Its functions are stubs, as with [RPC][]: each call runs in the green thread,
in an invocation context of its own, as messages do. Other exports are
copies, taken when the main module has been evaluated. Exported classes
can't be constructed from the host. Export a function that returns an
[`RpcTarget`][] instead.

Errors of green threads, including module loading errors, reach the host as
structured clones, which have no `code` property.

## Class: `RpcTarget`

<!-- YAML
added: REPLACEME
-->

The base class of objects that are passed as stubs, whose methods the other
side can call. Green threads have it as a global too.

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
may briefly exceed the limit. See [`greenThread.postMessage()`][] for
`ArrayBuffer`s that are transferred.

`maxOldGenerationSizeMb` and `maxYoungGenerationSizeMb` limit only the heap,
like the [`Worker`][] options of the same names.

### Green threads that don't stop

Resource limits and [`greenThread.terminate()`][] interrupt the running
JavaScript code. In rare cases, V8 compiles code in a way that no longer
handles interrupts, so the green thread keeps running. Node.js then stops
waiting for it after a grace period, one second by default: the green thread
emits the [`'error'`][]
event for the limit it reached, if any, and the [`'exit'`][] event with exit
code `1`, and the process emits a `GreenThreadWarning`. The green thread is
abandoned: it never runs again, and the memory it uses is not freed until the
process exits.

Node.js then stops the pool thread that was running it as soon as that thread
runs JavaScript code, so that it can run other green threads again. Where that
isn't possible, or doesn't succeed within 10 seconds by default, Node.js
aborts the process. Stopping pool threads is only supported on Linux on x64
and arm64. [`greenThreads.setUnresponsiveTimeouts()`][] changes both
durations.

## `greenThreads.setUnresponsiveTimeouts(timeouts)`

<!-- YAML
added: REPLACEME
-->

* `timeouts` {Object}
  * `gracePeriodMs` {integer} How long a green thread has to stop after it was
    asked to, in milliseconds, before it is abandoned. Between `1` and
    `2 ** 31 - 1`. **Default:** `1000`.
  * `abortTimeoutMs` {integer|Infinity} How long Node.js tries to stop the
    pool thread running an abandoned green thread, in milliseconds, before it
    aborts the process. Between `0` and `2 ** 31 - 1`, or `Infinity` to never
    abort. **Default:** `10000`.

Sets, for the whole process, how Node.js handles [green threads that don't
stop][]. Omitted values are not changed. The new values apply to green threads
that are asked to stop from then on.

A short grace period may abandon green threads that would have stopped, for
example after a long garbage collection. With an `abortTimeoutMs` of
`Infinity`, a pool thread that can't be stopped keeps using CPU time until the
process exits, and the process doesn't wait for it when it exits.

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

* `source` {string} The script, or with `type: 'module'` the main module, to
  run in the green thread.
* `options` {Object}
  * `env` {Object} The `env` global of the green thread. Functions in it, at
    any depth within plain objects and arrays, and [`RpcTarget`][]s, are
    passed as stubs. Everything else is cloned. See [RPC][].
  * `type` {string} `'script'` or `'module'`. See [ES modules][].
    **Default:** `'script'`.
  * `filename` {string} The file name used in stack traces. For modules, the
    URL of the main module, as an absolute URL or path.
    **Default:** `'green-thread.js'`, or `'file:///green-thread.js'` for
    modules.
  * `modules` {Object} Modules that the green thread can import, by URL. Each
    value is the module's source, or an object like those returned by
    `load()`. See [ES modules][].
  * `resolve` {Function} Resolves specifiers that are neither URLs nor
    relative. See [ES modules][].
  * `load` {Function} Loads modules that are not in `modules`. See
    [ES modules][].
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

### `greenThread.exports`

<!-- YAML
added: REPLACEME
-->

* Type: {Promise|undefined}

For green threads with `type: 'module'`, a promise for the exports of the
main module. It is fulfilled once the main module has been evaluated,
including any top-level `await`, and rejected if the module can't be loaded
or throws, or the green thread exits first. `undefined` for scripts. See
[ES modules][].

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

### `greenThread.postMessage(value[, transfer])`

<!-- YAML
added: REPLACEME
-->

* `value` {any}
* `transfer` {Array|Object} `ArrayBuffer`s and [`MessagePort`][]s to transfer
  rather than copy, or an object whose `transfer` property is such an array.

Sends a structured clone of `value` to the green thread's `onmessage` handler.
Messages are dropped if `onmessage` is not set. Calling this after the green
thread has exited has no effect.

`ArrayBuffer`s and ports in `transfer` are moved to the receiver, and
detached on the sending side, as with [`port.postMessage()`][]. See
[Message ports][]. If the sender or the receiver
has a [`maxMemoryMb`][Memory limits] limit, their contents are copied, so that
they count towards the limit only while the green thread has them. Receiving
an `ArrayBuffer` that the receiving green thread has no memory left for is an
uncaught `RangeError` in it.

`SharedArrayBuffer`s are shared rather than copied. `Atomics.wait()` throws in
green threads, since it would block the pool thread, and with it other green
threads.

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
[ES modules]: #es-modules
[Isolate groups]: #isolate-groups
[Memory limits]: #memory-limits
[Message ports]: #message-ports
[RPC]: #rpc
[Resource limits]: #resource-limits
[`'error'`]: #event-error
[`'exit'`]: #event-exit
[`'message'`]: #event-message
[`--experimental-green-threads`]: cli.md#--experimental-green-threads
[`AbortController`]: globals.md#class-abortcontroller
[`AbortSignal`]: globals.md#class-abortsignal
[`DOMException`]: globals.md#class-domexception
[`ERR_GREEN_THREAD_CPU_LIMIT`]: errors.md#err_green_thread_cpu_limit
[`ERR_GREEN_THREAD_EXITED`]: errors.md#err_green_thread_exited
[`ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED`]: errors.md#err_green_thread_isolate_groups_unsupported
[`ERR_GREEN_THREAD_OUT_OF_MEMORY`]: errors.md#err_green_thread_out_of_memory
[`EventTarget`]: events.md#class-eventtarget
[`Event`]: events.md#class-event
[`GreenThread`]: #new-greenthreadsource-options
[`IsolateGroup.supported`]: #static-property-isolategroupsupported
[`IsolateGroup`]: #class-isolategroup
[`MessageChannel`]: worker_threads.md#class-messagechannel
[`MessagePort`]: worker_threads.md#class-messageport
[`RpcTarget`]: #class-rpctarget
[`Worker`]: worker_threads.md#class-worker
[`greenThread.exports`]: #greenthreadexports
[`greenThread.postMessage()`]: #greenthreadpostmessagevalue-transfer
[`greenThread.terminate()`]: #greenthreadterminate
[`greenThreads.setUnresponsiveTimeouts()`]: #greenthreadssetunresponsivetimeoutstimeouts
[`port.postMessage()`]: worker_threads.md#portpostmessagevalue-transferlist
[green threads that don't stop]: #green-threads-that-dont-stop
