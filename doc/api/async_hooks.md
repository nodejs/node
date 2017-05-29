# Async Hooks

> Stability: 1 - Experimental

The `async_hooks` module provides an API to register callbacks tracking the
lifetime of asynchronous resources created inside a Node.js application.
It can be accessed using:

```js
const async_hooks = require('async_hooks');
```

## Terminology

An async resource represents either a _handle_ or a _request_.

Handles are a reference to a system resource. Each handle can be continually
reused to access and operate on the referenced resource.

Requests are short lived data structures created to accomplish one task. The
callback for a request should always and only ever fire one time, which is when
the assigned task has either completed or encountered an error. Requests are
used by handles to perform tasks such as accepting a new connection or
writing data to disk.

## Public API

### Overview

Following is a simple overview of the public API. All of this API is explained in
more detail further down.

```js
const async_hooks = require('async_hooks');

// Return the id of the current execution context.
const cid = async_hooks.currentId();

// Return the id of the handle responsible for triggering the callback of the
// current execution scope to fire.
const tid = async_hooks.triggerId();

// Create a new AsyncHook instance. All of these callbacks are optional.
const asyncHook = async_hooks.createHook({ init, before, after, destroy });

// Allow callbacks of this AsyncHook instance to fire. This is not an implicit
// action after running the constructor, and must be explicitly run to begin
// executing callbacks.
asyncHook.enable();

// Disable listening for new asynchronous events.
asyncHook.disable();

//
// The following are the callbacks that can be passed to createHook().
//

// init() is called during object construction. The resource may not have
// completed construction when this callback runs, therefore all fields of the
// resource referenced by "id" may not have been populated.
function init(id, type, triggerId, resource) { }

// before() is called just before the resource's callback is called. It can be
// called 0-N times for handles (e.g. TCPWrap), and should be called exactly 1
// time for requests (e.g. FSReqWrap).
function before(id) { }

// after() is called just after the resource's callback has finished.
// This hook will not be called if an uncaught exception occurred.
function after(id) { }

// destroy() is called when an AsyncWrap instance is destroyed. In cases like
// HTTPParser where the resource is reused, or timers where the handle is only
// a JS object, destroy() will be triggered manually soon after after() has
// completed.
function destroy(id) { }
```

#### `async_hooks.createHook(callbacks)`

<!-- YAML
added: REPLACEME
-->

* `callbacks` {Object} the callbacks to register
* Returns: `{AsyncHook}` instance used for disabling and enabling hooks

Registers functions to be called for different lifetime events of each async
operation.
The callbacks `init()`/`before()`/`after()`/`destroy()` are registered via an
`AsyncHook` instance and are called during the respective asynchronous events
in the lifetime of the event loop.

All callbacks are optional. So, for example, if only resource cleanup needs to
be tracked then only the `destroy()` callback needs to be passed. The
specifics of all functions that can be passed to `callbacks` is in the section
`Hook Callbacks`.

##### Error Handling

If any `AsyncHook` callbacks throw, the application will
print the stack trace and exit. The exit path does follow that of any uncaught
exception. However, `'exit'` callbacks will still fire unless the application
is run with `--abort-on-uncaught-exception`, in which case a stack trace will
be printed and the application exits, leaving a core file.

The reason for this error handling behavior is that these callbacks are running
at potentially volatile points in an object's lifetime, for example during
class construction and destruction. Because of this, it is deemed necessary to
bring down the process quickly in order to prevent an unintentional abort in the
future. This is subject to change in the future if a comprehensive analysis is
performed to ensure an exception can follow the normal control flow without
unintentional side effects.


##### Printing in AsyncHooks callbacks

Because printing to the console is an asynchronous operations `console.log()`
will cause the AsyncHooks callbacks to write. Using `console.log()` or similar
asynchronous operations inside an AsyncHooks callback function will thus
cause an infinite recursion. An easily solution to this when debugging is
to use a synchronous logging operation such as `fs.writeSync(1, msg)`. This
will print to stdout because `1` is the file descriptor for stdout and will
not invoke AsyncHooks recursively because it is synchronous.

