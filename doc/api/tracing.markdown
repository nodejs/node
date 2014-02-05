# Tracing

    Stability: 1 - Experimental

The tracing module is designed for instrumenting your Node application. It is
not meant for general purpose use.

***Be very careful with callbacks used in conjunction with this module***

Many of these callbacks interact directly with asynchronous subsystems in a
synchronous fashion. That is to say, you may be in a callback where a call to
`console.log()` could result in an infinite recursive loop.  Also of note, many
of these callbacks are in hot execution code paths. That is to say your
callbacks are executed quite often in the normal operation of Node, so be wary
of doing CPU bound or synchronous workloads in these functions. Consider a ring
buffer and a timer to defer processing.

`require('tracing')` to use this module.

## v8

The `v8` property is an [EventEmitter][], it exposes events and interfaces
specific to the version of `v8` built with node. These interfaces are subject
to change by upstream and are therefore not covered under the stability index.

### Event: 'gc'

`function (before, after) { }`

Emitted each time a GC run is completed.

`before` and `after` are objects with the following properties:

```
{
  type: 'mark-sweep-compact',
  flags: 0,
  timestamp: 905535650119053,
  total_heap_size: 6295040,
  total_heap_size_executable: 4194304,
  total_physical_size: 6295040,
  used_heap_size: 2855416,
  heap_size_limit: 1535115264
}
```

### getHeapStatistics()

Returns an object with the following properties

```
{
  total_heap_size: 7326976,
  total_heap_size_executable: 4194304,
  total_physical_size: 7326976,
  used_heap_size: 3476208,
  heap_size_limit: 1535115264
}
```


# Async Listeners

The `AsyncListener` API is the JavaScript interface for the `AsyncWrap`
class which allows developers to be notified about key events in the
lifetime of an asynchronous event. Node performs a lot of asynchronous
events internally, and significant use of this API may have a
**significant performance impact** on your application.


## tracing.createAsyncListener(callbacksObj[, userData])

* `callbacksObj` {Object} Contains optional callbacks that will fire at
specific times in the life cycle of the asynchronous event.
* `userData` {Value} a value that will be passed to all callbacks.

Returns a constructed `AsyncListener` object.

To begin capturing asynchronous events pass either the `callbacksObj` or
pass an existing `AsyncListener` instance to [`tracing.addAsyncListener()`][].
The same `AsyncListener` instance can only be added once to the active
queue, and subsequent attempts to add the instance will be ignored.

To stop capturing pass the `AsyncListener` instance to
[`tracing.removeAsyncListener()`][]. This does _not_ mean the
`AsyncListener` previously added will stop triggering callbacks. Once
attached to an asynchronous event it will persist with the lifetime of the
asynchronous call stack.

Explanation of function parameters:


`callbacksObj`: An `Object` which may contain several optional fields:

* `create(userData)`: A `Function` called when an asynchronous
event is instantiated. If a `Value` is returned then it will be attached
to the event and overwrite any value that had been passed to
`tracing.createAsyncListener()`'s `userData` argument. If an initial
`userData` was passed when created, then `create()` will
receive that as a function argument.

* `before(context, userData)`: A `Function` that is called immediately
before the asynchronous callback is about to run. It will be passed both
the `context` (i.e. `this`) of the calling function and the `userData`
either returned from `create()` or passed during construction (if
either occurred).

* `after(context, userData)`: A `Function` called immediately after
the asynchronous event's callback has run. Note this will not be called
if the callback throws and the error is not handled.

* `error(userData, error)`: A `Function` called if the event's
callback threw. If this registered callback returns `true` then Node will
assume the error has been properly handled and resume execution normally.
When multiple `error()` callbacks have been registered only **one** of
those callbacks needs to return `true` for `AsyncListener` to accept that
the error has been handled, but all `error()` callbacks will always be run.

`userData`: A `Value` (i.e. anything) that will be, by default,
attached to all new event instances. This will be overwritten if a `Value`
is returned by `create()`.

