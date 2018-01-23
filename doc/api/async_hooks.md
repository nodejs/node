# Async Hooks

<!--introduced_in=v8.1.0-->

> Stability: 1 - Experimental

The `async_hooks` module provides an API to register callbacks tracking the
lifetime of asynchronous resources created inside a Node.js application.
It can be accessed using:

```js
const async_hooks = require('async_hooks');
```

## Terminology

An asynchronous resource represents an object with an associated callback.
This callback may be called multiple times, for example, the `connection` event
in `net.createServer`, or just a single time like in `fs.open`. A resource
can also be closed before the callback is called. AsyncHook does not
explicitly distinguish between these different cases but will represent them
as the abstract concept that is a resource.

## Public API

### Overview

Following is a simple overview of the public API.

```js
const async_hooks = require('async_hooks');

// Return the ID of the current execution context.
const eid = async_hooks.executionAsyncId();

// Return the ID of the handle responsible for triggering the callback of the
// current execution scope to call.
const tid = async_hooks.triggerAsyncId();

// Create a new AsyncHook instance. All of these callbacks are optional.
const asyncHook =
    async_hooks.createHook({ init, before, after, destroy, promiseResolve });

// Allow callbacks of this AsyncHook instance to call. This is not an implicit
// action after running the constructor, and must be explicitly run to begin
// executing callbacks.
asyncHook.enable();

// Disable listening for new asynchronous events.
asyncHook.disable();

//
// The following are the callbacks that can be passed to createHook().
//

// init is called during object construction. The resource may not have
// completed construction when this callback runs, therefore all fields of the
// resource referenced by "asyncId" may not have been populated.
function init(asyncId, type, triggerAsyncId, resource) { }

// before is called just before the resource's callback is called. It can be
// called 0-N times for handles (e.g. TCPWrap), and will be called exactly 1
// time for requests (e.g. FSReqWrap).
function before(asyncId) { }

// after is called just after the resource's callback has finished.
function after(asyncId) { }

// destroy is called when an AsyncWrap instance is destroyed.
function destroy(asyncId) { }

// promiseResolve is called only for promise resources, when the
// `resolve` function passed to the `Promise` constructor is invoked
// (either directly or through other means of resolving a promise).
function promiseResolve(asyncId) { }
```

#### `async_hooks.createHook(callbacks)`

<!-- YAML
added: v8.1.0
-->

* `callbacks` {Object} The [Hook Callbacks][] to register
  * `init` {Function} The [`init` callback][].
  * `before` {Function} The [`before` callback][].
  * `after` {Function} The [`after` callback][].
  * `destroy` {Function} The [`destroy` callback][].
* Returns: `{AsyncHook}` Instance used for disabling and enabling hooks

Registers functions to be called for different lifetime events of each async
operation.

The callbacks `init()`/`before()`/`after()`/`destroy()` are called for the
respective asynchronous event during a resource's lifetime.

All callbacks are optional. For example, if only resource cleanup needs to
be tracked, then only the `destroy` callback needs to be passed. The
specifics of all functions that can be passed to `callbacks` is in the
[Hook Callbacks][] section.

```js
const async_hooks = require('async_hooks');

const asyncHook = async_hooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) { },
  destroy(asyncId) { }
});
```

Note that the callbacks will be inherited via the prototype chain:

```js
class MyAsyncCallbacks {
  init(asyncId, type, triggerAsyncId, resource) { }
  destroy(asyncId) {}
}

class MyAddedCallbacks extends MyAsyncCallbacks {
  before(asyncId) { }
  after(asyncId) { }
}

const asyncHook = async_hooks.createHook(new MyAddedCallbacks());
```

##### Error Handling

If any `AsyncHook` callbacks throw, the application will print the stack trace
and exit. The exit path does follow that of an uncaught exception, but
all `uncaughtException` listeners are removed, thus forcing the process to
exit. The `'exit'` callbacks will still be called unless the application is run
with `--abort-on-uncaught-exception`, in which case a stack trace will be
printed and the application exits, leaving a core file.

The reason for this error handling behavior is that these callbacks are running
at potentially volatile points in an object's lifetime, for example during
class construction and destruction. Because of this, it is deemed necessary to
bring down the process quickly in order to prevent an unintentional abort in the
future. This is subject to change in the future if a comprehensive analysis is
performed to ensure an exception can follow the normal control flow without
unintentional side effects.


