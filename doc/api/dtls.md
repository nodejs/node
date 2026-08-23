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
  * `secureContext` {DTLSSecureContext} A context from
    [`dtls.createSecureContext()`][] to use instead of building one from the
    credential options below. Must have been created with `isServer: true`.
    Cannot be combined with any option the context already carries.
  * `sni` {Object|Function} Server Name Indication. A map of host names to the
    identity to serve them with, or a function returning one. See
    [Server Name Indication][].
  * `passphrase` {string} Passphrase to decrypt `key`, if it is encrypted.
    Ignored when `key` is not encrypted. Unlike `key` and `cert`, this must be
    a string, matching [`tls.createSecureContext()`][].
  * `port` {number} Port to bind to. **Required.**
  * `host` {string} Address to bind to. **Default:** `'0.0.0.0'`.
  * `ca` {string|Buffer|string\[]|Buffer\[]} CA certificates in PEM format.
  * `ciphers` {string} OpenSSL cipher list string.
  * `alpn` {string\[]|Buffer} ALPN protocol names. Each name must be between
    1 and 255 bytes. A `Buffer` must already be in ALPN wire format: one
    length byte followed by that many bytes, repeated.
  * `srtp` {string} Colon-separated SRTP protection profile names
    (e.g., `'SRTP_AES128_CM_SHA1_80:SRTP_AEAD_AES_128_GCM'`).
  * `requestCert` {boolean} Request a certificate from the client.
    **Default:** `false`.
  * `rejectUnauthorized` {boolean} Only has an effect together with
    `requestCert`. When `true`, a client that presents no certificate, or one
    that does not chain to a trusted CA, is rejected during the handshake and
    receives a TLS alert. When `false`, the certificate is still requested and
    verified but the handshake completes regardless, leaving the decision to
    the application via [`session.authorized`][]. **Default:** `true`.
  * `mtu` {number}
  * `handshakeTimeout` {number} Milliseconds a handshake may take before it is
    abandoned. `0` disables it. **Default:** `60000`. See
    [Handshake timeout][]. Maximum size in bytes of a DTLS datagram. **Default:**
    `1200`.
    **Default:** `1200`.
  * `maxSessions` {number} The maximum number of concurrent sessions the
    endpoint will hold. Set to `0` for no limit. **Default:** `10000`.
  * `maxSessionsPerHost` {number} The maximum number of concurrent sessions
    from any single source IP address, ignoring port. Set to `0` for no limit.
    **Default:** `1000`.
  * `sessionIdContext` {string} Opaque identifier scoping resumable sessions
    to this server, at most 32 bytes. **Default:** a value derived from
    `process.argv`, as in `tls.createServer()`.
* Returns: {DTLSEndpoint}

Creates a DTLS server bound to the specified address and port. The server
uses automatic HMAC-based cookie exchange for DoS protection. See
[Denial of service][].

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

* `host` {string} Remote host to connect to, as an IPv4 or IPv6 literal.
  Host names are not resolved.
