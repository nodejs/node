# TLS (SSL)

    Stability: 3 - Stable

Use `require('tls')` to access this module.

The `tls` module uses OpenSSL to provide Transport Layer Security and/or
Secure Socket Layer: encrypted stream communication.

TLS/SSL is a public/private key infrastructure. Each client and each
server must have a private key. A private key is created like this:

    openssl genrsa -out ryans-key.pem 2048

All servers and some clients need to have a certificate. Certificates are public
keys signed by a Certificate Authority or self-signed. The first step to
getting a certificate is to create a "Certificate Signing Request" (CSR)
file. This is done with:

    openssl req -new -sha256 -key ryans-key.pem -out ryans-csr.pem

To create a self-signed certificate with the CSR, do this:

    openssl x509 -req -in ryans-csr.pem -signkey ryans-key.pem -out ryans-cert.pem

Alternatively you can send the CSR to a Certificate Authority for signing.

(TODO: docs on creating a CA, for now interested users should just look at
`test/fixtures/keys/Makefile` in the Node source code)

To create .pfx or .p12, do this:

    openssl pkcs12 -export -in agent5-cert.pem -inkey agent5-key.pem \
        -certfile ca-cert.pem -out agent5.pfx

  - `in`:  certificate
  - `inkey`: private key
  - `certfile`: all CA certs concatenated in one file like
    `cat ca1-cert.pem ca2-cert.pem > ca-cert.pem`


## Client-initiated renegotiation attack mitigation

<!-- type=misc -->

The TLS protocol lets the client renegotiate certain aspects of the TLS session.
Unfortunately, session renegotiation requires a disproportional amount of
server-side resources, which makes it a potential vector for denial-of-service
attacks.

To mitigate this, renegotiations are limited to three times every 10 minutes. An
error is emitted on the [tls.TLSSocket][] instance when the threshold is
exceeded. The limits are configurable:

  - `tls.CLIENT_RENEG_LIMIT`: renegotiation limit, default is 3.

  - `tls.CLIENT_RENEG_WINDOW`: renegotiation window in seconds, default is
    10 minutes.

Don't change the defaults unless you know what you are doing.

