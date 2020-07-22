# QUIC

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

The `net` module provides an implementation of the QUIC protocol. To
access it, the Node.js binary must be compiled using the
`--experimental-quic` configuration flag.

```js
const { createQuicSocket } = require('net');
```

## Example

```js
'use strict';

const key = getTLSKeySomehow();
const cert = getTLSCertSomehow();

const { createQuicSocket } = require('net');

// Create the QUIC UDP IPv4 socket bound to local IP port 1234
const socket = createQuicSocket({ endpoint: { port: 1234 } });

socket.on('session', async (session) => {
  // A new server side session has been created!

  // The peer opened a new stream!
  session.on('stream', (stream) => {
    // Let's say hello
    stream.end('Hello World');

    // Let's see what the peer has to say...
    stream.setEncoding('utf8');
    stream.on('data', console.log);
    stream.on('end', () => console.log('stream ended'));
  });

  const uni = await session.openStream({ halfOpen: true });
  uni.write('hi ');
  uni.end('from the server!');
});

// Tell the socket to operate as a server using the given
// key and certificate to secure new connections, using
// the fictional 'hello' application protocol.
(async function() {
  await socket.listen({ key, cert, alpn: 'hello' });
  console.log('The socket is listening for sessions!');
})();

```

## QUIC basics

QUIC is a UDP-based network transport protocol that includes built-in security
via TLS 1.3, flow control, error correction, connection migration,
multiplexing, and more.

Within the Node.js implementation of the QUIC protocol, there are three main
components: the `QuicSocket`, the `QuicSession` and the `QuicStream`.

### QuicSocket

A `QuicSocket` encapsulates a binding to one or more local UDP ports. It is
used to send data to, and receive data from, remote endpoints. Once created,
a `QuicSocket` is associated with a local network address and IP port and can
act as both a QUIC client and server simultaneously. User code at the
JavaScript level interacts with the `QuicSocket` object to:

* Query or modified the properties of the local UDP binding;
* Create client `QuicSession` instances;
* Wait for server `QuicSession` instances; or
* Query activity statistics

Unlike the `net.Socket` and `tls.TLSSocket`, a `QuicSocket` instance cannot be
directly used by user code at the JavaScript level to send or receive data over
the network.

### Client and server QuicSessions

A `QuicSession` represents a logical connection between two QUIC endpoints (a
client and a server). In the JavaScript API, each is represented by the
`QuicClientSession` and `QuicServerSession` specializations.

At any given time, a `QuicSession` exists is one of four possible states:

* `Initial` - Entered as soon as the `QuicSession` is created.
* `Handshake` - Entered as soon as the TLS 1.3 handshake between the client and
  server begins. The handshake is always initiated by the client.
* `Ready` - Entered as soon as the TLS 1.3 handshake completes. Once the
  `QuicSession` enters the `Ready` state, it may be used to exchange
  application data using `QuicStream` instances.
* `Closed` - Entere as soon as the `QuicSession` connection has been
  terminated.

New instances of `QuicClientSession` are created using the `connect()`
function on a `QuicSocket` as in the example below:

```js
const { createQuicSocket } = require('net');

// Create a QuicSocket associated with localhost and port 1234
const socket = createQuicSocket({ endpoint: { port: 1234 } });

(async function() {
  const client = await socket.connect({
    address: 'example.com',
    port: 4567,
    alpn: 'foo'
  });
})();
```

As soon as the `QuicClientSession` is created, the `address` provided in
the connect options will be resolved to an IP address (if necessary), and
the TLS 1.3 handshake will begin. The `QuicClientSession` cannot be used
to exchange application data until after the `'secure'` event has been
emitted by the `QuicClientSession` object, signaling the completion of
the TLS 1.3 handshake.

```js
client.on('secure', () => {
  // The QuicClientSession can now be used for application data
});
```

New instances of `QuicServerSession` are created internally by the
`QuicSocket` if it has been configured to listen for new connections
using the `listen()` method.

```js
const { createQuicSocket } = require('net');

const key = getTLSKeySomehow();
const cert = getTLSCertSomehow();

const socket = createQuicSocket();

socket.on('session', (session) => {
  session.on('secure', () => {
    // The QuicServerSession can now be used for application data
  });
});

(async function() {
  await socket.listen({ key, cert, alpn: 'foo' });
})();
```

As with client `QuicSession` instances, the `QuicServerSession` cannot be
used to exhange application data until the `'secure'` event has been emitted.

### QuicSession and ALPN

QUIC uses the TLS 1.3 [ALPN][] ("Application-Layer Protocol Negotiation")
extension to identify the application level protocol that is using the QUIC
connection. Every `QuicSession` instance has an ALPN identifier that *must* be
specified in either the `connect()` or `listen()` options. ALPN identifiers that
are known to Node.js (such as the ALPN identifier for HTTP/3) will alter how the
`QuicSession` and `QuicStream` objects operate internally, but the QUIC
implementation for Node.js has been designed to allow any ALPN to be specified
and used.

### QuicStream

Once a `QuicSession` transitions to the `Ready` state, `QuicStream` instances
may be created and used to exchange application data. On a general level, all
`QuicStream` instances are simply Node.js Duplex Streams that allow
bidirectional data flow between the QUIC client and server. However, the
application protocol negotiated for the `QuicSession` may alter the semantics
and operation of a `QuicStream` associated with the session. Specifically,
some features of the `QuicStream` (e.g. headers) are enabled only if the
application protocol selected is known by Node.js to support those features.

Once the `QuicSession` is ready, a `QuicStream` may be created by either the
client or server, and may be unidirectional or bidirectional.

The `openStream()` method is used to create a new `QuicStream`:

```js
// Create a new bidirectional stream
async function createStreams(session) {
  const stream1 = await session.openStream();

  // Create a new unidirectional stream
  const stream2 = await session.openStream({ halfOpen: true });
}
```

As suggested by the names, a bidirectional stream allows data to be sent on
a stream in both directions, by both client and server, regardless of which
peer opened the stream. A unidirectional stream can be written to only by the
QuicSession that opened it.

The `'stream'` event is emitted by the `QuicSession` when a new `QuicStream`
has been initated by the connected peer:

```js
session.on('stream', (stream) => {
  if (stream.bidirectional) {
    stream.write('Hello World');
    stream.end();
  }
  stream.on('data', console.log);
  stream.on('end', () => {});
});
```

#### QuicStream headers

Some QUIC application protocols (like HTTP/3) make use of headers.

There are four kinds of headers that the Node.js QUIC implementation
is capable of handling dependent entirely on known application protocol
support:

* Informational Headers
* Initial Headers
* Trailing Headers
* Push Headers

These categories correlate exactly with the equivalent HTTP
concepts:

* Informational Headers: Any response headers transmitted within
  a block of headers using a `1xx` status code.
* Initial Headers: HTTP request or response headers
* Trailing Headers: A block of headers that follow the body of a
  request or response.
* Push Promise Headers: A block of headers included in a promised
  push stream.

If headers are supported by the application protocol in use for
a given `QuicSession`, the `'initialHeaders'`, `'informationalHeaders'`,
and `'trailingHeaders'` events will be emitted by the `QuicStream`
object when headers are received; and the `submitInformationalHeaders()`,
`submitInitialHeaders()`, and `submitTrailingHeaders()` methods can be
used to send headers.

## QUIC and HTTP/3

HTTP/3 is an application layer protocol that uses QUIC as the transport.

TBD

## QUIC JavaScript API

