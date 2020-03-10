# Worker Threads

<!--introduced_in=v10.5.0-->

> Stability: 2 - Stable

The `worker_threads` module enables the use of threads that execute JavaScript
in parallel. To access it:

```js
const worker = require('worker_threads');
```

Workers (threads) are useful for performing CPU-intensive JavaScript operations.
They will not help much with I/O-intensive work. Node.js’s built-in asynchronous
I/O operations are more efficient than Workers can be.

Unlike `child_process` or `cluster`, `worker_threads` can share memory. They do
so by transferring `ArrayBuffer` instances or sharing `SharedArrayBuffer`
instances.

```js
const {
  Worker, isMainThread, parentPort, workerData
} = require('worker_threads');

if (isMainThread) {
  module.exports = function parseJSAsync(script) {
    return new Promise((resolve, reject) => {
      const worker = new Worker(__filename, {
        workerData: script
      });
      worker.on('message', resolve);
      worker.on('error', reject);
      worker.on('exit', (code) => {
        if (code !== 0)
          reject(new Error(`Worker stopped with exit code ${code}`));
      });
    });
  };
} else {
  const { parse } = require('some-js-parsing-library');
  const script = workerData;
  parentPort.postMessage(parse(script));
}
```

The above example spawns a Worker thread for each `parse()` call. In actual
practice, use a pool of Workers instead for these kinds of tasks. Otherwise, the
overhead of creating Workers would likely exceed their benefit.

When implementing a worker pool, use the [`AsyncResource`][] API to inform
diagnostic tools (e.g. in order to provide asynchronous stack traces) about the
correlation between tasks and their outcomes. See
["Using `AsyncResource` for a `Worker` thread pool"][async-resource-worker-pool]
in the `async_hooks` documentation for an example implementation.

Worker threads inherit non-process-specific options by default. Refer to
[`Worker constructor options`][] to know how to customize worker thread options,
specifically `argv` and `execArgv` options.

## `worker.isMainThread`
<!-- YAML
added: v10.5.0
-->

* {boolean}

Is `true` if this code is not running inside of a [`Worker`][] thread.

```js
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  // This re-loads the current file inside a Worker instance.
  new Worker(__filename);
} else {
  console.log('Inside Worker!');
  console.log(isMainThread);  // Prints 'false'.
}
```

## `worker.moveMessagePortToContext(port, contextifiedSandbox)`
<!-- YAML
added: v11.13.0
-->

* `port` {MessagePort} The message port which will be transferred.
* `contextifiedSandbox` {Object} A [contextified][] object as returned by the
  `vm.createContext()` method.

* Returns: {MessagePort}

Transfer a `MessagePort` to a different [`vm`][] Context. The original `port`
object will be rendered unusable, and the returned `MessagePort` instance will
take its place.

The returned `MessagePort` will be an object in the target context, and will
inherit from its global `Object` class. Objects passed to the
[`port.onmessage()`][] listener will also be created in the target context
and inherit from its global `Object` class.

However, the created `MessagePort` will no longer inherit from
[`EventEmitter`][], and only [`port.onmessage()`][] can be used to receive
events using it.

## `worker.parentPort`
<!-- YAML
added: v10.5.0
-->

* {null|MessagePort}

If this thread was spawned as a [`Worker`][], this will be a [`MessagePort`][]
allowing communication with the parent thread. Messages sent using
`parentPort.postMessage()` will be available in the parent thread
using `worker.on('message')`, and messages sent from the parent thread
using `worker.postMessage()` will be available in this thread using
`parentPort.on('message')`.

```js
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
  const worker = new Worker(__filename);
  worker.once('message', (message) => {
    console.log(message);  // Prints 'Hello, world!'.
  });
  worker.postMessage('Hello, world!');
} else {
  // When a message from the parent thread is received, send it back:
  parentPort.once('message', (message) => {
    parentPort.postMessage(message);
  });
}
```

## `worker.receiveMessageOnPort(port)`
<!-- YAML
added: v12.3.0
-->

* `port` {MessagePort}

* Returns: {Object|undefined}

Receive a single message from a given `MessagePort`. If no message is available,
`undefined` is returned, otherwise an object with a single `message` property
that contains the message payload, corresponding to the oldest message in the
`MessagePort`’s queue.

```js
const { MessageChannel, receiveMessageOnPort } = require('worker_threads');
const { port1, port2 } = new MessageChannel();
port1.postMessage({ hello: 'world' });

console.log(receiveMessageOnPort(port2));
// Prints: { message: { hello: 'world' } }
console.log(receiveMessageOnPort(port2));
// Prints: undefined
```