To test your server, connect to it with `openssl s_client -connect address:port`
and tap `R<CR>` (that's the letter `R` followed by a carriage return) a few
times.


## NPN and SNI

<!-- type=misc -->

NPN (Next Protocol Negotiation) and SNI (Server Name Indication) are TLS
handshake extensions allowing you:

  * NPN - to use one TLS server for multiple protocols (HTTP, SPDY)
  * SNI - to use one TLS server for multiple hostnames with different SSL
    certificates.


## Perfect Forward Secrecy

<!-- type=misc -->

The term "[Forward Secrecy]" or "Perfect Forward Secrecy" describes a feature of
key-agreement (i.e. key-exchange) methods. Practically it means that even if the
private key of a (your) server is compromised, communication can only be
decrypted by eavesdroppers if they manage to obtain the key-pair specifically
generated for each session.

This is achieved by randomly generating a key pair for key-agreement on every
handshake (in contrary to the same key for all sessions). Methods implementing
this technique, thus offering Perfect Forward Secrecy, are called "ephemeral".

Currently two methods are commonly used to achieve Perfect Forward Secrecy (note
the character "E" appended to the traditional abbreviations):

  * [DHE] - An ephemeral version of the Diffie Hellman key-agreement protocol.
  * [ECDHE] - An ephemeral version of the Elliptic Curve Diffie Hellman
    key-agreement protocol.

Ephemeral methods may have some performance drawbacks, because key generation
is expensive.


## tls.getCiphers()

Returns an array with the names of the supported SSL ciphers.

Example:

    var ciphers = tls.getCiphers();
    console.log(ciphers); // ['AES128-SHA', 'AES256-SHA', ...]


## tls.createServer(options[, secureConnectionListener])

Creates a new [tls.Server][].  The `connectionListener` argument is
automatically set as a listener for the [secureConnection][] event.  The
`options` object has these possibilities:

  - `pfx`: A string or `Buffer` containing the private key, certificate and
    CA certs of the server in PFX or PKCS12 format. (Mutually exclusive with
    the `key`, `cert` and `ca` options.)

  - `key`: A string or `Buffer` containing the private key of the server in
    PEM format. (Could be an array of keys). (Required)

  - `passphrase`: A string of passphrase for the private key or pfx.

  - `cert`: A string or `Buffer` containing the certificate key of the server in
    PEM format. (Could be an array of certs). (Required)

  - `ca`: An array of strings or `Buffer`s of trusted certificates in PEM
    format. If this is omitted several well known "root" CAs will be used,
    like VeriSign. These are used to authorize connections.

  - `crl` : Either a string or list of strings of PEM encoded CRLs (Certificate
    Revocation List)

  - `ciphers`: A string describing the ciphers to use or exclude.

    To mitigate [BEAST attacks] it is recommended that you use this option in
    conjunction with the `honorCipherOrder` option described below to
    prioritize the non-CBC cipher.

    Defaults to
    `ECDHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL`.
    Consult the [OpenSSL cipher list format documentation] for details
    on the format.

    `ECDHE-RSA-AES128-SHA256`, `DHE-RSA-AES128-SHA256` and
    `AES128-GCM-SHA256` are TLS v1.2 ciphers and used when io.js is
    linked against OpenSSL 1.0.1 or newer, such as the bundled version
    of OpenSSL.  Note that it is still possible for a TLS v1.2 client
    to negotiate a weaker cipher unless `honorCipherOrder` is enabled.

    `RC4` is used as a fallback for clients that speak on older version of
    the TLS protocol.  `RC4` has in recent years come under suspicion and
    should be considered compromised for anything that is truly sensitive.
    It is speculated that state-level actors possess the ability to break it.

    **NOTE**: Previous revisions of this section suggested `AES256-SHA` as an
    acceptable cipher. Unfortunately, `AES256-SHA` is a CBC cipher and therefore
    susceptible to [BEAST attacks]. Do *not* use it.

  - `ecdhCurve`: A string describing a named curve to use for ECDH key agreement
    or false to disable ECDH.

    Defaults to `prime256v1`. Consult [RFC 4492] for more details.

  - `dhparam`: DH parameter file to use for DHE key agreement. Use
    `openssl dhparam` command to create it. If the file is invalid to
    load, it is silently discarded.

  - `handshakeTimeout`: Abort the connection if the SSL/TLS handshake does not
    finish in this many milliseconds. The default is 120 seconds.

    A `'clientError'` is emitted on the `tls.Server` object whenever a handshake
    times out.

  - `honorCipherOrder` : When choosing a cipher, use the server's preferences
    instead of the client preferences.

    Although, this option is disabled by default, it is *recommended* that you
    use this option in conjunction with the `ciphers` option to mitigate
    BEAST attacks.

  - `requestCert`: If `true` the server will request a certificate from
    clients that connect and attempt to verify that certificate. Default:
    `false`.

  - `rejectUnauthorized`: If `true` the server will reject any connection
    which is not authorized with the list of supplied CAs. This option only
    has an effect if `requestCert` is `true`. Default: `false`.

  - `checkServerIdentity(servername, cert)`: Provide an override for checking
    server's hostname against the certificate. Should return an error if verification
    fails. Return `undefined` if passing.

  - `NPNProtocols`: An array or `Buffer` of possible NPN protocols. (Protocols
    should be ordered by their priority).

  - `SNICallback(servername, cb)`: A function that will be called if client
    supports SNI TLS extension. Two argument will be passed to it: `servername`,
    and `cb`. `SNICallback` should invoke `cb(null, ctx)`, where `ctx` is a
    SecureContext instance.
    (You can use `tls.createSecureContext(...)` to get proper
    SecureContext). If `SNICallback` wasn't provided - default callback with
    high-level API will be used (see below).

  - `sessionTimeout`: An integer specifying the seconds after which TLS
    session identifiers and TLS session tickets created by the server are
    timed out. See [SSL_CTX_set_timeout] for more details.

  - `ticketKeys`: A 48-byte `Buffer` instance consisting of 16-byte prefix,
    16-byte hmac key, 16-byte AES key. You could use it to accept tls session
    tickets on multiple instances of tls server.

    NOTE: Automatically shared between `cluster` module workers.

  - `sessionIdContext`: A string containing an opaque identifier for session
    resumption. If `requestCert` is `true`, the default is MD5 hash value
    generated from command-line. Otherwise, the default is not provided.

  - `secureProtocol`: The SSL method to use, e.g. `SSLv3_method` to force
    SSL version 3. The possible values depend on your installation of
    OpenSSL and are defined in the constant [SSL_METHODS][].

Here is a simple example echo server:

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      key: fs.readFileSync('server-key.pem'),
      cert: fs.readFileSync('server-cert.pem'),

      // This is necessary only if using the client certificate authentication.
      requestCert: true,

      // This is necessary only if the client uses the self-signed certificate.
      ca: [ fs.readFileSync('client-cert.pem') ]
    };

    var server = tls.createServer(options, function(socket) {
      console.log('server connected',
                  socket.authorized ? 'authorized' : 'unauthorized');
      socket.write("welcome!\n");
      socket.setEncoding('utf8');
      socket.pipe(socket);
    });
    server.listen(8000, function() {
      console.log('server bound');
    });

