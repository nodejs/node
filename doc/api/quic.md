# QUIC

<!-- introduced_in=v23.8.0-->

<!-- YAML
added: v23.8.0
-->

> Stability: 1.0 - Early development

<!-- source_link=lib/quic.js -->

The 'node:quic' module provides an implementation of the QUIC protocol.
To access it, start Node.js with the `--experimental-quic` option and:

```mjs
import quic from 'node:quic';
```

```cjs
const quic = require('node:quic');
```

The module is only available under the `node:` scheme.

## `quic.connect(address[, options])`

<!-- YAML
added: v23.8.0
-->

* `address` {string|net.SocketAddress}
* `options` {quic.SessionOptions}
* Returns: {Promise} a promise for a {quic.QuicSession}

Initiate a new client-side session.

```mjs
import { connect } from 'node:quic';
import { Buffer } from 'node:buffer';

const enc = new TextEncoder();
const alpn = 'foo';
const client = await connect('123.123.123.123:8888', { alpn });
await client.createUnidirectionalStream({
  body: enc.encode('hello world'),
});
```

By default, every call to `connect(...)` will create a new local
`QuicEndpoint` instance bound to a new random local IP port. To
specify the exact local address to use, or to multiplex multiple
QUIC sessions over a single local port, pass the `endpoint` option
with either a `QuicEndpoint` or `EndpointOptions` as the argument.

```mjs
import { QuicEndpoint, connect } from 'node:quic';

const endpoint = new QuicEndpoint({
  address: '127.0.0.1:1234',
});

const client = await connect('123.123.123.123:8888', { endpoint });
```

## `quic.listen(onsession,[options])`

<!-- YAML
added: v23.8.0
-->

* `onsession` {quic.OnSessionCallback}
* `options` {quic.SessionOptions}
* Returns: {Promise} a promise for a {quic.QuicEndpoint}

Configures the endpoint to listen as a server. When a new session is initiated by
a remote peer, the given `onsession` callback will be invoked with the created
session.

```mjs
import { listen } from 'node:quic';

const endpoint = await listen((session) => {
  // ... handle the session
});

// Closing the endpoint allows any sessions open when close is called
// to complete naturally while preventing new sessions from being
// initiated. Once all existing sessions have finished, the endpoint
// will be destroyed. The call returns a promise that is resolved once
// the endpoint is destroyed.
await endpoint.close();
```

By default, every call to `listen(...)` will create a new local
`QuicEndpoint` instance bound to a new random local IP port. To
specify the exact local address to use, or to multiplex multiple
QUIC sessions over a single local port, pass the `endpoint` option
with either a `QuicEndpoint` or `EndpointOptions` as the argument.

At most, any single `QuicEndpoint` can only be configured to listen as
a server once.

## Class: `QuicEndpoint`

A `QuicEndpoint` encapsulates the local UDP-port binding for QUIC. It can be
used as both a client and a server.

### `new QuicEndpoint([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {quic.EndpointOptions}

### `endpoint.address`

<!-- YAML
added: v23.8.0
-->

* Type: {net.SocketAddress|undefined}

The local UDP socket address to which the endpoint is bound, if any.

If the endpoint is not currently bound then the value will be `undefined`. Read only.

### `endpoint.busy`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

When `endpoint.busy` is set to true, the endpoint will temporarily reject
new sessions from being created. Read/write.

```mjs
// Mark the endpoint busy. New sessions will be prevented.
endpoint.busy = true;

// Mark the endpoint free. New session will be allowed.
endpoint.busy = false;
```

The `busy` property is useful when the endpoint is under heavy load and needs to
temporarily reject new sessions while it catches up.

### `endpoint.close()`

<!-- YAML
added: v23.8.0
-->

* Returns: {Promise}

Gracefully close the endpoint. The endpoint will close and destroy itself when
all currently open sessions close. Once called, new sessions will be rejected.

Returns a promise that is fulfilled when the endpoint is destroyed.

### `endpoint.closed`

<!-- YAML
added: v23.8.0
-->

* Type: {Promise}

A promise that is fulfilled when the endpoint is destroyed. This will be the same promise that is
returned by the `endpoint.close()` function. Read only.

### `endpoint.closing`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if `endpoint.close()` has been called and closing the endpoint has not yet completed.
Read only.

### `endpoint.destroy([error])`

<!-- YAML
added: v23.8.0
-->

* `error` {any}

Forcefully closes the endpoint by forcing all open sessions to be immediately
closed.

### `endpoint.destroyed`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if `endpoint.destroy()` has been called. Read only.

### `endpoint.setSNIContexts(entries[, options])`

<!-- YAML
added: REPLACEME
-->

* `entries` {object} An object mapping host names to TLS identity options.
  Each entry must include `keys` and `certs`.
* `options` {object}
  * `replace` {boolean} If `true`, replaces the entire SNI map. If `false`
    (the default), merges the entries into the existing map.