### `net.createQuicSocket(\[options\])`
<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `client` {Object} A default configuration for QUIC client sessions created
    using `quicsocket.connect()`.
  * `disableStatelessReset` {boolean} When `true` the `QuicSocket` will not
    send stateless resets. **Default**: `false`.
  * `endpoint` {Object} An object describing the local address to bind to.
    * `address` {string} The local address to bind to. This may be an IPv4 or
      IPv6 address or a host name. If a host name is given, it will be resolved
      to an IP address.
    * `port` {number} The local port to bind to.
    * `type` {string} Can be one of `'udp4'`, `'upd6'`, or `'udp6-only'` to
      use IPv4, IPv6, or IPv6 with dual-stack mode disabled.
      **Default**: `'udp4'`.
  * `lookup` {Function} A [custom DNS lookup function][].
    **Default**: undefined.
  * `maxConnections` {number} The maximum number of total active inbound
    connections.
  * `maxConnectionsPerHost` {number} The maximum number of inbound connections
    allowed per remote host. Default: `100`.
  * `maxStatelessResetsPerHost` {number} The maximum number of stateless
    resets that the `QuicSocket` is permitted to send per remote host.
    Default: `10`.
  * `qlog` {boolean} Whether to enable ['qlog'][] for incoming sessions.
    (For outgoing client sessions, set `client.qlog`.) Default: `false`.
  * `retryTokenTimeout` {number} The maximum number of *seconds* for retry token
    validation. Default: `10` seconds.
  * `server` {Object} A default configuration for QUIC server sessions.
  * `statelessResetSecret` {Buffer|Uint8Array} A 16-byte `Buffer` or
    `Uint8Array` providing the secret to use when generating stateless reset
    tokens. If not specified, a random secret will be generated for the
    `QuicSocket`. **Default**: `undefined`.
  * `validateAddress` {boolean} When `true`, the `QuicSocket` will use explicit
    address validation using a QUIC `RETRY` frame when listening for new server
    sessions. Default: `false`.
  * `validateAddressLRU` {boolean} When `true`, validation will be skipped if
    the address has been recently validated. Currently, only the 10 most
    recently validated addresses are remembered. Setting `validateAddressLRU`
    to `true`, will enable the `validateAddress` option as well. Default:
    `false`.

The `net.createQuicSocket()` function is used to create new `QuicSocket`
instances associated with a local UDP address.

### Class: `QuicEndpoint`
<!-- YAML
added: REPLACEME
-->

The `QuicEndpoint` wraps a local UDP binding used by a `QuicSocket` to send
and receive data. A single `QuicSocket` may be bound to multiple
`QuicEndpoint` instances at any given time.

Users will not create instances of `QuicEndpoint` directly.

#### `quicendpoint.addMembership(address, iface)`
<!-- YAML
added: REPLACEME
-->

* `address` {string}
* `iface` {string}

Tells the kernel to join a multicast group at the given `multicastAddress` and
`multicastInterface` using the `IP_ADD_MEMBERSHIP` socket option. If the
`multicastInterface` argument is not specified, the operating system will
choose one interface and will add membership to it. To add membership to every
available interface, call `addMembership()` multiple times, once per
interface.

#### `quicendpoint.address`
<!-- YAML
added: REPLACEME
-->

* Type: Address

An object containing the address information for a bound `QuicEndpoint`.

The object will contain the properties:

* `address` {string} The local IPv4 or IPv6 address to which the `QuicEndpoint` is
  bound.
* `family` {string} Either `'IPv4'` or `'IPv6'`.
* `port` {number} The local IP port to which the `QuicEndpoint` is bound.

If the `QuicEndpoint` is not bound, `quicendpoint.address` is an empty object.

#### `quicendpoint.bind(\[options\])`
<!-- YAML
added: REPLACEME
-->

Binds the `QuicEndpoint` if it has not already been bound. User code will
not typically be responsible for binding a `QuicEndpoint` as the owning
`QuicSocket` will do that automatically.

* `options` {Object}
  * `signal` {AbortSignal} Optionally allows the `bind()` to be canceled
    using an `AbortController`.
* Returns: {Promise}

The `quicendpoint.bind()` function returns `Promise` that will be resolved
with the address once the bind operation is successful.

If the `QuicEndpoint` has been destroyed, or is destroyed while the `Promise`
is pending, the `Promise` will be rejected with an `ERR_INVALID_STATE` error.

If an `AbortSignal` is specified in the `options` and it is triggered while
the `Promise` is pending, the `Promise` will be rejected with an `AbortError`.

If `quicendpoint.bind()` is called again while a previously returned `Promise`
is still pending or has already successfully resolved, the previously returned
pending `Promise` will be returned. If the additional call to
`quicendpoint.bind()` contains an `AbortSignal`, the `signal` will be ignored.

#### `quicendpoint.bound`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicEndpoint` is bound to the local UDP port.

#### `quicendpoint.close()`
<!-- YAML
added: REPLACEME
-->

Closes and destroys the `QuicEndpoint`. Returns a `Promise` that is resolved
once the `QuicEndpoint` has been destroyed, or rejects if the `QuicEndpoint`
is destroyed with an error.

* Returns: {Promise}

The `Promise` cannot be canceled. Once `quicendpoint.close()` is called, the
`QuicEndpoint` will be destroyed.

#### `quicendpoint.closing`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicEndpoint` is in the process of closing.

#### `quicendpoint.destroy(\[error\])`
<!-- YAML
added: REPLACEME
-->

* `error` {Object} An `Error` object.

Closes and destroys the `QuicEndpoint` instance making it usuable.

#### `quicendpoint.destroyed`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicEndpoint` has been destroyed.

#### `quicendpoint.dropMembership(address, iface)`
<!-- YAML
added: REPLACEME
-->

* `address` {string}
* `iface` {string}

Instructs the kernel to leave a multicast group at `multicastAddress` using the
`IP_DROP_MEMBERSHIP` socket option. This method is automatically called by the
kernel when the socket is closed or the process terminates, so most apps will
never have reason to call this.

If `multicastInterface` is not specified, the operating system will attempt to
drop membership on all valid interfaces.

#### `quicendpoint.fd`
<!-- YAML
added: REPLACEME
-->

* Type: {integer}

The system file descriptor the `QuicEndpoint` is bound to. This property
is not set on Windows.

#### `quicendpoint.pending`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicEndpoint` is in the process of binding to
the local UDP port.

#### `quicendpoint.ref()`
<!-- YAML
added: REPLACEME
-->

#### `quicendpoint.setBroadcast(\[on\])`
<!-- YAML
added: REPLACEME
-->

* `on` {boolean}

Sets or clears the `SO_BROADCAST` socket option. When set to `true`, UDP
packets may be sent to a local interface's broadcast address.

#### `quicendpoint.setMulticastInterface(iface)`
<!-- YAML
added: REPLACEME
-->

* `iface` {string}

All references to scope in this section are referring to IPv6 Zone Indices,
which are defined by [RFC 4007][]. In string form, an IP with a scope index
is written as `'IP%scope'` where scope is an interface name or interface
number.

Sets the default outgoing multicast interface of the socket to a chosen
interface or back to system interface selection. The multicastInterface must
be a valid string representation of an IP from the socket's family.

For IPv4 sockets, this should be the IP configured for the desired physical
interface. All packets sent to multicast on the socket will be sent on the
interface determined by the most recent successful use of this call.

For IPv6 sockets, multicastInterface should include a scope to indicate the
interface as in the examples that follow. In IPv6, individual send calls can
also use explicit scope in addresses, so only packets sent to a multicast
address without specifying an explicit scope are affected by the most recent
successful use of this call.

##### Examples: IPv6 outgoing multicast interface
<!-- YAML
added: REPLACEME
-->
On most systems, where scope format uses the interface name:

```js
const { createQuicSocket } = require('net');
const socket = createQuicSocket({ endpoint: { type: 'udp6', port: 1234 } });

socket.on('ready', () => {
  socket.endpoints[0].setMulticastInterface('::%eth1');
});
```

On Windows, where scope format uses an interface number:

```js
const { createQuicSocket } = require('net');
const socket = createQuicSocket({ endpoint: { type: 'udp6', port: 1234 } });

socket.on('ready', () => {
  socket.endpoints[0].setMulticastInterface('::%2');
});
```

##### Example: IPv4 outgoing multicast interface
<!-- YAML
added: REPLACEME
-->
All systems use an IP of the host on the desired physical interface:

```js
const { createQuicSocket } = require('net');
const socket = createQuicSocket({ endpoint: { type: 'udp4', port: 1234 } });

socket.on('ready', () => {
  socket.endpoints[0].setMulticastInterface('10.0.0.2');
});
```

##### Call results

A call on a socket that is not ready to send or no longer open may throw a
Not running Error.

If multicastInterface can not be parsed into an IP then an `EINVAL` System
Error is thrown.

On IPv4, if `multicastInterface` is a valid address but does not match any
interface, or if the address does not match the family then a System Error
such as `EADDRNOTAVAIL` or `EPROTONOSUP` is thrown.

On IPv6, most errors with specifying or omitting scope will result in the
socket continuing to use (or returning to) the system's default interface
selection.

A socket's address family's ANY address (IPv4 `'0.0.0.0'` or IPv6 `'::'`)
can be used to return control of the sockets default outgoing interface to
the system for future multicast packets.

#### `quicendpoint.setMulticastLoopback(\[on\])`
<!-- YAML
added: REPLACEME
-->

* `on` {boolean}