* `port` {number} Remote port to connect to.
* `options` {Object}
  * `ca` {string|Buffer|string\[]|Buffer\[]} CA certificates in PEM format.
  * `cert` {string|Buffer} Client certificate in PEM format.
  * `key` {string|Buffer} Client private key in PEM format.
  * `secureContext` {DTLSSecureContext} A context from
    [`dtls.createSecureContext()`][] to use instead of building one from the
    credential options below. Must **not** have been created with
    `isServer: true`. Cannot be combined with any option the context already
    carries.
  * `psk` {Object|Function} A pre-shared key as `{ identity, key }`, or a
    function returning one. See [Pre-shared keys][].
  * `session` {Buffer} A session from [`session.session`][] on an earlier
    connection, to resume rather than handshake in full. See
    [Session resumption][].
  * `passphrase` {string} Passphrase to decrypt `key`, if it is encrypted.
    Ignored when `key` is not encrypted. Unlike `key` and `cert`, this must be
    a string, matching [`tls.createSecureContext()`][].
  * `rejectUnauthorized` {boolean} When `true`, the server's certificate must
    both chain to a trusted CA and match the expected identity (`servername`,
    or `host` when `servername` is not set); otherwise the handshake is
    aborted and `session.opened` rejects. When `false`, the certificate is not
    verified. **Default:** `true`.
  * `servername` {string} Server name used for the SNI (Server Name
    Indication) extension and as the identity checked during certificate
    verification. **Default:** the `host` argument. Set to `''` to disable SNI.
    SNI is never sent for IP address literals.
  * `bindHost` {string} Local bind address. **Default:** `'::'` when `host` is an
    IPv6 literal, otherwise `'0.0.0.0'`. The local socket must be in the same
    address family as the peer.
  * `bindPort` {number} Local bind port. **Default:** `0` (ephemeral).
  * `alpn` {string\[]|Buffer} ALPN protocol names. Each name must be between
    1 and 255 bytes. A `Buffer` must already be in ALPN wire format: one
    length byte followed by that many bytes, repeated.
  * `srtp` {string} SRTP protection profile names.
  * `mtu` {number} Maximum size in bytes of a DTLS datagram. **Default:**
    `1200`.
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

## `dtls.createSecureContext([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `alpn` {string\[]} ALPN protocols.
  * `ca` {string|Buffer|Array} CA certificates in PEM format. When omitted,
    the bundled default certificate authorities are used.
  * `cert` {string|Buffer} Certificate in PEM format.
  * `ciphers` {string} OpenSSL cipher suite list.
  * `ecdhCurve` {string} Named curve or curve list for ECDH.
  * `isServer` {boolean} Build a context for a server. **Default:** `false`.
  * `key` {string|Buffer} Private key in PEM format.
  * `passphrase` {string} Passphrase for `key`, if it is encrypted.
  * `rejectUnauthorized` {boolean} Verification behaviour, as for
    [`dtls.listen()`][] and [`dtls.connect()`][].
  * `requestCert` {boolean} Request a certificate from the peer. Servers only.
  * `sessionIdContext` {string} Session id context. Servers only.
  * `psk` {Object|Function} Pre-shared keys. See [Pre-shared keys][].
  * `pskIdentityHint` {string} Identity hint to advertise. Servers only.
  * `srtp` {string} SRTP profile list.
  * `ticketKeys` {Buffer} Session ticket keys, for resuming sessions across
    endpoints and restarts. Servers only. See [Session resumption][].
* Returns: {DTLSSecureContext}

Creates a reusable secure context. Pass it to [`dtls.listen()`][] or
[`dtls.connect()`][] as `secureContext` in place of the credential options.

A context holds a parsed certificate and key and, when `ca` is given, its own
certificate store; roughly 28 KiB in total. Building one per connection is
therefore expensive in memory rather than in time -- two thousand of them cost
about 54 MiB, against 2 MiB when a single context is shared. Clients opening
many connections should build the context once.

The peer identity checked during verification is **not** part of the context.
It is bound to each connection from `servername` (or the host), so one context
can be used against different peers and still reject the wrong certificate.

`isServer` is fixed when the context is created, because it selects the
underlying OpenSSL method. Passing a server context to [`dtls.connect()`][],
or a client context to [`dtls.listen()`][], throws.

```mjs
import { connect, createSecureContext, listen } from 'node:dtls';
import { readFileSync } from 'node:fs';

const serverContext = createSecureContext({
  cert: readFileSync('server-cert.pem'),
  key: readFileSync('server-key.pem'),
  isServer: true,
});

// One context, several endpoints.
const a = listen(onsession, { secureContext: serverContext, port: 5684 });
const b = listen(onsession, { secureContext: serverContext, port: 5685 });

const clientContext = createSecureContext({
  ca: readFileSync('ca-cert.pem'),
});

// One context, many connections, each verified against its own name.
const s1 = connect('a.example.com', 5684, { secureContext: clientContext });
const s2 = connect('b.example.com', 5684, { secureContext: clientContext });
```