Replaces or updates the SNI TLS contexts for this endpoint. This allows
changing the TLS identity (key/certificate) used for specific host names
without restarting the endpoint. Existing sessions are unaffected — only
new sessions will use the updated contexts.

```mjs
endpoint.setSNIContexts({
  'api.example.com': { keys: [newApiKey], certs: [newApiCert] },
});

// Replace the entire SNI map
endpoint.setSNIContexts({
  'api.example.com': { keys: [newApiKey], certs: [newApiCert] },
}, { replace: true });
```

### `endpoint.stats`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.QuicEndpoint.Stats}

The statistics collected for an active session. Read only.

### `endpoint[Symbol.asyncDispose]()`

<!-- YAML
added: v23.8.0
-->

Calls `endpoint.close()` and returns a promise that fulfills when the
endpoint has closed.

## Class: `QuicEndpoint.Stats`

<!-- YAML
added: v23.8.0
-->

A view of the collected statistics for an endpoint.

### `endpointStats.createdAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} A timestamp indicating the moment the endpoint was created. Read only.

### `endpointStats.destroyedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} A timestamp indicating the moment the endpoint was destroyed. Read only.

### `endpointStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of bytes received by this endpoint. Read only.

### `endpointStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of bytes sent by this endpoint. Read only.

### `endpointStats.packetsReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of QUIC packets successfully received by this endpoint. Read only.

### `endpointStats.packetsSent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of QUIC packets successfully sent by this endpoint. Read only.

### `endpointStats.serverSessions`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of peer-initiated sessions received by this endpoint. Read only.

### `endpointStats.clientSessions`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of sessions initiated by this endpoint. Read only.

### `endpointStats.serverBusyCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of times an initial packet was rejected due to the
  endpoint being marked busy. Read only.

### `endpointStats.retryCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of QUIC retry attempts on this endpoint. Read only.

### `endpointStats.versionNegotiationCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number sessions rejected due to QUIC version mismatch. Read only.

### `endpointStats.statelessResetCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of stateless resets handled by this endpoint. Read only.

### `endpointStats.immediateCloseCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint} The total number of sessions that were closed before handshake completed. Read only.

## Class: `QuicSession`

<!-- YAML
added: v23.8.0
-->

A `QuicSession` represents the local side of a QUIC connection.

### `session.close()`

<!-- YAML
added: v23.8.0
-->

* Returns: {Promise}

Initiate a graceful close of the session. Existing streams will be allowed
to complete but no new streams will be opened. Once all streams have closed,
the session will be destroyed. The returned promise will be fulfilled once
the session has been destroyed.

### `session.opened`

<!-- YAML
added: REPLACEME
-->

* Type: {Promise} for an {Object}
  * `local` {net.SocketAddress} The local socket address.
  * `remote` {net.SocketAddress} The remote socket address.
  * `servername` {string} The SNI server name negotiated during the handshake.
  * `protocol` {string} The ALPN protocol negotiated during the handshake.
  * `cipher` {string} The name of the negotiated TLS cipher suite.
  * `cipherVersion` {string} The TLS protocol version of the cipher suite
    (e.g., `'TLSv1.3'`).
  * `validationErrorReason` {string} If certificate validation failed, the
    reason string. Empty string if validation succeeded.
  * `validationErrorCode` {number} If certificate validation failed, the
    error code. `0` if validation succeeded.
  * `earlyDataAttempted` {boolean} Whether 0-RTT early data was attempted.
  * `earlyDataAccepted` {boolean} Whether 0-RTT early data was accepted by
    the server.

A promise that is fulfilled once the TLS handshake completes successfully.
The resolved value contains information about the established session
including the negotiated protocol, cipher suite, certificate validation
status, and 0-RTT early data status.

If the handshake fails or the session is destroyed before the handshake
completes, the promise will be rejected.

### `session.closed`

<!-- YAML
added: v23.8.0
-->

* Type: {Promise}

A promise that is fulfilled once the session is destroyed.

### `session.destroy([error])`

<!-- YAML
added: v23.8.0
-->

* `error` {any}

Immediately destroy the session. All streams will be destroys and the
session will be closed.

### `session.destroyed`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if `session.destroy()` has been called. Read only.

### `session.endpoint`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.QuicEndpoint}

The endpoint that created this session. Read only.

### `session.onstream`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnStreamCallback}

The callback to invoke when a new stream is initiated by a remote peer. Read/write.

### `session.ondatagram`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnDatagramCallback}

The callback to invoke when a new datagram is received from a remote peer. Read/write.

### `session.ondatagramstatus`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnDatagramStatusCallback}

The callback to invoke when the status of a datagram is updated. Read/write.

### `session.onpathvalidation`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnPathValidationCallback}

The callback to invoke when the path validation is updated. Read/write.

