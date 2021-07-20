# QUIC

## Overview

The `net/quic` module provides client and server implementations of the QUIC
and HTTP/3 protocols.

```mjs
import {
  Endpoint,
} from 'node:net/quic';
```

```cjs
const {
  Endpoint,
} = require('net/quic');
```

The {quic.Endpoint} object is the primary entrypoint for creating
both clients and servers.

### A simple QUIC Server

```mjs
import {
  Endpoint,
} from 'node:net/quic';

const server = new Endpoint();

server.onsession = ({ session }) => {
  session.onstream = ({ stream, respondWith }) => {
    respondWith({ body: 'Hello World' });
  };
};

server.listen({ alpn: 'xyz' });
```

```cjs
const {
  Endpoint,
} = require('net/quic');

const server = new Endpoint();

server.onsession = ({ session }) => {
  session.onstream = ({ stream, respondWith }) => {
    respondWith({ body: 'Hello World' });
  };
};

server.listen({ alpn: 'xyz' });
```

### A simple QUIC Client

```mjs
import {
  Endpoint,
} from 'node:net/quic';

const client = new Endpoint();
const session = client.connect({
  address: '123.123.123.123',
  port: 1234,
}, { alpn: 'xyz' });
const stream = session.open({ body: 'Hello World' });
console.log(await stream.text());
```

```cjs
const {
  Endpoint,
} = require('net/quic');

async function sayHi() {
  const client = new Endpoint();
  const session = client.connect({
    address: '123.123.123.123',
    port: 1234,
  }, { alpn: 'xyz' });
  const stream = session.open({ body: 'Hello World' });
  console.log(await stream.text());
}

sayHi();
```

### HTTP/3

#### A simple HTTP/3 Server

```mjs
import {
  Endpoint,
} from 'node:net/quic';

import { stdout } from 'node:process';

const endpoint = new Endpoint({ address: { port: 12345 } });

endpoint.onsession = ({ session }) => {
  session.onstream = async ({ stream, respondWith }) => {
    respondWith({
      headers: { 'content-type': 'text/plain' },
      body: 'right back at you',
    });

    console.log(await stream.headers);

    stream.readableNodeStream().pipe(stdout);
  };
};

endpoint.listen({
  secure: {
    key: getTLSKeySomehow(),
    cert: getTLSCertSomehow(),
  },
});
```

#### A simple HTTP/3 Client

```mjs
import {
  Endpoint,
} from 'node:net/quic';

import { stdout } from 'node:process';

const client = new Endpoint();

const req = client.connect({
  address: '127.0.0.1',
  port: 12345
}, {
  hostname: 'localhost',
});

await req.handshake;

const stream = req.open({
  headers: { ':method': 'POST' },
  body: 'hello there',
});

console.log(await stream.headers);

stream.readableNodeStream().pipe(stdout);

await client.close();
```

## API

### Events

#### Class: `DatagramEvent`

* Extends: {Event}

##### `datagramEvent.datagram`

* Type: {ArrayBuffer}

##### `datagramEvent.session`

* Type: {quic.Session}

#### Class: `SessionEvent`

* Extends: {Event}

The `SessionEvent` is emitted by {quic.Endpoint} objects when a new server
`Session` has been created.

##### `sessionEvent.name`

* Type: {string} `'session'`

##### `sessionEvent.session`

* Type: {quic.Session}

The server {quic.Session} that has been created.

#### Class: `StreamEvent`

* Events: {Event}

The `StreamEvent` is emitted by a {quic.Session} whenever a peer-initiated
{quic.Stream} has been created.

##### `streamEvent.name`

* Type: {string} `'stream'`

##### `streamEvent.respondWith(options)`

* `options` {quic.ResponseOptionsInit|quic.ResponseOptions|Promise} An object
  that specifies the properties of the response, or a promise that will provide
  that object.

If the {Stream} is bidirectional, the `streamEvent.respondWith()` method can be
used to provide a response payload. If the {Stream} is unidirectional, the
`streamEvent.respondWith` property will be `undefined`.

##### `streamEvent.stream`

* Type: {quic.Stream}

The peer-initiated {Stream} that was created.

### Class: `quic.ClientHello`

When the `sessionConfig.clienthello` option is `true`, the server {quic.Session}
will pause at the start of the TLS handshake and will fulfill the {quic.Session}
object's `session.clienthello` promise with an instance of `quic.ClientHello`.
This object provides information about the client peer's requested ALPN,
ciphers, and target server name. The TLS handshake will not resume until
the `clientHello.done()` method is called.

#### `clientHello.alpn`

* Type: {string}

Specifies the client requested ALPN identifier for this connection.

#### `clientHello.ciphers`

* Type: {Object[]}
  * `name` {string}
  * `standardName` {string}
  * `version` {string}

