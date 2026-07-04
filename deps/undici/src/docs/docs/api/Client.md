# Client

<!--introduced_in=v1.0.0-->

<!--type=module-->

<!-- source_link=lib/dispatcher/client.js -->

> Stability: 2 - Stable

A basic HTTP/1.1 client mapped on top of a single TCP/TLS connection. When the
server supports HTTP/2 and selects it during ALPN negotiation, the same
connection is used for HTTP/2. Pipelining is disabled by default.

`Client` extends [`Dispatcher`][] and is the lowest-level dispatcher in undici:
it manages exactly one origin over one connection. For pooling across multiple
connections use [`Pool`][], and for routing across multiple origins use
[`Agent`][]. Requests are not guaranteed to be dispatched in the order in which
they are invoked.

```mjs
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
const { statusCode, body } = await client.request({ path: '/', method: 'GET' })
```

## Class: `Client`

<!-- YAML
added: v1.0.0
-->

* Extends: {Dispatcher}

### `new Client(url[, options])`

<!-- YAML
added: v1.0.0
-->

* `url` {URL|string} The origin to connect to. Only the protocol, hostname, and
  port are used.
* `options` {Object} (optional)
  * `bodyTimeout` {number|null} The timeout, in milliseconds, after which a
    request times out while receiving body data. Monitors the time between
    consecutive body chunks. Use `0` to disable it entirely. **Default:**
    `300e3`.
  * `headersTimeout` {number|null} The timeout, in milliseconds, the parser
    waits to receive the complete HTTP headers before the request times out. Use
    `0` to disable it entirely. **Default:** `300e3`.
  * `connectTimeout` {number|null} The timeout, in milliseconds, for
    establishing a socket connection. Use `0` to disable it entirely.
    **Default:** `10e3`.
  * `keepAliveTimeout` {number|null} The timeout, in milliseconds, after which a
    socket with no active requests times out. Monitors the time between activity
    on a connected socket. This value may be overridden by *keep-alive* hints
    from the server. **Default:** `4e3`.
  * `keepAliveMaxTimeout` {number|null} The maximum allowed `keepAliveTimeout`,
    in milliseconds, when overridden by *keep-alive* hints from the server.
    **Default:** `600e3`.
  * `keepAliveTimeoutThreshold` {number|null} A number of milliseconds
    subtracted from server *keep-alive* hints when overriding `keepAliveTimeout`,
    to account for timing inaccuracies caused by e.g. transport latency.
    **Default:** `2e3`.
  * `maxHeaderSize` {number|null} The maximum length of request headers, in
    bytes. **Default:** the value of Node.js' `--max-http-header-size` or
    `16384` (16 KiB).
  * `maxResponseSize` {number|null} The maximum length of a response body, in
    bytes. Set to `-1` to disable it. **Default:** `-1`.
  * `maxRequestsPerClient` {number|null} The maximum number of requests to send
    over a single connection before the socket is reset. Use `0` to disable this
    limit. **Default:** `null`.
  * `localAddress` {string|null} The local IP address the socket should connect
    from. **Default:** `null`.
  * `pipelining` {number|null} The number of concurrent requests sent over the
    single connection, per [RFC 7230, section 6.3.2][]. Only enable values
    greater than `1` when the remote server is trusted. Set to `0` to disable
    keep-alive connections. Has no effect once HTTP/2 is negotiated; see
    `maxConcurrentStreams` for the HTTP/2 dispatch ceiling. **Default:** `1`.
  * `connect` {Object|Function|null} Configures how connections are established.
    Either a `ConnectOptions` object passed to the built-in
    [`buildConnector`][] (every [`tls.connect()`][] option is accepted, plus the
    fields below), or a custom connector function with the signature
    `(options, callback)`. **Default:** `null`.
    * `socketPath` {string|null} An IPC endpoint, either a Unix domain socket or
      a Windows named pipe. **Default:** `null`.
    * `maxCachedSessions` {number|null} The maximum number of cached TLS
      sessions. Use `0` to disable TLS session caching. **Default:** `100`.
    * `timeout` {number|null} The connection timeout, in milliseconds.
      **Default:** `10e3`.
    * `servername` {string|null} The TLS server name used for SNI.
    * `keepAlive` {boolean|null} Whether TCP keep-alive is enabled on the socket.
      **Default:** `true`.
    * `keepAliveInitialDelay` {number|null} The TCP keep-alive interval for the
      socket, in milliseconds. **Default:** `60000`.
  * `strictContentLength` {boolean} Whether to throw when the request
    `content-length` header does not match the length of the request body.
    Disabling this exposes the application to HTTP request smuggling; only do so
    in controlled environments where the request source is fully trusted.
    **Default:** `true`.
  * `autoSelectFamily` {boolean} Enables a family autodetection algorithm that
    loosely implements section 5 of [RFC 8305][]. Ignored if unsupported by the
    current Node.js version. **Default:** `false`.
  * `autoSelectFamilyAttemptTimeout` {number} The time, in milliseconds, to wait
    for a connection attempt to finish before trying the next address when
    `autoSelectFamily` is enabled. **Default:** `250`.
  * `allowH2` {boolean} Enables HTTP/2 support when the server assigns it a
    higher priority through ALPN negotiation. **Default:** `true`.
  * `useH2c` {boolean} Enforces h2c (HTTP/2 cleartext) for non-HTTPS
    connections. **Default:** `false`.
  * `maxConcurrentStreams` {number} The maximum number of concurrent HTTP/2
    streams for a single session. Once h2 is negotiated this — not `pipelining`,
    which is HTTP/1.1 only — is the ceiling used to dispatch in-flight requests.
    It may be overridden by the server's `SETTINGS_MAX_CONCURRENT_STREAMS`
    frame. **Default:** `100`.
  * `initialWindowSize` {number} The HTTP/2 stream-level flow-control window
    size (`SETTINGS_INITIAL_WINDOW_SIZE`). Must be a positive integer.
    **Default:** `262144`.
  * `connectionWindowSize` {number} The HTTP/2 connection-level flow-control
    window size set via `ClientHttp2Session.setLocalWindowSize()`. Must be a
    positive integer. **Default:** `524288`.
  * `pingInterval` {number} The time interval, in milliseconds, between HTTP/2
    PING frames. Set to `0` to disable PING frames. Applies only to HTTP/2
    connections and emits a `ping` event on the client. **Default:** `60e3`.
  * `webSocket` {Object} (optional) WebSocket-specific configuration.
    * `maxFragments` {number} The maximum number of fragments in a message. Set
      to `0` to disable the limit. **Default:** `131072`.
    * `maxPayloadSize` {number} The maximum allowed payload size, in bytes, for
      WebSocket messages. Applied to uncompressed messages, compressed frame
      payloads, and decompressed (`permessage-deflate`) messages. Set to `0` to
      disable the limit. **Default:** `134217728`.