Sets or clears the `IP_MULTICAST_LOOP` socket option. When set to `true`,
multicast packets will also be received on the local interface.

#### `quicendpoint.setMulticastTTL(ttl)`
<!-- YAML
added: REPLACEME
-->

* `ttl` {number}

Sets the `IP_MULTICAST_TTL` socket option. While TTL generally stands for
"Time to Live", in this context it specifies the number of IP hops that a
packet is allowed to travel through, specifically for multicast traffic. Each
router or gateway that forwards a packet decrements the TTL. If the TTL is
decremented to `0` by a router, it will not be forwarded.

The argument passed to `setMulticastTTL()` is a number of hops between
`0` and `255`. The default on most systems is `1` but can vary.

#### `quicendpoint.setTTL(ttl)`
<!-- YAML
added: REPLACEME
-->

* `ttl` {number}

Sets the `IP_TTL` socket option. While TTL generally stands for "Time to Live",
in this context it specifies the number of IP hops that a packet is allowed to
travel through. Each router or gateway that forwards a packet decrements the
TTL. If the TTL is decremented to `0` by a router, it will not be forwarded.
Changing TTL values is typically done for network probes or when multicasting.

The argument to `setTTL()` is a number of hops between `1` and `255`.
The default on most systems is `64` but can vary.

#### `quicendpoint.unref()`
<!-- YAML
added: REPLACEME
-->

### Class: `QuicSession extends EventEmitter`
<!-- YAML
added: REPLACEME
-->
* Extends: {EventEmitter}

The `QuicSession` is an abstract base class that defines events, methods, and
properties that are shared by both `QuicClientSession` and `QuicServerSession`.

Users will not create instances of `QuicSession` directly.

#### Event: `'close'`
<!-- YAML
added: REPLACEME
-->

Emitted after the `QuicSession` has been destroyed and is no longer usable.

The `'close'` event will not be emitted more than once.

#### Event: `'error'`
<!-- YAML
added: REPLACEME
-->

Emitted immediately before the `'close'` event if the `QuicSession` was
destroyed with an error.

The callback will be invoked with a single argument:

* `error` {Object} An `Error` object.

The `'error'` event will not be emitted more than once.

#### Event: `'keylog'`
<!-- YAML
added: REPLACEME
-->

Emitted when key material is generated or received by a `QuicSession`
(typically during or immediately following the handshake process). This keying
material can be stored for debugging, as it allows captured TLS traffic to be
decrypted. It may be emitted multiple times per `QuicSession` instance.

The callback will be invoked with a single argument:

* `line` <Buffer> Line of ASCII text, in NSS SSLKEYLOGFILE format.

A typical use case is to append received lines to a common text file, which is
later used by software (such as Wireshark) to decrypt the traffic:

```js
const log = fs.createWriteStream('/tmp/ssl-keys.log', { flags: 'a' });
// ...
session.on('keylog', (line) => log.write(line));
```

The `'keylog'` event will be emitted multiple times.

#### Event: `'pathValidation'`
<!-- YAML
added: REPLACEME
-->

Emitted when a path validation result has been determined. This event
is strictly informational. When path validation is successful, the
`QuicSession` will automatically update to use the new validated path.

The callback will be invoked with three arguments:

* `result` {string} Either `'failure'` or `'success'`, denoting the status
  of the path challenge.
* `local` {Object} The local address component of the tested path.
* `remote` {Object} The remote address component of the tested path.

The `'pathValidation'` event will be emitted multiple times.

#### Event: `'secure'`
<!-- YAML
added: REPLACEME
-->

Emitted after the TLS handshake has been completed.

The callback will be invoked with two arguments:

* `servername` {string} The SNI servername requested by the client.
* `alpnProtocol` {string} The negotiated ALPN protocol.
* `cipher` {Object} Information about the selected cipher algorithm.
  * `name` {string} The cipher algorithm name.
  * `version` {string} The TLS version (currently always `'TLSv1.3'`).

These will also be available using the `quicsession.servername`,
`quicsession.alpnProtocol`, and `quicsession.cipher` properties.

The `'secure'` event will not be emitted more than once.

#### Event: `'stream'`
<!-- YAML
added: REPLACEME
-->

Emitted when a new `QuicStream` has been initiated by the connected peer.

The `'stream'` event may be emitted multiple times.

#### `quicsession.ackDelayRetransmitCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of retransmissions caused by delayed acknowledgements.

#### `quicsession.address`
<!-- YAML
added: REPLACEME
-->

* Type: {Object}
  * `address` {string} The local IPv4 or IPv6 address to which the `QuicSession`
    is bound.
  * `family` {string} Either `'IPv4'` or `'IPv6'`.
  * `port` {number} The local IP port to which the `QuicSocket` is bound.

An object containing the local address information for the `QuicSocket` to which
the `QuicSession` is currently associated.

#### `quicsession.alpnProtocol`
<!-- YAML
added: REPLACEME
-->

* Type: {string}

The ALPN protocol identifier negotiated for this session.

#### `quicsession.authenticated`
<!--YAML
added: REPLACEME
-->
* Type: {boolean}

True if the certificate provided by the peer during the TLS 1.3
handshake has been verified.

#### `quicsession.authenticationError`

* Type: {Object} An error object

If `quicsession.authenticated` is false, returns an `Error` object
representing the reason the peer certificate verification failed.

#### `quicsession.bidiStreamCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bidirectional streams created for this `QuicSession`.

#### `quicsession.blockCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of times the `QuicSession` has been blocked from sending
stream data due to flow control.

Such blocks indicate that transmitted stream data is not being consumed
quickly enough by the connected peer.

#### `quicsession.bytesInFlight`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of unacknowledged bytes this QUIC endpoint has transmitted
to the connected peer.

#### `quicsession.bytesReceived`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes received from the peer.

#### `quicsession.bytesSent`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes sent to the peer.

#### `quicsession.cipher`
<!-- YAML
added: REPLACEME
-->

* Type: {Object}
  * `name` {string} The cipher algorithm name.
  * `type` {string} The TLS version (currently always `'TLSv1.3'`).

Information about the cipher algorithm selected for the session.

#### `quicsession.close()`
<!-- YAML
added: REPLACEME
-->

Begins a graceful close of the `QuicSession`. Existing `QuicStream` instances
will be permitted to close naturally. New `QuicStream` instances will not be
permitted. Once all `QuicStream` instances have closed, the `QuicSession`
instance will be destroyed. Returns a `Promise` that is resolved once the
`QuicSession` instance is destroyed.

#### `quicsession.closeCode`
<!-- YAML
added: REPLACEME
-->
* Type: {Object}
  * `code` {number} The error code reported when the `QuicSession` closed.
  * `family` {number} The type of error code reported (`0` indicates a QUIC
    protocol level error, `1` indicates a TLS error, `2` represents an
    application level error.)

#### `quicsession.closing`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicSession` is in the process of a graceful shutdown.

#### `quicsession.destroy(\[error\])`
<!-- YAML
added: REPLACEME
-->

* `error` {any}

Destroys the `QuicSession` immediately causing the `close` event to be emitted.
If `error` is not `undefined`, the `error` event will be emitted immediately
before the `close` event.

Any `QuicStream` instances that are still opened will be abruptly closed.

#### `quicsession.destroyed`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicSession` has been destroyed.

#### `quicsession.duration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time the `QuicSession` was active.

#### `quicsession.getCertificate()`
<!-- YAML
added: REPLACEME
-->

* Returns: {Object} A [Certificate Object][].

Returns an object representing the *local* certificate. The returned object has
some properties corresponding to the fields of the certificate.

If there is no local certificate, or if the `QuicSession` has been destroyed,
an empty object will be returned.

#### `quicsession.getPeerCertificate(\[detailed\])`
<!-- YAML
added: REPLACEME
-->

* `detailed` {boolean} Include the full certificate chain if `true`, otherwise
  include just the peer's certificate. **Default**: `false`.
* Returns: {Object} A [Certificate Object][].

Returns an object representing the peer's certificate. If the peer does not
provide a certificate, or if the `QuicSession` has been destroyed, an empty
object will be returned.

If the full certificate chain was requested (`details` equals `true`), each
certificate will include an `issuerCertificate` property containing an object
representing the issuer's certificate.

#### `quicsession.handshakeAckHistogram`
<!-- YAML
added: REPLACEME
-->

TBD

#### `quicsession.handshakeContinuationHistogram`
<!-- YAML
added: REPLACEME
-->

TBD