When this function is used, no `'message'` event will be emitted and the
`onmessage` listener will not be invoked.

## `worker.resourceLimits`
<!-- YAML
added: v13.2.0
-->

* {Object}
  * `maxYoungGenerationSizeMb` {number}
  * `maxOldGenerationSizeMb` {number}
  * `codeRangeSizeMb` {number}

Provides the set of JS engine resource constraints inside this Worker thread.
If the `resourceLimits` option was passed to the [`Worker`][] constructor,
this matches its values.

If this is used in the main thread, its value is an empty object.

## `worker.SHARE_ENV`
<!-- YAML
added: v11.14.0
-->

* {symbol}

A special value that can be passed as the `env` option of the [`Worker`][]
constructor, to indicate that the current thread and the Worker thread should
share read and write access to the same set of environment variables.

```js
const { Worker, SHARE_ENV } = require('worker_threads');
new Worker('process.env.SET_IN_WORKER = "foo"', { eval: true, env: SHARE_ENV })
  .on('exit', () => {
    console.log(process.env.SET_IN_WORKER);  // Prints 'foo'.
  });
```

## `worker.threadId`
<!-- YAML
added: v10.5.0
-->

* {integer}

An integer identifier for the current thread. On the corresponding worker object
(if there is any), it is available as [`worker.threadId`][].
This value is unique for each [`Worker`][] instance inside a single process.

## `worker.workerData`
<!-- YAML
added: v10.5.0
-->

An arbitrary JavaScript value that contains a clone of the data passed
to this thread’s `Worker` constructor.

The data is cloned as if using [`postMessage()`][`port.postMessage()`],
according to the [HTML structured clone algorithm][].

```js
const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  const worker = new Worker(__filename, { workerData: 'Hello, world!' });
} else {
  console.log(workerData);  // Prints 'Hello, world!'.
}
```

## Class: `MessageChannel`
<!-- YAML
added: v10.5.0
-->

Instances of the `worker.MessageChannel` class represent an asynchronous,
two-way communications channel.
The `MessageChannel` has no methods of its own. `new MessageChannel()`
yields an object with `port1` and `port2` properties, which refer to linked
[`MessagePort`][] instances.

```js
const { MessageChannel } = require('worker_threads');

const { port1, port2 } = new MessageChannel();
port1.on('message', (message) => console.log('received', message));
port2.postMessage({ foo: 'bar' });
// Prints: received { foo: 'bar' } from the `port1.on('message')` listener
```

## Class: `MessagePort`
<!-- YAML
added: v10.5.0
-->

* Extends: {EventEmitter}

Instances of the `worker.MessagePort` class represent one end of an
asynchronous, two-way communications channel. It can be used to transfer
structured data, memory regions and other `MessagePort`s between different
[`Worker`][]s.

With the exception of `MessagePort`s being [`EventEmitter`][]s rather
than [`EventTarget`][]s, this implementation matches [browser `MessagePort`][]s.

### Event: `'close'`
<!-- YAML
added: v10.5.0
-->

The `'close'` event is emitted once either side of the channel has been
disconnected.

```js
const { MessageChannel } = require('worker_threads');
const { port1, port2 } = new MessageChannel();

// Prints:
//   foobar
//   closed!
port2.on('message', (message) => console.log(message));
port2.on('close', () => console.log('closed!'));

port1.postMessage('foobar');
port1.close();
```

### Event: `'message'`
<!-- YAML
added: v10.5.0
-->

* `value` {any} The transmitted value

The `'message'` event is emitted for any incoming message, containing the cloned
input of [`port.postMessage()`][].

Listeners on this event will receive a clone of the `value` parameter as passed
to `postMessage()` and no further arguments.

### `port.close()`
<!-- YAML
added: v10.5.0
-->

Disables further sending of messages on either side of the connection.
This method can be called when no further communication will happen over this
`MessagePort`.

The [`'close'` event][] will be emitted on both `MessagePort` instances that
are part of the channel.

### `port.postMessage(value[, transferList])`
<!-- YAML
added: v10.5.0
-->

* `value` {any}
* `transferList` {Object[]}

Sends a JavaScript value to the receiving side of this channel.
`value` will be transferred in a way which is compatible with
the [HTML structured clone algorithm][].

In particular, the significant differences to `JSON` are:

