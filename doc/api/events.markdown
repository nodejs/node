# Events

    Stability: 4 - API Frozen

<!--type=module-->

Many objects in Node emit events: a `net.Server` emits an event each time
a peer connects to it, a `fs.readStream` emits an event when the file is
opened. All objects which emit events are instances of `events.EventEmitter`.
You can access this module by doing: `require("events");`

Typically, event names are represented by a camel-cased string, however,
there aren't any strict restrictions on that, as any string will be accepted.

Functions can then be attached to objects, to be executed when an event
is emitted. These functions are called _listeners_. Inside a listener
function, `this` refers to the `EventEmitter` that the listener was
attached to.


## Class: events.EventEmitter

To access the EventEmitter class, `require('events').EventEmitter`.

When an `EventEmitter` instance experiences an error, the typical action is
to emit an `'error'` event.  Error events are treated as a special case in node.
If there is no listener for it, then the default action is to print a stack
trace and exit the program.

All EventEmitters emit the event `'newListener'` when new listeners are
added and `'removeListener'` when a listener is removed.

### emitter.addListener(event, listener)
### emitter.on(event, listener)

Adds a listener to the end of the listeners array for the specified `event`.
No checks are made to see if the `listener` has already been added. Multiple
calls passing the same combination of `event` and `listener` will result in the
`listener` being added multiple times.

    server.on('connection', function (stream) {
      console.log('someone connected!');
    });

Returns emitter, so calls can be chained.

### emitter.once(event, listener)

Adds a **one time** listener for the event. This listener is
invoked only the next time the event is fired, after which
it is removed.

    server.once('connection', function (stream) {
      console.log('Ah, we have our first user!');
    });

Returns emitter, so calls can be chained.

### emitter.removeListener(event, listener)

Remove a listener from the listener array for the specified event.
**Caution**: changes array indices in the listener array behind the listener.

    var callback = function(stream) {
      console.log('someone connected!');
    };
    server.on('connection', callback);
    // ...
    server.removeListener('connection', callback);

`removeListener` will remove, at most, one instance of a listener from the
listener array. If any single listener has been added multiple times to the
listener array for the specified `event`, then `removeListener` must be called
multiple times to remove each instance.

Returns emitter, so calls can be chained.

### emitter.removeAllListeners([event])

Removes all listeners, or those of the specified event. It's not a good idea to
remove listeners that were added elsewhere in the code, especially when it's on
an emitter that you didn't create (e.g. sockets or file streams).

Returns emitter, so calls can be chained.

### emitter.setMaxListeners(n)

By default EventEmitters will print a warning if more than 10 listeners are
added for a particular event. This is a useful default which helps finding
memory leaks. Obviously not all Emitters should be limited to 10. This function
allows that to be increased. Set to zero for unlimited.

Returns emitter, so calls can be chained.

### EventEmitter.defaultMaxListeners

`emitter.setMaxListeners(n)` sets the maximum on a per-instance basis.
This class property lets you set it for *all* `EventEmitter` instances,
current and future, effective immediately. Use with care.

Note that `emitter.setMaxListeners(n)` still has precedence over
`EventEmitter.defaultMaxListeners`.


### emitter.listeners(event)

Returns an array of listeners for the specified event.

    server.on('connection', function (stream) {
      console.log('someone connected!');
    });
    console.log(util.inspect(server.listeners('connection'))); // [ [Function] ]


### emitter.emit(event[, arg1][, arg2][, ...])

Execute each of the listeners in order with the supplied arguments.

Returns `true` if event had listeners, `false` otherwise.


### Class Method: EventEmitter.listenerCount(emitter, event)

Return the number of listeners for a given event.


### Event: 'newListener'

* `event` {String} The event name
* `listener` {Function} The event handler function

This event is emitted any time a listener is added. When this event is triggered,
the listener may not yet have been added to the array of listeners for the `event`.


### Event: 'removeListener'

* `event` {String} The event name
* `listener` {Function} The event handler function

This event is emitted any time someone removes a listener.  When this event is triggered,
the listener may not yet have been removed from the array of listeners for the `event`.