* Returns: {Client}

Instantiating a `Client` does not open a connection; the connection is
established lazily once a request is queued. Call `client.connect()` to connect
eagerly.

> Notes about HTTP/2:
>
> * Pseudo headers (`:path`, `:method`, `:scheme`, `:authority`) are attached
>   automatically and overwrite any user-provided values.
> * The server must support HTTP/2 and select it during ALPN negotiation, and
>   must not give HTTP/1.1 a higher priority than HTTP/2.
> * `PUSH` frames are not supported.

```mjs displayName="Basic instantiation"
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
```

```mjs displayName="TLS options (object connector)"
import { Client } from 'undici'
import { readFileSync } from 'node:fs'

const client = new Client('https://localhost:3000', {
  connect: {
    rejectUnauthorized: false,
    ca: readFileSync('./ca-cert.pem')
  }
})
```

```mjs displayName="Unix domain socket"
import { Client } from 'undici'

const client = new Client('http://localhost:3000', {
  connect: {
    socketPath: '/var/run/docker.sock'
  }
})
```

```mjs displayName="Custom DNS lookup"
import { Client } from 'undici'
import dns from 'node:dns'

const client = new Client('https://example.com', {
  connect: {
    lookup (hostname, options, callback) {
      dns.lookup(hostname, { ...options, family: 4 }, callback)
    }
  }
})
```

