## net

The `net` module provides you with an asynchronous network wrapper. It contains
methods for creating both servers and clients (called streams). You can include
this module with `require("net");`

### net.createServer(connectionListener)

Creates a new TCP server. The `connectionListener` argument is
automatically set as a listener for the `'connection'` event.

### net.createConnection(arguments...)

Construct a new stream object and opens a stream to the given location. When
the stream is established the `'connect'` event will be emitted.

The arguments for this method change the type of connection:

* `net.createConnection(port, [host])`

  Creates a TCP connection to `port` on `host`. If `host` is omitted, `localhost`
  will be assumed.

* `net.createConnection(path)`

  Creates unix socket connection to `path`

---

### net.Server

This class is used to create a TCP or UNIX server.

Here is an example of a echo server which listens for connections
on port 8124:

    var net = require('net');
    var server = net.createServer(function (c) {
      c.write('hello\r\n');
      c.pipe(c);
    });
    server.listen(8124, 'localhost');

Test this by using `telnet`:

    telnet localhost 8124

To listen on the socket `/tmp/echo.sock` the last line would just be
changed to

    server.listen('/tmp/echo.sock');

Use `nc` to connect to a UNIX domain socket server:

    nc -U /tmp/echo.sock

`net.Server` is an `EventEmitter` with the following events:

#### server.listen(port, [host], [callback])

Begin accepting connections on the specified `port` and `host`.  If the
`host` is omitted, the server will accept connections directed to any
IPv4 address (`INADDR_ANY`).

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.

One issue some users run into is getting `EADDRINUSE` errors. Meaning
another server is already running on the requested port. One way of handling this
would be to wait a second and the try again. This can be done with

    server.on('error', function (e) {
      if (e.errno == require('constants').EADDRINUSE) {
        console.log('Address in use, retrying...');
        setTimeout(function () {
          server.close();
          server.listen(PORT, HOST);
        }, 1000);
      }
    });

(Note: All sockets in Node are set SO_REUSEADDR already)


#### server.listen(path, [callback])

Start a UNIX socket server listening for connections on the given `path`.

This function is asynchronous. The last parameter `callback` will be called
when the server has been bound.

#### server.listenFD(fd)

Start a server listening for connections on the given file descriptor.

This file descriptor must have already had the `bind(2)` and `listen(2)` system
calls invoked on it.

#### server.close()

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.


#### server.address()

Returns the bound address of the server as seen by the operating system.
Useful to find which port was assigned when giving getting an OS-assigned address

Example:

    var server = net.createServer(function (socket) {
      socket.end("goodbye\n");
    });

    // grab a random port.
    server.listen(function() {
      address = server.address();
      console.log("opened server on %j", address);
    });


#### server.maxConnections

Set this property to reject connections when the server's connection count gets high.

#### server.connections

The number of concurrent connections on the server.

#### Event: 'connection'

`function (stream) {}`

Emitted when a new connection is made. `stream` is an instance of
`net.Stream`.

#### Event: 'close'

`function () {}`

Emitted when the server closes.

---

### net.Stream

This object is an abstraction of of a TCP or UNIX socket.  `net.Stream`
instance implement a duplex stream interface.  They can be created by the
user and used as a client (with `connect()`) or they can be created by Node
and passed to the user through the `'connection'` event of a server.

`net.Stream` instances are EventEmitters with the following events:

#### stream.connect(port, [host])
#### stream.connect(path)

Opens the connection for a given stream. If `port` and `host` are given,
then the stream will be opened as a TCP stream, if `host` is omitted,
`localhost` will be assumed. If a `path` is given, the stream will be
opened as a unix socket to that path.

Normally this method is not needed, as `net.createConnection` opens the
stream. Use this only if you are implementing a custom Stream or if a
Stream is closed and you want to reuse it to connect to another server.

This function is asynchronous. When the `'connect'` event is emitted the
stream is established. If there is a problem connecting, the `'connect'`
event will not be emitted, the `'error'` event will be emitted with
the exception.


#### stream.setEncoding(encoding=null)

