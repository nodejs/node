# Dispatcher

<!--introduced_in=v4.0.0-->

<!--type=module-->

<!-- source_link=lib/dispatcher/dispatcher.js -->

> Stability: 2 - Stable

`Dispatcher` is the core abstraction used to dispatch HTTP requests in undici.
It extends [`EventEmitter`][] and defines the low-level [`dispatch()`][] contract
along with the higher-level [`request()`][], [`stream()`][], [`pipeline()`][],
[`connect()`][], and [`upgrade()`][] methods that every concrete dispatcher
provides.

`Dispatcher` itself is abstract: its [`dispatch()`][], [`close()`][], and
[`destroy()`][] methods throw `Error: not implemented`. Concrete dispatchers such
as [`Client`][], [`Pool`][], [`BalancedPool`][], and [`Agent`][] implement the
contract and are what application code instantiates. Requests are not guaranteed
to be dispatched in the order in which they are invoked.

```mjs
import { Dispatcher, Agent } from 'undici'

const dispatcher = new Agent()
console.log(dispatcher instanceof Dispatcher) // true
```

## Class: `Dispatcher`

<!-- YAML
added: v4.0.0
-->

* Extends: {EventEmitter}

Base dispatcher class. Concrete dispatchers extend it and implement the
[`dispatch()`][] method on which the other request helpers are built.

### `dispatcher.close([callback])`

<!-- YAML
added: v4.0.0
-->

* `callback` {Function} (optional) Invoked once the dispatcher is closed.
  * `error` {Error|null}
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with `void` once the dispatcher is closed.

Closes the dispatcher and gracefully waits for enqueued requests to complete
before resolving or invoking `callback`.

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end('undici')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

try {
  const { body } = await client.request({ path: '/', method: 'GET' })
  body.setEncoding('utf8')
  body.on('data', console.log)
} catch (error) {}

await client.close()

console.log('Client closed')
server.close()
```

### `dispatcher.connect(options[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {Object}
  * `origin` {string|URL} The origin to connect to.
  * `path` {string} The request path.
  * `headers` {UndiciHeaders} Request headers. **Default:** `null`.
  * `signal` {AbortSignal|EventEmitter|null} A signal used to abort the request.
    **Default:** `null`.
  * `opaque` {any} An opaque value passed through to the returned `ConnectData`.
  * `responseHeaders` {string|null} Set to `'raw'` to return the response headers
    as a raw array instead of an object. **Default:** `null`.
* `callback` {Function} (optional) Invoked when the connection is established.
  * `error` {Error|null}
  * `data` {Object}
    * `statusCode` {number}
    * `headers` {Object} The response headers.
    * `socket` {Duplex} The established socket.
    * `opaque` {any} The `opaque` value passed in `options`.
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with the `data` object described above.

Starts two-way communications with the requested resource using the
[HTTP `CONNECT`][] method.

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  throw new Error('should never get here')
}).listen()

server.on('connect', (req, socket, head) => {
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n')

  let data = head.toString()
  socket.on('data', (buf) => {
    data += buf.toString()
  })

  socket.on('end', () => {
    socket.end(data)
  })
})

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

try {
  const { socket } = await client.connect({ path: '/' })
  const wanted = 'Body'
  let data = ''
  socket.on('data', d => { data += d })
  socket.on('end', () => {
    console.log(`Data received: ${data.toString()} | Data wanted: ${wanted}`)
    client.close()
    server.close()
  })
  socket.write(wanted)
  socket.end()
} catch (error) {}
```

### `dispatcher.destroy([error[, callback]])`

<!-- YAML
added: v4.0.0
-->

* `error` {Error|null} (optional) The error with which pending requests are
  rejected. **Default:** `null`.
* `callback` {Function} (optional) Invoked once the socket is closed.
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with `void` once the dispatcher is destroyed.

Destroys the dispatcher abruptly with the given `error`. All pending and running
requests are asynchronously aborted and rejected. Because the operation is
dispatched asynchronously, dispatched requests may still make some progress
before being aborted.

Both arguments are optional, so the method can be called in four ways:

```mjs
dispatcher.destroy() // -> Promise
dispatcher.destroy(new Error()) // -> Promise
dispatcher.destroy(() => {}) // -> void
dispatcher.destroy(new Error(), () => {}) // -> void
```

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end()
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

try {
  const request = client.request({ path: '/', method: 'GET' })
  client.destroy()
    .then(() => {
      console.log('Client destroyed')
      server.close()
    })
  await request
} catch (error) {
  console.error(error)
}
```

