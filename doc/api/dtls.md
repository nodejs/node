# DTLS

<!-- YAML
added: REPLACEME
-->

<!-- introduced_in=REPLACEME -->

> Stability: 1 - Experimental

<!-- source_link=lib/dtls.js -->

The `node:dtls` module provides an implementation of the Datagram Transport
Layer Security (DTLS) protocol over UDP. DTLS provides TLS-equivalent
security guarantees for datagram-based communication, including
confidentiality, integrity, and authentication.

To use this module, it must be enabled at build time with the
`--experimental-dtls` configure flag and at runtime with the
`--experimental-dtls` CLI flag.

```bash
node --experimental-dtls app.mjs
```

```mjs
import { listen, connect } from 'node:dtls';
```

```cjs
const { listen, connect } = require('node:dtls');
```

## Permission model

When using the [Permission Model][], the `--allow-net` flag must be passed to
allow DTLS network operations. Without it, calling [`dtls.connect()`][] or
[`dtls.listen()`][] will throw an `ERR_ACCESS_DENIED` error.

```console
node --permission --allow-fs-read=* --experimental-dtls index.mjs
Error: Access to this API has been restricted. Use --allow-net to manage permissions.
  code: 'ERR_ACCESS_DENIED',
  permission: 'Net',
}
```

Creating a [`DTLSEndpoint`][] instance without connecting or listening
is permitted even without `--allow-net`, since no network I/O occurs until
[`dtls.connect()`][] or [`dtls.listen()`][] is called.

## DTLS vs TLS

DTLS is designed for UDP transport and differs from TLS in several key ways:

* No stream guarantees: Messages may arrive out of order or be lost.
  DTLS preserves datagram semantics.
* One socket, many peers: A single UDP socket can serve multiple DTLS
  sessions. The `DTLSEndpoint` manages this multiplexing.
* Cookie exchange: DTLS servers use a stateless cookie mechanism
  (HelloVerifyRequest) to prevent denial-of-service amplification attacks.
* Retransmission: DTLS handles handshake retransmission internally since
  UDP does not guarantee delivery.

## `dtls.listen(callback, options)`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function} Called for each new DTLS session accepted by the
  server.
  * `session` {DTLSSession} The new session.
* `options` {Object}
  * `cert` {string|Buffer} Server certificate in PEM format. **Required.**
  * `key` {string|Buffer} Server private key in PEM format. **Required.**
  * `port` {number} Port to bind to. **Required.**
  * `host` {string} Address to bind to. **Default:** `'0.0.0.0'`.
  * `ca` {string|Buffer|string\[]|Buffer\[]} CA certificates in PEM format.
  * `ciphers` {string} OpenSSL cipher list string.
  * `alpn` {string\[]|Buffer} ALPN protocol names.
  * `srtp` {string} Colon-separated SRTP protection profile names
    (e.g., `'SRTP_AES128_CM_SHA1_80:SRTP_AEAD_AES_128_GCM'`).
  * `requestCert` {boolean} Request client certificate. **Default:** `false`.
  * `mtu` {number} Maximum transmission unit for DTLS records.
    **Default:** `1200`.
* Returns: {DTLSEndpoint}

Creates a DTLS server bound to the specified address and port. The server
uses automatic HMAC-based cookie exchange for DoS protection.

```mjs
import { listen } from 'node:dtls';
import { readFileSync } from 'node:fs';

const endpoint = listen((session) => {
  session.onmessage = (data) => {
    console.log('Received:', data.toString());
    session.send('pong');
  };

  session.onhandshake = (protocol) => {
    console.log('Handshake complete:', protocol);
  };
}, {
  cert: readFileSync('server-cert.pem'),
  key: readFileSync('server-key.pem'),
  port: 4433,
});

console.log('DTLS server listening on', endpoint.address);
```

## `dtls.connect(host, port[, options])`

<!-- YAML
added: REPLACEME
-->

* `host` {string} Remote host to connect to.
* `port` {number} Remote port to connect to.
* `options` {Object}
  * `ca` {string|Buffer|string\[]|Buffer\[]} CA certificates in PEM format.
  * `cert` {string|Buffer} Client certificate in PEM format.
  * `key` {string|Buffer} Client private key in PEM format.
  * `rejectUnauthorized` {boolean} Reject connections with unverifiable
    certificates. **Default:** `true`.
  * `bindHost` {string} Local bind address. **Default:** `'0.0.0.0'`.
  * `bindPort` {number} Local bind port. **Default:** `0` (ephemeral).
  * `alpn` {string\[]|Buffer} ALPN protocol names.
  * `srtp` {string} SRTP protection profile names.
  * `mtu` {number} Maximum transmission unit. **Default:** `1200`.
