# Global objects

<!--introduced_in=v0.10.0-->
<!-- type=misc -->

These objects are available in all modules. The following variables may appear
to be global but are not. They exist only in the scope of modules, see the
[module system documentation][]:

* [`__dirname`][]
* [`__filename`][]
* [`exports`][]
* [`module`][]
* [`require()`][]

The objects listed here are specific to Node.js. There are [built-in objects][]
that are part of the JavaScript language itself, which are also globally
accessible.

## Class: `AbortController`
<!--YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

<!-- type=global -->

A utility class used to signal cancelation in selected `Promise`-based APIs.
The API is based on the Web API [`AbortController`][].

```js
const ac = new AbortController();

ac.signal.addEventListener('abort', () => console.log('Aborted!'),
                           { once: true });

ac.abort();

console.log(ac.signal.aborted);  // Prints True
```

### `abortController.abort()`
<!-- YAML
added: REPLACEME
-->

Triggers the abort signal, causing the `abortController.signal` to emit
the `'abort'` event.

### `abortController.signal`
<!-- YAML
added: REPLACEME
-->

* Type: {AbortSignal}

### Class: `AbortSignal`
<!-- YAML
added: REPLACEME
-->

* Extends: {EventTarget}

The `AbortSignal` is used to notify observers when the
`abortController.abort()` method is called.

#### Event: `'abort'`
<!-- YAML
added: REPLACEME
-->

The `'abort'` event is emitted when the `abortController.abort()` method
is called. The callback is invoked with a single object argument with a
single `type` propety set to `'abort'`:

```js
const ac = new AbortController();

// Use either the onabort property...
ac.signal.onabort = () => console.log('aborted!');

// Or the EventTarget API...
ac.signal.addEventListener('abort', (event) => {
  console.log(event.type);  // Prints 'abort'
}, { once: true });

ac.abort();
```

The `AbortController` with which the `AbortSignal` is associated will only
ever trigger the `'abort'` event once. Any event listeners attached to the
`AbortSignal` *should* use the `{ once: true }` option (or, if using the
`EventEmitter` APIs to attach a listener, use the `once()` method) to ensure
that the event listener is removed as soon as the `'abort'` event is handled.
Failure to do so may result in memory leaks.

#### `abortSignal.aborted`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean} True after the `AbortController` has been aborted.

#### `abortSignal.onabort`
<!-- YAML
added: REPLACEME
-->

* Type: {Function}

An optional callback function that may be set by user code to be notified
when the `abortController.abort()` function has been called.

## Class: `Buffer`
<!-- YAML
added: v0.1.103
-->

<!-- type=global -->

* {Function}

Used to handle binary data. See the [buffer section][].

## `__dirname`

This variable may appear to be global but is not. See [`__dirname`][].

## `__filename`

This variable may appear to be global but is not. See [`__filename`][].

## `clearImmediate(immediateObject)`
<!-- YAML
added: v0.9.1
-->

<!--type=global-->

[`clearImmediate`][] is described in the [timers][] section.

## `clearInterval(intervalObject)`
<!-- YAML
added: v0.0.1
-->

<!--type=global-->

[`clearInterval`][] is described in the [timers][] section.

## `clearTimeout(timeoutObject)`
<!-- YAML
added: v0.0.1
-->

<!--type=global-->

[`clearTimeout`][] is described in the [timers][] section.

## `console`
<!-- YAML
added: v0.1.100
-->

<!-- type=global -->

* {Object}

Used to print to stdout and stderr. See the [`console`][] section.

## `exports`

This variable may appear to be global but is not. See [`exports`][].

## `global`
<!-- YAML
added: v0.1.27
-->

<!-- type=global -->

* {Object} The global namespace object.

In browsers, the top-level scope is the global scope. This means that
within the browser `var something` will define a new global variable. In
Node.js this is different. The top-level scope is not the global scope;
`var something` inside a Node.js module will be local to that module.

## `module`

This variable may appear to be global but is not. See [`module`][].

## `process`
<!-- YAML
added: v0.1.7
-->

<!-- type=global -->

* {Object}

The process object. See the [`process` object][] section.

