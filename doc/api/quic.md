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

### `endpoint.listening`

* Type: {boolean}

True if the endpoint is actively listening for incoming connections. Read only.

### `endpoint.maxConnectionsPerHost`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum number of concurrent connections allowed per remote IP address.
`0` means unlimited (default). Can be set at construction time via the
`maxConnectionsPerHost` option and changed dynamically at any time.
The valid range is `0` to `65535`.

### `endpoint.maxConnectionsTotal`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum total number of concurrent connections across all remote
addresses. `0` means unlimited (default). Can be set at construction time via
the `maxConnectionsTotal` option and changed dynamically at any time.
The valid range is `0` to `65535`.

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

### `session.close([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `code` {bigint|number} The error code to include in the `CONNECTION_CLOSE`
    frame sent to the peer. Defaults to `0` (no error). **Default:** `0`.
  * `type` {string} Either `'transport'` or `'application'`. Determines the
    error code namespace used in the `CONNECTION_CLOSE` frame. When `'transport'`
    (the default), the frame type is `0x1c` and the code is interpreted as a QUIC
    transport error. When `'application'`, the frame type is `0x1d` and the code
    is application-specific. **Default:** `'transport'`.
  * `reason` {string} An optional human-readable reason string included in
    the `CONNECTION_CLOSE` frame. Per RFC 9000, this is for diagnostic purposes
    only and should not be used for machine-readable error descriptions.
* Returns: {Promise}

Initiate a graceful close of the session. Existing streams will be allowed
to complete but no new streams will be opened. Once all streams have closed,
the session will be destroyed. The returned promise will be fulfilled once
the session has been destroyed. If a non-zero `code` is specified, the
promise will reject with an `ERR_QUIC_TRANSPORT_ERROR` or
`ERR_QUIC_APPLICATION_ERROR` depending on the `type`.

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

### `session.destroy([error[, options]])`

<!-- YAML
added: v23.8.0
-->

* `error` {any}
* `options` {Object}
  * `code` {bigint|number} The error code to include in the `CONNECTION_CLOSE`
    frame sent to the peer. **Default:** `0`.
  * `type` {string} Either `'transport'` or `'application'`. **Default:**
    `'transport'`.
  * `reason` {string} An optional human-readable reason string included in
    the `CONNECTION_CLOSE` frame.

Immediately destroy the session. All streams will be destroyed and the
session will be closed. If `error` is provided and [`session.onerror`][] is
set, the `onerror` callback is invoked before destruction. The
`session.closed` promise will reject with the error. If `options` is
provided, the `CONNECTION_CLOSE` frame sent to the peer will include the
specified error code, type, and reason.

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

### `session.onerror`

* Type: {Function|undefined}

An optional callback invoked when the session is destroyed with an error.
This includes errors caused by user callbacks that throw or reject (see
[Callback error handling][]). The callback receives a single argument: the
error that triggered the destruction. If the `onerror` callback itself throws
or returns a promise that rejects, the error is surfaced as an uncaught
exception. Read/write.

Can also be set via the `onerror` option in [`quic.connect()`][] or
[`quic.listen()`][].

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

### `session.onearlyrejected`

* Type: {Function|undefined}

The callback to invoke when the server rejects 0-RTT early data. When
this fires, all streams that were opened during the 0-RTT phase have
been destroyed. The application should re-open streams if needed.
Read/write.

This callback only fires on the client side when the server rejects
the client's 0-RTT attempt. The connection falls back to 1-RTT and
continues normally.

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

### `session.ongoaway`

<!-- YAML
added: REPLACEME
-->

* Type: {Function}

The callback to invoke when the peer sends an HTTP/3 GOAWAY frame,
indicating it is initiating a graceful shutdown. The callback receives
`(lastStreamId)` where `lastStreamId` is a `{bigint}`:

* When `lastStreamId` is `-1n`, the peer sent a shutdown notice (intent
  to close) without specifying a stream boundary. All existing streams
  may still be processed.
* When `lastStreamId` is `>= 0n`, it is the highest stream ID the peer
  may have processed. Streams with IDs above this value were NOT
  processed and can be safely retried on a new connection.

After GOAWAY is received, `session.createBidirectionalStream()` will
throw `ERR_INVALID_STATE`. Existing streams continue until they
complete or the session closes.

This callback is only relevant for HTTP/3 sessions. Read/write.

### `session.onkeylog`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnKeylogCallback}

The callback to invoke when TLS key material is available. Requires
[`sessionOptions.keylog`][] to be `true`. Each invocation receives a single
line of [NSS Key Log Format][] text (including a trailing newline). This is
useful for decrypting packet captures with tools like Wireshark. Read/write.

Can also be set via the `onkeylog` option in [`quic.connect()`][] or
[`quic.listen()`][].

### `session.onqlog`

<!-- YAML
added: REPLACEME
-->

* Type: {quic.OnQlogCallback}

The callback to invoke when qlog data is available. Requires
[`sessionOptions.qlog`][] to be `true`. The callback receives a string
chunk of [JSON-SEQ][] formatted qlog data and a boolean `fin` flag. When
`fin` is `true`, the chunk is the final qlog output for this session and
the concatenated chunks form a complete qlog trace. Read/write.

Qlog data arrives during the connection lifecycle. The first chunk contains
the qlog header with format metadata. Subsequent chunks contain trace
events. The final chunk (with `fin` set to `true`) is emitted during
session destruction and completes the JSON-SEQ output.

Can also be set via the `onqlog` option in [`quic.connect()`][] or
[`quic.listen()`][].

### `session.createBidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView |
    Blob | FileHandle | AsyncIterable | Iterable | Promise | null}
    The outbound body source. See [`stream.setBody()`][] for details on
    supported types. When omitted, the stream starts half-closed (writable
    side open, no body queued).
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
  * `highWaterMark` {number} The maximum number of bytes that the writer
    will buffer before `writeSync()` returns `false`. When the buffered
    data exceeds this limit, the caller should wait for drain before
    writing more. **Default:** `65536` (64 KB).
  * `onheaders` {Function} Callback for received initial response headers.
    Called with `(headers)`.
  * `ontrailers` {Function} Callback for received trailing headers.
    Called with `(trailers)`.
  * `oninfo` {Function} Callback for received informational (1xx) headers.
    Called with `(headers)`.
  * `onwanttrailers` {Function} Callback when trailers should be sent.
    Called with no arguments; use [`stream.sendTrailers()`][] within the
    callback.
* Returns: {Promise} for a {quic.QuicStream}

