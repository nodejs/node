# Domain

    Stability: 0 - Deprecated

The `domain` modules provides a mechanism for grouping and handling multiple IO
operations as a single group. `EventEmitters` and callback functions are
associated with a single `Domain` object. If any of those emit an `'error'`
event, or throw an error, then the `Domain` object is notified, rather than
losing the context of the error in the `process.on('uncaughtException')`
handler, or causing the program to exit immediately with an error code.

**This module is pending deprecation**. Once a replacement API has been
finalized, this module will be fully deprecated. Most end users should
**not** have cause to use this module. Users who absolutely must have
the functionality provided by the `domain` module may rely on it for the
time being but should expect to migrate to a different solution in the future.

## Warning: Do not Ignore Errors!

<!-- type=misc -->

Domain error handlers are not a substitute for closing down a process when
an error occurs.

By the very nature of how [`throw`][] works in JavaScript, when an error is
thrown, there is almost never a safe way to "pick up where you left off"
without leaking references, or creating some other sort of undefined brittle
state.

The safest way to respond to a thrown error is to shut down the
process.  Of course, in a normal web server, an application might have many
connections open, and it is not reasonable to abruptly shut those down
because an error was triggered by someone else.

The better approach is to handle thrown errors gracefully by sending an error
response to the request that triggered the error, while letting other requests
finish in their normal time.

The `domain` module can work well with the `cluster` module. For instance,
a master process can fork a new worker whenever a worker encounters an error.
Use of the `domain` module allows this kind of response to occur in a more
controlled manner.

For instance, in the example `cluster` application below, worker processes
are intentionally written to randomly throw errors. When these occur, the
worker process shuts down immediately, leading to an ungraceful exit of the
master process:

    // An example of what not to do
    const cluster = require('cluster');
    
    if (cluster.isMaster) {
      cluster.fork();
      cluster.fork();
    } else {
      function randomError() {
        var r = Math.floor(Math.random() * 11);
        if (r >= 7) throw new Error();
        setTimeout(randomError, 1000);
      }
      setTimeout(randomError, 1000);
    }

The `domain` module can be introduced within the worker process to provide
graceful error handling as in the following modified version. However, since
the errors are simply being ignored, resources are being leaked and held on
to.

    // Another example of what not to do
    const cluster = require('cluster');

    if (cluster.isMaster) {
      cluster.fork();
      cluster.fork();
    } else {
      const domain = require('domain').create();
      domain.on('error', (err) => {
        console.log('There was an error in the worker');
      });
      domain.run(() => {
        function randomError() {
          var r = Math.floor(Math.random() * 11);
          if (r >= 7) throw new Error();
          setTimeout(randomError, 1000);
        }
        setTimeout(randomError, 1000);
      });
    }

The better approach is to handle the error, disconnect the worker explicitly
from the master process, allow the failing worker process to exit, then have
the master create a new worker to take it's place:

    const cluster = require('cluster');

    if (cluster.isMaster) {
      cluster.fork();
      cluster.fork();
    
      // Spawn a new worker when one disconnects   
      cluster.on('disconnect', (worker) => {
        console.error(`${worker.id} failed. spawning a replacement.`);
        cluster.fork();
      });
      
    } else {
      const domain = require('domain').create();
      domain.on('error', (err) => {
        console.log(`There was an error in ${cluster.worker.id}.`);
        
        try {
          // Make sure the worker closes down within 30 seconds.
          // But don't keep the process open just for that!
          setTimeout(() => {
            process.exit(1);
          }, 30000).unref();

          // Let the master know this worker has failed.  
          // This will trigger a 'disconnect' in the cluster
          // master, and then it will fork a new worker.
          cluster.worker.disconnect();
        } catch (err2) {
          // There's nothing further we can do
          console.error('Error shutting down.', err2.stack);
        }
        
      });
      domain.run(() => {
        function randomError() {
          var r = Math.floor(Math.random() * 11);
          if (r >= 7) throw new Error();
          setTimeout(randomError, 1000);
        }
        setTimeout(randomError, 1000);
      });
    }

## Additions to Error objects

<!-- type=misc -->

Whenever an `Error` object is routed through a `Domain`, a number of
additional properties are added.

* `error.domain`: The `Domain` that first handled the error.
* `error.domainEmitter`: The `EventEmitter` that emitted an `'error'` event
  with the error object.
