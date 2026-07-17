# WebSocket

<!--introduced_in=v5.15.0-->
<!--type=module-->
<!-- source_link=lib/web/websocket/websocket.js -->

> Stability: 2 - Stable

The `WebSocket` API provides a way to manage a WebSocket connection to a
server, allowing bidirectional communication. It follows the
[WHATWG WebSocket specification][] and [RFC 6455][], and is intended to be
interchangeable with the global `WebSocket` available in browsers.

```mjs
import { WebSocket } from 'undici'

const ws = new WebSocket('wss://echo.websocket.events')

ws.addEventListener('open', () => {
  ws.send('Hello, world!')
})

ws.addEventListener('message', (event) => {
  console.log('received:', event.data)
})
```

In addition to the standard `WebSocket` interface, this module exports the
streams-based [`WebSocketStream`][] and [`WebSocketError`][] APIs, as well as a
[`ping()`][] helper for sending ping frames.

## Class: `WebSocket`

<!-- YAML
added: v5.15.0
-->

* Extends: {EventTarget}

The `WebSocket` object represents a single WebSocket connection. Constructing a
`WebSocket` immediately begins the opening handshake; the connection cannot be
reused once it has been closed.

### `new WebSocket(url[, protocols])`

<!-- YAML
added: v5.15.0
-->

* `url` {URL|string} The URL of the server to connect to. Must use the `ws:`
  or `wss:` scheme.
* `protocols` {string|string[]|WebSocketInit} A single subprotocol, a list of
  subprotocols to negotiate with the server, or a `WebSocketInit` object.
  **Default:** `[]`.
  * `protocols` {string|string[]} Subprotocol(s) to request the server use.
    **Default:** `[]`.
  * `dispatcher` {Dispatcher} The dispatcher used to open the connection.
    **Default:** the global dispatcher.
  * `headers` {HeadersInit|null} Additional headers to send with the handshake
    request. (optional)

Creates a new `WebSocket` and begins connecting to `url`. The second argument
may be a string or array of subprotocols, or a `WebSocketInit` object. The
object form is an undici extension and is not available in browsers; it allows a
custom `dispatcher` and additional `headers` to be supplied.

If a value in `protocols` is repeated or is not a valid subprotocol name, the
constructor throws a `SyntaxError` `DOMException`.

```mjs
import { WebSocket } from 'undici'

const ws = new WebSocket('wss://echo.websocket.events', ['echo', 'chat'])
```

To route the connection through a custom dispatcher, such as a
[`ProxyAgent`][], pass a `WebSocketInit` object:

```mjs
import { WebSocket, ProxyAgent } from 'undici'

const dispatcher = new ProxyAgent('http://localhost:8080')

const ws = new WebSocket('wss://echo.websocket.events', {
  dispatcher,
  protocols: ['echo', 'chat']
})
```

WebSocket over HTTP/2 is experimental and may change in the future. It can be
enabled by passing a dispatcher configured with `allowH2`:

```mjs
import { WebSocket, Agent } from 'undici'

const dispatcher = new Agent({ allowH2: true })

const ws = new WebSocket('wss://echo.websocket.events', {
  dispatcher,
  protocols: ['echo', 'chat']
})
```

### `websocket.close([code[, reason]])`

<!-- YAML
added: v5.15.0
-->

* `code` {number} The status code explaining why the connection is being
  closed. **Default:** `null`.
* `reason` {string} A human-readable string explaining why the connection is
  being closed. **Default:** `''`.

Starts the closing handshake. The `code` is clamped to an unsigned 16-bit
integer; when omitted, no status code is sent. The `reason` is encoded as
UTF-8 and must not exceed 123 bytes.

```mjs
ws.close(1000, 'Done')
```

### `websocket.send(data)`

<!-- YAML
added: v5.15.0
-->

* `data` {string|ArrayBuffer|TypedArray|DataView|Blob} The data to enqueue for
  transmission.

Enqueues `data` to be transmitted to the server. String data is sent as a text
frame; `ArrayBuffer`, any `TypedArray`, `DataView`, and `Blob` values are sent
as binary frames. The number of bytes queued is added to
[`websocket.bufferedAmount`][].

If [`websocket.readyState`][] is `CONNECTING`, an `InvalidStateError`
`DOMException` is thrown. If the connection is closing or already closed, the
call is silently ignored.

```mjs
ws.send('a text message')
ws.send(new Uint8Array([1, 2, 3]))
```

### `websocket.readyState`

<!-- YAML
added: v5.15.0
-->

* Type: {number}

The current state of the connection. One of the numeric constants
`WebSocket.CONNECTING` (`0`), `WebSocket.OPEN` (`1`),
`WebSocket.CLOSING` (`2`), or `WebSocket.CLOSED` (`3`). This property is
read-only.

### `websocket.bufferedAmount`

<!-- YAML
added: v5.15.0
-->