Open a new bidirectional stream. If the `body` option is not specified,
the outgoing stream will be half-closed. The `priority` and `incremental`
options are only used when the session supports priority (e.g. HTTP/3).
The `headers`, `onheaders`, `ontrailers`, `oninfo`, and `onwanttrailers`
options are only used when the session supports headers (e.g. HTTP/3).

### `session.createUnidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView |
    Blob | FileHandle | AsyncIterable | Iterable | Promise | null}
    The outbound body source. See [`stream.setBody()`][] for details on
    supported types. When omitted, the stream is closed immediately.
  * `headers` {Object} Initial request headers to send.
  * `priority` {string} The priority level of the stream. One of `'high'`,
    `'default'`, or `'low'`. **Default:** `'default'`.
  * `incremental` {boolean} When `true`, data from this stream may be
    interleaved with data from other streams of the same priority level.
    When `false`, the stream should be completed before same-priority peers.
    **Default:** `false`.
  * `onheaders` {Function} Callback for received initial response headers.
    Called with `(headers)`.
  * `ontrailers` {Function} Callback for received trailing headers.
    Called with `(trailers)`.
  * `oninfo` {Function} Callback for received informational (1xx) headers.
    Called with `(headers)`.
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

If `datagram` is an `ArrayBufferView`, the bytes are copied into an
internal buffer; the caller's source buffer is unchanged and may be reused
or mutated immediately after the call returns. Callers that want to ensure
their source cannot be mutated after the call (for example, when handing
the buffer off to another async consumer) can call
`ArrayBuffer.prototype.transfer()` themselves before passing the buffer.

If `datagram` is a `Promise`, it will be awaited before sending. If the
session closes while awaiting, `0n` is returned silently (datagrams are
inherently unreliable).

If the datagram payload is zero-length (empty string after encoding, detached
buffer, or zero-length view), `0n` is returned and no datagram is sent.

For HTTP/3 sessions, the peer must advertise `SETTINGS_H3_DATAGRAM=1`
(via `application: { enableDatagrams: true }`) for datagrams to be sent.
If the peer's setting is `0`, `sendDatagram()` returns `0n` (per RFC 9297
§3, an endpoint MUST NOT send HTTP Datagrams unless the peer indicated
support).

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

* Type: {number}

The maximum datagram payload size in bytes that the peer will accept.
This is derived from the peer's `maxDatagramFrameSize` transport
parameter minus the DATAGRAM frame overhead (type byte and variable-length
integer encoding). Returns `0` if the peer does not support datagrams or
if the handshake has not yet completed. Datagrams larger than this value
will not be sent.

### `session.maxPendingDatagrams`

<!-- YAML
added: REPLACEME
-->

* Type: {number}
* **Default:** `128`

The maximum number of datagrams that can be queued for sending. Datagrams
are queued when `sendDatagram()` is called and sent opportunistically
alongside stream data by the packet serialization loop. When the queue
is full, the [`sessionOptions.datagramDropPolicy`][] determines whether
the oldest or newest datagram is dropped. Dropped datagrams are reported
as lost via the `ondatagramstatus` callback.

This property can be changed dynamically to adjust queue capacity
based on application activity or memory pressure. The valid range
is `0` to `65535`.

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

## Class: `QuicError`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

A `QuicError` is an `Error` subclass that carries an explicit numeric
QUIC error code. Use it to abort a QUIC stream or session with a
specific application-protocol-defined error code rather than letting
the implementation pick a generic fallback.

The class is exported from `node:quic`:

```mjs
import { QuicError } from 'node:quic';
```

```cjs
const { QuicError } = require('node:quic');
```

When a `QuicError` is supplied to APIs that emit a wire frame
([`writer.fail()`][], [`stream.destroy()`][]), the QUIC stack uses
[`error.errorCode`][] as the wire code for the resulting frame.
When any other value is supplied (for example a plain `Error`), the
implementation falls back to the negotiated application protocol's
"internal error" code (`H3_INTERNAL_ERROR` (`0x102`) for HTTP/3, or
the QUIC transport-layer `INTERNAL_ERROR` (`0x1`) for raw QUIC).

The Node.js error code (`error.code`) defaults to
`'ERR_QUIC_STREAM_ABORTED'`. Callers who need a more specific code
string can override it via `options.code` — the numeric QUIC code
is unaffected.

The Node.js error code is fixed at `'ERR_QUIC_STREAM_ABORTED'` so that
catch blocks can distinguish a `QuicError` from other Node.js errors
without checking the prototype chain. The numeric QUIC code lives on
the separate [`error.errorCode`][] property to avoid colliding with
the Node.js convention that `error.code` is a string.

### `new QuicError(message, options)`

<!-- YAML
added: REPLACEME
-->

* `message` {string} A human-readable description of the error.
* `options` {Object}
  * `errorCode` {bigint | number} The numeric QUIC error code. Numbers
    are coerced to `BigInt`. Must be a non-negative 62-bit unsigned
    varint (`0n <= errorCode <= 2n ** 62n - 1n`).
  * `code` {string} The Node.js-style error code string assigned to
    `error.code`. Defaults to `'ERR_QUIC_STREAM_ABORTED'`.
  * `type` {string} Either `'application'` (default) or `'transport'`.
    Indicates whether the code is defined by the negotiated
    application protocol (e.g. RFC 9114 for HTTP/3) or by the QUIC
    transport layer (RFC 9000). Stream resets always carry application
    codes, so the default is `'application'`.

```mjs
import { QuicError } from 'node:quic';

const err = new QuicError('rejecting stream', { errorCode: 0x10cn });
console.log(err.code);       // 'ERR_QUIC_STREAM_ABORTED'
console.log(err.errorCode);  // 268n
console.log(err.type);       // 'application'

const custom = new QuicError('custom failure', {
  errorCode: 0x10cn,
  code: 'ERR_MY_QUIC_FAILURE',
});
console.log(custom.code);    // 'ERR_MY_QUIC_FAILURE'
```

### `error.errorCode`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint}

The numeric QUIC error code carried by this error.

### `error.type`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

Either `'application'` or `'transport'`. Indicates the namespace of
[`error.errorCode`][].

## Class: `QuicStream`

<!-- YAML
added: v23.8.0
-->

### `stream.closed`

<!-- YAML
added: v23.8.0
-->

* Type: {Promise}