```js
const fs = require('fs');
const util = require('util');

function debug() {
  // use a function like this one when debugging inside an AsyncHooks callback
  fs.writeSync(1, util.format.apply(null, arguments));
};
```

If an asynchronous operation is needed for logging, it is possible to keep
track of what caused the asynchronous operation using the information
provided by AsyncHooks itself. The logging should then be skipped when
it was the logging itself that caused AsyncHooks callback to fire. By
doing this the otherwise infinite recursion is broken.

#### `asyncHook.enable()`

* Returns {AsyncHook} A reference to `asyncHook`.

Enable the callbacks for a given `AsyncHook` instance.

```js
const async_hooks = require('async_hooks');

const hook = async_hooks.createHook(callbacks).enable();
```

#### `asyncHook.disable()`

* Returns {AsyncHook} A reference to `asyncHook`.

Disable the callbacks for a given `AsyncHook` instance from the global pool of
hooks to be executed. Once a hook has been disabled it will not fire again
until enabled.

For API consistency `disable()` also returns the `AsyncHook` instance.

#### Hook Callbacks

Key events in the lifetime of asynchronous events have been categorized into
four areas: instantiation, before/after the callback is called, and when the
instance is destructed. For cases where resources are reused, instantiation and
destructor calls are emulated.

##### `init(id, type, triggerId, resource)`

* `id` {number} a unique id for the async resource
* `type` {string} the type of the async resource
* `triggerId` {number} the unique id of the async resource in whose
  execution context this async resource was created
* `resource` {Object} reference to the resource representing the async operation,
  needs to be released during _destroy_

Called when a class is constructed that has the _possibility_ to trigger an
asynchronous event. This _does not_ mean the instance must trigger
`before()`/`after()` before `destroy()` is called, only that the possibility
exists.

This behavior can be observed by doing something like opening a resource then
closing it before the resource can be used. The following snippet demonstrates
this.

```js
require('net').createServer().listen(function() { this.close() });
// OR
clearTimeout(setTimeout(() => {}, 10));
```

Every new resource is assigned a unique id.

The `type` is a string that represents the type of resource that caused
`init()` to fire. Generally it will be the name of the resource's constructor.
The resource types provided by the build in Node.js modules are:

```
FSEVENTWRAP, FSREQWRAP, GETADDRINFOREQWRAP, GETNAMEINFOREQWRAP, HTTPPARSER,
JSSTREAM, PIPECONNECTWRAP, PIPEWRAP, PROCESSWRAP, QUERYWRAP, SHUTDOWNWRAP,
SIGNALWRAP, STATWATCHER, TCPCONNECTWRAP, TCPWRAP, TIMERWRAP, TTYWRAP,
UDPSENDWRAP, UDPWRAP, WRITEWRAP, ZLIB, SSLCONNECTION, PBKDF2REQUEST,
RANDOMBYTESREQUEST, TLSWRAP
```

Users are be able to define their own `type` when using the public embedder API.

*Note:* It is possible to have type name collisions. Embedders are recommended
to use unique prefixes per module to prevent collisions when listening to the
hooks.

`triggerId` is the `id` of the resource that caused (or "triggered") the
new resource to initialize and that caused `init()` to fire.

The following is a simple demonstration of this:

```js
const async_hooks = require('async_hooks');

async_hooks.createHook({
  init (id, type, triggerId) {
    const cId = async_hooks.currentId();
    fs.writeSync(1, `${type}(${id}): trigger: ${triggerId} scope: ${cId}\n`);
  }
}).enable();

require('net').createServer(c => {}).listen(8080);
```

Output when hitting the server with `nc localhost 8080`:

```
TCPWRAP(2): trigger: 1 scope: 1
TCPWRAP(4): trigger: 2 scope: 0
```

