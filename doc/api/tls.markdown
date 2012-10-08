# TLS (SSL)

    Stability: 3 - Stable

Use `require('tls')` to access this module.

The `tls` module uses OpenSSL to provide Transport Layer Security and/or
Secure Socket Layer: encrypted stream communication.

TLS/SSL is a public/private key infrastructure. Each client and each
server must have a private key. A private key is created like this

    openssl genrsa -out ryans-key.pem 1024

All severs and some clients need to have a certificate. Certificates are public
keys signed by a Certificate Authority or self-signed. The first step to
getting a certificate is to create a "Certificate Signing Request" (CSR)
file. This is done with:

    openssl req -new -key ryans-key.pem -out ryans-csr.pem

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
error is emitted on the [CleartextStream][] instance when the threshold is
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


## tls.createServer(options, [secureConnectionListener])

Creates a new [tls.Server][].  The `connectionListener` argument is
automatically set as a listener for the [secureConnection][] event.  The
`options` object has these possibilities:

  - `pfx`: A string or `Buffer` containing the private key, certificate and
    CA certs of the server in PFX or PKCS12 format. (Mutually exclusive with
    the `key`, `cert` and `ca` options.)

  - `key`: A string or `Buffer` containing the private key of the server in
    PEM format. (Required)

  - `passphrase`: A string of passphrase for the private key or pfx.

  - `cert`: A string or `Buffer` containing the certificate key of the server in
    PEM format. (Required)

  - `ca`: An array of strings or `Buffer`s of trusted certificates. If this is
    omitted several well known "root" CAs will be used, like VeriSign.
    These are used to authorize connections.

  - `crl` : Either a string or list of strings of PEM encoded CRLs (Certificate
    Revocation List)

  - `ciphers`: A string describing the ciphers to use or exclude.

    To mitigate [BEAST attacks] it is recommended that you use this option in
    conjunction with the `honorCipherOrder` option described below to
    prioritize the non-CBC cipher.

    Defaults to
    `ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:RC4:HIGH:!MD5:!aNULL:!EDH`.
    Consult the [OpenSSL cipher list format documentation] for details on the
    format.

    `ECDHE-RSA-AES128-SHA256` and `AES128-GCM-SHA256` are used when node.js is
    linked against OpenSSL 1.0.1 or newer and the client speaks TLS 1.2, RC4 is
    used as a secure fallback.

    **NOTE**: Previous revisions of this section suggested `AES256-SHA` as an
    acceptable cipher. Unfortunately, `AES256-SHA` is a CBC cipher and therefore
    susceptible to BEAST attacks. Do *not* use it.

  - `honorCipherOrder` : When choosing a cipher, use the server's preferences
    instead of the client preferences.

    Note that if SSLv2 is used, the server will send its list of preferences
    to the client, and the client chooses the cipher.

    Although, this option is disabled by default, it is *recommended* that you
    use this option in conjunction with the `ciphers` option to mitigate
    BEAST attacks.

  - `requestCert`: If `true` the server will request a certificate from
    clients that connect and attempt to verify that certificate. Default:
    `false`.

  - `rejectUnauthorized`: If `true` the server will reject any connection
    which is not authorized with the list of supplied CAs. This option only
    has an effect if `requestCert` is `true`. Default: `false`.

  - `NPNProtocols`: An array or `Buffer` of possible NPN protocols. (Protocols
    should be ordered by their priority).

  - `SNICallback`: A function that will be called if client supports SNI TLS
    extension. Only one argument will be passed to it: `servername`. And
    `SNICallback` should return SecureContext instance.
    (You can use `crypto.createCredentials(...).context` to get proper
    SecureContext). If `SNICallback` wasn't provided - default callback with
    high-level API will be used (see below).

  - `sessionIdContext`: A string containing a opaque identifier for session
    resumption. If `requestCert` is `true`, the default is MD5 hash value
    generated from command-line. Otherwise, the default is not provided.

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

    var server = tls.createServer(options, function(cleartextStream) {
      console.log('server connected',
                  cleartextStream.authorized ? 'authorized' : 'unauthorized');
      cleartextStream.write("welcome!\n");
      cleartextStream.setEncoding('utf8');
      cleartextStream.pipe(cleartextStream);
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

    var server = tls.createServer(options, function(cleartextStream) {
      console.log('server connected',
                  cleartextStream.authorized ? 'authorized' : 'unauthorized');
      cleartextStream.write("welcome!\n");
      cleartextStream.setEncoding('utf8');
      cleartextStream.pipe(cleartextStream);
    });
    server.listen(8000, function() {
      console.log('server bound');
    });
You can test this server by connecting to it with `openssl s_client`:


    openssl s_client -connect 127.0.0.1:8000


## tls.connect(options, [callback])
## tls.connect(port, [host], [options], [callback])

Creates a new client connection to the given `port` and `host` (old API) or
`options.port` and `options.host`. (If `host` is omitted, it defaults to
`localhost`.) `options` should be an object which specifies:

  - `host`: Host the client should connect to

  - `port`: Port the client should connect to

  - `socket`: Establish secure connection on a given socket rather than
    creating a new socket. If this option is specified, `host` and `port`
    are ignored.

  - `pfx`: A string or `Buffer` containing the private key, certificate and
    CA certs of the server in PFX or PKCS12 format.

  - `key`: A string or `Buffer` containing the private key of the client in
    PEM format.

  - `passphrase`: A string of passphrase for the private key or pfx.

  - `cert`: A string or `Buffer` containing the certificate key of the client in
    PEM format.

  - `ca`: An array of strings or `Buffer`s of trusted certificates. If this is
    omitted several well known "root" CAs will be used, like VeriSign.
    These are used to authorize connections.

  - `rejectUnauthorized`: If `true`, the server certificate is verified against
    the list of supplied CAs. An `'error'` event is emitted if verification
    fails. Default: `false`.

  - `NPNProtocols`: An array of string or `Buffer` containing supported NPN
    protocols. `Buffer` should have following format: `0x05hello0x05world`,
    where first byte is next protocol name's length. (Passing array should
    usually be much simpler: `['hello', 'world']`.)

  - `servername`: Servername for SNI (Server Name Indication) TLS extension.

The `callback` parameter will be added as a listener for the
['secureConnect'][] event.

`tls.connect()` returns a [CleartextStream][] object.

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

    var cleartextStream = tls.connect(8000, options, function() {
      console.log('client connected',
                  cleartextStream.authorized ? 'authorized' : 'unauthorized');
      process.stdin.pipe(cleartextStream);
      process.stdin.resume();
    });
    cleartextStream.setEncoding('utf8');
    cleartextStream.on('data', function(data) {
      console.log(data);
    });
    cleartextStream.on('end', function() {
      server.close();
    });

Or

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      pfx: fs.readFileSync('client.pfx')
    };

    var cleartextStream = tls.connect(8000, options, function() {
      console.log('client connected',
                  cleartextStream.authorized ? 'authorized' : 'unauthorized');
      process.stdin.pipe(cleartextStream);
      process.stdin.resume();
    });
    cleartextStream.setEncoding('utf8');
    cleartextStream.on('data', function(data) {
      console.log(data);
    });
    cleartextStream.on('end', function() {
      server.close();
    });

