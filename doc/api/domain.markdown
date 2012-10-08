# Domain

    Stability: 1 - Experimental

Domains provide a way to handle multiple different IO operations as a
single group.  If any of the event emitters or callbacks registered to a
domain emit an `error` event, or throw an error, then the domain object
will be notified, rather than losing the context of the error in the
`process.on('uncaughtException')` handler, or causing the program to
exit with an error code.

This feature is new in Node version 0.8.  It is a first pass, and is
expected to change significantly in future versions.  Please use it and
provide feedback.

Due to their experimental nature, the Domains features are disabled unless
the `domain` module is loaded at least once.  No domains are created or
registered by default.  This is by design, to prevent adverse effects on
current programs.  It is expected to be enabled by default in future
Node.js versions.

## Additions to Error objects

<!-- type=misc -->

Any time an Error object is routed through a domain, a few extra fields
are added to it.

* `error.domain` The domain that first handled the error.
* `error.domain_emitter` The event emitter that emitted an 'error' event
  with the error object.
* `error.domain_bound` The callback function which was bound to the
  domain, and passed an error as its first argument.
* `error.domain_thrown` A boolean indicating whether the error was
  thrown, emitted, or passed to a bound callback function.

## Implicit Binding

<!--type=misc-->

If domains are in use, then all new EventEmitter objects (including
Stream objects, requests, responses, etc.) will be implicitly bound to
the active domain at the time of their creation.

Additionally, callbacks passed to lowlevel event loop requests (such as
to fs.open, or other callback-taking methods) will automatically be
bound to the active domain.  If they throw, then the domain will catch
the error.

In order to prevent excessive memory usage, Domain objects themselves
are not implicitly added as children of the active domain.  If they
were, then it would be too easy to prevent request and response objects
from being properly garbage collected.

If you *want* to nest Domain objects as children of a parent Domain,
then you must explicitly add them, and then dispose of them later.

Implicit binding routes thrown errors and `'error'` events to the
Domain's `error` event, but does not register the EventEmitter on the
Domain, so `domain.dispose()` will not shut down the EventEmitter.
Implicit binding only takes care of thrown errors and `'error'` events.

## Explicit Binding

<!--type=misc-->

Sometimes, the domain in use is not the one that ought to be used for a
specific event emitter.  Or, the event emitter could have been created
in the context of one domain, but ought to instead be bound to some
other domain.

For example, there could be one domain in use for an HTTP server, but
perhaps we would like to have a separate domain to use for each request.

That is possible via explicit binding.

For example:

```
// create a top-level domain for the server
var serverDomain = domain.create();

serverDomain.run(function() {
  // server is created in the scope of serverDomain
  http.createServer(function(req, res) {
    // req and res are also created in the scope of serverDomain
    // however, we'd prefer to have a separate domain for each request.
    // create it first thing, and add req and res to it.
    var reqd = domain.create();
    reqd.add(req);
    reqd.add(res);
    reqd.on('error', function(er) {
      console.error('Error', er, req.url);
      try {
        res.writeHead(500);
        res.end('Error occurred, sorry.');
        res.on('close', function() {
          // forcibly shut down any other things added to this domain
          reqd.dispose();
        });
      } catch (er) {
        console.error('Error sending 500', er, req.url);
        // tried our best.  clean up anything remaining.
        reqd.dispose();
      }
    });
  }).listen(1337);
});
```

## domain.create()

* return: {Domain}

Returns a new Domain object.

## Class: Domain

The Domain class encapsulates the functionality of routing errors and
uncaught exceptions to the active Domain object.

Domain is a child class of [EventEmitter][].  To handle the errors that it
catches, listen to its `error` event.

### domain.run(fn)

* `fn` {Function}

Run the supplied function in the context of the domain, implicitly
binding all event emitters, timers, and lowlevel requests that are
created in that context.

This is the most basic way to use a domain.

Example:

```
var d = domain.create();
d.on('error', function(er) {
  console.error('Caught error!', er);
});
d.run(function() {
  process.nextTick(function() {
    setTimeout(function() { // simulating some various async stuff
      fs.open('non-existent file', 'r', function(er, fd) {
        if (er) throw er;
        // proceed...
      });
    }, 100);
  });
});
```

In this example, the `d.on('error')` handler will be triggered, rather
than crashing the program.

### domain.members

* {Array}

An array of timers and event emitters that have been explicitly added
to the domain.

### domain.add(emitter)

* `emitter` {EventEmitter | Timer} emitter or timer to be added to the domain

Explicitly adds an emitter to the domain.  If any event handlers called by
the emitter throw an error, or if the emitter emits an `error` event, it
will be routed to the domain's `error` event, just like with implicit
binding.

This also works with timers that are returned from `setInterval` and
`setTimeout`.  If their callback function throws, it will be caught by
the domain 'error' handler.

If the Timer or EventEmitter was already bound to a domain, it is removed
from that one, and bound to this one instead.

### domain.remove(emitter)

* `emitter` {EventEmitter | Timer} emitter or timer to be removed from the domain

The opposite of `domain.add(emitter)`.  Removes domain handling from the
specified emitter.

### domain.bind(callback)

* `callback` {Function} The callback function
* return: {Function} The bound function

The returned function will be a wrapper around the supplied callback
function.  When the returned function is called, any errors that are
thrown will be routed to the domain's `error` event.

#### Example

    var d = domain.create();

    function readSomeFile(filename, cb) {
      fs.readFile(filename, 'utf8', d.bind(function(er, data) {
        // if this throws, it will also be passed to the domain
        return cb(er, data ? JSON.parse(data) : null);
      }));
    }

    d.on('error', function(er) {
      // an error occurred somewhere.
      // if we throw it now, it will crash the program
      // with the normal line number and stack message.
    });

### domain.intercept(callback)

* `callback` {Function} The callback function
* return: {Function} The intercepted function

This method is almost identical to `domain.bind(callback)`.  However, in
addition to catching thrown errors, it will also intercept `Error`
objects sent as the first argument to the function.

In this way, the common `if (er) return callback(er);` pattern can be replaced
with a single error handler in a single place.

#### Example

    var d = domain.create();

    function readSomeFile(filename, cb) {
      fs.readFile(filename, 'utf8', d.intercept(function(data) {
        // note, the first argument is never passed to the
        // callback since it is assumed to be the 'Error' argument
        // and thus intercepted by the domain.

        // if this throws, it will also be passed to the domain
        // so the error-handling logic can be moved to the 'error'
        // event on the domain instead of being repeated throughout
        // the program.
        return cb(null, JSON.parse(data));
      }));
    }

    d.on('error', function(er) {
      // an error occurred somewhere.
      // if we throw it now, it will crash the program
      // with the normal line number and stack message.
    });

### domain.dispose()

The dispose method destroys a domain, and makes a best effort attempt to
clean up any and all IO that is associated with the domain.  Streams are
aborted, ended, closed, and/or destroyed.  Timers are cleared.
Explicitly bound callbacks are no longer called.  Any error events that
are raised as a result of this are ignored.

The intention of calling `dispose` is generally to prevent cascading
errors when a critical part of the Domain context is found to be in an
error state.

Once the domain is disposed the `dispose` event will emit.

Note that IO might still be performed.  However, to the highest degree
possible, once a domain is disposed, further errors from the emitters in
that set will be ignored.  So, even if some remaining actions are still
in flight, Node.js will not communicate further about them.

[EventEmitter]: events.html#events_class_events_eventemitter