* Type: {number}

The number of bytes of data that have been queued using
[`websocket.send()`][] but not yet transmitted to the network. This value
resets to `0` once all queued data is sent. This property is read-only.

### `websocket.url`

<!-- YAML
added: v5.15.0
-->

* Type: {string}

The absolute URL of the WebSocket as resolved during construction. This
property is read-only.

### `websocket.extensions`

<!-- YAML
added: v5.15.0
-->

* Type: {string}

The extensions negotiated by the server, taken from the
`Sec-WebSocket-Extensions` response header. Empty until the connection is
established. This property is read-only.

### `websocket.protocol`

<!-- YAML
added: v5.15.0
-->

* Type: {string}

The subprotocol selected by the server, taken from the
`Sec-WebSocket-Protocol` response header. Empty until the connection is
established and only set when a subprotocol was negotiated. This property is
read-only.

### `websocket.binaryType`

<!-- YAML
added: v5.15.0
-->

* Type: {string} **Default:** `'blob'`.

Controls the type used to represent binary data delivered by the
[`'message'`][] event. Must be either `'blob'` (binary frames are exposed as a
`Blob`) or `'arraybuffer'` (binary frames are exposed as an `ArrayBuffer`).
Assigning any other value resets it to `'blob'`.

### `websocket.onopen`

<!-- YAML
added: v5.15.0
-->

* Type: {Function|null} **Default:** `null`.

An event handler for the [`'open'`][] event. Setting this property replaces any
previously registered `onopen` handler.

### `websocket.onerror`

<!-- YAML
added: v5.15.0
-->

* Type: {Function|null} **Default:** `null`.

An event handler for the [`'error'`][] event. Setting this property replaces any
previously registered `onerror` handler.

### `websocket.onclose`

<!-- YAML
added: v5.15.0
-->

* Type: {Function|null} **Default:** `null`.

An event handler for the [`'close'`][] event. Setting this property replaces any
previously registered `onclose` handler.

### `websocket.onmessage`

<!-- YAML
added: v5.15.0
-->

* Type: {Function|null} **Default:** `null`.

An event handler for the [`'message'`][] event. Setting this property replaces
any previously registered `onmessage` handler.

### Event: `'open'`

<!-- YAML
added: v5.21.0
-->

Emitted when the WebSocket connection is established. After this event,
[`websocket.readyState`][] is `OPEN` and data may be sent with
[`websocket.send()`][]. The listener receives an `Event`.

### Event: `'message'`

<!-- YAML
added: v7.0.0
-->

Emitted when a message is received from the server. The listener receives a
`MessageEvent` whose `data` property is a `string` for text frames, and either a
`Blob` or an `ArrayBuffer` for binary frames depending on
[`websocket.binaryType`][]. The event's `origin` is the origin of
[`websocket.url`][].

### Event: `'error'`

<!-- YAML
added: v7.0.0
-->

Emitted when the connection is terminated abnormally, for example when no close
frame was received before the underlying transport was lost. The listener
receives an `ErrorEvent`.

### Event: `'close'`

<!-- YAML
added: v7.0.0
-->

Emitted when the connection is closed. The listener receives a `CloseEvent`
whose `code` and `reason` describe why the connection closed, and whose
`wasClean` property is `true` only when the closing handshake completed in both
directions.

## `ping(websocket[, payload])`

<!-- YAML
added: v7.12.0
-->

* `websocket` {WebSocket} The WebSocket to send the ping frame on.
* `payload` {Buffer} Optional payload to include in the ping frame. Must not
  exceed 125 bytes. **Default:** `undefined`.

Sends a ping control frame to the server. A conforming server replies with a
pong frame containing the same payload, which can be used to keep the connection
alive or to verify that it is still active.

The frame is only sent when the connection is open; if it is connecting,
closing, or closed, the call is a no-op. A `TypeError` is thrown if `payload`
is not a `Buffer` or is larger than 125 bytes.

```mjs
import { WebSocket, ping } from 'undici'

const ws = new WebSocket('wss://echo.websocket.events')

ws.addEventListener('open', () => {
  ping(ws)
  ping(ws, Buffer.from('hello'))
})
```

## Class: `WebSocketStream`

<!-- YAML
added: v7.0.0
-->

> Stability: 1 - Experimental

`WebSocketStream` is a streams-based alternative to the event-based
[`WebSocket`][] interface, exposing a connection as a pair of `ReadableStream`
and `WritableStream` objects. See [MDN][MDN WebSocketStream] for background.

Constructing a `WebSocketStream` for the first time emits an
`ExperimentalWarning` with the code `UNDICI-WSS`.

### `new WebSocketStream(url[, options])`

<!-- YAML
added: v7.0.0
-->

> Stability: 1 - Experimental

