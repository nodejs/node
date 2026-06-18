# HTTP/3

<!-- introduced_in=REPLACEME -->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early development

<!-- source_link=lib/http3.js -->

The `node:http3` module provides an implementation of the HTTP/3 protocol on
top of the QUIC transport in [`node:quic`][]. To access it, start Node.js with
the `--experimental-quic` option and:

```mjs
import http3 from 'node:http3';
```

```cjs
const http3 = require('node:http3');
```

The module is only available under the `node:` scheme.

## Overview

`node:http3` allows you to send & receive HTTP/3 requests and responses. It
builds on top of the QUIC transport from [`node:quic`][].

Each connection is an {Http3Session}, wrapping a [`QuicSession`][], and
each request/response exchange is an {Http3Stream}, wrapping a
[`QuicStream`][].

Servers call [`listen()`][] to create an endpoint, from which sessions will
be emitted for incoming connections. Clients call [`connect()`][] to create
a session with a remote server.

Transport-level configuration (TLS certificates, SNI, address validation,
flow control, and so on) is identical to `node:quic`. QUIC options not
managed by `node:http3` can be passed straight through to the underlying
endpoint, session, or stream.

HTTP/3 requests are always client-initiated. A server session receives request
streams via its `onstream` callback and cannot itself call `request()`.

### Example

Server:

```mjs
import { listen } from 'node:http3';

const endpoint = await listen((session) => {
  session.onstream = (stream) => {
    stream.onheaders = (headers) => {
      // headers[':method'], headers[':path'], headers[':authority'] ...
      stream.sendHeaders({ ':status': '200', 'content-type': 'text/plain' });
      const w = stream.writer;
      w.writeSync('hello h3');
      w.endSync();
    };
  };
}, {
  sni: { '*': { keys: [key], certs: [cert] } },
});
```

Client:

```mjs
import { connect } from 'node:http3';
import { bytes } from 'stream/iter';

const session = await connect(endpoint.address, { servername: 'localhost' });
await session.opened;

const stream = await session.request({
  ':method': 'GET',
  ':path': '/hello',
  ':scheme': 'https',
  ':authority': 'localhost',
}, {
  onheaders: (headers) => { /* headers[':status'] === '200' */ },
});

const body = new TextDecoder().decode(await bytes(stream));
// body === 'hello h3'

await session.close();
```

## `http3.connect(address[, options])`

<!-- YAML
added: REPLACEME
-->

* `address` {string|net.SocketAddress} The server address. See [`quic.connect()`][].
* `options` {Object} HTTP/3 options. Other [`quic.connect()`][] session options
  may also be provided and will be passed through to the underlying QUIC session.
  * `settings` {Http3Settings} Local HTTP/3 SETTINGS to advertise to the peer.
  * `ongoaway` {http3.OnGoawayCallback} The session's initial `ongoaway` callback.
  * `onorigin` {http3.OnOriginCallback} The session's initial `onorigin` callback.
  * `onsettings` {http3.OnSettingsCallback} The session's initial `onsettings` callback.
* Returns: {Promise} Fulfilled with an {Http3Session}.

Opens a client connection to an HTTP/3 server. The ALPN identifier is fixed to
`h3`. Await [`session.opened`][] before sending requests: the TLS handshake —
including certificate validation — completes asynchronously, and any handshake
or connection failure is reported through `opened` rather than by `connect()`.

## `http3.listen(onsession[, options])`

<!-- YAML
added: REPLACEME
-->

* `onsession` {http3.OnSessionCallback} Invoked with each new {Http3Session}.
* `options` {Object} The same HTTP/3 options as [`connect()`][], plus any
  [`quic.listen()`][] option (for example `sni`), passed through to the
  endpoint.
* Returns: {Promise} Fulfilled with the listening {quic.QuicEndpoint}.

Creates a server endpoint that negotiates the `h3` ALPN identifier and invokes
`onsession` for each incoming connection.