### `dispatcher.dispatch(options, handler)`

<!-- YAML
added: v4.0.0
-->

* `options` {Object}
  * `origin` {string|URL} (optional) The origin to dispatch the request to.
  * `path` {string} The request path.
  * `method` {string} The HTTP method, e.g. `'GET'` or `'POST'`.
  * `body` {string|Buffer|Uint8Array|Readable|FormData|null} The request body.
    **Default:** `null`.
  * `headers` {UndiciHeaders} Request headers. **Default:** `null`.
  * `query` {Object} Query string parameters embedded into the request URL. Both
    keys and values are encoded with `encodeURIComponent`. **Default:** `null`.
  * `idempotent` {boolean} Whether the request can be safely retried. If `false`,
    the request is not sent until all preceding requests in the pipeline have
    completed. **Default:** `true` when `method` is `'HEAD'` or `'GET'`.
  * `blocking` {boolean} Whether the response is expected to block the pipeline.
    When `true`, further pipelining on the same connection is avoided until
    headers have been received. **Default:** `method !== 'HEAD'`.
  * `typeOfService` {number|null} The IP Type of Service (ToS) value for the
    request socket. Must be an integer between `0` and `255`. **Default:** `0`.
  * `upgrade` {boolean|string|null} Upgrades the request, e.g. to `'Websocket'`.
    **Default:** `null`, or the request method when it is `'CONNECT'`.
  * `headersTimeout` {number|null} The time, in milliseconds, the parser waits to
    receive the complete HTTP headers. Defaults to 300 seconds.
  * `bodyTimeout` {number|null} The time, in milliseconds, after which the request
    times out while receiving body data. Monitors the time between body chunks.
    Use `0` to disable it entirely. Defaults to 300 seconds.
    HTTP/1.1 headers/body parser timeouts are not guaranteed to fire with exact
    millisecond precision: delays up to 1000ms use native timers, while larger
    delays use undici's lower-overhead fast timers with a target resolution
    around 500ms.
  * `reset` {boolean} Whether the request should establish a keep-alive
    connection. **Default:** `false`.
  * `expectContinue` {boolean} For HTTP/2, appends the `expect: 100-continue`
    header and halts the request body until a `100 Continue` is received from the
    remote server. **Default:** `false`.
* `handler` {Object} A set of callbacks invoked over the request lifecycle.
  * `onRequestStart` {Function} Invoked before the request is dispatched on the
    socket. May be invoked multiple times when the request is retried.
    * `controller` {DispatchController}
    * `context` {any}
  * `onRequestUpgrade` {Function} (optional) Invoked when the request is upgraded.
    Required when `options.upgrade` is set or `options.method` is `'CONNECT'`.
    * `controller` {DispatchController}
    * `statusCode` {number}
    * `headers` {Object}
    * `socket` {Duplex}
  * `onResponseStart` {Function} (optional) Invoked when the status code and
    headers have been received. May be invoked multiple times for 1xx
    informational responses. Not used for `upgrade` requests.
    * `controller` {DispatchController}
    * `statusCode` {number}
    * `headers` {Object}
    * `statusMessage` {string} (optional)
  * `onResponseData` {Function} (optional) Invoked when response payload data is
    received. Not used for `upgrade` requests.
    * `controller` {DispatchController}
    * `chunk` {Buffer}
  * `onResponseEnd` {Function} (optional) Invoked when the response payload and
    trailers have been received and the request has completed. Not used for
    `upgrade` requests.
    * `controller` {DispatchController}
    * `trailers` {Object}
  * `onResponseError` {Function} (optional) Invoked when an error occurs. Must not
    throw.
    * `controller` {DispatchController}
    * `error` {Error}
  * `onResponseStarted` {Function} (optional) Invoked when the response is
    received, before headers have been read.
  * `onBodySent` {Function} (optional) Invoked when a chunk of the request body is
    sent. May be invoked multiple times for chunked requests.
    * `chunk` {Buffer}
  * `onRequestSent` {Function} (optional) Invoked after the request body is fully
    sent.
