# Events

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

<!--type=module-->

Much of the Node.js core API is built around an idiomatic asynchronous
event-driven architecture in which certain kinds of objects (called "emitters")
emit named events that cause `Function` objects ("listeners") to be called.

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
passed to the listener functions. Keep in mind that when
an ordinary listener function is called, the standard `this` keyword
is intentionally set to reference the `EventEmitter` instance to which the
listener is attached.

```js
const myEmitter = new MyEmitter();
myEmitter.on('event', function(a, b) {
  console.log(a, b, this, this === myEmitter);
  // Prints:
  //   a b MyEmitter {
  //     domain: null,
  //     _events: { event: [Function] },
  //     _eventsCount: 1,
  //     _maxListeners: undefined } true
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

The `EventEmitter` calls all listeners synchronously in the order in which
they were registered. This ensures the proper sequencing of
events and helps avoid race conditions and logic errors. When appropriate,
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
let m = 0;
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
let m = 0;
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

To guard against crashing the Node.js process the [`domain`][] module can be
used. (Note, however, that the `domain` module is deprecated.)

As a best practice, listeners should always be added for the `'error'` events.

```js
const myEmitter = new MyEmitter();
myEmitter.on('error', (err) => {
  console.error('whoops! there was an error');
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

All `EventEmitter`s emit the event `'newListener'` when new listeners are
added and `'removeListener'` when existing listeners are removed.

### Event: 'newListener'
<!-- YAML
added: v0.1.26
-->

* `eventName` {string|symbol} The name of the event being listened for
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
changes:
  - version: v6.1.0, v4.7.0
    pr-url: https://github.com/nodejs/node/pull/6394
    description: For listeners attached using `.once()`, the `listener` argument
                 now yields the original listener function.
-->

* `eventName` {string|symbol} The event name
* `listener` {Function} The event handler function

The `'removeListener'` event is emitted *after* the `listener` is removed.

### EventEmitter.listenerCount(emitter, eventName)
<!-- YAML
added: v0.9.12
deprecated: v4.0.0
-->

> Stability: 0 - Deprecated: Use [`emitter.listenerCount()`][] instead.

* `emitter` {EventEmitter} The emitter to query
* `eventName` {string|symbol} The event name

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
property can be used. If this value is not a positive number, a `TypeError`
will be thrown.

Take caution when setting the `EventEmitter.defaultMaxListeners` because the
change affects *all* `EventEmitter` instances, including those created before
the change is made. However, calling [`emitter.setMaxListeners(n)`][] still has
precedence over `EventEmitter.defaultMaxListeners`.

This is not a hard limit. The `EventEmitter` instance will allow
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

The [`--trace-warnings`][] command line flag can be used to display the
stack trace for such warnings.

The emitted warning can be inspected with [`process.on('warning')`][] and will
have the additional `emitter`, `type` and `count` properties, referring to
the event emitter instance, the event’s name and the number of attached
listeners, respectively.
Its `name` property is set to `'MaxListenersExceededWarning'`.

### emitter.addListener(eventName, listener)
<!-- YAML
added: v0.1.26
-->

* `eventName` {string|symbol}
* `listener` {Function}

Alias for `emitter.on(eventName, listener)`.

### emitter.emit(eventName\[, ...args\])
<!-- YAML
added: v0.1.26
-->

* `eventName` {string|symbol}
* `...args` {any}
* Returns: {boolean}

Synchronously calls each of the listeners registered for the event named
`eventName`, in the order they were registered, passing the supplied arguments
to each.

Returns `true` if the event had listeners, `false` otherwise.

```js
const EventEmitter = require('events');
const myEmitter = new EventEmitter();

// First listener
myEmitter.on('event', function firstListener() {
  console.log('Helloooo! first listener');
});
// Second listener
myEmitter.on('event', function secondListener(arg1, arg2) {
  console.log(`event with parameters ${arg1}, ${arg2} in second listener`);
});
// Third listener
myEmitter.on('event', function thirdListener(...args) {
  const parameters = args.join(', ');
  console.log(`event with parameters ${parameters} in third listener`);
});

console.log(myEmitter.listeners('event'));

myEmitter.emit('event', 1, 2, 3, 4, 5);

// Prints:
// [
//   [Function: firstListener],
//   [Function: secondListener],
//   [Function: thirdListener]
// ]
// Helloooo! first listener
// event with parameters 1, 2 in second listener
// event with parameters 1, 2, 3, 4, 5 in third listener
```

### emitter.eventNames()
<!-- YAML
added: v6.0.0
-->

* Returns: {Array}

Returns an array listing the events for which the emitter has registered
listeners. The values in the array will be strings or `Symbol`s.

```js
const EventEmitter = require('events');
const myEE = new EventEmitter();
myEE.on('foo', () => {});
myEE.on('bar', () => {});

const sym = Symbol('symbol');
myEE.on(sym, () => {});

console.log(myEE.eventNames());
// Prints: [ 'foo', 'bar', Symbol(symbol) ]
```

### emitter.getMaxListeners()
<!-- YAML
added: v1.0.0
-->

* Returns: {integer}

Returns the current max listener value for the `EventEmitter` which is either
set by [`emitter.setMaxListeners(n)`][] or defaults to
[`EventEmitter.defaultMaxListeners`][].

### emitter.listenerCount(eventName)
<!-- YAML
added: v3.2.0
-->

* `eventName` {string|symbol} The name of the event being listened for
* Returns: {integer}

Returns the number of listeners listening to the event named `eventName`.

### emitter.listeners(eventName)
<!-- YAML
added: v0.1.26
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/6881
    description: For listeners attached using `.once()` this returns the
                 original listeners instead of wrapper functions now.
-->

* `eventName` {string|symbol}
* Returns: {Function[]}

Returns a copy of the array of listeners for the event named `eventName`.

```js
server.on('connection', (stream) => {
  console.log('someone connected!');
});
console.log(util.inspect(server.listeners('connection')));
// Prints: [ [Function] ]
```

### emitter.off(eventName, listener)
<!-- YAML
added: v10.0.0
-->

* `eventName` {string|symbol}
* `listener` {Function}
* Returns: {EventEmitter}

Alias for [`emitter.removeListener()`][].

### emitter.on(eventName, listener)
<!-- YAML
added: v0.1.101
-->

* `eventName` {string|symbol} The name of the event.
* `listener` {Function} The callback function
* Returns: {EventEmitter}

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

* `eventName` {string|symbol} The name of the event.
* `listener` {Function} The callback function
* Returns: {EventEmitter}

Adds a **one-time** `listener` function for the event named `eventName`. The
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

* `eventName` {string|symbol} The name of the event.
* `listener` {Function} The callback function
* Returns: {EventEmitter}

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

* `eventName` {string|symbol} The name of the event.
* `listener` {Function} The callback function
* Returns: {EventEmitter}

Adds a **one-time** `listener` function for the event named `eventName` to the
*beginning* of the listeners array. The next time `eventName` is triggered, this
listener is removed, and then invoked.

```js
server.prependOnceListener('connection', (stream) => {
  console.log('Ah, we have our first user!');
});
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.removeAllListeners(\[eventName\])
<!-- YAML
added: v0.1.26
-->

* `eventName` {string|symbol}
* Returns: {EventEmitter}

Removes all listeners, or those of the specified `eventName`.

It is bad practice to remove listeners added elsewhere in the code,
particularly when the `EventEmitter` instance was created by some other
component or module (e.g. sockets or file streams).

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.removeListener(eventName, listener)
<!-- YAML
added: v0.1.26
-->

* `eventName` {string|symbol}
* `listener` {Function}
* Returns: {EventEmitter}

Removes the specified `listener` from the listener array for the event named
`eventName`.

```js
const callback = (stream) => {
  console.log('someone connected!');
};
server.on('connection', callback);
// ...
server.removeListener('connection', callback);
```

`removeListener()` will remove, at most, one instance of a listener from the
listener array. If any single listener has been added multiple times to the
listener array for the specified `eventName`, then `removeListener()` must be
called multiple times to remove each instance.

Once an event has been emitted, all listeners attached to it at the
time of emitting will be called in order. This implies that any
`removeListener()` or `removeAllListeners()` calls *after* emitting and
*before* the last listener finishes execution will not remove them from
`emit()` in progress. Subsequent events will behave as expected.

```js
const myEmitter = new MyEmitter();

const callbackA = () => {
  console.log('A');
  myEmitter.removeListener('event', callbackB);
};

const callbackB = () => {
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

When a single function has been added as a handler multiple times for a single
event (as in the example below), `removeListener()` will remove the most
recently added instance. In the example the `once('ping')`
listener is removed:

```js
const ee = new EventEmitter();

function pong() {
  console.log('pong');
}

ee.on('ping', pong);
ee.once('ping', pong);
ee.removeListener('ping', pong);

ee.emit('ping');
ee.emit('ping');
```

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.setMaxListeners(n)
<!-- YAML
added: v0.3.5
-->

* `n` {integer}
* Returns: {EventEmitter}

By default `EventEmitter`s will print a warning if more than `10` listeners are
added for a particular event. This is a useful default that helps finding
memory leaks. Obviously, not all events should be limited to just 10 listeners.
The `emitter.setMaxListeners()` method allows the limit to be modified for this
specific `EventEmitter` instance. The value can be set to `Infinity` (or `0`)
to indicate an unlimited number of listeners.

Returns a reference to the `EventEmitter`, so that calls can be chained.

### emitter.rawListeners(eventName)
<!-- YAML
added: v9.4.0
-->

* `eventName` {string|symbol}
* Returns: {Function[]}

Returns a copy of the array of listeners for the event named `eventName`,
including any wrappers (such as those created by `.once()`).

```js
const emitter = new EventEmitter();
emitter.once('log', () => console.log('log once'));

// Returns a new Array with a function `onceWrapper` which has a property
// `listener` which contains the original listener bound above
const listeners = emitter.rawListeners('log');
const logFnWrapper = listeners[0];

// Logs "log once" to the console and does not unbind the `once` event
logFnWrapper.listener();

// Logs "log once" to the console and removes the listener
logFnWrapper();

emitter.on('log', () => console.log('log persistently'));
// Will return a new Array with a single function bound by `.on()` above
const newListeners = emitter.rawListeners('log');

// Logs "log persistently" twice
newListeners[0]();
emitter.emit('log');
```

## events.once(emitter, name)
<!-- YAML
added: v11.13.0
-->

* `emitter` {EventEmitter}
* `name` {string}
* Returns: {Promise}

Creates a `Promise` that is fulfilled when the `EventEmitter` emits the given
event or that is rejected when the `EventEmitter` emits `'error'`.
The `Promise` will resolve with an array of all the arguments emitted to the
given event.

This method is intentionally generic and works with the web platform
[EventTarget][WHATWG-EventTarget] interface, which has no special
`'error'` event semantics and does not listen to the `'error'` event.

```js
const { once, EventEmitter } = require('events');

async function run() {
  const ee = new EventEmitter();

  process.nextTick(() => {
    ee.emit('myevent', 42);
  });

  const [value] = await once(ee, 'myevent');
  console.log(value);

  const err = new Error('kaboom');
  process.nextTick(() => {
    ee.emit('error', err);
  });

  try {
    await once(ee, 'myevent');
  } catch (err) {
    console.log('error happened', err);
  }
}

run();
```

[WHATWG-EventTarget]: https://dom.spec.whatwg.org/#interface-eventtarget
[`--trace-warnings`]: cli.html#cli_trace_warnings
[`EventEmitter.defaultMaxListeners`]: #events_eventemitter_defaultmaxlisteners
[`domain`]: domain.html
[`emitter.listenerCount()`]: #events_emitter_listenercount_eventname
[`emitter.removeListener()`]: #events_emitter_removelistener_eventname_listener
[`emitter.setMaxListeners(n)`]: #events_emitter_setmaxlisteners_n
[`fs.ReadStream`]: fs.html#fs_class_fs_readstream
[`net.Server`]: net.html#net_class_net_server
[`process.on('warning')`]: process.html#process_event_warning
[stream]: stream.html