* `value` may contain circular references.
* `value` may contain instances of builtin JS types such as `RegExp`s,
  `BigInt`s, `Map`s, `Set`s, etc.
* `value` may contain typed arrays, both using `ArrayBuffer`s
   and `SharedArrayBuffer`s.
* `value` may contain [`WebAssembly.Module`][] instances.
* `value` may not contain native (C++-backed) objects other than `MessagePort`s.

```js
const { MessageChannel } = require('worker_threads');
const { port1, port2 } = new MessageChannel();

port1.on('message', (message) => console.log(message));

const circularData = {};
circularData.foo = circularData;
// Prints: { foo: [Circular] }
port2.postMessage(circularData);
```

`transferList` may be a list of `ArrayBuffer` and `MessagePort` objects.
After transferring, they will not be usable on the sending side of the channel
anymore (even if they are not contained in `value`). Unlike with
[child processes][], transferring handles such as network sockets is currently
not supported.

If `value` contains [`SharedArrayBuffer`][] instances, those will be accessible
from either thread. They cannot be listed in `transferList`.

`value` may still contain `ArrayBuffer` instances that are not in
`transferList`; in that case, the underlying memory is copied rather than moved.

```js
const { MessageChannel } = require('worker_threads');
const { port1, port2 } = new MessageChannel();

port1.on('message', (message) => console.log(message));

const uint8Array = new Uint8Array([ 1, 2, 3, 4 ]);
// This posts a copy of `uint8Array`:
port2.postMessage(uint8Array);
// This does not copy data, but renders `uint8Array` unusable:
port2.postMessage(uint8Array, [ uint8Array.buffer ]);

// The memory for the `sharedUint8Array` will be accessible from both the
// original and the copy received by `.on('message')`:
const sharedUint8Array = new Uint8Array(new SharedArrayBuffer(4));
port2.postMessage(sharedUint8Array);

// This transfers a freshly created message port to the receiver.
// This can be used, for example, to create communication channels between
// multiple `Worker` threads that are children of the same parent thread.
const otherChannel = new MessageChannel();
port2.postMessage({ port: otherChannel.port1 }, [ otherChannel.port1 ]);
```

Because the object cloning uses the structured clone algorithm,
non-enumerable properties, property accessors, and object prototypes are
not preserved. In particular, [`Buffer`][] objects will be read as
plain [`Uint8Array`][]s on the receiving side.

The message object will be cloned immediately, and can be modified after
posting without having side effects.

For more information on the serialization and deserialization mechanisms
behind this API, see the [serialization API of the `v8` module][v8.serdes].

### `port.ref()`
<!-- YAML
added: v10.5.0
-->

Opposite of `unref()`. Calling `ref()` on a previously `unref()`ed port will
*not* let the program exit if it's the only active handle left (the default
behavior). If the port is `ref()`ed, calling `ref()` again will have no effect.

If listeners are attached or removed using `.on('message')`, the port will
be `ref()`ed and `unref()`ed automatically depending on whether
listeners for the event exist.

### `port.start()`
<!-- YAML
added: v10.5.0
-->

Starts receiving messages on this `MessagePort`. When using this port
as an event emitter, this will be called automatically once `'message'`
listeners are attached.

This method exists for parity with the Web `MessagePort` API. In Node.js,
it is only useful for ignoring messages when no event listener is present.
Node.js also diverges in its handling of `.onmessage`. Setting it will
automatically call `.start()`, but unsetting it will let messages queue up
until a new handler is set or the port is discarded.

### `port.unref()`
<!-- YAML
added: v10.5.0
-->

Calling `unref()` on a port will allow the thread to exit if this is the only
active handle in the event system. If the port is already `unref()`ed calling
`unref()` again will have no effect.

If listeners are attached or removed using `.on('message')`, the port will
be `ref()`ed and `unref()`ed automatically depending on whether
listeners for the event exist.

## Class: `Worker`
<!-- YAML
added: v10.5.0
-->

* Extends: {EventEmitter}

The `Worker` class represents an independent JavaScript execution thread.
Most Node.js APIs are available inside of it.

Notable differences inside a Worker environment are:

* The [`process.stdin`][], [`process.stdout`][] and [`process.stderr`][]
  may be redirected by the parent thread.
* The [`require('worker_threads').isMainThread`][] property is set to `false`.
* The [`require('worker_threads').parentPort`][] message port is available.
* [`process.exit()`][] does not stop the whole program, just the single thread,
  and [`process.abort()`][] is not available.