## `queueMicrotask(callback)`
<!-- YAML
added: v11.0.0
-->

<!-- type=global -->

* `callback` {Function} Function to be queued.

The `queueMicrotask()` method queues a microtask to invoke `callback`. If
`callback` throws an exception, the [`process` object][] `'uncaughtException'`
event will be emitted.

The microtask queue is managed by V8 and may be used in a similar manner to
the [`process.nextTick()`][] queue, which is managed by Node.js. The
`process.nextTick()` queue is always processed before the microtask queue
within each turn of the Node.js event loop.

```js
// Here, `queueMicrotask()` is used to ensure the 'load' event is always
// emitted asynchronously, and therefore consistently. Using
// `process.nextTick()` here would result in the 'load' event always emitting
// before any other promise jobs.

DataHandler.prototype.load = async function load(key) {
  const hit = this._cache.get(url);
  if (hit !== undefined) {
    queueMicrotask(() => {
      this.emit('load', hit);
    });
    return;
  }

  const data = await fetchData(key);
  this._cache.set(url, data);
  this.emit('load', data);
};
```

## `require()`

This variable may appear to be global but is not. See [`require()`][].

## `setImmediate(callback[, ...args])`
<!-- YAML
added: v0.9.1
-->

<!-- type=global -->

[`setImmediate`][] is described in the [timers][] section.

## `setInterval(callback, delay[, ...args])`
<!-- YAML
added: v0.0.1
-->

<!-- type=global -->

[`setInterval`][] is described in the [timers][] section.

## `setTimeout(callback, delay[, ...args])`
<!-- YAML
added: v0.0.1
-->

<!-- type=global -->

[`setTimeout`][] is described in the [timers][] section.

## `TextDecoder`
<!-- YAML
added: v11.0.0
-->

<!-- type=global -->

The WHATWG `TextDecoder` class. See the [`TextDecoder`][] section.

## `TextEncoder`
<!-- YAML
added: v11.0.0
-->

<!-- type=global -->

The WHATWG `TextEncoder` class. See the [`TextEncoder`][] section.

## `URL`
<!-- YAML
added: v10.0.0
-->

<!-- type=global -->

The WHATWG `URL` class. See the [`URL`][] section.

## `URLSearchParams`
<!-- YAML
added: v10.0.0
-->

<!-- type=global -->

The WHATWG `URLSearchParams` class. See the [`URLSearchParams`][] section.

## `WebAssembly`
<!-- YAML
added: v8.0.0
-->

<!-- type=global -->

* {Object}

The object that acts as the namespace for all W3C
[WebAssembly][webassembly-org] related functionality. See the
[Mozilla Developer Network][webassembly-mdn] for usage and compatibility.

[`AbortController`]: https://developer.mozilla.org/en-US/docs/Web/API/AbortController
[`TextDecoder`]: util.html#util_class_util_textdecoder
[`TextEncoder`]: util.html#util_class_util_textencoder
[`URLSearchParams`]: url.html#url_class_urlsearchparams
[`URL`]: url.html#url_class_url
[`__dirname`]: modules.html#modules_dirname
[`__filename`]: modules.html#modules_filename
[`clearImmediate`]: timers.html#timers_clearimmediate_immediate
[`clearInterval`]: timers.html#timers_clearinterval_timeout
[`clearTimeout`]: timers.html#timers_cleartimeout_timeout
[`console`]: console.html
[`exports`]: modules.html#modules_exports
[`module`]: modules.html#modules_module
[`process.nextTick()`]: process.html#process_process_nexttick_callback_args
[`process` object]: process.html#process_process
[`require()`]: modules.html#modules_require_id
[`setImmediate`]: timers.html#timers_setimmediate_callback_args
[`setInterval`]: timers.html#timers_setinterval_callback_delay_args
[`setTimeout`]: timers.html#timers_settimeout_callback_delay_args
[buffer section]: buffer.html
[built-in objects]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
[module system documentation]: modules.html
[timers]: timers.html
[webassembly-mdn]: https://developer.mozilla.org/en-US/docs/WebAssembly
[webassembly-org]: https://webassembly.org
