# TLS (SSL)

> Stability: 2 - Stable

The `tls` module provides an implementation of the Transport Layer Security
(TLS) and Secure Socket Layer (SSL) protocols that is built on top of OpenSSL.
The module can be accessed using:

```js
const tls = require('tls');
```

## TLS/SSL Concepts

The TLS/SSL is a public/private key infrastructure (PKI). For most common
cases, each client and server must have a *private key*.

Private keys can be generated in multiple ways. The example below illustrates
use of the OpenSSL command-line interface to generate a 2048-bit RSA private
key:

```sh
openssl genrsa -out ryans-key.pem 2048
```

With TLS/SSL, all servers (and some clients) must have a *certificate*.
Certificates are *public keys* that correspond to a private key, and that are
digitally signed either by a Certificate Authority or by the owner of the
private key (such certificates are referred to as "self-signed"). The first
step to obtaining a certificate is to create a *Certificate Signing Request*
(CSR) file.

The OpenSSL command-line interface can be used to generate a CSR for a private
key:

```sh
openssl req -new -sha256 -key ryans-key.pem -out ryans-csr.pem
```

Once the CSR file is generated, it can either be sent to a Certificate
Authority for signing or used to generate a self-signed certificate.

Creating a self-signed certificate using the OpenSSL command-line interface
is illustrated in the example below:

```sh
openssl x509 -req -in ryans-csr.pem -signkey ryans-key.pem -out ryans-cert.pem
```

Once the certificate is generated, it can be used to generate a `.pfx` or
`.p12` file:

```sh
openssl pkcs12 -export -in ryans-cert.pem -inkey ryans-key.pem \
      -certfile ca-cert.pem -out ryans.pfx
```

Where:

* `in`: is the signed certificate
* `inkey`: is the associated private key
* `certfile`: is a concatenation of all Certificate Authority (CA) certs into
   a single file, e.g. `cat ca1-cert.pem ca2-cert.pem > ca-cert.pem`

### Perfect Forward Secrecy

<!-- type=misc -->

The term "[Forward Secrecy]" or "Perfect Forward Secrecy" describes a feature of
key-agreement (i.e., key-exchange) methods. That is, the server and client keys
are used to negotiate new temporary keys that are used specifically and only for
the current communication session. Practically, this means that even if the
server's private key is compromised, communication can only be decrypted by
eavesdroppers if the attacker manages to obtain the key-pair specifically
generated for the session.

Perfect Forward Secrecy is achieved by randomly generating a key pair for
key-agreement on every TLS/SSL handshake (in contrast to using the same key for
all sessions). Methods implementing this technique are called "ephemeral".

Currently two methods are commonly used to achieve Perfect Forward Secrecy (note
the character "E" appended to the traditional abbreviations):

* [DHE] - An ephemeral version of the Diffie Hellman key-agreement protocol.
* [ECDHE] - An ephemeral version of the Elliptic Curve Diffie Hellman
  key-agreement protocol.

Ephemeral methods may have some performance drawbacks, because key generation
is expensive.

To use Perfect Forward Secrecy using `DHE` with the `tls` module, it is required
to generate Diffie-Hellman parameters. The following illustrates the use of the
OpenSSL command-line interface to generate such parameters:

```sh
openssl dhparam -outform PEM -out dhparam.pem 2048
```

If using Perfect Foward Secrecy using `ECDHE`, Diffie-Hellman parameters are
not required and a default ECDHE curve will be used. The `ecdheCurve` property
can be used when creating a TLS Server to specify the name of an
alternative curve to use.

### ALPN, NPN and SNI

<!-- type=misc -->

ALPN (Application-Layer Protocol Negotiation Extension), NPN (Next
Protocol Negotiation) and, SNI (Server Name Indication) are TLS
handshake extensions:

* ALPN/NPN - Allows the use of one TLS server for multiple protocols (HTTP,
  SPDY, HTTP/2)
* SNI - Allows the use of one TLS server for multiple hostnames with different
  SSL certificates.

*Note*: Use of ALPN is recommended over NPN. The NPN extension has never been
formally defined or documented and generally not recommended for use.

### Client-initiated renegotiation attack mitigation

<!-- type=misc -->

The TLS protocol allows clients to renegotiate certain aspects of the TLS
session. Unfortunately, session renegotiation requires a disproportionate amount
of server-side resources, making it a potential vector for denial-of-service
attacks.

To mitigate the risk, renegotiation is limited to three times every ten minutes.
An `'error'` event is emitted on the [`tls.TLSSocket`][] instance when this
threshold is exceeded. The limits are configurable:

* `tls.CLIENT_RENEG_LIMIT` {number} Specifies the number of renegotiation
  requests. Defaults to `3`.
* `tls.CLIENT_RENEG_WINDOW` {number} Specifies the time renegotiation window
  in seconds. Defaults to `600` (10 minutes).

*Note*: The default renegotiation limits should not be modified without a full
understanding of the implications and risks.

To test the renegotiation limits on a server, connect to it using the OpenSSL
command-line client (`openssl s_client -connect address:port`) then input
`R<CR>` (i.e., the letter `R` followed by a carriage return) multiple times.

## Modifying the Default TLS Cipher suite

Node.js is built with a default suite of enabled and disabled TLS ciphers.
Currently, the default cipher suite is:

```txt
ECDHE-RSA-AES128-GCM-SHA256:
ECDHE-ECDSA-AES128-GCM-SHA256:
ECDHE-RSA-AES256-GCM-SHA384:
ECDHE-ECDSA-AES256-GCM-SHA384:
DHE-RSA-AES128-GCM-SHA256:
ECDHE-RSA-AES128-SHA256:
DHE-RSA-AES128-SHA256:
ECDHE-RSA-AES256-SHA384:
DHE-RSA-AES256-SHA384:
ECDHE-RSA-AES256-SHA256:
DHE-RSA-AES256-SHA256:
HIGH:
!aNULL:
!eNULL:
!EXPORT:
!DES:
!RC4:
!MD5:
!PSK:
!SRP:
!CAMELLIA
```

