# Migrating from Undici 7 to 8

This guide covers the changes you are most likely to hit when upgrading an
application or library from Undici v7 to v8.

## Before you upgrade

- Make sure your runtime is Node.js `>= 22.19.0`.
- If you have custom dispatchers, interceptors, or handlers, review the
  handler API changes before updating.
- If you rely on HTTP/1.1-only behavior, plan to set `allowH2: false`
  explicitly.

## 1. Update your Node.js version

Undici v8 requires Node.js `>= 22.19.0`.

If you are still on Node.js 20 or an older Node.js 22 release, upgrade Node.js
first:

```bash
node -v
```

If that command prints a version lower than `v22.19.0`, upgrade Node.js before
installing Undici v8.

## 2. Migrate custom dispatcher handlers to the v2 API

Undici v8 uses the newer dispatcher handler API consistently.

If you implemented custom dispatchers, interceptors, or wrappers around
`dispatch()`, update legacy callbacks such as `onConnect`, `onHeaders`, and
`onComplete` to the newer callback names.

### Old handler callbacks vs. v8 callbacks

| Undici 7 style | Undici 8 style |
|---|---|
| `onConnect(abort, context)` | `onRequestStart(controller, context)` |
| `onHeaders(statusCode, rawHeaders, resume, statusText)` | `onResponseStart(controller, statusCode, headers, statusText)` |
| `onData(chunk)` | `onResponseData(controller, chunk)` |
| `onComplete(trailers)` | `onResponseEnd(controller, trailers)` |
| `onError(err)` | `onResponseError(controller, err)` |
| `onUpgrade(statusCode, rawHeaders, socket)` | `onRequestUpgrade(controller, statusCode, headers, socket)` |

### Example

Before:

```js
client.dispatch(options, {
  onConnect (abort) {
    this.abort = abort
  },
  onHeaders (statusCode, headers, resume) {
    this.resume = resume
    return true
  },
  onData (chunk) {
    chunks.push(chunk)
    return true
  },
  onComplete (trailers) {
    console.log(trailers)
  },
  onError (err) {
    console.error(err)
  }
})
```

After:

```js
client.dispatch(options, {
  onRequestStart (controller) {
    this.controller = controller
  },
  onResponseStart (controller, statusCode, headers, statusText) {
    console.log(statusCode, statusText, headers)
  },
  onResponseData (controller, chunk) {
    chunks.push(chunk)
  },
  onResponseEnd (controller, trailers) {
    console.log(trailers)
  },
  onResponseError (controller, err) {
    console.error(err)
  }
})
```

### Pause, resume, and abort now go through the controller

In Undici v7, legacy handlers could return `false` or keep references to
`abort()` and `resume()` callbacks. In Undici v8, use the controller instead:

```js
onRequestStart (controller) {
  this.controller = controller
}

onResponseData (controller, chunk) {
  controller.pause()
  setImmediate(() => controller.resume())
}

onResponseError (controller, err) {
  controller.abort(err)
}
```

### Raw headers and trailers moved to the controller

If you need the raw header arrays, read them from the controller:

- `controller.rawHeaders`
- `controller.rawTrailers`

## 3. Update `onBodySent()` handlers

If you implemented `onBodySent()`, note that its signature changed.

Before, handlers received counters:

```js
onBodySent (chunkSize, totalBytesSent) {}
```

In Undici v8, handlers receive the actual chunk:

```js
onBodySent (chunk) {}
```

If you need a notification that the whole body has been sent, use
`onRequestSent()`:

```js
onRequestSent () {
  console.log('request body fully sent')
}
```

## 4. If you need HTTP/1.1 only, disable HTTP/2 explicitly

Undici v8 enables HTTP/2 by default when a TLS server negotiates it via ALPN.

If your application depends on HTTP/1.1-specific behavior, set `allowH2: false`
explicitly.

Before:

```js
const client = new Client('https://example.com')
```

After, to keep HTTP/1.1 only:

```js
const client = new Client('https://example.com', {
  allowH2: false
})
```

The same applies when you configure an `Agent`:

```js
const agent = new Agent({
  allowH2: false
})
```

## 5. Use real `Blob` and `File` instances

Undici v8 no longer accepts fake Blob-like values that only imitate `Blob` or
`File` via properties such as `Symbol.toStringTag`.

If you were passing custom objects that looked like `Blob`s, replace them with
actual `Blob` or `File` instances:

```js
const body = new Blob(['hello'])
```

## 6. Avoid depending on the internal global dispatcher symbol

`setGlobalDispatcher()` and `getGlobalDispatcher()` remain the public APIs and
should continue to be used.

Internally, Undici v8 stores its dispatcher under
`Symbol.for('undici.globalDispatcher.2')` and mirrors a v1-compatible wrapper
for legacy consumers such as Node.js built-in `fetch`.

If your code was reading or writing `Symbol.for('undici.globalDispatcher.1')`
directly, migrate to the public APIs instead:

```js
import { setGlobalDispatcher, getGlobalDispatcher, Agent } from 'undici'

setGlobalDispatcher(new Agent())
const dispatcher = getGlobalDispatcher()
```

If you must expose a dispatcher to legacy v1 handler consumers, wrap it with
`Dispatcher1Wrapper`:

```js
import { Agent, Dispatcher1Wrapper } from 'undici'

const legacyCompatibleDispatcher = new Dispatcher1Wrapper(new Agent())
```

## 7. Verify the upgrade

After moving to Undici v8, it is worth checking these paths in your test suite:

- requests that use a custom `dispatcher`
- `setGlobalDispatcher()` behavior
- any custom interceptor or retry handler
- uploads that use `Blob`, `File`, or `FormData`
- integrations that depend on HTTP/1.1-only behavior

## Related documentation

- [Dispatcher](/docs/api/Dispatcher.md)
- [Client](/docs/api/Client.md)
- [Global Installation](/docs/api/GlobalInstallation.md)
- [Undici Module vs. Node.js Built-in Fetch](/docs/best-practices/undici-vs-builtin-fetch.md)
