# Connector

<!--introduced_in=v4.3.0-->
<!--type=module-->
<!-- source_link=lib/core/connect.js -->

> Stability: 2 - Stable

undici opens the underlying socket for every request through a *connector*. By
default this is handled internally, so most applications never interact with the
connector directly. When a request needs additional control over the socket —
for example to inspect a TLS certificate, pin a CA fingerprint, or tunnel the
connection — a custom connector can be supplied through the `connect` option of
a [`Dispatcher`][].

Use the `buildConnector` helper to construct a connector with sensible defaults
and then wrap it:

```mjs
import { buildConnector } from 'undici'

const connector = buildConnector({ rejectUnauthorized: false })
```

`buildConnector` accepts the same TLS options as
[`tls.connect()`][tls.connect] in addition to the undici-specific options listed
below, and returns a [`connector`](#connectoroptions-callback) function that
undici invokes for each new connection.

## `buildConnector([options])`

<!-- YAML
added: v5.23.4
-->

* `options` {buildConnector.BuildOptions} (optional) Connection options. In
  addition to every [`tls.connect()`][tls.connect] option, the following fields
  are supported:
  * `allowH2` {boolean} Whether to offer HTTP/2 (`h2`) during TLS ALPN
    negotiation. **Default:** `true`.
  * `preferH2` {boolean} Only effective together with `allowH2`. When `true`,
    ALPN is offered as `['h2', 'http/1.1']` (HTTP/2 first) instead of the default
    `['http/1.1', 'h2']`. Use this when the server selects the ALPN protocol by
    *client* preference (for example, some load balancers) so that HTTP/2 is
    negotiated whenever the server supports it. If the server does not support
    HTTP/2, ALPN transparently falls back to `http/1.1`. **Default:** `false`.
  * `maxCachedSessions` {number|null} Maximum number of TLS sessions to cache for
    reuse. Use `0` to disable TLS session caching. Must be a non-negative
    integer. **Default:** `100`.
  * `socketPath` {string|null} An IPC endpoint, either a Unix domain socket or a
    Windows named pipe. **Default:** `null`.
  * `timeout` {number|null} The connection timeout in milliseconds. A
    [`ConnectTimeoutError`][] is raised when the socket fails to connect within
    this period. **Default:** `10e3`.
  * `port` {number} The port to connect to when one is not provided to the
    connector. Defaults to `443` for `https:` and `80` otherwise.
  * `keepAlive` {boolean|null} Whether to enable TCP keep-alive on the socket.
    **Default:** `true`.
  * `keepAliveInitialDelay` {number|null} The delay in milliseconds before the
    first TCP keep-alive probe is sent on an idle socket. **Default:** `60e3`.
  * `typeOfService` {number|null} The IP Type of Service (ToS) value to set on
    the socket. **Default:** `null`.
* Returns: {buildConnector.connector} A
  [`connector`](#connectoroptions-callback) function bound to the supplied
  options.

Builds a connector function. `maxCachedSessions` must be a non-negative integer
or an [`InvalidArgumentError`][] is thrown.

The returned function may be passed directly as the `connect` option of a
[`Dispatcher`][], or wrapped to perform extra validation on each socket before it
is handed back to undici.

```mjs
import { Client, buildConnector } from 'undici'

const connector = buildConnector({ rejectUnauthorized: false })

const client = new Client('https://localhost:3000', {
  connect (opts, cb) {
    connector(opts, (err, socket) => {
      if (err) {
        cb(err)
      } else if (/* assertion */ false) {
        socket.destroy()
        cb(new Error('kaboom'))
      } else {
        cb(null, socket)
      }
    })
  }
})
```

## `connector(options, callback)`

<!-- YAML
added: v4.3.0
-->

* `options` {Object} Per-connection options provided by undici.
  * `hostname` {string} The host to connect to.
  * `host` {string} (optional) The value of the request `Host` header, used to
    derive the TLS `servername` when one is not supplied.
  * `protocol` {string} The request protocol, for example `'https:'` or
    `'http:'`. A value of `'https:'` establishes a TLS connection.
  * `port` {string} The port to connect to. Defaults to `443` for `https:` and
    `80` otherwise.
  * `servername` {string} (optional) The TLS Server Name Indication (SNI) value.
    Derived from `host` when omitted.
  * `localAddress` {string|null} (optional) The local address the socket should
    connect from.
  * `httpSocket` {Socket} (optional) An existing socket on which to establish the
    secure connection rather than creating a new one. May only be supplied for a
    TLS upgrade (`protocol` of `'https:'`).
* `callback` {Function} Called once the socket has connected or failed.
  * `err` {Error|null} The connection error, or `null` on success.
  * `socket` {Socket|TLSSocket|null} The connected socket, or `null` on failure.
* Returns: {Socket|TLSSocket} The socket being established.

The function returned by [`buildConnector()`](#buildconnectoroptions). undici
calls it for every new connection. It opens a [`net.Socket`][] for `http:`
targets or a [`tls.TLSSocket`][] for `https:` targets, applies TCP keep-alive and
the connect timeout, and invokes `callback` once the socket connects or errors.
The socket is also returned synchronously.

When connecting over TLS, established sessions are cached (subject to
`maxCachedSessions`) and reused for subsequent connections to the same
`servername` or `hostname`.

### Example: validate the CA fingerprint

A custom connector is a convenient place to verify the server certificate before
any request is sent over the socket.

```mjs
import { Client, buildConnector } from 'undici'

const caFingerprint = 'FO:OB:AR'
const connector = buildConnector({ rejectUnauthorized: false })

const client = new Client('https://localhost:3000', {
  connect (opts, cb) {
    connector(opts, (err, socket) => {
      if (err) {
        cb(err)
      } else if (getIssuerCertificate(socket).fingerprint256 !== caFingerprint) {
        socket.destroy()
        cb(new Error('Fingerprint does not match or malformed certificate'))
      } else {
        cb(null, socket)
      }
    })
  }
})

client.request({
  path: '/',
  method: 'GET'
}, (err, data) => {
  if (err) throw err

  const bufs = []
  data.body.on('data', (buf) => {
    bufs.push(buf)
  })
  data.body.on('end', () => {
    console.log(Buffer.concat(bufs).toString('utf8'))
    client.close()
  })
})

function getIssuerCertificate (socket) {
  let certificate = socket.getPeerCertificate(true)
  while (certificate && Object.keys(certificate).length > 0) {
    // Invalid certificate.
    if (certificate.issuerCertificate == null) {
      return null
    }

    // We have reached the root certificate. In case of self-signed
    // certificates, `issuerCertificate` may be a circular reference.
    if (certificate.fingerprint256 === certificate.issuerCertificate.fingerprint256) {
      break
    }

    certificate = certificate.issuerCertificate
  }
  return certificate
}
```

[`ConnectTimeoutError`]: Errors.md#class-connecttimeouterror
[`Dispatcher`]: Dispatcher.md#class-dispatcher
[`InvalidArgumentError`]: Errors.md#class-invalidargumenterror
[`net.Socket`]: https://nodejs.org/api/net.html#class-netsocket
[`tls.TLSSocket`]: https://nodejs.org/api/tls.html#class-tlstlssocket
[tls.connect]: https://nodejs.org/api/tls.html#tlsconnectoptions-callback