* Returns: {boolean} `false` when the dispatcher is busy and further `dispatch()`
  calls will make no progress until the [`'drain'`][] event is emitted.

This is the low-level API on which all the higher-level methods are built. This
API is expected to evolve through semver-major versions and is less stable than
the higher-level methods. It is primarily intended for library developers who
implement higher-level APIs on top of it.

The `controller` passed to each handler is a `DispatchController` with the
following shape:

* `aborted` {boolean} Whether the request has been aborted.
* `paused` {boolean} Whether the request is paused.
* `reason` {Error|null} The reason the request was aborted, if any.
* `rawHeaders` {Buffer[]|string[]|Object|null} The raw response headers,
  preserving duplicates and casing.
* `rawTrailers` {Buffer[]|string[]|Object|null} The raw response trailers.
* `abort(reason)` Aborts the request with the given `reason`.
* `pause()` Pauses the response stream.
* `resume()` Resumes the response stream.

Call `controller.pause()` and `controller.resume()` to apply backpressure rather
than returning `false` from a handler.

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

const data = []

client.dispatch({
  path: '/',
  method: 'GET',
  headers: { 'x-foo': 'bar' }
}, {
  onRequestStart: () => {
    console.log('Connected!')
  },
  onResponseError: (_controller, error) => {
    console.error(error)
  },
  onResponseStart: (_controller, statusCode, headers) => {
    console.log(`statusCode: ${statusCode} | headers: ${JSON.stringify(headers)}`)
  },
  onResponseData: (_controller, chunk) => {
    data.push(chunk)
  },
  onResponseEnd: (_controller, trailers) => {
    console.log(`trailers: ${JSON.stringify(trailers)}`)
    console.log(`Data: ${Buffer.concat(data).toString('utf8')}`)
    client.close()
    server.close()
  }
})
```

### `dispatcher.pipeline(options, handler)`

<!-- YAML
added: v4.0.0
-->

* `options` {Object} Accepts every field of [`dispatcher.request()`][]'s
  `options`, plus:
  * `objectMode` {boolean} Set to `true` when `handler` returns an object stream.
    **Default:** `false`.
* `handler` {Function} Returns a `Readable` from which the result is read.
  * `data` {Object}
    * `statusCode` {number}
    * `headers` {Object} The response headers.
    * `opaque` {any} The `opaque` value passed in `options`.
    * `body` {Readable} The response body.
    * `context` {Object}
  * Returns: {Readable}
* Returns: {Duplex} A duplex stream that writes to the request and reads from the
  response.

For easy use with [`stream.pipeline()`][]. The `handler` should return a
`Readable` from which the result is read. Usually it returns the `body` directly,
unless a transformation is needed based on, for example, `headers` or
`statusCode`. The `handler` should validate the response and save any required
state; if there is an error, it should be thrown.

```mjs
import { Readable, Writable, PassThrough, pipeline } from 'node:stream'
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  request.pipe(response)
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

let res = ''