* [`process.chdir()`][] and `process` methods that set group or user ids
  are not available.
* [`process.env`][] is a copy of the parent thread's environment variables,
  unless otherwise specified. Changes to one copy will not be visible in other
  threads, and will not be visible to native add-ons (unless
  [`worker.SHARE_ENV`][] has been passed as the `env` option to the
  [`Worker`][] constructor).
* [`process.title`][] cannot be modified.
* Signals will not be delivered through [`process.on('...')`][Signals events].
* Execution may stop at any point as a result of [`worker.terminate()`][]
  being invoked.
* IPC channels from parent processes are not accessible.
* The [`trace_events`][] module is not supported.
* Native add-ons can only be loaded from multiple threads if they fulfill
  [certain conditions][Addons worker support].

Creating `Worker` instances inside of other `Worker`s is possible.

Like [Web Workers][] and the [`cluster` module][], two-way communication can be
achieved through inter-thread message passing. Internally, a `Worker` has a
built-in pair of [`MessagePort`][]s that are already associated with each other
when the `Worker` is created. While the `MessagePort` object on the parent side
is not directly exposed, its functionalities are exposed through
[`worker.postMessage()`][] and the [`worker.on('message')`][] event
on the `Worker` object for the parent thread.

To create custom messaging channels (which is encouraged over using the default
global channel because it facilitates separation of concerns), users can create
a `MessageChannel` object on either thread and pass one of the
`MessagePort`s on that `MessageChannel` to the other thread through a
pre-existing channel, such as the global one.

See [`port.postMessage()`][] for more information on how messages are passed,
and what kind of JavaScript values can be successfully transported through
the thread barrier.

```js
const assert = require('assert');
const {
  Worker, MessageChannel, MessagePort, isMainThread, parentPort
} = require('worker_threads');
if (isMainThread) {
  const worker = new Worker(__filename);
  const subChannel = new MessageChannel();
  worker.postMessage({ hereIsYourPort: subChannel.port1 }, [subChannel.port1]);
  subChannel.port2.on('message', (value) => {
    console.log('received:', value);
  });
} else {
  parentPort.once('message', (value) => {
    assert(value.hereIsYourPort instanceof MessagePort);
    value.hereIsYourPort.postMessage('the worker is sending this');
    value.hereIsYourPort.close();
  });
}
```

### `new Worker(filename[, options])`
<!-- YAML
added: v10.5.0
changes:
  - version: v14.0.0
    pr-url: https://github.com/nodejs/node/pull/32278
    description: The `transferList` option was introduced.
  - version: v13.12.0
    pr-url: https://github.com/nodejs/node/pull/31664
    description: The `filename` parameter can be a WHATWG `URL` object using
                 `file:` protocol.
  - version: v13.2.0
    pr-url: https://github.com/nodejs/node/pull/26628
    description: The `resourceLimits` option was introduced.
  - version: v13.4.0
    pr-url: https://github.com/nodejs/node/pull/30559
    description: The `argv` option was introduced.
-->

* `filename` {string|URL} The path to the Worker’s main script or module. Must
  be either an absolute path or a relative path (i.e. relative to the
  current working directory) starting with `./` or `../`, or a WHATWG `URL`
  object using `file:` protocol.
  If `options.eval` is `true`, this is a string containing JavaScript code
  rather than a path.