## tls.createSecurePair([credentials], [isServer], [requestCert], [rejectUnauthorized])

Creates a new secure pair object with two streams, one of which reads/writes
encrypted data, and one reads/writes cleartext data.
Generally the encrypted one is piped to/from an incoming encrypted data stream,
and the cleartext one is used as a replacement for the initial encrypted stream.

 - `credentials`: A credentials object from crypto.createCredentials( ... )

 - `isServer`: A boolean indicating whether this tls connection should be
   opened as a server or a client.

 - `requestCert`: A boolean indicating whether a server should request a
   certificate from a connecting client. Only applies to server connections.

 - `rejectUnauthorized`: A boolean indicating whether a server should
   automatically reject clients with invalid certificates. Only applies to
   servers with `requestCert` enabled.

`tls.createSecurePair()` returns a SecurePair object with [cleartext][] and
`encrypted` stream properties.

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

`function (cleartextStream) {}`

This event is emitted after a new connection has been successfully
handshaked. The argument is a instance of [CleartextStream][]. It has all the
common stream methods and events.

`cleartextStream.authorized` is a boolean value which indicates if the
client has verified by one of the supplied certificate authorities for the
server. If `cleartextStream.authorized` is false, then
`cleartextStream.authorizationError` is set to describe how authorization
failed. Implied but worth mentioning: depending on the settings of the TLS
server, you unauthorized connections may be accepted.
`cleartextStream.npnProtocol` is a string containing selected NPN protocol.
`cleartextStream.servername` is a string containing servername requested with
SNI.