## Class: `Http3Session`

<!-- YAML
added: REPLACEME
-->

An `Http3Session` wraps a {quic.QuicSession} with HTTP/3 semantics. Servers receive
one through the [`listen()`][] callback; clients create one with [`connect()`][].
Alternatively, you can call the constructor directly on a QuicSession if it's not
already active.

### `new Http3Session(session[, callbacks])`

<!-- YAML
added: REPLACEME
-->

* `session` {quic.QuicSession} The raw transport session to wrap.
* `callbacks` {Object}
  * `ongoaway` {http3.OnGoawayCallback} The initial `ongoaway` callback.
  * `onorigin` {http3.OnOriginCallback} The initial `onorigin` callback.
  * `onsettings` {http3.OnSettingsCallback} The initial `onsettings` callback.

Attaches HTTP/3 to an existing `node:quic` session.

The HTTP/3 session must be attached before the QUIC session becomes active and
begins emitting events: for servers that means synchronously inside a server's
`onsession` callback, or for clients before the handshake completes. Throws
`ERR_INVALID_STATE` if the QUIC session is already attached or active.

### `session.request(headers[, options])`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object} The request pseudo-headers (`:method`, `:path`,
  `:scheme`, `:authority`) and regular headers. When omitted, headers can be
  sent later with `sendHeaders()`.
* `options` {Object} Any {quic.QuicStream} option (for example `body`) may also be
  given and is passed through.
  * `priority` {string} Initial priority level: `'high'`, `'default'`, or
    `'low'`. **Default:** `'default'`.
  * `incremental` {boolean} Whether the stream may be served incrementally.
    **Default:** `false`.
  * `onheaders` {http3.OnHeadersCallback} The new stream's `onheaders` callback.
  * `oninfo` {http3.OnInfoCallback} The new stream's `oninfo` callback.
  * `ontrailers` {http3.OnTrailersCallback} The new stream's `ontrailers` callback.
  * `onwanttrailers` {http3.OnWantTrailersCallback} The new stream's `onwanttrailers`
    callback.
  * `onreset` {http3.OnResetCallback} The new stream's `onreset` callback.
  * `onerror` {http3.OnErrorCallback} The new stream's `onerror` callback.
* Returns: {Promise} Fulfilled with an {Http3Stream}.

Opens a request stream. Client only — throws `ERR_INVALID_STATE` on a server
session. The per-stream callbacks are passed here so they are attached before
any event can be delivered on the new stream.

### `session.close([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
* Returns: {Promise}

Gracefully closes the session, delegating to the underlying QUIC session.

### `session.destroy([error[, options]])`

<!-- YAML
added: REPLACEME
-->

* `error` {Error}
* `options` {Object}

Immediately destroys the session and all of its streams.

### `session.settings`

<!-- YAML
added: REPLACEME
-->

* Type: {Http3Settings|null}

The effective HTTP/3 settings for the session: the locally configured values
plus any updates received in the peer's SETTINGS frame. Returns `null` once the
session is destroyed. Read only.

### `session.opened`

<!-- YAML
added: REPLACEME
-->

* Type: {Promise}

Resolves with the handshake details once the TLS handshake completes, or
rejects if the session fails to open. Read only.

### `session.closed`

<!-- YAML
added: REPLACEME
-->

* Type: {Promise}

Resolves when the session has fully closed, or rejects if it ends with an
error. Read only.

### `session.servername`

<!-- YAML
added: REPLACEME
-->

* Type: {string|undefined}

The SNI servername of the session, or `undefined` if none was negotiated.
Read only.

### `session.alpnProtocol`

<!-- YAML
added: REPLACEME
-->

* Type: {string|undefined}

The negotiated ALPN protocol (`'h3'`), or `undefined` before negotiation
completes. Read only.

### `session.session`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.QuicSession}

The underlying transport session. Read only.

### `session.onstream`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnStreamCallback}

Server callback invoked with each incoming request stream. Read/write.