* `options` {Object}
  * `argv` {any[]} List of arguments which would be stringified and appended to
    `process.argv` in the worker. This is mostly similar to the `workerData`
    but the values will be available on the global `process.argv` as if they
    were passed as CLI options to the script.
  * `env` {Object} If set, specifies the initial value of `process.env` inside
    the Worker thread. As a special value, [`worker.SHARE_ENV`][] may be used
    to specify that the parent thread and the child thread should share their
    environment variables; in that case, changes to one thread’s `process.env`
    object will affect the other thread as well. **Default:** `process.env`.
  * `eval` {boolean} If `true` and the first argument is a `string`, interpret
    the first argument to the constructor as a script that is executed once the
    worker is online.
  * `execArgv` {string[]} List of node CLI options passed to the worker.
    V8 options (such as `--max-old-space-size`) and options that affect the
    process (such as `--title`) are not supported. If set, this will be provided
    as [`process.execArgv`][] inside the worker. By default, options will be
    inherited from the parent thread.
  * `stdin` {boolean} If this is set to `true`, then `worker.stdin` will
    provide a writable stream whose contents will appear as `process.stdin`
    inside the Worker. By default, no data is provided.
  * `stdout` {boolean} If this is set to `true`, then `worker.stdout` will
    not automatically be piped through to `process.stdout` in the parent.
  * `stderr` {boolean} If this is set to `true`, then `worker.stderr` will
    not automatically be piped through to `process.stderr` in the parent.
  * `workerData` {any} Any JavaScript value that will be cloned and made
    available as [`require('worker_threads').workerData`][]. The cloning will
    occur as described in the [HTML structured clone algorithm][], and an error
    will be thrown if the object cannot be cloned (e.g. because it contains
    `function`s).
  * `resourceLimits` {Object} An optional set of resource limits for the new
    JS engine instance. Reaching these limits will lead to termination of the
    `Worker` instance. These limits only affect the JS engine, and no external
    data, including no `ArrayBuffer`s. Even if these limits are set, the process
    may still abort if it encounters a global out-of-memory situation.
    * `maxOldGenerationSizeMb` {number} The maximum size of the main heap in MB.
    * `maxYoungGenerationSizeMb` {number} The maximum size of a heap space for
      recently created objects.
    * `codeRangeSizeMb` {number} The size of a pre-allocated memory range
      used for generated code.

### Event: `'error'`
<!-- YAML
added: v10.5.0
-->

* `err` {Error}

The `'error'` event is emitted if the worker thread throws an uncaught
exception. In that case, the worker will be terminated.

### Event: `'exit'`
<!-- YAML
added: v10.5.0
-->

* `exitCode` {integer}

The `'exit'` event is emitted once the worker has stopped. If the worker
exited by calling [`process.exit()`][], the `exitCode` parameter will be the
passed exit code. If the worker was terminated, the `exitCode` parameter will
be `1`.

This is the final event emitted by any `Worker` instance.

### Event: `'message'`
<!-- YAML
added: v10.5.0
-->

* `value` {any} The transmitted value

The `'message'` event is emitted when the worker thread has invoked
[`require('worker_threads').parentPort.postMessage()`][].
See the [`port.on('message')`][] event for more details.

All messages sent from the worker thread will be emitted before the
[`'exit'` event][] is emitted on the `Worker` object.

### Event: `'online'`
<!-- YAML
added: v10.5.0
-->

The `'online'` event is emitted when the worker thread has started executing
JavaScript code.

### `worker.getHeapSnapshot()`
<!-- YAML
added: v13.9.0
-->

* Returns: {Promise} A promise for a Readable Stream containing
  a V8 heap snapshot

Returns a readable stream for a V8 snapshot of the current state of the Worker.
See [`v8.getHeapSnapshot()`][] for more details.

If the Worker thread is no longer running, which may occur before the
[`'exit'` event][] is emitted, the returned `Promise` will be rejected
immediately with an [`ERR_WORKER_NOT_RUNNING`][] error.

### `worker.postMessage(value[, transferList])`
<!-- YAML
added: v10.5.0
-->

* `value` {any}
* `transferList` {Object[]}

Send a message to the worker that will be received via
[`require('worker_threads').parentPort.on('message')`][].
See [`port.postMessage()`][] for more details.

### `worker.ref()`
<!-- YAML
added: v10.5.0
-->

Opposite of `unref()`, calling `ref()` on a previously `unref()`ed worker will
*not* let the program exit if it's the only active handle left (the default
behavior). If the worker is `ref()`ed, calling `ref()` again will have
no effect.

### `worker.resourceLimits`
<!-- YAML
added: v13.2.0
-->

* {Object}
  * `maxYoungGenerationSizeMb` {number}
  * `maxOldGenerationSizeMb` {number}
  * `codeRangeSizeMb` {number}

Provides the set of JS engine resource constraints for this Worker thread.
If the `resourceLimits` option was passed to the [`Worker`][] constructor,
this matches its values.

If the worker has stopped, the return value is an empty object.

### `worker.stderr`
<!-- YAML
added: v10.5.0
-->

* {stream.Readable}

This is a readable stream which contains data written to [`process.stderr`][]
inside the worker thread. If `stderr: true` was not passed to the
[`Worker`][] constructor, then data will be piped to the parent thread's
[`process.stderr`][] stream.

### `worker.stdin`
<!-- YAML
added: v10.5.0
-->

* {null|stream.Writable}

If `stdin: true` was passed to the [`Worker`][] constructor, this is a
writable stream. The data written to this stream will be made available in
the worker thread as [`process.stdin`][].

