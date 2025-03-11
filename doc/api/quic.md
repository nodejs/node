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

* {net.SocketAddress|undefined}

The local UDP socket address to which the endpoint is bound, if any.

If the endpoint is not currently bound then the value will be `undefined`. Read only.

### `endpoint.busy`

<!-- YAML
added: v23.8.0
-->

* {boolean}

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

* {Promise}

A promise that is fulfilled when the endpoint is destroyed. This will be the same promise that is
returned by the `endpoint.close()` function. Read only.

### `endpoint.closing`

<!-- YAML
added: v23.8.0
-->

* {boolean}

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

* {boolean}

True if `endpoint.destroy()` has been called. Read only.

### `endpoint.stats`

<!-- YAML
added: v23.8.0
-->

* {quic.QuicEndpoint.Stats}

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

* {bigint} A timestamp indicating the moment the endpoint was created. Read only.

### `endpointStats.destroyedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint} A timestamp indicating the moment the endpoint was destroyed. Read only.

### `endpointStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of bytes received by this endpoint. Read only.

### `endpointStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of bytes sent by this endpoint. Read only.

### `endpointStats.packetsReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of QUIC packets successfully received by this endpoint. Read only.

### `endpointStats.packetsSent`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of QUIC packets successfully sent by this endpoint. Read only.

### `endpointStats.serverSessions`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of peer-initiated sessions received by this endpoint. Read only.

### `endpointStats.clientSessions`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of sessions initiated by this endpoint. Read only.

### `endpointStats.serverBusyCount`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of times an initial packet was rejected due to the
  endpoint being marked busy. Read only.

### `endpointStats.retryCount`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of QUIC retry attempts on this endpoint. Read only.

### `endpointStats.versionNegotiationCount`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number sessions rejected due to QUIC version mismatch. Read only.

### `endpointStats.statelessResetCount`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of stateless resets handled by this endpoint. Read only.

### `endpointStats.immediateCloseCount`

<!-- YAML
added: v23.8.0
-->

* {bigint} The total number of sessions that were closed before handshake completed. Read only.

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

### `session.closed`

<!-- YAML
added: v23.8.0
-->

* {Promise}

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

* {boolean}

True if `session.destroy()` has been called. Read only.

### `session.endpoint`

<!-- YAML
added: v23.8.0
-->

* {quic.QuicEndpoint}

The endpoint that created this session. Read only.

### `session.onstream`

<!-- YAML
added: v23.8.0
-->

* {quic.OnStreamCallback}

The callback to invoke when a new stream is initiated by a remote peer. Read/write.

### `session.ondatagram`

<!-- YAML
added: v23.8.0
-->

* {quic.OnDatagramCallback}

The callback to invoke when a new datagram is received from a remote peer. Read/write.

### `session.ondatagramstatus`

<!-- YAML
added: v23.8.0
-->

* {quic.OnDatagramStatusCallback}

The callback to invoke when the status of a datagram is updated. Read/write.

### `session.onpathvalidation`

<!-- YAML
added: v23.8.0
-->

* {quic.OnPathValidationCallback}

The callback to invoke when the path validation is updated. Read/write.

### `seesion.onsessionticket`

<!-- YAML
added: v23.8.0
-->

* {quic.OnSessionTicketCallback}

The callback to invoke when a new session ticket is received. Read/write.

### `session.onversionnegotiation`

<!-- YAML
added: v23.8.0
-->

* {quic.OnVersionNegotiationCallback}

The callback to invoke when a version negotiation is initiated. Read/write.

### `session.onhandshake`

<!-- YAML
added: v23.8.0
-->

* {quic.OnHandshakeCallback}

The callback to invoke when the TLS handshake is completed. Read/write.

### `session.createBidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {ArrayBuffer | ArrayBufferView | Blob}
  * `sendOrder` {number}
* Returns: {Promise} for a {quic.QuicStream}

Open a new bidirectional stream. If the `body` option is not specified,
the outgoing stream will be half-closed.

### `session.createUnidirectionalStream([options])`

<!-- YAML
added: v23.8.0
-->

* `options` {Object}
  * `body` {ArrayBuffer | ArrayBufferView | Blob}
  * `sendOrder` {number}
* Returns: {Promise} for a {quic.QuicStream}

Open a new unidirectional stream. If the `body` option is not specified,
the outgoing stream will be closed.

### `session.path`

<!-- YAML
added: v23.8.0
-->

* {Object|undefined}
  * `local` {net.SocketAddress}
  * `remote` {net.SocketAddress}

The local and remote socket addresses associated with the session. Read only.

### `session.sendDatagram(datagram)`