## Server Name Indication

An endpoint can serve more than one identity by giving `listen()` an `sni`
map, or a function. Each key of a map is a host name and each value is either
a
[`DTLSSecureContext`][] created with `isServer: true`, or a plain object of
the same options [`dtls.createSecureContext()`][] takes:

```mjs
import { createSecureContext, listen } from 'node:dtls';
import { readFileSync } from 'node:fs';

const endpoint = listen(onsession, {
  cert: readFileSync('default-cert.pem'),
  key: readFileSync('default-key.pem'),
  port: 5684,
  sni: {
    'api.example.com': {
      cert: readFileSync('api-cert.pem'),
      key: readFileSync('api-key.pem'),
    },
    'www.example.com': createSecureContext({
      cert: readFileSync('www-cert.pem'),
      key: readFileSync('www-key.pem'),
      isServer: true,
    }),
    '*': {
      cert: readFileSync('default-cert.pem'),
      key: readFileSync('default-key.pem'),
    },
  },
});
```

The `'*'` key is the fallback, used when the client's name matches nothing and
when the client sends no name at all. **Without it, an unmatched name is
refused with an `unrecognized_name` alert** rather than falling back to the
endpoint's own `cert` and `key`; providing an `sni` map is taken to mean that
only the names in it are served. [`tls.createServer()`][] differs here: its
`SNICallback` falls back to the default identity silently.

Verification follows the selected identity, so an entry carrying its own `ca`
accepts only client certificates issued under it. `requestCert` and
`rejectUnauthorized` are not per-identity: they belong to the endpoint and
apply to every name it serves.

A function may be given instead of a map, for identities that are chosen
rather than enumerated:

```mjs
listen(onsession, {
  port: 5684,
  cert,
  key,
  sni: (servername) => contexts.get(servername),
});
```

It is called with the name the client asked for, or `undefined` if the client
sent no SNI extension, and returns what a map entry holds: a
[`dtls.createSecureContext()`][] result or the options to build one. Returning
nothing declines the name, which is refused exactly as an unmatched map with no
`'*'` entry is, rather than falling back to the endpoint's own certificate.

The function runs during the handshake and must return synchronously, so it
cannot consult a database. Returning a prepared context is worth doing:
building one from options parses the certificate again on every handshake.

An exception thrown by the function fails that handshake and is reported to the
session's error handler, like any other handshake failure. It does not reach
the process as an uncaught exception.

The certificate and the cipher list both follow the selected context. The
pre-shared key callbacks do not: OpenSSL binds those to the connection when it
is created, before a name is known, so they come from the endpoint's own
context regardless of which identity is selected.

A connection refused for an unrecognized name still reaches the `listen()`
callback: the session exists once the client's address is validated, which
happens before the name is examined. It then fails like any other handshake
failure.

## Denial of service

Cookie exchange proves a peer can receive at its claimed address, but it does
not limit how many sessions that peer may then establish, and each session
holds a TLS state machine, two buffers and a timer. `maxSessions` bounds the
total; `maxSessionsPerHost` is what prevents one peer from taking all of it.
A peer refused by either cap is answered with silence rather than an alert,
because replying to an address that has not completed cookie exchange would
create an amplification vector; a legitimate client retransmits and is
admitted once there is room. Refusals are counted by
[`endpointStats.serverRefusedCount`][].

Deployments serving many clients behind a single NAT may need to raise
`maxSessionsPerHost`.

## Handshake timeout

A handshake that never finishes is abandoned after `handshakeTimeout`
milliseconds, and its session error is `DTLS handshake timeout`.

OpenSSL already gives up on its own, but only after twelve retransmits on a
doubling backoff capped at 60 seconds -- around eight minutes in total. Until
then the session holds its place against `maxSessions` (see
[Denial of service][]),
so handshakes that are started and abandoned can occupy an endpoint for the
cost of starting them. That needs no spoofing: the peer completes the cookie
exchange and then simply stops.