Or

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      pfx: fs.readFileSync('server.pfx'),

      // This is necessary only if using the client certificate authentication.
      requestCert: true,

    };

    var server = tls.createServer(options, function(socket) {
      console.log('server connected',
                  socket.authorized ? 'authorized' : 'unauthorized');
      socket.write("welcome!\n");
      socket.setEncoding('utf8');
      socket.pipe(socket);
    });
    server.listen(8000, function() {
      console.log('server bound');
    });
You can test this server by connecting to it with `openssl s_client`:


    openssl s_client -connect 127.0.0.1:8000


## tls.connect(options[, callback])
## tls.connect(port[, host][, options][, callback])

Creates a new client connection to the given `port` and `host` (old API) or
`options.port` and `options.host`. (If `host` is omitted, it defaults to
`localhost`.) `options` should be an object which specifies:

  - `host`: Host the client should connect to

  - `port`: Port the client should connect to

  - `socket`: Establish secure connection on a given socket rather than
    creating a new socket. If this option is specified, `host` and `port`
    are ignored.

  - `path`: Creates unix socket connection to path. If this option is
    specified, `host` and `port` are ignored.

  - `pfx`: A string or `Buffer` containing the private key, certificate and
    CA certs of the client in PFX or PKCS12 format.

  - `key`: A string or `Buffer` containing the private key of the client in
    PEM format. (Could be an array of keys).

  - `passphrase`: A string of passphrase for the private key or pfx.

  - `cert`: A string or `Buffer` containing the certificate key of the client in
    PEM format. (Could be an array of certs).

  - `ca`: An array of strings or `Buffer`s of trusted certificates in PEM
    format. If this is omitted several well known "root" CAs will be used,
    like VeriSign. These are used to authorize connections.

  - `rejectUnauthorized`: If `true`, the server certificate is verified against
    the list of supplied CAs. An `'error'` event is emitted if verification
    fails; `err.code` contains the OpenSSL error code. Default: `true`.

  - `NPNProtocols`: An array of strings or `Buffer`s containing supported NPN
    protocols. `Buffer`s should have following format: `0x05hello0x05world`,
    where first byte is next protocol name's length. (Passing array should
    usually be much simpler: `['hello', 'world']`.)

  - `servername`: Servername for SNI (Server Name Indication) TLS extension.

  - `secureProtocol`: The SSL method to use, e.g. `SSLv3_method` to force
    SSL version 3. The possible values depend on your installation of
    OpenSSL and are defined in the constant [SSL_METHODS][].

  - `session`: A `Buffer` instance, containing TLS session.

