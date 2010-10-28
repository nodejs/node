## EventEmitter

Many objects in Node emit events: a TCP server emits an event each time
there is a stream, a child process emits an event when it exits. All
objects which emit events are instances of `events.EventEmitter`.

Events are represented by a camel-cased string. Here are some examples:
`'connection'`, `'data'`, `'messageBegin'`.

Functions can be then be attached to objects, to be executed when an event
is emitted. These functions are called _listeners_.

`require('events').EventEmitter` to access the `EventEmitter` class.

All EventEmitters emit the event `'newListener'` when new listeners are
added.

When an `EventEmitter` experiences an error, the typical action is to emit an
`'error'` event.  Error events are special--if there is no handler for them
they will print a stack trace and exit the program.

### Event: 'newListener'

`function (event, listener) { }`

This event is emitted any time someone adds a new listener.

### Event: 'error'

`function (exception) { }`

If an error was encountered, then this event is emitted. This event is
special - when there are no listeners to receive the error Node will
terminate execution and display the exception's stack trace.

### emitter.on(event, listener)

Adds a listener to the end of the listeners array for the specified event.

    server.on('connection', function (stream) {
      console.log('someone connected!');
    });

### emitter.once(event, listener)

Adds a **one time** listener for the event. The listener is
invoked only the first time the event is fired, after which
it is removed.

    server.once('connection', function (stream) {
      console.log('Ah, we have our first user!');
    });

### emitter.removeListener(event, listener)

Remove a listener from the listener array for the specified event.
**Caution**: changes array indices in the listener array behind the listener.

    var callback = function(stream) {
	  console.log('someone connected!');
	};
    server.on('connection', callback);
	// ...
	server.removeListener('connection', callback);


### emitter.removeAllListeners(event)

Removes all listeners from the listener array for the specified event.


### emitter.listeners(event)

Returns an array of listeners for the specified event. This array can be
manipulated, e.g. to remove listeners.

    server.on('connection', function (stream) {
      console.log('someone connected!');
    });
	console.log(util.inspect(server.listeners('connection'));
	// [ [Function] ]


### emitter.emit(event, [arg1], [arg2], [...])

Execute each of the listeners in order with the supplied arguments.