A promise that is fulfilled when the stream is fully closed. It resolves
when the stream closes cleanly (including idle timeout). It rejects with
an `ERR_QUIC_APPLICATION_ERROR` or `ERR_QUIC_TRANSPORT_ERROR` when the
stream is closed due to a QUIC error (e.g., stream reset by the peer,
CONNECTION\_CLOSE with a non-zero error code).

### `stream.destroy([error[, options]])`

<!-- YAML
added: v23.8.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/62876
    description: Added the `options` parameter accepting `code` and `reason`.
-->

* `error` {any}
* `options` {Object}
  * `code` {bigint|number} The application error code to include in the
    `RESET_STREAM` and `STOP_SENDING` frames sent to the peer. Numbers are
    coerced to `BigInt`. When omitted, the wire code is derived from `error`
    (see below).
  * `reason` {string} An optional human-readable reason string. Accepted for
    symmetry with [`session.close()`][] and [`session.destroy()`][], but
    **not transmitted on the wire** — neither `RESET_STREAM` nor
    `STOP_SENDING` carry a reason field. Provided for application logging
    and for use by the [`stream.onerror`][] callback.

Immediately and abruptly destroys the stream. If `error` is provided and
[`stream.onerror`][] is set, the `onerror` callback is invoked before
destruction. The `stream.closed` promise rejects with the error.

When the stream is destroyed with an `error` (or with an explicit
`options.code`), the QUIC stack signals the abort to the peer:

* If the writable side is still open, a `RESET_STREAM` frame is sent.
* If the readable side is still open (a bidirectional stream, or a
  remote-initiated unidirectional stream), a `STOP_SENDING` frame is sent.

Both frames carry the same wire code, resolved with the following
precedence:

1. `options.code`, when explicitly provided.
2. [`error.errorCode`][], when `error` is a [`QuicError`][].
3. The negotiated application protocol's "internal error" code
   (`H3_INTERNAL_ERROR` (`0x102`) for HTTP/3, or the QUIC transport-layer
   `INTERNAL_ERROR` (`0x1`) for raw QUIC).

A clean destroy — no `error` and no `options.code` — does not emit
`RESET_STREAM` or `STOP_SENDING`; the stream's existing close machinery
handles teardown.

See [Aborting a stream][] for an overview of the available stream-abort
APIs.

### `stream.destroyed`

<!-- YAML
added: v23.8.0
-->

* Type: {boolean}

True if `stream.destroy()` has been called.

### Aborting a stream

A QuicStream can be aborted in three ways, each producing different
wire-frame side effects:

* [`writer.fail(reason)`][] — Aborts only the writable side. Sends
  `RESET_STREAM` to the peer. The readable side is unaffected; any data
  already buffered for read remains available.
* [`stream.destroy()`][] with an `error` argument — Tears the stream
  down completely. Sends `RESET_STREAM` on any still-open writable side
  **and** `STOP_SENDING` on any still-open readable side. The wire code
  is derived from `error` (see [`stream.destroy()`][] for the precedence
  rules).
* [`stream.destroy()`][] with an explicit `options.code` — Same as the
  previous form but with a caller-supplied wire code, which takes
  precedence over any code carried by `error`.

When `error` is a [`QuicError`][], its [`error.errorCode`][] is used as
the wire code for both `writer.fail()` and `stream.destroy()`. Otherwise
the implementation falls back to the negotiated application protocol's
"internal error" code (see [`QuicError`][]).

### `stream.early`

* Type: {boolean}

True if any data on this stream was received as 0-RTT (early data)
before the TLS handshake completed. Early data is less secure and
could potentially be replayed by an attacker. Applications should
treat early data with appropriate caution.

This property is only meaningful on the server side. On the client
side, it is always `false`.

### `stream.direction`

<!-- YAML
added: v23.8.0
-->

* Type: {string} One of either `'bidi'` or `'uni'`.

The directionality of the stream. Read only.

### `stream.highWaterMark`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum number of bytes that the writer will buffer before
`writeSync()` returns `false`. When the buffered data exceeds this limit,
the caller should wait for the `drainableProtocol` promise to resolve
before writing more.

The value can be changed dynamically at any time. This is particularly
useful for streams received via the `onstream` callback, where the
default (65536) may need to be adjusted based on application needs.
The valid range is `0` to `4294967295`.

### `stream.id`

<!-- YAML
added: v23.8.0
-->

* Type: {bigint}

The stream ID. Read only.

### `stream.onerror`

* Type: {Function|undefined}

An optional callback invoked when the stream is destroyed with an error.
This includes errors caused by user callbacks that throw or reject (see
[Callback error handling][]). The callback receives a single argument: the
error that triggered the destruction. If the `onerror` callback itself throws
or returns a promise that rejects, the error is surfaced as an uncaught
exception. Read/write.

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

The callback to invoke when the peer aborts a direction of the stream by
sending a `RESET_STREAM` frame (the peer abandons their writable side, so
no further data will arrive on our readable side) or a `STOP_SENDING`
frame (the peer asks us to stop writing on our writable side).

The callback receives a Node.js error whose `errorCode` (`bigint`)
property carries the application error code from the wire frame.

The stream is **not** automatically destroyed when this callback fires —
the application chooses how to react. Common patterns are: ignore (and
continue using the still-active direction on a bidirectional stream),
abort the other direction with [`writer.fail()`][], or tear down the
whole stream with [`stream.destroy()`][]. Read/write.

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

* Type: {Function}

The callback to invoke when initial headers are received on the stream. The
callback receives `(headers)` where `headers` is an object (same format as
`stream.headers`). For HTTP/3, this delivers request pseudo-headers on the
server side and response headers on the client side. Throws
`ERR_INVALID_STATE` if set on a session that does not support headers.
Read/write.

### `stream.ontrailers`

<!-- YAML
added: REPLACEME
-->

* Type: {Function}

The callback to invoke when trailing headers are received from the peer.
The callback receives `(trailers)` where `trailers` is an object in the
same format as `stream.headers`. Throws `ERR_INVALID_STATE` if set on a
session that does not support headers. Read/write.

### `stream.oninfo`

<!-- YAML
added: REPLACEME
-->

* Type: {Function}

The callback to invoke when informational (1xx) headers are received from
the server. The callback receives `(headers)` where `headers` is an object
in the same format as `stream.headers`. Informational headers are sent
before the final response (e.g., 103 Early Hints). Throws
`ERR_INVALID_STATE` if set on a session that does not support headers.
Read/write.

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
* `fail(reason)` — Errors the stream (sends `RESET_STREAM` to peer).
  When `reason` is a [`QuicError`][], its [`error.errorCode`][] is used
  as the wire code on the resulting `RESET_STREAM` frame; otherwise
  the wire code falls back to the negotiated application protocol's
  "internal error" code (`H3_INTERNAL_ERROR` (`0x102`) for HTTP/3, or
  the QUIC transport-layer `INTERNAL_ERROR` (`0x1`) for raw QUIC).
  See [`stream.destroy()`][] for a full-stream abort that also resets
  the readable side via `STOP_SENDING`.
