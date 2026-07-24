# Diagnostics Channel Support

<!--introduced_in=v6.3.0-->
<!--type=misc-->

> Stability: 1 - Experimental

Undici is instrumented with Node.js' built-in [`diagnostics_channel`][] module.
It is the preferred way to observe undici's internal behaviour without patching
the library, and it backs the human-readable [debug logs][Debug].

Each integration point is exposed as a named channel. To observe an event,
obtain the channel by name and subscribe to it:

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:create').subscribe(({ request }) => {
  console.log(request.method, request.origin, request.path)
})
```

```cjs
const diagnosticsChannel = require('node:diagnostics_channel')

diagnosticsChannel.channel('undici:request:create').subscribe(({ request }) => {
  console.log(request.method, request.origin, request.path)
})
```

The message published to a channel is always a single object whose shape depends
on the channel. The sections below describe every channel and the object it
publishes. Subscriber callbacks run synchronously inside undici, so they should
be fast and must not throw.

The same `request` object is shared across all `undici:request:*` channels for a
given request, so a subscriber may correlate the lifecycle of one request by
identity.

## Event: `'undici:request:create'`

<!-- YAML
added: v6.3.0
-->

Published when a new outgoing request is created, before it is dispatched.

* `message` {Object}
  * `request` {Object} The request being created.
    * `origin` {string|URL} The origin the request is sent to.
    * `completed` {boolean} Whether the request has completed. `false` at this
      point.
    * `method` {string} The HTTP method, e.g. `'GET'`.
    * `path` {string} The request path, including any query string.
    * `headers` {string[]} The request headers as a flat array of strings,
      alternating between header name and value.

The `request` object also exposes an `addHeader(name, value)` method that
appends a header before the request is sent.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:create').subscribe(({ request }) => {
  console.log('origin', request.origin)
  console.log('completed', request.completed)
  console.log('method', request.method)
  console.log('path', request.path)
  console.log('headers', request.headers)
  request.addHeader('hello', 'world')
})
```

A request is only loosely bound to a given socket at this stage.

## Event: `'undici:request:bodyChunkSent'`

<!-- YAML
added: v7.11.0
-->

Published each time a chunk of the request body is written to the socket.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].
  * `chunk` {Uint8Array|string} The body chunk being sent.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:bodyChunkSent').subscribe(({ request, chunk }) => {
  console.log('sent', chunk.length, 'bytes')
})
```

## Event: `'undici:request:bodySent'`

<!-- YAML
added: v6.3.0
-->

Published after the request body has been fully sent.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:bodySent').subscribe(({ request }) => {
  // request is the same object as in 'undici:request:create'
})
```

## Event: `'undici:request:headers'`

<!-- YAML
added: v6.3.0
-->

Published after the response headers have been received.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].
  * `response` {Object} The response being received.
    * `statusCode` {number} The HTTP status code.
    * `statusText` {string} The HTTP status message.
    * `headers` {Buffer[]} The raw response headers as an array of buffers,
      alternating between header name and value.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:headers').subscribe(({ request, response }) => {
  console.log('statusCode', response.statusCode)
  console.log(response.statusText)
  console.log(response.headers.map((x) => x.toString()))
})
```

## Event: `'undici:request:bodyChunkReceived'`

<!-- YAML
added: v7.11.0
-->

Published each time a chunk of the response body is received.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].
  * `chunk` {Buffer} The body chunk that was received.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:bodyChunkReceived').subscribe(({ request, chunk }) => {
  console.log('received', chunk.length, 'bytes')
})
```

## Event: `'undici:request:trailers'`

<!-- YAML
added: v6.3.0
-->

Published after the response body and trailers have been received, that is, once
the response has fully completed.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][]. `request.completed` is now `true`.
  * `trailers` {Buffer[]} The raw response trailers as an array of buffers,
    alternating between header name and value.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:trailers').subscribe(({ request, trailers }) => {
  console.log('completed', request.completed)
  console.log(trailers.map((x) => x.toString()))
})
```

## Event: `'undici:request:error'`

<!-- YAML
added: v6.3.0
-->

Published when the request is about to error, before the error is surfaced to
the caller.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].
  * `error` {Error} The error the request is about to fail with.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:error').subscribe(({ request, error }) => {
  console.error(error)
})
```