```mjs displayName="Custom connector (function)"
import { Client, buildConnector } from 'undici'

const connector = buildConnector({ rejectUnauthorized: false })
const client = new Client('https://localhost:3000', {
  connect (opts, cb) {
    connector(opts, (err, socket) => {
      if (err) {
        cb(err)
      } else {
        cb(null, socket)
      }
    })
  }
})
```

When a connector function is provided, undici wraps it to automatically inject
`socketPath` and `allowH2` into the `options` argument when those values are set
on the client. See [`Connector`][] for details on building custom connectors.

### `client.close([callback])`

<!-- YAML
added: v1.0.0
-->

Implements [`dispatcher.close([callback])`][]. Closes the client gracefully,
waiting for enqueued requests to complete before resolving.

### `client.destroy([error[, callback]])`

<!-- YAML
added: v1.0.0
-->

Implements [`dispatcher.destroy([error[, callback]])`][]. Destroys the client
abruptly, aborting all pending and running requests, and waits until the socket
is closed before invoking `callback` (or resolving the returned promise).

### `client.connect(options[, callback])`

<!-- YAML
added: v1.0.0
-->

* `options` {Object} The connect options, identical to
  [`dispatcher.connect()`][] except that `origin` is omitted because the client
  is bound to a single origin.
* `callback` {Function} (optional)
* Returns: {Promise|void} A promise when `callback` is not provided.

Starts a connection to the client's origin and tunnels a connection through it,
as defined by [`dispatcher.connect()`][].

### `client.dispatch(options, handlers)`

<!-- YAML
added: v1.0.0
-->

Implements [`dispatcher.dispatch(options, handler)`][]. This is the lowest-level
API used by all other request methods; prefer `client.request()` for most use
cases.

### `client.pipeline(options, handler)`

<!-- YAML
added: v1.0.0
-->

See [`dispatcher.pipeline(options, handler)`][]. Sends a request and returns a
{Duplex} stream that the request body is written to and the response body is
read from.

### `client.request(options[, callback])`

<!-- YAML
added: v1.0.0
-->

* `options` {Object}
* `callback` {Function} (optional)
* Returns: {Promise|void} A promise resolving to the response data when
  `callback` is not provided.

See [`dispatcher.request(options[, callback])`][]. Performs an HTTP request and
returns its status code, headers, trailers, and a readable body stream.

```mjs
import { Client } from 'undici'

const client = new Client('http://localhost:3000')
const { statusCode, headers, body } = await client.request({
  path: '/',
  method: 'GET'
})

console.log(statusCode)
for await (const chunk of body) {
  console.log(chunk.toString())
}
```

### `client.stream(options, factory[, callback])`

<!-- YAML
added: v1.0.0
-->

* `options` {Object}
* `factory` {Function} Returns a {Writable} the response body is piped into.
* `callback` {Function} (optional)
* Returns: {Promise|void} A promise when `callback` is not provided.

See [`dispatcher.stream(options, factory[, callback])`][]. A faster alternative
to `client.request()` when the destination of the response body is known in
advance.

### `client.upgrade(options[, callback])`

<!-- YAML
added: v1.0.0
-->

* `options` {Object}
* `callback` {Function} (optional)
* Returns: {Promise|void} A promise when `callback` is not provided.

See [`dispatcher.upgrade(options[, callback])`][]. Upgrades a connection to a
different protocol, such as for WebSocket or HTTP `CONNECT`.

### `client.closed`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean}

`true` after `client.close()` has been called.

### `client.destroyed`

<!-- YAML
added: v1.0.0
-->

* Type: {boolean}

`true` after `client.destroy()` has been called, or after `client.close()` has
been called and the client shutdown has completed.