* `desiredSize` — Available capacity in bytes, or `null` if closed/errored.

The bytes from each `writeSync()` / `writevSync()` / `write()` / `writev()`
input chunk are copied into an internal buffer, so the caller's source
buffer is unchanged and may be reused or mutated immediately after the
call returns. Callers that want to ensure a source buffer cannot be
mutated after handing it off can call `ArrayBuffer.prototype.transfer()`
themselves before passing the buffer.

### `stream.setBody(body)`

<!-- YAML
added: REPLACEME
-->

* `body` {string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView |
  Blob | FileHandle | AsyncIterable | Iterable | Promise | null}

Sets the outbound body source for the stream. Can only be called once.
Mutually exclusive with [`stream.writer`][].

The following body source types are supported:

* `null` — The writable side is closed immediately (FIN sent with no data).
* `string` — UTF-8 encoded and sent as a single chunk.
* `ArrayBuffer`, `SharedArrayBuffer`, `ArrayBufferView` — Sent as a single
  chunk. The bytes are copied into an internal buffer, so the caller's
  source buffer is unchanged and may be reused or mutated immediately
  after the call returns. Callers wanting to ensure their source cannot
  be mutated after handing it off can call
  `ArrayBuffer.prototype.transfer()` themselves before passing the buffer.
* `Blob` — Sent from the Blob's underlying data queue.
* {FileHandle} — The file contents are read asynchronously via an
  fd-backed data source. The `FileHandle` must be opened for reading
  (e.g. via [`fs.promises.open(path, 'r')`][]). Once passed as a body, the
  `FileHandle` is locked and cannot be used as a body for another stream.
  The `FileHandle` is automatically closed when the stream finishes.
* `AsyncIterable`, `Iterable` — Each yielded chunk (string or
  `Uint8Array`) is written incrementally in streaming mode.
* `Promise` — Awaited; the resolved value is used as the body (subject
  to the same type rules).

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

#### `endpointOptions.disableStatelessReset`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

When `true`, the endpoint will not send stateless reset packets in response
to packets from unknown connections. Stateless resets allow a peer to detect
that a connection has been lost even when the server has no state for it.
Disabling them may be useful in testing or when stateless resets are handled
at a different layer.

#### `endpointOptions.idleTimeout`

<!-- YAML
added: REPLACEME
-->

* Type: {number}
* Default: `0`

The number of seconds an endpoint will remain alive after all sessions have
closed and it is no longer listening. A value of `0` (default) means the
endpoint is only destroyed when explicitly closed via `endpoint.close()` or
`endpoint.destroy()`. A positive value starts an idle timer when the endpoint
becomes idle; if no new sessions are created before the timer fires, the
endpoint is automatically destroyed. This is useful for connection pooling
where endpoints should linger briefly for reuse by future `connect()` calls.

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

* Type: {number}
* Default: `0` (unlimited)

Specifies the maximum number of concurrent sessions allowed per remote IP
address (ignoring port). When the limit is reached, new connections from the
same IP are refused with `CONNECTION_REFUSED`. A value of `0` disables the
limit. The maximum value is `65535`.

This limit can also be changed dynamically after construction via
[`endpoint.maxConnectionsPerHost`][].

#### `endpointOptions.maxConnectionsTotal`

<!-- YAML
added: v23.8.0
-->

* Type: {number}
* Default: `0` (unlimited)

Specifies the maximum total number of concurrent sessions across all remote
addresses. When the limit is reached, new connections are refused with
`CONNECTION_REFUSED`. A value of `0` disables the limit. The maximum value is
`65535`.

This limit can also be changed dynamically after construction via
[`endpoint.maxConnectionsTotal`][].

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

#### `sessionOptions.application`

<!-- YAML
added: REPLACEME
-->

* Type: {Object}

HTTP/3 application-specific options. These only apply when the negotiated
ALPN selects the HTTP/3 application (`'h3'`).

* `maxHeaderPairs` {number} Maximum number of header name-value pairs
  accepted per header block. Headers beyond this limit are silently
  dropped. **Default:** `128`
* `maxHeaderLength` {number} Maximum total byte length of all header
  names and values combined per header block. Headers that would push
  the total over this limit are silently dropped. **Default:** `8192`
* `maxFieldSectionSize` {number} Maximum size of a compressed header
  field section (QPACK). `0` means unlimited. **Default:** `0`
* `qpackMaxDTableCapacity` {number} QPACK dynamic table capacity in
  bytes. Set to `0` to disable the dynamic table. **Default:** `4096`
* `qpackEncoderMaxDTableCapacity` {number} QPACK encoder maximum
  dynamic table capacity. **Default:** `4096`
* `qpackBlockedStreams` {number} Maximum number of streams that can
  be blocked waiting for QPACK dynamic table updates.
  **Default:** `100`
* `enableConnectProtocol` {boolean} Enable the extended CONNECT
  protocol (RFC 9220). **Default:** `false`
* `enableDatagrams` {boolean} Enable HTTP/3 datagrams (RFC 9297).
  **Default:** `false`

```mjs
const { listen } = await import('node:quic');

await listen((session) => { /* ... */ }, {
  application: {
    maxHeaderPairs: 64,
    qpackMaxDTableCapacity: 8192,
    enableDatagrams: true,
  },
  // ... other session options
});
```

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

When `true`, enables TLS key logging for the session. Key material is
delivered to the [`session.onkeylog`][] callback in [NSS Key Log Format][].
Each callback invocation receives a single line of key material. The output
can be used with tools such as Wireshark to decrypt captured QUIC traffic.

#### `sessionOptions.keys` (client only)

<!-- YAML
added: v23.8.0
changes:
  - version:
     - v26.0.0
     - v25.9.0
     - v24.15.0
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

When `true`, enables [qlog][] diagnostic output for the session. Qlog data
is delivered to the [`session.onqlog`][] callback as chunks of [JSON-SEQ][]
formatted text. The output can be analyzed with qlog visualization tools
such as [qvis][].

#### `sessionOptions.sessionTicket`

<!-- YAML
added: v23.8.0
-->

* Type: {ArrayBufferView} A session ticket to use for 0RTT session resumption.

