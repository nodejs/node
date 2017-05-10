# Async Hooks


> Stability: 1 - Experimental


The `async-hooks` module provides an API to register callbacks tracking the
lifetime of asynchronous resources created inside a Node.js application.

## Terminology

An async resource represents either a "handle" or a "request".

Handles are a reference to a system resource. Some resources are a simple
identifier. For example, file system handles are represented by a file
descriptor. Other handles are represented by libuv as a platform abstracted
struct, e.g. `uv_tcp_t`. Each handle can be continually reused to access and
operate on the referenced resource.

Requests are short lived data structures created to accomplish one task. The
callback for a request should always and only ever fire one time. Which is when
the assigned task has either completed or encountered an error. Requests are
used by handles to perform tasks such as accepting a new connection or
writing data to disk.

## Public API

### Overview

Here is a simple overview of the public API. All of this API is explained in
more detail further down.

```js
const async_hooks = require('async_hooks');

// Return the id of the current execution context.
const cid = async_hooks.currentId();

// Return the id of the handle responsible for triggering the callback of the
// current execution scope to fire.
const tid = aysnc_hooks.triggerId();

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

Registers functions to be called for different lifetime events of each async
operation.
The callbacks `init()`/`before()`/`after()`/`destroy()` are registered via an
`AsyncHooks` instance and fire during the respective asynchronous events in the
lifetime of the event loop. The focal point of these calls centers around the
lifetime of the `AsyncWrap` C++ class. These callbacks will also be called to
emulate the lifetime of handles and requests that do not fit this model. For
example, `HTTPParser` instances are recycled to improve performance. Therefore the
`destroy()` callback is called manually after a connection is done using
it, just before it's placed back into the unused resource pool.

All callbacks are optional. So, for example, if only resource cleanup needs to
be tracked then only the `destroy()` callback needs to be passed. The
specifics of all functions that can be passed to `callbacks` is in the section
`Hook Callbacks`.

**Error Handling**: If any `AsyncHook` callbacks throw, the application will
print the stack trace and exit. The exit path does follow that of any uncaught
exception. However `'exit'` callbacks will still fire unless the application
is run with `--abort-on-uncaught-exception`, in which case a stack trace will
be printed and the application exits, leaving a core file.

The reason for this error handling behavior is that these callbacks are running
at potentially volatile points in an object's lifetime. For example, during
class construction and destruction. Because of this, it is deemed necessary to
bring down the process quickly in order to prevent an unintentional abort in the
future. This is subject to change in the future if a comprehensive analysis is
performed to ensure an exception can follow the normal control flow without
unintentional side effects.


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
* `type` {String} the type of the async resource
* `triggerId` {number} the unique id of the async resource in whose
  execution context this async resource was created
* `resource` {Object} reference to the resource representing the async operation,
  needs to be released during _destroy_

Called when a class is constructed that has the _possibility_ to trigger an
asynchronous event. This _does not_ mean the instance must trigger
`before()`/`after()` before `destroy()` is called. Only that the possibility
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

The `type` is a String that represents the type of resource that caused
`init()` to fire. Generally it will be the name of the resource's constructor.
Some examples include `TCP`, `GetAddrInfo` and `HTTPParser`. Users will be able
to define their own `type` when using the public embedder API.

**Note:** It is possible to have type name collisions. Embedders are recommended
to use unique prefixes per module to prevent collisions when listening to the
hooks.

`triggerId` is the `id` of the resource that caused (or "triggered") the
new resource to initialize and that caused `init()` to fire.

The following is a simple demonstration of this:

```js
const async_hooks = require('async_hooks');

