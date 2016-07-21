# Events

> Stability: 2 - Stable

<!--type=module-->

Much of the Node.js core API is built around an idiomatic asynchronous
event-driven architecture in which certain kinds of objects (called "emitters")
periodically emit named events that cause Function objects ("listeners") to be
called.

For instance: a [`net.Server`][] object emits an event each time a peer
connects to it; a [`fs.ReadStream`][] emits an event when the file is opened;
a [stream][] emits an event whenever data is available to be read.

All objects that emit events are instances of the `EventEmitter` class. These
objects expose an `eventEmitter.on()` function that allows one or more
functions to be attached to named events emitted by the object. Typically,
event names are camel-cased strings but any valid JavaScript property key
can be used.

When the `EventEmitter` object emits an event, all of the functions attached
to that specific event are called _synchronously_. Any values returned by the
called listeners are _ignored_ and will be discarded.

The following example shows a simple `EventEmitter` instance with a single
listener. The `eventEmitter.on()` method is used to register listeners, while
the `eventEmitter.emit()` method is used to trigger the event.

```js
const EventEmitter = require('events');

class MyEmitter extends EventEmitter {}

const myEmitter = new MyEmitter();
myEmitter.on('event', () => {
  console.log('an event occurred!');
});
myEmitter.emit('event');
```

## Passing arguments and `this` to listeners

The `eventEmitter.emit()` method allows an arbitrary set of arguments to be
passed to the listener functions. It is important to keep in mind that when an
ordinary listener function is called by the `EventEmitter`, the standard `this`
keyword is intentionally set to reference the `EventEmitter` to which the
listener is attached.

```js
const myEmitter = new MyEmitter();
myEmitter.on('event', function(a, b) {
  console.log(a, b, this);
    // Prints:
    //   a b MyEmitter {
    //     domain: null,
    //     _events: { event: [Function] },
    //     _eventsCount: 1,
    //     _maxListeners: undefined }
});
myEmitter.emit('event', 'a', 'b');
```

It is possible to use ES6 Arrow Functions as listeners, however, when doing so,
the `this` keyword will no longer reference the `EventEmitter` instance:

```js
const myEmitter = new MyEmitter();
myEmitter.on('event', (a, b) => {
  console.log(a, b, this);
    // Prints: a b {}
});
myEmitter.emit('event', 'a', 'b');
```

## Asynchronous vs. Synchronous

The `EventListener` calls all listeners synchronously in the order in which
they were registered. This is important to ensure the proper sequencing of
events and to avoid race conditions or logic errors. When appropriate,
listener functions can switch to an asynchronous mode of operation using
the `setImmediate()` or `process.nextTick()` methods:

```js
const myEmitter = new MyEmitter();
myEmitter.on('event', (a, b) => {
  setImmediate(() => {
    console.log('this happens asynchronously');
  });
});
myEmitter.emit('event', 'a', 'b');
```

## Handling events only once

When a listener is registered using the `eventEmitter.on()` method, that
listener will be invoked _every time_ the named event is emitted.

```js
const myEmitter = new MyEmitter();
var m = 0;
myEmitter.on('event', () => {
  console.log(++m);
});
myEmitter.emit('event');
  // Prints: 1
myEmitter.emit('event');
  // Prints: 2
```

Using the `eventEmitter.once()` method, it is possible to register a listener
that is called at most once for a particular event. Once the event is emitted,
the listener is unregistered and *then* called.

```js
const myEmitter = new MyEmitter();
var m = 0;
myEmitter.once('event', () => {
  console.log(++m);
});
myEmitter.emit('event');
  // Prints: 1
myEmitter.emit('event');
  // Ignored
```

## Error events

When an error occurs within an `EventEmitter` instance, the typical action is
for an `'error'` event to be emitted. These are treated as special cases
within Node.js.

If an `EventEmitter` does _not_ have at least one listener registered for the
`'error'` event, and an `'error'` event is emitted, the error is thrown, a
stack trace is printed, and the Node.js process exits.

```js
const myEmitter = new MyEmitter();
myEmitter.emit('error', new Error('whoops!'));
  // Throws and crashes Node.js
```