#### `sessionOptions.datagramDropPolicy`

<!-- YAML
added: REPLACEME
-->

* Type: {string}
* **Default:** `'drop-oldest'`

Controls which datagram to drop when the pending datagram queue
(sized by [`session.maxPendingDatagrams`][]) is full. Must be one of
`'drop-oldest'` (discard the oldest queued datagram to make room) or
`'drop-newest'` (reject the incoming datagram). Dropped datagrams are
reported as lost via the `ondatagramstatus` callback.

This option is immutable after session creation.

#### `sessionOptions.maxDatagramSendAttempts`

* Type: {number}
* **Default:** `5`

The maximum number of `SendPendingData` cycles a datagram can survive
without being sent before it is abandoned. When a datagram cannot be
sent due to congestion control or packet size constraints, it remains
in the queue and the attempt counter increments. Once the limit is
reached, the datagram is dropped and reported as `'abandoned'` via the
`ondatagramstatus` callback. Valid range: `1` to `255`.

#### `sessionOptions.drainingPeriodMultiplier`

<!-- YAML
added: REPLACEME
-->

* Type: {number}
* **Default:** `3`

A multiplier applied to the Probe Timeout (PTO) to compute the draining
period duration after receiving a `CONNECTION_CLOSE` frame from the peer.
RFC 9000 Section 10.2 requires the draining period to persist for at least
three times the current PTO. The valid range is `3` to `255`. Values below
`3` are clamped to `3`.

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
Indication (SNI) support. This is required for server sessions and must
contain at least one entry. The special key `'*'` specifies the optional
default/fallback identity used when no other host name matches. If no
wildcard entry is provided, connections with unrecognized server names
will be rejected with a TLS `unrecognized_name` alert. Each entry may
contain:

* `keys` {KeyObject|KeyObject\[]} The TLS private keys. **Required.**
* `certs` {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}
  The TLS certificates. **Required.**
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

#### `sessionOptions.reuseEndpoint`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}
* Default: `true`

When `true` (the default), `connect()` will attempt to reuse an existing
endpoint rather than creating a new one for each session. This provides
connection pooling behavior — multiple sessions can share a single UDP
socket. The reuse logic will not return an endpoint that is listening on
the same address as the connect target (to prevent CID routing conflicts).

Set to `false` to force creation of a new endpoint for the session. This
is useful when endpoint isolation is required (e.g., testing stateless
reset behavior where source port identity matters).

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

* Type: {net.SocketAddress} The preferred IPv4 address to advertise (only
  used by servers).

#### `transportParams.preferredAddressIpv6`

<!-- YAML
added: v23.8.0
-->

* Type: {net.SocketAddress} The preferred IPv6 address to advertise (only
  used by servers)

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

### Callback error handling

All session and stream callbacks may be synchronous functions or async
functions. If a callback throws synchronously or returns a promise that
rejects, the error is caught and the owning session or stream is destroyed
with that error:

* Stream callbacks (`onblocked`, `onreset`, `onheaders`, `ontrailers`,
  `oninfo`, `onwanttrailers`): the stream is destroyed.
* Session callbacks (`onstream`, `ondatagram`, `ondatagramstatus`,
  `onpathvalidation`, `onsessionticket`, `onnewtoken`,
  `onversionnegotiation`, `onorigin`, `ongoaway`, `onhandshake`,
  `onkeylog`, `onqlog`): the session is destroyed along with all of its
  streams.

Before destruction, the optional [`session.onerror`][] or
[`stream.onerror`][] callback is invoked (if set), giving the application a
chance to observe or log the error. The `session.closed` or `stream.closed`
promise will reject with the error.

If the `onerror` callback itself throws or returns a promise that rejects,
the error from `onerror` is surfaced as an uncaught exception.

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
* `status` {string} One of `'acknowledged'`, `'lost'`, or `'abandoned'`.
  `'acknowledged'` means the peer confirmed receipt. `'lost'` means the
  datagram was sent but the network lost it. `'abandoned'` means the
  datagram was never sent on the wire (dropped due to queue overflow,
  send attempt limit exceeded, or frame size rejection).

### Callback: `OnPathValidationCallback`

<!-- YAML
added: v23.8.0
-->