The two limits coexist and whichever comes first ends the handshake. The
retransmit schedule itself is untouched, deliberately -- compressing it to
force earlier failure would cause spurious retransmissions on exactly the
lossy links DTLS is meant for.

The timeout covers resumed and PSK handshakes as well, and stops applying once
the handshake completes; it is not an idle timeout.

A handshake can stall without either peer being at fault or aware.
DTLS discards records it cannot authenticate rather than answering them
(RFC 6347 section 4.1.2.1), so a mismatched pre-shared key or a cipher list
with nothing in common produces silence rather than an alert. This timeout is
what ends those.

## Pre-shared keys

DTLS can authenticate with a key both peers already hold instead of a
certificate (RFC 4279). This is how it is usually deployed to constrained
devices, which frequently have no certificate at all.

A server gives the identities it accepts; a client gives the one it is. No
certificate is needed on either side:

```mjs
import { connect, listen } from 'node:dtls';

const endpoint = listen(onsession, {
  port: 5684,
  psk: { 'device-42': deviceKey },
});

const client = connect('gateway.example', 5684, {
  psk: { identity: 'device-42', key: deviceKey },
});
```

Either side may pass a function instead, for keys that are looked up or
derived rather than known up front. A server's is called with the identity the
client offered and returns the key, or nothing to refuse it. A client's is
called with the server's identity hint, if it sent one, and returns
`{ identity, key }`:

```mjs
listen(onsession, {
  port: 5684,
  psk: (identity) => deriveKey(masterSecret, identity),
});
```

The callback runs during the handshake and must return synchronously, so it
cannot consult a database. Where both are given, the map is checked first and
the callback is only reached when the map has no answer -- a configuration
using only the map never runs JavaScript inside the handshake.

An exception thrown by the callback fails that handshake and is reported to
the session's error handler. It does not reach the process as an uncaught
exception.

### Cipher suites

The default cipher list excludes PSK, so giving `psk` without `ciphers`
enables the PSK suites. Supplying `ciphers` disables that and uses exactly
what was asked for.

A server keeps the certificate suites as well, since it may serve both kinds
of client on one port. A client does not: a client that configured a
pre-shared key and no CA wants the key, and leaving the certificate suites
enabled would let a server choose one, failing the handshake while verifying a
certificate the caller never meant to rely on.

Forward-secret PSK key exchanges are preferred over plain PSK of the same
strength. Plain PSK derives its keys from the shared secret alone, so anyone
who later learns that key can decrypt traffic they recorded earlier. `RSA-PSK`
is excluded: it needs a certificate and adds no forward secrecy.

CoAP requires `TLS_PSK_WITH_AES_128_CCM_8` (RFC 7252), whose 64-bit
authentication tag OpenSSL rejects at security level 1 and above. Node.js
default is above it, so that suite has to be asked for explicitly and with the
security level lowered:

```mjs
listen(onsession, { port: 5684, psk, ciphers: 'PSK-AES128-CCM8@SECLEVEL=0' });
```

### Failure modes

A wrong key does not produce an error. The identity only names the key, so the
handshake proceeds and the two sides derive different secrets; the first
record that fails authentication is then discarded rather than answered, since
DTLS discards invalid records instead of replying to them (RFC 6347 section
4.1.2.1). Neither peer is told anything and both retransmit.

A cipher list with nothing in common behaves the same way, which is what makes
the `CCM8` case above present as a stall rather than a rejection. Both are
ended by [`handshakeTimeout`][], after 60 seconds by default.

An identity the server does not recognise is refused outright, and the client
sees the handshake fail.

## Session resumption

A resumed handshake skips the server's certificate, which matters more here
than it does over TCP: the `Certificate` flight is fragmented across several
datagrams, and losing any one of them costs a retransmission timeout. Measured
on loopback, a full handshake has the server send 1850 bytes in 4 packets
against 280 bytes in 3 for a resumed one.

A client reads [`session.session`][] once the session is open and passes it to
a later [`dtls.connect()`][]:

```mjs
import { connect } from 'node:dtls';

const first = connect('device.example', 5684, { ca });
await first.opened;
const ticket = first.session;        // Buffer.
await first.close();

const second = connect('device.example', 5684, { ca, session: ticket });
await second.opened;
console.log(second.reused);          // True.
```

A session that the server will not accept -- expired, or issued by a different
endpoint -- is not an error. The handshake simply proceeds in full, and
[`session.reused`][] is `false`.

The cookie exchange still happens for a resumed handshake, so resumption is not
a way around the address validation described under [Denial of service][].

### Binding to the authenticated host

A session may only be resumed against the identity it was authenticated for --
the `servername`, or the host when there is none. Reusing it for anything else
throws.

This is not a convenience check. A resumed handshake does not re-send or
re-verify the peer's certificate; it inherits the authenticated identity of the
original session. Replaying a session against a different host would therefore
skip verification while appearing to succeed. For the same reason a `session`
that did not come from [`session.session`][] is rejected outright: nothing
records which identity it belongs to, so it cannot be checked.

### Ticket keys

The key that encrypts session tickets is generated at random for each context,
so by default a ticket is only good for the endpoint that issued it and only
until the process restarts. Give every endpoint the same `ticketKeys` to let
tickets be resumed across a restart or a cluster:

```mjs
import { listen } from 'node:dtls';
import { randomBytes } from 'node:crypto';

const ticketKeys = randomBytes(80);    // Share this between processes.
const endpoint = listen(onsession, { cert, key, port: 5684, ticketKeys });
```

The length is OpenSSL's: a key name followed by an HMAC key and an AES key. It
differs from the 48 bytes [`tls.createServer()`][] uses, which is a layout
`node:tls` defines for itself. Supplying the wrong length throws and reports
the length expected.

Ticket keys are long-lived secrets. Anyone holding them can decrypt tickets and
recover the sessions they protect, so treat them as key material and rotate
them.

## Class: `DTLSSecureContext`

<!-- YAML
added: REPLACEME
-->

An opaque, reusable bundle of credentials and TLS settings, created by
[`dtls.createSecureContext()`][]. It cannot be constructed directly.

### `secureContext.isServer`

* Returns: {boolean} `true` if the context was created for a server.

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

### `endpointStats.serverRejectedCount`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The number of datagrams discarded before a handshake was
  attempted because they could not be a ClientHello. Read only.

Datagrams arriving at a listening endpoint that do not match an existing
session are screened for the shape of a DTLS ClientHello record before any
state is allocated for them. A steadily rising value indicates junk or scan
traffic rather than failing clients, which are counted as sessions that never
complete.

### `endpointStats.serverRefusedCount`

<!-- YAML
added: REPLACEME
-->

* Type: {bigint} The number of otherwise valid handshake attempts refused
  because the endpoint was at `maxSessions` or the peer was at
  `maxSessionsPerHost`. Read only.

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

* `data` {string|Buffer} The data to send. At most 16384 bytes.
* Returns: {number} The number of bytes written to the DTLS layer.

Send application data to the peer. The data is encrypted by DTLS before
being sent over UDP. Can only be called after the handshake completes
(`session.opened` has resolved).

DTLS carries application data in a single record per datagram and does not
fragment it, so `data` must fit in one record. Sending more throws
`ERR_OUT_OF_RANGE`. This limit is independent of the `mtu` option: a record
larger than the path MTU is still sent, and is fragmented by IP.

Throws `ERR_INVALID_STATE` if the handshake has not completed, or if the
session is closed or destroyed.

A successful return means the data was handed to the DTLS layer and written
to the socket, not that the peer received it. DTLS runs over UDP, so
application data may still be lost in transit.

### `session.close()`

* Returns: {Promise} Resolves when the session is closed.

Initiates a graceful DTLS shutdown by sending a `close_notify` alert.

### `session.destroy([error])`

Immediately destroys the session without sending `close_notify`.