* Returns: {DTLSSession}

Connects to a DTLS server. Returns a `DTLSSession` whose `opened` property
is a `Promise` that resolves when the handshake completes.

```mjs
import { connect } from 'node:dtls';
import { readFileSync } from 'node:fs';

const session = connect('localhost', 4433, {
  ca: [readFileSync('ca-cert.pem')],
});

await session.opened;
session.send('hello');

session.onmessage = (data) => {
  console.log('Received:', data.toString());
};
```

## Class: `DTLSEndpoint`

<!-- YAML
added: REPLACEME
-->

Manages a UDP socket and multiplexes DTLS sessions.

### `endpoint.address`

* Returns: {Object} `{ address, family, port }`

The local address the endpoint is bound to.

### `endpoint.state`

* Returns: {DTLSEndpointState}

Shared state object with properties:

* `bound` {boolean}
* `listening` {boolean}
* `closing` {boolean}
* `destroyed` {boolean}
* `sessionCount` {number}
* `busy` {boolean}

### `endpoint.stats`

<!-- YAML
added: REPLACEME
-->

* Type: {DTLSEndpoint.Stats}

The statistics collected for this endpoint. Read only. The stats object is
live and updated by the C++ internals as data flows through the endpoint.

### `endpoint.busy`

* {boolean}

When `true`, the endpoint rejects new incoming connections. Can be set
to implement backpressure.

### `endpoint.close()`

* Returns: {Promise} Resolves when the endpoint is fully closed.

Gracefully closes the endpoint. All active sessions are closed with
`close_notify` alerts before the UDP socket is released.

### `endpoint.destroy([error])`

Immediately destroys the endpoint without sending `close_notify` alerts.

### `endpoint.closed`

* {Promise} Resolves when the endpoint has fully closed.

### `endpoint[Symbol.asyncDispose]()`

Equivalent to calling `endpoint.close()`.

## Class: `DTLSEndpoint.Stats`

<!-- YAML
added: REPLACEME
-->

A view of the collected statistics for an endpoint.

### `endpointStats.createdAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when the endpoint was created. Read only.

### `endpointStats.destroyedAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when the endpoint was destroyed. Read only.

### `endpointStats.bytesReceived`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of bytes received by this endpoint. Read only.

### `endpointStats.bytesSent`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of bytes sent by this endpoint. Read only.

### `endpointStats.packetsReceived`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of UDP packets received by this endpoint. Read only.

### `endpointStats.packetsSent`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of UDP packets sent by this endpoint. Read only.

### `endpointStats.serverSessions`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of peer-initiated sessions accepted by this
  endpoint. Read only.

### `endpointStats.clientSessions`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of sessions initiated by this endpoint. Read only.

### `endpointStats.serverBusyCount`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of incoming connections rejected because the
  endpoint was marked busy. Read only.

### `endpointStats.isConnected`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the stats object is still connected to the underlying endpoint.
Once the endpoint is destroyed, the stats become a stale snapshot.

## Class: `DTLSSession`

<!-- YAML
added: REPLACEME
-->

Represents a DTLS association with a single remote peer.

### `session.send(data)`

* `data` {string|Buffer} The data to send.
* Returns: {number} The number of bytes written to the DTLS layer.

Send application data to the peer. The data is encrypted by DTLS before
being sent over UDP. Can only be called after the handshake completes
(`session.opened` has resolved).

### `session.close()`

* Returns: {Promise} Resolves when the session is closed.

Initiates a graceful DTLS shutdown by sending a `close_notify` alert.

### `session.destroy([error])`

Immediately destroys the session without sending `close_notify`.

### `session.opened`

* {Promise} Resolves with `{ protocol }` when the DTLS handshake completes.

### `session.closed`

* {Promise} Resolves when the session is fully closed.

### `session.remoteAddress`

* Returns: {Object} `{ address, family, port }`

### `session.protocol`

* Returns: {string} The negotiated DTLS protocol version
  (e.g., `'DTLSv1.2'`).

### `session.cipher`

* Returns: {Object} `{ name, standardName, version }`