Lists the TLS 1.3 compatible ciphers the client is requesting for this
connection.

#### `clientHello.done([context])`

* `context` {tls.SecureContext} An optional replacement {tls.SecureContext}
  to use for the TLS handshake.

#### `clientHello.servername`

* Type: {string}

The SNI host name the client is requesting for this connection.

### Class: `quic.Endpoint`

* Extends: {EventTarget}

#### `new quic.Endpoint([options])`

* `options` {quic.EndpointConfigInit|quic.EndpointConfig}

Creates a new {quic.Endpoint} using the given `options` (or using the
defaults if `options` is not provided).

#### Event: `'session'`

The `'session'` event is emitted after a listening endpoint receives a
new initial packet from a remote peer signaling the creation of a new
`Session`.

The callback is invoked with the newly created server {quic.Session}.

The `'session'` event can be handled either by registering a handler
using the standard `EventTarget` `addEventListener()` method:

```js
endpoint.addEventListener('session', (event) => {
  const { session } = event;
  /* ... */
});
```

Or by setting the value of the `onsession` property:

```js
endpoint.onsession = ({ session }) => {
  /* ... */
};
```

The `'session'` event may be emitted multiple times.

The only argument passed to the event handler will be a {quic.SessionEvent}.
object.

#### `endpoint.address`

* Type: {net.SocketAddress} If the `Endpoint` is actively bound to a
  local UDP endpoint, the `endpoint.address` value will be a
  {net.SocketAddress} identifying the local bound IP address and port.
  Otherwise the `endpoint.address` will be `undefined`.

#### `endpoint.close()`

* Returns: {Promise}

Begins a graceful shutdown of the `Endpoint`, allowing any existing
{quic.Session}s to complete before destroying the `Endpoint`.

The returned promise will be fulfilled successfully once the `Endpoint`
is destroyed, or will reject if any error occurs while waiting for
the shutdown to complete.

The graceful shutdown cannot be canceled once it has started.

Once called, all calls to `endpoint.connect()` to create new client sessions
will throw and new inbound initial session packets will be ignored.

#### `endpoint.closed`

* Type: {Promise} A promise that is resolved when the `Endpoint` has been
  closed, or is rejected if the `Endpoint` is abnormally terminated. This
  will be the same promise that is returned by the `endpoint.close()`
  function.

#### `endpoint.closing`

* Type: {boolean} Set to `true` after `endpoint.close()` has been
  called and the `Endpoint` has not yet been destroyed.

#### `endpoint.connect(address[, options[, resume]])`

* `address` {net.SocketAddressInit|net.SocketAddress}
* `options` {quic.SessionConfigInit|quic.SessionConfig}
* `resume` {Blob} A saved TLS session ticket as provided by the
  `session.sessionTicket` property of a previously established {quic.Session}.
* Returns: {quic.Session}

Creates a new client {quic.Session} and begins the TLS handshake
to the identified remote peer.

The `address` argument is either a {net.SocketAddress} or a
{net.SocketAddressInit} object used to create a new {net.SocketAddress}.

#### `endpoint.destroy([error])`

* `error` {Error}

Immediately and synchronously destroys the `Endpoint`. Once destroyed,
the `endpoint.closed` promise will be fulfilled and the `Endpoint` will
no longer be usable.

#### `endpoint.listen(options)`

* `options` {quic.SessionConfigInit|quic.SessionConfig}

Causes the `Endpoint` to begin listening to inbound initial QUIC packets.

#### `endpoint.listening`

* Type: {boolean} Set to `true` after `endpoint.listen()` has been
  called successfully and before the `Endpoint` has been destroyed.

#### `endpoint.stats`

* Type: {quic.EndpointStats}

Returns an object providing access to statistics accumulated for the
`Endpoint`. After the `Endpoint` has been destroyed, the stats provide
the most details available at the time it was destroyed.

#### Cloning `Endpoint` using `MessagePort` or `BroadcastChannel`

The `Endpoint` object is cloneable using the Structured Clone Algorithm.
However, there are a number of details about how the object is cloned that
are important.

When an `Endpoint` is first created, the underlying UDP socket handle is created
and bound to the context and event loop associated with the current thread. A
cloned `Endpoint` may exist in a separate thread with a separate event loop but
will still use the original UDP socket handle in the original event loop to
transmit or receive packets.

<!-- lint disable nodejs-links -->
![Cloned QUIC Endpoints](assets/cloned-quic-endpoint.png)
<!-- lint enable nodejs-links -->

The diagram above illustrates the structure that is created internally within
Node.js for cloned `Endpoint`s. At the JavaScript level, every `Endpoint` is
backed by an internal `node::quic::EndpointWrap` object that exists within the
same context and event loop as the JavaScript object. The
`node::quic::EndpointWrap` is itself backed by a shared `node::quic::Endpoint`
internal object that handles the actual flow of information across the network.

