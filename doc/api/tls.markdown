## TLS (SSL)

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


### s = tls.connect(port, [host], [options], callback)

Creates a new client connection to the given `port` and `host`. (If `host`
defaults to `localhost`.) `options` should be an object which specifies

  - `key`: A string or `Buffer` containing the private key of the server in
    PEM format. (Required)

  - `cert`: A string or `Buffer` containing the certificate key of the server in
    PEM format.

  - `ca`: An array of strings or `Buffer`s of trusted certificates. If this is
    omitted several well known "root" CAs will be used, like VeriSign.
    These are used to authorize connections.

  - `NPNProtocols`: An array of string or `Buffer` containing supported NPN
    protocols. `Buffer` should have following format: `0x05hello0x05world`, where
    first byte is next protocol name's length. (Passing array should usually be
    much simplier: `['hello', 'world']`.)

  - `servername`: Servername for SNI (Server Name Indication) TLS extension.

`tls.connect()` returns a [CleartextStream](#tls.CleartextStream) object.

After the TLS/SSL handshake the `callback` is called. The `callback` will be
called no matter if the server's certificate was authorized or not. It is up
to the user to test `s.authorized` to see if the server certificate was signed
by one of the specified CAs. If `s.authorized === false` then the error
can be found in `s.authorizationError`. Also if NPN was used - you can check
`s.npnProtocol` for negotiated protocol.


### STARTTLS

In the v0.4 branch no function exists for starting a TLS session on an
already existing TCP connection.  This is possible it just requires a bit of
work. The technique is to use `tls.createSecurePair()` which returns two
streams: an encrypted stream and a cleartext stream. The encrypted stream is
then piped to the socket, the cleartext stream is what the user interacts with
thereafter.

[Here is some code that does it.](http://gist.github.com/848444)

### NPN and SNI

NPN (Next Protocol Negotitation) and SNI (Server Name Indication) are TLS
handshake extensions allowing you:

  * NPN - to use one TLS server for multiple protocols (HTTP, SPDY)
  * SNI - to use one TLS server for multiple hostnames with different SSL
    certificates.

### pair = tls.createSecurePair([credentials], [isServer], [requestCert], [rejectUnauthorized])

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

`tls.createSecurePair()` returns a SecurePair object with
[cleartext](#tls.CleartextStream) and `encrypted` stream properties.

#### Event: 'secure'

The event is emitted from the SecurePair once the pair has successfully
established a secure connection.

Similarly to the checking for the server 'secureConnection' event,
pair.cleartext.authorized should be checked to confirm whether the certificate
used properly authorized.

### tls.Server

This class is a subclass of `net.Server` and has the same methods on it.
Instead of accepting just raw TCP connections, this accepts encrypted
connections using TLS or SSL.

Here is a simple example echo server:

    var tls = require('tls');
    var fs = require('fs');

    var options = {
      key: fs.readFileSync('server-key.pem'),
      cert: fs.readFileSync('server-cert.pem')
    };

    tls.createServer(options, function (s) {
      s.write("welcome!\n");
      s.pipe(s);
    }).listen(8000);


You can test this server by connecting to it with `openssl s_client`:


    openssl s_client -connect 127.0.0.1:8000


#### tls.createServer(options, secureConnectionListener)

This is a constructor for the `tls.Server` class. The options object
has these possibilities:

  - `key`: A string or `Buffer` containing the private key of the server in
    PEM format. (Required)

  - `cert`: A string or `Buffer` containing the certificate key of the server in
    PEM format. (Required)

  - `ca`: An array of strings or `Buffer`s of trusted certificates. If this is
    omitted several well known "root" CAs will be used, like VeriSign.
    These are used to authorize connections.

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


#### Event: 'secureConnection'

`function (cleartextStream) {}`

This event is emitted after a new connection has been successfully
handshaked. The argument is a instance of
[CleartextStream](#tls.CleartextStream). It has all the common stream methods
and events.

`cleartextStream.authorized` is a boolean value which indicates if the
client has verified by one of the supplied certificate authorities for the
server. If `cleartextStream.authorized` is false, then
`cleartextStream.authorizationError` is set to describe how authorization
failed. Implied but worth mentioning: depending on the settings of the TLS
server, you unauthorized connections may be accepted.
`cleartextStream.npnProtocol` is a string containing selected NPN protocol.
`cleartextStream.servername` is a string containing servername requested with
SNI.


#### server.listen(port, [host], [callback])

Begin accepting connections on the specified `port` and `host`.  If the
`host` is omitted, the server will accept connections directed to any
IPv4 address (`INADDR_ANY`).

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.

See `net.Server` for more information.


#### server.close()

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.

#### server.addContext(hostname, credentials)

Add secure context that will be used if client request's SNI hostname is
matching passed `hostname` (wildcards can be used). `credentials` can contain
`key`, `cert` and `ca`.

#### server.maxConnections

Set this property to reject connections when the server's connection count
gets high.

#### server.connections

The number of concurrent connections on the server.


### tls.CleartextStream

This is a stream on top of the *Encrypted* stream that makes it possible to
read/write an encrypted data as a cleartext data.

This instance implements a duplex [Stream](streams.html#streams) interfaces.
It has all the common stream methods and events.

#### cleartextStream.authorized

A boolean that is `true` if the peer certificate was signed by one of the
specified CAs, otherwise `false`

#### cleartextStream.authorizationError

The reason why the peer's certificate has not been verified. This property
becomes available only when `cleartextStream.authorized === false`.

#### cleartextStream.getPeerCertificate()

Returns an object representing the peer's certicicate. The returned object has
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