The `callback` parameter will be added as a listener for the
['secureConnect'][] event.

`tls.connect()` returns a [tls.TLSSocket][] object.

Here is an example of a client of echo server as described previously:

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      // These are necessary only if using the client certificate authentication
      key: fs.readFileSync('client-key.pem'),
      cert: fs.readFileSync('client-cert.pem'),

      // This is necessary only if the server uses the self-signed certificate
      ca: [ fs.readFileSync('server-cert.pem') ]
    };

    var socket = tls.connect(8000, options, function() {
      console.log('client connected',
                  socket.authorized ? 'authorized' : 'unauthorized');
      process.stdin.pipe(socket);
      process.stdin.resume();
    });
    socket.setEncoding('utf8');
    socket.on('data', function(data) {
      console.log(data);
    });
    socket.on('end', function() {
      server.close();
    });

Or

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      pfx: fs.readFileSync('client.pfx')
    };

    var socket = tls.connect(8000, options, function() {
      console.log('client connected',
                  socket.authorized ? 'authorized' : 'unauthorized');
      process.stdin.pipe(socket);
      process.stdin.resume();
    });
    socket.setEncoding('utf8');
    socket.on('data', function(data) {
      console.log(data);
    });
    socket.on('end', function() {
      server.close();
    });

## Class: tls.TLSSocket

Wrapper for instance of [net.Socket][], replaces internal socket read/write
routines to perform transparent encryption/decryption of incoming/outgoing data.

## new tls.TLSSocket(socket, options)

Construct a new TLSSocket object from existing TCP socket.

`socket` is an instance of [net.Socket][]

`options` is an object that might contain following properties:

  - `secureContext`: An optional TLS context object from
     `tls.createSecureContext( ... )`

  - `isServer`: If true - TLS socket will be instantiated in server-mode

  - `server`: An optional [net.Server][] instance

  - `requestCert`: Optional, see [tls.createSecurePair][]

  - `rejectUnauthorized`: Optional, see [tls.createSecurePair][]

  - `NPNProtocols`: Optional, see [tls.createServer][]

  - `SNICallback`: Optional, see [tls.createServer][]

  - `session`: Optional, a `Buffer` instance, containing TLS session

  - `requestOCSP`: Optional, if `true` - OCSP status request extension would
    be added to client hello, and `OCSPResponse` event will be emitted on socket
    before establishing secure communication


## tls.createSecureContext(details)

Creates a credentials object, with the optional details being a
dictionary with keys:

* `pfx` : A string or buffer holding the PFX or PKCS12 encoded private
  key, certificate and CA certificates
* `key` : A string holding the PEM encoded private key
* `passphrase` : A string of passphrase for the private key or pfx
* `cert` : A string holding the PEM encoded certificate
* `ca` : Either a string or list of strings of PEM encoded CA
  certificates to trust.
* `crl` : Either a string or list of strings of PEM encoded CRLs
  (Certificate Revocation List)
* `ciphers`: A string describing the ciphers to use or exclude.
  Consult
  <http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT>
  for details on the format.
* `honorCipherOrder` : When choosing a cipher, use the server's preferences
  instead of the client preferences. For further details see `tls` module
  documentation.

If no 'ca' details are given, then io.js will use the default
publicly trusted list of CAs as given in
<http://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt>.


## tls.createSecurePair([context][, isServer][, requestCert][, rejectUnauthorized])