Sets the encoding (either `'ascii'`, `'utf8'`, or `'base64'`) for data that is
received.

#### stream.setSecure([credentials])

Enables SSL support for the stream, with the crypto module credentials specifying
the private key and certificate of the stream, and optionally the CA certificates
for use in peer authentication.

If the credentials hold one ore more CA certificates, then the stream will request
for the peer to submit a client certificate as part of the SSL connection handshake.
The validity and content of this can be accessed via `verifyPeer()` and `getPeerCertificate()`.

#### stream.verifyPeer()

Returns true or false depending on the validity of the peers's certificate in the
context of the defined or default list of trusted CA certificates.

#### stream.getPeerCertificate()

Returns a JSON structure detailing the peer's certificate, containing a dictionary
with keys for the certificate `'subject'`, `'issuer'`, `'valid_from'` and `'valid_to'`.

#### stream.write(data, [encoding])

Sends data on the stream. The second parameter specifies the encoding in the
case of a string--it defaults to UTF8 encoding.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is again free.

#### stream.write(data, [encoding], [fileDescriptor])

For UNIX sockets, it is possible to send a file descriptor through the
stream. Simply add the `fileDescriptor` argument and listen for the `'fd'`
event on the other end.


#### stream.end([data], [encoding])

Half-closes the stream. I.E., it sends a FIN packet. It is possible the
server will still send some data.

If `data` is specified, it is equivalent to calling `stream.write(data, encoding)`
followed by `stream.end()`.

#### stream.destroy()

Ensures that no more I/O activity happens on this stream. Only necessary in
case of errors (parse error or so).

#### stream.pause()

Pauses the reading of data. That is, `'data'` events will not be emitted.
Useful to throttle back an upload.

#### stream.resume()

Resumes reading after a call to `pause()`.

#### stream.setTimeout(timeout)

Sets the stream to timeout after `timeout` milliseconds of inactivity on
the stream. By default `net.Stream` do not have a timeout.

When an idle timeout is triggered the stream will receive a `'timeout'`
event but the connection will not be severed. The user must manually `end()`
or `destroy()` the stream.

If `timeout` is 0, then the existing idle timeout is disabled.

#### stream.setNoDelay(noDelay=true)

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `noDelay` will
immediately fire off data each time `stream.write()` is called.

#### stream.setKeepAlive(enable=false, [initialDelay])

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle stream.
Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting 0 for
initialDelay will leave the value unchanged from the default
(or previous) setting.

#### stream.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

This member is only present in server-side connections.


#### Event: 'connect'

`function () { }`

Emitted when a stream connection successfully is established.
See `connect()`.

#### Event: 'data'

`function (data) { }`

Emitted when data is received.  The argument `data` will be a `Buffer` or
`String`.  Encoding of data is set by `stream.setEncoding()`.
(See the section on `Readable Stream` for more information.)

#### Event: 'end'

`function () { }`

Emitted when the other end of the stream sends a FIN packet.

By default (`allowHalfOpen == false`) the stream will destroy its file
descriptor  once it has written out its pending write queue.  However, by
setting `allowHalfOpen == true` the stream will not automatically `end()`
its side allowing the user to write arbitrary amounts of data, with the
caveat that the user is required to `end()` their side now.


#### Event: 'timeout'

`function () { }`

Emitted if the stream times out from inactivity. This is only to notify that
the stream has been idle. The user must manually close the connection.

See also: `stream.setTimeout()`


#### Event: 'drain'

`function () { }`

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

#### Event: 'error'

`function (exception) { }`

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.

#### Event: 'close'

`function (had_error) { }`

Emitted once the stream is fully closed. The argument `had_error` is a boolean
which says if the stream was closed due to a transmission error.

---

### net.isIP

#### net.isIP(input)

Tests if input is an IP address. Returns 0 for invalid strings,
returns 4 for IP version 4 addresses, and returns 6 for IP version 6 addresses.


#### net.isIPv4(input)

Returns true if input is a version 4 IP address, otherwise returns false.


#### net.isIPv6(input)

Returns true if input is a version 6 IP address, otherwise returns false.