Here is an example of overwriting the `userData`:

    tracing.createAsyncListener({
      create: function listener(value) {
        // value === true
        return false;
    }, {
      before: function before(context, value) {
        // value === false
      }
    }, true);

**Note:** The [EventEmitter][], while used to emit status of an asynchronous
event, is not itself asynchronous. So `create()` will not fire when
an event is added, and `before()`/`after()` will not fire when emitted
callbacks are called.


## tracing.addAsyncListener(callbacksObj[, userData])
## tracing.addAsyncListener(asyncListener)

Returns a constructed `AsyncListener` object and immediately adds it to
the listening queue to begin capturing asynchronous events.

Function parameters can either be the same as
[`tracing.createAsyncListener()`][], or a constructed `AsyncListener`
object.

Example usage for capturing errors:

    var fs = require('fs');

    var cntr = 0;
    var key = tracing.addAsyncListener({
      create: function onCreate() {
        return { uid: cntr++ };
      },
      before: function onBefore(context, storage) {
        // Write directly to stdout or we'll enter a recursive loop
        fs.writeSync(1, 'uid: ' + storage.uid + ' is about to run\n');
      },
      after: function onAfter(context, storage) {
        fs.writeSync(1, 'uid: ' + storage.uid + ' ran\n');
      },
      error: function onError(storage, err) {
        // Handle known errors
        if (err.message === 'everything is fine') {
          // Writing to stderr this time.
          fs.writeSync(2, 'handled error just threw:\n');
          fs.writeSync(2, err.stack + '\n');
          return true;
        }
      }
    });

    process.nextTick(function() {
      throw new Error('everything is fine');
    });

    // Output:
    // uid: 0 is about to run
    // handled error just threw:
    // Error: really, it's ok
    //     at /tmp/test2.js:27:9
    //     at process._tickCallback (node.js:583:11)
    //     at Function.Module.runMain (module.js:492:11)
    //     at startup (node.js:123:16)
    //     at node.js:1012:3

## tracing.removeAsyncListener(asyncListener)

Removes the `AsyncListener` from the listening queue.

Removing the `AsyncListener` from the active queue does _not_ mean the
`asyncListener` callbacks will cease to fire on the events they've been
registered. Subsequently, any asynchronous events fired during the
execution of a callback will also have the same `asyncListener` callbacks
attached for future execution. For example:

    var fs = require('fs');

    var key = tracing.createAsyncListener({
      create: function asyncListener() {
        // Write directly to stdout or we'll enter a recursive loop
        fs.writeSync(1, 'You summoned me?\n');
      }
    });

    // We want to begin capturing async events some time in the future.
    setTimeout(function() {
      tracing.addAsyncListener(key);

      // Perform a few additional async events.
      setTimeout(function() {
        setImmediate(function() {
          process.nextTick(function() { });
        });
      });

      // Removing the listener doesn't mean to stop capturing events that
      // have already been added.
      tracing.removeAsyncListener(key);
    }, 100);

    // Output:
    // You summoned me?
    // You summoned me?
    // You summoned me?
    // You summoned me?

The fact that we logged 4 asynchronous events is an implementation detail
of Node's [Timers][].

To stop capturing from a specific asynchronous event stack
`tracing.removeAsyncListener()` must be called from within the call
stack itself. For example:

    var fs = require('fs');

    var key = tracing.createAsyncListener({
      create: function asyncListener() {
        // Write directly to stdout or we'll enter a recursive loop
        fs.writeSync(1, 'You summoned me?\n');
      }
    });

    // We want to begin capturing async events some time in the future.
    setTimeout(function() {
      tracing.addAsyncListener(key);

      // Perform a few additional async events.
      setImmediate(function() {
        // Stop capturing from this call stack.
        tracing.removeAsyncListener(key);

        process.nextTick(function() { });
      });
    }, 100);

    // Output:
    // You summoned me?

The user must be explicit and always pass the `AsyncListener` they wish
to remove. It is not possible to simply remove all listeners at once.


[EventEmitter]: events.html#events_class_events_eventemitter
[Timers]: timers.html
[`tracing.createAsyncListener()`]: #tracing_tracing_createasynclistener_asynclistener_callbacksobj_storagevalue
[`tracing.addAsyncListener()`]: #tracing_tracing_addasynclistener_asynclistener
[`tracing.removeAsyncListener()`]: #tracing_tracing_removeasynclistener_asynclistener