## Event: `'undici:client:sendHeaders'`

<!-- YAML
added: v6.3.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/undici/pull/3585
    description: Refactor the diagnostics channel internals.
-->

Published immediately before the first byte of the request is written to the
socket. The headers are published exactly as they will be sent to the server, in
raw form.

* `message` {Object}
  * `request` {Object} The same object published by
    [`'undici:request:create'`][].
  * `headers` {string} The request headers as a single raw string, with lines
    separated by `\r\n`.
  * `socket` {net.Socket} The socket the request is written to.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:client:sendHeaders').subscribe(({ request, headers, socket }) => {
  console.log(`Full headers list: ${headers.split('\r\n')}`)
})
```

## Event: `'undici:client:beforeConnect'`

<!-- YAML
added: v6.3.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/undici/pull/3585
    description: Refactor the diagnostics channel internals.
-->

Published before a new connection is opened for any request. This event cannot be
attributed to a specific request, because connections are pooled and shared.

* `message` {Object}
  * `connectParams` {Object} The parameters used to open the connection.
    * `host` {string} The connection host, including the port if present.
    * `hostname` {string} The connection hostname.
    * `protocol` {string} The protocol, e.g. `'https:'`.
    * `port` {string} The connection port.
    * `servername` {string|null} The TLS server name, or `null`.
  * `connector` {Function} The function that creates the socket.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:client:beforeConnect').subscribe(({ connectParams, connector }) => {
  const { host, hostname, protocol, port, servername } = connectParams
  // connector is the function that creates the socket
})
```

## Event: `'undici:client:connected'`

<!-- YAML
added: v6.3.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/undici/pull/3585
    description: Refactor the diagnostics channel internals.
-->

Published after a connection has been successfully established.

* `message` {Object}
  * `socket` {net.Socket} The newly connected socket.
  * `connectParams` {Object} The parameters used to open the connection. See
    [`'undici:client:beforeConnect'`][] for the field descriptions.
  * `connector` {Function} The function that created the socket.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:client:connected').subscribe(({ socket, connectParams, connector }) => {
  const { host, hostname, protocol, port, servername } = connectParams
})
```

## Event: `'undici:client:connectError'`

<!-- YAML
added: v6.3.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/undici/pull/3585
    description: Refactor the diagnostics channel internals.
-->

Published when a connection could not be established.

* `message` {Object}
  * `error` {Error} The error that prevented the connection.
  * `socket` {net.Socket} The socket associated with the failed attempt.
  * `connectParams` {Object} The parameters used to open the connection. See
    [`'undici:client:beforeConnect'`][] for the field descriptions.
  * `connector` {Function} The function that created the socket.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:client:connectError').subscribe(({ error, socket, connectParams, connector }) => {
  console.error(`Connect failed with ${error.message}`)
})
```

## Event: `'undici:websocket:open'`

<!-- YAML
added: v6.3.0
-->

Published after a [`WebSocket`][] has successfully connected to a server.

* `message` {Object}
  * `address` {Object|null} The remote socket address, as returned by
    [`socket.address()`][], or `null` if it cannot be determined. When present,
    it contains `address`, `family`, and `port`.
  * `protocol` {string} The negotiated subprotocol.
  * `extensions` {string} The negotiated extensions.
  * `websocket` {WebSocket} The `WebSocket` instance.
  * `handshakeResponse` {Object} The HTTP response that upgraded the connection.
    * `status` {number} The HTTP status code: `101` for an HTTP/1.1 upgrade, or
      `200` for an HTTP/2 extended `CONNECT`.
    * `statusText` {string} The HTTP status message: `'Switching Protocols'` for
      HTTP/1.1, and commonly `'OK'` for HTTP/2.
    * `headers` {Object} The response headers from the server. The `upgrade` and
      `connection` headers are only present for HTTP/1.1 handshakes.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:websocket:open').subscribe(({
  address,
  protocol,
  extensions,
  websocket,
  handshakeResponse
}) => {
  console.log(address)
  console.log(protocol)
  console.log(extensions)
  console.log(handshakeResponse.status)
  console.log(handshakeResponse.headers)
})
```

## Event: `'undici:websocket:close'`

<!-- YAML
added: v6.3.0
-->

Published after the [`WebSocket`][] connection has closed.

* `message` {Object}
  * `websocket` {WebSocket} The `WebSocket` instance.
  * `code` {number} The closing status code.
  * `reason` {string} The closing reason.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:websocket:close').subscribe(({ websocket, code, reason }) => {
  console.log(code, reason)
})
```