* `error.domainBound`: The callback function which was bound to the
  domain, and passed an error as its first argument.
* `error.domainThrown`: A boolean indicating whether the error was
  thrown, emitted, or passed to a bound callback function.

## Implicit Binding

<!--type=misc-->

When a `Domain` is in use, then all **new** `EventEmitter` objects (including
Stream objects, requests, responses, etc.) will be implicitly bound to
the active `Domain` at the time of their creation.

Additionally, callbacks passed to low-level event loop requests (such as
to `fs.open()`, or other callback-taking methods) will automatically be
bound to the active `Domain`.  If those callbacks throw, then the `Domain`
will catch the error.

To prevent excessive memory usage, `Domain` objects themselves are not
implicitly added as children of the active `Domain`. If they were, it would
be too easy to prevent request and response objects from being properly
garbage collected.

It is possible to nest `Domain` objects as children of a parent `Domain` by
explicitly adding those children to the parent.

Implicit binding routes thrown errors and `'error'` events to the
`Domain` objects `'error'` event, but does not register the `EventEmitter` on
the `Domain`, so [`domain.dispose()`][] will not shut down the EventEmitter.
Implicit binding only takes care of thrown errors and `'error'` events.

## Explicit Binding

<!--type=misc-->

There are times when implicit binding to the current `Domain` is not
appropriate and a particular `EventEmitter` should be bound to a different
`Domain` object. Doing so can be accomplished via explicit binding using the
`domain.add()` method.

For example:

    // Create a top-level domain for the server
    const domain = require('domain');
    const http = require('http');
    const serverDomain = domain.create();

    serverDomain.run(() => {
      // Server is implicitly bound to serverDomain
      http.createServer((req, res) => {
        // req and res are also implicitly bound to serverDomain
        // The preference, however, is to have a separate domain
        // for each request. First, create the new Domain, then
        // explicitly add req and res to it.
        var reqd = domain.create();
        reqd.add(req);
        reqd.add(res);
        reqd.on('error', (er) => {
          console.error('Error', er, req.url);
          try {
            res.writeHead(500);
            res.end('Error occurred, sorry.');
          } catch (er) {
            console.error('Error sending 500', er, req.url);
          }
        });
      }).listen(1337);
    });

## domain.create()

* return: {Domain}

Creates and returns a new `Domain` object. Instances of the `Domain` object are
not to be created using the `new` keyword.

    const domain = require('domain');
    const myDomain = domain.create();

## Class: Domain

The `Domain` class is an [`EventEmitter`][] that encapsulates the functionality
of routing errors and uncaught exceptions to the active Domain object.

When the `Domain` class intercepts an error, it will trigger the `'error'`
event and pass the caught error to the event handler.

### domain.run(fn[, arg][, ...])

* `fn` {Function}

Run the supplied `fn` function in the context of the domain, implicitly
binding all event emitters, timers, and low-level requests that are
created within that context.

When `domain.run()` is called with additional arguments, those are passed
to the `fn` function as input arguments.

For example:

    const domain = require('domain');
    const fs = require('fs');
    const d = domain.create();
    d.on('error', (er) => {
      console.error('Caught error!', er);
    });
    d.run(() => {
      process.nextTick(() => {
        setTimeout(() => { // simulating some various async stuff
          fs.open('non-existent file', 'r', (er, fd) => {
            if (er) throw er;
            // proceed...
          });
        }, 100);
      });
    });

In this example, the `d.on('error')` handler will be triggered, rather
than crashing the program.

### domain.members

* {Array}

Returns an array of timers and `EventEmitter` objects that have been explicitly
bound to the domain.

### domain.add(emitter)

* `emitter` {EventEmitter | Timer} emitter or timer to be added to the domain

Explicitly binds an `EventEmitter` or timer to the domain.  If any event
handlers called by the emitter throw an error, or if the `EventEmitter` emits
an `'error'` event, the error will be routed to the `Domain` objects own
`'error'` event.

Timers created and returned by [`setInterval()`][] and [`setTimeout()`][] can
be added to the `Domain`. If the timer callback function throws, the error will
be passed to the `Domain` `'error'` handler.

If the timer or `EventEmitter` was already bound to a `Domain`, it will be  
removed from that one, and bound to this one instead.