### `session.onsessionticket`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnSessionTicketCallback}

The callback to invoke when a new session ticket is received. Read/write.

### `session.onversionnegotiation`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnVersionNegotiationCallback}

The callback to invoke when a version negotiation is initiated. Read/write.

### `session.onhandshake`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnHandshakeCallback}

The callback to invoke when the TLS handshake is completed. Read/write.

### `session.onnewtoken`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnNewTokenCallback}

The callback to invoke when a NEW\_TOKEN token is received from the server.
The token can be passed as the `token` option on a future connection to
the same server to skip address validation. Read/write.

### `session.onorigin`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnOriginCallback}

The callback to invoke when an ORIGIN frame (RFC 9412) is received from
the server, indicating which origins the server is authoritative for.
Read/write.

### `session.createBidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {ArrayBuffer | ArrayBufferView | Blob}
  * `headers` {Object} Initial request or response headers to send. Only
    used when the session supports headers (e.g. HTTP/3). If `body` is not
    specified and `headers` is provided, the stream is treated as
    headers-only (terminal).
  * `priority` {string} The priority level of the stream. One of `'high'`,
    `'default'`, or `'low'`. **Default:** `'default'`.
  * `incremental` {boolean} When `true`, data from this stream may be
    interleaved with data from other streams of the same priority level.
    When `false`, the stream should be completed before same-priority peers.
    **Default:** `false`.
  * `onheaders` {quic.OnHeadersCallback} Callback for received headers.
  * `ontrailers` {quic.OnTrailersCallback} Callback for received trailers.
  * `onwanttrailers` {Function} Callback when trailers should be sent.
* Returns: {Promise} for a {quic.QuicStream}

Open a new bidirectional stream. If the `body` option is not specified,
the outgoing stream will be half-closed. The `priority` and `incremental`
options are only used when the session supports priority (e.g. HTTP/3).
The `headers`, `onheaders`, `ontrailers`, and `onwanttrailers` options
are only used when the session supports headers (e.g. HTTP/3).

### `session.createUnidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {ArrayBuffer | ArrayBufferView | Blob}
  * `headers` {Object} Initial request headers to send.
  * `priority` {string} The priority level of the stream. One of `'high'`,
    `'default'`, or `'low'`. **Default:** `'default'`.
  * `incremental` {boolean} When `true`, data from this stream may be
    interleaved with data from other streams of the same priority level.
    When `false`, the stream should be completed before same-priority peers.
    **Default:** `false`.
  * `onheaders` {quic.OnHeadersCallback} Callback for received headers.
  * `ontrailers` {quic.OnTrailersCallback} Callback for received trailers.
  * `onwanttrailers` {Function} Callback when trailers should be sent.
* Returns: {Promise} for a {quic.QuicStream}

Open a new unidirectional stream. If the `body` option is not specified,
the outgoing stream will be closed. The `priority` and `incremental`
options are only used when the session supports priority (e.g. HTTP/3).

### `session.path`

<!-- YAML
added: v23.8.0
-->

* Type: {Object|undefined}
  * `local` {net.SocketAddress}
  * `remote` {net.SocketAddress}

The local and remote socket addresses associated with the session. Read only.

### `session.sendDatagram(datagram[, encoding])`

<!-- YAML
added: v23.8.0
-->

* `datagram` {string|ArrayBufferView|Promise}
* `encoding` {string} The encoding to use if `datagram` is a string.
  **Default:** `'utf8'`.
* Returns: {Promise} for a {bigint} datagram ID.

Sends an unreliable datagram to the remote peer, returning a promise for
the datagram ID.

If `datagram` is a string, it will be encoded using the specified `encoding`.

If `datagram` is an `ArrayBufferView`, the underlying `ArrayBuffer` will be
transferred if possible (taking ownership to prevent mutation after send).
If the buffer is not transferable (e.g., a `SharedArrayBuffer` or a view
over a subset of a larger buffer such as a pooled `Buffer`), the data will
be copied instead.

If `datagram` is a `Promise`, it will be awaited before sending. If the
session closes while awaiting, `0n` is returned silently (datagrams are
inherently unreliable).

If the datagram payload is zero-length (empty string after encoding, detached
buffer, or zero-length view), `0n` is returned and no datagram is sent.

Datagrams cannot be fragmented — each must fit within a single QUIC packet.
The maximum datagram size is determined by the peer's
`maxDatagramFrameSize` transport parameter (which the peer advertises during
the handshake). If the peer sets this to `0`, datagrams are not supported
and `0n` will be returned. If the datagram exceeds the peer's limit, it
will be silently dropped and `0n` returned. The local
`maxDatagramFrameSize` transport parameter (default: `1200` bytes) controls
what this endpoint advertises to the peer as its own maximum.

### `session.certificate`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|undefined}