#### `quicsession.handshakeComplete`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the TLS handshake has completed.

#### `quicsession.handshakeConfirmed`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` when the TLS handshake completion has been confirmed.

#### `quicsession.handshakeDuration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time taken to complete the TLS handshake.

#### `quicsession.idleTimeout`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicSession` was closed due to an idle timeout.

#### `quicsession.keyUpdateCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of key update operations that have occured.

#### `quicsession.latestRTT`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The most recently recorded RTT for this `QuicSession`.

#### `quicsession.lossRetransmitCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of lost-packet retransmissions that have been performed on
this `QuicSession`.

#### `quicsession.maxDataLeft`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes the `QuicSession` is *currently* allowed to
send to the connected peer.

#### `quicsession.maxInFlightBytes`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum number of in-flight bytes recorded for this `QuicSession`.

#### `quicsession.maxStreams`
<!-- YAML
added: REPLACEME
-->

* Type: {Object}
  * `uni` {number} The maximum number of unidirectional streams.
  * `bidi` {number} The maximum number of bidirectional streams.

The highest cumulative number of bidirectional and unidirectional streams
that can currently be opened. The values are set initially by configuration
parameters when the `QuicSession` is created, then updated over the lifespan
of the `QuicSession` as the connected peer allows new streams to be created.

#### `quicsession.minRTT`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The minimum RTT recorded so far for this `QuicSession`.

#### `quicsession.openStream(\[options\])`
<!-- YAML
added: REPLACEME
-->
* `options` {Object}
  * `halfOpen` {boolean} Set to `true` to open a unidirectional stream, `false`
    to open a bidirectional stream. **Default**: `true`.
  * `highWaterMark` {number} Total number of bytes that the `QuicStream` may
    buffer internally before the `quicstream.write()` function starts returning
    `false`. Default: `16384`.
  * `defaultEncoding` {string} The default encoding that is used when no
    encoding is specified as an argument to `quicstream.write()`. Default:
    `'utf8'`.
* Returns: {Promise} containing {QuicStream}

Returns a `Promise` that resolves a new `QuicStream`.

The `Promise` will be rejected if the `QuicSession` has been destroyed, is in
the process of a graceful shutdown, or the `QuicSession` is otherwise blocked
from opening a new stream.

#### `quicsession.ping()`
<!--YAML
added: REPLACEME
-->

The `ping()` method will trigger the underlying QUIC connection to serialize
any frames currently pending in the outbound queue if it is able to do so.
This has the effect of keeping the connection with the peer active and resets
the idle and retransmission timers. The `ping()` method is a best-effort
that ignores any errors that may occur during the serialization and send
operations. There is no return value and there is no way to monitor the status
of the `ping()` operation.

#### `quicsession.peerInitiatedStreamCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of `QuicStreams` initiated by the connected peer.

#### `quicsession.qlog`
<!-- YAML
added: REPLACEME
-->

* Type: {stream.Readable}

If `qlog` support is enabled for `QuicSession`, the `quicsession.qlog` property
provides a [`stream.Readable`][] that may be used to access the `qlog` event
data according to the [qlog standard][]. For client `QuicSessions`, the
`quicsession.qlog` property will be `undefined` untilt the `'qlog'` event
is emitted.

#### `quicsession.remoteAddress`
<!-- YAML
added: REPLACEME
-->

* Type: {Object}
  * `address` {string} The local IPv4 or IPv6 address to which the `QuicSession`
    is connected.
  * `family` {string} Either `'IPv4'` or `'IPv6'`.
  * `port` {number} The local IP port to which the `QuicSocket` is bound.

An object containing the remote address information for the connected peer.

#### `quicsession.selfInitiatedStreamCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of `QuicStream` instances initiated by this `QuicSession`.

#### `quicsession.servername`
<!-- YAML
added: REPLACEME
-->

* Type: {string}

The SNI servername requested for this session by the client.

#### `quicsession.smoothedRTT`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The modified RTT calculated for this `QuicSession`.

#### `quicsession.socket`
<!-- YAML
added: REPLACEME
-->

* Type: {QuicSocket}

The `QuicSocket` the `QuicSession` is associated with.

#### `quicsession.statelessReset`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

True if the `QuicSession` was closed due to QUIC stateless reset.

#### `quicsession.uniStreamCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of unidirectional streams created on this `QuicSession`.

#### `quicsession.updateKey()`
<!-- YAML
added: REPLACEME
-->

* Returns: {boolean} `true` if the key update operation is successfully
  initiated.

Initiates QuicSession key update.

An error will be thrown if called before `quicsession.handshakeConfirmed`
is equal to `true`.

#### `quicsession.usingEarlyData`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

On server `QuicSession` instances, set to `true` on completion of the TLS
handshake if early data is enabled. On client `QuicSession` instances,
set to true on handshake completion if early data is enabled *and* was
accepted by the server.

### Class: `QuicClientSession extends QuicSession`
<!-- YAML
added: REPLACEME
-->

* Extends: {QuicSession}

The `QuicClientSession` class implements the client side of a QUIC connection.
Instances are created using the `quicsocket.connect()` method.

#### Event: `'OCSPResponse'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicClientSession` receives a requested OCSP certificate
status response from the QUIC server peer.

The callback is invoked with a single argument:

* `response` {Buffer}

Node.js does not perform any automatic validation or processing of the
response.

The `'OCSPResponse'` event will not be emitted more than once.

#### Event: `'sessionTicket'`
<!-- YAML
added: REPLACEME
-->

The `'sessionTicket'` event is emitted when a new TLS session ticket has been
generated for the current `QuicClientSession`. The callback is invoked with
two arguments:

* `sessionTicket` {Buffer} The serialized session ticket.
* `remoteTransportParams` {Buffer} The serialized remote transport parameters
  provided by the QUIC server.

The `sessionTicket` and `remoteTransportParams` are useful when creating a new
`QuicClientSession` to more quickly resume an existing session.

The `'sessionTicket'` event may be emitted multiple times.

#### Event: `'qlog'`
<!-- YAML
added: REPLACEME
-->

The `'qlog'` event is emitted when the `QuicClientSession` is ready to begin
providing `qlog` event data. The callback is invoked with a single argument:

* `qlog` {stream.Readable} A [`stream.Readable`][] that is also available using
  the `quicsession.qlog` property.

#### Event: `'usePreferredAddress'`
<!-- YAML
added: REPLACEME
-->

The `'usePreferredAddress'` event is emitted when the client `QuicSession`
is updated to use the server-advertised preferred address. The callback is
invoked with a single `address` argument:

* `address` {Object}
  * `address` {string} The preferred host name
  * `port` {number} The preferred IP port
  * `type` {string} Either `'udp4'` or `'udp6'`.

This event is purely informational and will be emitted only when
`preferredAddressPolicy` is set to `'accept'`.

The `'usePreferredAddress'` event will not be emitted more than once.

#### `quicclientsession.ephemeralKeyInfo`
<!-- YAML
added: REPLACEME
-->

* Type: {Object}

An object representing the type, name, and size of parameter of an ephemeral
key exchange in Perfect Forward Secrecy on a client connection. It is an
empty object when the key exchange is not ephemeral. The supported types are
`'DH'` and `'ECDH'`. The `name` property is available only when type is
`'ECDH'`.

For example: `{ type: 'ECDH', name: 'prime256v1', size: 256 }`.

#### `quicclientsession.setSocket(socket])`
<!-- YAML
added: REPLACEME
-->

* `socket` {QuicSocket} A `QuicSocket` instance to move this session to.
* Returns: {Promise}

Migrates the `QuicClientSession` to the given `QuicSocket` instance. If the new
`QuicSocket` has not yet been bound to a local UDP port, it will be bound prior
to attempting the migration.

### Class: `QuicServerSession extends QuicSession`
<!-- YAML
added: REPLACEME
-->

* Extends: {QuicSession}

The `QuicServerSession` class implements the server side of a QUIC connection.
Instances are created internally and are emitted using the `QuicSocket`
`'session'` event.

#### Event: `'clientHello'`
<!-- YAML
added: REPLACEME
-->

Emitted at the start of the TLS handshake when the `QuicServerSession` receives
the initial TLS Client Hello.

The event handler is given a callback function that *must* be invoked for the
handshake to continue.

The callback is invoked with four arguments:

* `alpn` {string} The ALPN protocol identifier requested by the client.
* `servername` {string} The SNI servername requested by the client.
* `ciphers` {string[]} The list of TLS cipher algorithms requested by the
  client.