## Event: `'undici:websocket:socket_error'`

<!-- YAML
added: v6.3.0
-->

Published when the underlying WebSocket socket experiences an error. Unlike the
other WebSocket channels, the published message is the error itself.

* `error` {Error} The socket error.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:websocket:socket_error').subscribe((error) => {
  console.error(error)
})
```

## Event: `'undici:websocket:ping'`

<!-- YAML
added: v6.3.0
-->

Published after the client receives a ping frame.

* `message` {Object}
  * `payload` {Buffer|undefined} The optional application data carried by the
    frame.
  * `websocket` {WebSocket} The `WebSocket` instance.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:websocket:ping').subscribe(({ payload, websocket }) => {
  console.log(payload)
})
```

## Event: `'undici:websocket:pong'`

<!-- YAML
added: v6.3.0
-->

Published after the client receives a pong frame.

* `message` {Object}
  * `payload` {Buffer|undefined} The optional application data carried by the
    frame.
  * `websocket` {WebSocket} The `WebSocket` instance.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:websocket:pong').subscribe(({ payload, websocket }) => {
  console.log(payload)
})
```

## Event: `'undici:proxy:connected'`

<!-- YAML
added: v7.17.0
-->

Published after a [`ProxyAgent`][] has established a tunnel to the proxy server.

* `message` {Object}
  * `socket` {net.Socket} The socket connected to the proxy.
  * `connectParams` {Object} The parameters used to establish the tunnel.
    * `origin` {string|URL} The proxy origin.
    * `port` {string} The proxy port.
    * `path` {string} The requested tunnel target, e.g. `'example.com:443'`.
    * `signal` {AbortSignal|undefined} The signal associated with the request.
    * `headers` {Object} The headers sent in the `CONNECT` request.
    * `servername` {string|undefined} The TLS server name used for the proxy.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:proxy:connected').subscribe(({ socket, connectParams }) => {
  const { origin, port, path, signal, headers, servername } = connectParams
})
```

## Event: `'undici:request:pending-requests'`

<!-- YAML
added: v6.3.0
-->

Published when the [`dedupe`][] interceptor's set of pending requests changes.
The interceptor coalesces concurrent identical requests so that only one reaches
the origin; this channel exposes that bookkeeping for monitoring and debugging.

* `message` {Object}
  * `type` {string} Either `'added'` when a new pending request is registered, or
    `'removed'` when a pending request completes, whether successfully or with an
    error.
  * `size` {number} The number of pending requests after the change.
  * `key` {string} The deduplication key for the request, composed of the origin,
    method, path, and request headers.

```mjs
import diagnosticsChannel from 'node:diagnostics_channel'

diagnosticsChannel.channel('undici:request:pending-requests').subscribe(({ type, size, key }) => {
  if (type === 'added') {
    console.log(`New pending request: ${key} (${size} total pending)`)
  } else {
    console.log(`Request completed: ${key} (${size} remaining)`)
  }
})
```

This is useful for verifying that deduplication is working, monitoring the number
of concurrent in-flight requests, and debugging deduplication behaviour in
production.

[`'undici:client:beforeConnect'`]: #event-undiciclientbeforeconnect
[`'undici:request:create'`]: #event-undicirequestcreate
[`'undici:request:headers'`]: #event-undicirequestheaders
[`ProxyAgent`]: ProxyAgent.md#class-proxyagent
[`WebSocket`]: WebSocket.md#class-websocket
[`dedupe`]: Dispatcher.md#dedupe
[`diagnostics_channel`]: https://nodejs.org/api/diagnostics_channel.html
[`socket.address()`]: https://nodejs.org/api/net.html#socketaddress
[Debug]: Debug.md
