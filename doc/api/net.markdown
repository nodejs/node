# net

    Stability: 3 - Stable

The `net` module provides you with an asynchronous network wrapper. It contains
methods for creating both servers and clients (called streams). You can include
this module with `require('net');`

## net.createServer([options][, connectionListener])

Creates a new TCP server. The `connectionListener` argument is
automatically set as a listener for the ['connection'][] event.

`options` is an object with the following defaults:

    {
      allowHalfOpen: false,
      pauseOnConnect: false
    }

If `allowHalfOpen` is `true`, then the socket won't automatically send a FIN
packet when the other end of the socket sends a FIN packet. The socket becomes
non-readable, but still writable. You should call the `end()` method explicitly.
See ['end'][] event for more information.

If `pauseOnConnect` is `true`, then the socket associated with each incoming
connection will be paused, and no data will be read from its handle. This allows
connections to be passed between processes without any data being read by the
original process. To begin reading data from a paused socket, call `resume()`.

Here is an example of an echo server which listens for connections
on port 8124:

    var net = require('net');
    var server = net.createServer(function(c) { //'connection' listener
      console.log('client connected');
      c.on('end', function() {
        console.log('client disconnected');
      });
      c.write('hello\r\n');
      c.pipe(c);
    });
    server.listen(8124, function() { //'listening' listener
      console.log('server bound');
    });

Test this by using `telnet`:

    telnet localhost 8124