* `callback` {Function} A callback function that must be called in order for
  the TLS handshake to continue.

The `'clientHello'` event will not be emitted more than once.

#### Event: `'OCSPRequest'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicServerSession` has received a OCSP certificate status
request as part of the TLS handshake.

The callback is invoked with three arguments:

* `servername` {string}
* `context` {tls.SecureContext}
* `callback` {Function}

The callback *must* be invoked in order for the TLS handshake to continue.

The `'OCSPRequest'` event will not be emitted more than once.

#### `quicserversession.addContext(servername\[, context\])`
<!-- YAML
added: REPLACEME
-->

* `servername` {string} A DNS name to associate with the given context.
* `context` {tls.SecureContext} A TLS SecureContext to associate with the `servername`.

TBD

### Class: `QuicSocket`
<!-- YAML
added: REPLACEME
-->

New instances of `QuicSocket` are created using the `net.createQuicSocket()`
method, and can be used as both a client and a server.

#### Event: `'busy'`
<!-- YAML
added: REPLACEME
-->

Emitted when the server busy state has been toggled using
`quicSocket.serverBusy = true | false`. The callback is invoked with no
arguments. Use the `quicsocket.serverBusy` property to determine the
current status. This event is strictly informational.

```js
const { createQuicSocket } = require('net');

const socket = createQuicSocket();

socket.on('busy', () => {
  if (socket.serverBusy)
    console.log('Server is busy');
  else
    console.log('Server is not busy');
});

socket.serverBusy = true;
socket.serverBusy = false;
```

This `'busy'` event may be emitted multiple times.

#### Event: `'close'`
<!-- YAML
added: REPLACEME
-->

Emitted after the `QuicSocket` has been destroyed and is no longer usable.

The `'close'` event will only ever be emitted once.

#### Event: `'endpointClose'`
<!-- YAML
added: REPLACEME
-->

Emitted after a `QuicEndpoint` associated with the `QuicSocket` closes and
has been destroyed. The handler will be invoked with two arguments:

* `endpoint` {QuicEndpoint} The `QuicEndpoint` that has been destroyed.
* `error` {Error} An `Error` object if the `QuicEndpoint` was destroyed because
  of an error.

When all of the `QuicEndpoint` instances associated with a `QuicSocket` have
closed, the `QuicEndpoint` will also automatically close.

#### Event: `'error'`
<!-- YAML
added: REPLACEME
-->

Emitted before the `'close'` event if the `QuicSocket` was destroyed with an
`error`.

The `'error'` event will only ever be emitted once.

#### Event: `'listening'`
<!-- YAML
added: REPLACEME
-->

Emitted after `quicsocket.listen()` is called and the `QuicSocket` has started
listening for incoming `QuicServerSession`s.  The callback is invoked with
no arguments.

The `'listening'` event will only ever be emitted once.

#### Event: `'ready'`
<!-- YAML
added: REPLACEME
-->

Emitted once the `QuicSocket` has been bound to a local UDP port.

The `'ready'` event will only ever be emitted once.

#### Event: `'session'`
<!-- YAML
added: REPLACEME
-->

Emitted when a new `QuicServerSession` has been created. The callback is
invoked with a single argument providing the newly created `QuicServerSession`
object.

```js
const { createQuicSocket } = require('net');

const options = getOptionsSomehow();
const server = createQuicSocket({ server: options });

server.on('session', (session) => {
  // Attach session event listeners.
});

server.listen();
```

The `'session'` event will be emitted multiple times.

The `'session'` event handler *may* be an async function.

If the `'session'` event handler throws an error, or if it returns a `Promise`
that is rejected, the error will be handled by destroying the `QuicServerSession`
automatically and emitting a `'sessionError'` event on the `QuicSocket`.

#### Event: `'sessionError'`
<!--YAML
added: REPLACEME
-->

Emitted when an error occurs processing an event related to a specific
`QuicSession` instance. The callback is invoked with two arguments:

* `error` {Error} The error that was either thrown or rejected.
* `session` {QuicSession} The `QuicSession` instance that was destroyed.

The `QuicSession` instance will have been destroyed by the time the
`'sessionError'` event is emitted.

```js
const { createQuicSocket } = require('net');

const options = getOptionsSomehow();
const server = createQuicSocket({ server: options });
server.listen();

server.on('session', (session) => {
  throw new Error('boom');
});

server.on('sessionError', (error, session) => {
  console.log('error:', error.message);
});
```

#### `quicsocket.addEndpoint(options)`
<!-- YAML
added: REPLACEME
-->

* `options`: {Object} An object describing the local address to bind to.
  * `address` {string} The local address to bind to. This may be an IPv4 or
    IPv6 address or a host name. If a host name is given, it will be resolved
    to an IP address.
  * `port` {number} The local port to bind to.
  * `type` {string} Can be one of `'udp4'`, `'upd6'`, or `'udp6-only'` to
    use IPv4, IPv6, or IPv6 with dual-stack mode disabled.
    **Default**: `'udp4'`.
  * `lookup` {Function} A [custom DNS lookup function][].
    **Default**: undefined.
* Returns: {QuicEndpoint}

Creates and adds a new `QuicEndpoint` to the `QuicSocket` instance. An
error will be thrown if `quicsock.addEndpoint()` is called either after
the `QuicSocket` has already started binding to the local ports, or after
the `QuicSocket` has been destroyed.

#### `quicsocket.bound`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Will be `true` if the `QuicSocket` has been successfully bound to a local UDP
port. Initially the value is `false`.

`QuicSocket` instances are not bound to a local UDP port until the first time
eithe `quicsocket.listen()` or `quicsocket.connect()` is called. The `'ready'`
event will be emitted once the `QuicSocket` has been bound and the value of
`quicsocket.bound` will become `true`.

Read-only.

#### `quicsocket.boundDuration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time this `QuicSocket` has been bound to a local port.

Read-only.

#### `quicsocket.bytesReceived`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of bytes received by this `QuicSocket`.

Read-only.

#### `quicsocket.bytesSent`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of bytes sent by this `QuicSocket`.

Read-only.

#### `quicsocket.clientSessions`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of client `QuicSession` instances that have been associated
with this `QuicSocket`.

Read-only.

#### `quicsocket.close()`
<!-- YAML
added: REPLACEME
-->

* Returns: {Promise}

Gracefully closes the `QuicSocket`. Existing `QuicSession` instances will be
permitted to close naturally. New `QuicClientSession` and `QuicServerSession`
instances will not be allowed. The returns `Promise` will be resolved once
the `QuicSocket` is destroyed.