* `url` {URL|string} The URL of the server to connect to.
* `options` {WebSocketStreamOptions}
  * `protocols` {string|string[]} Subprotocol(s) to request the server use.
    **Default:** `[]`.
  * `signal` {AbortSignal|null} An `AbortSignal` that aborts the opening
    handshake and rejects [`webSocketStream.opened`][] and
    [`webSocketStream.closed`][] when triggered. **Default:** `null`.

Creates a new `WebSocketStream` and begins connecting to `url`. As with
[`new WebSocket()`][], a repeated or invalid subprotocol causes a `SyntaxError`
`DOMException` to be thrown.

```mjs
import { WebSocketStream } from 'undici'

const stream = new WebSocketStream('wss://echo.websocket.events')
const { readable, writable } = await stream.opened

const writer = writable.getWriter()
await writer.write('Hello, world!')
writer.releaseLock()

const reader = readable.getReader()
const { value } = await reader.read()
console.log('received:', value)
```

### `webSocketStream.url`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* Type: {string}

The serialized URL of the WebSocket. This property is read-only.

### `webSocketStream.opened`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* Type: {Promise} Fulfills once the connection is established.

A promise that resolves with an object describing the open connection, or
rejects if the connection could not be established. The fulfillment value has
the following shape:

* `extensions` {string} The negotiated extensions.
* `protocol` {string} The negotiated subprotocol.
* `readable` {ReadableStream} A stream of incoming messages. Text frames are
  delivered as strings and binary frames as `Uint8Array` chunks.
* `writable` {WritableStream} A stream for sending messages. Writing a string
  sends a text frame; writing a `BufferSource` sends a binary frame.

### `webSocketStream.closed`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* Type: {Promise} Fulfills once the connection is closed cleanly.

A promise that resolves with a `WebSocketCloseInfo` object when the connection
closes cleanly, or rejects with a [`WebSocketError`][] when it closes
abnormally. The fulfillment value has the following shape:

* `closeCode` {number} The status code the connection was closed with.
* `reason` {string} The reason the connection was closed with.

### `webSocketStream.close([closeInfo])`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* `closeInfo` {Object}
  * `closeCode` {number} The status code to close with.
  * `reason` {string} The reason to close with. **Default:** `''`.

Starts the closing handshake. When `closeInfo` is omitted, the connection is
closed without a status code.

## Class: `WebSocketError`

<!-- YAML
added: v7.0.0
-->

> Stability: 1 - Experimental

* Extends: {DOMException}

An error type used by [`WebSocketStream`][] to report close information. It is
used to reject [`webSocketStream.closed`][] when the connection closes
abnormally, and may be passed to a stream's abort or cancel operations to close
the connection with a specific code and reason.

### `new WebSocketError([message[, init]])`

<!-- YAML
added: v7.0.0
-->

> Stability: 1 - Experimental

* `message` {string} A human-readable description of the error.
  **Default:** `''`.
* `init` {Object}
  * `closeCode` {number} The status code to associate with the error.
  * `reason` {string} The reason to associate with the error.
    **Default:** `''`.

Creates a new `WebSocketError`. If `reason` is non-empty but no `closeCode` is
given, `closeCode` defaults to `1000` ("Normal Closure"). The `closeCode` and
`reason` are validated, so an out-of-range code or an over-long reason throws.

### `webSocketError.closeCode`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* Type: {number|null}

The status code associated with the error, or `null` if none was set. This
property is read-only.

### `webSocketError.reason`

<!-- YAML
added: v5.15.0
-->

> Stability: 1 - Experimental

* Type: {string}

The reason associated with the error. This property is read-only.

## Read more

* [MDN: WebSocket][]
* [RFC 6455][]
* [WHATWG WebSocket specification][]

[MDN WebSocketStream]: https://developer.mozilla.org/docs/Web/API/WebSocketStream
[MDN: WebSocket]: https://developer.mozilla.org/docs/Web/API/WebSocket
[RFC 6455]: https://www.rfc-editor.org/rfc/rfc6455
[WHATWG WebSocket specification]: https://websockets.spec.whatwg.org/
[`'close'`]: #event-close
[`'error'`]: #event-error
[`'message'`]: #event-message
[`'open'`]: #event-open
[`ProxyAgent`]: ProxyAgent.md#class-proxyagent
[`WebSocket`]: #class-websocket
[`WebSocketError`]: #class-websocketerror
[`WebSocketStream`]: #class-websocketstream
[`new WebSocket()`]: #new-websocketurl-protocols
[`ping()`]: #pingwebsocket-payload
[`webSocketStream.closed`]: #websocketstreamclosed
[`webSocketStream.opened`]: #websocketstreamopened
[`websocket.bufferedAmount`]: #websocketbufferedamount
[`websocket.binaryType`]: #websocketbinarytype
[`websocket.readyState`]: #websocketreadystate
[`websocket.send()`]: #websocketsenddata
[`websocket.url`]: #websocketurl