### `worker.stdout`
<!-- YAML
added: v10.5.0
-->

* {stream.Readable}

This is a readable stream which contains data written to [`process.stdout`][]
inside the worker thread. If `stdout: true` was not passed to the
[`Worker`][] constructor, then data will be piped to the parent thread's
[`process.stdout`][] stream.

### `worker.terminate()`
<!-- YAML
added: v10.5.0
changes:
  - version: v12.5.0
    pr-url: https://github.com/nodejs/node/pull/28021
    description: This function now returns a Promise.
                 Passing a callback is deprecated, and was useless up to this
                 version, as the Worker was actually terminated synchronously.
                 Terminating is now a fully asynchronous operation.
-->

* Returns: {Promise}

Stop all JavaScript execution in the worker thread as soon as possible.
Returns a Promise for the exit code that is fulfilled when the
[`'exit'` event][] is emitted.

### `worker.threadId`
<!-- YAML
added: v10.5.0
-->

* {integer}

An integer identifier for the referenced thread. Inside the worker thread,
it is available as [`require('worker_threads').threadId`][].
This value is unique for each `Worker` instance inside a single process.

### `worker.unref()`
<!-- YAML
added: v10.5.0
-->

Calling `unref()` on a worker will allow the thread to exit if this is the only
active handle in the event system. If the worker is already `unref()`ed calling
`unref()` again will have no effect.

[`'close'` event]: #worker_threads_event_close
[`'exit'` event]: #worker_threads_event_exit
[`AsyncResource`]: async_hooks.html#async_hooks_class_asyncresource
[`Buffer`]: buffer.html
[`ERR_WORKER_NOT_RUNNING`]: errors.html#ERR_WORKER_NOT_RUNNING
[`EventEmitter`]: events.html
[`EventTarget`]: https://developer.mozilla.org/en-US/docs/Web/API/EventTarget
[`MessagePort`]: #worker_threads_class_messageport
[`SharedArrayBuffer`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
[`Uint8Array`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array
[`WebAssembly.Module`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module
[`Worker`]: #worker_threads_class_worker
[`cluster` module]: cluster.html
[`port.on('message')`]: #worker_threads_event_message
[`port.onmessage()`]: https://developer.mozilla.org/en-US/docs/Web/API/MessagePort/onmessage
[`port.postMessage()`]: #worker_threads_port_postmessage_value_transferlist
[`process.abort()`]: process.html#process_process_abort
[`process.chdir()`]: process.html#process_process_chdir_directory
[`process.env`]: process.html#process_process_env
[`process.execArgv`]: process.html#process_process_execargv
[`process.exit()`]: process.html#process_process_exit_code
[`process.stderr`]: process.html#process_process_stderr
[`process.stdin`]: process.html#process_process_stdin
[`process.stdout`]: process.html#process_process_stdout
[`process.title`]: process.html#process_process_title
[`require('worker_threads').isMainThread`]: #worker_threads_worker_ismainthread
[`require('worker_threads').parentPort.on('message')`]: #worker_threads_event_message
[`require('worker_threads').parentPort`]: #worker_threads_worker_parentport
[`require('worker_threads').parentPort.postMessage()`]: #worker_threads_worker_postmessage_value_transferlist
[`require('worker_threads').threadId`]: #worker_threads_worker_threadid
[`require('worker_threads').workerData`]: #worker_threads_worker_workerdata
[`trace_events`]: tracing.html
[`v8.getHeapSnapshot()`]: v8.html#v8_v8_getheapsnapshot
[`vm`]: vm.html
[`Worker constructor options`]: #worker_threads_new_worker_filename_options
[`worker.on('message')`]: #worker_threads_event_message_1
[`worker.postMessage()`]: #worker_threads_worker_postmessage_value_transferlist
[`worker.SHARE_ENV`]: #worker_threads_worker_share_env
[`worker.terminate()`]: #worker_threads_worker_terminate
[`worker.threadId`]: #worker_threads_worker_threadid_1
[Addons worker support]: addons.html#addons_worker_support
[async-resource-worker-pool]: async_hooks.html#async-resource-worker-pool
[HTML structured clone algorithm]: https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm
[Signals events]: process.html#process_signal_events
[Web Workers]: https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API
[browser `MessagePort`]: https://developer.mozilla.org/en-US/docs/Web/API/MessagePort
[child processes]: child_process.html
[contextified]: vm.html#vm_what_does_it_mean_to_contextify_an_object
[v8.serdes]: v8.html#v8_serialization_api