#### `quicsocket.connect(\[options\])`
<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `address` {string} The domain name or IP address of the QUIC server
    endpoint.
  * `alpn` {string} An ALPN protocol identifier.
  * `ca` {string|string[]|Buffer|Buffer[]} Optionally override the trusted CA
    certificates. Default is to trust the well-known CAs curated by Mozilla.
    Mozilla's CAs are completely replaced when CAs are explicitly specified
    using this option. The value can be a string or `Buffer`, or an `Array` of
    strings and/or `Buffer`s. Any string or `Buffer` can contain multiple PEM
    CAs concatenated together. The peer's certificate must be chainable to a CA
    trusted by the server for the connection to be authenticated. When using
    certificates that are not chainable to a well-known CA, the certificate's CA
    must be explicitly specified as a trusted or the connection will fail to
    authenticate.
    If the peer uses a certificate that doesn't match or chain to one of the
    default CAs, use the `ca` option to provide a CA certificate that the peer's
    certificate can match or chain to.
    For self-signed certificates, the certificate is its own CA, and must be
    provided.
    For PEM encoded certificates, supported types are "TRUSTED CERTIFICATE",
    "X509 CERTIFICATE", and "CERTIFICATE".
  * `cert` {string|string[]|Buffer|Buffer[]} Cert chains in PEM format. One cert
    chain should be provided per private key. Each cert chain should consist of
    the PEM formatted certificate for a provided private `key`, followed by the
    PEM formatted intermediate certificates (if any), in order, and not
    including the root CA (the root CA must be pre-known to the peer, see `ca`).
    When providing multiple cert chains, they do not have to be in the same
    order as their private keys in `key`. If the intermediate certificates are
    not provided, the peer will not be able to validate the certificate, and the
    handshake will fail.
  * `ciphers` {string} Cipher suite specification, replacing the default. For
    more information, see [modifying the default cipher suite][]. Permitted
    ciphers can be obtained via [`tls.getCiphers()`][]. Cipher names must be
    uppercased in order for OpenSSL to accept them.
  * `clientCertEngine` {string} Name of an OpenSSL engine which can provide the
    client certificate.
  * `crl` {string|string[]|Buffer|Buffer[]} PEM formatted CRLs (Certificate
    Revocation Lists).
  * `defaultEncoding` {string} The default encoding that is used when no
    encoding is specified as an argument to `quicstream.write()`. Default:
    `'utf8'`.
  * `dhparam` {string|Buffer} Diffie Hellman parameters, required for
    [Perfect Forward Secrecy][]. Use `openssl dhparam` to create the parameters.
    The key length must be greater than or equal to 1024 bits, otherwise an
    error will be thrown. It is strongly recommended to use 2048 bits or larger
    for stronger security. If omitted or invalid, the parameters are silently
    discarded and DHE ciphers will not be available.
  * `ecdhCurve` {string} A string describing a named curve or a colon separated
    list of curve NIDs or names, for example `P-521:P-384:P-256`, to use for
    ECDH key agreement. Set to `auto` to select the
    curve automatically. Use [`crypto.getCurves()`][] to obtain a list of
    available curve names. On recent releases, `openssl ecparam -list_curves`
    will also display the name and description of each available elliptic curve.
    **Default:** [`tls.DEFAULT_ECDH_CURVE`][].
  * `highWaterMark` {number} Total number of bytes that the `QuicStream` may
    buffer internally before the `quicstream.write()` function starts returning
    `false`. Default: `16384`.
  * `honorCipherOrder` {boolean} Attempt to use the server's cipher suite
    preferences instead of the client's. When `true`, causes
    `SSL_OP_CIPHER_SERVER_PREFERENCE` to be set in `secureOptions`, see
    [OpenSSL Options][] for more information.
  * `idleTimeout` {number}
  * `key` {string|string[]|Buffer|Buffer[]|Object[]} Private keys in PEM format.
    PEM allows the option of private keys being encrypted. Encrypted keys will
    be decrypted with `options.passphrase`. Multiple keys using different
    algorithms can be provided either as an array of unencrypted key strings or
    buffers, or an array of objects in the form `{pem: <string|buffer>[,
    passphrase: <string>]}`. The object form can only occur in an array.
    `object.passphrase` is optional. Encrypted keys will be decrypted with
    `object.passphrase` if provided, or `options.passphrase` if it is not.
  * `lookup` {Function} A [custom DNS lookup function][].
    **Default**: undefined.
  * `activeConnectionIdLimit` {number} Must be a value between `2` and `8`
    (inclusive). Default: `2`.
  * `congestionAlgorithm` {string} Must be either `'reno'` or `'cubic'`.
    **Default**: `'reno'`.
  * `maxAckDelay` {number}
  * `maxData` {number}
  * `maxUdpPayloadSize` {number}
  * `maxStreamDataBidiLocal` {number}
  * `maxStreamDataBidiRemote` {number}
  * `maxStreamDataUni` {number}
  * `maxStreamsBidi` {number}
  * `maxStreamsUni` {number}
  * `h3` {Object} HTTP/3 Specific Configuration Options
    * `qpackMaxTableCapacity` {number}
    * `qpackBlockedStreams` {number}
    * `maxHeaderListSize` {number}
    * `maxPushes` {number}
  * `passphrase` {string} Shared passphrase used for a single private key and/or
    a PFX.
  * `pfx` {string|string[]|Buffer|Buffer[]|Object[]} PFX or PKCS12 encoded
    private key and certificate chain. `pfx` is an alternative to providing
    `key` and `cert` individually. PFX is usually encrypted, if it is,
    `passphrase` will be used to decrypt it. Multiple PFX can be provided either
    as an array of unencrypted PFX buffers, or an array of objects in the form
    `{buf: <string|buffer>[, passphrase: <string>]}`. The object form can only
    occur in an array. `object.passphrase` is optional. Encrypted PFX will be
    decrypted with `object.passphrase` if provided, or `options.passphrase` if
    it is not.
  * `port` {number} The IP port of the remote QUIC server.
  * `preferredAddressPolicy` {string} `'accept'` or `'reject'`. When `'accept'`,
    indicates that the client will automatically use the preferred address
    advertised by the server.
  * `remoteTransportParams` {Buffer|TypedArray|DataView} The serialized remote
    transport parameters from a previously established session. These would
    have been provided as part of the `'sessionTicket'` event on a previous
    `QuicClientSession` object.
  * `qlog` {boolean} Whether to enable ['qlog'][] for this session.
    Default: `false`.
  * `requestOCSP` {boolean} If `true`, specifies that the OCSP status request
    extension will be added to the client hello and an `'OCSPResponse'` event
    will be emitted before establishing a secure communication.
  * `secureOptions` {number} Optionally affect the OpenSSL protocol behavior,
    which is not usually necessary. This should be used carefully if at all!
    Value is a numeric bitmask of the `SSL_OP_*` options from
    [OpenSSL Options][].
  * `servername` {string} The SNI servername.
  * `sessionTicket`: {Buffer|TypedArray|DataView} The serialized TLS Session
    Ticket from a previously established session. These would have been
    provided as part of the `'sessionTicket`' event on a previous
    `QuicClientSession` object.
  * `type`: {string} Identifies the type of UDP socket. The value must either
    be `'udp4'`, indicating UDP over IPv4, or `'udp6'`, indicating UDP over
    IPv6. **Default**: `'udp4'`.
* Returns: {Promise}

Returns a `Promise` that resolves a new `QuicClientSession`.

#### `quicsocket.destroy(\[error\])`
<!-- YAML
added: REPLACEME
-->

* `error` {any}

Destroys the `QuicSocket` then emits the `'close'` event when done. The `'error'`
event will be emitted after `'close'` if the `error` is not `undefined`.

#### `quicsocket.destroyed`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Will be `true` if the `QuicSocket` has been destroyed.

Read-only.

#### `quicsocket.duration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time this `QuicSocket` has been active,

Read-only.

#### `quicsocket.endpoints`
<!-- YAML
added: REPLACEME
-->

* Type: {QuicEndpoint[]}

An array of `QuicEndpoint` instances associated with the `QuicSocket`.

Read-only.

