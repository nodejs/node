# H2CClient

<!--introduced_in=v7.7.0-->
<!--type=module-->
<!-- source_link=lib/dispatcher/h2c-client.js -->

> Stability: 2 - Stable

An `H2CClient` is a [`Dispatcher`][] that speaks HTTP/2 in cleartext (h2c) over a
single TCP connection to an `http:` origin, without the protocol upgrade or prior
knowledge negotiation that a regular [`Client`][] performs. It is a thin
specialization of [`Client`][] that forces `allowH2` and `useH2c` on and aliases
`pipelining` to `maxConcurrentStreams`, so multiple requests are multiplexed as
HTTP/2 streams rather than pipelined HTTP/1.x requests.

Use an `H2CClient` when the server is known to accept cleartext HTTP/2 directly,
for example a service behind a trusted proxy or in a test environment. Only the
`http:` protocol is supported; passing any other protocol throws
`InvalidArgumentError`.

```mjs
import { H2CClient } from 'undici'

const client = new H2CClient('http://localhost:3000')

const { statusCode, body } = await client.request({ path: '/', method: 'GET' })
console.log(statusCode)
console.log(await body.text())
```

## Class: `H2CClient`

<!-- YAML
added: v7.7.0
changes:
  - version: v7.17.0
    pr-url: https://github.com/nodejs/undici/pull/4690
    description: Added support for h2c over Unix domain sockets.
-->

* Extends: {Client}

`H2CClient` inherits the full instance API of [`Client`][] (and therefore of
[`Dispatcher`][]). The sections below cover the surface that is meaningful for an
h2c client; refer to [`Dispatcher`][] for the complete semantics of each method
and event.

### `new H2CClient(url[, options])`

<!-- YAML
added: v7.7.0
-->

* `url` {URL|string} The origin to connect to. Should include only the protocol,
  hostname, and port. Only the `http:` protocol is supported.
* `options` {H2CClientOptions} (optional)
  * `maxConcurrentStreams` {number} The maximum number of concurrent HTTP/2
    streams for the session. It is advertised to the server and may be overridden
    by a remote `SETTINGS` frame. Must be a positive integer. **Default:** `100`.
  * `pipelining` {number} The number of concurrent requests multiplexed over the
    single connection. For an `H2CClient` this is aliased to
    `maxConcurrentStreams` and may not exceed it. Must be a positive integer.
    **Default:** `100`.
  * `maxHeaderSize` {number} The maximum length of request headers in bytes.
    **Default:** Node.js' `--max-http-header-size` or `16384` (16 KiB).
  * `headersTimeout` {number} The amount of time, in milliseconds, the parser
    waits to receive the complete HTTP headers. **Default:** `300e3`.
  * `connectTimeout` {number} The timeout for establishing a socket connection,
    in milliseconds. Use `0` to disable it entirely. **Default:** `10e3`.
  * `bodyTimeout` {number} The timeout, in milliseconds, after which a request
    times out. Monitors the time between received body data. Use `0` to disable
    it entirely. **Default:** `300e3`.
  * `keepAliveTimeout` {number} The timeout, in milliseconds, after which a
    socket without active requests times out. May be overridden by _keep-alive_
    hints from the server. **Default:** `4e3`.
  * `keepAliveMaxTimeout` {number} The maximum allowed `keepAliveTimeout`, in
    milliseconds, when overridden by _keep-alive_ hints from the server.
    **Default:** `600e3`.
  * `keepAliveTimeoutThreshold` {number} A number of milliseconds subtracted from
    server _keep-alive_ hints when overriding `keepAliveTimeout`, to account for
    timing inaccuracies such as transport latency. **Default:** `1e3`.
  * `socketPath` {string} An IPC endpoint, either a Unix domain socket or a
    Windows named pipe. **Default:** `null`.
  * `strictContentLength` {boolean} If `true`, an error is thrown when the
    request `content-length` header does not match the length of the request
    body. **Default:** `true`.
  * `maxCachedSessions` {number} The maximum number of TLS sessions cached by the
    built-in connector. Use `0` to disable TLS session caching. **Default:** `100`.
  * `connect` {Object|Function} Connector options passed to `buildConnector`, or
    a custom connector function. The `allowH2` option may not be set here.
    **Default:** `null`.
  * `maxRequestsPerClient` {number} The maximum number of requests to send over
    a single connection before it is reset. Use `0` to disable this limit.
    **Default:** `null`.
  * `localAddress` {string} The local IP address the socket should connect from.
  * `maxResponseSize` {number} The maximum allowed response body size in bytes.
    Use `-1` to disable. **Default:** `-1`.
  * `autoSelectFamily` {boolean} Enables a family autodetection algorithm that
    loosely implements section 5 of [RFC 8305][]. Ignored if not supported by the
    current Node.js version.
  * `autoSelectFamilyAttemptTimeout` {number} The amount of time, in
    milliseconds, to wait for a connection attempt to finish before trying the
    next address when `autoSelectFamily` is enabled.