The local certificate as an object with properties such as `subject`,
`issuer`, `valid_from`, `valid_to`, `fingerprint`, etc. Returns `undefined`
if the session is destroyed or no certificate is available.

### `session.peerCertificate`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|undefined}

The peer's certificate as an object with properties such as `subject`,
`issuer`, `valid_from`, `valid_to`, `fingerprint`, etc. Returns `undefined`
if the session is destroyed or the peer did not present a certificate.

### `session.ephemeralKeyInfo`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|undefined}

The ephemeral key information for the session, with properties such as
`type`, `name`, and `size`. Only available on client sessions. Returns
`undefined` for server sessions or if the session is destroyed.

### `session.maxDatagramSize`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint}

The maximum datagram payload size in bytes that the peer will accept,
as advertised in the peer's `maxDatagramFrameSize` transport parameter.
Returns `0n` if the peer does not support datagrams or if the handshake
has not yet completed. Datagrams larger than this value will not be sent.

### `session.stats`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.QuicSession.Stats}

Return the current statistics for the session. Read only.

### `session.updateKey()`

<!-- YAML
added: v23.8.0
-->

Initiate a key update for the session.

### `session[Symbol.asyncDispose]()`

<!-- YAML
added: v23.8.0
-->

Calls `session.close()` and returns a promise that fulfills when the
session has closed.

## Class: `QuicSession.Stats`

<!-- YAML
added: v23.8.0
-->

### `sessionStats.createdAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.closingAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.handshakeCompletedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.handshakeConfirmedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.bidiInStreamCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.bidiOutStreamCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.uniInStreamCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.uniOutStreamCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.maxBytesInFlight`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.bytesInFlight`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.blockCount`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.cwnd`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.latestRtt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.minRtt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.rttVar`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.smoothedRtt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.ssthresh`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.datagramsReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.datagramsSent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.datagramsAcknowledged`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `sessionStats.datagramsLost`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

## Class: `QuicStream`

<!-- YAML
added: v23.8.0
-->

### `stream.closed`

<!-- YAML
added: v23.8.0
-->

* Type: {Promise}

A promise that is fulfilled when the stream is fully closed.

### `stream.destroy([error])`

<!-- YAML
added: v23.8.0
-->

* `error` {any}

Immediately and abruptly destroys the stream.

### `stream.destroyed`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if `stream.destroy()` has been called.

### `stream.direction`

<!-- YAML
added: v23.8.0
-->

* Type: {string} One of either `'bidi'` or `'uni'`.

The directionality of the stream. Read only.

### `stream.id`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

The stream ID. Read only.

### `stream.onblocked`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnBlockedCallback}

The callback to invoke when the stream is blocked. Read/write.

### `stream.onreset`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.OnStreamErrorCallback}

The callback to invoke when the stream is reset. Read/write.

### `stream.headers`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|undefined}

The buffered initial headers received on this stream, or `undefined` if the
application does not support headers or no headers have been received yet.
For server-side streams, this contains the request headers (e.g., `:method`,
`:path`, `:scheme`). For client-side streams, this contains the response
headers (e.g., `:status`).

Header names are lowercase strings. Multi-value headers are represented as
arrays. The object has `__proto__: null`.

### `stream.onheaders`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnHeadersCallback}

The callback to invoke when headers are received on the stream. The callback
receives `(headers, kind)` where `headers` is an object (same format as
`stream.headers`) and `kind` is one of `'initial'` or `'informational'`
(for 1xx responses). Throws `ERR_INVALID_STATE` if set on a session that
does not support headers. Read/write.

### `stream.ontrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnTrailersCallback}

The callback to invoke when trailing headers are received from the peer.
The callback receives `(trailers)` where `trailers` is an object in the
same format as `stream.headers`. Throws `ERR_INVALID_STATE` if set on a
session that does not support headers. Read/write.

### `stream.onwanttrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {Function}

The callback to invoke when the application is ready for trailing headers
to be sent. This is called synchronously — the user must call
[`stream.sendTrailers()`][] within this callback. Throws
`ERR_INVALID_STATE` if set on a session that does not support headers.
Read/write.

### `stream.pendingTrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|undefined}

Set trailing headers to be sent automatically when the application requests
them. This is an alternative to the [`stream.onwanttrailers`][] callback
for cases where the trailers are known before the body completes. Throws
`ERR_INVALID_STATE` if set on a session that does not support headers.
Read/write.

### `stream.sendHeaders(headers[, options])`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object} Header object with string keys and string or
  string-array values. Pseudo-headers (`:method`, `:path`, etc.) must
  appear before regular headers.
* `options` {Object}
  * `terminal` {boolean} If `true`, the stream is closed for sending
    after the headers (no body will follow). **Default:** `false`.
* Returns: {boolean}

Sends initial or response headers on the stream. For client-side streams,
this sends request headers. For server-side streams, this sends response
headers. Throws `ERR_INVALID_STATE` if the session does not support headers.