#### `quicsocket.listen(\[options\])`
<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `alpn` {string} A required ALPN protocol identifier.
  * `ca` {string|string[]|Buffer|Buffer[]} Optionally override the trusted CA
    certificates. Default is to trust the well-known CAs curated by Mozilla.
    Mozilla's CAs are completely replaced when CAs are explicitly specified
    using this option. The value can be a string or `Buffer`, or an `Array` of
    strings and/or `Buffer`s. Any string or `Buffer` can contain multiple PEM
    CAs concatenated together. The peer's certificate must be chainable to a CA
    trusted by the server for the connection to be authenticated. When using
    certificates that are not chainable to a well-known CA, the certificate's CA
    must be explicitly specified as a trusted or the connection will fail to
    authenticate.
    If the peer uses a certificate that doesn't match or chain to one of the
    default CAs, use the `ca` option to provide a CA certificate that the peer's
    certificate can match or chain to.
    For self-signed certificates, the certificate is its own CA, and must be
    provided.
    For PEM encoded certificates, supported types are "TRUSTED CERTIFICATE",
    "X509 CERTIFICATE", and "CERTIFICATE".
  * `cert` {string|string[]|Buffer|Buffer[]} Cert chains in PEM format. One cert
    chain should be provided per private key. Each cert chain should consist of
    the PEM formatted certificate for a provided private `key`, followed by the
    PEM formatted intermediate certificates (if any), in order, and not
    including the root CA (the root CA must be pre-known to the peer, see `ca`).
    When providing multiple cert chains, they do not have to be in the same
    order as their private keys in `key`. If the intermediate certificates are
    not provided, the peer will not be able to validate the certificate, and the
    handshake will fail.
  * `ciphers` {string} Cipher suite specification, replacing the default. For
    more information, see [modifying the default cipher suite][]. Permitted
    ciphers can be obtained via [`tls.getCiphers()`][]. Cipher names must be
    uppercased in order for OpenSSL to accept them.
  * `clientCertEngine` {string} Name of an OpenSSL engine which can provide the
    client certificate.
  * `crl` {string|string[]|Buffer|Buffer[]} PEM formatted CRLs (Certificate
    Revocation Lists).
  * `defaultEncoding` {string} The default encoding that is used when no
    encoding is specified as an argument to `quicstream.write()`. Default:
    `'utf8'`.
  * `dhparam` {string|Buffer} Diffie Hellman parameters, required for
    [Perfect Forward Secrecy][]. Use `openssl dhparam` to create the parameters.
    The key length must be greater than or equal to 1024 bits, otherwise an
    error will be thrown. It is strongly recommended to use 2048 bits or larger
    for stronger security. If omitted or invalid, the parameters are silently
    discarded and DHE ciphers will not be available.
  * `earlyData` {boolean} Set to `false` to disable 0RTT early data.
    Default: `true`.
  * `ecdhCurve` {string} A string describing a named curve or a colon separated
    list of curve NIDs or names, for example `P-521:P-384:P-256`, to use for
    ECDH key agreement. Set to `auto` to select the
    curve automatically. Use [`crypto.getCurves()`][] to obtain a list of
    available curve names. On recent releases, `openssl ecparam -list_curves`
    will also display the name and description of each available elliptic curve.
    **Default:** [`tls.DEFAULT_ECDH_CURVE`][].
  * `highWaterMark` {number} Total number of bytes that `QuicStream` instances
    may buffer internally before the `quicstream.write()` function starts
    returning `false`. Default: `16384`.
  * `honorCipherOrder` {boolean} Attempt to use the server's cipher suite
    references instead of the client's. When `true`, causes
    `SSL_OP_CIPHER_SERVER_PREFERENCE` to be set in `secureOptions`, see
    [OpenSSL Options][] for more information.
  * `idleTimeout` {number}
  * `key` {string|string[]|Buffer|Buffer[]|Object[]} Private keys in PEM format.
    PEM allows the option of private keys being encrypted. Encrypted keys will
    be decrypted with `options.passphrase`. Multiple keys using different
    algorithms can be provided either as an array of unencrypted key strings or
    buffers, or an array of objects in the form `{pem: <string|buffer>[,
    passphrase: <string>]}`. The object form can only occur in an array.
    `object.passphrase` is optional. Encrypted keys will be decrypted with
    `object.passphrase` if provided, or `options.passphrase` if it is not.
  * `lookup` {Function} A [custom DNS lookup function][].
    **Default**: undefined.
  * `activeConnectionIdLimit` {number}
  * `congestionAlgorithm` {string} Must be either `'reno'` or `'cubic'`.
    **Default**: `'reno'`.
  * `maxAckDelay` {number}
  * `maxData` {number}
  * `maxUdpPayloadSize` {number}
  * `maxStreamsBidi` {number}
  * `maxStreamsUni` {number}
  * `maxStreamDataBidiLocal` {number}
  * `maxStreamDataBidiRemote` {number}
  * `maxStreamDataUni` {number}
  * `passphrase` {string} Shared passphrase used for a single private key
    and/or a PFX.
  * `pfx` {string|string[]|Buffer|Buffer[]|Object[]} PFX or PKCS12 encoded
    private key and certificate chain. `pfx` is an alternative to providing
    `key` and `cert` individually. PFX is usually encrypted, if it is,
    `passphrase` will be used to decrypt it. Multiple PFX can be provided either
    as an array of unencrypted PFX buffers, or an array of objects in the form
    `{buf: <string|buffer>[, passphrase: <string>]}`. The object form can only
    occur in an array. `object.passphrase` is optional. Encrypted PFX will be
    decrypted with `object.passphrase` if provided, or `options.passphrase` if
    it is not.
  * `preferredAddress` {Object}
    * `address` {string}
    * `port` {number}
    * `type` {string} `'udp4'` or `'udp6'`.
  * `requestCert` {boolean} Request a certificate used to authenticate the
    client.
  * `rejectUnauthorized` {boolean} If not `false` the server will reject any
    connection which is not authorized with the list of supplied CAs. This
    option only has an effect if `requestCert` is `true`. Default: `true`.
  * `secureOptions` {number} Optionally affect the OpenSSL protocol behavior,
    which is not usually necessary. This should be used carefully if at all!
    Value is a numeric bitmask of the `SSL_OP_*` options from
    [OpenSSL Options][].
  * `sessionIdContext` {string} Opaque identifier used by servers to ensure
    session state is not shared between applications. Unused by clients.
* Returns: {Promise}

Listen for new peer-initiated sessions. Returns a `Promise` that is resolved
once the `QuicSocket` is actively listening.

#### `quicsocket.listenDuration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time this `QuicSocket` has been listening for connections.

Read-only

#### `quicsocket.listening`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the `QuicSocket` is listening for new connections.

Read-only.

#### `quicsocket.packetsIgnored`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of packets received by this `QuicSocket` that have been ignored.

Read-only.

#### `quicsocket.packetsReceived`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of packets successfully received by this `QuicSocket`.

Read-only

#### `quicsocket.packetsSent`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of packets sent by this `QuicSocket`.

Read-only

#### `quicsocket.pending`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Set to `true` if the socket is not yet bound to the local UDP port.

Read-only.

#### `quicsocket.ref()`
<!-- YAML
added: REPLACEME
-->

#### `quicsocket.serverBusy`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean} When `true`, the `QuicSocket` will reject new connections.

Setting `quicsocket.serverBusy` to `true` will tell the `QuicSocket`
to reject all new incoming connection requests using the `SERVER_BUSY` QUIC
error code. To begin receiving connections again, disable busy mode by setting
`quicsocket.serverBusy = false`.

#### `quicsocket.serverBusyCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of `QuicSession` instances rejected due to server busy status.

Read-only.

#### `quicsocket.serverSessions`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of server `QuicSession` instances that have been associated with
this `QuicSocket`.

Read-only.

#### `quicsocket.setDiagnosticPacketLoss(options)`
<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `rx` {number} A value in the range `0.0` to `1.0` that specifies the
    probability of received packet loss.
  * `tx` {number} A value in the range `0.0` to `1.0` that specifies the
    probability of transmitted packet loss.

The `quicsocket.setDiagnosticPacketLoss()` method is a diagnostic only tool
that can be used to *simulate* packet loss conditions for this `QuicSocket`
by artificially dropping received or transmitted packets.

This method is *not* to be used in production applications.

#### `quicsocket.statelessReset`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean} `true` if stateless reset processing is enabled; `false`
  if disabled.

By default, a listening `QuicSocket` will generate stateless reset tokens when
appropriate. The `disableStatelessReset` option may be set when the
`QuicSocket` is created to disable generation of stateless resets. The
`quicsocket.statelessReset` property allows stateless reset to be turned on and
off dynamically through the lifetime of the `QuicSocket`.

#### `quicsocket.statelessResetCount`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of stateless resets that have been sent.

Read-only.

#### `quicsocket.unref();`
<!-- YAML
added: REPLACEME
-->

### Class: `QuicStream extends stream.Duplex`
<!-- YAML
added: REPLACEME
-->

* Extends: {stream.Duplex}

#### Event: `'blocked'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicStream` has been prevented from sending queued data for
the `QuicStream` due to congestion control.

#### Event: `'close'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicStream` has is completely closed and the underlying
resources have been freed.

#### Event: `'data'`
<!-- YAML
added: REPLACEME
-->

#### Event: `'end'`
<!-- YAML
added: REPLACEME
-->

#### Event: `'error'`
<!-- YAML
added: REPLACEME
-->

#### Event: `'informationalHeaders'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicStream` has received a block of informational headers.

Support for headers depends entirely on the QUIC Application used as identified
by the `alpn` configuration option. In QUIC Applications that support headers,
informational header blocks typically come before initial headers.

The event handler is invoked with a single argument representing the block of
Headers as an object.

```js
stream('informationalHeaders', (headers) => {
  // Use headers
});
```

#### Event: `'initialHeaders'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicStream` has received a block of initial headers.

Support for headers depends entirely on the QUIC Application used as identified
by the `alpn` configuration option. HTTP/3, for instance, supports two kinds of
initial headers: request headers for HTTP request messages and response headers
for HTTP response messages. For HTTP/3 QUIC streams, request and response
headers are each emitted using the `'initialHeaders'` event.

The event handler is invoked with a single argument representing the block of
Headers as an object.

```js
stream('initialHeaders', (headers) => {
  // Use headers
});
```

#### Event: `'trailingHeaders'`
<!-- YAML
added: REPLACEME
-->

Emitted when the `QuicStream` has received a block of trailing headers.

Support for headers depends entirely on the QUIC Application used as identified
by the `alpn` configuration option. Trailing headers typically follow any data
transmitted on the `QuicStream`, and therefore typically emit sometime after the
last `'data'` event but before the `'close'` event. The precise timing may
vary from one QUIC application to another.