### `session.ongoaway`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnGoawayCallback}

Invoked when the peer sends an HTTP/3 GOAWAY frame. Read/write.

### `session.onorigin`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnOriginCallback}

Invoked when the peer sends an HTTP/3 ORIGIN frame (RFC 9412). Read/write.

### `session.onsettings`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnSettingsCallback}

Invoked when the peer's SETTINGS frame is received, which may be after the
session opens. The current values are also available synchronously via
`session.settings`. Read/write.

### `session.onerror`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnErrorCallback}

Invoked when the session encounters an error. Read/write.

## Class: `Http3Stream`

<!-- YAML
added: REPLACEME
-->

An `Http3Stream` represents a single HTTP/3 request/response exchange, wrapping
a {quic.QuicStream}. It is async-iterable: `for await (const chunk of stream)` (or
the `stream/iter` `bytes()` helper) reads the inbound body.

### `stream.sendHeaders(headers[, options])`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object} The header block to send — a server's response headers or
  a client's request headers.
* `options` {Object}
  * `terminal` {boolean} When `true`, the header block is the final frame and
    no body follows. **Default:** `false`.
* Returns: {boolean} `true` if the headers were scheduled to be sent.

Sends the initial request or response header block.

### `stream.sendInformationalHeaders(headers)`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}
* Returns: {boolean} `true` if the headers were scheduled to be sent.

Sends an informational (1xx) response header block. Server only.

### `stream.sendTrailers(headers)`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}
* Returns: {boolean} `true` if the trailers were scheduled to be sent.

Sends a trailing header block. Must be called synchronously from the
`onwanttrailers` callback.

### `stream.setPriority([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string} `'high'`, `'default'`, or `'low'`. **Default:**
    `'default'`.
  * `incremental` {boolean} Whether the stream may be served incrementally.
    **Default:** `false`.

Updates the stream priority. Before the request is submitted the priority is
carried in the initial header block; afterwards it is signaled with a
PRIORITY\_UPDATE frame.

### `stream.stopSending([code])`

<!-- YAML
added: REPLACEME
-->

* `code` {bigint|number} The application error code sent to the peer.
  **Default:** `0n`.

Asks the peer to stop sending data on this stream, delegating to the underlying
QUIC stream.

### `stream.resetStream([code])`

<!-- YAML
added: REPLACEME
-->

* `code` {bigint|number} The application error code sent to the peer.
  **Default:** `0n`.

Tells the peer this end will send no more data on this stream, delegating to
the underlying QUIC stream.

### `stream.destroy([error[, options]])`

<!-- YAML
added: REPLACEME
-->

* `error` {Error}
* `options` {Object}

Immediately destroys the stream.

### `stream[Symbol.asyncIterator]()`

<!-- YAML
added: REPLACEME
-->

Iterates the inbound body bytes, ending at the stream FIN. Equivalent to
iterating the underlying [`QuicStream`][].

### `stream.writer`

<!-- YAML
added: REPLACEME
-->

* Type: {QuicStreamWriter}

The outbound body writer (`write`, `writeSync`, `end`, `endSync`). Read only.

### `stream.headers`

<!-- YAML
added: REPLACEME
-->

* Type: {Object}

The most recently received header block, or `undefined` before any headers
arrive. Read only.

### `stream.priority`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|null}

The stream's priority as `{ level, incremental }` — on a client the requested
value, on a server the peer's requested priority. `null` once the stream is
destroyed. Read only.

### `stream.id`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint}

The QUIC stream id. Read only.

### `stream.direction`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

`'bidi'` or `'uni'`. Read only.

### `stream.early`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the stream's data arrived as 0-RTT early data. Read only.

### `stream.closed`

<!-- YAML
added: REPLACEME
-->

* Type: {Promise}

Resolves when the stream has closed. Read only.

### `stream.destroyed`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` once the stream has been destroyed. Read only.

### `stream.session`

<!-- YAML
added: REPLACEME
-->