### Event: 'clientError'

`function (exception) { }`

When a client connection emits an 'error' event before secure connection is
established - it will be forwarded here.


### server.listen(port, [host], [callback])

Begin accepting connections on the specified `port` and `host`.  If the
`host` is omitted, the server will accept connections directed to any
IPv4 address (`INADDR_ANY`).

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.

See `net.Server` for more information.


### server.close()

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.

### server.address()

Returns the bound address, the address family name and port of the
server as reported by the operating system.  See [net.Server.address()][] for
more information.

### server.addContext(hostname, credentials)

Add secure context that will be used if client request's SNI hostname is
matching passed `hostname` (wildcards can be used). `credentials` can contain
`key`, `cert` and `ca`.

### server.maxConnections

Set this property to reject connections when the server's connection count
gets high.

### server.connections

The number of concurrent connections on the server.


## Class: tls.CleartextStream

This is a stream on top of the *Encrypted* stream that makes it possible to
read/write an encrypted data as a cleartext data.

This instance implements a duplex [Stream][] interfaces.  It has all the
common stream methods and events.

A ClearTextStream is the `clear` member of a SecurePair object.

### Event: 'secureConnect'

This event is emitted after a new connection has been successfully handshaked. 
The listener will be called no matter if the server's certificate was
authorized or not. It is up to the user to test `cleartextStream.authorized`
to see if the server certificate was signed by one of the specified CAs.
If `cleartextStream.authorized === false` then the error can be found in
`cleartextStream.authorizationError`. Also if NPN was used - you can check
`cleartextStream.npnProtocol` for negotiated protocol.

### cleartextStream.authorized

A boolean that is `true` if the peer certificate was signed by one of the
specified CAs, otherwise `false`

### cleartextStream.authorizationError

The reason why the peer's certificate has not been verified. This property
becomes available only when `cleartextStream.authorized === false`.

### cleartextStream.getPeerCertificate()

Returns an object representing the peer's certificate. The returned object has
some properties corresponding to the field of the certificate.

Example:

    { subject: 
       { C: 'UK',
         ST: 'Acknack Ltd',
         L: 'Rhys Jones',
         O: 'node.js',
         OU: 'Test TLS Certificate',
         CN: 'localhost' },
      issuer: 
       { C: 'UK',
         ST: 'Acknack Ltd',
         L: 'Rhys Jones',
         O: 'node.js',
         OU: 'Test TLS Certificate',
         CN: 'localhost' },
      valid_from: 'Nov 11 09:52:22 2009 GMT',
      valid_to: 'Nov  6 09:52:22 2029 GMT',
      fingerprint: '2A:7A:C2:DD:E5:F9:CC:53:72:35:99:7A:02:5A:71:38:52:EC:8A:DF' }

If the peer does not provide a certificate, it returns `null` or an empty
object.

### cleartextStream.getCipher()
Returns an object representing the cipher name and the SSL/TLS
protocol version of the current connection.

Example:
{ name: 'AES256-SHA', version: 'TLSv1/SSLv3' }

See SSL_CIPHER_get_name() and SSL_CIPHER_get_version() in
http://www.openssl.org/docs/ssl/ssl.html#DEALING_WITH_CIPHERS for more
information.

### cleartextStream.address()

Returns the bound address, the address family name and port of the
underlying socket as reported by the operating system. Returns an
object with three properties, e.g.
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

### cleartextStream.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

### cleartextStream.remotePort

The numeric representation of the remote port. For example, `443`.

[OpenSSL cipher list format documentation]: http://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT
[BEAST attacks]: http://blog.ivanristic.com/2011/10/mitigating-the-beast-attack-on-tls.html
[CleartextStream]: #tls_class_tls_cleartextstream
[net.Server.address()]: net.html#net_server_address
['secureConnect']: #tls_event_secureconnect
[secureConnection]: #tls_event_secureconnection
[Stream]: stream.html#stream_stream
[tls.Server]: #tls_class_tls_server