Creates a new `H2CClient` for the given origin. The client does not connect until
a request is queued; call [`h2cClient.connect()`](#h2cclientconnectoptions-callback)
to connect eagerly.

```mjs
import { H2CClient } from 'undici'

const client = new H2CClient('http://localhost:3000')
```

### `h2cClient.close([callback])`

<!-- YAML
added: v7.7.0
-->

* `callback` {Function} (optional) Invoked once all queued requests have settled
  and the client is closed.
* Returns: {Promise|undefined} A `Promise` that resolves once the client is
  closed, when `callback` is not provided.

Closes the client and gracefully waits for enqueued requests to complete before
resolving. See [`dispatcher.close([callback])`][].

### `h2cClient.destroy([error[, callback]])`

<!-- YAML
added: v7.7.0
-->

* `error` {Error} (optional) The error with which to reject pending requests.
  **Default:** `null`.
* `callback` {Function} (optional) Invoked once the client is destroyed.
* Returns: {Promise|undefined} A `Promise` that resolves once the client is
  destroyed, when `callback` is not provided.

Destroys the client abruptly, aborting any pending requests. Waits until the
socket is closed before invoking `callback` (or resolving the returned `Promise`).
See [`dispatcher.destroy([error[, callback]])`][].

### `h2cClient.connect(options[, callback])`

<!-- YAML
added: v7.7.0
-->

* `options` {Object}
  * `path` {string}
  * `headers` {Object} (optional) **Default:** `null`.
  * `signal` {AbortSignal|EventEmitter|null} (optional) **Default:** `null`.
  * `opaque` {any} (optional) An opaque value passed through to the returned
    connect data.
  * `responseHeaders` {string|null} (optional) Set to `'raw'` to return the
    response headers as a raw array instead of an object. **Default:** `null`.
* `callback` {Function} (optional) Invoked with `(err, data)` once the connection
  is established. If omitted, a `Promise` is returned instead.
* Returns: {Promise|undefined} A `Promise` resolving to the connect data, when
  `callback` is not provided.

Starts an HTTP `CONNECT` request. See [`dispatcher.connect(options[, callback])`][].

### `h2cClient.dispatch(options, handlers)`

<!-- YAML
added: v7.7.0
-->

* `options` {Object} The request options.
* `handlers` {Object} The dispatch handler callbacks.
* Returns: {boolean} `false` if the dispatcher is busy and the caller should wait
  for the [`'drain'`](#event-drain) event before dispatching again.

The lowest-level request API. All other request methods are implemented on top of
it. See [`dispatcher.dispatch(options, handler)`][].

### `h2cClient.pipeline(options, handler)`

<!-- YAML
added: v7.7.0
-->

* `options` {Object} The request options.
* `handler` {Function} A handler that receives the response data and returns a
  {Readable} body.
* Returns: {Duplex} A duplex stream for the request body and response body.

Sends a request and returns a duplex stream, suitable for piping. See
[`dispatcher.pipeline(options, handler)`][].

### `h2cClient.request(options[, callback])`

<!-- YAML
added: v7.7.0
-->

* `options` {Object} The request options.
* `callback` {Function} (optional) Invoked with `(err, data)` once the response
  is received. If omitted, a `Promise` is returned instead.
* Returns: {Promise|undefined} A `Promise` resolving to the response data, when
  `callback` is not provided.

Performs an HTTP request. See [`dispatcher.request(options[, callback])`][].

### `h2cClient.stream(options, factory[, callback])`

<!-- YAML
added: v7.7.0
-->

* `options` {Object} The request options.
* `factory` {Function} A factory that returns a {Writable} to which the response
  body is written.
* `callback` {Function} (optional) Invoked once the request completes. If
  omitted, a `Promise` is returned instead.
* Returns: {Promise|undefined} A `Promise` that resolves once the request
  completes, when `callback` is not provided.

Sends a request and writes the response body to the stream returned by `factory`.
See [`dispatcher.stream(options, factory[, callback])`][].

### `h2cClient.upgrade(options[, callback])`

<!-- YAML
added: v7.7.0
-->

* `options` {Object} The request options.
* `callback` {Function} (optional) Invoked with `(err, data)` once the upgrade
  completes. If omitted, a `Promise` is returned instead.
* Returns: {Promise|undefined} A `Promise` resolving to the upgrade data, when
  `callback` is not provided.

Upgrades a connection to a different protocol. See
[`dispatcher.upgrade(options[, callback])`][].

### `h2cClient.closed`

<!-- YAML
added: v7.7.0
-->

* Type: {boolean}

`true` after [`h2cClient.close()`](#h2cclientclosecallback) has been called.

### `h2cClient.destroyed`

<!-- YAML
added: v7.7.0
-->

* Type: {boolean}

`true` after [`h2cClient.destroy()`](#h2cclientdestroyerror-callback) has been
called, or after [`h2cClient.close()`](#h2cclientclosecallback) has been called
and the client shutdown has completed.

### `h2cClient.pipelining`

<!-- YAML
added: v7.7.0
-->

* Type: {number}

Gets and sets the pipelining factor, i.e. the number of requests multiplexed over
the connection.

### Event: `'connect'`

<!-- YAML
added: v7.7.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}

Emitted when the socket has been created and connected. The client connects once
`client.size > 0`. See [Dispatcher Event: `'connect'`][].

```mjs
import { createServer } from 'node:http2'
import { once } from 'node:events'
import { H2CClient } from 'undici'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new H2CClient(`http://localhost:${server.address().port}`)

client.on('connect', (origin) => {
  console.log(`Connected to ${origin}`)
})

const { body } = await client.request({ path: '/', method: 'GET' })
console.log(await body.text())

client.close()
server.close()
```

### Event: `'disconnect'`

<!-- YAML
added: v7.7.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}
* `error` {Error}

Emitted when the socket has disconnected. The `error` argument is the error that
caused the socket to disconnect. The client reconnects if (or once)
`client.size > 0`. See [Dispatcher Event: `'disconnect'`][].

```mjs
import { createServer } from 'node:http2'
import { once } from 'node:events'
import { H2CClient } from 'undici'

const server = createServer((request, response) => {
  response.destroy()
}).listen()

await once(server, 'listening')

const client = new H2CClient(`http://localhost:${server.address().port}`)

client.on('disconnect', (origin) => {
  console.log(`Disconnected from ${origin}`)
})

try {
  await client.request({ path: '/', method: 'GET' })
} catch (error) {
  console.error(error.message)
} finally {
  client.close()
  server.close()
}
```

### Event: `'drain'`

<!-- YAML
added: v7.7.0
-->

* `origin` {URL}

Emitted when the client is no longer busy and can accept more requests. See
[Dispatcher Event: `'drain'`][].

### Event: `'error'`

<!-- YAML
added: v7.7.0
-->

* `error` {Error}

Emitted for user errors, such as an error thrown from an `onResponseError`
handler.

[RFC 8305]: https://tools.ietf.org/html/rfc8305#section-5
[Dispatcher Event: `'connect'`]: Dispatcher.md#event-connect
[Dispatcher Event: `'disconnect'`]: Dispatcher.md#event-disconnect
[Dispatcher Event: `'drain'`]: Dispatcher.md#event-drain
[`Client`]: Client.md#class-client
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`dispatcher.close([callback])`]: Dispatcher.md#dispatcherclosecallback
[`dispatcher.connect(options[, callback])`]: Dispatcher.md#dispatcherconnectoptions-callback
[`dispatcher.destroy([error[, callback]])`]: Dispatcher.md#dispatcherdestroyerror-callback
[`dispatcher.dispatch(options, handler)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.pipeline(options, handler)`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`dispatcher.stream(options, factory[, callback])`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`dispatcher.upgrade(options[, callback])`]: Dispatcher.md#dispatcherupgradeoptions-callback