### `session.opened`

* {Promise} Resolves with `{ protocol }` when the DTLS handshake completes.

Rejects if the handshake fails, and also if the session is closed or
destroyed before the handshake completes -- in that case with
`ERR_INVALID_STATE`, or with the error passed to
[`session.destroy()`][] if one was given. The promise always settles, so
awaiting it cannot hang.

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

* Returns: {string|undefined} The peer's certificate in PEM format, or
  `undefined` if the peer sent none.

This is the leaf certificate as PEM text and nothing else. For the issuer chain
and the parsed fields, use [`session.peerX509Certificate`][], whose `toString()`
returns this same PEM. Use [`session.authorized`][] and
\[`session.authorizationError`]\[] for the verification result rather than
parsing either.

### `session.peerX509Certificate`

<!-- YAML
added: REPLACEME
-->

* Returns: {X509Certificate|undefined} The peer's certificate, or `undefined`
  if the peer sent none.

An [`X509Certificate`][] for the peer's leaf certificate. The issuer chain is
reachable through its `issuerCertificate` property, and the parsed fields --
`subject`, `issuer`, `validFrom`, `validTo`, `fingerprint256`, `serialNumber`
and the rest -- are properties of that object.

Where [`tls.TLSSocket.getPeerCertificate()`][] returns a plain dictionary with
`valid_from`, `valid_to` and a chain walked through `issuerCertificate`, this
returns the same `X509Certificate` class that
[`tls.TLSSocket.getPeerX509Certificate()`][] does. Call `toLegacyObject()` on
it to get the dictionary form.

The same object is returned on every access once the peer's certificate is
available.

### `session.session`

<!-- YAML
added: REPLACEME
-->

* Returns: {Buffer|undefined} An opaque session for resuming this connection
  later, or `undefined` on a server session or before the handshake completes.

Pass it as the `session` option to a later [`dtls.connect()`][]. It is bound to
the host this connection authenticated against and is refused elsewhere; see
[Session resumption][].

Server sessions return `undefined`: a server has no identity to bind the value
to, and it is the client that carries a session between connections.

### `session.reused`

<!-- YAML
added: REPLACEME
-->

* Returns: {boolean} `true` if this connection resumed an earlier session
  rather than performing a full handshake.

Like [`session.authorized`][], this reads `false` once the session is closed.

### `session.authorized`

<!-- YAML
added: REPLACEME
-->

* Returns: {boolean} `true` if the peer presented a certificate chain that
  verified against the configured certificate authorities, and, for a client,
  matched the requested identity. `false` before the handshake completes.

### `session.authorizationError`

<!-- YAML
added: REPLACEME
-->

* Returns: {string|undefined} The short X509 verification error code, for
  example `'CERT_HAS_EXPIRED'` or `'HOSTNAME_MISMATCH'`, or `undefined` if the
  peer's chain verified.

A peer that presented no certificate at all reports
`'UNABLE_TO_GET_ISSUER_CERT'`, so this can be used to distinguish "no
certificate" from "a certificate that failed to verify".

The chain is verified even when `rejectUnauthorized` is `false`; the result is
simply not enforced. That makes these two properties the way to apply a custom
authorization policy:

```mjs
import { connect } from 'node:dtls';

const session = connect('192.0.2.1', 4433, {
  ca: [caCert],
  servername: 'example.com',
  rejectUnauthorized: false,
});

await session.opened;

if (!session.authorized && session.authorizationError !== 'CERT_HAS_EXPIRED') {
  await session.close();
}
```

### `session.alpnProtocol`

* Returns: {string|undefined} The negotiated ALPN protocol, or `undefined` if
  ALPN was not used.

If a server has `alpn` configured and a client offers only protocols the
server does not support, the server sends a fatal `no_application_protocol`
alert and the handshake fails, as required by [RFC 7301][] section 3.2. A
server with no `alpn` configured declines the extension instead, and the
handshake completes with no protocol negotiated.

### `session.srtpProfile`

