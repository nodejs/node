# QUIC

<!-- introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
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

## Class: `QuicEndpoint`

A `QuicEndpoint` encapsulates the local UDP-port binding for QUIC. It can be
used as both a client and a server.

```mjs
import { QuicEndpoint } from 'node:quic';

const endpoint = new QuicEndpoint();

// Server...
endpoint.listen((session) => {
  session.onstream = (stream) => {
    // Handle the stream....
  };
});

// Client...
const client = endpoint.connect('123.123.123.123:8888');
const stream = client.openBidirectionalStream();
```

### `new QuicEndpoint([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {quic.EndpointOptions}

### `endpoint.address`

<!-- YAML
added: REPLACEME
-->

* {net.SocketAddress|undefined}

The local UDP socket address to which the endpoint is bound, if any.

If the endpoint is not currently bound then the value will be `undefined`. Read only.

### `endpoint.busy`

<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* Returns: {Promise}

Gracefully close the endpoint. The endpoint will close and destroy itself when
all currently open sessions close. Once called, new sessions will be rejected.

Returns a promise that is fulfilled when the endpoint is destroyed.

### `endpoint.closed`

<!-- YAML
added: REPLACEME
-->

* {Promise}

A promise that is fulfilled when the endpoint is destroyed. This will be the same promise that is
returned by the `endpoint.close()` function. Read only.

### `endpoint.connect(address[, options])`

<!-- YAML
added: REPLACEME
-->

* `address` {string|net.SocketAddress}
* `options` {quic.SessionOptions}
* Returns: {quic.QuicSession}

Initiate a new client-side session using this endpoint.

### `endpoint.destroy([error])`

<!-- YAML
added: REPLACEME
-->

* `error` {any}
* Returns: {Promise}

Forcefully closes the endpoint by forcing all open sessions to be immediately
closed. Returns `endpoint.closed`.

### `endpoint.destroyed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True if `endpoint.destroy()` has been called. Read only.

### `endpoint.listen([onsession,][options])`

<!-- YAML
added: REPLACEME
-->

* `onsession` {quic.OnSessionCallback}
* `options` {quic.SessionOptions}

Configures the endpoint to listen as a server. When a new session is initiated by
a remote peer, the given `onsession` callback will be invoked with the created
session.

The `onsession` callback must be specified either here or by setting the `onsession`
property or an error will be thrown.

### `endpoint.onsession`

* {quic.OnSessionCallback}

The callback function that is invoked when a new session is initiated by a remote peer.
Read/write.

### `endpoint.sessions`

<!-- YAML
added: REPLACEME
-->

* {Iterator} of {quic.QuicSession}.

An iterator over all sessions associated with this endpoint. Read only.

### `endpoint.state`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicEndpointState}

The state associated with an active session. Read only.

### `endpoint.stats`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicEndpointStats}

The statistics collected for an active session. Read only.

### `endpoint[Symbol.asyncDispose]()`

<!-- YAML
added: REPLACEME
-->

Calls `endpoint.close()` and returns a promise that fulfills when the
endpoint has closed.

## Class: `QuicEndpointState`

<!-- YAML
added: REPLACEME
-->

A view of the internal state of an endpoint.

### `endpointState.isBound`

<!-- YAML
added: REPLACEME
-->

* {boolean} True if the endpoint is bound to a local UDP port.

### `endpointState.isReceiving`

<!-- YAML
added: REPLACEME
-->

* {boolean} True if the endpoint is bound to a local UDP port and actively listening
  for packets.

### `endpointState.isListening`

<!-- YAML
added: REPLACEME
-->

* {boolean} True if the endpoint is listening as a server.

### `endpointState.isClosing`

<!-- YAML
added: REPLACEME
-->

* {boolean} True if the endpoint is in the process of closing down.

### `endpointState.isBusy`

<!-- YAML
added: REPLACEME
-->

* {boolean} True if the endpoint has been marked busy.

### `endpointState.pendingCallbacks`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of pending callbacks the endpoint is waiting on currently.

## Class: `QuicEndpointStats`

<!-- YAML
added: REPLACEME
-->

A view of the collected statistics for an endpoint.

### `endpointStats.createdAt`

<!-- YAML
added: REPLACEME
-->

* {bigint} A timestamp indicating the moment the endpoint was created. Read only.

### `endpointStats.destroyedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint} A timestamp indicating the moment the endpoint was destroyed. Read only.