Creates a new secure pair object with two streams, one of which reads/writes
encrypted data, and one reads/writes cleartext data.
Generally the encrypted one is piped to/from an incoming encrypted data stream,
and the cleartext one is used as a replacement for the initial encrypted stream.

 - `credentials`: A secure context object from tls.createSecureContext( ... )

 - `isServer`: A boolean indicating whether this tls connection should be
   opened as a server or a client.

 - `requestCert`: A boolean indicating whether a server should request a
   certificate from a connecting client. Only applies to server connections.

 - `rejectUnauthorized`: A boolean indicating whether a server should
   automatically reject clients with invalid certificates. Only applies to
   servers with `requestCert` enabled.

`tls.createSecurePair()` returns a SecurePair object with `cleartext` and
`encrypted` stream properties.

NOTE: `cleartext` has the same APIs as [tls.TLSSocket][]

## Class: SecurePair

Returned by tls.createSecurePair.

### Event: 'secure'

The event is emitted from the SecurePair once the pair has successfully
established a secure connection.

Similarly to the checking for the server 'secureConnection' event,
pair.cleartext.authorized should be checked to confirm whether the certificate
used properly authorized.

## Class: tls.Server

This class is a subclass of `net.Server` and has the same methods on it.
Instead of accepting just raw TCP connections, this accepts encrypted
connections using TLS or SSL.

### Event: 'secureConnection'

`function (tlsSocket) {}`

This event is emitted after a new connection has been successfully
handshaked. The argument is an instance of [tls.TLSSocket][]. It has all the
common stream methods and events.

`socket.authorized` is a boolean value which indicates if the
client has verified by one of the supplied certificate authorities for the
server. If `socket.authorized` is false, then
`socket.authorizationError` is set to describe how authorization
failed. Implied but worth mentioning: depending on the settings of the TLS
server, you unauthorized connections may be accepted.
`socket.npnProtocol` is a string containing selected NPN protocol.
`socket.servername` is a string containing servername requested with
SNI.


### Event: 'clientError'

`function (exception, tlsSocket) { }`

When a client connection emits an 'error' event before secure connection is
established - it will be forwarded here.

`tlsSocket` is the [tls.TLSSocket][] that the error originated from.


### Event: 'newSession'

`function (sessionId, sessionData, callback) { }`

Emitted on creation of TLS session. May be used to store sessions in external
storage. `callback` must be invoked eventually, otherwise no data will be
sent or received from secure connection.

NOTE: adding this event listener will have an effect only on connections
established after addition of event listener.


### Event: 'resumeSession'

`function (sessionId, callback) { }`