pipeline(
  new Readable({
    read () {
      this.push(Buffer.from('undici'))
      this.push(null)
    }
  }),
  client.pipeline({ path: '/', method: 'GET' }, ({ statusCode, headers, body }) => {
    console.log(`response received ${statusCode}`)
    console.log('headers', headers)
    return pipeline(body, new PassThrough(), () => {})
  }),
  new Writable({
    write (chunk, _, callback) {
      res += chunk.toString()
      callback()
    },
    final (callback) {
      console.log(`Response pipelined to writable: ${res}`)
      callback()
    }
  }),
  error => {
    if (error) {
      console.error(error)
    }
    client.close()
    server.close()
  }
)
```

### `dispatcher.request(options[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {Object} Accepts every field of [`dispatcher.dispatch()`][]'s
  `options`, plus:
  * `opaque` {any} A value passed through to the returned response data.
    **Default:** `null`.
  * `signal` {AbortSignal|EventEmitter|null} A signal used to abort the request.
    **Default:** `null`.
  * `onInfo` {Function|null} Invoked for each informational (1xx) response
    received. **Default:** `null`.
    * `info` {Object}
      * `statusCode` {number}
      * `headers` {Object}
  * `responseHeaders` {string|null} Set to `'raw'` to return the response headers
    as a raw array. **Default:** `null`.
  * `highWaterMark` {number} The high water mark of the response body stream.
    **Default:** `65536` (64 KiB).
* `callback` {Function} (optional) Invoked when the response is received.
  * `error` {Error|null}
  * `data` {Object}
    * `statusCode` {number}
    * `statusText` {string} The status message, e.g. `'OK'` or `'Not Found'`.
    * `headers` {Object} The response headers. All keys are lower-cased.
    * `body` {Readable} The response body, which also implements the
      [body mixin][] from the Fetch Standard.
    * `trailers` {Object} Starts out empty and is mutated to contain the trailers
      after `body` emits `'end'`.
    * `opaque` {any} The `opaque` value passed in `options`.
    * `context` {Object}
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with the `data` object described above.

Performs an HTTP request. `options.method` must not be `'CONNECT'`.

Non-idempotent requests are not pipelined, to avoid indirect failures. Idempotent
requests are automatically retried if they fail due to an indirect failure from
the request at the head of the pipeline; this does not apply to idempotent
requests with a stream request body. All response bodies must always be fully
consumed or destroyed.

The `body` exposes the following [body mixin][] methods and properties:
[`.arrayBuffer()`][], [`.blob()`][], [`.bytes()`][], [`.json()`][],
[`.text()`][], `body`, and `bodyUsed`. A body cannot be consumed twice; for
example, calling `text()` after `json()` throws a `TypeError`. The body also
provides `dump({ limit })`, which discards up to `limit` bytes (default
`131072`) without destroying the socket.

The body is always a `Readable`, even when empty. Deserializing an empty body
with `json()` throws. To guard against this, verify the status code is not `204`
and the `content-type` header starts with `application/json` before calling
`json()`.

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

try {
  const { body, headers, statusCode, statusText, trailers } = await client.request({
    path: '/',
    method: 'GET'
  })
  console.log(`response received ${statusCode}`)
  console.log('headers', headers)
  body.setEncoding('utf8')
  body.on('data', console.log)
  body.on('error', console.error)
  body.on('end', () => {
    console.log('trailers', trailers)
  })

  client.close()
  server.close()
} catch (error) {
  console.error(error)
}
```

A request can be aborted with an `AbortSignal`, with any `EventEmitter` that
emits an `'abort'` event, or by destroying the response body:

```mjs
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
const abortController = new AbortController()

try {
  client.request({ path: '/', method: 'GET', signal: abortController.signal })
} catch (error) {
  console.error(error) // RequestAbortedError
}

abortController.abort()
```

When the response body is conditionally read, always fully consume it otherwise:

```mjs
const { body, statusCode } = await client.request({ path: '/', method: 'GET' })

if (statusCode === 200) {
  return await body.arrayBuffer()
}

await body.dump()

return null
```

### `dispatcher.stream(options, factory[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {Object} Accepts the same fields as [`dispatcher.request()`][]'s
  `options`.
* `factory` {Function} Returns the `Writable` to which the response is written.
  * `data` {Object}
    * `statusCode` {number}
    * `headers` {Object} The response headers.
    * `opaque` {any} The `opaque` value passed in `options`.
    * `context` {Object}
  * Returns: {Writable}
* `callback` {Function} (optional) Invoked when the request has completed.
  * `error` {Error|null}
  * `data` {Object}
    * `opaque` {any} The `opaque` value passed in `options`.
    * `trailers` {Object}
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with the `data` object described above.

A faster version of [`dispatcher.request()`][]. The `factory` returns a
[`Writable`][] to which the response is written, which avoids creating an
intermediate [`Readable`][] when the caller intends to pipe the response body
directly to a `Writable`.

Use `options.opaque` to avoid creating a closure for the `factory` method. This
pattern works well with Node.js web frameworks such as [Fastify][].

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'
import { Writable } from 'node:stream'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

const bufs = []

try {
  await client.stream({
    path: '/',
    method: 'GET',
    opaque: { bufs }
  }, ({ statusCode, headers, opaque: { bufs } }) => {
    console.log(`response received ${statusCode}`)
    console.log('headers', headers)
    return new Writable({
      write (chunk, encoding, callback) {
        bufs.push(chunk)
        callback()
      }
    })
  })

  console.log(Buffer.concat(bufs).toString('utf-8'))

  client.close()
  server.close()
} catch (error) {
  console.error(error)
}
```

### `dispatcher.upgrade(options[, callback])`

<!-- YAML
added: v4.0.0
-->

* `options` {Object}
  * `path` {string} The request path.
  * `method` {string} The HTTP method. **Default:** `'GET'`.
  * `headers` {UndiciHeaders} Request headers. **Default:** `null`.
  * `protocol` {string} A comma-separated list of protocols, in descending
    preference order. **Default:** `'Websocket'`.
  * `signal` {AbortSignal|EventEmitter|null} A signal used to abort the request.
    **Default:** `null`.
  * `responseHeaders` {string|null} Set to `'raw'` to return the response headers
    as a raw array. **Default:** `null`.
* `callback` {Function} (optional) Invoked when the upgrade completes.
  * `error` {Error|null}
  * `data` {Object}
    * `headers` {Object} The response headers.
    * `socket` {Duplex} The upgraded socket.
    * `opaque` {any}
* Returns: {Promise} A `Promise` is returned only when `callback` is omitted. It
  resolves with the `data` object described above.

Upgrades to a different protocol. See
[MDN: Protocol upgrade mechanism][] for more details.

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.statusCode = 101
  response.setHeader('connection', 'upgrade')
  response.setHeader('upgrade', request.headers.upgrade)
  response.end()
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

try {
  const { headers, socket } = await client.upgrade({ path: '/' })
  socket.on('end', () => {
    console.log(`upgrade: ${headers.upgrade}`) // upgrade: Websocket
    client.close()
    server.close()
  })
  socket.end()
} catch (error) {
  console.error(error)
  client.close()
  server.close()
}
```

### `dispatcher.compose(interceptors[, interceptor])`

<!-- YAML
added: v6.9.0
-->

* `interceptors` {Array|Function} An array of interceptor functions, or the first
  of several interceptors passed as separate arguments.
* `...interceptor` {Function} Additional interceptors when passed as separate
  arguments.
* Returns: {Dispatcher} A new dispatcher that applies the interceptors over the
  current dispatcher's [`dispatch()`][] method.

Composes a new dispatcher from the current dispatcher and the given
interceptors. Each interceptor is a function that takes a `dispatch` method and
returns a `dispatch`-like function with the same signature
(`(options, handler)`).

The order of the interceptors matters: the last interceptor in the chain is the
first to be called. Forking the chain of interceptors can lead to unexpected
results.

```text
compose([interceptor1, interceptor2, interceptor3])

Request flow:
Request -> interceptor3 -> interceptor2 -> interceptor1 -> dispatcher.dispatch
           (called first)  (called second) (called last)
```

```mjs
import { Client, RedirectHandler } from 'undici'

const redirectInterceptor = dispatch => {
  return (opts, handler) => {
    const { maxRedirections } = opts

    if (!maxRedirections) {
      return dispatch(opts, handler)
    }

    const redirectHandler = new RedirectHandler(dispatch, maxRedirections, opts, handler)
    opts = { ...opts, maxRedirections: 0 } // Stop the sub-dispatcher from also redirecting.
    return dispatch(opts, redirectHandler)
  }
}

const client = new Client('http://localhost:3000').compose(redirectInterceptor)

await client.request({ path: '/', method: 'GET' })
```

Interceptors can be chained by calling `compose()` repeatedly:

```mjs
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
  .compose(redirectInterceptor)
  .compose(retryInterceptor)

await client.request({ path: '/', method: 'GET' })
```

For the full list of built-in interceptors provided by undici, see [Interceptors](Interceptors.md).
For an example of a custom interceptor that wraps handler callbacks, see
[Custom interceptors](Interceptors.md#custom-interceptors).

### Event: `'connect'`

<!-- YAML
added: v4.0.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}

Emitted when the dispatcher has connected to the origin.

### Event: `'disconnect'`

<!-- YAML
added: v4.0.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}
* `error` {Error}

Emitted when the dispatcher has been disconnected from the origin.

For HTTP/2, this event is also emitted when the dispatcher receives a
[GOAWAY frame][], with an error whose message is `HTTP/2: "GOAWAY" frame received`
and whose code is `UND_ERR_INFO`. Because of the binary-framing nature of the
protocol, a request may hang if a frame is received between the `HEADER` and
`DATA` frames. It is recommended to handle this event and close the dispatcher to
create a new HTTP/2 session.

### Event: `'connectionError'`

<!-- YAML
added: v4.0.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}
* `error` {Error}

Emitted when the dispatcher fails to connect to the origin.

### Event: `'drain'`

<!-- YAML
added: v4.0.0
-->

* `origin` {URL}

Emitted when the dispatcher is no longer busy and further [`dispatch()`][] calls
can make progress.

## Pre-built interceptors

For the full reference of built-in interceptors (`dump`, `retry`, `redirect`,
`decompress`, `responseError`, `dns`, `cache`, `deduplicate`) and their options,
see [Interceptors](Interceptors.md).

[Fastify]: https://fastify.dev
[GOAWAY frame]: https://webconcepts.info/concepts/http2-frame-type/0x7
[HTTP `CONNECT`]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/CONNECT
[MDN: Protocol upgrade mechanism]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Protocol_upgrade_mechanism
[`.arrayBuffer()`]: https://fetch.spec.whatwg.org/#dom-body-arraybuffer
[`.blob()`]: https://fetch.spec.whatwg.org/#dom-body-blob
[`.bytes()`]: https://fetch.spec.whatwg.org/#dom-body-bytes
[`.json()`]: https://fetch.spec.whatwg.org/#dom-body-json
[`.text()`]: https://fetch.spec.whatwg.org/#dom-body-text
[`'drain'`]: #event-drain
[`Agent`]: Agent.md#class-agent
[`BalancedPool`]: BalancedPool.md#class-balancedpool
[`Client`]: Client.md#class-client
[`EventEmitter`]: https://nodejs.org/api/events.html#class-eventemitter
[`Pool`]: Pool.md#class-pool
[`Readable`]: https://nodejs.org/api/stream.html#class-streamreadable
[`Writable`]: https://nodejs.org/api/stream.html#class-streamwritable
[`close()`]: #dispatcherclosecallback
[`connect()`]: #dispatcherconnectoptions-callback
[`destroy()`]: #dispatcherdestroyerror-callback
[`dispatch()`]: #dispatcherdispatchoptions-handler
[`dispatcher.compose()`]: #dispatchercomposeinterceptors-interceptor
[`dispatcher.dispatch()`]: #dispatcherdispatchoptions-handler
[`dispatcher.request()`]: #dispatcherrequestoptions-callback
[`pipeline()`]: #dispatcherpipelineoptions-handler
[`request()`]: #dispatcherrequestoptions-callback
[`stream()`]: #dispatcherstreamoptions-factory-callback
[`stream.pipeline()`]: https://nodejs.org/api/stream.html#streampipelinesource-transforms-destination-options
[`upgrade()`]: #dispatcherupgradeoptions-callback
[body mixin]: https://fetch.spec.whatwg.org/#body-mixin