### `endpointStats.bytesReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of bytes received by this endpoint. Read only.

### `endpointStats.bytesSent`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of bytes sent by this endpoint. Read only.

### `endpointStats.packetsReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of QUIC packets successfully received by this endpoint. Read only.

### `endpointStats.packetsSent`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of QUIC packets successfully sent by this endpoint. Read only.

### `endpointStats.serverSessions`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of peer-initiated sessions received by this endpoint. Read only.

### `endpointStats.clientSessions`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of sessions initiated by this endpoint. Read only.

### `endpointStats.serverBusyCount`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of times an initial packet was rejected due to the
  endpoint being marked busy. Read only.

### `endpointStats.retryCount`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of QUIC retry attempts on this endpoint. Read only.

### `endpointStats.versionNegotiationCount`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number sessions rejected due to QUIC version mismatch. Read only.

### `endpointStats.statelessResetCount`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of stateless resets handled by this endpoint. Read only.

### `endpointStats.immediateCloseCount`

<!-- YAML
added: REPLACEME
-->

* {bigint} The total number of sessions that were closed before handshake completed. Read only.

## Class: `QuicSession`

<!-- YAML
added: REPLACEME
-->

### `session.close()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Promise}

### `session.closed`

<!-- YAML
added: REPLACEME
-->

* {Promise}

### `session.destroy([error])`

<!-- YAML
added: REPLACEME
-->

* `error` {any}
* Returns: {Promise}

### `session.destroyed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `session.endpoint`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicEndpoint}

### `session.onstream`

<!-- YAML
added: REPLACEME
-->

* {quic.OnStreamCallback}

### `session.ondatagram`

<!-- YAML
added: REPLACEME
-->

* {quic.OnDatagramCallback}

### `session.ondatagramstatus`

<!-- YAML
added: REPLACEME
-->

* {quic.OnDatagramStatusCallback}

### `session.onpathvalidation`

<!-- YAML
added: REPLACEME
-->

* {quic.OnPathValidationCallback}

### `seesion.onsessionticket`

<!-- YAML
added: REPLACEME
-->

* {quic.OnSessionTicketCallback}

### `session.onversionnegotiation`

<!-- YAML
added: REPLACEME
-->

* {quic.OnVersionNegotiationCallback}

### `session.onhandshake`

<!-- YAML
added: REPLACEME
-->

* {quic.OnHandshakeCallback}

### `session.openBidirectionalStream([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `headers` {Object}
* Returns: {quic.QuicStream}

### `session.openUnidirectionalStream([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `headers` {Object
* Returns: {quic.QuicStream}

### `session.path`

<!-- YAML
added: REPLACEME
-->

* {Object|undefined}
  * `local` {net.SocketAddress}
  * `remote` {net.SocketAddress}

### `session.sendDatagram(datagram)`

<!-- YAML
added: REPLACEME
-->

* `datagram` {Uint8Array}
* Returns: {bigint}

### `session.state`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicSessionState}

### `session.stats`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicSessionStats}

### `session.updateKey()`

<!-- YAML
added: REPLACEME
-->

### `session[Symbol.asyncDispose]()`

<!-- YAML
added: REPLACEME
-->

## Class: `QuicSessionState`

<!-- YAML
added: REPLACEME
-->

### `sessionState.hasPathValidationListener`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.hasVersionNegotiationListener`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.hasDatagramListener`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.hasSessionTicketListener`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isClosing`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isGracefulClose`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isSilentClose`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isStatelessReset`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isDestroyed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isHandshakeCompleted`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isHandshakeConfirmed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isStreamOpenAllowed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isPrioritySupported`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.isWrapped`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `sessionState.lastDatagramId`

<!-- YAML
added: REPLACEME
-->

* {bigint}

## Class: `QuicSessionStats`

<!-- YAML
added: REPLACEME
-->

### `sessionStats.createdAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.closingAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.destroyedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.handshakeCompletedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.handshakeConfirmedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.gracefulClosingAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.bytesReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.bytesSent`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.bidiInStreamCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.bidiOutStreamCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.uniInStreamCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.uniOutStreamCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.lossRetransmitCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.maxBytesInFlights`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.bytesInFlight`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.blockCount`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.cwnd`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.latestRtt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.minRtt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.rttVar`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.smoothedRtt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.ssthresh`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.datagramsReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.datagramsSent`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.datagramsAcknowledged`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `sessionStats.datagramsLost`

<!-- YAML
added: REPLACEME
-->