### `stream.sendInformationalHeaders(headers)`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object} Header object. Must include `:status` with a 1xx
  value (e.g., `{ ':status': '103', 'link': '</style.css>; rel=preload' }`).
* Returns: {boolean}

Sends informational (1xx) response headers. Server only. Throws
`ERR_INVALID_STATE` if the session does not support headers.

### `stream.sendTrailers(headers)`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object} Trailing header object. Pseudo-headers must not be
  included in trailers.
* Returns: {boolean}

Sends trailing headers on the stream. Must be called synchronously during
the [`stream.onwanttrailers`][] callback, or set ahead of time via
[`stream.pendingTrailers`][]. Throws `ERR_INVALID_STATE` if the session
does not support headers.

### `stream.priority`

<!-- YAML
added: REPLACEME
-->

* Type: {Object|null}
  * `level` {string} One of `'high'`, `'default'`, or `'low'`.
  * `incremental` {boolean} Whether the stream data should be interleaved
    with other streams of the same priority level.

The current priority of the stream. Returns `null` if the session does not
support priority (e.g. non-HTTP/3) or if the stream has been destroyed.
Read only. Use [`stream.setPriority()`][] to change the priority.

On client-side HTTP/3 sessions, the value reflects what was set via
[`stream.setPriority()`][]. On server-side HTTP/3 sessions, the value
reflects the peer's requested priority (e.g., from `PRIORITY_UPDATE` frames).

### `stream.setPriority([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `level` {string} The priority level. One of `'high'`, `'default'`, or
    `'low'`. **Default:** `'default'`.
  * `incremental` {boolean} When `true`, data from this stream may be
    interleaved with data from other streams of the same priority level.
    **Default:** `false`.

Sets the priority of the stream. Throws `ERR_INVALID_STATE` if the session
does not support priority (e.g. non-HTTP/3). Has no effect if the stream
has been destroyed.

### `stream[Symbol.asyncIterator]()`

<!-- YAML
added: REPLACEME
-->

* Returns: {AsyncIterableIterator} yielding {Uint8Array\[]}

The stream implements `Symbol.asyncIterator`, making it directly usable
in `for await...of` loops. Each iteration yields a batch of `Uint8Array`
chunks.

Only one async iterator can be obtained per stream. A second call throws
`ERR_INVALID_STATE`. Non-readable streams (outbound-only unidirectional
or closed) return an immediately-finished iterator.

```mjs
for await (const chunks of stream) {
  for (const chunk of chunks) {
    // Process each Uint8Array chunk
  }
}
```

Compatible with stream/iter utilities:

```mjs
import Stream from 'node:stream/iter';
const body = await Stream.bytes(stream);
const text = await Stream.text(stream);
await Stream.pipeTo(stream, someWriter);
```

### `stream.writer`

<!-- YAML
added: REPLACEME
-->

* Type: {Object}

Returns a Writer object for pushing data to the stream incrementally.
The Writer implements the stream/iter Writer interface with the
try-sync-fallback-to-async pattern.

Only available when no `body` source was provided at creation time or via
[`stream.setBody()`][]. Non-writable streams return an already-closed
Writer. Throws `ERR_INVALID_STATE` if the outbound is already configured.

The Writer has the following methods:

* `writeSync(chunk)` — Synchronous write. Returns `true` if accepted,
  `false` if flow-controlled. Data is NOT accepted on `false`.
* `write(chunk[, options])` — Async write with drain wait. `options.signal`
  is checked at entry but not observed during the write.
* `writevSync(chunks)` — Synchronous vectored write. All-or-nothing.
* `writev(chunks[, options])` — Async vectored write.
* `endSync()` — Synchronous close. Returns total bytes or `-1`.
* `end([options])` — Async close.
* `fail(reason)` — Errors the stream (sends RESET\_STREAM to peer).
* `desiredSize` — Available capacity in bytes, or `null` if closed/errored.

### `stream.setBody(body)`

<!-- YAML
added: REPLACEME
-->

* `body` {string|ArrayBuffer|SharedArrayBuffer|TypedArray|Blob|AsyncIterable|Iterable|Promise|null}

Sets the outbound body source for the stream. Can only be called once.
Mutually exclusive with [`stream.writer`][].

If `body` is `null`, the writable side is closed immediately (FIN sent).
If `body` is a `Promise`, it is awaited and the resolved value is used.
Other types are handled per their optimization tier (see below).

Throws `ERR_INVALID_STATE` if the outbound is already configured or if
the writer has been accessed.

### `stream.session`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.QuicSession}

The session that created this stream. Read only.

### `stream.stats`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.QuicStream.Stats}

The current statistics for the stream. Read only.

## Class: `QuicStream.Stats`

<!-- YAML
added: v23.8.0
-->