### domain.remove(emitter)

* `emitter` {EventEmitter | Timer} emitter or timer to be removed from the domain

Removes the specified `EventEmitter` or timer from this `Domain`.

### domain.bind(callback)

* `callback` {Function} The callback function
* return: {Function} The bound function

Wraps the supplied `callback` function in a new function that catches any
errors thrown by the `callback` and routes those to the `Domain` objects
`'error'` event.

For example:

    const domain = require('domain');
    const myDomain = domain.create();

    function readSomeFile(filename, cb) {
    
      const handler = myDomain.bind((err, data) => {
        return cb(err, data ? JSON.parse(data) : null);
      });
    
      fs.readFile(filename, 'utf8', handler);
    }
    
    myDomain.on('error', (err) => {
      console.error(`An error occurred: ${err.message}`);
    });

### domain.intercept(callback)

* `callback` {Function} The callback function
* return: {Function} The intercepted function

Wraps the supplied `callback` function in a new function that catches any
errors thrown by the `callback`, as well as any [`Error`][] objects passed
as the first argument to the `callback`, and routes those to the `Domain`
objects `'error'` event.

Using `domain.intercept()`, the idiomatic `if (err) return callback(err);`
pattern can be replaced with a single error handler established at the
`Domain`.

For example

    const domain = require('domain');
    const myDomain = domain.create();
    
    function readSomeFile(filename, cb) {
      
      const handler = myDomain.intercept((data) => {
        return cb(data ? JSON.parse(data) : null);
      });
    
      fs.readFile(filename, 'utf8', handler);
    }
    
    myDomain.on('error', (err) => {
      console.error(`An error occurred: ${err.message}`);
    });

### domain.enter()

The `domain.enter()` method is a utility used by the `domain.run()`,
`domain.bind()`, and `domain.intercept()` methods to set the active `Domain`.
When called, `domain.enter()` sets `domain.active` and `process.domain`
properties to the `Domain`, and implicitly pushes the `Domain` onto the domain
stack managed internally by the `domain` module (see [`domain.exit()`][] for
details on the domain stack). The call to `domain.enter()` delimits the
beginning of a chain of asynchronous calls and I/O operations bound to a domain.

Calling `domain.enter()` changes only the active `Domain`, and does not alter
the state of the `Domain` itself. The `domain.enter()` and `domain.exit()`
methods can be called an arbitrary number of times on a single `Domain`.

If the `Domain` on which `domain.enter()` is called has been disposed,
the method will return without setting the domain.

### domain.exit()

The `domain.exit()` method exits the current `Domain`, popping it off the
domain stack that is internally managed by the `domain` module. Any time
execution is going to switch to the context of a different chain of
asynchronous calls, it is important to ensure that the current `Domain` is
exited. The call to `domain.exit()` delimits either the end of, or an
interruption to, the chain of asynchronous calls and I/O operations bound to a
`Domain`.

If there are multiple, nested `Domain` objects bound to the current execution
context, `domain.exit()` will exit any nested `Domain` objets within this
`Domain`.

Calling `domain.exit()` changes only the active `Domain`, and does not alter
the state of the `Domain` itself. The `domain.enter()` and `domain.exit()`
methods can be called an arbitrary number of times on a single `Domain`.

If the `Domain` on which `domain.exit()` is called has been disposed,
the method will return without exiting the `Domain`.

### domain.dispose()

    Stability: 0 - Deprecated.  Please recover from failed IO actions
    explicitly via `'error'` event handlers set on the Domain.

Once `domain.dispose()` has been called, the `Domain` will no longer be used
by callback functions bound into the `Domain` via `domain.run()`,
`domain.bind()`, or `domain.intercept()`, and a `'dispose'` event is emitted.

[`domain.add(emitter)`]: #domain_domain_add_emitter
[`domain.bind(callback)`]: #domain_domain_bind_callback
[`domain.dispose()`]: #domain_domain_dispose
[`domain.exit()`]: #domain_domain_exit
[`Error`]: errors.html#errors_class_error
[`EventEmitter`]: events.html#events_class_events_eventemitter
[`setInterval()`]: timers.html#timers_setinterval_callback_delay_arg
[`setTimeout()`]: timers.html#timers_settimeout_callback_delay_arg
[`throw`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/throw
