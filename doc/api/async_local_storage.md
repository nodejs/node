# Async Hooks
<!--introduced_in: REPLACEME-->

> Stability: 1 - Experimental

## Class: `AsyncLocalStorage`
<!-- YAML
added: v13.10.0
-->

This class is used to create asynchronous state within callbacks and promise
chains. It allows storing data throughout the lifetime of a web request
or any other asynchronous duration. It is similar to thread-local storage
in other languages.

The following example uses `AsyncLocalStorage` to build a simple logger
that assigns IDs to incoming HTTP requests and includes them in messages
logged within each request.

```js
const http = require('http');
const AsyncLocalStorage = require('async_local_storage');

const asyncLocalStorage = new AsyncLocalStorage();

function logWithId(msg) {
  const id = asyncLocalStorage.getStore();
  console.log(`${id !== undefined ? id : '-'}:`, msg);
}

let idSeq = 0;
http.createServer((req, res) => {
  asyncLocalStorage.run(idSeq++, () => {
    logWithId('start');
    // Imagine any chain of async operations here
    setImmediate(() => {
      logWithId('finish');
      res.end();
    });
  });
}).listen(8080);

http.get('http://localhost:8080');
http.get('http://localhost:8080');
// Prints:
//   0: start
//   1: start
//   0: finish
//   1: finish
```

When having multiple instances of `AsyncLocalStorage`, they are independent
from each other. It is safe to instantiate this class multiple times.

### `new AsyncLocalStorage()`
<!-- YAML
added: v13.10.0
-->

Creates a new instance of `AsyncLocalStorage`. Store is only provided within a
`run` method call.

### `asyncLocalStorage.disable()`
<!-- YAML
added: v13.10.0
-->

This method disables the instance of `AsyncLocalStorage`. All subsequent calls
to `asyncLocalStorage.getStore()` will return `undefined` until
`asyncLocalStorage.run()` is called again.

When calling `asyncLocalStorage.disable()`, all current contexts linked to the
instance will be exited.

Calling `asyncLocalStorage.disable()` is required before the
`asyncLocalStorage` can be garbage collected. This does not apply to stores
provided by the `asyncLocalStorage`, as those objects are garbage collected
along with the corresponding async resources.

This method is to be used when the `asyncLocalStorage` is not in use anymore
in the current process.

### `asyncLocalStorage.getStore()`
<!-- YAML
added: v13.10.0
-->

* Returns: {any}

This method returns the current store.
If this method is called outside of an asynchronous context initialized by
calling `asyncLocalStorage.run`, it will return `undefined`.

### `asyncLocalStorage.enterWith(store)`
<!-- YAML
added: v13.11.0
-->

* `store` {any}

Calling `asyncLocalStorage.enterWith(store)` will transition into the context
for the remainder of the current synchronous execution and will persist
through any following asynchronous calls.

Example:

```js
const store = { id: 1 };
asyncLocalStorage.enterWith(store);
asyncLocalStorage.getStore(); // Returns the store object
someAsyncOperation(() => {
  asyncLocalStorage.getStore(); // Returns the same object
});
```

This transition will continue for the _entire_ synchronous execution.
This means that if, for example, the context is entered within an event
handler subsequent event handlers will also run within that context unless
specifically bound to another context with an `AsyncResource`.

```js
const store = { id: 1 };

emitter.on('my-event', () => {
  asyncLocalStorage.enterWith(store);
});
emitter.on('my-event', () => {
  asyncLocalStorage.getStore(); // Returns the same object
});

asyncLocalStorage.getStore(); // Returns undefined
emitter.emit('my-event');
asyncLocalStorage.getStore(); // Returns the same object
```

### `asyncLocalStorage.run(store, callback[, ...args])`
<!-- YAML
added: v13.10.0
-->

* `store` {any}
* `callback` {Function}
* `...args` {any}

This methods runs a function synchronously within a context and return its
return value. The store is not accessible outside of the callback function or
the asynchronous operations created within the callback.

Optionally, arguments can be passed to the function. They will be passed to
the callback function.

If the callback function throws an error, it will be thrown by `run` too.
The stacktrace will not be impacted by this call and the context will
be exited.

Example:

```js
const store = { id: 2 };
try {
  asyncLocalStorage.run(store, () => {
    asyncLocalStorage.getStore(); // Returns the store object
    throw new Error();
  });
} catch (e) {
  asyncLocalStorage.getStore(); // Returns undefined
  // The error will be caught here
}
```

### `asyncLocalStorage.exit(callback[, ...args])`
<!-- YAML
added: v13.10.0
-->

* `callback` {Function}
* `...args` {any}

This methods runs a function synchronously outside of a context and return its
return value. The store is not accessible within the callback function or
the asynchronous operations created within the callback.

Optionally, arguments can be passed to the function. They will be passed to
the callback function.

If the callback function throws an error, it will be thrown by `exit` too.
The stacktrace will not be impacted by this call and
the context will be re-entered.

Example:

```js
// Within a call to run
try {
  asyncLocalStorage.getStore(); // Returns the store object or value
  asyncLocalStorage.exit(() => {
    asyncLocalStorage.getStore(); // Returns undefined
    throw new Error();
  });
} catch (e) {
  asyncLocalStorage.getStore(); // Returns the same object or value
  // The error will be caught here
}
```

### Usage with `async/await`

If, within an async function, only one `await` call is to run within a context,
the following pattern should be used:

```js
async function fn() {
  await asyncLocalStorage.run(new Map(), () => {
    asyncLocalStorage.getStore().set('key', value);
    return foo(); // The return value of foo will be awaited
  });
}
```

In this example, the store is only available in the callback function and the
functions called by `foo`. Outside of `run`, calling `getStore` will return
`undefined`.