### `streamStats.ackedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.createdAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.destroyedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.finalSize`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.isConnected`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.maxOffset`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.maxOffsetAcknowledged`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.maxOffsetReceived`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.openedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

### `streamStats.receivedAt`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

## Types

### Type: `EndpointOptions`

<!-- YAML
added: v23.8.0
-->

* Type: {Object}

The endpoint configuration options passed when constructing a new `QuicEndpoint` instance.

#### `endpointOptions.address`

<!-- YAML
added: v23.8.0
-->

* Type: {net.SocketAddress | string} The local UDP address and port the endpoint should bind to.

If not specified the endpoint will bind to IPv4 `localhost` on a random port.

#### `endpointOptions.addressLRUSize`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

The endpoint maintains an internal cache of validated socket addresses as a
performance optimization. This option sets the maximum number of addresses
that are cache. This is an advanced option that users typically won't have
need to specify.

#### `endpointOptions.ipv6Only`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

When `true`, indicates that the endpoint should bind only to IPv6 addresses.

#### `endpointOptions.maxConnectionsPerHost`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum number of concurrent sessions allowed per remote peer address.

#### `endpointOptions.maxConnectionsTotal`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum total number of concurrent sessions.

#### `endpointOptions.maxRetries`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum number of QUIC retry attempts allowed per remote peer address.

#### `endpointOptions.maxStatelessResetsPerHost`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum number of stateless resets that are allowed per remote peer address.

#### `endpointOptions.retryTokenExpiration`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the length of time a QUIC retry token is considered valid.

#### `endpointOptions.resetTokenSecret`

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC retry tokens.

#### `endpointOptions.tokenExpiration`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the length of time a QUIC token is considered valid.

#### `endpointOptions.tokenSecret`

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC tokens.

#### `endpointOptions.udpReceiveBufferSize`

<!-- YAML
added: v23.8.0
-->

* Type: {number}

#### `endpointOptions.udpSendBufferSize`

<!-- YAML
added: v23.8.0
-->

* Type: {number}

#### `endpointOptions.udpTTL`

<!-- YAML
added: v23.8.0
-->

* Type: {number}

#### `endpointOptions.validateAddress`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

When `true`, requires that the endpoint validate peer addresses using retry packets
while establishing a new connection.

### Type: `SessionOptions`

<!-- YAML
added: v23.8.0
-->

#### `sessionOptions.alpn`

<!-- YAML
added: REPLACEME
-->

* Type: {string} (client) | {string\[]} (server)

The ALPN (Application-Layer Protocol Negotiation) identifier(s).

For **client** sessions, this is a single string specifying the protocol
the client wants to use (e.g. `'h3'`).

For **server** sessions, this is an array of protocol names in preference
order that the server supports (e.g. `['h3', 'h3-29']`). During the TLS
handshake, the server selects the first protocol from its list that the
client also supports.

The negotiated ALPN determines which Application implementation is used
for the session. `'h3'` and `'h3-*'` variants select the HTTP/3
application; all other values select the default application.

Default: `'h3'`

#### `sessionOptions.ca` (client only)

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CA certificates to use for client sessions. For server sessions, CA
certificates are specified per-identity in the [`sessionOptions.sni`][] map.

#### `sessionOptions.cc`

<!-- YAML
added: v23.8.0
-->

* Type: {string}

Specifies the congestion control algorithm that will be used
. Must be set to one of either `'reno'`, `'cubic'`, or `'bbr'`.

This is an advanced option that users typically won't have need to specify.

#### `sessionOptions.certs` (client only)

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The TLS certificates to use for client sessions. For server sessions,
certificates are specified per-identity in the [`sessionOptions.sni`][] map.

#### `sessionOptions.ciphers`

<!-- YAML
added: v23.8.0
-->

* Type: {string}

The list of supported TLS 1.3 cipher algorithms.

#### `sessionOptions.crl` (client only)

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CRL to use for client sessions. For server sessions, CRLs are specified
per-identity in the [`sessionOptions.sni`][] map.

#### `sessionOptions.enableEarlyData`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean} **Default:** `true`

When `true`, enables TLS 0-RTT early data for this session. Early data
allows the client to send application data before the TLS handshake
completes, reducing latency on reconnection when a valid session ticket
is available. Set to `false` to disable early data support.

#### `sessionOptions.groups`

<!-- YAML
added: v23.8.0
-->

* Type: {string}

The list of support TLS 1.3 cipher groups.

#### `sessionOptions.keylog`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True to enable TLS keylogging output.

#### `sessionOptions.keys` (client only)

<!-- YAML
added: v23.8.0
changes:
  - version: v25.9.0
    pr-url: https://github.com/nodejs/node/pull/62335
    description: CryptoKey is no longer accepted.
-->

* Type: {KeyObject|KeyObject\[]}

The TLS crypto keys to use for client sessions. For server sessions,
keys are specified per-identity in the [`sessionOptions.sni`][] map.