##### Printing in AsyncHooks callbacks

Because printing to the console is an asynchronous operation, `console.log()`
will cause the AsyncHooks callbacks to be called. Using `console.log()` or
similar asynchronous operations inside an AsyncHooks callback function will thus
cause an infinite recursion. An easily solution to this when debugging is
to use a synchronous logging operation such as `fs.writeSync(1, msg)`. This
will print to stdout because `1` is the file descriptor for stdout and will
not invoke AsyncHooks recursively because it is synchronous.

```js
const fs = require('fs');
const util = require('util');

function debug(...args) {
  // use a function like this one when debugging inside an AsyncHooks callback
  fs.writeSync(1, `${util.format(...args)}\n`);
}
```

If an asynchronous operation is needed for logging, it is possible to keep
track of what caused the asynchronous operation using the information
provided by AsyncHooks itself. The logging should then be skipped when
it was the logging itself that caused AsyncHooks callback to call. By
doing this the otherwise infinite recursion is broken.

#### `asyncHook.enable()`

* Returns: {AsyncHook} A reference to `asyncHook`.

Enable the callbacks for a given `AsyncHook` instance. If no callbacks are
provided enabling is a noop.

The `AsyncHook` instance is disabled by default. If the `AsyncHook` instance
should be enabled immediately after creation, the following pattern can be used.

```js
const async_hooks = require('async_hooks');

const hook = async_hooks.createHook(callbacks).enable();
```

#### `asyncHook.disable()`

* Returns: {AsyncHook} A reference to `asyncHook`.

Disable the callbacks for a given `AsyncHook` instance from the global pool of
AsyncHook callbacks to be executed. Once a hook has been disabled it will not
be called again until enabled.

For API consistency `disable()` also returns the `AsyncHook` instance.

#### Hook Callbacks

Key events in the lifetime of asynchronous events have been categorized into
four areas: instantiation, before/after the callback is called, and when the
instance is destroyed.

##### `init(asyncId, type, triggerAsyncId, resource)`

* `asyncId` {number} A unique ID for the async resource.
* `type` {string} The type of the async resource.
* `triggerAsyncId` {number} The unique ID of the async resource in whose
  execution context this async resource was created.
* `resource` {Object} Reference to the resource representing the async operation,
  needs to be released during _destroy_.

Called when a class is constructed that has the _possibility_ to emit an
asynchronous event. This _does not_ mean the instance must call
`before`/`after` before `destroy` is called, only that the possibility
exists.

This behavior can be observed by doing something like opening a resource then
closing it before the resource can be used. The following snippet demonstrates
this.

```js
require('net').createServer().listen(function() { this.close(); });
// OR
clearTimeout(setTimeout(() => {}, 10));
```

Every new resource is assigned an ID that is unique within the scope of the
current process.

###### `type`

The `type` is a string identifying the type of resource that caused
`init` to be called. Generally, it will correspond to the name of the
resource's constructor.

```text
FSEVENTWRAP, FSREQWRAP, GETADDRINFOREQWRAP, GETNAMEINFOREQWRAP, HTTPPARSER,
JSSTREAM, PIPECONNECTWRAP, PIPEWRAP, PROCESSWRAP, QUERYWRAP, SHUTDOWNWRAP,
SIGNALWRAP, STATWATCHER, TCPCONNECTWRAP, TCPSERVER, TCPWRAP, TIMERWRAP, TTYWRAP,
UDPSENDWRAP, UDPWRAP, WRITEWRAP, ZLIB, SSLCONNECTION, PBKDF2REQUEST,
RANDOMBYTESREQUEST, TLSWRAP, Timeout, Immediate, TickObject
```

There is also the `PROMISE` resource type, which is used to track `Promise`
instances and asynchronous work scheduled by them.

Users are able to define their own `type` when using the public embedder API.

*Note:* It is possible to have type name collisions. Embedders are encouraged
to use unique prefixes, such as the npm package name, to prevent collisions
when listening to the hooks.

###### `triggerId`