Cloning the JavaScript `Endpoint` object creates a new JavaScript `Endpoint`
object and associated `node::quic::EndpointWrap` that point to the same
underlying `node::quic::Endpoint`.

Each of the JavaScript `Endpoint` objects operate independently of each other,
and may be used independently as either (or both) a QUIC server or client.
Multiple cloned `Endpoint` can listen at the same time, all using the same
underlying UDP port binding. When the underlying shared `node::quic::Endpoint`
receives an initial QUIC packet, it will dispatch that packet to the first
available listening `node::quic::EndpointWrap` in a round-robin pattern. This
is useful for distributing the handling of QUIC sessions across multiple
Node.js worker threads.

### Class: `quic.EndpointConfig`

The `quic.EndpointConfig` object is used to provide configuration options
for a newly created {quic.Endpoint}.

#### Static method: `quic.EndpointConfig.isEndpointConfig(value)`

* `value` {any}

Returns `true` if `value` is an instance of {quic.EndpointConfig}.

#### `new quic.EndpointConfig(options)`

* `options` {quic.EndpointConfigInit}

#### Object: `quic.EndpointConfigInit`

The `quic.EndpointConfigInit` is an ordinary JavaScript object
with properties that are used to configure a new `Endpoint`.

The most commonly used properties are:

* `address` {Object|net.SocketAddress} Identifies the local IPv4 or IPv6
  address the `Endpoint` will bind to.
  * `address` {string} The network address as either an IPv4 or IPv6 string.
    **Default**: `'127.0.0.1'` if `family` is `'ipv4'`; `'::'` if `family` is
    `'ipv6'`.
  * `family` {string} One of either `'ipv4'` or `'ipv6'`.
    **Default**: `'ipv4'`.
  * `flowlabel` {number} An IPv6 flow-label used only if `family` is `'ipv6'`.
  * `port` {number} An IP port.
* `resetTokenSecret` {ArrayBuffer|TypedArray|DataView|Buffer} A secret that
  will be used to generate stateless reset tokens. The reset token secret
  must be exactly 16-bytes long and *must* be kept confidential.

Additional advanced configuration properties are also available.
Most applications should rarely have reason to use the advanced
settings, and they should be used only when necessary:

* `addressLRUSize` {number|bigint} The maximum size of the validated
  address LRU cache maintained by the `Endpoint`. **Default**: `1000`.
* `ccAlgorithm` {string} Specifies the congestion control algorithm that
  will be used. Must be one of either `'cubic'` or `'reno'`. **Default**:
  `'cubic'`.
* `disableStatelessReset` {boolean} Disables the QUIC stateless reset
  mechanism. Disabling stateless resets is an advanced option. It is
  typically not a good idea to disable it. **Default**: `false`.
* `maxConnectionsPerHost` {number|bigint} The maximum number of concurrent
  sessions a remote peer is permitted to maintain. **Default**: `100`.
* `maxConnectionsTotal` {number|bigint} The maximum number of concurrent
  sessions this endpoint is permitted. **Default**: `1000`.
* `maxPayloadSize` {number|bigint} The maximum payload size of QUIC
  packets sent by this `Endpoint`. **Default**: For IPv4, `1252`, for
  IPv6, `1232`.
* `maxStatelessResets` {number|bigint} The maximum number of stateless
  resets this endpoint is permitted to generate per remote peer.
  **Default**: `10`.
* `maxStreamWindowOverride` {number|bigint} The maximum per-stream flow control
  window size. Setting this overrides the automatic congresion control
  mechanism.  **Default**: `undefined`.
* `maxWindowOverride` {number|bigint} The maximum per-session flow control
  window size. Setting this overrides the automatic congestion control
  mechanism.  **Default**: `undefined`.
* `retryLimit` {number|bigint} The maximum number of QUIC retry attempts
  the endpoint will attempt while trying to validate a remote peer.
  **Default**: `10`.
* `retryTokenExpiration` {number|bigint} The maximum amount of time (in seconds)
  a QUIC retry token is considered to be valid. **Default**: 600 seconds
  (10 minutes).
* `rxPacketLoss` {number} A value between `0.0` and `1.0` indicating the
  probability of simulated received packet loss. This should only ever
  be used in testing. **Default**: `0.0`.
* `tokenExpiration` {number|bigint} The maximum amount of time (in seconds)
  a QUIC token (sent via the `NEW_TOKEN` QUIC frame) is considered to be valid.
  **Default**: 600 seconds (10 minutes).
* `txPacketLoss` {number} A value between `0.0` and `1.0` indicating the
  probability of simulated transmitted packet loss. This should only ever
  be used in testing. **Default**: `0.0`