* `this` {quic.QuicSession}
* `result` {string} One of either `'success'`, `'failure'`, or `'aborted'`.
* `newLocalAddress` {net.SocketAddress} The local address of the validated path.
* `newRemoteAddress` {net.SocketAddress} The remote address of the validated path.
* `oldLocalAddress` {net.SocketAddress | null} The local address of the previous
  path, or `null` if this is the first path validation (e.g., preferred address
  migration from the client's perspective).
* `oldRemoteAddress` {net.SocketAddress | null} The remote address of the previous
  path, or `null`.
* `preferredAddress` {boolean} `true` if the path validation was triggered by
  a preferred address migration on the client side. `undefined` on the server side.

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
* `version` {number} The QUIC version that was configured for this session
  (the version that the server did not support).
* `requestedVersions` {number\[]} The versions advertised by the server in
  the Version Negotiation packet. These are the versions the server supports.
* `supportedVersions` {number\[]} The versions supported locally, expressed
  as a two-element array `[minVersion, maxVersion]`.

Called when the server responds to the client's Initial packet with a
Version Negotiation packet, indicating that the version used by the client
is not supported. The session is always destroyed immediately after this
callback returns.

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

### Callback: `OnKeylogCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `line` {string} A single line of [NSS Key Log Format][] text, including
  a trailing newline character.

Called when TLS key material is available. Only fires when
[`sessionOptions.keylog`][] is `true`. Multiple lines are emitted during the
TLS 1.3 handshake, each containing a secret label, the client random, and
the secret value.

### Callback: `OnQlogCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `data` {string} A chunk of [JSON-SEQ][] formatted [qlog][] data.
* `fin` {boolean} `true` if this is the final qlog chunk for the session.

Called when qlog diagnostic data is available. Only fires when
[`sessionOptions.qlog`][] is `true`. The `data` chunks should be
concatenated in order to produce the complete qlog output. When `fin` is
`true`, no more chunks will be emitted and the concatenated result is a
complete JSON-SEQ document.

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

Called when initial request or response headers are received. For HTTP/3,
this delivers request pseudo-headers on the server and response headers
on the client.

### Callback: `OnTrailersCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `trailers` {Object} Trailing header object.

Called when trailing headers are received from the peer.

### Callback: `OnInfoCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `headers` {Object} Informational header object.

Called when informational (1xx) headers are received from the server
(e.g., 103 Early Hints).

## HTTP/3 support

<!-- YAML
added: REPLACEME
-->

When the negotiated ALPN identifier is `'h3'` (or one of the `'h3-*'`
draft variants), the QUIC session runs the HTTP/3 application backed
by `nghttp3`. `'h3'` is the default ALPN for `quic.connect()` and
`quic.listen()`, so HTTP/3 is what you get unless you select a
different ALPN explicitly.

Selecting the HTTP/3 application enables a number of stream- and
session-level capabilities that are not available to non-HTTP/3
applications:

* **Headers and trailers** — request and response header blocks
  (including pseudo-headers such as `:method`, `:path`, `:scheme`,
  `:authority`, and `:status`), trailing headers, and informational
  (`1xx`) responses. See [`stream.sendHeaders()`][],
  [`stream.sendTrailers()`][], and
  [`stream.sendInformationalHeaders()`][].
* **Stream priority (RFC 9218)** — per-stream urgency and
  incremental flags. See [`stream.priority`][] and
  [`stream.setPriority()`][].
* **HTTP/3 datagrams (RFC 9297)** — unreliable application-layer
  datagrams. The peer must advertise `SETTINGS_H3_DATAGRAM=1`, which
  is enabled by setting [`application.enableDatagrams`][] to `true`
  on both peers. See [`session.sendDatagram()`][] and
  [`session.ondatagram`][].
* **ORIGIN frame (RFC 9412)** — servers automatically advertise the
  hostnames in their [`sessionOptions.sni`][] map (entries with
  `authoritative: true`); clients receive the list via
  [`session.onorigin`][].
* **GOAWAY** — graceful shutdown. The server emits `GOAWAY` as part
  of [`session.close()`][]; the client observes it via
  [`session.ongoaway`][] and stops opening new bidirectional streams.
* **Extended CONNECT settings (RFC 9220)** — the
  `SETTINGS_ENABLE_CONNECT_PROTOCOL` setting can be enabled via
  [`application.enableConnectProtocol`][]. The setting is exchanged
  but the application is responsible for handling the `:protocol`
  pseudo-header and any payload framing on top.
* **QPACK tuning** — dynamic-table size and blocked-streams limits
  via [`application.qpackMaxDTableCapacity`][] and friends.

### Minimal HTTP/3 client

```mjs
import { connect } from 'node:quic';
import process from 'node:process';

const session = await connect('example.com:443', {
  // ALPN defaults to 'h3'.
  servername: 'example.com',
});
await session.opened;

const stream = await session.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'example.com',
  },
  onheaders(headers) {
    console.log('status:', headers[':status']);
  },
});

const decoder = new TextDecoder();
for await (const chunk of stream) {
  process.stdout.write(decoder.decode(chunk, { stream: true }));
}

await session.close();
```

A few things to note:

* `session.createBidirectionalStream({ headers })` automatically
  marks the HEADERS frame as terminal when no `body` is provided —
  the request is `HEADERS` followed by `END_STREAM`.
* The `onheaders` callback receives the response pseudo-headers and
  regular headers in a single object with lowercase string keys.
  After the callback returns, the same object is also accessible
  via [`stream.headers`][].
* Reading `for await (const chunk of stream)` consumes the response
  body as `Uint8Array` chunks.
* HTTP semantic helpers (URL parsing, method/status validation,
  redirects, content negotiation, and so on) are intentionally not
  built in. The caller is responsible for any HTTP-level handling
  beyond the wire framing.

### Minimal HTTP/3 server

```mjs
import { listen } from 'node:quic';

const encoder = new TextEncoder();

const endpoint = await listen((session) => {
  // The session.onstream callback fires for each new client-initiated stream.
}, {
  sni: { '*': { keys: [defaultKey], certs: [defaultCert] } },
  // ALPN defaults to 'h3'.
  onheaders(headers) {
    // `this` is the QuicStream. Pseudo-headers are available on the
    // request header block (`:method`, `:path`, `:scheme`,
    // `:authority`).
    if (headers[':path'] === '/health') {
      this.sendHeaders({ ':status': '200', 'content-type': 'text/plain' });
      const w = this.writer;
      w.writeSync(encoder.encode('ok\n'));
      w.endSync();
    } else {
      this.sendHeaders({ ':status': '404' }, { terminal: true });
    }
  },
});

console.log('listening on', endpoint.address);
```

Server-side notes:

* Setting `onheaders` at the [`listen()`][`quic.listen()`] level
  applies it to every incoming stream (it is wired up before
  `onstream` fires). Setting it inside `onstream` is too late for
  HTTP/3, where the request HEADERS frame is the first thing that
  arrives on the stream.
* `this.sendHeaders(headers, { terminal: true })` marks the
  response HEADERS frame as terminal (no body follows).
* For body responses, send headers first, then write to
  `this.writer` and call `endSync()` to send the body and close the
  stream cleanly.

### What is not implemented

* **Server push** — `PUSH_PROMISE` and the related push-stream
  machinery are not implemented and are not on the near-term
  roadmap. Server push has limited deployment in practice, and most
  use cases are better served by Early Hints (`103`) or by direct
  fetches from the client.
* **WebTransport / extended-CONNECT helpers** — the
  `SETTINGS_ENABLE_CONNECT_PROTOCOL` setting can be negotiated but
  there is no built-in support for the `:protocol` pseudo-header,
  WebTransport datagram demultiplexing, or capsule framing.
* **Higher-level HTTP semantics** — there is no built-in
  request/response router, URL parsing, content-encoding
  negotiation, body-type coercion, redirect following, or
  cookie handling. These are deliberately left to higher-level
  libraries built on top of `node:quic`.

## Performance measurement

<!-- YAML
added: REPLACEME
-->

QUIC sessions, streams, and endpoints emit [`PerformanceEntry`][] objects
with `entryType` set to `'quic'`. These entries are only created when a
[`PerformanceObserver`][] is observing the `'quic'` entry type, ensuring
zero overhead when not in use.

Each entry provides:

* `name` {string} One of `'QuicEndpoint'`, `'QuicSession'`, or `'QuicStream'`.
* `entryType` {string} Always `'quic'`.
* `startTime` {number} High-resolution timestamp (ms) when the object was created.
* `duration` {number} Lifetime in milliseconds from creation to destruction.
* `detail` {Object} Entry-specific metadata (see below).

### `QuicEndpoint` entries

* `detail.stats` {QuicEndpointStats} The endpoint's statistics object
  (frozen at destruction time).

### `QuicSession` entries

* `detail.stats` {QuicSessionStats} The session's statistics object
  (frozen at destruction time). Includes bytes sent/received, RTT
  measurements, congestion window, packet counts, and more.
* `detail.handshake` {Object|undefined} Timing-relevant handshake metadata,
  or `undefined` if the handshake did not complete before destruction.
  * `servername` {string} The negotiated SNI server name.
  * `protocol` {string} The negotiated ALPN protocol.
  * `earlyDataAttempted` {boolean} Whether 0-RTT early data was attempted.
  * `earlyDataAccepted` {boolean} Whether 0-RTT early data was accepted.
* `detail.path` {Object|undefined} The session's network path, or
  `undefined` if not yet established.
  * `local` {net.SocketAddress}
  * `remote` {net.SocketAddress}

### `QuicStream` entries

* `detail.stats` {QuicStreamStats} The stream's statistics object
  (frozen at destruction time). Includes bytes sent/received, timing
  timestamps, and offset tracking.
* `detail.direction` {string} Either `'bidi'` or `'uni'`.

### Example

```mjs
import { PerformanceObserver } from 'node:perf_hooks';

const obs = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    console.log(`${entry.name}: ${entry.duration.toFixed(1)}ms`);
    if (entry.name === 'QuicSession') {
      const { stats, handshake } = entry.detail;
      console.log(`  protocol: ${handshake?.protocol}`);
      console.log(`  bytes sent: ${stats.bytesSent}`);
      console.log(`  smoothed RTT: ${stats.smoothedRtt}ns`);
    }
  }
});
obs.observe({ entryTypes: ['quic'] });
```

## Diagnostic Channels

### Channel: `quic.endpoint.created`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `config` {quic.EndpointOptions}

Published when a new endpoint is created.

### Channel: `quic.endpoint.listen`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `options` {quic.SessionOptions}

Published when an endpoint begins listening for incoming connections.

### Channel: `quic.endpoint.connect`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `address` {net.SocketAddress} The target server address.
* `options` {quic.SessionOptions}

Published when [`quic.connect()`][] is about to create a client session.
Fires before the ngtcp2 connection is established, allowing diagnostic
subscribers to observe the connection intent.

### Channel: `quic.endpoint.closing`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `hasPendingError` {boolean}

Published when an endpoint begins gracefully closing.

### Channel: `quic.endpoint.closed`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `stats` {quic.QuicEndpoint.Stats} Final endpoint statistics.

Published when an endpoint has finished closing and is destroyed.

### Channel: `quic.endpoint.error`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `error` {any}

Published when an endpoint encounters an error that causes it to close.

### Channel: `quic.endpoint.busy.change`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `busy` {boolean}

Published when an endpoint's busy state changes.

### Channel: `quic.session.created.client`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `session` {quic.QuicSession}
* `address` {net.SocketAddress} The remote server address.
* `options` {quic.SessionOptions}

Published when a client-initiated session is created.

### Channel: `quic.session.created.server`

<!-- YAML
added: v23.8.0
-->

* `endpoint` {quic.QuicEndpoint}
* `session` {quic.QuicSession}
* `address` {net.SocketAddress|undefined} The remote peer address.

Published when a server-side session is created for an incoming connection.

### Channel: `quic.session.open.stream`

<!-- YAML
added: v23.8.0
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `direction` {string} Either `'bidi'` or `'uni'`.

Published when a locally-initiated stream is opened.

### Channel: `quic.session.received.stream`

<!-- YAML
added: v23.8.0
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `direction` {string} Either `'bidi'` or `'uni'`.

Published when a remotely-initiated stream is received.

### Channel: `quic.session.send.datagram`

<!-- YAML
added: v23.8.0
-->

* `id` {bigint} The datagram ID.
* `length` {number} The datagram payload size in bytes.
* `session` {quic.QuicSession}

Published when a datagram is queued for sending.

### Channel: `quic.session.update.key`

<!-- YAML
added: v23.8.0
-->

* `session` {quic.QuicSession}

Published when a TLS key update is initiated.

### Channel: `quic.session.closing`

<!-- YAML
added: v23.8.0
-->

* `session` {quic.QuicSession}

Published when a session begins gracefully closing (including when a
GOAWAY frame is received from the peer).

### Channel: `quic.session.closed`

<!-- YAML
added: v23.8.0
-->

* `session` {quic.QuicSession}
* `error` {any} The error that caused the close, or `undefined` if clean.
* `stats` {quic.QuicSession.Stats} Final session statistics.

Published when a session is destroyed. The `stats` object is a snapshot
of the final statistics at the time of destruction.

### Channel: `quic.session.error`

<!-- YAML
added: REPLACEME
-->

* `session` {quic.QuicSession}
* `error` {any} The error that caused the session to be destroyed.

Published when a session is destroyed due to an error. Fires before the
`onerror` callback and before streams are torn down. Unlike
`quic.session.closed` (which fires for both clean and error closes), this
channel fires only when an error is present, making it suitable for
error-only alerting.

### Channel: `quic.session.receive.datagram`

<!-- YAML
added: v23.8.0
-->

* `length` {number} The datagram payload size in bytes.
* `early` {boolean} Whether the datagram was received as 0-RTT early data.
* `session` {quic.QuicSession}

Published when a datagram is received from the remote peer.

### Channel: `quic.session.receive.datagram.status`

<!-- YAML
added: v23.8.0
-->

* `id` {bigint} The datagram ID.
* `status` {string} One of `'acknowledged'`, `'lost'`, or `'abandoned'`.
* `session` {quic.QuicSession}

Published when the delivery status of a sent datagram is updated.

### Channel: `quic.session.path.validation`

<!-- YAML
added: v23.8.0
-->

* `result` {string} One of `'success'`, `'failure'`, or `'aborted'`.
* `newLocalAddress` {net.SocketAddress}
* `newRemoteAddress` {net.SocketAddress}
* `oldLocalAddress` {net.SocketAddress|null}
* `oldRemoteAddress` {net.SocketAddress|null}
* `preferredAddress` {boolean}
* `session` {quic.QuicSession}

Published when a path validation attempt completes.

### Channel: `quic.session.new.token`

<!-- YAML
added: REPLACEME
-->

* `token` {Buffer} The NEW\_TOKEN token data.
* `address` {net.SocketAddress} The remote server address.
* `session` {quic.QuicSession}

Published when a client session receives a NEW\_TOKEN frame from the
server.

### Channel: `quic.session.ticket`

<!-- YAML
added: v23.8.0
-->

* `ticket` {Object} The opaque session ticket.
* `session` {quic.QuicSession}

Published when a new TLS session ticket is received.

### Channel: `quic.session.version.negotiation`

<!-- YAML
added: v23.8.0
-->

* `version` {number} The QUIC version that was configured for this session.
* `requestedVersions` {number\[]} The versions advertised by the server.
* `supportedVersions` {number\[]} The versions supported locally.
* `session` {quic.QuicSession}

Published when the client receives a Version Negotiation packet from the
server. The session is always destroyed immediately after.

### Channel: `quic.session.receive.origin`

<!-- YAML
added: REPLACEME
-->

* `origins` {string\[]} The list of origins the server is authoritative for.
* `session` {quic.QuicSession}

Published when the session receives an ORIGIN frame (RFC 9412) from
the peer.

### Channel: `quic.session.handshake`

<!-- YAML
added: v23.8.0
-->

* `session` {quic.QuicSession}
* `servername` {string}
* `protocol` {string}
* `cipher` {string}
* `cipherVersion` {string}
* `validationErrorReason` {string}
* `validationErrorCode` {number}
* `earlyDataAttempted` {boolean}
* `earlyDataAccepted` {boolean}

Published when the TLS handshake completes.

### Channel: `quic.session.goaway`

<!-- YAML
added: REPLACEME
-->

* `session` {quic.QuicSession}
* `lastStreamId` {bigint} The highest stream ID the peer may have processed.

Published when the peer sends an HTTP/3 GOAWAY frame. Streams with IDs
above `lastStreamId` were not processed and can be retried on a new
connection. A `lastStreamId` of `-1n` indicates a shutdown notice without
a stream boundary.

### Channel: `quic.session.early.rejected`

<!-- YAML
added: REPLACEME
-->

* `session` {quic.QuicSession}

Published when the server rejects 0-RTT early data. All streams that were
opened during the 0-RTT phase have been destroyed. Useful for diagnosing
latency regressions when 0-RTT is expected to succeed.

### Channel: `quic.stream.closed`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `error` {any} The error that caused the close, or `undefined` if clean.
* `stats` {quic.QuicStream.Stats} Final stream statistics.

Published when a stream is destroyed. The `stats` object is a snapshot
of the final statistics at the time of destruction.

### Channel: `quic.stream.headers`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `headers` {Object} The initial request or response headers.

Published when initial headers are received on a stream. For HTTP/3
server-side streams, this contains request pseudo-headers (`:method`,
`:path`, etc.). For client-side streams, this contains response headers
(`:status`, etc.).

### Channel: `quic.stream.trailers`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `trailers` {Object} The trailing headers.

Published when trailing headers are received on a stream.

### Channel: `quic.stream.info`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `headers` {Object} The informational headers.

Published when informational (1xx) headers are received on a stream
(e.g., 103 Early Hints).

### Channel: `quic.stream.reset`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}
* `error` {any} The QUIC error associated with the reset.

Published when a stream receives a STOP\_SENDING or RESET\_STREAM frame
from the peer, indicating the peer has aborted the stream. This is a
key signal for diagnosing application-level issues such as cancelled
requests.

### Channel: `quic.stream.blocked`

<!-- YAML
added: REPLACEME
-->

* `stream` {quic.QuicStream}
* `session` {quic.QuicSession}

Published when a stream is flow-control blocked and cannot send data
until the peer increases the flow control window. Useful for diagnosing
throughput issues caused by flow control.

[Aborting a stream]: #aborting-a-stream
[Callback error handling]: #callback-error-handling
[JSON-SEQ]: https://www.rfc-editor.org/rfc/rfc7464
[NSS Key Log Format]: https://udn.realityripple.com/docs/Mozilla/Projects/NSS/Key_Log_Format
[`PerformanceEntry`]: perf_hooks.md#class-performanceentry
[`PerformanceObserver`]: perf_hooks.md#class-performanceobserver
[`QuicError`]: #class-quicerror
[`application.enableConnectProtocol`]: #sessionoptionsapplication
[`application.enableDatagrams`]: #sessionoptionsapplication
[`application.qpackMaxDTableCapacity`]: #sessionoptionsapplication
[`endpoint.maxConnectionsPerHost`]: #endpointmaxconnectionsperhost
[`endpoint.maxConnectionsTotal`]: #endpointmaxconnectionstotal
[`error.errorCode`]: #errorerrorcode
[`fs.promises.open(path, 'r')`]: fs.md#fspromisesopenpath-flags-mode
[`quic.connect()`]: #quicconnectaddress-options
[`quic.listen()`]: #quiclistencallback-options
[`session.close()`]: #sessioncloseoptions
[`session.destroy()`]: #sessiondestroyerror-options
[`session.maxPendingDatagrams`]: #sessionmaxpendingdatagrams
[`session.ondatagram`]: #sessionondatagram
[`session.onerror`]: #sessiononerror
[`session.ongoaway`]: #sessionongoaway
[`session.onkeylog`]: #sessiononkeylog
[`session.onnewtoken`]: #sessiononnewtoken
[`session.onorigin`]: #sessiononorigin
[`session.onqlog`]: #sessiononqlog
[`session.sendDatagram()`]: #sessionsenddatagramdatagram-encoding
[`sessionOptions.datagramDropPolicy`]: #sessionoptionsdatagramdroppolicy
[`sessionOptions.keylog`]: #sessionoptionskeylog
[`sessionOptions.qlog`]: #sessionoptionsqlog
[`sessionOptions.sni`]: #sessionoptionssni-server-only
[`stream.destroy()`]: #streamdestroyerror-options
[`stream.headers`]: #streamheaders
[`stream.onerror`]: #streamonerror
[`stream.onwanttrailers`]: #streamonwanttrailers
[`stream.pendingTrailers`]: #streampendingtrailers
[`stream.priority`]: #streampriority
[`stream.sendHeaders()`]: #streamsendheadersheaders-options
[`stream.sendInformationalHeaders()`]: #streamsendinformationalheadersheaders
[`stream.sendTrailers()`]: #streamsendtrailersheaders
[`stream.setBody()`]: #streamsetbodybody
[`stream.setPriority()`]: #streamsetpriorityoptions
[`stream.writer`]: #streamwriter
[`writer.fail()`]: #streamwriter
[`writer.fail(reason)`]: #streamwriter
[qlog]: https://datatracker.ietf.org/doc/draft-ietf-quic-qlog-main-schema/
[qvis]: https://qvis.quictools.info/