* {bigint}

## Class: `QuicStream`

<!-- YAML
added: REPLACEME
-->

### `stream.closed`

<!-- YAML
added: REPLACEME
-->

* {Promise}

### `stream.destroy([error])`

<!-- YAML
added: REPLACEME
-->

* `error` {any}
* Returns: {Promise}

### `stream.destroyed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `stream.direction`

<!-- YAML
added: REPLACEME
-->

* {string} One of either `'bidi'` or `'uni'`.

### `stream.id`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `stream.onblocked`

<!-- YAML
added: REPLACEME
-->

* {quic.OnBlockedCallback}

### `stream.onheaders`

<!-- YAML
added: REPLACEME
-->

* {quic.OnHeadersCallback}

### `stream.onreset`

<!-- YAML
added: REPLACEME
-->

* {quic.OnStreamErrorCallback}

### `stream.ontrailers`

<!-- YAML
added: REPLACEME
-->

* {quic.OnTrailersCallback}

### `stream.pull(callback)`

<!-- YAML
added: REPLACEME
-->

* `callback` {quic.OnPullCallback}

### `stream.sendHeaders(headers)`

<!-- YAML
added: REPLACEME
-->

* `headers` {Object}

### `stream.session`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicSession}

### `stream.state`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicStreamState}

### `stream.stats`

<!-- YAML
added: REPLACEME
-->

* {quic.QuicStreamStats}

## Class: `QuicStreamState`

<!-- YAML
added: REPLACEME
-->

### `streamState.destroyed`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.finReceived`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.finSent`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.hasReader`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.id`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamState.paused`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.pending`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.readEnded`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.reset`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.wantsBlock`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.wantsHeaders`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.wantsReset`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.wantsTrailers`

<!-- YAML
added: REPLACEME
-->

* {boolean}

### `streamState.writeEnded`

<!-- YAML
added: REPLACEME
-->

* {boolean}

## Class: `QuicStreamStats`

<!-- YAML
added: REPLACEME
-->

### `streamStats.ackedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.bytesReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.bytesSent`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.createdAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.destroyedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.finalSize`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.isConnected`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.maxOffset`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.maxOffsetAcknowledged`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.maxOffsetReceived`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.openedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

### `streamStats.receivedAt`

<!-- YAML
added: REPLACEME
-->

* {bigint}

## Types

### Type: `ApplicationOptions`

<!-- YAML
added: REPLACEME
-->

#### `applicationOptions.maxHeaderPairs`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The maximum number of header pairs that can be received.

#### `applicationOptions.maxHeaderLength`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The maximum number of header bytes that can be received.

#### `applicationOptions.maxFieldSectionSize`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The maximum header field section size.

#### `applicationOptions.qpackMaxDTableCapacity`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The QPack maximum dynamic table capacity.

#### `applicationOptions.qpackEncoderMaxDTableCapacity`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The QPack encoder maximum dynamic table capacity.

#### `applicationOptions.qpackBlockedStreams`

<!-- YAML
added: REPLACEME
-->

* {bigint|number} The maximum number of QPack blocked streams.

#### `applicationOptions.enableConnectProtocol`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to allow use of the `CONNECT` method when using HTTP/3.

#### `applicationOptions.enableDatagrams`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to allow use of unreliable datagrams.

### Type: `EndpointOptions`

<!-- YAML
added: REPLACEME
-->

* {Object}

#### `endpointOptions.address`

<!-- YAML
added: REPLACEME
-->

* {net.SocketAddress | string} The local UDP address and port the endpoint should bind to.

If not specified the endpoint will bind to IPv4 `localhost` on a random port.

#### `endpointOptions.addressLRUSize`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

The endpoint maintains an internal cache of validated socket addresses as a
performance optimization. This option sets the maximum number of addresses
that are cache. This is an advanced option that users typically won't have
need to specify.

#### `endpointOptions.cc`

<!-- YAML
added: REPLACEME
-->

* {string|number}

Specifies the congestion control algorithm that will be used by all sessions
using this endpoint. Must be set to one of:

* `QuicEndpoint.CC_ALGO_RENO`
* `QuicEndpoint.CC_ALGP_RENO_STR`
* `QuicEndpoint.CC_ALGO_CUBIC`
* `QuicEndpoint.CC_ALGO_CUBIC_STR`
* `QuicEndpoint.CC_ALGO_BBR`
* `QuicEndpoint.CC_ALGO_BBR_STR`