* `udp` {Object} UDP specific options
  * `ipv6Only` {boolean} By default, when the `Endpoint` is configured
    to use an IPv6 address, the `Endpoint` will support "dual-stack"
    mode (supporting both IPv6 and IPv4). When `udp.ipv6Only` is `true`,
    dual-stack support is disabled.
  * `receiveBufferSize` {number} The maximum UDP receive buffer size.
  * `sendBufferSize` {number} The maximum UDP send buffer size.
  * `ttl` {number}
* `unacknowledgedPacketThreshold` {number|bigint} The maximum number of
  unacknowledged packets this `Endpoint` will allow to accumulate before
  sending an acknowledgement to the remote peer. Setting this overrides
  the automatic acknowledgement handling mechanism.
* `validateAddress` {boolean} The QUIC specification requires that each
  endpoint validate the address of received packets. This option allows
  validation to be disabled. Doing so violates the specification. This
  option should only ever be used in testing. **Default**: `false`.

All properties are optional.

### Class: `quic.EndpointStats`

The `quic.EndpointStats` object provides detailed statistical information
about the {quic.Endpoint}. The stats will be actively updated while the
`Endpoint` is active. When the `Endpoint` has is destroyed, the statistics
are captured and frozen as of that moment.

#### `endpointStats.bytesReceived`

* Type: {bigint}

The total number of bytes received by this `Endpoint`.

#### `endpointStats.bytesSent`

* Type: {bigint}

The total number of bytes transmitted by this `Endoint`.

#### `endpointStats.clientSessions`

* Type: {bigint}

The total number of client `Session` instances served by this
`Endpoint`.

#### `endpointStats.createdAt`

* Type: {bigint}

The timestamp at which this `Endpoint` was created.

#### `endpointStats.duration`

* Type: {bigint}

The length of time since `createdAt` this `Endpoint` has been active (or
was active is the `Endpoint` has been destroyed).

#### `endpointStats.packetsReceived`

* Type: {bigint}

The total number of QUIC packets received by this `Endpoint`.

#### `endpointStats.packetsSent`

* Type: {bigint}

The total number of QUIC packets sent by this `Endpoint`.

#### `endpointStats.serverBusyCount`

* Type: {bigint}

The total accumulated number of times this `Endpoint` has sent a
`SERVER_BUSY` response to a peer.

#### `endpointStats.serverSessions`

* Type: {bigint}

The total number of server `Session` instances handled by this `Endpoint`.

#### `endpointStats.statelessResetCount`

* Type: {bigint}

The total number of stateless resets this `Endpoint` has sent.

### Class: `quic.OCSPRequest`

When the `sessionConfig.ocsp` option has been set for a server `Session`,
the `session.ocsp` promise is fulfilled with a `quic.OCSPRequest` object
that provides information about the X.509 certificate for which OCSP status
is being requested. The TLS handshake will be paused until the
`ocspRequest.respondWith()` method is called, providing the OCSP status
response that is to be sent to the client.

#### `ocspRequest.certificate`

* Type: {ArrayBuffer}

#### `ocspRequest.issuer`

* Type: {ArrayBuffer}

#### `ocspRequest.respondWith([response])`

* `response` {string|ArrayBuffer|TypedArray|DataView|Buffer}

### Class: `quic.OCSPResponse`

When the `sessionConfig.ocsp` option has been set for a client `Session`,
the `session.ocsp` promise is fulfilled with a `quic.OCSPResponse` object
when the server has provided a response to the OCSP status query during
the TLS handshake.

#### `ocspResponse.response`

* Type: {ArrayBuffer}

### Class: `quic.ResponseOptions`

The `quic.ResponseOptions` is an object whose properties provide the response
payload for a received bidirectional stream initiated by the remote peer.

#### Static method: `quic.ResponseOptions.isResponseOptions(value)`

* `value` {any}

Returns `true` if `value` is an instance of {quic.ResponseOptions}.

#### `new quic.ResponseOptions([options])`

* `options` {quic.ResponseOptionsInit}

#### Object: `quic.ResponseOptionsInit`

The `quic.ResponseOptionsInit` is an ordinary JavaScript object
with the following properties:

<!--lint disable maximum-line-length remark-lint-->
* `headers` {Object|Promise} If supported by the application protocol, the headers
  to be transmitted at the start of the stream.
* `hints` {Object} If supported by the application protocol, early informational
  headers (or hints) to be transmitted before the initial `headers` at the start
  of the stream.
* `trailers` {Object|Promise} If supported by the application protocol, the trailing
  headers to be transmitted after the stream body.
* `body` {string|ArrayBuffer|TypedArray|DataView|Buffer|stream.Readable|ReadableStream|Blob|Promise|Function}
<!--lint enable maximum-line-length remark-lint-->