### `client.pipelining`

<!-- YAML
added: v1.0.0
-->

* Type: {number}

The pipelining factor. This property can be read and written to adjust the
number of concurrent requests sent over the connection. Only enable pipelining
with trusted remote servers.

### `client.stats`

<!-- YAML
added: v7.9.0
-->

* Type: {ClientStats}

Aggregate statistics for the client. See [`ClientStats`][].

### Event: `'connect'`

<!-- YAML
added: v1.0.0
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}

Emitted when a socket has been created and connected. The client connects once
`client.size > 0`. See [`Dispatcher` Event: `'connect'`][].

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

client.on('connect', (origin) => {
  console.log(`Connected to ${origin}`)
})

const { body } = await client.request({ path: '/', method: 'GET' })
body.setEncoding('utf8')
body.on('data', console.log)

client.close()
server.close()
```

### Event: `'disconnect'`

<!-- YAML
added: v1.0.0
changes:
  - version: v4.0.0
    pr-url: https://github.com/nodejs/undici/pull/771
    description: The event is only emitted if the client had previously
                 connected.
-->

* `origin` {URL}
* `targets` {Array<Dispatcher>}
* `error` {Error}

Emitted when a socket has disconnected. The `error` argument is the error that
caused the disconnection. The client reconnects if or once `client.size > 0`.
See [`Dispatcher` Event: `'disconnect'`][].

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.destroy()
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

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
added: v1.0.0
-->

* `origin` {URL}

Emitted when the pipeline is no longer busy. See
[`Dispatcher` Event: `'drain'`][].

```mjs
import { createServer } from 'node:http'
import { Client } from 'undici'
import { once } from 'node:events'

const server = createServer((request, response) => {
  response.end('Hello, World!')
}).listen()

await once(server, 'listening')

const client = new Client(`http://localhost:${server.address().port}`)

client.on('drain', () => {
  console.log('drain event')
  client.close()
  server.close()
})

await Promise.all([
  client.request({ path: '/', method: 'GET' }),
  client.request({ path: '/', method: 'GET' }),
  client.request({ path: '/', method: 'GET' })
])
```

### Event: `'error'`

<!-- YAML
added: v1.0.0
-->

* `error` {Error}

Emitted for user errors, such as throwing inside an `onResponseError` handler.

[RFC 7230, section 6.3.2]: https://tools.ietf.org/html/rfc7230#section-6.3.2
[RFC 8305]: https://tools.ietf.org/html/rfc8305#section-5
[`Agent`]: Agent.md#class-agent
[`ClientStats`]: ClientStats.md
[`Connector`]: Connector.md
[`Dispatcher` Event: `'connect'`]: Dispatcher.md#event-connect
[`Dispatcher` Event: `'disconnect'`]: Dispatcher.md#event-disconnect
[`Dispatcher` Event: `'drain'`]: Dispatcher.md#event-drain
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`Pool`]: Pool.md#class-pool
[`buildConnector`]: Connector.md
[`dispatcher.close([callback])`]: Dispatcher.md#dispatcherclosecallback
[`dispatcher.connect()`]: Dispatcher.md#dispatcherconnectoptions-callback
[`dispatcher.destroy([error[, callback]])`]: Dispatcher.md#dispatcherdestroyerror-callback
[`dispatcher.dispatch(options, handler)`]: Dispatcher.md#dispatcherdispatchoptions-handler
[`dispatcher.pipeline(options, handler)`]: Dispatcher.md#dispatcherpipelineoptions-handler
[`dispatcher.request(options[, callback])`]: Dispatcher.md#dispatcherrequestoptions-callback
[`dispatcher.stream(options, factory[, callback])`]: Dispatcher.md#dispatcherstreamoptions-factory-callback
[`dispatcher.upgrade(options[, callback])`]: Dispatcher.md#dispatcherupgradeoptions-callback
[`tls.connect()`]: https://nodejs.org/api/tls.html#tlsconnectoptions-callback