asyns_hooks.createHook({
  init (id, type, triggerId) {
    const cId = async_hooks.currentId();
    process._rawDebug(`${type}(${id}): trigger: ${triggerId} scope: ${cId}`);
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
terms of what caused them to be created. So `triggerId` is given the task of
propagating what resource is responsible for the new resource's existence.

Below is another example with additional information about the calls to
`init()` between the `before()` and `after()` calls. Specifically what the
callback to `listen()` will look like. The output formatting is slightly more
elaborate to make calling context easier to see.

```js
const async_hooks = require('async_hooks');

let ws = 0;
async_hooks.createHook({
  init (id, type, triggerId) {
    const cId = async_hooks.currentId();
    process._rawDebug(' '.repeat(ws) +
                      `${type}(${id}): trigger: ${triggerId} scope: ${cId}`);
  },
  before (id) {
    process._rawDebug(' '.repeat(ws) + 'before: ', id);
    ws += 2;
  },
  after (id) {
    ws -= 2;
    process._rawDebug(' '.repeat(ws) + 'after:  ', id);
  },
  destroy (id) {
    process._rawDebug(' '.repeat(ws) + 'destroy:', id);
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

Now if we only use `scope` to graph resource allocation we get the following:

```
TTYWRAP(6) -> Timeout(4) -> TIMERWRAP(5) -> TickObject(3) -> root(1)
```

The `TCPWRAP` isn't part of this graph; evne though it was the reason for
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

Called either when the class destructor is run or if the resource is manually
marked as free. For core C++ classes that have a destructor the callback will
fire during deconstruction. It is also called synchronously from the embedder
API `emitDestroy()`.

Some resources, such as `HTTPParser`, are not actually destructed but instead
placed in an unused resource pool to be used later. For these `destroy()` will
be called just before the resource is placed on the unused resource pool.

**Note:** Some resources depend on GC for cleanup. So if a reference is made to
the `resource` object passed to `init()` it's possible that `destroy()` is
never called. Causing a memory leak in the application. Of course if you know
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
execution timing.  Not causality (which is covered by `triggerId()`). For
example:

```js
const server = net.createServer(function onconnection(conn) {
  // Returns the id of the server, not of the new connection. Because the
  // on connection callback runs in the execution scope of the server's
  // MakeCallback().
  async_hooks.currentId();

}).listen(port, function onlistening() {
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

### `class AsyncEvent()`

The class `AsyncEvent` was designed to be extended from for embedder's async
resources. Using this users can easily trigger the lifetime events of their
own resources.

The `init()` hook will trigger when an `AsyncEvent` is instantiated.
 
It is important that before/after calls are unwound
in the same order they are called. Otherwise an unrecoverable exception
will be made.

```js
// AsyncEvent() is meant to be extended. Instantiating a new AsyncEvent() also
// triggers init(). If triggerId is omitted then currentId() is used.
const asyncEvent = new AsyncEvent(type[, triggerId]);

// Call before() hooks.
asyncEvent.emitBefore();

// Call after() hooks.
asyncEvent.emitAfter();

// Call destroy() hooks.
asyncEvent.emitDestroy();

// Return the unique id assigned to the AsyncEvent instance.
asyncEvent.asyncId();

// Return the trigger id for the AsyncEvent instance.
asyncEvent.triggerId();
```

#### `AsyncEvent(type[, triggerId])`

* arguments
  * `type` {String} the type of ascycn event
  * `triggerId` {number} the id of the execution context that created this async
    event
* Returns {AsyncEvent} A reference to `asyncHook`.

Example usage:

```js
class DBQuery extends AsyncEvent {
  construtor(db) {
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

#### `asyncEvent.emitBefore()`

* Returns {Undefined}

Call all `before()` hooks and let them know a new asynchronous execution
context is being entered. If nested calls to `emitBefore()` are made the stack
of `id`s will be tracked and properly unwound.

#### `asyncEvent.emitAfter()`

* Returns {Undefined}

Call all `after()` hooks. If nested calls to `emitBefore()` were made then make
sure the stack is unwound properly. Otherwise an error will be thrown.

If the user's callback throws an exception then `emitAfter()` will
automatically be called for all `id`'s on the stack if the error is handled by
a domain or `'uncaughtException'` handler. So there is no need to guard against
this.

#### `asyncEvent.emitDestroy()`

* Returns {Undefined}

Call all `destroy()` hooks. This should only ever be called once. An error will
be thrown if it is called more than once. This **must** be manually called. If
the resource is left to be collected by the GC then the `destroy()` hooks will
never be called.

#### `asyncEvent.asyncId()`

* Returns {number} the unique `id` assigned to the resource. 

Useful when used with `triggerIdScope()`.

#### `asyncEvent.triggerId()`

* Returns {number} the same `triggerId` that is passed to `init()` hooks.

### Standalone JS API

The following API can be used as an alternative to using `AsyncEvent()`, but it
is left to the embedder to manually track values needed for all emitted events
(e.g. `id` and `triggerId`). It mainly exists for use in Node.js core itself and
it is highly recommended that embedders instead use `AsyncEvent`.

```js
const async_hooks = require('async_hooks');

// Return new unique id for a constructing handle.
const id = async_hooks.newUid();

// Propagating the correct trigger id to newly created asynchronous resource is
// important. To make that easier triggerIdScope() will make sure all resources
// created during callback() have that trigger. This also tracks nested calls
// and will unwind properly.
async_hooks.triggerIdScope(triggerId, callback);

// Set the current global id.
async_hooks.setCurrentId(id);

// Call the init() callbacks.
async_hooks.emitInit(number, type[, triggerId][, resource]));

// Call the before() callbacks. The reason for requiring both arguments is
// explained in further detail below.
async_hooks.emitBefore(id);

// Call the after() callbacks.
async_hooks.emitAfter(id);

// Call the destroy() callbacks.
async_hooks.emitDestroy(id);
```

#### `async_hooks.newId()`

* Returns {number} a new unique `id` meant for a newly created asynchronous resource.

The value returned will never be assigned to another resource.

Generally this should be used during object construction. e.g.:

```js
class MyClass {
  constructor() {
    this._id = async_hooks.newId();
    this._triggerId = async_hooks.initTriggerId();
  }
}
```

#### `async_hooks.initTriggerId()`

* Returns {number}

There are several ways to set the `triggerId` for an instantiated resource.
This API is how that value is retrieved. It returns the `id` of the resource
responsible for the newly created resource being instantiated. For example:


#### `async_hooks.emitInit(id, type[, triggerId][, resource])`

* `id` {number} Generated by calling `newId()`
* `type` {String}
* `triggerId` {number} **Default:** `currentId()`
* `resource` {Object}
* Returns {Undefined}

Emit that a resource is being initialized. `id` should be a value returned by
`async_hooks.newId()`. Usage will probably be as follows:

```js
class Foo {
  constructor() {
    this._id = async_hooks.newId();
    this._triggerId = async_hooks.initTriggerId();
    async_hooks.emitInit(this._id, 'Foo', this._triggerId, this);
  }
}
```

In the circumstance that the embedder needs to define a different trigger id
than `currentId()`, they can pass in that id manually.

It is suggested to have `emitInit()` be the last call in the object's
constructor.


#### `async_hooks.emitBefore(id[, triggerId])`

* `id` {number} Generated by `newId()`
* `triggerId` {number}
* Returns {Undefined}

Notify `before()` hooks that the resource is about to enter its execution call
stack. If the `triggerId` of the resource is different from `id` then pass
it in.

Example usage:

```js
MyThing.prototype.done = function done() {
  // First call the before() hooks. So currentId() shows the id of the
  // resource wrapping the id that's been passed.
  async_hooks.emitBefore(this._id);

  // Run the callback.
  this.callback();

  // Call after() callbacks now that the old id has been restored.
  async_hooks.emitAfter(this._id);
};
```

#### `async_hooks.emitAfter(id)`

* `id` {number} Generated by `newId()`
* Returns {Undefined}

Notify `after()` hooks that the resource is exiting its execution call stack.

Even though the state of `id` is tracked internally, passing it in is required
as a way to validate that the stack is unwinding properly.

For example, no two async stack should cross when `emitAfter()` is called.

```
init # Foo
init # Bar
...
before # Foo
before # Bar
after # Foo   <- Should be called after Bar
after # Bar
```

**Note:** It is not necessary to wrap the callback in a try/finally and force
emitAfter() if the callback throws. That is automatically handled by the
fatal exception handler.

#### `async_hooks.emitDestroy(id)`

* `id` {number} Generated by `newId()`
* Returns {Undefined}

Notify hooks that a resource is being destroyed (or being moved to the free'd
resource pool).

#### `async_hooks.triggerIdScope(triggerId, callback)`

* `triggerId` {number}
* `callback` {Function}
* Returns {Undefined}

All resources created during the execution of `callback` will be given
`triggerId`. Unless it was otherwise 1) passed in as an argument to
`AsyncEvent` 2) set via `setInitTriggerId()` or 3) a nested call to
`triggerIdScope()` is made.

Meant to be used in conjunction with the `AsyncEvent` API, and preferred over
`setInitTriggerId()` because it is more error proof.

Example using this to make sure the correct `triggerId` propagates to newly
created asynchronous resources:

```js
class MyThing extends AsyncEvent {
  constructor(foo, cb) {
    this.foo = foo;
    async_hooks.triggerIdScope(this.asyncId(), () => {
      process.nextTick(cb);
    });
  }
}
```