All properties are optional.

The `body` provides the outgoing data that is to be transmitted over the
`Stream`. The `body` can be one of either {string}, {ArrayBuffer}, {TypedArray},
{DataView}, {Buffer}, {stream.Readable}, {ReadableStream}, {Blob}, or a promise,
function (sync or async) that provides any of these.

### Class: `quic.Session`

The `quic.Session` object encapsulates a connection between QUIC peers.

#### Event: `'datagram'`

the `'datagram'` event is emitted when a QUIC datagram is received on
a {quic.Session}.

The callback is invoked with a `DatagramEvent`.

The `'datagram'` event can be handled either by registering a handler
using the standard `EventTarget` `addEventListener()` method:

```js
session.addEventListener('datagram', ({ datagram, session }) => {
  console.log(datagram);
  console.log(session);
});
```

Or by setting the value of the `ondatagram` property:

```js
session.ondatagram = ({ datagram, session }) => {
  console.log(datagram);
  console.log(session);
};
```

The `'datagram'` event may be emitted multiple times.

#### Event: `'stream'`

The `'stream'` event is emitted when the `Session` receives a
new peer initiated stream.

The callback is invoked with a `StreamEvent` that provides access to the
newly created {quic.Stream}.

The `'stream'` event can be handled either by registering a handler
using the standard `EventTarget` `addEventListener()` method:

```js
session.addEventListener('stream', ({ stream }) => { /* ... */ });
```

Or by setting the value of the `onstream` property:

```js
session.onstream = ({ stream }) => { /* ... */ };
```

The `'stream'` event may be emitted multiple times.

#### `session.alpn`

* Type: {string} The application protocol negotiated for the session.

The `session.alpn` property is set on completion of the TLS handshake.

#### `session.cancel([reason])`

* `reason` {any}

Immediately and synchronously cancels the `Session`, abruptly interrupting
any `Streams` remaining open. After canceling, the `session.closed` promise
will be fulfilled.

#### `session.certificate`

* Type: {X509Certificate} The local X.509 Certificate.

#### `session.cipher`

* Type: {Object}
  * `name` {string}
  * `version` {string}

The `session.cipher` property is set on completion of the TLS handshake, and
provides details for the negotiated TLS cipher in use.

#### `session.clienthello`

* Type: {Promise} When the `sessionConfig.clientHello` option is `true`, the
  `session.clienthello` will be a promise that is fulfilled at the start of\
  the TLS handshake for server-side `Session`s only.

The promise is fulfilled with a {quic.ClientHello} object that provides details
about the initial parameters of the handshake and gives user code the
opportunity to supply a new `tls.SecureContext` to use with the session.

When the promise is fulfilled, the TLS handshake will be paused until the
`clientHello.done()` method is called.

```js
endpoint.addEventListener('session', ({ session }) => {
  session.clienthello.then(({ alpn, servername, done }) => {
    console.log(alpn, servername);
    done();
  });
  // ...
});
```

#### `session.close()`

* Return: {Promise}

Starts the graceful shutdown of the `Session`. Existing `Streams` associated
with the `Session` will be permitted to close naturally. Attempts to create
new streams using `session.open()` will fail, and all new peer-initiated
streams will be ignored.

The promise returned will be fulfilled successfully once the `Session` has
been destroyed, or rejected if any error occurs while waiting for the `Session`
to close.

#### `session.closed`

* Return: {Promise} A promise that will be resolved when the `Session` is
  closed, or rejected if the session is canceled with a `reason`.

#### `session.closing`

* Type: {boolean} Set to `true` after `session.close()` has been called.

#### `session.earlyData`

* Type: {boolean} Set to true if the early data was received during the TLS
  handshake.

#### `session.ephemeralKeyInfo`

* Type: {Object} Ephermeral key details established during the
  TLS handhake.
  * `name` {string}
  * `size` {number}
  * `type` {string}

#### `session.handshake`

* Type: {Promise} A promise that is fulfilled when the TLS handshake
  completes.

#### `session.keylog`

* Type: {stream.Readable} When `quic.SessionConfig` `keylog` option
  is set to `true`, the `session.keylog` will be a {stream.Readable} that
  produces TLS keylog diagnostic output.

#### `session.ocsp`

* Type: {Promise} When the `quic.SessionConfig` `ocsp` option is set to
  `true`, the `session.ocsp` is a promise that is fulfilled during the
  OCSP-phase of the TLS handshake. For client `Session`s, the promise
  is fulfilled with a {quic.OCSPResponse}, for server `Session`s, the
  promise is fulfilled with a {quic.OCSPRequest}.

On server `Session`s, the TLS handshake will be suspended until the
`ocspRequest.respondWith()` method is called.