#### `sessionOptions.maxPayloadSize`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum UDP packet payload size.

#### `sessionOptions.maxStreamWindow`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum stream flow-control window size.

#### `sessionOptions.maxWindow`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum session flow-control window size.

#### `sessionOptions.minVersion`

<!-- YAML
added: v23.8.0
-->

* Type: {number}

The minimum QUIC version number to allow. This is an advanced option that users
typically won't have need to specify.

#### `sessionOptions.preferredAddressPolicy`

<!-- YAML
added: v23.8.0
-->

* Type: {string} One of `'use'`, `'ignore'`, or `'default'`.

When the remote peer advertises a preferred address, this option specifies whether
to use it or ignore it.

#### `sessionOptions.qlog`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if qlog output should be enabled.

#### `sessionOptions.sessionTicket`

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBufferView} A session ticket to use for 0RTT session resumption.

#### `sessionOptions.handshakeTimeout`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum number of milliseconds a TLS handshake is permitted to take
to complete before timing out.

#### `sessionOptions.keepAlive`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint|number}
* **Default:** `0` (disabled)

Specifies the keep-alive timeout in milliseconds. When set to a non-zero
value, PING frames will be sent automatically to keep the connection alive
before the idle timeout fires. The value should be less than the effective
idle timeout (`maxIdleTimeout` transport parameter) to be useful.

#### `sessionOptions.servername` (client only)

<!-- YAML
added: v23.8.0
-->

* Type: {string}

The peer server name to target (SNI). Defaults to `'localhost'`.

#### `sessionOptions.sni` (server only)

<!-- YAML
added: REPLACEME
-->

* Type: {Object}

An object mapping host names to TLS identity options for Server Name
Indication (SNI) support. This is required for server sessions. The
special key `'*'` specifies the default/fallback identity used when
no other host name matches. Each entry may contain:

* `keys` {KeyObject|KeyObject\[]} The TLS private keys. **Required.**
* `certs` {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}
  The TLS certificates. **Required.**
* `ca` {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}
  Optional CA certificate overrides.
* `crl` {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}
  Optional certificate revocation lists.
* `verifyPrivateKey` {boolean} Verify the private key. Default: `false`.
* `port` {number} The port to advertise in ORIGIN frames (RFC 9412) for
  this host name. **Default:** `443`. Only used for HTTP/3 sessions.
* `authoritative` {boolean} Whether to include this host name in ORIGIN
  frames. **Default:** `true`. Set to `false` to exclude a host name
  from ORIGIN advertisements. Wildcard (`'*'`) entries are always
  excluded regardless of this setting.

```mjs
const endpoint = await listen(callback, {
  sni: {
    '*': { keys: [defaultKey], certs: [defaultCert] },
    'api.example.com': { keys: [apiKey], certs: [apiCert], port: 8443 },
    'www.example.com': { keys: [wwwKey], certs: [wwwCert], ca: [customCA] },
    'internal.example.com': { keys: [intKey], certs: [intCert], authoritative: false },
  },
});
```

Shared TLS options (such as `ciphers`, `groups`, `keylog`, and `verifyClient`)
are specified at the top level of the session options and apply to all
identities. Each SNI entry overrides only the per-identity certificate
fields.

The SNI map can be replaced at runtime using `endpoint.setSNIContexts()`,
which atomically swaps the map for new sessions while existing sessions
continue to use their original identity.

#### `sessionOptions.tlsTrace`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True to enable TLS tracing output.

#### `sessionOptions.token` (client only)

<!-- YAML
added: REPLACEME
-->

* Type: {ArrayBufferView}

An opaque address validation token previously received from the server
via the [`session.onnewtoken`][] callback. Providing a valid token on
reconnection allows the client to skip the server's address validation,
reducing handshake latency.

#### `sessionOptions.transportParams`

<!-- YAML
added: v23.8.0
-->

* Type: {quic.TransportParams}

The QUIC transport parameters to use for the session.

#### `sessionOptions.unacknowledgedPacketThreshold`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

Specifies the maximum number of unacknowledged packets a session should allow.

#### `sessionOptions.rejectUnauthorized`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean} **Default:** `true`

If `true`, the peer certificate is verified against the list of supplied CAs.
An error is emitted if verification fails; the error can be inspected via
the `validationErrorReason` and `validationErrorCode` fields in the
handshake callback. If `false`, peer certificate verification errors are
ignored.

#### `sessionOptions.verifyClient`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True to require verification of TLS client certificate.

#### `sessionOptions.verifyPrivateKey` (client only)

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True to require private key verification for client sessions. For server
sessions, this option is specified per-identity in the
[`sessionOptions.sni`][] map.

#### `sessionOptions.version`

<!-- YAML
added: v23.8.0
-->

* Type: {number}