This is an advanced option that users typically won't have need to specify
unless.

#### `endpointOptions.disableActiveMigration`

<!-- YAML
added: REPLACEME
-->

* {boolean}

When `true`, this option disables the ability for a session to migrate to a different
socket address.

\*\* THIS OPTION IS AT RISK OF BEING DROPPED \*\*

#### `endpointOptions.handshakeTimeout`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum number of milliseconds a TLS handshake is permitted to take
to complete before timing out.

#### `endpointOptions.ipv6Only`

<!-- YAML
added: REPLACEME
-->

* {boolean}

When `true`, indicates that the endpoint should bind only to IPv6 addresses.

#### `endpointOptions.maxConnectionsPerHost`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum number of concurrent sessions allowed per remote peer address.

#### `endpointOptions.maxConnectionsTotal`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum total number of concurrent sessions.

#### `endpointOptions.maxPayloadSize`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum UDP packet payload size.

#### `endpointOptions.maxRetries`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum number of QUIC retry attempts allowed per remote peer address.

#### `endpointOptions.maxStatelessResetsPerHost`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum number of stateless resets that are allowed per remote peer address.

#### `endpointOptions.maxStreamWindow`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum stream flow-control window size.

#### `endpointOptions.maxWindow`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maxumum session flow-control window size.

#### `endpointOptions.noUdpPayloadSizeShaping`

<!-- YAML
added: REPLACEME
-->

* {boolean}

#### `endpointOptions.retryTokenExpiration`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the length of time a QUIC retry token is considered valid.

#### `endpointOptions.resetTokenSecret`

<!-- YAML
added: REPLACEME
-->

