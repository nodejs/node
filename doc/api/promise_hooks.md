# Promise hooks

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!-- source_link=lib/promise_hooks.js -->

The `promise_hooks` module provides an API to track promise lifecycle events.
To track _all_ async activity, see [`async_hooks`][] which internally uses this
module to produce promise lifecycle events in addition to events for other
async resources. For request context management, see [`AsyncLocalStorage`][].

It can be accessed using:

```mjs
import promiseHooks from 'promise_hooks';
```

```cjs
const promiseHooks = require('promise_hooks');
```

## Overview

Following is a simple overview of the public API.

```mjs
import promiseHooks from 'promise_hooks';

// There are four lifecycle events produced by promises:

// The `init` event represents the creation of a promise. This could be a
// direct creation such as with `new Promise(...)` or a continuation such
// as `then()` or `catch()`. It also happens whenever an async function is
// called or does an `await`. If a continuation promise is created, the
// `parent` will be the promise it is a continuation from.
function init(promise, parent) {
  console.log('a promise was created', { promise, parent });
}

// The `resolve` event happens when a promise receives a resolution or
// rejection value. This may happen synchronously such as when using
// `Promise.resolve()` on non-promise input.
function resolve(promise) {
  console.log('a promise resolved or rejected', { promise });
}

// The `before` event runs immediately before a `then()` handler runs or
// an `await` resumes execution.
function before(promise) {
  console.log('a promise is about to call a then handler', { promise });
}

// The `after` event runs immediately after a `then()` handler runs or when
// an `await` begins after resuming from another.
function after(promise) {
  console.log('a promise is done calling a then handler', { promise });
}

// Lifecycle hooks may be started and stopped individually
const stopWatchingInits = promiseHooks.onInit(init);
const stopWatchingResolves = promiseHooks.onResolve(resolve);
const stopWatchingBefores = promiseHooks.onBefore(before);
const stopWatchingAfters = promiseHooks.onAfter(after);

// Or they may be started and stopped in groups
const stopAll = promiseHooks.createHook({
  init,
  resolve,
  before,
  after
});

// To stop a hook, call the function returned at its creation.
stopWatchingInits();
stopWatchingResolves();
stopWatchingBefores();
stopWatchingAfters();
stopAll();
```

## `promiseHooks.createHook(callbacks)`

* `callbacks` {Object} The [Hook Callbacks][] to register
  * `init` {Function} The [`init` callback][].
  * `before` {Function} The [`before` callback][].
  * `after` {Function} The [`after` callback][].
  * `resolve` {Function} The [`resolve` callback][].
* Returns: {Function} Used for disabling hooks

Registers functions to be called for different lifetime events of each promise.

The callbacks `init()`/`before()`/`after()`/`resolve()` are called for the
respective events during a promise's lifetime.

All callbacks are optional. For example, if only promise creation needs to
be tracked, then only the `init` callback needs to be passed. The
specifics of all functions that can be passed to `callbacks` is in the
[Hook Callbacks][] section.

```mjs
import promiseHooks from 'promise_hooks';

const stopAll = promiseHooks.createHook({
  init(promise, parent) {}
});
```

```cjs
const promiseHooks = require('promise_hooks');

const stopAll = promiseHooks.createHook({
  init(promise, parent) {}
});
```

### Hook callbacks

Key events in the lifetime of a promise have been categorized into four areas:
creation of a promise, before/after a continuation handler is called or around
an await, and when the promise resolves or rejects.

While these hooks are similar to those of [`async_hooks`][] they lack a
`destroy` hook. Other types of async resources typically represent sockets or
file descriptors which have a distinct "closed" state to express the `destroy`
lifecycle event while promises remain usable for as long as code can still
reach them. Garbage collection tracking is used to make promises fit into the
`async_hooks` event model, however this tracking is very expensive and they may
not necessarily ever even be garbage collected.

#### `init(promise, parent)`

* `promise` {Promise} The promise being created.
* `parent` {Promise} The promise continued from, if applicable.

Called when a promise is constructed. This _does not_ mean that corresponding
`before`/`after` events will occur, only that the possibility exists. This will
happen if a promise is created without ever getting a continuation.

#### `before(promise)`

* `promise` {Promise}

Called before a promise continuation executes. This can be in the form of a
`then()` handler or an `await` resuming.

The `before` callback will be called 0 to N times. The `before` callback
will typically be called 0 times if no continuation was ever made for the
promise. The `before` callback may be called many times in the case where
many continuations have been made from the same promise.

#### `after(promise)`

* `promise` {Promise}

Called immediately after a promise continuation executes. This may be after a
`then()` handler or before an `await` after another `await`.

#### `resolve(promise)`

* `promise` {Promise}

Called when the promise receives a resolution or rejection value. This may
occur synchronously in the case of `Promise.resolve()` or `Promise.reject()`.

## `promiseHooks.onInit(init)`

* `init` {Function} The [`init` callback][] to call when a promise is created.
* Returns: {Function} Call to stop the hook.

## `promiseHooks.onResolve(resolve)`

* `resolve` {Function} The [`resolve` callback][] to call when a promise
  is resolved or rejected.
* Returns: {Function} Call to stop the hook.

## `promiseHooks.onBefore(before)`

* `before` {Function} The [`before` callback][] to call before a promise
  continuation executes.
* Returns: {Function} Call to stop the hook.

## `promiseHooks.onAfter(after)`

* `after` {Function} The [`after` callback][] to call after a promise
  continuation executes.
* Returns: {Function} Call to stop the hook.

[Hook Callbacks]: #promise_hooks_hook_callbacks
[`AsyncLocalStorage`]: async_context.md#async_context_class_asynclocalstorage
[`after` callback]: #promise_hooks_after_promise
[`async_hooks`]: async_hooks.md#async_hooks_async_hooks
[`before` callback]: #promise_hooks_before_promise
[`init` callback]: #promise_hooks_init_promise_parent
[`resolve` callback]: #promise_hooks_resolve_promise