```js
session.addEventListener('session', ({ session }) => {
  session.ocsp.then(({ respondWith }) => {
    respondWith('ok');
  });
});
```

#### `session.open([options])`

* `options` {quic.StreamOptions}
* Returns: {quic.Stream}

Creates and returns a new {quic.Stream}. By default, the new `Stream`
will be bidirectional.

#### `session.peerCertificate`

* Type: {X509Certificate} The peer X.509 Certificate.

#### `session.qlog`

* Type: {stream.Readable} When the `quic.SessionConfig` `qlog` option
  is set to `true`, the `session.qlog` will be a {stream.Readable} that
  produces QUIC qlog diagnostic output.

#### `session.remoteAddress`

* Type: {net.SocketAddress} The socket address of the remote peer.

#### `session.servername`

* Type: {string} Identifies the SNI server name configured for the session.

The `session.servername` property is set on completion of the TLS handshake.

#### `session.sessionTicket`

* Type: {Blob} Returns the most recently available TLS session ticket available
  for this `Sessions` (if any). The session ticket data is used to resume a
  previously established QUIC session without re-negotiating the handshake. The
  {Blob} given by `session.sessionTicket` can be passed directly on to the
  `endpoint.connect()` method.

#### `session.stats`

* Type: {quic.SessionStats}

#### `session.updateKey()`

Initiates a key update for this session.

#### `session.validationError`

* Type: {Object}
  * `reason` {string} A simple, human-readable description of the validation
    failure.
  * `code` {string} A static error-code indicating the reason for the validation
    failure.

The `session.validationError` property is set at the completion of the TLS
handshake if the peer certificate failed validation. If the certificate did not
fail validation, this property will be `undefined`.

### Class: `quic.SessionConfig`

The `quic.SessionConfig` object is used to provide configuration options for
a newly created {quic.Session}.

#### Static method: `quic.SessionConfig.isSessionConfig(value)`

* `value` {any}

Returns `true` if `value` is an instance of {quic.SessionConfig}.

#### `new quic.SessionConfig(side, options)`

* `side` {string} One of `'client'` or `'server'`
* `options` {quic.SessionConfigInit}

#### Object: `quic.SessionConfigInit`

The `quic.SessionConfigInit` is an ordinary JavaScript object
with properties that are used to configure a new `Session`.

The most commonly used properties are:

* `alpn` {string} The standard ALPN identifier for the QUIC
  application protocol. **Default**: `'h3'` (HTTP/3)
* `hostname` {string} The SNI host name to be used with `Session`.
* `secure` {Object} Options use to configure the TLS secure context
  used by the `Session`.
  * `ca` {string|string[]|Buffer|Buffer[]}
  * `cert` {string|string[]|Buffer|Buffer[]}
  * `key` {string|string[]|Buffer|Buffer[]|Object[]}
  * `rejectUnauthorized` {boolean}
  * `requestPeerCertificate` {boolean}

Additional advanced configuration properties are also available.
Most applications should rarely have reason to use the advanced
settings, and they should be used only when necessary:

* `dcid` {string|ArrayBuffer|TypedArray|DataView|Buffer} For
  client `Session` objects, the initial connection identifier
  is typically randomly generated. The `dcid` option allows
  an initial connection identifier to be provided. The identifier
  must be between 0 and 20 bytes in length.
* `scid` {string|ArrayBuffer|TypedArray|DataView|Buffer} For
  client `Session` objects, the initial connection identifier
  for the local peer is typically randomly generated. The `scid`
  options an initial connection identifier to be provided. The
  identifier must be between 0 and 20 bytes in length.
* `preferredAddressStrategy` {string} One of `'use'` or `'ignore'`.
  Used by client `Session` instances to determine whether to use
  or ignore a server's advertised preferred address. **Default**: `'use'`.
* `qlog` {boolean} Enables the generation of qlog diagnostics output for
  the endpoint. **Default**: `false`.
* `secure` {Object} Options use to configure the TLS secure context
  used by the `Session`.
  * `ciphers` {string}
  * `clientCertEngine` {string}
  * `clientHello` {boolean}
  * `crl` {string|string[]|Buffer|Buffer[]}
  * `dhparam` {string|Buffer}
  * `enableTLSTrace` {boolean}
  * `ecdhCurve` {string}
  * `keylog` {boolean}
  * `ocsp` {boolean}
  * `passphrase` {string}
  * `pfx` {string|string[]|Buffer|Buffer[]|Object[]}
  * `privateKey` {Object}
    * `engine` {string}
    * `identifier` {string}
  * `pskCallback` {Function}
    * `socket` {tls.TLSSocket}
    * `identity` {string}
    * Returns: {Buffer|TypedArray|DataView}
  * `requestPeerCertificate` {boolean}
  * `secureOptions`
  * `sessionIdContext` {string}
  * `sessionTimeout` {number}
  * `sigalgs` {string}
  * `ticketKeys` {Buffer}
  * `verifyHostnameIdentity` {boolean}