### `session.peerCertificate`

* Returns: {string|undefined} The peer's certificate in PEM format.

### `session.alpnProtocol`

* Returns: {string|undefined} The negotiated ALPN protocol.

### `session.srtpProfile`

* Returns: {string|undefined} The negotiated SRTP protection profile name.

### `session.stats`

<!-- YAML
added: REPLACEME
-->

* Type: {DTLSSession.Stats}

The statistics collected for this session. Read only. The stats object is
live and updated as data flows through the session.

### `session.exportKeyingMaterial(length, label[, context])`

* `length` {number} Number of bytes to export.
* `label` {string} The label for the exported keying material.
* `context` {Buffer} Optional context value.
* Returns: {Buffer}

Exports keying material from the DTLS session, as defined in
[RFC 5705][]. This is commonly used with DTLS-SRTP to derive
encryption keys for media streams.

## Class: `DTLSSession.Stats`

<!-- YAML
added: REPLACEME
-->

A view of the collected statistics for a session.

### `sessionStats.createdAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when the session was created. Read only.

### `sessionStats.destroyedAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when the session was destroyed. Read only.

### `sessionStats.closingAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when `close()` was called. Read only.

### `sessionStats.handshakeCompletedAt`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} A timestamp indicating when the DTLS handshake completed. Read only.

### `sessionStats.bytesReceived`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of application data bytes received. Read only.

### `sessionStats.bytesSent`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of application data bytes sent. Read only.

### `sessionStats.messagesReceived`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of application messages received. Read only.

### `sessionStats.messagesSent`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of application messages sent. Read only.

### `sessionStats.retransmitCount`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The total number of DTLS handshake retransmissions. Read only.

### `sessionStats.isConnected`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the stats object is still connected to the underlying session.
Once the session is destroyed, the stats become a stale snapshot.

### Callback properties

#### `session.onmessage`

* {Function}
  * `data` {Buffer}

Set to receive application data from the peer.

#### `session.onerror`

* {Function}
  * `error` {Error}

Set to receive error notifications.

#### `session.onhandshake`

* {Function}
  * `protocol` {string}

Set to receive handshake completion notifications.

#### `session.onkeylog`

* {Function}
  * `line` {string}

Set to receive TLS key log lines (for debugging with Wireshark).

### `session[Symbol.asyncDispose]()`

Equivalent to calling `session.close()`.

## DTLS-SRTP example

DTLS-SRTP is used by WebRTC for media encryption. The DTLS handshake
negotiates the SRTP protection profile and provides keying material.

```mjs
import { listen, connect } from 'node:dtls';
import { readFileSync } from 'node:fs';

// Server with SRTP
const server = listen((session) => {
  session.onhandshake = () => {
    console.log('SRTP profile:', session.srtpProfile);
    const keys = session.exportKeyingMaterial(
      60,
      'EXTRACTOR-dtls_srtp',
    );
    console.log('SRTP keying material:', keys);
  };
}, {
  cert: readFileSync('server-cert.pem'),
  key: readFileSync('server-key.pem'),
  port: 5004,
  srtp: 'SRTP_AES128_CM_SHA1_80:SRTP_AEAD_AES_128_GCM',
});

// Client with SRTP
const session = connect('localhost', 5004, {
  rejectUnauthorized: false,
  srtp: 'SRTP_AEAD_AES_128_GCM:SRTP_AES128_CM_SHA1_80',
});

await session.opened;
console.log('Negotiated SRTP:', session.srtpProfile);
const keys = session.exportKeyingMaterial(60, 'EXTRACTOR-dtls_srtp');
```

## MTU considerations

Since libuv does not currently support path MTU discovery, the DTLS module
uses a conservative default MTU of 1200 bytes. This value works across
virtually all network paths but may be suboptimal for local networks.

The MTU can be configured via the `mtu` option:

```mjs
// For a local network where you know the path MTU
const endpoint = listen(callback, {
  // ...
  mtu: 1400,
});
```

The minimum allowed MTU is 256 bytes. The maximum is 65535.

[Permission Model]: permissions.md#permission-model
[RFC 5705]: https://www.rfc-editor.org/rfc/rfc5705
[`DTLSEndpoint`]: #class-dtlsendpoint
[`dtls.connect()`]: #dtlsconnecthost-port-options
[`dtls.listen()`]: #dtlslistencallback-options