The event handler is invoked with a single argument representing the block of
Headers as an object.

```js
stream('trailingHeaders', (headers) => {
  // Use headers
});
```

#### Event: `'readable'`
<!-- YAML
added: REPLACEME
-->

#### `quicstream.bidirectional`
<!--YAML
added: REPLACEME
-->

* Type: {boolean}

When `true`, the `QuicStream` is bidirectional. Both the readable and
writable sides of the `QuicStream` `Duplex` are open.

Read-only.

#### `quicstream.bytesReceived`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes received for this `QuicStream`.

Read-only.

#### `quicstream.bytesSent`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes sent by this `QuicStream`.

Read-only.

#### `quicstream.clientInitiated`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Will be `true` if the `QuicStream` was initiated by a `QuicClientSession`
instance.

Read-only.

#### `quicstream.close()`
<!-- YAML
added: REPLACEME
-->

* Returns: {Promise`}

Closes the `QuicStream` by ending both sides of the `QuicStream` `Duplex`.
Returns a `Promise` that is resolved once the `QuicStream` has been destroyed.

#### `quicstream.dataAckHistogram`
<!-- YAML
added: REPLACEME
-->

TBD

#### `quicstream.dataRateHistogram`
<!-- YAML
added: REPLACEME
-->

TBD

#### `quicstream.dataSizeHistogram`
<!-- YAML
added: REPLACEME
-->
TBD

#### `quicstream.duration`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The length of time the `QuicStream` has been active.

Read-only.

#### `quicstream.finalSize`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The total number of bytes successfully received by the `QuicStream`.

Read-only.

#### `quicstream.id`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The numeric identifier of the `QuicStream`.

Read-only.

#### `quicstream.maxAcknowledgedOffset`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The highest acknowledged data offset received for this `QuicStream`.

Read-only.

#### `quicstream.maxExtendedOffset`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum extended data offset that has been reported to the connected peer.

Read-only.

#### `quicstream.maxReceivedOffset`
<!-- YAML
added: REPLACEME
-->

* Type: {number}

The maximum received offset for this `QuicStream`.

Read-only.

#### `quicstream.pushStream(headers\[, options\])`
<!-- YAML
added: REPLACEME
-->

* `headers` {Object} An object representing a block of headers to be
  transmitted with the push promise.
* `options` {Object}
  * `highWaterMark` {number} Total number of bytes that the `QuicStream` may
    buffer internally before the `quicstream.write()` function starts returning
    `false`. Default: `16384`.
  * `defaultEncoding` {string} The default encoding that is used when no
    encoding is specified as an argument to `quicstream.write()`. Default:
    `'utf8'`.

* Returns: {QuicStream}

If the selected QUIC application protocol supports push streams, then the
`pushStream()` method will initiate a new push promise and create a new
unidirectional `QuicStream` object used to fulfill that push.

Currently only HTTP/3 supports the use of `pushStream()`.

If the selected QUIC application protocol does not support push streams, an
error will be thrown.

#### `quicstream.serverInitiated`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Will be `true` if the `QuicStream` was initiated by a `QuicServerSession`
instance.

Read-only.

#### `quicstream.session`
<!-- YAML
added: REPLACEME
-->

* Type: {QuicSession}

The `QuicServerSession` or `QuicClientSession` to which the
`QuicStream` belongs.

Read-only.

#### `quicstream.sendFD(fd\[, options\])`
<!-- YAML
added: REPLACEME
-->

* `fd` {number|FileHandle} A readable file descriptor.
* `options` {Object}
  * `offset` {number} The offset position at which to begin reading.
    Default: `-1`.
  * `length` {number} The amount of data from the fd to send.
    Default: `-1`.

Instead of using a `Quicstream` as a writable stream, send data from a given
file descriptor.

If `offset` is set to a non-negative number, reading starts from that position
and the file offset will not be advanced.
If `length` is set to a non-negative number, it gives the maximum number of
bytes that are read from the file.

The file descriptor or `FileHandle` is not closed when the stream is closed,
so it will need to be closed manually once it is no longer needed.
Using the same file descriptor concurrently for multiple streams
is not supported and may result in data loss. Re-using a file descriptor
after a stream has finished is supported.

#### `quicstream.sendFile(path\[, options\])`
<!-- YAML
added: REPLACEME
-->

* `path` {string|Buffer|URL}
* `options` {Object}
  * `onError` {Function} Callback function invoked in the case of an
    error before send.
  * `offset` {number} The offset position at which to begin reading.
    Default: `-1`.
  * `length` {number} The amount of data from the fd to send.
    Default: `-1`.

Instead of using a `QuicStream` as a writable stream, send data from a given
file path.

The `options.onError` callback will be called if the file could not be opened.
If `offset` is set to a non-negative number, reading starts from that position.
If `length` is set to a non-negative number, it gives the maximum number of
bytes that are read from the file.

#### `quicstream.submitInformationalHeaders(headers)`
<!-- YAML
added: REPLACEME
-->
* `headers` {Object}

TBD

#### `quicstream.submitInitialHeaders(headers)`
<!-- YAML
added: REPLACEME
-->
* `headers` {Object}

TBD

#### `quicstream.submitTrailingHeaders(headers)`
<!-- YAML
added: REPLACEME
-->
* `headers` {Object}

TBD

#### `quicstream.unidirectional`
<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Will be `true` if the `QuicStream` is undirectional. Whether the `QuicStream`
will be readable or writable depends on whether the `quicstream.session` is
a `QuicClientSession` or `QuicServerSession`, and whether the `QuicStream`
was initiated locally or remotely.

| `quicstream.session` | `quicstream.serverInitiated` | Readable | Writable |
| -------------------- | ---------------------------- | -------- | -------- |
|  `QuicClientSession` |            `true`            |     Y    |     N    |
|  `QuicServerSession` |            `true`            |     N    |     Y    |
|  `QuicClientSession` |            `false`           |     N    |     Y    |
|  `QuicServerSession` |            `false`           |     Y    |     N    |

| `quicstream.session` | `quicstream.clientInitiated` | Readable | Writable |
| -------------------- | ---------------------------- | -------- | -------- |
|  `QuicClientSession` |            `true`            |     N    |     Y    |
|  `QuicServerSession` |            `true`            |     Y    |     N    |
|  `QuicClientSession` |            `false`           |     Y    |     N    |
|  `QuicServerSession` |            `false`           |     N    |     Y    |

Read-only.

## Additional notes

### Custom DNS lookup functions

By default, the QUIC implementation uses the `dns` module's
[promisified version of `lookup()`][] to resolve domains names
into IP addresses. For most typical use cases, this will be
sufficient. However, it is possible to pass a custom `lookup`
function as an option in several places throughout the QUIC API:

* `net.createQuicSocket()`
* `quicsocket.addEndpoint()`
* `quicsocket.connect()`
* `quicsocket.listen()`

The custom `lookup` function must return a `Promise` that is
resolved once the lookup is complete. It will be invoked with
two arguments:

* `address` {string|undefined} The host name to resolve, or
  `undefined` if no host name was provided.
* `family` {number} One of `4` or `6`, identifying either
  IPv4 or IPv6.

```js
async function myCustomLookup(address, type) {
  // TODO(@jasnell): Make this example more useful
  return resolveTheAddressSomehow(address, type);
}
```

[`crypto.getCurves()`]: crypto.html#crypto_crypto_getcurves
[`stream.Readable`]: #stream_class_stream_readable
[`tls.DEFAULT_ECDH_CURVE`]: #tls_tls_default_ecdh_curve
[`tls.getCiphers()`]: tls.html#tls_tls_getciphers
[ALPN]: https://tools.ietf.org/html/rfc7301
[RFC 4007]: https://tools.ietf.org/html/rfc4007
[Certificate Object]: https://nodejs.org/dist/latest-v12.x/docs/api/tls.html#tls_certificate_object
[custom DNS lookup function]: #quic_custom_dns_lookup_functions
[modifying the default cipher suite]: tls.html#tls_modifying_the_default_tls_cipher_suite
[OpenSSL Options]: crypto.html#crypto_openssl_options
[Perfect Forward Secrecy]: #tls_perfect_forward_secrecy
[promisified version of `lookup()`]: dns.html#dns_dnspromises_lookup_hostname_options
['qlog']: #quic_quicsession_qlog
[qlog standard]: https://tools.ietf.org/id/draft-marx-qlog-event-definitions-quic-h3-00.html