* `transportParams` {Object} The QUIC transport parameters to use when
  establishing the `Session`.
  * `ackDelayExponent` {number|bigint}
  * `activeConnectionIdLimit` {number|bigint}
  * `disableActiveMigration` {boolean}
  * `initialMaxData` {number|bigint}
  * `initialMaxStreamDataBidiLocal` {number|bigint}
  * `initialMaxStreamDataBidiRemote` {number|bigint}
  * `initialMaxStreamDataUni` {number|bigint}
  * `initialMaxStreamsBidi` {number|bigint}
  * `initialMaxStreamsUni` {number|bigint}
  * `maxAckDelay` {number|bigint}
  * `maxDatagramFrameSize` {number|bigint}
  * `maxIdleTimeout` {number|bigint}
  * `preferredAddress` {Object}
    * `ipv4` {net.SocketAddressInit|net.SocketAddress}
    * `ipv6` {net.SocketAddressInit|net.SocketAddress}

All properties are optional.

### Class: `quic.SessionStats`

#### `sessionStats.bidiStreamCount`

* Type: {bigint}

The total number of bidirectional `Stream`s during this session.

#### `sessionStats.blockCount`

* Type: {bigint}

The total number of times that this session has been blocked from
transmitting due to flow control.

#### `sessionStats.bytesInFlight`

* Type: {bigint}

The total number of unacknowledged bytes currently in flight for this
session.

#### `sessionStats.bytesReceived`

* Type: {bigint}

The total number of bytes that have been received for this session.

#### `sessionStats.bytesSent`

* Type: {bigint}

The total number of bytes that have been sent for this session.

#### `sessionStats.closingAt`

* Type: {bigint}

The timestamp at which graceful shutdown of this session started.

#### `sessionStats.congestionRecoveryStartTS`

* Type: {bigint}

#### `sessionStats.createdAt`

* Type: {bigint}

The timestamp at which this session was created.

#### `sessionStats.cwnd`

* Type: {bigint}

The current size of the flow control window.

#### `sessionStats.deliveryRateSec`

* Type: {bigint}

The current data delivery rate per second.

#### `sessionStats.duration`

* Type: {bigint}

The total length of time since `createdAt` that this session has been
active.

#### `sessionStats.firstRttSampleTS`

* Type: {bigint}

#### `sessionStats.handshakeCompletedAt`

* Type: {bigint}

The timestamp when the TLS handshake completed.

#### `sessionStats.handshakeConfirmedAt`

* Type: {bigint}

The timestamp when the TLS handshake was confirmed.

#### `sessionStats.inboundStreamsCount`

* Type: {bigint}

The total number of peer-initiated streams that have been handled by this
session.

#### `sessionStats.initialRtt`

* Type: {bigint}

The initial round-trip time (RTT) used by this session.

#### `sessionStats.keyUpdateCount`

* Type: {bigint}

The total number of times this session has processed a key update.

#### `sessionStats.lastReceivedAt`

* Type: {bigint}

The timestamp indicating when the most recently received packet for this
session was received.

#### `sessionStats.lastSentAt`

* Type: {bigint}

The timestamp indicating when the most recently sent packet for this
session was sent.

#### `sessionStats.lastSentPacketTS`

* Type: {bigint}

#### `sessionStats.latestRtt`

* Type: {bigint}

The most recently recorded round-trip time for this session.

#### `sessionStats.lossDetectionTimer`

* Type: {bigint}

#### `sessionStats.lossRetransmitCount`

* Type: {bigint}

The total number of times the retransmit timer has fired triggering a
retransmission.

#### `sessionStats.lossTime`

* Type: {bigint}

#### `sessionStats.maxBytesInFlight`

* Type: {bigint}

The maximum number of unacknowledged bytes in flight during the
lifetime of the session.

#### `sessionStats.maxUdpPayloadSize`

* Type: {bigint}

The maximum UDP payload size used on this session.

#### `sessionStats.minRtt`

* Type: {bigint}

The minimum recorded round-trip time for this session.

#### `sessionStats.outboundStreamsCount`

* Type: {bigint}

The total number of locally initiated streams handled by this session.

#### `sessionStats.ptoCount`

* Type: {bigint}

#### `sessionStats.receiveRate`

* Type: {bigint}

The data received rate per second.

#### `sessionStats.rttVar`

* Type: {bigint}

#### `sessionStats.sendRate`

* Type: {bigint}

The data send rate per second.

#### `sessionStats.smoothedRtt`

* Type: {bigint}

The most recently calculated "smoothed" round-trip time for this session.

#### `sessionStats.ssthresh`