The QUIC version number to use. This is an advanced option that users typically
won't have need to specify.

### Type: `TransportParams`

<!-- YAML
added: v23.8.0
-->

#### `transportParams.preferredAddressIpv4`

<!-- YAML
added: v23.8.0
-->

* Type: {net.SocketAddress} The preferred IPv4 address to advertise.

#### `transportParams.preferredAddressIpv6`

<!-- YAML
added: v23.8.0
-->

* Type: {net.SocketAddress} The preferred IPv6 address to advertise.

#### `transportParams.initialMaxStreamDataBidiLocal`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.initialMaxStreamDataBidiRemote`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.initialMaxStreamDataUni`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.initialMaxData`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.initialMaxStreamsBidi`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.initialMaxStreamsUni`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.maxIdleTimeout`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.activeConnectionIDLimit`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.ackDelayExponent`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.maxAckDelay`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}

#### `transportParams.maxDatagramFrameSize`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint|number}
* **Default:** `1200`

The maximum size in bytes of a DATAGRAM frame payload that this endpoint
is willing to receive. Set to `0` to disable datagram support. The peer
will not send datagrams larger than this value. The actual maximum size of
a datagram that can be _sent_ is determined by the peer's
`maxDatagramFrameSize`, not this endpoint's value.

## Callbacks

### Callback: `OnSessionCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicEndpoint}
* `session` {quic.QuicSession}

The callback function that is invoked when a new session is initiated by a remote peer.

### Callback: `OnStreamCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `stream` {quic.QuicStream}

### Callback: `OnDatagramCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `datagram` {Uint8Array}
* `early` {boolean}

### Callback: `OnDatagramStatusCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `id` {bigint}
* `status` {string} One of either `'lost'` or `'acknowledged'`.

### Callback: `OnPathValidationCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `result` {string} One of either `'success'`, `'failure'`, or `'aborted'`.
* `newLocalAddress` {net.SocketAddress}
* `newRemoteAddress` {net.SocketAddress}
* `oldLocalAddress` {net.SocketAddress}
* `oldRemoteAddress` {net.SocketAddress}
* `preferredAddress` {boolean}

### Callback: `OnSessionTicketCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `ticket` {Object}

### Callback: `OnVersionNegotiationCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `version` {number}
* `requestedVersions` {number\[]}
* `supportedVersions` {number\[]}

### Callback: `OnHandshakeCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `sni` {string}
* `alpn` {string}
* `cipher` {string}
* `cipherVersion` {string}
* `validationErrorReason` {string}
* `validationErrorCode` {number}
* `earlyDataAttempted` {boolean}
* `earlyDataAccepted` {boolean}

### Callback: `OnNewTokenCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `token` {Buffer} The NEW\_TOKEN token data.
* `address` {SocketAddress} The remote address the token is associated with.

### Callback: `OnOriginCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `origins` {string\[]} The list of origins the server is authoritative for.

### Callback: `OnBlockedCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicStream}

### Callback: `OnStreamErrorCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicStream}
* `error` {any}

### Callback: `OnHeadersCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `headers` {Object} Header object with lowercase string keys and
  string or string-array values.
* `kind` {string} One of `'initial'` or `'informational'`.

### Callback: `OnTrailersCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `trailers` {Object} Trailing header object.

## Diagnostic Channels

### Channel: `quic.endpoint.created`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `config` {quic.EndpointOptions}

### Channel: `quic.endpoint.listen`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `optoins` {quic.SessionOptions}

### Channel: `quic.endpoint.closing`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `hasPendingError` {boolean}

### Channel: `quic.endpoint.closed`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}

### Channel: `quic.endpoint.error`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `error` {any}

### Channel: `quic.endpoint.busy.change`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `busy` {boolean}

### Channel: `quic.session.created.client`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.created.server`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.open.stream`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.received.stream`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.send.datagram`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.update.key`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.closing`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.closed`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.receive.datagram`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.receive.datagram.status`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.path.validation`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.new.token`

<!-- YAML
added: REPLACEME
-->

Published when a client session receives a NEW\_TOKEN frame from the
server. The message contains `token` {Buffer}, `address` {SocketAddress},
and `session` {quic.QuicSession}.

### Channel: `quic.session.ticket`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.version.negotiation`

<!-- YAML
added: v23.8.0
-->

### Channel: `quic.session.handshake`

<!-- YAML
added: v23.8.0
-->

[`session.onnewtoken`]: #sessiononnewtoken
[`sessionOptions.sni`]: #sessionoptionssni-server-only
[`stream.onwanttrailers`]: #streamonwanttrailers
[`stream.pendingTrailers`]: #streampendingtrailers
[`stream.sendTrailers()`]: #streamsendtrailersheaders
[`stream.setBody()`]: #streamsetbodybody
[`stream.setPriority()`]: #streamsetpriorityoptions
[`stream.writer`]: #streamwriter