* Type: {Http3Session}

The session that owns this stream. Read only.

### `stream.onheaders`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnHeadersCallback}

Invoked when the initial response/request header block is received. Read/write.

### `stream.oninfo`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnInfoCallback}

Invoked when an informational (1xx) header block is received. Read/write.

### `stream.ontrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnTrailersCallback}

Invoked when a trailing header block is received. Read/write.

### `stream.onwanttrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnWantTrailersCallback}

Invoked when trailers may be sent. Setting it keeps the stream open after the
body; send trailers synchronously with `sendTrailers()`. Read/write.

### `stream.onreset`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnResetCallback}

Invoked when the peer aborts the stream with a `RESET_STREAM` frame. Read/write.

### `stream.onerror`

<!-- YAML
added: REPLACEME
-->

* Type: {http3.OnErrorCallback}

Invoked when the stream encounters an error. Read/write.

## Types

### Type: `Http3Settings`

<!-- YAML
added: REPLACEME
-->

* `maxFieldSectionSize` {number} Maximum size, in bytes, of a header field
  section the endpoint will accept.
* `qpackMaxDtableCapacity` {number} Maximum QPACK dynamic table capacity.
  **Default:** `4096`.
* `qpackEncoderMaxDtableCapacity` {number} Maximum QPACK encoder dynamic table
  capacity. **Default:** `4096`.
* `qpackBlockedStreams` {number} Maximum number of streams that may be blocked
  waiting for QPACK state. **Default:** `100`.
* `enableConnectProtocol` {boolean} Whether the Extended CONNECT protocol
  (RFC 9220) is enabled. **Default:** `true`.

The HTTP/3 SETTINGS for a session (RFC 9114). Advertised to the peer via the
`settings` option, and read back — merged with the peer's values — through
`session.settings`.

## Callbacks

### Callback: `OnSessionCallback`

<!-- YAML
added: REPLACEME
-->

* `session` {Http3Session}

Invoked by [`listen()`][] for each new server session.

### Callback: `OnStreamCallback`

<!-- YAML
added: REPLACEME
-->

* `stream` {Http3Stream}

Invoked with each incoming request stream (server side).

### Callback: `OnGoawayCallback`

<!-- YAML
added: REPLACEME
-->

* `lastStreamId` {bigint}

Invoked when the peer sends an HTTP/3 GOAWAY frame.

### Callback: `OnOriginCallback`

<!-- YAML
added: REPLACEME
-->

* `origins` {string\[]}

Invoked when the peer sends an HTTP/3 ORIGIN frame (RFC 9412).

### Callback: `OnSettingsCallback`

<!-- YAML
added: REPLACEME
-->

* `settings` {Http3Settings}

Invoked when the peer's SETTINGS frame is received.

### Callback: `OnHeadersCallback`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}

Invoked when a response or request header block is received.

### Callback: `OnInfoCallback`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}

Invoked when an informational (1xx) header block is received.

### Callback: `OnTrailersCallback`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}

Invoked when a trailing header block is received.

### Callback: `OnWantTrailersCallback`

<!-- YAML
added: REPLACEME
-->

Invoked with no arguments when trailers may be sent.

### Callback: `OnResetCallback`

<!-- YAML
added: REPLACEME
-->

* `error` {Error}

Invoked when the peer aborts the stream with a `RESET_STREAM` frame.

### Callback: `OnErrorCallback`

<!-- YAML
added: REPLACEME
-->

* `error` {Error}

Invoked when the session or stream encounters an error.

[`QuicSession`]: quic.md#class-quicsession
[`QuicStream`]: quic.md#class-quicstream
[`connect()`]: #http3connectaddress-options
[`listen()`]: #http3listenonsession-options
[`node:quic`]: quic.md
[`quic.connect()`]: quic.md#quicconnectaddress-options
[`quic.listen()`]: quic.md#quiclistenonsession-options
[`session.opened`]: #sessionopened