* Type: {bigint}

#### `sessionStats.uniStreamCount`

* Type: {bigint}

The total number of unidirectional streams handled by this session.

### Class: `quic.Stream`

#### `stream.blocked`

* Type: {Promise} A promise that is fulfilled whenever the `Stream` becomes
  blocked due to congestion control. Once the promise is fulfilled,
  a new `stream.blocked` promise is created.

#### `stream.closed`

* Returns: {Promise} A promise that is resolved when the `Stream` is closed, or
  rejected if the `Stream` is canceled with a `reason`.

#### `stream.cancel([reason])`

* `reason` {any}

Immediately and synchronously cancels the `Stream`. Any queued inbound
or outbound data is lost. The `stream.closed` promise will be fulfilled.

#### `stream.headers`

* Type: {Promise} A promise fulfilled with a null-prototype object containing
  the headers that have been received for this `Stream` (only supported if the
  ALPN-identified protocol supports headers).

#### `stream.id`

* Type: {bigint} The numeric ID of the `Stream`.

#### `stream.locked`

* Type: {boolean} Set to `true` if `stream.readableStream()` or
  `stream.streamReadable()` has been called to acquire a consumer for
  this `Stream`'s inbound data.

#### `stream.readableNodeStream()`

* Returns: {stream.Readable} A `stream.Readable` that may be used to
  consume this `Stream`'s inbound data.

#### `stream.readableWebStream()`

* Returns: {ReadableStream} A `ReadableStream` that may be used to consume the
  inbound `Stream` data received from the peer.

#### `stream.respondWith(options)`

* `options` {quic.ResponseOptions}

#### `stream.serverInitiated`

* Type: {boolean} Set to `true` if the `Stream` was initiated by the
  server.

#### `stream.stats`

* Type: {quic.StreamStats}

#### `stream.trailers`

* Type: {Promise} A promise that is fulfilled when this `Stream` receives
  trailing headers for the stream (only supported if the ALPN-identified
  protocol supports headers).

#### `stream.unidirectional`

* Type: {boolean} Set to `true` if the `Stream` is undirectional.

### Class: `quic.StreamOptions`

#### Static method: `quic.StreamOptions.isStreamOptions(value)`

* `value` {any}

Returns `true` if `value` is an instance of {quic.StreamOptions}.

#### `new quic.StreamOptions([options])`

* `options` {quic.StreamOptionsInit}

#### Object: `quic.StreamOptionsInit`

The `quic.StreamOptionsInit` is an ordinary JavaScript object
with the following properties:

<!--lint disable maximum-line-length remark-lint-->
* `unidirectional` {boolean} If `true`, the `Stream` will be opened as a
  unidirectional stream, allowing only for outbound data to be transmitted.
* `headers` {Object|Promise} If supported by the application protocol, the headers
  to be transmitted at the start of the stream, or a promise to be fulfilled with
  those headers.
* `trailers` {Object|Promise} If supported by the application protocol, the trailing
  headers to be transmitted after the stream body, or a promise to be fulfilled with
  those headers.
* `body` {string|ArrayBuffer|TypedArray|DataView|Buffer|stream.Readable|ReadableStream|Blob|Promise|Function}
<!--lint enable maximum-line-length remark-lint-->

All properties are optional.

The `body` provides the outgoing data that is to be transmitted over the
`Stream`. The `body` can be one of either {string}, {ArrayBuffer}, {TypedArray},
{DataView}, {Buffer}, {stream.Readable}, {ReadableStream}, {Blob}, or a promise,
function (sync or async) that provides any of these.

### Class: `quic.StreamStats`

#### `streamStats.bytesReceived`

Type: {bigint}

The total bytes received for this stream.

#### `streamStats.bytesSent`

Type: {bigint}

The total bytes sent for this stream.

#### `streamStats.closingAt`

Type: {bigint}

The timestamp at which graceful shutdown of this stream was started.

#### `streamStats.createdAt`

Type: {bigint}

The timestamp at which this stream was created.

#### `streamStats.duration`

Type: {bigint}

The length of time since `createdAt` that this stream has been active.

#### `streamStats.finalSize`

Type: {bigint}

The final number of bytes successfully received and processed by this stream.

#### `streamStats.lastAcknowledgeAt`

Type: {bigint}

The timestamp indicating the last time this stream received an acknowledgement
for sent bytes.

#### `streamStats.lastReceivedAt`

Type: {bigint}

The timestamp indicating the last time this stream received data.

#### `streamStats.maxOffset`

Type: {bigint}

The maximum data offset received by this stream.

#### `streamStats.maxOffsetAcknowledged`

Type: {bigint}

The maximum acknowledged offset received by this stream.

#### `streamStats.maxOffsetReceived`

Type: {bigint}