<!-- YAML
added: v23.8.0
-->

* `datagram` {string|ArrayBufferView}
* Returns: {bigint}

Sends an unreliable datagram to the remote peer, returning the datagram ID.
If the datagram payload is specified as an `ArrayBufferView`, then ownership of
that view will be transfered to the underlying stream.

### `session.stats`

<!-- YAML
added: v23.8.0
-->

* {quic.QuicSession.Stats}

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

* {bigint}

### `sessionStats.closingAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.handshakeCompletedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.handshakeConfirmedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.bidiInStreamCount`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.bidiOutStreamCount`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.uniInStreamCount`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.uniOutStreamCount`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.maxBytesInFlights`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.bytesInFlight`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.blockCount`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.cwnd`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.latestRtt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.minRtt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.rttVar`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.smoothedRtt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.ssthresh`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.datagramsReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.datagramsSent`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.datagramsAcknowledged`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `sessionStats.datagramsLost`

<!-- YAML
added: v23.8.0
-->

* {bigint}

## Class: `QuicStream`

<!-- YAML
added: v23.8.0
-->

### `stream.closed`

<!-- YAML
added: v23.8.0
-->

* {Promise}

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

* {boolean}

True if `stream.destroy()` has been called.

### `stream.direction`

<!-- YAML
added: v23.8.0
-->

* {string} One of either `'bidi'` or `'uni'`.

The directionality of the stream. Read only.

### `stream.id`

<!-- YAML
added: v23.8.0
-->

* {bigint}

The stream ID. Read only.

### `stream.onblocked`

<!-- YAML
added: v23.8.0
-->

* {quic.OnBlockedCallback}

The callback to invoke when the stream is blocked. Read/write.

### `stream.onreset`

<!-- YAML
added: v23.8.0
-->

* {quic.OnStreamErrorCallback}

The callback to invoke when the stream is reset. Read/write.

### `stream.readable`

<!-- YAML
added: v23.8.0
-->

* {ReadableStream}

### `stream.session`

<!-- YAML
added: v23.8.0
-->

* {quic.QuicSession}

The session that created this stream. Read only.

### `stream.stats`

<!-- YAML
added: v23.8.0
-->

* {quic.QuicStream.Stats}

The current statistics for the stream. Read only.

## Class: `QuicStream.Stats`

<!-- YAML
added: v23.8.0
-->

### `streamStats.ackedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.bytesReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.bytesSent`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.createdAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.destroyedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.finalSize`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.isConnected`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.maxOffset`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.maxOffsetAcknowledged`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.maxOffsetReceived`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.openedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

### `streamStats.receivedAt`

<!-- YAML
added: v23.8.0
-->

* {bigint}

## Types

### Type: `EndpointOptions`

<!-- YAML
added: v23.8.0
-->

* {Object}

The endpoint configuration options passed when constructing a new `QuicEndpoint` instance.

#### `endpointOptions.address`

<!-- YAML
added: v23.8.0
-->

* {net.SocketAddress | string} The local UDP address and port the endpoint should bind to.

If not specified the endpoint will bind to IPv4 `localhost` on a random port.

#### `endpointOptions.addressLRUSize`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

The endpoint maintains an internal cache of validated socket addresses as a
performance optimization. This option sets the maximum number of addresses
that are cache. This is an advanced option that users typically won't have
need to specify.

#### `endpointOptions.ipv6Only`

<!-- YAML
added: v23.8.0
-->

* {boolean}

When `true`, indicates that the endpoint should bind only to IPv6 addresses.

#### `endpointOptions.maxConnectionsPerHost`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum number of concurrent sessions allowed per remote peer address.

#### `endpointOptions.maxConnectionsTotal`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum total number of concurrent sessions.

#### `endpointOptions.maxRetries`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum number of QUIC retry attempts allowed per remote peer address.

#### `endpointOptions.maxStatelessResetsPerHost`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum number of stateless resets that are allowed per remote peer address.

#### `endpointOptions.retryTokenExpiration`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the length of time a QUIC retry token is considered valid.

#### `endpointOptions.resetTokenSecret`

<!-- YAML
added: v23.8.0
-->