This default can be replaced entirely using the `--tls-cipher-list` command
line switch. For instance, the following makes
`ECDHE-RSA-AES128-GCM-SHA256:!RC4` the default TLS cipher suite:

```sh
node --tls-cipher-list="ECDHE-RSA-AES128-GCM-SHA256:!RC4"
```

*Note*: The default cipher suite included within Node.js has been carefully
selected to reflect current security best practices and risk mitigation.
Changing the default cipher suite can have a significant impact on the security
of an application. The `--tls-cipher-list` switch should by used only if
absolutely necessary.

## Class: tls.Server
<!-- YAML
added: v0.3.2
-->

The `tls.Server` class is a subclass of `net.Server` that accepts encrypted
connections using TLS or SSL.

### Event: 'tlsClientError'
<!-- YAML
added: v6.0.0
-->

The `'tlsClientError'` event is emitted when an error occurs before a secure
connection is established. The listener callback is passed two arguments when
called:

* `exception` {Error} The `Error` object describing the error
* `tlsSocket` {tls.TLSSocket} The `tls.TLSSocket` instance from which the
  error originated.

### Event: 'newSession'
<!-- YAML
added: v0.9.2
-->

The `'newSession'` event is emitted upon creation of a new TLS session. This may
be used to store sessions in external storage. The listener callback is passed
three arguments when called:

* `sessionId` - The TLS session identifier
* `sessionData` - The TLS session data
* `callback` {Function} A callback function taking no arguments that must be
  invoked in order for data to be sent or received over the secure connection.

*Note*: Listening for this event will have an effect only on connections
established after the addition of the event listener.

### Event: 'OCSPRequest'
<!-- YAML
added: v0.11.13
-->

The `'OCSPRequest'` event is emitted when the client sends a certificate status
request. The listener callback is passed three arguments when called:

* `certificate` {Buffer} The server certificate
* `issuer` {Buffer} The issuer's certificate
* `callback` {Function} A callback function that must be invoked to provide
  the results of the OCSP request.

The server's current certificate can be parsed to obtain the OCSP URL
and certificate ID; after obtaining an OCSP response, `callback(null, resp)` is
then invoked, where `resp` is a `Buffer` instance containing the OCSP response.
Both `certificate` and `issuer` are `Buffer` DER-representations of the
primary and issuer's certificates. These can be used to obtain the OCSP
certificate ID and OCSP endpoint URL.

Alternatively, `callback(null, null)` may be called, indicating that there was
no OCSP response.

Calling `callback(err)` will result in a `socket.destroy(err)` call.

The typical flow of an OCSP Request is as follows:

1. Client connects to the server and sends an `'OCSPRequest'` (via the status
   info extension in ClientHello).
2. Server receives the request and emits the `'OCSPRequest'` event, calling the
   listener if registered.
3. Server extracts the OCSP URL from either the `certificate` or `issuer` and
   performs an [OCSP request] to the CA.
4. Server receives `OCSPResponse` from the CA and sends it back to the client
   via the `callback` argument
5. Client validates the response and either destroys the socket or performs a
   handshake.

*Note*: The `issuer` can be `null` if the certificate is either self-signed or
the issuer is not in the root certificates list. (An issuer may be provided
via the `ca` option when establishing the TLS connection.)

*Note*: Listening for this event will have an effect only on connections
established after the addition of the event listener.

*Note*: An npm module like [asn1.js] may be used to parse the certificates.

### Event: 'resumeSession'
<!-- YAML
added: v0.9.2
-->

The `'resumeSession'` event is emitted when the client requests to resume a
previous TLS session. The listener callback is passed two arguments when
called:

* `sessionId` - The TLS/SSL session identifier
* `callback` {Function} A callback function to be called when the prior session
  has been recovered.