Emitted when client wants to resume previous TLS session. Event listener may
perform lookup in external storage using given `sessionId`, and invoke
`callback(null, sessionData)` once finished. If session can't be resumed
(i.e. doesn't exist in storage) one may call `callback(null, null)`. Calling
`callback(err)` will terminate incoming connection and destroy socket.

NOTE: adding this event listener will have an effect only on connections
established after addition of event listener.


### Event: 'OCSPRequest'

`function (certificate, issuer, callback) { }`

Emitted when the client sends a certificate status request. You could parse
server's current certificate to obtain OCSP url and certificate id, and after
obtaining OCSP response invoke `callback(null, resp)`, where `resp` is a
`Buffer` instance. Both `certificate` and `issuer` are a `Buffer`
DER-representations of the primary and issuer's certificates. They could be used
to obtain OCSP certificate id and OCSP endpoint url.

Alternatively, `callback(null, null)` could be called, meaning that there is no
OCSP response.

Calling `callback(err)` will result in a `socket.destroy(err)` call.

Typical flow:

1. Client connects to server and sends `OCSPRequest` to it (via status info
   extension in ClientHello.)
2. Server receives request and invokes `OCSPRequest` event listener if present
3. Server grabs OCSP url from either `certificate` or `issuer` and performs an
   [OCSP request] to the CA
4. Server receives `OCSPResponse` from CA and sends it back to client via
   `callback` argument
5. Client validates the response and either destroys socket or performs a
   handshake.

NOTE: `issuer` could be null, if the certificate is self-signed or if the issuer
is not in the root certificates list. (You could provide an issuer via `ca`
option.)

NOTE: adding this event listener will have an effect only on connections
established after addition of event listener.

NOTE: you may want to use some npm module like [asn1.js] to parse the
certificates.


### server.listen(port[, hostname][, callback])

Begin accepting connections on the specified `port` and `hostname`. If the
`hostname` is omitted, the server will accept connections on any IPv6 address
(`::`) when IPv6 is available, or any IPv4 address (`0.0.0.0`) otherwise. A
port value of zero will assign a random port.

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.

See `net.Server` for more information.


### server.close([callback])

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.  Optionally, you can pass a callback to listen for the `'close'` event.

### server.address()

Returns the bound address, the address family name and port of the
server as reported by the operating system.  See [net.Server.address()][] for
more information.

### server.addContext(hostname, context)

Add secure context that will be used if client request's SNI hostname is
matching passed `hostname` (wildcards can be used). `context` can contain
`key`, `cert`, `ca` and/or any other properties from `tls.createSecureContext`
`options` argument.

### server.maxConnections

Set this property to reject connections when the server's connection count
gets high.

### server.connections

The number of concurrent connections on the server.


## Class: CryptoStream

    Stability: 0 - Deprecated. Use tls.TLSSocket instead.

This is an encrypted stream.

### cryptoStream.bytesWritten

A proxy to the underlying socket's bytesWritten accessor, this will return
the total bytes written to the socket, *including the TLS overhead*.

## Class: tls.TLSSocket

This is a wrapped version of [net.Socket][] that does transparent encryption
of written data and all required TLS negotiation.

This instance implements a duplex [Stream][] interfaces.  It has all the
common stream methods and events.

### Event: 'secureConnect'

This event is emitted after a new connection has been successfully handshaked.
The listener will be called no matter if the server's certificate was
authorized or not. It is up to the user to test `tlsSocket.authorized`
to see if the server certificate was signed by one of the specified CAs.
If `tlsSocket.authorized === false` then the error can be found in
`tlsSocket.authorizationError`. Also if NPN was used - you can check
`tlsSocket.npnProtocol` for negotiated protocol.

### Event: 'OCSPResponse'

`function (response) { }`

This event will be emitted if `requestOCSP` option was set. `response` is a
buffer object, containing server's OCSP response.

Traditionally, the `response` is a signed object from the server's CA that
contains information about server's certificate revocation status.

### tlsSocket.encrypted

Static boolean value, always `true`. May be used to distinguish TLS sockets
from regular ones.

### tlsSocket.authorized

A boolean that is `true` if the peer certificate was signed by one of the
specified CAs, otherwise `false`

### tlsSocket.authorizationError

The reason why the peer's certificate has not been verified. This property
becomes available only when `tlsSocket.authorized === false`.

### tlsSocket.getPeerCertificate([ detailed ])

Returns an object representing the peer's certificate. The returned object has
some properties corresponding to the field of the certificate. If `detailed`
argument is `true` - the full chain with `issuer` property will be returned,
if `false` - only the top certificate without `issuer` property.

Example:

    { subject:
       { C: 'UK',
         ST: 'Acknack Ltd',
         L: 'Rhys Jones',
         O: 'io.js',
         OU: 'Test TLS Certificate',
         CN: 'localhost' },
      issuerInfo:
       { C: 'UK',
         ST: 'Acknack Ltd',
         L: 'Rhys Jones',
         O: 'io.js',
         OU: 'Test TLS Certificate',
         CN: 'localhost' },
      issuer:
       { ... another certificate ... },
      raw: < RAW DER buffer >,
      valid_from: 'Nov 11 09:52:22 2009 GMT',
      valid_to: 'Nov  6 09:52:22 2029 GMT',
      fingerprint: '2A:7A:C2:DD:E5:F9:CC:53:72:35:99:7A:02:5A:71:38:52:EC:8A:DF',
      serialNumber: 'B9B0D332A1AA5635' }

If the peer does not provide a certificate, it returns `null` or an empty
object.

### tlsSocket.getCipher()
Returns an object representing the cipher name and the SSL/TLS
protocol version of the current connection.

Example:
{ name: 'AES256-SHA', version: 'TLSv1/SSLv3' }

See SSL_CIPHER_get_name() and SSL_CIPHER_get_version() in
http://www.openssl.org/docs/ssl/ssl.html#DEALING_WITH_CIPHERS for more
information.

### tlsSocket.renegotiate(options, callback)

Initiate TLS renegotiation process. The `options` may contain the following
fields: `rejectUnauthorized`, `requestCert` (See [tls.createServer][]
for details). `callback(err)` will be executed with `null` as `err`,
once the renegotiation is successfully completed.

NOTE: Can be used to request peer's certificate after the secure connection
has been established.

ANOTHER NOTE: When running as the server, socket will be destroyed
with an error after `handshakeTimeout` timeout.

### tlsSocket.setMaxSendFragment(size)

Set maximum TLS fragment size (default and maximum value is: `16384`, minimum
is: `512`). Returns `true` on success, `false` otherwise.

Smaller fragment size decreases buffering latency on the client: large
fragments are buffered by the TLS layer until the entire fragment is received
and its integrity is verified; large fragments can span multiple roundtrips,
and their processing can be delayed due to packet loss or reordering. However,
smaller fragments add extra TLS framing bytes and CPU overhead, which may
decrease overall server throughput.

### tlsSocket.getSession()

Return ASN.1 encoded TLS session or `undefined` if none was negotiated. Could
be used to speed up handshake establishment when reconnecting to the server.

### tlsSocket.getTLSTicket()

NOTE: Works only with client TLS sockets. Useful only for debugging, for
session reuse provide `session` option to `tls.connect`.

Return TLS session ticket or `undefined` if none was negotiated.

### tlsSocket.address()

Returns the bound address, the address family name and port of the
underlying socket as reported by the operating system. Returns an
object with three properties, e.g.
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

### tlsSocket.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

### tlsSocket.remoteFamily

The string representation of the remote IP family. `'IPv4'` or `'IPv6'`.

### tlsSocket.remotePort

The numeric representation of the remote port. For example, `443`.

### tlsSocket.localAddress

The string representation of the local IP address.

### tlsSocket.localPort

The numeric representation of the local port.

[OpenSSL cipher list format documentation]: http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT
[BEAST attacks]: http://blog.ivanristic.com/2011/10/mitigating-the-beast-attack-on-tls.html
[tls.createServer]: #tls_tls_createserver_options_secureconnectionlistener
[tls.createSecurePair]: #tls_tls_createsecurepair_credentials_isserver_requestcert_rejectunauthorized
[tls.TLSSocket]: #tls_class_tls_tlssocket
[net.Server]: net.html#net_class_net_server
[net.Socket]: net.html#net_class_net_socket
[net.Server.address()]: net.html#net_server_address
['secureConnect']: #tls_event_secureconnect
[secureConnection]: #tls_event_secureconnection
[Stream]: stream.html#stream_stream
[SSL_METHODS]: http://www.openssl.org/docs/ssl/ssl.html#DEALING_WITH_PROTOCOL_METHODS
[tls.Server]: #tls_class_tls_server
[SSL_CTX_set_timeout]: http://www.openssl.org/docs/ssl/SSL_CTX_set_timeout.html
[RFC 4492]: http://www.rfc-editor.org/rfc/rfc4492.txt
[Forward secrecy]: http://en.wikipedia.org/wiki/Perfect_forward_secrecy
[DHE]: https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange
[ECDHE]: https://en.wikipedia.org/wiki/Elliptic_curve_Diffie%E2%80%93Hellman
[asn1.js]: http://npmjs.org/package/asn1.js
[OCSP request]: http://en.wikipedia.org/wiki/OCSP_stapling