* {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC retry tokens.

#### `endpointOptions.tokenExpiration`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the length of time a QUIC token is considered valid.

#### `endpointOptions.tokenSecret`

<!-- YAML
added: v23.8.0
-->

* {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC tokens.

#### `endpointOptions.udpReceiveBufferSize`

<!-- YAML
added: v23.8.0
-->

* {number}

#### `endpointOptions.udpSendBufferSize`

<!-- YAML
added: v23.8.0
-->

* {number}

#### `endpointOptions.udpTTL`

<!-- YAML
added: v23.8.0
-->

* {number}

#### `endpointOptions.validateAddress`

<!-- YAML
added: v23.8.0
-->

* {boolean}

When `true`, requires that the endpoint validate peer addresses using retry packets
while establishing a new connection.

### Type: `SessionOptions`

<!-- YAML
added: v23.8.0
-->

#### `sessionOptions.alpn`

<!-- YAML
added: v23.8.0
-->

* {string}

The ALPN protocol identifier.

#### `sessionOptions.ca`

<!-- YAML
added: v23.8.0
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CA certificates to use for sessions.

#### `sessionOptions.cc`

<!-- YAML
added: v23.8.0
-->

* {string}

Specifies the congestion control algorithm that will be used
. Must be set to one of either `'reno'`, `'cubic'`, or `'bbr'`.

This is an advanced option that users typically won't have need to specify.

#### `sessionOptions.certs`

<!-- YAML
added: v23.8.0
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The TLS certificates to use for sessions.

#### `sessionOptions.ciphers`

<!-- YAML
added: v23.8.0
-->

* {string}

The list of supported TLS 1.3 cipher algorithms.

#### `sessionOptions.crl`

<!-- YAML
added: v23.8.0
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CRL to use for sessions.

#### `sessionOptions.groups`

<!-- YAML
added: v23.8.0
-->

* {string}

The list of support TLS 1.3 cipher groups.

#### `sessionOptions.keylog`

<!-- YAML
added: v23.8.0
-->

* {boolean}

True to enable TLS keylogging output.

#### `sessionOptions.keys`

<!-- YAML
added: v23.8.0
-->

* {KeyObject|CryptoKey|KeyObject\[]|CryptoKey\[]}

The TLS crypto keys to use for sessions.

#### `sessionOptions.maxPayloadSize`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum UDP packet payload size.

#### `sessionOptions.maxStreamWindow`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum stream flow-control window size.

#### `sessionOptions.maxWindow`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maxumum session flow-control window size.

#### `sessionOptions.minVersion`

<!-- YAML
added: v23.8.0
-->

* {number}

The minimum QUIC version number to allow. This is an advanced option that users
typically won't have need to specify.

#### `sessionOptions.preferredAddressPolicy`

<!-- YAML
added: v23.8.0
-->

* {string} One of `'use'`, `'ignore'`, or `'default'`.

When the remote peer advertises a preferred address, this option specifies whether
to use it or ignore it.

#### `sessionOptions.qlog`

<!-- YAML
added: v23.8.0
-->

* {boolean}

True if qlog output should be enabled.

#### `sessionOptions.sessionTicket`

<!-- YAML
added: v23.8.0
-->

* {ArrayBufferView} A session ticket to use for 0RTT session resumption.

#### `sessionOptions.handshakeTimeout`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum number of milliseconds a TLS handshake is permitted to take
to complete before timing out.

#### `sessionOptions.sni`

<!-- YAML
added: v23.8.0
-->

* {string}

The peer server name to target.

#### `sessionOptions.tlsTrace`

<!-- YAML
added: v23.8.0
-->

* {boolean}

True to enable TLS tracing output.

#### `sessionOptions.transportParams`

<!-- YAML
added: v23.8.0
-->

* {quic.TransportParams}

The QUIC transport parameters to use for the session.

#### `sessionOptions.unacknowledgedPacketThreshold`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

Specifies the maximum number of unacknowledged packets a session should allow.

#### `sessionOptions.verifyClient`

<!-- YAML
added: v23.8.0
-->

* {boolean}

True to require verification of TLS client certificate.

#### `sessionOptions.verifyPrivateKey`

<!-- YAML
added: v23.8.0
-->

* {boolean}

True to require private key verification.

#### `sessionOptions.version`

<!-- YAML
added: v23.8.0
-->

* {number}

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

* {net.SocketAddress} The preferred IPv4 address to advertise.

#### `transportParams.preferredAddressIpv6`

<!-- YAML
added: v23.8.0
-->

* {net.SocketAddress} The preferred IPv6 address to advertise.

#### `transportParams.initialMaxStreamDataBidiLocal`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.initialMaxStreamDataBidiRemote`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.initialMaxStreamDataUni`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.initialMaxData`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.initialMaxStreamsBidi`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.initialMaxStreamsUni`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.maxIdleTimeout`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.activeConnectionIDLimit`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.ackDelayExponent`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.maxAckDelay`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

#### `transportParams.maxDatagramFrameSize`

<!-- YAML
added: v23.8.0
-->

* {bigint|number}

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
* `earlyDataAccepted` {boolean}

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