To listen on the socket `/tmp/echo.sock` the third line from the last would
just be changed to

    server.listen('/tmp/echo.sock', function() { //'listening' listener

Use `nc` to connect to a UNIX domain socket server:

    nc -U /tmp/echo.sock

## net.connect(options[, connectionListener])
## net.createConnection(options[, connectionListener])

A factory method, which returns a new ['net.Socket'](#net_class_net_socket)
and connects to the supplied address and port.

When the socket is established, the ['connect'][] event will be emitted.

Has the same events as ['net.Socket'](#net_class_net_socket).

For TCP sockets, `options` argument should be an object which specifies:

  - `port`: Port the client should connect to (Required).

  - `host`: Host the client should connect to. Defaults to `'localhost'`.

  - `localAddress`: Local interface to bind to for network connections.

  - `localPort`: Local port to bind to for network connections.

  - `family` : Version of IP stack. Defaults to `4`.

For local domain sockets, `options` argument should be an object which
specifies:

  - `path`: Path the client should connect to (Required).

Common options are:

  - `allowHalfOpen`: if `true`, the socket won't automatically send
    a FIN packet when the other end of the socket sends a FIN packet.
    Defaults to `false`.  See ['end'][] event for more information.

The `connectListener` parameter will be added as an listener for the
['connect'][] event.

Here is an example of a client of echo server as described previously:

    var net = require('net');
    var client = net.connect({port: 8124},
        function() { //'connect' listener
      console.log('connected to server!');
      client.write('world!\r\n');
    });
    client.on('data', function(data) {
      console.log(data.toString());
      client.end();
    });
    client.on('end', function() {
      console.log('disconnected from server');
    });

To connect on the socket `/tmp/echo.sock` the second line would just be
changed to

    var client = net.connect({path: '/tmp/echo.sock'});

## net.connect(port[, host][, connectListener])
## net.createConnection(port[, host][, connectListener])

Creates a TCP connection to `port` on `host`. If `host` is omitted,
`'localhost'` will be assumed.
The `connectListener` parameter will be added as an listener for the
['connect'][] event.

Is a factory method which returns a new ['net.Socket'](#net_class_net_socket).

## net.connect(path[, connectListener])
## net.createConnection(path[, connectListener])

Creates unix socket connection to `path`.
The `connectListener` parameter will be added as an listener for the
['connect'][] event.

A factory method which returns a new ['net.Socket'](#net_class_net_socket).

## Class: net.Server

This class is used to create a TCP or local server.

### server.listen(port[, hostname][, backlog][, callback])

Begin accepting connections on the specified `port` and `hostname`. If the
`hostname` is omitted, the server will accept connections on any IPv6 address
(`::`) when IPv6 is available, or any IPv4 address (`0.0.0.0`) otherwise. A
port value of zero will assign a random port.

Backlog is the maximum length of the queue of pending connections.
The actual length will be determined by your OS through sysctl settings such as
`tcp_max_syn_backlog` and `somaxconn` on linux. The default value of this
parameter is 511 (not 512).

This function is asynchronous.  When the server has been bound,
['listening'][] event will be emitted.  The last parameter `callback`
will be added as an listener for the ['listening'][] event.

One issue some users run into is getting `EADDRINUSE` errors. This means that
another server is already running on the requested port. One way of handling this
would be to wait a second and then try again. This can be done with

    server.on('error', function (e) {
      if (e.code == 'EADDRINUSE') {
        console.log('Address in use, retrying...');
        setTimeout(function () {
          server.close();
          server.listen(PORT, HOST);
        }, 1000);
      }
    });

(Note: All sockets in io.js set `SO_REUSEADDR` already)


### server.listen(path[, callback])

* `path` {String}
* `callback` {Function}

Start a local socket server listening for connections on the given `path`.

This function is asynchronous.  When the server has been bound,
['listening'][] event will be emitted.  The last parameter `callback`
will be added as an listener for the ['listening'][] event.

On UNIX, the local domain is usually known as the UNIX domain. The path is a
filesystem path name. It is subject to the same naming conventions and
permissions checks as would be done on file creation, will be visible in the
filesystem, and will *persist until unlinked*.

On Windows, the local domain is implemented using a named pipe. The path *must*
refer to an entry in `\\?\pipe\` or `\\.\pipe\`. Any characters are permitted,
but the latter may do some processing of pipe names, such as resolving `..`
sequences. Despite appearances, the pipe name space is flat.  Pipes will *not
persist*, they are removed when the last reference to them is closed. Do not
forget javascript string escaping requires paths to be specified with
double-backslashes, such as:

    net.createServer().listen(
        path.join('\\\\?\\pipe', process.cwd(), 'myctl'))

### server.listen(handle[, callback])

* `handle` {Object}
* `callback` {Function}

The `handle` object can be set to either a server or socket (anything
with an underlying `_handle` member), or a `{fd: <n>}` object.

This will cause the server to accept connections on the specified
handle, but it is presumed that the file descriptor or handle has
already been bound to a port or domain socket.

Listening on a file descriptor is not supported on Windows.

This function is asynchronous.  When the server has been bound,
['listening'][] event will be emitted.
the last parameter `callback` will be added as an listener for the
['listening'][] event.

### server.listen(options[, callback])

* `options` {Object} - Required. Supports the following properties:
  * `port` {Number} - Optional.
  * `host` {String} - Optional.
  * `backlog` {Number} - Optional.
  * `path` {String} - Optional.
  * `exclusive` {Boolean} - Optional.
* `callback` {Function} - Optional.

The `port`, `host`, and `backlog` properties of `options`, as well as the
optional callback function, behave as they do on a call to
[server.listen(port, \[host\], \[backlog\], \[callback\])
](#net_server_listen_port_host_backlog_callback). Alternatively, the `path`
option can be used to specify a UNIX socket.

If `exclusive` is `false` (default), then cluster workers will use the same
underlying handle, allowing connection handling duties to be shared. When
`exclusive` is `true`, the handle is not shared, and attempted port sharing
results in an error. An example which listens on an exclusive port is
shown below.

    server.listen({
      host: 'localhost',
      port: 80,
      exclusive: true
    });

### server.close([callback])

Stops the server from accepting new connections and keeps existing
connections. This function is asynchronous, the server is finally
closed when all connections are ended and the server emits a `'close'`
event. Optionally, you can pass a callback to listen for the `'close'`
event. If present, the callback is invoked with any potential error
as the first and only argument.

### server.address()

Returns the bound address, the address family name and port of the server
as reported by the operating system.
Useful to find which port was assigned when giving getting an OS-assigned address.
Returns an object with three properties, e.g.
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

Example:

    var server = net.createServer(function (socket) {
      socket.end("goodbye\n");
    });

    // grab a random port.
    server.listen(function() {
      address = server.address();
      console.log("opened server on %j", address);
    });

Don't call `server.address()` until the `'listening'` event has been emitted.

### server.unref()

Calling `unref` on a server will allow the program to exit if this is the only
active server in the event system. If the server is already `unref`d calling
`unref` again will have no effect.

### server.ref()

Opposite of `unref`, calling `ref` on a previously `unref`d server will *not*
let the program exit if it's the only server left (the default behavior). If
the server is `ref`d calling `ref` again will have no effect.

### server.maxConnections

Set this property to reject connections when the server's connection count gets
high.

It is not recommended to use this option once a socket has been sent to a child
with `child_process.fork()`.

### server.connections

This function is **deprecated**; please use [server.getConnections()][] instead.
The number of concurrent connections on the server.

This becomes `null` when sending a socket to a child with
`child_process.fork()`. To poll forks and get current number of active
connections use asynchronous `server.getConnections` instead.

### server.getConnections(callback)

Asynchronously get the number of concurrent connections on the server. Works
when sockets were sent to forks.

Callback should take two arguments `err` and `count`.

`net.Server` is an [EventEmitter][] with the following events:

### Event: 'listening'

Emitted when the server has been bound after calling `server.listen`.

### Event: 'connection'

* {Socket object} The connection object

Emitted when a new connection is made. `socket` is an instance of
`net.Socket`.

### Event: 'close'

Emitted when the server closes. Note that if connections exist, this
event is not emitted until all connections are ended.

### Event: 'error'

* {Error Object}

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.  See example in discussion of `server.listen`.

## Class: net.Socket

This object is an abstraction of a TCP or local socket.  `net.Socket`
instances implement a duplex Stream interface.  They can be created by the
user and used as a client (with `connect()`) or they can be created by io.js
and passed to the user through the `'connection'` event of a server.

### new net.Socket([options])

Construct a new socket object.

`options` is an object with the following defaults:

    { fd: null
      allowHalfOpen: false,
      readable: false,
      writable: false
    }

`fd` allows you to specify the existing file descriptor of socket.
Set `readable` and/or `writable` to `true` to allow reads and/or writes on this
socket (NOTE: Works only when `fd` is passed).
About `allowHalfOpen`, refer to `createServer()` and `'end'` event.

### socket.connect(port[, host][, connectListener])
### socket.connect(path[, connectListener])

Opens the connection for a given socket. If `port` and `host` are given,
then the socket will be opened as a TCP socket, if `host` is omitted,
`localhost` will be assumed. If a `path` is given, the socket will be
opened as a unix socket to that path.

Normally this method is not needed, as `net.createConnection` opens the
socket. Use this only if you are implementing a custom Socket.

This function is asynchronous. When the ['connect'][] event is emitted the
socket is established. If there is a problem connecting, the `'connect'` event
will not be emitted, the `'error'` event will be emitted with the exception.

The `connectListener` parameter will be added as an listener for the
['connect'][] event.


### socket.bufferSize

`net.Socket` has the property that `socket.write()` always works. This is to
help users get up and running quickly. The computer cannot always keep up
with the amount of data that is written to a socket - the network connection
simply might be too slow. io.js will internally queue up the data written to a
socket and send it out over the wire when it is possible. (Internally it is
polling on the socket's file descriptor for being writable).

The consequence of this internal buffering is that memory may grow. This
property shows the number of characters currently buffered to be written.
(Number of characters is approximately equal to the number of bytes to be
written, but the buffer may contain strings, and the strings are lazily
encoded, so the exact number of bytes is not known.)

Users who experience large or growing `bufferSize` should attempt to
"throttle" the data flows in their program with `pause()` and `resume()`.


### socket.setEncoding([encoding])

Set the encoding for the socket as a Readable Stream. See
[stream.setEncoding()][] for more information.

### socket.write(data[, encoding][, callback])

Sends data on the socket. The second parameter specifies the encoding in the
case of a string--it defaults to UTF8 encoding.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is again free.

The optional `callback` parameter will be executed when the data is finally
written out - this may not be immediately.

### socket.end([data][, encoding])

Half-closes the socket. i.e., it sends a FIN packet. It is possible the
server will still send some data.

If `data` is specified, it is equivalent to calling
`socket.write(data, encoding)` followed by `socket.end()`.

### socket.destroy()

Ensures that no more I/O activity happens on this socket. Only necessary in
case of errors (parse error or so).

### socket.pause()

Pauses the reading of data. That is, `'data'` events will not be emitted.
Useful to throttle back an upload.

### socket.resume()

Resumes reading after a call to `pause()`.

### socket.setTimeout(timeout[, callback])

Sets the socket to timeout after `timeout` milliseconds of inactivity on
the socket. By default `net.Socket` do not have a timeout.

When an idle timeout is triggered the socket will receive a `'timeout'`
event but the connection will not be severed. The user must manually `end()`
or `destroy()` the socket.

If `timeout` is 0, then the existing idle timeout is disabled.

The optional `callback` parameter will be added as a one time listener for the
`'timeout'` event.

### socket.setNoDelay([noDelay])

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `true` for
`noDelay` will immediately fire off data each time `socket.write()` is called.
`noDelay` defaults to `true`.

### socket.setKeepAlive([enable][, initialDelay])

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle socket.
`enable` defaults to `false`.

Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting 0 for
initialDelay will leave the value unchanged from the default
(or previous) setting. Defaults to `0`.

### socket.address()

Returns the bound address, the address family name and port of the
socket as reported by the operating system. Returns an object with
three properties, e.g.
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

### socket.unref()

Calling `unref` on a socket will allow the program to exit if this is the only
active socket in the event system. If the socket is already `unref`d calling
`unref` again will have no effect.

### socket.ref()

Opposite of `unref`, calling `ref` on a previously `unref`d socket will *not*
let the program exit if it's the only socket left (the default behavior). If
the socket is `ref`d calling `ref` again will have no effect.

### socket.remoteAddress

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`.

### socket.remoteFamily

The string representation of the remote IP family. `'IPv4'` or `'IPv6'`.

### socket.remotePort

The numeric representation of the remote port. For example,
`80` or `21`.

### socket.localAddress

The string representation of the local IP address the remote client is
connecting on. For example, if you are listening on `'0.0.0.0'` and the
client connects on `'192.168.1.1'`, the value would be `'192.168.1.1'`.

### socket.localPort

The numeric representation of the local port. For example,
`80` or `21`.

### socket.bytesRead

The amount of received bytes.

### socket.bytesWritten

The amount of bytes sent.


`net.Socket` instances are [EventEmitter][] with the following events:

### Event: 'lookup'

Emitted after resolving the hostname but before connecting.
Not applicable to UNIX sockets.

* `err` {Error | Null} The error object.  See [dns.lookup()][].
* `address` {String} The IP address.
* `family` {String | Null} The address type.  See [dns.lookup()][].

### Event: 'connect'

Emitted when a socket connection is successfully established.
See `connect()`.

### Event: 'data'

* {Buffer object}

Emitted when data is received.  The argument `data` will be a `Buffer` or
`String`.  Encoding of data is set by `socket.setEncoding()`.
(See the [Readable Stream][] section for more information.)

Note that the __data will be lost__ if there is no listener when a `Socket`
emits a `'data'` event.

### Event: 'end'

Emitted when the other end of the socket sends a FIN packet.

By default (`allowHalfOpen == false`) the socket will destroy its file
descriptor  once it has written out its pending write queue.  However, by
setting `allowHalfOpen == true` the socket will not automatically `end()`
its side allowing the user to write arbitrary amounts of data, with the
caveat that the user is required to `end()` their side now.


### Event: 'timeout'

Emitted if the socket times out from inactivity. This is only to notify that
the socket has been idle. The user must manually close the connection.

See also: `socket.setTimeout()`


### Event: 'drain'

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

See also: the return values of `socket.write()`

### Event: 'error'

* {Error object}

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.

### Event: 'close'

* `had_error` {Boolean} true if the socket had a transmission error

Emitted once the socket is fully closed. The argument `had_error` is a boolean
which says if the socket was closed due to a transmission error.

## net.isIP(input)

Tests if input is an IP address. Returns 0 for invalid strings,
returns 4 for IP version 4 addresses, and returns 6 for IP version 6 addresses.


## net.isIPv4(input)

Returns true if input is a version 4 IP address, otherwise returns false.


## net.isIPv6(input)

Returns true if input is a version 6 IP address, otherwise returns false.

['connect']: #net_event_connect
['connection']: #net_event_connection
['end']: #net_event_end
[EventEmitter]: events.html#events_class_events_eventemitter
['listening']: #net_event_listening
[server.getConnections()]: #net_server_getconnections_callback
[Readable Stream]: stream.html#stream_class_stream_readable
[stream.setEncoding()]: stream.html#stream_stream_setencoding_encoding
[dns.lookup()]: dns.html#dns_dns_lookup_domain_family_callback
