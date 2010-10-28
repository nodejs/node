## net.Server

This class is used to create a TCP or UNIX server.

Here is an example of a echo server which listens for connections
on port 8124:

    var net = require('net');
    var server = net.createServer(function (stream) {
      stream.setEncoding('utf8');
      stream.on('connect', function () {
        stream.write('hello\r\n');
      });
      stream.on('data', function (data) {
        stream.write(data);
      });
      stream.on('end', function () {
        stream.write('goodbye\r\n');
        stream.end();
      });
    });
    server.listen(8124, 'localhost');

To listen on the socket `'/tmp/echo.sock'`, the last line would just be
changed to

    server.listen('/tmp/echo.sock');

This is an `EventEmitter` with the following events:

### Event: 'connection'

`function (stream) {}`

Emitted when a new connection is made. `stream` is an instance of
`net.Stream`.

### Event: 'close'

`function () {}`

Emitted when the server closes.


### net.createServer(connectionListener)

Creates a new TCP server. The `connectionListener` argument is
automatically set as a listener for the `'connection'` event.


### server.listen(port, [host], [callback])

Begin accepting connections on the specified `port` and `host`.  If the
`host` is omitted, the server will accept connections directed to any
IPv4 address (`INADDR_ANY`).

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.


### server.listen(path, [callback])

Start a UNIX socket server listening for connections on the given `path`.

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.


### server.listenFD(fd)

Start a server listening for connections on the given file descriptor.

This file descriptor must have already had the `bind(2)` and `listen(2)` system
calls invoked on it.

### server.close()

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.

### server.maxConnections

Set this property to reject connections when the server's connection count gets high.

### server.connections

The number of concurrent connections on the server.


## net.isIP

### net.isIP(input)

Tests if input is an IP address. Returns 0 for invalid strings,
returns 4 for IP version 4 addresses, and returns 6 for IP version 6 addresses.


### net.isIPv4(input)

Returns true if input is a version 4 IP address, otherwise returns false.


### net.isIPv6(input)

Returns true if input is a version 6 IP address, otherwise returns false.


## net.Stream

This object is an abstraction of of a TCP or UNIX socket.  `net.Stream`
instance implement a duplex stream interface.  They can be created by the
user and used as a client (with `connect()`) or they can be created by Node
and passed to the user through the `'connection'` event of a server.

`net.Stream` instances are EventEmitters with the following events:

### Event: 'connect'

`function () { }`

Emitted when a stream connection successfully is established.
See `connect()`.


### Event: 'secure'

`function () { }`

Emitted when a stream connection successfully establishes an SSL handshake with its peer.


### Event: 'data'

`function (data) { }`

Emitted when data is received.  The argument `data` will be a `Buffer` or
`String`.  Encoding of data is set by `stream.setEncoding()`.
(See the section on `Readable Stream` for more information.)

### Event: 'end'

`function () { }`

Emitted when the other end of the stream sends a FIN packet. 

By default (`allowHalfOpen == false`) the stream will destroy its file
descriptor  once it has written out its pending write queue.  However, by
setting `allowHalfOpen == true` the stream will not automatically `end()`
its side allowing the user to write arbitrary amounts of data, with the
caviot that the user is required to `end()` thier side now. In the
`allowHalfOpen == true` case after `'end'` is emitted the `readyState` will
be `'writeOnly'`.


### Event: 'timeout'

`function () { }`

Emitted if the stream times out from inactivity. This is only to notify that
the stream has been idle. The user must manually close the connection.

See also: `stream.setTimeout()`


### Event: 'drain'

`function () { }`

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

### Event: 'error'

`function (exception) { }`

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.

### Event: 'close'

`function (had_error) { }`

Emitted once the stream is fully closed. The argument `had_error` is a boolean which says if
the stream was closed due to a transmission
error.


### net.createConnection(port, host='127.0.0.1')

Construct a new stream object and opens a stream to the specified `port`
and `host`. If the second parameter is omitted, localhost is assumed.

When the stream is established the `'connect'` event will be emitted.

### stream.connect(port, host='127.0.0.1')

Opens a stream to the specified `port` and `host`. `createConnection()`
also opens a stream; normally this method is not needed. Use this only if
a stream is closed and you want to reuse the object to connect to another
server.

This function is asynchronous. When the `'connect'` event is emitted the
stream is established. If there is a problem connecting, the `'connect'`
event will not be emitted, the `'error'` event will be emitted with 
the exception.


### stream.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

This member is only present in server-side connections.

### stream.readyState

Either `'closed'`, `'open'`, `'opening'`, `'readOnly'`, or `'writeOnly'`.

### stream.setEncoding(encoding=null)

Sets the encoding (either `'ascii'`, `'utf8'`, or `'base64'`) for data that is
received.

### stream.setSecure([credentials])

Enables SSL support for the stream, with the crypto module credentials specifying the private key and certificate of the stream, and optionally the CA certificates for use in peer authentication.

If the credentials hold one ore more CA certificates, then the stream will request for the peer to submit a client certificate as part of the SSL connection handshake. The validity and content of this can be accessed via verifyPeer() and getPeerCertificate().

### stream.verifyPeer()

Returns true or false depending on the validity of the peers's certificate in the context of the defined or default list of trusted CA certificates.

### stream.getPeerCertificate()

Returns a JSON structure detailing the peer's certificate, containing a dictionary with keys for the certificate 'subject', 'issuer', 'valid\_from' and 'valid\_to'

### stream.write(data, encoding='ascii')

Sends data on the stream. The second parameter specifies the encoding in
the case of a string--it defaults to ASCII because encoding to UTF8 is rather
slow.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is again free.

### stream.end([data], [encoding])

Half-closes the stream. I.E., it sends a FIN packet. It is possible the
server will still send some data. After calling this `readyState` will be
`'readOnly'`.

If `data` is specified, it is equivalent to calling `stream.write(data, encoding)`
followed by `stream.end()`.

### stream.destroy()

Ensures that no more I/O activity happens on this stream. Only necessary in
case of errors (parse error or so).

### stream.pause()

Pauses the reading of data. That is, `'data'` events will not be emitted.
Useful to throttle back an upload.

### stream.resume()

Resumes reading after a call to `pause()`.

### stream.setTimeout(timeout)

Sets the stream to timeout after `timeout` milliseconds of inactivity on
the stream. By default `net.Stream` do not have a timeout.

When an idle timeout is triggered the stream will receive a `'timeout'`
event but the connection will not be severed. The user must manually `end()`
or `destroy()` the stream.

If `timeout` is 0, then the existing idle timeout is disabled.

### stream.setNoDelay(noDelay=true)

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `noDelay` will
immediately fire off data each time `stream.write()` is called.

### stream.setKeepAlive(enable=false, [initialDelay])

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle stream.
Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting 0 for
initialDelay will leave the value unchanged from the default
(or previous) setting.
