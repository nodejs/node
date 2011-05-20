## net

The `net` module provides you with an asynchronous network wrapper. It contains
methods for creating both servers and clients (called streams). You can include
this module with `require("net");`

### net.createServer([options], [connectionListener])

Creates a new TCP server. The `connectionListener` argument is
automatically set as a listener for the `'connection'` event.

`options` is an object with the following defaults:

    { allowHalfOpen: false
    }

If `allowHalfOpen` is `true`, then the socket won't automatically send FIN
packet when the other end of the socket sends a FIN packet. The socket becomes
non-readable, but still writable. You should call the end() method explicitly.
See `'end'` event for more information.

### net.createConnection(arguments...)

Construct a new socket object and opens a socket to the given location. When
the socket is established the `'connect'` event will be emitted.

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
      if (e.code == 'EADDRINUSE') {
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
calls invoked on it.  Additionally, it must be set non-blocking; try
`fcntl(fd, F_SETFL, O_NONBLOCK)`.

#### server.pause(msecs)

Stop accepting connections for the given number of milliseconds (default is
one second).  This could be useful for throttling new connections against
DoS attacks or other oversubscription.

#### server.close()

Stops the server from accepting new connections. This function is
asynchronous, the server is finally closed when the server emits a `'close'`
event.


#### server.address()

Returns the bound address and port of the server as reported by the operating system.
Useful to find which port was assigned when giving getting an OS-assigned address.
Returns an object with two properties, e.g. `{"address":"127.0.0.1", "port":2121}`

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

`function (socket) {}`

Emitted when a new connection is made. `socket` is an instance of
`net.Socket`.

#### Event: 'close'

`function () {}`

Emitted when the server closes.

---

### net.Socket

This object is an abstraction of of a TCP or UNIX socket.  `net.Socket`
instances implement a duplex Stream interface.  They can be created by the
user and used as a client (with `connect()`) or they can be created by Node
and passed to the user through the `'connection'` event of a server.

`net.Socket` instances are EventEmitters with the following events:

#### new net.Socket([options])

Construct a new socket object.

`options` is an object with the following defaults:

    { fd: null
      type: null
      allowHalfOpen: false
    }

`fd` allows you to specify the existing file descriptor of socket. `type`
specified underlying protocol. It can be `'tcp4'`, `'tcp6'`, or `'unix'`.
About `allowHalfOpen`, refer to `createServer()` and `'end'` event.

#### socket.connect(port, [host], [callback])
#### socket.connect(path, [callback])

Opens the connection for a given socket. If `port` and `host` are given,
then the socket will be opened as a TCP socket, if `host` is omitted,
`localhost` will be assumed. If a `path` is given, the socket will be
opened as a unix socket to that path.

Normally this method is not needed, as `net.createConnection` opens the
socket. Use this only if you are implementing a custom Socket or if a
Socket is closed and you want to reuse it to connect to another server.

This function is asynchronous. When the `'connect'` event is emitted the
socket is established. If there is a problem connecting, the `'connect'`
event will not be emitted, the `'error'` event will be emitted with
the exception.

The `callback` parameter will be added as an listener for the 'connect'
event.


#### socket.bufferSize

`net.Socket` has the property that `socket.write()` always works. This is to
help users get up an running quickly. The computer cannot necessarily keep up
with the amount of data that is written to a socket - the network connection simply
might be too slow. Node will internally queue up the data written to a socket and
send it out over the wire when it is possible. (Internally it is polling on
the socket's file descriptor for being writable).

The consequence of this internal buffering is that memory may grow. This
property shows the number of characters currently buffered to be written.
(Number of characters is approximately equal to the number of bytes to be
written, but the buffer may contain strings, and the strings are lazily
encoded, so the exact number of bytes is not known.)

Users who experience large or growing `bufferSize` should attempt to
"throttle" the data flows in their program with `pause()` and resume()`.


#### socket.setEncoding(encoding=null)

Sets the encoding (either `'ascii'`, `'utf8'`, or `'base64'`) for data that is
received.

#### socket.setSecure()

This function has been removed in v0.3. It used to upgrade the connection to
SSL/TLS. See the [TLS section](tls.html#tLS_) for the new API.


#### socket.write(data, [encoding], [callback])

Sends data on the socket. The second parameter specifies the encoding in the
case of a string--it defaults to UTF8 encoding.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is again free.

The optional `callback` parameter will be executed when the data is finally
written out - this may not be immediately.

#### socket.write(data, [encoding], [fileDescriptor], [callback])

For UNIX sockets, it is possible to send a file descriptor through the
socket. Simply add the `fileDescriptor` argument and listen for the `'fd'`
event on the other end.


#### socket.end([data], [encoding])

Half-closes the socket. I.E., it sends a FIN packet. It is possible the
server will still send some data.

If `data` is specified, it is equivalent to calling `socket.write(data, encoding)`
followed by `socket.end()`.

#### socket.destroy()

Ensures that no more I/O activity happens on this socket. Only necessary in
case of errors (parse error or so).

#### socket.pause()

Pauses the reading of data. That is, `'data'` events will not be emitted.
Useful to throttle back an upload.

#### socket.resume()

Resumes reading after a call to `pause()`.

#### socket.setTimeout(timeout, [callback])

Sets the socket to timeout after `timeout` milliseconds of inactivity on
the socket. By default `net.Socket` do not have a timeout.

When an idle timeout is triggered the socket will receive a `'timeout'`
event but the connection will not be severed. The user must manually `end()`
or `destroy()` the socket.

If `timeout` is 0, then the existing idle timeout is disabled.

The optional `callback` parameter will be added as a one time listener for the `'timeout'` event.

#### socket.setNoDelay(noDelay=true)

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `noDelay` will
immediately fire off data each time `socket.write()` is called.

#### socket.setKeepAlive(enable=false, [initialDelay])

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle socket.
Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting 0 for
initialDelay will leave the value unchanged from the default
(or previous) setting.

#### socket.address()

Returns the bound address and port of the socket as reported by the operating system.
Returns an object with two properties, e.g. `{"address":"192.168.57.1", "port":62053}`

#### socket.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

This member is only present in server-side connections.


#### Event: 'connect'

`function () { }`

Emitted when a socket connection successfully is established.
See `connect()`.

#### Event: 'data'

`function (data) { }`

Emitted when data is received.  The argument `data` will be a `Buffer` or
`String`.  Encoding of data is set by `socket.setEncoding()`.
(See the [Readable Stream](streams.html#readable_Stream) section for more information.)

#### Event: 'end'

`function () { }`

Emitted when the other end of the socket sends a FIN packet.

By default (`allowHalfOpen == false`) the socket will destroy its file
descriptor  once it has written out its pending write queue.  However, by
setting `allowHalfOpen == true` the socket will not automatically `end()`
its side allowing the user to write arbitrary amounts of data, with the
caveat that the user is required to `end()` their side now.


#### Event: 'timeout'

`function () { }`

Emitted if the socket times out from inactivity. This is only to notify that
the socket has been idle. The user must manually close the connection.

See also: `socket.setTimeout()`


#### Event: 'drain'

`function () { }`

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

#### Event: 'error'

`function (exception) { }`

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.

#### Event: 'close'

`function (had_error) { }`

Emitted once the socket is fully closed. The argument `had_error` is a boolean
which says if the socket was closed due to a transmission error.

---

### net.isIP

#### net.isIP(input)

Tests if input is an IP address. Returns 0 for invalid strings,
returns 4 for IP version 4 addresses, and returns 6 for IP version 6 addresses.


#### net.isIPv4(input)

Returns true if input is a version 4 IP address, otherwise returns false.


#### net.isIPv6(input)

Returns true if input is a version 6 IP address, otherwise returns false.