The second `TCPWRAP` is the new connection from the client. When a new
connection is made the `TCPWrap` instance is immediately constructed. This
happens outside of any JavaScript stack (side note: a `currentId()` of `0`
means it's being executed in "the void", with no JavaScript stack above it).
With only that information it would be impossible to link resources together in
terms of what caused them to be created, so `triggerId` is given the task of
propagating what resource is responsible for the new resource's existence.

Below is another example with additional information about the calls to
`init()` between the `before()` and `after()` calls, specifically what the
callback to `listen()` will look like. The output formatting is slightly more
elaborate to make calling context easier to see.

```js
const async_hooks = require('async_hooks');

let indent = 0;
async_hooks.createHook({
  init(id, type, triggerId) {
    const cId = async_hooks.currentId();
    fs.writeSync(1, ' '.repeat(indent) +
                    `${type}(${id}): trigger: ${triggerId} scope: ${cId}\n`);
  },
  before(id) {
    fs.writeSync(1, ' '.repeat(indent) + `before:  ${id}`);
    indent += 2;
  },
  after(id) {
    indent -= 2;
    fs.writeSync(1, ' '.repeat(indent) + `after:   ${id}`);
  },
  destroy(id) {
    fs.writeSync(1, ' '.repeat(indent) + `destroy: ${id}`);
  },
}).enable();

require('net').createServer(() => {}).listen(8080, () => {
  // Let's wait 10ms before logging the server started.
  setTimeout(() => {
    console.log('>>>', async_hooks.currentId());
  }, 10);
});
```

Output from only starting the server:

```
TCPWRAP(2): trigger: 1 scope: 1
TickObject(3): trigger: 2 scope: 1
before:  3
  Timeout(4): trigger: 3 scope: 3
  TIMERWRAP(5): trigger: 3 scope: 3
after:   3
destroy: 3
before:  5
  before:  4
    TTYWRAP(6): trigger: 4 scope: 4
    SIGNALWRAP(7): trigger: 4 scope: 4
    TTYWRAP(8): trigger: 4 scope: 4
>>> 4
    TickObject(9): trigger: 4 scope: 4
  after:   4
  destroy: 4
after:   5
before:  9
after:   9
destroy: 9
destroy: 5
```

First notice that `scope` and the value returned by `currentId()` are always
the same. That's because `currentId()` simply returns the value of the
current execution context; which is defined by `before()` and `after()` calls.

Now only using `scope` to graph resource allocation results in the following:

```
TTYWRAP(6) -> Timeout(4) -> TIMERWRAP(5) -> TickObject(3) -> root(1)
```

The `TCPWRAP` isn't part of this graph; even though it was the reason for
`console.log()` being called. This is because binding to a port without a
hostname is actually synchronous, but to maintain a completely asynchronous API
the user's callback is placed in a `process.nextTick()`.

The graph only shows **when** a resource was created. Not **why**. So to track
the **why** use `triggerId`.


##### `before(id)`

* `id` {number}

When an asynchronous operation is triggered (such as a TCP server receiving a
new connection) or completes (such as writing data to disk) a callback is
called to notify Node. The `before()` callback is called just before said
callback is executed. `id` is the unique identifier assigned to the
resource about to execute the callback.

The `before()` callback will be called 0-N times if the resource is a handle,
and exactly 1 time if the resource is a request.


##### `after(id)`

* `id` {number}

Called immediately after the callback specified in `before()` is completed. If
an uncaught exception occurs during execution of the callback then `after()`
will run after the `'uncaughtException'` event or a `domain`'s handler runs.


##### `destroy(id)`

* `id` {number}

Called after the resource corresponding to `id` is destroyed. It is also called
asynchronously from the embedder API `emitDestroy()`.

*Note:* Some resources depend on GC for cleanup, so if a reference is made to
the `resource` object passed to `init()` it's possible that `destroy()` is
never called, causing a memory leak in the application. Of course if
the resource doesn't depend on GC then this isn't an issue.

#### `async_hooks.currentId()`

* Returns {number} the `id` of the current execution context. Useful to track when
  something fires.

For example:

```js
console.log(async_hooks.currentId());  // 1 - bootstrap
fs.open(path, (err, fd) => {
  console.log(async_hooks.currentId());  // 2 - open()
}):
```

It is important to note that the id returned fom `currentId()` is related to
execution timing, not causality (which is covered by `triggerId()`). For
example:

```js
const server = net.createServer(function onConnection(conn) {
  // Returns the id of the server, not of the new connection, because the
  // onConnection callback runs in the execution scope of the server's
  // MakeCallback().
  async_hooks.currentId();

}).listen(port, function onListening() {
  // Returns the id of a TickObject (i.e. process.nextTick()) because all
  // callbacks passed to .listen() are wrapped in a nextTick().
  async_hooks.currentId();
});
```

#### `async_hooks.triggerId()`

* Returns {number} the id of the resource responsible for calling the callback
  that is currently being executed.

For example:

```js
const server = net.createServer(conn => {
  // Though the resource that caused (or triggered) this callback to
  // be called was that of the new connection. Thus the return value
  // of triggerId() is the id of "conn".
  async_hooks.triggerId();

}).listen(port, () => {
  // Even though all callbacks passed to .listen() are wrapped in a nextTick()
  // the callback itself exists because the call to the server's .listen()
  // was made. So the return value would be the id of the server.
  async_hooks.triggerId();
});
```

## Embedder API

Library developers that handle their own I/O will need to hook into the
AsyncWrap API so that all the appropriate callbacks are called. To accommodate
this a JavaScript API is provided.

### `class AsyncResource()`

The class `AsyncResource` was designed to be extended by the embedder's async
resources. Using this users can easily trigger the lifetime events of their
own resources.

The `init()` hook will trigger when an `AsyncResource` is instantiated.

It is important that before/after calls are unwound
in the same order they are called. Otherwise an unrecoverable exception
will be made.

```js
// AsyncResource() is meant to be extended. Instantiating a
// new AsyncResource() also triggers init(). If triggerId is omitted then
// currentId() is used.
const asyncResource = new AsyncResource(type[, triggerId]);

// Call before() hooks.
asyncResource.emitBefore();

// Call after() hooks.
asyncResource.emitAfter();

// Call destroy() hooks.
asyncResource.emitDestroy();

// Return the unique id assigned to the AsyncResource instance.
asyncResource.asyncId();

// Return the trigger id for the AsyncResource instance.
asyncResource.triggerId();
```

#### `AsyncResource(type[, triggerId])`

* arguments
  * `type` {string} the type of ascycn event
  * `triggerId` {number} the id of the execution context that created this async
    event
* Returns {AsyncResource} A reference to `asyncHook`.

Example usage:

```js
class DBQuery extends AsyncResource {
  constructor(db) {
    this.db = db;
  }

  getInfo(query, callback) {
    this.db.get(query, (err, data) => {
      this.emitBefore();
      callback(err, data)
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

* Returns {undefined}

Call all `before()` hooks and let them know a new asynchronous execution
context is being entered. If nested calls to `emitBefore()` are made the stack
of `id`s will be tracked and properly unwound.

#### `asyncResource.emitAfter()`

* Returns {undefined}

Call all `after()` hooks. If nested calls to `emitBefore()` were made then make
sure the stack is unwound properly. Otherwise an error will be thrown.

If the user's callback throws an exception then `emitAfter()` will
automatically be called for all `id`'s on the stack if the error is handled by
a domain or `'uncaughtException'` handler.

#### `asyncResource.emitDestroy()`

* Returns {undefined}

Call all `destroy()` hooks. This should only ever be called once. An error will
be thrown if it is called more than once. This **must** be manually called. If
the resource is left to be collected by the GC then the `destroy()` hooks will
never be called.

#### `asyncResource.asyncId()`

* Returns {number} the unique `id` assigned to the resource.

#### `asyncResource.triggerId()`

* Returns {number} the same `triggerId` that is passed to `init()` hooks.