`triggerAsyncId` is the `asyncId` of the resource that caused (or "triggered")
the new resource to initialize and that caused `init` to call. This is different
from `async_hooks.executionAsyncId()` that only shows *when* a resource was
created, while `triggerAsyncId` shows *why* a resource was created.


The following is a simple demonstration of `triggerAsyncId`:

```js
async_hooks.createHook({
  init(asyncId, type, triggerAsyncId) {
    const eid = async_hooks.executionAsyncId();
    fs.writeSync(
      1, `${type}(${asyncId}): trigger: ${triggerAsyncId} execution: ${eid}\n`);
  }
}).enable();

require('net').createServer((conn) => {}).listen(8080);
```

Output when hitting the server with `nc localhost 8080`:

```console
TCPSERVERWRAP(2): trigger: 1 execution: 1
TCPWRAP(4): trigger: 2 execution: 0
```

The `TCPSERVERWRAP` is the server which receives the connections.

The `TCPWRAP` is the new connection from the client. When a new
connection is made the `TCPWrap` instance is immediately constructed. This
happens outside of any JavaScript stack (side note: a `executionAsyncId()` of `0`
means it's being executed from C++, with no JavaScript stack above it).
With only that information, it would be impossible to link resources together in
terms of what caused them to be created, so `triggerAsyncId` is given the task of
propagating what resource is responsible for the new resource's existence.

###### `resource`

`resource` is an object that represents the actual async resource that has
been initialized. This can contain useful information that can vary based on
the value of `type`. For instance, for the `GETADDRINFOREQWRAP` resource type,
`resource` provides the hostname used when looking up the IP address for the
hostname in `net.Server.listen()`. The API for accessing this information is
currently not considered public, but using the Embedder API, users can provide
and document their own resource objects. For example, such a resource object
could contain the SQL query being executed.

In the case of Promises, the `resource` object will have `promise` property
that refers to the Promise that is being initialized, and a `parentId` property
set to the `asyncId` of a parent Promise, if there is one, and `undefined`
otherwise. For example, in the case of `b = a.then(handler)`, `a` is considered
a parent Promise of `b`.

*Note*: In some cases the resource object is reused for performance reasons,
it is thus not safe to use it as a key in a `WeakMap` or add properties to it.

###### Asynchronous context example

The following is an example with additional information about the calls to
`init` between the `before` and `after` calls, specifically what the
callback to `listen()` will look like. The output formatting is slightly more
elaborate to make calling context easier to see.

```js
let indent = 0;
async_hooks.createHook({
  init(asyncId, type, triggerAsyncId) {
    const eid = async_hooks.executionAsyncId();
    const indentStr = ' '.repeat(indent);
    fs.writeSync(
      1,
      `${indentStr}${type}(${asyncId}):` +
      ` trigger: ${triggerAsyncId} execution: ${eid}\n`);
  },
  before(asyncId) {
    const indentStr = ' '.repeat(indent);
    fs.writeSync(1, `${indentStr}before:  ${asyncId}\n`);
    indent += 2;
  },
  after(asyncId) {
    indent -= 2;
    const indentStr = ' '.repeat(indent);
    fs.writeSync(1, `${indentStr}after:   ${asyncId}\n`);
  },
  destroy(asyncId) {
    const indentStr = ' '.repeat(indent);
    fs.writeSync(1, `${indentStr}destroy: ${asyncId}\n`);
  },
}).enable();

require('net').createServer(() => {}).listen(8080, () => {
  // Let's wait 10ms before logging the server started.
  setTimeout(() => {
    console.log('>>>', async_hooks.executionAsyncId());
  }, 10);
});
```

Output from only starting the server:

```console
TCPSERVERWRAP(2): trigger: 1 execution: 1
TickObject(3): trigger: 2 execution: 1
before:  3
  Timeout(4): trigger: 3 execution: 3
  TIMERWRAP(5): trigger: 3 execution: 3
after:   3
destroy: 3
before:  5
  before:  4
    TTYWRAP(6): trigger: 4 execution: 4
    SIGNALWRAP(7): trigger: 4 execution: 4
    TTYWRAP(8): trigger: 4 execution: 4
>>> 4
    TickObject(9): trigger: 4 execution: 4
  after:   4
after:   5
before:  9
after:   9
destroy: 4
destroy: 9
destroy: 5
```

*Note*: As illustrated in the example, `executionAsyncId()` and `execution`
each specify the value of the current execution context; which is delineated by
calls to `before` and `after`.

Only using `execution` to graph resource allocation results in the following:

```console
TTYWRAP(6) -> Timeout(4) -> TIMERWRAP(5) -> TickObject(3) -> root(1)
```

The `TCPSERVERWRAP` is not part of this graph, even though it was the reason for
`console.log()` being called. This is because binding to a port without a
hostname is a *synchronous* operation, but to maintain a completely asynchronous
API the user's callback is placed in a `process.nextTick()`.

The graph only shows *when* a resource was created, not *why*, so to track
the *why* use `triggerAsyncId`.


##### `before(asyncId)`

* `asyncId` {number}

When an asynchronous operation is initiated (such as a TCP server receiving a
new connection) or completes (such as writing data to disk) a callback is
called to notify the user. The `before` callback is called just before said
callback is executed. `asyncId` is the unique identifier assigned to the
resource about to execute the callback.

The `before` callback will be called 0 to N times. The `before` callback
will typically be called 0 times if the asynchronous operation was cancelled
or, for example, if no connections are received by a TCP server. Persistent
asynchronous resources like a TCP server will typically call the `before`
callback multiple times, while other operations like `fs.open()` will call
it only once.


##### `after(asyncId)`

* `asyncId` {number}

Called immediately after the callback specified in `before` is completed.

*Note:* If an uncaught exception occurs during execution of the callback, then
`after` will run *after* the `'uncaughtException'` event is emitted or a
`domain`'s handler runs.


##### `destroy(asyncId)`

* `asyncId` {number}

Called after the resource corresponding to `asyncId` is destroyed. It is also
called asynchronously from the embedder API `emitDestroy()`.

*Note:* Some resources depend on garbage collection for cleanup, so if a
reference is made to the `resource` object passed to `init` it is possible that
`destroy` will never be called, causing a memory leak in the application. If
the resource does not depend on garbage collection, then this will not be an
issue.

##### `promiseResolve(asyncId)`

* `asyncId` {number}

Called when the `resolve` function passed to the `Promise` constructor is
invoked (either directly or through other means of resolving a promise).

Note that `resolve()` does not do any observable synchronous work.

*Note:* This does not necessarily mean that the `Promise` is fulfilled or
rejected at this point, if the `Promise` was resolved by assuming the state
of another `Promise`.

For example:

```js
new Promise((resolve) => resolve(true)).then((a) => {});
```

calls the following callbacks:

```text
init for PROMISE with id 5, trigger id: 1
  promise resolve 5      # corresponds to resolve(true)
init for PROMISE with id 6, trigger id: 5  # the Promise returned by then()
  before 6               # the then() callback is entered
  promise resolve 6      # the then() callback resolves the promise by returning
  after 6
```

#### `async_hooks.executionAsyncId()`

<!-- YAML
added: v8.1.0
changes:
  - version: v8.2.0
    pr-url: https://github.com/nodejs/node/pull/13490
    description: Renamed from currentId
-->

* Returns: {number} The `asyncId` of the current execution context. Useful to
  track when something calls.

For example:

```js
const async_hooks = require('async_hooks');

console.log(async_hooks.executionAsyncId());  // 1 - bootstrap
fs.open(path, 'r', (err, fd) => {
  console.log(async_hooks.executionAsyncId());  // 6 - open()
});
```

The ID returned from `executionAsyncId()` is related to execution timing, not
causality (which is covered by `triggerAsyncId()`). For example:

```js
const server = net.createServer(function onConnection(conn) {
  // Returns the ID of the server, not of the new connection, because the
  // onConnection callback runs in the execution scope of the server's
  // MakeCallback().
  async_hooks.executionAsyncId();

}).listen(port, function onListening() {
  // Returns the ID of a TickObject (i.e. process.nextTick()) because all
  // callbacks passed to .listen() are wrapped in a nextTick().
  async_hooks.executionAsyncId();
});
```

#### `async_hooks.triggerAsyncId()`

* Returns: {number} The ID of the resource responsible for calling the callback
  that is currently being executed.

For example:

```js
const server = net.createServer((conn) => {
  // The resource that caused (or triggered) this callback to be called
  // was that of the new connection. Thus the return value of triggerAsyncId()
  // is the asyncId of "conn".
  async_hooks.triggerAsyncId();

}).listen(port, () => {
  // Even though all callbacks passed to .listen() are wrapped in a nextTick()
  // the callback itself exists because the call to the server's .listen()
  // was made. So the return value would be the ID of the server.
  async_hooks.triggerAsyncId();
});
```

## JavaScript Embedder API

Library developers that handle their own asynchronous resources performing tasks
like I/O, connection pooling, or managing callback queues may use the `AsyncWrap`
JavaScript API so that all the appropriate callbacks are called.

### `class AsyncResource()`

The class `AsyncResource` was designed to be extended by the embedder's async
resources. Using this users can easily trigger the lifetime events of their
own resources.

The `init` hook will trigger when an `AsyncResource` is instantiated.

*Note*: `before` and `after` calls must be unwound in the same order that they
are called. Otherwise, an unrecoverable exception will occur and the process
will abort.

The following is an overview of the `AsyncResource` API.

```js
const { AsyncResource, executionAsyncId } = require('async_hooks');

// AsyncResource() is meant to be extended. Instantiating a
// new AsyncResource() also triggers init. If triggerAsyncId is omitted then
// async_hook.executionAsyncId() is used.
const asyncResource = new AsyncResource(
  type, { triggerAsyncId: executionAsyncId(), requireManualDestroy: false }
);

// Call AsyncHooks before callbacks.
asyncResource.emitBefore();

// Call AsyncHooks after callbacks.
asyncResource.emitAfter();

// Call AsyncHooks destroy callbacks.
asyncResource.emitDestroy();

// Return the unique ID assigned to the AsyncResource instance.
asyncResource.asyncId();

// Return the trigger ID for the AsyncResource instance.
asyncResource.triggerAsyncId();
```

#### `AsyncResource(type[, options])`

* `type` {string} The type of async event.
* `options` {Object}
  * `triggerAsyncId` {number} The ID of the execution context that created this
  async event.  **Default:** `executionAsyncId()`
  * `requireManualDestroy` {boolean} Disables automatic `emitDestroy` when the
  object is garbage collected. This usually does not need to be set (even if
  `emitDestroy` is called manually), unless the resource's asyncId is retrieved
  and the sensitive API's `emitDestroy` is called with it.
  **Default:** `false`

Example usage:

```js
class DBQuery extends AsyncResource {
  constructor(db) {
    super('DBQuery');
    this.db = db;
  }

  getInfo(query, callback) {
    this.db.get(query, (err, data) => {
      this.emitBefore();
      callback(err, data);
      this.emitAfter();
    });
  }

  close() {
    this.db = null;
    this.emitDestroy();
  }
}
```

#### `asyncResource.emitBefore()`

* Returns: {undefined}

Call all `before` callbacks to notify that a new asynchronous execution context
is being entered. If nested calls to `emitBefore()` are made, the stack of
`asyncId`s will be tracked and properly unwound.

#### `asyncResource.emitAfter()`

* Returns: {undefined}

Call all `after` callbacks. If nested calls to `emitBefore()` were made, then
make sure the stack is unwound properly. Otherwise an error will be thrown.

If the user's callback throws an exception, `emitAfter()` will automatically be
called for all `asyncId`s on the stack if the error is handled by a domain or
`'uncaughtException'` handler.

#### `asyncResource.emitDestroy()`

* Returns: {undefined}

Call all `destroy` hooks. This should only ever be called once. An error will
be thrown if it is called more than once. This **must** be manually called. If
the resource is left to be collected by the GC then the `destroy` hooks will
never be called.

#### `asyncResource.asyncId()`

* Returns: {number} The unique `asyncId` assigned to the resource.

#### `asyncResource.triggerAsyncId()`

* Returns: {number} The same `triggerAsyncId` that is passed to the `AsyncResource`
constructor.

[`after` callback]: #async_hooks_after_asyncid
[`before` callback]: #async_hooks_before_asyncid
[`destroy` callback]: #async_hooks_destroy_asyncid
[`init` callback]: #async_hooks_init_asyncid_type_triggerasyncid_resource
[Hook Callbacks]: #async_hooks_hook_callbacks