When called, the event listener may perform a lookup in external storage using
the given `sessionId` and invoke `callback(null, sessionData)` once finished. If
the session cannot be resumed (i.e., doesn't exist in storage) the callback may
be invoked as `callback(null, null)`. Calling `callback(err)` will terminate the
incoming connection and destroy the socket.

*Note*: Listening for this event will have an effect only on connections
established after the addition of the event listener.

The following illustrates resuming a TLS session:

```js
const tlsSessionStore = {};
server.on('newSession', (id, data, cb) => {
  tlsSessionStore[id.toString('hex')] = data;
  cb();
});
server.on('resumeSession', (id, cb) => {
  cb(null, tlsSessionStore[id.toString('hex')] || null);
});
```

### Event: 'secureConnection'
<!-- YAML
added: v0.3.2
-->

The `'secureConnection'` event is emitted after the handshaking process for a
new connection has successfully completed. The listener callback is passed a
single argument when called:

* `tlsSocket` {tls.TLSSocket} The established TLS socket.

The `tlsSocket.authorized` property is a `boolean` indicating whether the
client has been verified by one of the supplied Certificate Authorities for the
server. If `tlsSocket.authorized` is `false`, then `socket.authorizationError`
is set to describe how authorization failed. Note that depending on the settings
of the TLS server, unauthorized connections may still be accepted.

The `tlsSocket.npnProtocol` and `tlsSocket.alpnProtocol` properties are strings
that contain the selected NPN and ALPN protocols, respectively. When both NPN
and ALPN extensions are received, ALPN takes precedence over NPN and the next
protocol is selected by ALPN.

When ALPN has no selected protocol, `tlsSocket.alpnProtocol` returns `false`.

The `tlsSocket.servername` property is a string containing the server name
requested via SNI.

### server.addContext(hostname, context)
<!-- YAML
added: v0.5.3
-->

* `hostname` {string} A SNI hostname or wildcard (e.g. `'*'`)
* `context` {Object} An object containing any of the possible properties
  from the [`tls.createSecureContext()`][] `options` arguments (e.g. `key`,
  `cert`, `ca`, etc).

The `server.addContext()` method adds a secure context that will be used if
the client request's SNS hostname matches the supplied `hostname` (or wildcard).

### server.address()
<!-- YAML
added: v0.6.0
-->

Returns the bound address, the address family name, and port of the
server as reported by the operating system.  See [`net.Server.address()`][] for
more information.

### server.close([callback])
<!-- YAML
added: v0.3.2
-->

* `callback` {Function} An optional listener callback that will be registered to
  listen for the server instance's `'close'` event.

The `server.close()` method stops the server from accepting new connections.

This function operates asynchronously. The `'close'` event will be emitted
when the the server is finally closed.

### server.connections
<!-- YAML
added: v0.3.2
-->

Returns the current number of concurrent connections on the server.

### server.getTicketKeys()
<!-- YAML
added: v3.0.0
-->

Returns a `Buffer` instance holding the keys currently used for
encryption/decryption of the [TLS Session Tickets][]

### server.listen(port[, hostname][, callback])
<!-- YAML
added: v0.3.2
-->

* `port` {number} The TCP/IP port on which to begin listening for connections.
  A value of `0` (zero) will assign a random port.
* `hostname` {string} The hostname, IPv4, or IPv6 address on which to begin
  listening for connections. If `undefined`, the server will accept connections
  on any IPv6 address (`::`) when IPv6 is available, or any IPv4 address
  (`0.0.0.0`) otherwise.
* `callback` {Function} A callback function to be invoked when the server has
  begun listening the the `port` and `hostname`.

The `server.listen()` methods instructs the server to begin accepting
connections on the specified `port` and `hostname`.

This function operates asynchronously. If the `callback` is given, it will be
called when the server has started listening.

See `net.Server` for more information.

### server.setTicketKeys(keys)
<!-- YAML
added: v3.0.0
-->

* `keys` {Buffer} The keys used for encryption/decryption of the
  [TLS Session Tickets][].

Updates the keys for encryption/decryption of the [TLS Session Tickets][].

*Note*: The key's `Buffer` should be 48 bytes long. See `ticketKeys` option in
[tls.createServer](#tls_tls_createserver_options_secureconnectionlistener) for
more information on how it is used.

*Note*: Changes to the ticket keys are effective only for future server
connections. Existing or currently pending server connections will use the
previous keys.


## Class: tls.TLSSocket
<!-- YAML
added: v0.11.4
-->

The `tls.TLSSocket` is a subclass of [`net.Socket`][] that performs transparent
encryption of written data and all required TLS negotiation.

Instances of `tls.TLSSocket` implement the duplex [Stream][] interface.

*Note*: Methods that return TLS connection metadata (e.g.
[`tls.TLSSocket.getPeerCertificate()`][] will only return data while the
connection is open.

### new tls.TLSSocket(socket[, options])
<!-- YAML
added: v0.11.4
-->

* `socket` {net.Socket} An instance of [`net.Socket`][]
* `options` {Object}
  * `secureContext`: An optional TLS context object from
     [`tls.createSecureContext()`][]
  * `isServer`: If `true` the TLS socket will be instantiated in server-mode.
    Defaults to `false`.
  * `server` {net.Server} An optional [`net.Server`][] instance.
  * `requestCert`: Optional, see [`tls.createServer()`][]
  * `rejectUnauthorized`: Optional, see [`tls.createServer()`][]
  * `NPNProtocols`: Optional, see [`tls.createServer()`][]
  * `ALPNProtocols`: Optional, see [`tls.createServer()`][]
  * `SNICallback`: Optional, see [`tls.createServer()`][]
  * `session` {Buffer} An optional `Buffer` instance containing a TLS session.
  * `requestOCSP` {boolean} If `true`, specifies that the OCSP status request
    extension will be added to the client hello and an `'OCSPResponse'` event
    will be emitted on the socket before establishing a secure communication

Construct a new `tls.TLSSocket` object from an existing TCP socket.

### Event: 'OCSPResponse'
<!-- YAML
added: v0.11.13
-->

The `'OCSPResponse'` event is emitted if the `requestOCSP` option was set
when the `tls.TLSSocket` was created and an OCSP response has been received.
The listener callback is passed a single argument when called:

* `response` {Buffer} The server's OCSP response

Typically, the `response` is a digitally signed object from the server's CA that
contains information about server's certificate revocation status.

### Event: 'secureConnect'
<!-- YAML
added: v0.11.4
-->

The `'secureConnect'` event is emitted after the handshaking process for a new
connection has successfully completed. The listener callback will be called
regardless of whether or not the server's certificate has been authorized. It
is the client's responsibility to check the `tlsSocket.authorized` property to
determine if the server certificate was signed by one of the specified CAs. If
`tlsSocket.authorized === false`, then the error can be found by examining the
`tlsSocket.authorizationError` property. If either ALPN or NPN was used,
the `tlsSocket.alpnProtocol` or `tlsSocket.npnProtocol` properties can be
checked to determine the negotiated protocol.

### tlsSocket.address()
<!-- YAML
added: v0.11.4
-->

Returns the bound address, the address family name, and port of the
underlying socket as reported by the operating system. Returns an
object with three properties, e.g.,
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

### tlsSocket.authorized
<!-- YAML
added: v0.11.4
-->

Returns `true` if the peer certificate was signed by one of the CAs specified
when creating the `tls.TLSSocket` instance, otherwise `false`.

### tlsSocket.authorizationError
<!-- YAML
added: v0.11.4
-->

Returns the reason why the peer's certificate was not been verified. This
property is set only when `tlsSocket.authorized === false`.

### tlsSocket.encrypted
<!-- YAML
added: v0.11.4
-->

Always returns `true`. This may be used to distinguish TLS sockets from regular
`net.Socket` instances.

### tlsSocket.getCipher()
<!-- YAML
added: v0.11.4
-->

Returns an object representing the cipher name and the SSL/TLS protocol version
that first defined the cipher.

For example: `{ name: 'AES256-SHA', version: 'TLSv1/SSLv3' }`

See `SSL_CIPHER_get_name()` and `SSL_CIPHER_get_version()` in
https://www.openssl.org/docs/manmaster/ssl/SSL_CIPHER_get_name.html for more
information.

### tlsSocket.getEphemeralKeyInfo()
<!-- YAML
added: v5.0.0
-->

Returns an object representing the type, name, and size of parameter of
an ephemeral key exchange in [Perfect Forward Secrecy][] on a client
connection. It returns an empty object when the key exchange is not
ephemeral. As this is only supported on a client socket; `null` is returned
if called on a server socket. The supported types are `'DH'` and `'ECDH'`. The
`name` property is available only when type is 'ECDH'.

For Example: `{ type: 'ECDH', name: 'prime256v1', size: 256 }`

### tlsSocket.getPeerCertificate([ detailed ])
<!-- YAML
added: v0.11.4
-->

* `detailed` {boolean} Specify `true` to request that the full certificate
  chain with the `issuer` property be returned; `false` to return only the
  top certificate without the `issuer` property.

Returns an object representing the peer's certificate. The returned object has
some properties corresponding to the fields of the certificate.

For example:

```text
{ subject:
   { C: 'UK',
     ST: 'Acknack Ltd',
     L: 'Rhys Jones',
     O: 'node.js',
     OU: 'Test TLS Certificate',
     CN: 'localhost' },
  issuerInfo:
   { C: 'UK',
     ST: 'Acknack Ltd',
     L: 'Rhys Jones',
     O: 'node.js',
     OU: 'Test TLS Certificate',
     CN: 'localhost' },
  issuer:
   { ... another certificate ... },
  raw: < RAW DER buffer >,
  valid_from: 'Nov 11 09:52:22 2009 GMT',
  valid_to: 'Nov  6 09:52:22 2029 GMT',
  fingerprint: '2A:7A:C2:DD:E5:F9:CC:53:72:35:99:7A:02:5A:71:38:52:EC:8A:DF',
  serialNumber: 'B9B0D332A1AA5635' }
```

If the peer does not provide a certificate, `null` or an empty object will be
returned.

### tlsSocket.getProtocol()
<!-- YAML
added: v5.7.0
-->

Returns a string containing the negotiated SSL/TLS protocol version of the
current connection. The value `'unknown'` will be returned for connected
sockets that have not completed the handshaking process. The value `null` will
be returned for server sockets or disconnected client sockets.

Example responses include:

* `SSLv3`
* `TLSv1`
* `TLSv1.1`
* `TLSv1.2`
* `unknown`

See https://www.openssl.org/docs/manmaster/ssl/SSL_get_version.html for more
information.

### tlsSocket.getSession()
<!-- YAML
added: v0.11.4
-->

Returns the ASN.1 encoded TLS session or `undefined` if no session was
negotiated. Can be used to speed up handshake establishment when reconnecting
to the server.

### tlsSocket.getTLSTicket()
<!-- YAML
added: v0.11.4
-->

Returns the TLS session ticket or `undefined` if no session was negotiated.

*Note*: This only works with client TLS sockets. Useful only for debugging, for
session reuse provide `session` option to [`tls.connect()`][].

### tlsSocket.localAddress
<!-- YAML
added: v0.11.4
-->

Returns the string representation of the local IP address.

### tlsSocket.localPort
<!-- YAML
added: v0.11.4
-->

Returns the numeric representation of the local port.

### tlsSocket.remoteAddress
<!-- YAML
added: v0.11.4
-->

Returns the string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

### tlsSocket.remoteFamily
<!-- YAML
added: v0.11.4
-->

Returns the string representation of the remote IP family. `'IPv4'` or `'IPv6'`.

### tlsSocket.remotePort
<!-- YAML
added: v0.11.4
-->

Returns the numeric representation of the remote port. For example, `443`.

### tlsSocket.renegotiate(options, callback)
<!-- YAML
added: v0.11.8
-->

* `options` {Object}
  * `rejectUnauthorized` {boolean}
  * `requestCert`
* `callback` {Function} A function that will be called when the renegotiation
  request has been completed.

The `tlsSocket.renegotiate()` method initiates a TLS renegotiation process.
Upon completion, the `callback` function will be passed a single argument
that is either an `Error` (if the request failed) or `null`.

*Note*: This method can be used to request a peer's certificate after the
secure connection has been established.

*Note*: When running as the server, the socket will be destroyed with an error
after `handshakeTimeout` timeout.

### tlsSocket.setMaxSendFragment(size)
<!-- YAML
added: v0.11.11
-->

* `size` {number} The maximum TLS fragment size. Defaults to `16384`. The
  maximum value is `16384`.

The `tlsSocket.setMaxSendFragment()` method sets the maximum TLS fragment size.
Returns `true` if setting the limit succeeded; `false` otherwise.

Smaller fragment sizes decrease the buffering latency on the client: larger
fragments are buffered by the TLS layer until the entire fragment is received
and its integrity is verified; large fragments can span multiple roundtrips
and their processing can be delayed due to packet loss or reordering. However,
smaller fragments add extra TLS framing bytes and CPU overhead, which may
decrease overall server throughput.


## tls.connect(options[, callback])
<!-- YAML
added: v0.11.3
-->

* `options` {Object}
  * `host` {string} Host the client should connect to.
  * `port` {number} Port the client should connect to.
  * `socket` {net.Socket} Establish secure connection on a given socket rather
    than creating a new socket. If this option is specified, `host` and `port`
    are ignored.
  * `path` {string} Creates unix socket connection to path. If this option is
    specified, `host` and `port` are ignored.
  * `pfx` {string|Buffer} A string or `Buffer` containing the private key,
    certificate, and CA certs of the client in PFX or PKCS12 format.
  * `key` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of
    strings, or array of `Buffer`s containing the private key of the client in
    PEM format.
  * `passphrase` {string} A string containing the passphrase for the private key
    or pfx.
  * `cert` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of
    strings, or array of `Buffer`s containing the certificate key of the client
    in PEM format.
  * `ca` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of strings,
    or array of `Buffer`s of trusted certificates in PEM format. If this is
    omitted several well known "root" CAs (like VeriSign) will be used. These
    are used to authorize connections.
  * `ciphers` {string} A string describing the ciphers to use or exclude,
    separated by `:`. Uses the same default cipher suite as
    [`tls.createServer()`][].
  * `rejectUnauthorized` {boolean} If `true`, the server certificate is verified
    against the list of supplied CAs. An `'error'` event is emitted if
    verification fails; `err.code` contains the OpenSSL error code. Defaults to
    `true`.
  * `NPNProtocols` {string[]|Buffer[]} An array of strings or `Buffer`s
    containing supported NPN protocols. `Buffer`s should have the format
    `[len][name][len][name]...` e.g. `0x05hello0x05world`, where the first
    byte is the length of the next protocol name. Passing an array is usually
    much simpler, e.g. `['hello', 'world']`.
  * `ALPNProtocols`: {string[]|Buffer[]} An array of strings or `Buffer`s
    containing the supported ALPN protocols. `Buffer`s should have the format
    `[len][name][len][name]...` e.g. `0x05hello0x05world`, where the first byte
    is the length of the next protocol name. Passing an array is usually much
    simpler: `['hello', 'world']`.)
  * `servername`: {string} Server name for the SNI (Server Name Indication) TLS
    extension.
  * `checkServerIdentity(servername, cert)` {Function} A callback function
    to be used when checking the server's hostname against the certificate.
    This should throw an error if verification fails. The method should return
    `undefined` if the `servername` and `cert` are verified.
  * `secureProtocol` {string} The SSL method to use, e.g., `SSLv3_method` to
    force SSL version 3. The possible values depend on the version of OpenSSL
    installed in the environment and are defined in the constant
    [SSL_METHODS][].
  * `secureContext` {object} An optional TLS context object as returned by from
    `tls.createSecureContext( ... )`. It can be used for caching client
    certificates, keys, and CA certificates.
  * `session` {Buffer} A `Buffer` instance, containing TLS session.
  * `minDHSize` {number} Minimum size of the DH parameter in bits to accept a
    TLS connection. When a server offers a DH parameter with a size less
    than `minDHSize`, the TLS connection is destroyed and an error is thrown.
    Defaults to `1024`.
* `callback` {Function}

Creates a new client connection to the given `options.port` and `options.host`
If `options.host` is omitted, it defaults to `localhost`.

The `callback` function, if specified, will be added as a listener for the
[`'secureConnect'`][] event.

`tls.connect()` returns a [`tls.TLSSocket`][] object.

## tls.connect(port[, host][, options][, callback])
<!-- YAML
added: v0.11.3
-->

* `port` {number}
* `host` {string}
* `options` {Object}
  * `host` {string} Host the client should connect to.
  * `port` {number} Port the client should connect to.
  * `socket` {net.Socket} Establish secure connection on a given socket rather
    than creating a new socket. If this option is specified, `host` and `port`
    are ignored.
  * `path` {string} Creates unix socket connection to path. If this option is
    specified, `host` and `port` are ignored.
  * `pfx` {string|Buffer} A string or `Buffer` containing the private key,
    certificate, and CA certs of the client in PFX or PKCS12 format.
  * `key` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of
    strings, or array of `Buffer`s containing the private key of the client in
    PEM format.
  * `passphrase` {string} A string containing the passphrase for the private key
    or pfx.
  * `cert` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of
    strings, or array of `Buffer`s containing the certificate key of the client
    in PEM format.
  * `ca` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of strings,
    or array of `Buffer`s of trusted certificates in PEM format. If this is
    omitted several well known "root" CAs (like VeriSign) will be used. These
    are used to authorize connections.
  * `ciphers` {string} A string describing the ciphers to use or exclude,
    separated by `:`. Uses the same default cipher suite as
    [`tls.createServer()`][].
  * `rejectUnauthorized` {boolean} If `true`, the server certificate is verified
    against the list of supplied CAs. An `'error'` event is emitted if
    verification fails; `err.code` contains the OpenSSL error code. Defaults to
    `true`.
  * `NPNProtocols` {string[]|Buffer[]} An array of strings or `Buffer`s
    containing supported NPN protocols. `Buffer`s should have the format
    `[len][name][len][name]...` e.g. `0x05hello0x05world`, where the first
    byte is the length of the next protocol name. Passing an array is usually
    much simpler, e.g. `['hello', 'world']`.
  * `ALPNProtocols`: {string[]|Buffer[]} An array of strings or `Buffer`s
    containing the supported ALPN protocols. `Buffer`s should have the format
    `[len][name][len][name]...` e.g. `0x05hello0x05world`, where the first byte
    is the length of the next protocol name. Passing an array is usually much
    simpler: `['hello', 'world']`.)
  * `servername`: {string} Server name for the SNI (Server Name Indication) TLS
    extension.
  * `checkServerIdentity(servername, cert)` {Function} A callback function
    to be used when checking the server's hostname against the certificate.
    This should throw an error if verification fails. The method should return
    `undefined` if the `servername` and `cert` are verified.
  * `secureProtocol` {string} The SSL method to use, e.g., `SSLv3_method` to
    force SSL version 3. The possible values depend on the version of OpenSSL
    installed in the environment and are defined in the constant
    [SSL_METHODS][].
  * `secureContext` {object} An optional TLS context object as returned by from
    `tls.createSecureContext( ... )`. It can be used for caching client
    certificates, keys, and CA certificates.
  * `session` {Buffer} A `Buffer` instance, containing TLS session.
  * `minDHSize` {number} Minimum size of the DH parameter in bits to accept a
    TLS connection. When a server offers a DH parameter with a size less
    than `minDHSize`, the TLS connection is destroyed and an error is thrown.
    Defaults to `1024`.
* `callback` {Function}

Creates a new client connection to the given `port` and `host` or
`options.port` and `options.host`. (If `host` is omitted, it defaults to
`localhost`.)

The `callback` function, if specified, will be added as a listener for the
[`'secureConnect'`][] event.

`tls.connect()` returns a [`tls.TLSSocket`][] object.

The following implements a simple "echo server" example:

```js
const tls = require('tls');
const fs = require('fs');

const options = {
  // Necessary only if using the client certificate authentication
  key: fs.readFileSync('client-key.pem'),
  cert: fs.readFileSync('client-cert.pem'),

  // Necessary only if the server uses the self-signed certificate
  ca: [ fs.readFileSync('server-cert.pem') ]
};

const socket = tls.connect(8000, options, () => {
  console.log('client connected',
              socket.authorized ? 'authorized' : 'unauthorized');
  process.stdin.pipe(socket);
  process.stdin.resume();
});
socket.setEncoding('utf8');
socket.on('data', (data) => {
  console.log(data);
});
socket.on('end', () => {
  server.close();
});
```

Or

```js
const tls = require('tls');
const fs = require('fs');

const options = {
  pfx: fs.readFileSync('client.pfx')
};

const socket = tls.connect(8000, options, () => {
  console.log('client connected',
              socket.authorized ? 'authorized' : 'unauthorized');
  process.stdin.pipe(socket);
  process.stdin.resume();
});
socket.setEncoding('utf8');
socket.on('data', (data) => {
  console.log(data);
});
socket.on('end', () => {
  server.close();
});
```


## tls.createSecureContext(options)
<!-- YAML
added: v0.11.13
-->

* `options` {Object}
  * `pfx` {string|Buffer} A string or `Buffer` holding the PFX or PKCS12 encoded
    private key, certificate, and CA certificates.
  * `key` {string|string[]|Buffer|Object[]} The private key of the server in
    PEM format. To support multiple keys using different algorithms, an array
    can be provided either as an array of key strings or as an array of objects
    in the format `{pem: key, passphrase: passphrase}`. This option is
    *required* for ciphers that make use of private keys.
  * `passphrase` {string} A string containing the passphrase for the private key
    or pfx.
  * `cert` {string} A string containing the PEM encoded certificate
  * `ca`{string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of strings,
    or array of `Buffer`s of trusted certificates in PEM format. If omitted,
    several well known "root" CAs (like VeriSign) will be used. These are used
    to authorize connections.
  * `crl` {string|string[]} Either a string or array of strings of PEM encoded
    CRLs (Certificate Revocation List).
  * `ciphers` {string} A string describing the ciphers to use or exclude.
    Consult
    <https://www.openssl.org/docs/apps/ciphers.html#CIPHER-LIST-FORMAT>
    for details on the format.
  * `honorCipherOrder` {boolean} If `true`, when a cipher is being selected,
    the server's preferences will be used instead of the client preferences.

The `tls.createSecureContext()` method creates a credentials object.

If the 'ca' option is not given, then Node.js will use the default
publicly trusted list of CAs as given in
<http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt>.


## tls.createServer(options[, secureConnectionListener])
<!-- YAML
added: v0.3.2
-->

* `options` {Object}
  * `pfx` {string|Buffer} A string or `Buffer` containing the private key,
    certificate and CA certs of the server in PFX or PKCS12 format. (Mutually
    exclusive with the `key`, `cert`, and `ca` options.)
  * `key` {string|string[]|Buffer|Object[]} The private key of the server in
    PEM format. To support multiple keys using different algorithms an array can
    be provided either as a plain array of key strings or an array of objects
    in the format `{pem: key, passphrase: passphrase}`. This option is
    *required* for ciphers that make use of private keys.
  * `passphrase` {string} A string containing the passphrase for the private
    key or pfx.
  * `cert` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of
    strings, or array of `Buffer`s containing the certificate key of the server
    in PEM format. (Required)
  * `ca` {string|string[]|Buffer|Buffer[]} A string, `Buffer`, array of strings,
    or array of `Buffer`s of trusted certificates in PEM format. If this is
    omitted several well known "root" CAs (like VeriSign) will be used. These
    are used to authorize connections.
  * `crl` {string|string[]} Either a string or array of strings of PEM encoded
    CRLs (Certificate Revocation List).
  * `ciphers` {string} A string describing the ciphers to use or exclude,
    separated by `:`.
  * `ecdhCurve` {string} A string describing a named curve to use for ECDH key
    agreement or `false` to disable ECDH. Defaults to `prime256v1` (NIST P-256).
    Use [`crypto.getCurves()`][] to obtain a list of available curve names. On
    recent releases, `openssl ecparam -list_curves` will also display the name
    and description of each available elliptic curve.
  * `dhparam` {string|Buffer} A string or `Buffer` containing Diffie Hellman
    parameters, required for [Perfect Forward Secrecy][]. Use
    `openssl dhparam` to create the parameters. The key length must be greater
    than or equal to 1024 bits, otherwise an error will be thrown. It is
    strongly recommended to use 2048 bits or larger for stronger security. If
    omitted or invalid, the parameters are silently discarded and DHE ciphers
    will not be available.
  * `handshakeTimeout` {number} Abort the connection if the SSL/TLS handshake
    does not finish in the specified number of milliseconds. Defaults to `120`
    seconds. A `'clientError'` is emitted on the `tls.Server` object whenever a
    handshake times out.
  * `honorCipherOrder` {boolean} When choosing a cipher, use the server's
    preferences instead of the client preferences. Defaults to `true`.
  * `requestCert` {boolean} If `true` the server will request a certificate from
    clients that connect and attempt to verify that certificate. Defaults to
    `false`.
  * `rejectUnauthorized` {boolean} If `true` the server will reject any
    connection which is not authorized with the list of supplied CAs. This
    option only has an effect if `requestCert` is `true`. Defaults to `false`.
  * `NPNProtocols` {string[]|Buffer} An array of strings or a `Buffer` naming
    possible NPN protocols. (Protocols should be ordered by their priority.)
  * `ALPNProtocols` {string[]|Buffer} An array of strings or a `Buffer` naming
    possible ALPN protocols. (Protocols should be ordered by their priority.)
    When the server receives both NPN and ALPN extensions from the client,
    ALPN takes precedence over NPN and the server does not send an NPN
    extension to the client.
  * `SNICallback(servername, cb)` {Function} A function that will be called if
    the client supports SNI TLS extension. Two arguments will be passed when
    called: `servername` and `cb`. `SNICallback` should invoke `cb(null, ctx)`,
    where `ctx` is a SecureContext instance. (`tls.createSecureContext(...)` can
    be used to get a proper SecureContext.) If `SNICallback` wasn't provided the
    default callback with high-level API will be used (see below).
  * `sessionTimeout` {number} An integer specifying the number of seconds after
    which the TLS session identifiers and TLS session tickets created by the
    server will time out. See [SSL_CTX_set_timeout] for more details.
  * `ticketKeys`: A 48-byte `Buffer` instance consisting of a 16-byte prefix,
    a 16-byte HMAC key, and a 16-byte AES key. This can be used to accept TLS
    session tickets on multiple instances of the TLS server. *Note* that this is
    automatically shared between `cluster` module workers.
  * `sessionIdContext` {string} A string containing an opaque identifier for
    session resumption. If `requestCert` is `true`, the default is a 128 bit
    truncated SHA1 hash value generated from the command-line. Otherwise, a
    default is not provided.
  * `secureProtocol` {string} The SSL method to use, e.g., `SSLv3_method` to
    force SSL version 3. The possible values depend on the version of OpenSSL
    installed in the environment and are defined in the constant
    [SSL_METHODS][].
* `secureConnectionListener` {Function}

Creates a new [tls.Server][].  The `secureConnectionListener`, if provided, is
automatically set as a listener for the [`'secureConnection'`][] event.

For the `ciphers` option, the default cipher suite is:

```text
ECDHE-RSA-AES128-GCM-SHA256:
ECDHE-ECDSA-AES128-GCM-SHA256:
ECDHE-RSA-AES256-GCM-SHA384:
ECDHE-ECDSA-AES256-GCM-SHA384:
DHE-RSA-AES128-GCM-SHA256:
ECDHE-RSA-AES128-SHA256:
DHE-RSA-AES128-SHA256:
ECDHE-RSA-AES256-SHA384:
DHE-RSA-AES256-SHA384:
ECDHE-RSA-AES256-SHA256:
DHE-RSA-AES256-SHA256:
HIGH:
!aNULL:
!eNULL:
!EXPORT:
!DES:
!RC4:
!MD5:
!PSK:
!SRP:
!CAMELLIA
```

The default cipher suite prefers GCM ciphers for [Chrome's 'modern
cryptography' setting] and also prefers ECDHE and DHE ciphers for Perfect
Forward Secrecy, while offering *some* backward compatibility.

128 bit AES is preferred over 192 and 256 bit AES in light of [specific
attacks affecting larger AES key sizes].

Old clients that rely on insecure and deprecated RC4 or DES-based ciphers
(like Internet Explorer 6) cannot complete the handshaking process with
the default configuration. If these clients _must_ be supported, the
[TLS recommendations] may offer a compatible cipher suite. For more details
on the format, see the [OpenSSL cipher list format documentation].

The following illustrates a simple echo server:

```js
const tls = require('tls');
const fs = require('fs');

const options = {
  key: fs.readFileSync('server-key.pem'),
  cert: fs.readFileSync('server-cert.pem'),

  // This is necessary only if using the client certificate authentication.
  requestCert: true,

  // This is necessary only if the client uses the self-signed certificate.
  ca: [ fs.readFileSync('client-cert.pem') ]
};

const server = tls.createServer(options, (socket) => {
  console.log('server connected',
              socket.authorized ? 'authorized' : 'unauthorized');
  socket.write('welcome!\n');
  socket.setEncoding('utf8');
  socket.pipe(socket);
});
server.listen(8000, () => {
  console.log('server bound');
});
```

Or

```js
const tls = require('tls');
const fs = require('fs');

const options = {
  pfx: fs.readFileSync('server.pfx'),

  // This is necessary only if using the client certificate authentication.
  requestCert: true,

};

const server = tls.createServer(options, (socket) => {
  console.log('server connected',
              socket.authorized ? 'authorized' : 'unauthorized');
  socket.write('welcome!\n');
  socket.setEncoding('utf8');
  socket.pipe(socket);
});
server.listen(8000, () => {
  console.log('server bound');
});
```

This server can be tested by connecting to it using `openssl s_client`:

```sh
openssl s_client -connect 127.0.0.1:8000
```

## tls.getCiphers()
<!-- YAML
added: v0.10.2
-->

Returns an array with the names of the supported SSL ciphers.

For example:

```js
console.log(tls.getCiphers()); // ['AES128-SHA', 'AES256-SHA', ...]
```

## Deprecated APIs

### Class: CryptoStream
<!-- YAML
added: v0.3.4
deprecated: v0.11.3
-->

> Stability: 0 - Deprecated: Use [`tls.TLSSocket`][] instead.

The `tls.CryptoStream` class represents a stream of encrypted data. This class
has been deprecated and should no longer be used.

#### cryptoStream.bytesWritten
<!-- YAML
added: v0.3.4
deprecated: v0.11.3
-->

The `cryptoStream.bytesWritten` property returns the total number of bytes
written to the underlying socket *including* the bytes required for the
implementation of the TLS protocol.

### Class: SecurePair
<!-- YAML
added: v0.3.2
deprecated: v0.11.3
-->

> Stability: 0 - Deprecated: Use [`tls.TLSSocket`][] instead.

Returned by [`tls.createSecurePair()`][].

#### Event: 'secure'
<!-- YAML
added: v0.3.2
deprecated: v0.11.3
-->

The `'secure'` event is emitted by the `SecurePair` object once a secure
connection has been established.

As with checking for the server [`secureConnection`](#tls_event_secureconnection)
event, `pair.cleartext.authorized` should be inspected to confirm whether the
certificate used is properly authorized.

## tls.createSecurePair([context][, isServer][, requestCert][, rejectUnauthorized][, options])
<!-- YAML
added: v0.3.2
deprecated: v0.11.3
-->

> Stability: 0 - Deprecated: Use [`tls.TLSSocket`][] instead.

* `context` {Object} A secure context object as returned by
  `tls.createSecureContext()`
* `isServer` {boolean} `true` to specify that this TLS connection should be
  opened as a server.
* `requestCert` {boolean} `true` to specify whether a server should request a
  certificate from a connecting client. Only applies when `isServer` is `true`.
* `rejectUnauthorized` {boolean} `true` to specify whether a server should
  automatically reject clients with invalid certificates. Only applies when
  `isServer` is `true`.
* `options`
  * `secureContext`: An optional TLS context object from
     [`tls.createSecureContext()`][]
  * `isServer`: If `true` the TLS socket will be instantiated in server-mode.
    Defaults to `false`.
  * `server` {net.Server} An optional [`net.Server`][] instance
  * `requestCert`: Optional, see [`tls.createServer()`][]
  * `rejectUnauthorized`: Optional, see [`tls.createServer()`][]
  * `NPNProtocols`: Optional, see [`tls.createServer()`][]
  * `ALPNProtocols`: Optional, see [`tls.createServer()`][]
  * `SNICallback`: Optional, see [`tls.createServer()`][]
  * `session` {Buffer} An optional `Buffer` instance containing a TLS session.
  * `requestOCSP` {boolean} If `true`, specifies that the OCSP status request
    extension will be added to the client hello and an `'OCSPResponse'` event
    will be emitted on the socket before establishing a secure communication

Creates a new secure pair object with two streams, one of which reads and writes
the encrypted data and the other of which reads and writes the cleartext data.
Generally, the encrypted stream is piped to/from an incoming encrypted data
stream and the cleartext one is used as a replacement for the initial encrypted
stream.

`tls.createSecurePair()` returns a `tls.SecurePair` object with `cleartext` and
`encrypted` stream properties.

*Note*: `cleartext` has the same API as [`tls.TLSSocket`][].

*Note*: The `tls.createSecurePair()` method is now deprecated in favor of
`tls.TLSSocket()`. For example, the code:

```js
pair = tls.createSecurePair( ... );
pair.encrypted.pipe(socket);
socket.pipe(pair.encrypted);
```

can be replaced by:

```js
secure_socket = tls.TLSSocket(socket, options);
```

where `secure_socket` has the same API as `pair.cleartext`.

[OpenSSL cipher list format documentation]: https://www.openssl.org/docs/apps/ciphers.html#CIPHER-LIST-FORMAT
[Chrome's 'modern cryptography' setting]: https://www.chromium.org/Home/chromium-security/education/tls#TOC-Cipher-Suites
[specific attacks affecting larger AES key sizes]: https://www.schneier.com/blog/archives/2009/07/another_new_aes.html
[`crypto.getCurves()`]: crypto.html#crypto_crypto_getcurves
[`tls.createServer()`]: #tls_tls_createserver_options_secureconnectionlistener
[`tls.createSecurePair()`]: #tls_tls_createsecurepair_context_isserver_requestcert_rejectunauthorized_options
[`tls.TLSSocket`]: #tls_class_tls_tlssocket
[`net.Server`]: net.html#net_class_net_server
[`net.Socket`]: net.html#net_class_net_socket
[`net.Server.address()`]: net.html#net_server_address
[`'secureConnect'`]: #tls_event_secureconnect
[`'secureConnection'`]: #tls_event_secureconnection
[Perfect Forward Secrecy]: #tls_perfect_forward_secrecy
[Stream]: stream.html#stream_stream
[SSL_METHODS]: https://www.openssl.org/docs/ssl/ssl.html#DEALING-WITH-PROTOCOL-METHODS
[tls.Server]: #tls_class_tls_server
[SSL_CTX_set_timeout]: https://www.openssl.org/docs/ssl/SSL_CTX_set_timeout.html
[Forward secrecy]: https://en.wikipedia.org/wiki/Perfect_forward_secrecy
[DHE]: https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange
[ECDHE]: https://en.wikipedia.org/wiki/Elliptic_curve_Diffie%E2%80%93Hellman
[asn1.js]: https://npmjs.org/package/asn1.js
[OCSP request]: https://en.wikipedia.org/wiki/OCSP_stapling
[TLS recommendations]: https://wiki.mozilla.org/Security/Server_Side_TLS
[TLS Session Tickets]: https://www.ietf.org/rfc/rfc5077.txt
[`tls.TLSSocket.getPeerCertificate()`]: #tls_tlssocket_getpeercertificate_detailed
[`tls.createSecureContext()`]: #tls_tls_createsecurecontext_options
[`tls.connect()`]: #tls_tls_connect_options_callback