To guard against crashing the Node.js process, a listener can be registered
on the [`process` object's `uncaughtException` event][] or the [`domain`][] module
can be used. (_Note, however, that the `domain` module has been deprecated_)

```js
const myEmitter = new MyEmitter();

process.on('uncaughtException', (err) => {
  console.log('whoops! there was an error');
});

myEmitter.emit('error', new Error('whoops!'));
  // Prints: whoops! there was an error
```

As a best practice, listeners should always be added for the `'error'` events.

```js
const myEmitter = new MyEmitter();
myEmitter.on('error', (err) => {
  console.log('whoops! there was an error');
});
myEmitter.emit('error', new Error('whoops!'));
  // Prints: whoops! there was an error
```

## Class: EventEmitter
<!-- YAML
added: v0.1.26
-->

The `EventEmitter` class is defined and exposed by the `events` module:

```js
const EventEmitter = require('events');
```

All EventEmitters emit the event `'newListener'` when new listeners are
added and `'removeListener'` when existing listeners are removed.

### Event: 'newListener'
<!-- YAML
added: v0.1.26
-->

* `eventName` {String|Symbol} The name of the event being listened for
* `listener` {Function} The event handler function

The `EventEmitter` instance will emit its own `'newListener'` event *before*
a listener is added to its internal array of listeners.

Listeners registered for the `'newListener'` event will be passed the event
name and a reference to the listener being added.

The fact that the event is triggered before adding the listener has a subtle
but important side effect: any *additional* listeners registered to the same
`name` *within* the `'newListener'` callback will be inserted *before* the
listener that is in the process of being added.

```js
const myEmitter = new MyEmitter();
// Only do this once so we don't loop forever
myEmitter.once('newListener', (event, listener) => {
  if (event === 'event') {
    // Insert a new listener in front
    myEmitter.on('event', () => {
      console.log('B');
    });
  }
});
myEmitter.on('event', () => {
  console.log('A');
});
myEmitter.emit('event');
  // Prints:
  //   B
  //   A
```

### Event: 'removeListener'
<!-- YAML
added: v0.9.3
-->

* `eventName` {String|Symbol} The event name
* `listener` {Function} The event handler function

The `'removeListener'` event is emitted *after* the `listener` is removed.

### EventEmitter.listenerCount(emitter, eventName)
<!-- YAML
added: v0.9.12
deprecated: v4.0.0
-->

> Stability: 0 - Deprecated: Use [`emitter.listenerCount()`][] instead.

A class method that returns the number of listeners for the given `eventName`
registered on the given `emitter`.

```js
const myEmitter = new MyEmitter();
myEmitter.on('event', () => {});
myEmitter.on('event', () => {});
console.log(EventEmitter.listenerCount(myEmitter, 'event'));
  // Prints: 2
```

### EventEmitter.defaultMaxListeners
<!-- YAML
added: v0.11.2
-->

By default, a maximum of `10` listeners can be registered for any single
event. This limit can be changed for individual `EventEmitter` instances
using the [`emitter.setMaxListeners(n)`][] method. To change the default
for *all* `EventEmitter` instances, the `EventEmitter.defaultMaxListeners`
property can be used.

Take caution when setting the `EventEmitter.defaultMaxListeners` because the
change effects *all* `EventEmitter` instances, including those created before
the change is made. However, calling [`emitter.setMaxListeners(n)`][] still has
precedence over `EventEmitter.defaultMaxListeners`.

Note that this is not a hard limit. The `EventEmitter` instance will allow
more listeners to be added but will output a trace warning to stderr indicating
that a "possible EventEmitter memory leak" has been detected. For any single
`EventEmitter`, the `emitter.getMaxListeners()` and `emitter.setMaxListeners()`
methods can be used to temporarily avoid this warning:

```js
emitter.setMaxListeners(emitter.getMaxListeners() + 1);
emitter.once('event', () => {
  // do stuff
  emitter.setMaxListeners(Math.max(emitter.getMaxListeners() - 1, 0));
});
```

### emitter.addListener(eventName, listener)
<!-- YAML
added: v0.1.26
-->

Alias for `emitter.on(eventName, listener)`.

### emitter.emit(eventName[, arg1][, arg2][, ...])
<!-- YAML
added: v0.1.26
-->

Synchronously calls each of the listeners registered for the event named
`eventName`, in the order they were registered, passing the supplied arguments
to each.

Returns `true` if the event had listeners, `false` otherwise.

### emitter.eventNames()
<!-- YAML
added: v6.0.0
-->

Returns an array listing the events for which the emitter has registered
listeners. The values in the array will be strings or Symbols.

```js
const EventEmitter = require('events');
const myEE = new EventEmitter();
myEE.on('foo', () => {});
myEE.on('bar', () => {});

const sym = Symbol('symbol');
myEE.on(sym, () => {});

console.log(myEE.eventNames());
  // Prints [ 'foo', 'bar', Symbol(symbol) ]
```

### emitter.getMaxListeners()
<!-- YAML
added: v1.0.0
-->

Returns the current max listener value for the `EventEmitter` which is either
set by [`emitter.setMaxListeners(n)`][] or defaults to
[`EventEmitter.defaultMaxListeners`][].

### emitter.listenerCount(eventName)
<!-- YAML
added: v3.2.0
-->

* `eventName` {String|Symbol} The name of the event being listened for

Returns the number of listeners listening to the event named `eventName`.

### emitter.listeners(eventName)
<!-- YAML
added: v0.1.26
-->

Returns a copy of the array of listeners for the event named `eventName`.

```js
server.on('connection', (stream) => {
  console.log('someone connected!');
});
console.log(util.inspect(server.listeners('connection')));
  // Prints: [ [Function] ]
```

### emitter.on(eventName, listener)
<!-- YAML
added: v0.1.101
-->

* `eventName` {String|Symbol} The name of the event.
* `listener` {Function} The callback function

Adds the `listener` function to the end of the listeners array for the
event named `eventName`. No checks are made to see if the `listener` has
already been added. Multiple calls passing the same combination of `eventName`
and `listener` will result in the `listener` being added, and called, multiple
times.

```js
server.on('connection', (stream) => {
  console.log('someone connected!');
});
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

By default, event listeners are invoked in the order they are added. The
`emitter.prependListener()` method can be used as an alternative to add the
event listener to the beginning of the listeners array.

```js
const myEE = new EventEmitter();
myEE.on('foo', () => console.log('a'));
myEE.prependListener('foo', () => console.log('b'));
myEE.emit('foo');
  // Prints:
  //   b
  //   a
```

### emitter.once(eventName, listener)
<!-- YAML
added: v0.3.0
-->

* `eventName` {String|Symbol} The name of the event.
* `listener` {Function} The callback function

Adds a **one time** `listener` function for the event named `eventName`. The
next time `eventName` is triggered, this listener is removed and then invoked.

```js
server.once('connection', (stream) => {
  console.log('Ah, we have our first user!');
});
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

By default, event listeners are invoked in the order they are added. The
`emitter.prependOnceListener()` method can be used as an alternative to add the
event listener to the beginning of the listeners array.

```js
const myEE = new EventEmitter();
myEE.once('foo', () => console.log('a'));
myEE.prependOnceListener('foo', () => console.log('b'));
myEE.emit('foo');
  // Prints:
  //   b
  //   a
```

### emitter.prependListener(eventName, listener)
<!-- YAML
added: v6.0.0
-->

* `eventName` {String|Symbol} The name of the event.
* `listener` {Function} The callback function

Adds the `listener` function to the *beginning* of the listeners array for the
event named `eventName`. No checks are made to see if the `listener` has
already been added. Multiple calls passing the same combination of `eventName`
and `listener` will result in the `listener` being added, and called, multiple
times.

```js
server.prependListener('connection', (stream) => {
  console.log('someone connected!');
});
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.prependOnceListener(eventName, listener)
<!-- YAML
added: v6.0.0
-->

* `eventName` {String|Symbol} The name of the event.
* `listener` {Function} The callback function

Adds a **one time** `listener` function for the event named `eventName` to the
*beginning* of the listeners array. The next time `eventName` is triggered, this
listener is removed, and then invoked.

```js
server.prependOnceListener('connection', (stream) => {
  console.log('Ah, we have our first user!');
});
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.removeAllListeners([eventName])
<!-- YAML
added: v0.1.26
-->

Removes all listeners, or those of the specified `eventName`.

Note that it is bad practice to remove listeners added elsewhere in the code,
particularly when the `EventEmitter` instance was created by some other
component or module (e.g. sockets or file streams).

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.removeListener(eventName, listener)
<!-- YAML
added: v0.1.26
-->

Removes the specified `listener` from the listener array for the event named
`eventName`.

```js
var callback = (stream) => {
  console.log('someone connected!');
};
server.on('connection', callback);
// ...
server.removeListener('connection', callback);
```

`removeListener` will remove, at most, one instance of a listener from the
listener array. If any single listener has been added multiple times to the
listener array for the specified `eventName`, then `removeListener` must be
called multiple times to remove each instance.

Note that once an event has been emitted, all listeners attached to it at the
time of emitting will be called in order. This implies that any `removeListener()`
or `removeAllListeners()` calls *after* emitting and *before* the last listener
finishes execution will not remove them from `emit()` in progress. Subsequent
events will behave as expected.

```js
const myEmitter = new MyEmitter();

var callbackA = () => {
  console.log('A');
  myEmitter.removeListener('event', callbackB);
};

var callbackB = () => {
  console.log('B');
};

myEmitter.on('event', callbackA);

myEmitter.on('event', callbackB);

// callbackA removes listener callbackB but it will still be called.
// Internal listener array at time of emit [callbackA, callbackB]
myEmitter.emit('event');
  // Prints:
  //   A
  //   B

// callbackB is now removed.
// Internal listener array [callbackA]
myEmitter.emit('event');
  // Prints:
  //   A

```

Because listeners are managed using an internal array, calling this will
change the position indices of any listener registered *after* the listener
being removed. This will not impact the order in which listeners are called,
but it means that any copies of the listener array as returned by
the `emitter.listeners()` method will need to be recreated.

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.setMaxListeners(n)
<!-- YAML
added: v0.3.5
-->

By default EventEmitters will print a warning if more than `10` listeners are
added for a particular event. This is a useful default that helps finding
memory leaks. Obviously, not all events should be limited to just 10 listeners.
The `emitter.setMaxListeners()` method allows the limit to be modified for this
specific `EventEmitter` instance. The value can be set to `Infinity` (or `0`)
to indicate an unlimited number of listeners.

Returns a reference to the `EventEmitter`, so that calls can be chained.

[`net.Server`]: net.html#net_class_net_server
[`fs.ReadStream`]: fs.html#fs_class_fs_readstream
[`emitter.setMaxListeners(n)`]: #events_emitter_setmaxlisteners_n
[`EventEmitter.defaultMaxListeners`]: #events_eventemitter_defaultmaxlisteners
[`emitter.listenerCount()`]: #events_emitter_listenercount_eventname
[`domain`]: domain.html
[`process` object's `uncaughtException` event]: process.html#process_event_uncaughtexception
[stream]: stream.html