* {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC retry tokens.

#### `endpointOptions.tokenExpiration`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the length of time a QUIC token is considered valid.

#### `endpointOptions.tokenSecret`

<!-- YAML
added: REPLACEME
-->

* {ArrayBufferView}

Specifies the 16-byte secret used to generate QUIC tokens.

#### `endpointOptions.udpReceiveBufferSize`

<!-- YAML
added: REPLACEME
-->

* {number}

#### `endpointOptions.udpSendBufferSize`

<!-- YAML
added: REPLACEME
-->

* {number}

#### `endpointOptions.udpTTL`

<!-- YAML
added: REPLACEME
-->

* {number}

#### `endpointOptions.unacknowledgedPacketThreshold`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

Specifies the maximum number of unacknowledged packets a session should allow.

#### `endpointOptions.validateAddress`

<!-- YAML
added: REPLACEME
-->

* {boolean}

When `true`, requires that the endpoint validate peer addresses while establishing
a connection.

### Type: `SessionOptions`

<!-- YAML
added: REPLACEME
-->

#### `sessionOptions.application`

<!-- YAML
added: REPLACEME
-->

* {quic.ApplicationOptions}

The application-level options to use for the session.

#### `sessionOptions.minVersion`

<!-- YAML
added: REPLACEME
-->

* {number}

The minimum QUIC version number to allow. This is an advanced option that users
typically won't have need to specify.

#### `sessionOptions.preferredAddressPolicy`

<!-- YAML
added: REPLACEME
-->

* {string} One of `'use'`, `'ignore'`, or `'default'`.

When the remote peer advertises a preferred address, this option specifies whether
to use it or ignore it.

#### `sessionOptions.qlog`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True if qlog output should be enabled.

#### `sessionOptions.sessionTicket`

<!-- YAML
added: REPLACEME
-->

* {ArrayBufferView} A session ticket to use for 0RTT session resumption.

#### `sessionOptions.tls`

<!-- YAML
added: REPLACEME
-->

* {quic.TlsOptions}

The TLS options to use for the session.

#### `sessionOptions.transportParams`

<!-- YAML
added: REPLACEME
-->

* {quic.TransportParams}

The QUIC transport parameters to use for the session.

#### `sessionOptions.version`

<!-- YAML
added: REPLACEME
-->

* {number}

The QUIC version number to use. This is an advanced option that users typically
won't have need to specify.

### Type: `TlsOptions`

<!-- YAML
added: REPLACEME
-->

#### `tlsOptions.sni`

<!-- YAML
added: REPLACEME
-->

* {string}

The peer server name to target.

#### `tlsOptions.alpn`

<!-- YAML
added: REPLACEME
-->

* {string}

The ALPN protocol identifier.

#### `tlsOptions.ciphers`

<!-- YAML
added: REPLACEME
-->

* {string}

The list of supported TLS 1.3 cipher algorithms.

#### `tlsOptions.groups`

<!-- YAML
added: REPLACEME
-->

* {string}

The list of support TLS 1.3 cipher groups.

#### `tlsOptions.keylog`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to enable TLS keylogging output.

#### `tlsOptions.verifyClient`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to require verification of TLS client certificate.

#### `tlsOptions.tlsTrace`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to enable TLS tracing output.

#### `tlsOptions.verifyPrivateKey`

<!-- YAML
added: REPLACEME
-->

* {boolean}

True to require private key verification.

#### `tlsOptions.keys`

<!-- YAML
added: REPLACEME
-->

* {KeyObject|CryptoKey|KeyObject\[]|CryptoKey\[]}

The TLS crypto keys to use for sessions.

#### `tlsOptions.certs`

<!-- YAML
added: REPLACEME
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The TLS certificates to use for sessions.

#### `tlsOptions.ca`

<!-- YAML
added: REPLACEME
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CA certificates to use for sessions.

#### `tlsOptions.crl`

<!-- YAML
added: REPLACEME
-->

* {ArrayBuffer|ArrayBufferView|ArrayBuffer\[]|ArrayBufferView\[]}

The CRL to use for sessions.

### Type: `TransportParams`

<!-- YAML
added: REPLACEME
-->

#### `transportParams.preferredAddressIpv4`

<!-- YAML
added: REPLACEME
-->

* {net.SocketAddress} The preferred IPv4 address to advertise.

#### `transportParams.preferredAddressIpv6`

<!-- YAML
added: REPLACEME
-->

* {net.SocketAddress} The preferred IPv6 address to advertise.

#### `transportParams.initialMaxStreamDataBidiLocal`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.initialMaxStreamDataBidiRemote`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.initialMaxStreamDataUni`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.initialMaxData`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.initialMaxStreamsBidi`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.initialMaxStreamsUni`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.maxIdleTimeout`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.activeConnectionIDLimit`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.ackDelayExponent`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.maxAckDelay`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.maxDatagramFrameSize`

<!-- YAML
added: REPLACEME
-->

* {bigint|number}

#### `transportParams.disableActiveMigration`

<!-- YAML
added: REPLACEME
-->

* {boolean}

## Callbacks

### Callback: `OnSessionCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicEndpoint}
* `session` {quic.QuicSession}

The callback function that is invoked when a new session is initiated by a remote peer.

### Callback: `OnStreamCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `stream` {quic.QuicStream}

### Callback: `OnDatagramCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `datagram` {Uint8Array}
* `early` {boolean}

### Callback: `OnDatagramStatusCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `id` {bigint}
* `status` {string} One of either `'lost'` or `'acknowledged'`.

### Callback: `OnPathValidationCallback`

<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `ticket` {Object}

### Callback: `OnVersionNegotiationCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicSession}
* `version` {number}
* `requestedVersions` {number\[]}
* `supportedVersions` {number\[]}

### Callback: `OnHandshakeCallback`

<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* `this` {quic.QuicStream}

### Callback: `OnStreamErrorCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `error` {any}

### Callback: `OnHeadersCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}
* `headers` {Object}
* `kind` {string}

### Callback: `OnTrailersCallback`

<!-- YAML
added: REPLACEME
-->

* `this` {quic.QuicStream}

### Callback: `OnPullCallback`

<!-- YAML
added: REPLACEME
-->

* `chunks` {Uint8Array\[]}

## Diagnostic Channels

### Channel: `quic.endpoint.created`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `config` {quic.EndpointOptions}

### Channel: `quic.endpoint.listen`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `optoins` {quic.SessionOptions}

### Channel: `quic.endpoint.closing`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `hasPendingError` {boolean}

### Channel: `quic.endpoint.closed`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}

### Channel: `quic.endpoint.error`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `error` {any}

### Channel: `quic.endpoint.busy.change`

<!-- YAML
added: REPLACEME
-->

* `endpoint` {quic.QuicEndpoint}
* `busy` {boolean}

### Channel: `quic.session.created.client`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.created.server`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.open.stream`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.received.stream`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.send.datagram`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.update.key`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.closing`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.closed`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.receive.datagram`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.receive.datagram.status`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.path.validation`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.ticket`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.version.negotiation`

<!-- YAML
added: REPLACEME
-->

### Channel: `quic.session.handshake`

<!-- YAML
added: REPLACEME
-->