* Returns: {string|undefined} The negotiated SRTP protection profile name.

### Internal properties

`session.state` and `endpoint.sessions` exist on these objects but are not
public API, and may change or disappear without notice.

`session.state` is a shared-memory view of flags used to coordinate with the
C++ layer (`handshaking`, `open`, and whether a message or keylog listener is
attached). Use [`session.opened`][] and [`session.closed`][] instead.

`endpoint.sessions` is the live `Set` the endpoint tracks its sessions in,
not a copy, so mutating it desynchronises the JavaScript and C++ views of
which sessions exist. Use [`endpoint.state`][] to count sessions.

Note [`endpoint.state`][] and [`endpoint.stats`][] _are_ public and documented
above.

### `session.stats`

<!-- YAML
added: REPLACEME
-->

* Type: {DTLSSession.Stats}

The statistics collected for this session. Read only. The stats object is
live and updated as data flows through the session.

### `session.exportKeyingMaterial(length, label[, context])`

* `length` {number} Number of bytes to export. Must be an integer between
  `1` and `65536`.
* `label` {string} The label for the exported keying material.
* `context` {Buffer} Optional context value.
* Returns: {Buffer}

Exports keying material from the DTLS session, as defined in
[RFC 5705][]. This is commonly used with DTLS-SRTP to derive
encryption keys for media streams.

Throws `ERR_OUT_OF_RANGE` if `length` is outside the accepted range. The upper
bound is not imposed by [RFC 5705][]; it exists so that a caller cannot request
an arbitrarily large allocation, and is far above what any defined exporter
needs (DTLS-SRTP uses 60 bytes).

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

This bounds the UDP payload, not the application payload: a record carries
somewhat less once its header and MAC are accounted for. It is fixed when the
endpoint is created and cannot be changed afterwards. It does not bound
[`session.send()`][], which is limited by the DTLS record size instead.

The MTU can be configured via the `mtu` option:

```mjs
// For a local network where you know the path MTU
const endpoint = listen(callback, {
  // ...
  mtu: 1400,
});
```

The minimum allowed MTU is 256 bytes. The maximum is 65535.

[Denial of service]: #denial-of-service
[Handshake timeout]: #handshake-timeout
[Permission Model]: permissions.md#permission-model
[Pre-shared keys]: #pre-shared-keys
[RFC 5705]: https://www.rfc-editor.org/rfc/rfc5705
[RFC 7301]: https://www.rfc-editor.org/rfc/rfc7301
[Server Name Indication]: #server-name-indication
[Session resumption]: #session-resumption
[`DTLSEndpoint`]: #class-dtlsendpoint
[`DTLSSecureContext`]: #class-dtlssecurecontext
[`X509Certificate`]: crypto.md#class-x509certificate
[`dtls.connect()`]: #dtlsconnecthost-port-options
[`dtls.createSecureContext()`]: #dtlscreatesecurecontextoptions
[`dtls.listen()`]: #dtlslistencallback-options
[`endpoint.state`]: #endpointstate
[`endpoint.stats`]: #endpointstats
[`endpointStats.serverRefusedCount`]: #endpointstatsserverrefusedcount
[`handshakeTimeout`]: #handshake-timeout
[`session.authorized`]: #sessionauthorized
[`session.closed`]: #sessionclosed
[`session.destroy()`]: #sessiondestroyerror
[`session.opened`]: #sessionopened
[`session.peerX509Certificate`]: #sessionpeerx509certificate
[`session.reused`]: #sessionreused
[`session.send()`]: #sessionsenddata
[`session.session`]: #sessionsession
[`tls.TLSSocket.getPeerCertificate()`]: tls.md#tlssocketgetpeercertificatedetailed
[`tls.TLSSocket.getPeerX509Certificate()`]: tls.md#tlssocketgetpeerx509certificate
[`tls.createSecureContext()`]: tls.md#tlscreatesecurecontextoptions
[`tls.createServer()`]: tls.md#tlscreateserveroptions-secureconnectionlistener
