# Net

> Stability: 2 - Stable

The `net` module provides an asynchronous network API for creating stream-based
servers ([`net.Server`][]) and clients ([`net.Socket`][]) that implement TCP
or local communications (domain sockets on UNIX, named pipes on Windows).
It can be accessed using:

```js
const net = require('net');
```

## Class: net.Server
<!-- YAML
added: v0.1.90
-->

This class is used to create a TCP or local server.

## new net.Server([options][, connectionListener])

See [`net.createServer([options][, connectionListener])`][`net.createServer()`].

`net.Server` is an [`EventEmitter`][] with the following events:

### Event: 'close'
<!-- YAML
added: v0.5.0
-->

Emitted when the server closes. Note that if connections exist, this
event is not emitted until all connections are ended.

### Event: 'connection'
<!-- YAML
added: v0.1.90
-->

* {net.Socket} The connection object

Emitted when a new connection is made. `socket` is an instance of
`net.Socket`.

### Event: 'error'
<!-- YAML
added: v0.1.90
-->

* {Error}

Emitted when an error occurs. Unlike [`net.Socket`][], the [`'close'`][]
event will **not** be emitted directly following this event unless
[`server.close()`][] is manually called. See the example in discussion of
[`server.listen()`][].

### Event: 'listening'
<!-- YAML
added: v0.1.90
-->

Emitted when the server has been bound after calling [`server.listen()`][].

### server.address()
<!-- YAML
added: v0.1.90
-->

Returns the bound address, the address family name, and port of the server
as reported by the operating system.
Useful to find which port was assigned when getting an OS-assigned address.
Returns an object with `port`, `family`, and `address` properties:
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

Example:

```js
var server = net.createServer((socket) => {
  socket.end('goodbye\n');
}).on('error', (err) => {
  // handle errors here
  throw err;
});

// grab an arbitrary unused port.
server.listen(() => {
  console.log('opened server on', server.address());
});
```

Don't call `server.address()` until the `'listening'` event has been emitted.

### server.close([callback])
<!-- YAML
added: v0.1.90
-->

Stops the server from accepting new connections and keeps existing
connections. This function is asynchronous, the server is finally
closed when all connections are ended and the server emits a [`'close'`][] event.
The optional `callback` will be called once the `'close'` event occurs. Unlike
that event, it will be called with an Error as its only argument if the server
was not open when it was closed.

### server.connections
<!-- YAML
added: v0.2.0
deprecated: v0.9.7
-->

> Stability: 0 - Deprecated: Use [`server.getConnections()`][] instead.

The number of concurrent connections on the server.

This becomes `null` when sending a socket to a child with
[`child_process.fork()`][]. To poll forks and get current number of active
connections use asynchronous `server.getConnections` instead.

### server.getConnections(callback)
<!-- YAML
added: v0.9.7
-->

Asynchronously get the number of concurrent connections on the server. Works
when sockets were sent to forks.

Callback should take two arguments `err` and `count`.

### server.listen()

Start a server listening for connections. A `net.Server` can be a TCP or
local (domain sockets on UNIX, named pipes on Windows) server depending on
what it listens to.

*Note*: Unix named pipes (FIFOs) are not supported.

Possible signatures:

* [`server.listen(handle[, backlog][, callback])`][`server.listen(handle)`]
* [`server.listen(options[, callback])`][`server.listen(options)`]
* [`server.listen(path[, backlog][, callback])`][`server.listen(path)`]
  for local servers
* [`server.listen([port][, host][, backlog][, callback])`][`server.listen(port, host)`]
  for TCP servers

This function is asynchronous.  When the server starts listening, the
[`'listening'`][] event will be emitted.  The last parameter `callback`
will be added as a listener for the [`'listening'`][] event.

All `listen()` methods can take a `backlog` parameter to specify the maximum
length of the queue of pending connections. The actual length will be determined
by the OS through sysctl settings such as `tcp_max_syn_backlog` and `somaxconn`
on Linux. The default value of this parameter is 511 (not 512).


Note:

* All [`net.Socket`][] are set to `SO_REUSEADDR`(See [socket(7)][] for details).
* The `server.listen()` method may be called multiple times. Each
  subsequent call will *re-open* the server using the provided options.

One of the most common errors raised when listening is `EADDRINUSE`.
This happens when another server is already listening on the requested
`port` / `path` / `handle`. One way to handle this would be to retry after a certain
amount of time:

```js
server.on('error', (e) => {
  if (e.code === 'EADDRINUSE') {
    console.log('Address in use, retrying...');
    setTimeout(() => {
      server.close();
      server.listen(PORT, HOST);
    }, 1000);
  }
});
```

#### server.listen(handle[, backlog][, callback])
<!-- YAML
added: v0.5.10
-->

* `handle` {Object}
* `backlog` {number} Common parameter of [`server.listen()`][] functions
* `callback` {Function} Common parameter of [`server.listen()`][] functions

Start a server listening for connections on a given `handle` that has
already been bound to a port, a UNIX domain socket, or a Windows named pipe.

The `handle` object can be either a server, a socket (anything with an
underlying `_handle` member), or an object with a `fd` member that is a valid file descriptor.

*Note*: Listening on a file descriptor is not supported on Windows.

#### server.listen(options[, callback])
<!-- YAML
added: v0.11.14
-->

* `options` {Object} Required. Supports the following properties:
  * `port` {number} Optional.
  * `host` {string} Optional.
  * `path` {string} Optional. Will be ignored if `port` is specified.
  * `backlog` {number} Optional. Common parameter of [`server.listen()`][]
    functions
  * `exclusive` {boolean} Optional. Default to `false`
* `callback` {Function} Optional. Common parameter of [`server.listen()`][]
  functions

If `port` is specified, it behaves the same as
[`server.listen([port][, hostname][, backlog][, callback])`][`server.listen(port, host)`].
Otherwise, if `path` is specified, it behaves the same as
[`server.listen(path[, backlog][, callback])`][`server.listen(path)`].
If none of them is specified, an error will be thrown.

If `exclusive` is `false` (default), then cluster workers will use the same
underlying handle, allowing connection handling duties to be shared. When
`exclusive` is `true`, the handle is not shared, and attempted port sharing
results in an error. An example which listens on an exclusive port is
shown below.

```js
server.listen({
  host: 'localhost',
  port: 80,
  exclusive: true
});
```

#### server.listen(path[, backlog][, callback])
<!-- YAML
added: v0.1.90
-->

* `path` {String}
* `backlog` {number} Common parameter of [`server.listen()`][] functions
* `callback` {Function} Common parameter of [`server.listen()`][] functions

Start a local socket server listening for connections on the given `path`.

On UNIX, the local domain is usually known as the UNIX domain. The path is a
filesystem path name. It gets truncated to `sizeof(sockaddr_un.sun_path)`
bytes, decreased by 1. It varies on different operating system between 91 and
107 bytes. The typical values are 107 on Linux and 103 on OS X. The path is
subject to the same naming conventions and permissions checks as would be done
on file creation, will be visible in the filesystem, and will *persist until
unlinked*.

On Windows, the local domain is implemented using a named pipe. The path *must*
refer to an entry in `\\?\pipe\` or `\\.\pipe\`. Any characters are permitted,
but the latter may do some processing of pipe names, such as resolving `..`
sequences. Despite appearances, the pipe name space is flat.  Pipes will *not
persist*, they are removed when the last reference to them is closed. Do not
forget JavaScript string escaping requires paths to be specified with
double-backslashes, such as:

```js
net.createServer().listen(
    path.join('\\\\?\\pipe', process.cwd(), 'myctl'))
```

#### server.listen([port][, host][, backlog][, callback])
<!-- YAML
added: v0.1.90
-->
* `port` {number}
* `host` {string}
* `backlog` {number} Common parameter of [`server.listen()`][] functions
* `callback` {Function} Common parameter of [`server.listen()`][] functions

Start a TCP server listening for connections on the given `port` and `host`.

If `port` is omitted or is 0, the operating system will assign an arbitrary
unused port, which can be retrieved by using `server.address().port`
after the [`'listening'`][] event has been emitted.

If `host` is omitted, the server will accept connections on the
[unspecified IPv6 address][] (`::`) when IPv6 is available, or the
[unspecified IPv4 address][] (`0.0.0.0`) otherwise.

*Note*: in most operating systems, listening to the
[unspecified IPv6 address][] (`::`) may cause the `net.Server` to also listen on
the [unspecified IPv4 address][] (`0.0.0.0`).

### server.listening
<!-- YAML
added: v5.7.0
-->

A Boolean indicating whether or not the server is listening for
connections.

### server.maxConnections
<!-- YAML
added: v0.2.0
-->

Set this property to reject connections when the server's connection count gets
high.

It is not recommended to use this option once a socket has been sent to a child
with [`child_process.fork()`][].

### server.ref()
<!-- YAML
added: v0.9.1
-->

Opposite of `unref`, calling `ref` on a previously `unref`d server will *not*
let the program exit if it's the only server left (the default behavior). If
the server is `ref`d calling `ref` again will have no effect.

Returns `server`.

### server.unref()
<!-- YAML
added: v0.9.1
-->

Calling `unref` on a server will allow the program to exit if this is the only
active server in the event system. If the server is already `unref`d calling
`unref` again will have no effect.

Returns `server`.

## Class: net.Socket
<!-- YAML
added: v0.3.4
-->

This object is an abstraction of a TCP or local socket.  `net.Socket`
instances implement a duplex Stream interface.  They can be created by the
user and used as a client (with [`connect()`][]) or they can be created by Node.js
and passed to the user through the `'connection'` event of a server.

### new net.Socket([options])
<!-- YAML
added: v0.3.4
-->

Construct a new socket object.

`options` is an object with the following defaults:

```js
{
  fd: null,
  allowHalfOpen: false,
  readable: false,
  writable: false
}
```

`fd` allows you to specify the existing file descriptor of socket.
Set `readable` and/or `writable` to `true` to allow reads and/or writes on this
socket (NOTE: Works only when `fd` is passed).
About `allowHalfOpen`, refer to [`net.createServer()`][] and [`'end'`][] event.

`net.Socket` instances are [`EventEmitter`][] with the following events:

### Event: 'close'
<!-- YAML
added: v0.1.90
-->

* `had_error` {boolean} `true` if the socket had a transmission error.

Emitted once the socket is fully closed. The argument `had_error` is a boolean
which says if the socket was closed due to a transmission error.

### Event: 'connect'
<!-- YAML
added: v0.1.90
-->

Emitted when a socket connection is successfully established.
See [`connect()`][].

### Event: 'data'
<!-- YAML
added: v0.1.90
-->

* {Buffer}

Emitted when data is received.  The argument `data` will be a `Buffer` or
`String`.  Encoding of data is set by `socket.setEncoding()`.
(See the [Readable Stream][] section for more information.)

Note that the **data will be lost** if there is no listener when a `Socket`
emits a `'data'` event.

### Event: 'drain'
<!-- YAML
added: v0.1.90
-->

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

See also: the return values of `socket.write()`

### Event: 'end'
<!-- YAML
added: v0.1.90
-->

Emitted when the other end of the socket sends a FIN packet.

By default (`allowHalfOpen` is `false`) the socket will send a FIN packet
back and destroy its file descriptor once it has written out its pending
write queue. However, if `allowHalfOpen` is set to `true`, the socket will
not automatically [`end()`][`socket.end()`] its side, allowing the user to
write arbitrary amounts of data. The user must call [`end()`][`socket.end()`]
explicitly to close the connection.

### Event: 'error'
<!-- YAML
added: v0.1.90
-->

* {Error}

Emitted when an error occurs.  The `'close'` event will be called directly
following this event.

### Event: 'lookup'
<!-- YAML
added: v0.11.3
changes:
  - version: v5.10.0
    pr-url: https://github.com/nodejs/node/pull/5598
    description: The `host` parameter is supported now.
-->

Emitted after resolving the hostname but before connecting.
Not applicable to UNIX sockets.

* `err` {Error|null} The error object.  See [`dns.lookup()`][].
* `address` {string} The IP address.
* `family` {string|null} The address type.  See [`dns.lookup()`][].
* `host` {string} The hostname.

### Event: 'timeout'
<!-- YAML
added: v0.1.90
-->

Emitted if the socket times out from inactivity. This is only to notify that
the socket has been idle. The user must manually close the connection.

See also: [`socket.setTimeout()`][]

### socket.address()
<!-- YAML
added: v0.1.90
-->

Returns the bound address, the address family name and port of the
socket as reported by the operating system. Returns an object with
three properties, e.g.
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`

### socket.bufferSize
<!-- YAML
added: v0.3.8
-->

`net.Socket` has the property that `socket.write()` always works. This is to
help users get up and running quickly. The computer cannot always keep up
with the amount of data that is written to a socket - the network connection
simply might be too slow. Node.js will internally queue up the data written to a
socket and send it out over the wire when it is possible. (Internally it is
polling on the socket's file descriptor for being writable).

The consequence of this internal buffering is that memory may grow. This
property shows the number of characters currently buffered to be written.
(Number of characters is approximately equal to the number of bytes to be
written, but the buffer may contain strings, and the strings are lazily
encoded, so the exact number of bytes is not known.)

Users who experience large or growing `bufferSize` should attempt to
"throttle" the data flows in their program with
[`socket.pause()`][] and [`socket.resume()`][].

### socket.bytesRead
<!-- YAML
added: v0.5.3
-->

The amount of received bytes.

### socket.bytesWritten
<!-- YAML
added: v0.5.3
-->

The amount of bytes sent.

### socket.connect(options[, connectListener])
<!-- YAML
added: v0.1.90
changes:
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6021
    description: The `hints` option defaults to `0` in all cases now.
                 Previously, in the absence of the `family` option it would
                 default to `dns.ADDRCONFIG | dns.V4MAPPED`.
  - version: v5.11.0
    pr-url: https://github.com/nodejs/node/pull/6000
    description: The `hints` option is supported now.
-->

Opens the connection for a given socket.

For TCP sockets, `options` argument should be an object which specifies:

  - `port`: Port the client should connect to (Required).

  - `host`: Host the client should connect to. Defaults to `'localhost'`.

  - `localAddress`: Local interface to bind to for network connections.

  - `localPort`: Local port to bind to for network connections.

  - `family` : Version of IP stack. Defaults to `4`.

  - `hints`: [`dns.lookup()` hints][]. Defaults to `0`.

  - `lookup` : Custom lookup function. Defaults to `dns.lookup`.

For local domain sockets, `options` argument should be an object which
specifies:

  - `path`: Path the client should connect to (Required).

Normally this method is not needed, as `net.createConnection` opens the
socket. Use this only if you are implementing a custom Socket.

This function is asynchronous. When the [`'connect'`][] event is emitted the
socket is established. If there is a problem connecting, the `'connect'` event
will not be emitted, the [`'error'`][] event will be emitted with the exception.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event.

### socket.connect(path[, connectListener])
### socket.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

As [`socket.connect(options[, connectListener])`][`socket.connect(options, connectListener)`],
with options as either `{port: port, host: host}` or `{path: path}`.

### socket.connecting
<!-- YAML
added: v6.1.0
-->

If `true` - [`socket.connect(options[, connectListener])`][`socket.connect(options, connectListener)`] was called and
haven't yet finished. Will be set to `false` before emitting `connect` event
and/or calling [`socket.connect(options[, connectListener])`][`socket.connect(options, connectListener)`]'s callback.

### socket.destroy([exception])
<!-- YAML
added: v0.1.90
-->

Ensures that no more I/O activity happens on this socket. Only necessary in
case of errors (parse error or so).

If `exception` is specified, an [`'error'`][] event will be emitted and any
listeners for that event will receive `exception` as an argument.

### socket.destroyed

A Boolean value that indicates if the connection is destroyed or not. Once a
connection is destroyed no further data can be transferred using it.

### socket.end([data][, encoding])
<!-- YAML
added: v0.1.90
-->

Half-closes the socket. i.e., it sends a FIN packet. It is possible the
server will still send some data.

If `data` is specified, it is equivalent to calling
`socket.write(data, encoding)` followed by [`socket.end()`][].

### socket.localAddress
<!-- YAML
added: v0.9.6
-->

The string representation of the local IP address the remote client is
connecting on. For example, if you are listening on `'0.0.0.0'` and the
client connects on `'192.168.1.1'`, the value would be `'192.168.1.1'`.

### socket.localPort
<!-- YAML
added: v0.9.6
-->

The numeric representation of the local port. For example,
`80` or `21`.

### socket.pause()

Pauses the reading of data. That is, [`'data'`][] events will not be emitted.
Useful to throttle back an upload.

### socket.ref()
<!-- YAML
added: v0.9.1
-->

Opposite of `unref`, calling `ref` on a previously `unref`d socket will *not*
let the program exit if it's the only socket left (the default behavior). If
the socket is `ref`d calling `ref` again will have no effect.

Returns `socket`.

### socket.remoteAddress
<!-- YAML
added: v0.5.10
-->

The string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`. Value may be `undefined` if
the socket is destroyed (for example, if the client disconnected).

### socket.remoteFamily
<!-- YAML
added: v0.11.14
-->

The string representation of the remote IP family. `'IPv4'` or `'IPv6'`.

### socket.remotePort
<!-- YAML
added: v0.5.10
-->

The numeric representation of the remote port. For example,
`80` or `21`.

### socket.resume()

Resumes reading after a call to [`socket.pause()`][].

### socket.setEncoding([encoding])
<!-- YAML
added: v0.1.90
-->

Set the encoding for the socket as a [Readable Stream][]. See
[`stream.setEncoding()`][] for more information.

### socket.setKeepAlive([enable][, initialDelay])
<!-- YAML
added: v0.1.92
-->

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle socket.
`enable` defaults to `false`.

Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting 0 for
initialDelay will leave the value unchanged from the default
(or previous) setting. Defaults to `0`.

Returns `socket`.

### socket.setNoDelay([noDelay])
<!-- YAML
added: v0.1.90
-->

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `true` for
`noDelay` will immediately fire off data each time `socket.write()` is called.
`noDelay` defaults to `true`.

Returns `socket`.

### socket.setTimeout(timeout[, callback])
<!-- YAML
added: v0.1.90
-->

Sets the socket to timeout after `timeout` milliseconds of inactivity on
the socket. By default `net.Socket` do not have a timeout.

When an idle timeout is triggered the socket will receive a [`'timeout'`][]
event but the connection will not be severed. The user must manually call
[`socket.end()`][] or [`socket.destroy()`][] to end the connection.

```js
socket.setTimeout(3000);
socket.on('timeout', () => {
  console.log('socket timeout');
  socket.end();
})
```

If `timeout` is 0, then the existing idle timeout is disabled.

The optional `callback` parameter will be added as a one time listener for the
[`'timeout'`][] event.

Returns `socket`.

### socket.unref()
<!-- YAML
added: v0.9.1
-->

Calling `unref` on a socket will allow the program to exit if this is the only
active socket in the event system. If the socket is already `unref`d calling
`unref` again will have no effect.

Returns `socket`.

### socket.write(data[, encoding][, callback])
<!-- YAML
added: v0.1.90
-->

Sends data on the socket. The second parameter specifies the encoding in the
case of a string--it defaults to UTF8 encoding.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
[`'drain'`][] will be emitted when the buffer is again free.

The optional `callback` parameter will be executed when the data is finally
written out - this may not be immediately.

## net.connect(options[, connectListener])
<!-- YAML
added: v0.7.0
-->

A factory function, which returns a new [`net.Socket`][] and automatically
connects with the supplied `options`.

The options are passed to both the [`net.Socket`][] constructor and the
[`socket.connect`][] method.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

Here is an example of a client of the previously described echo server:

```js
const net = require('net');
const client = net.connect({port: 8124}, () => {
  // 'connect' listener
  console.log('connected to server!');
  client.write('world!\r\n');
});
client.on('data', (data) => {
  console.log(data.toString());
  client.end();
});
client.on('end', () => {
  console.log('disconnected from server');
});
```

To connect on the socket `/tmp/echo.sock` the second line would just be
changed to

```js
const client = net.connect({path: '/tmp/echo.sock'});
```

## net.connect(path[, connectListener])
<!-- YAML
added: v0.1.90
-->

A factory function, which returns a new unix [`net.Socket`][] and automatically
connects to the supplied `path`.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

## net.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

A factory function, which returns a new [`net.Socket`][] and automatically
connects to the supplied `port` and `host`.

If `host` is omitted, `'localhost'` will be assumed.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

## net.createConnection(options[, connectListener])
<!-- YAML
added: v0.1.90
-->

A factory function, which returns a new [`net.Socket`][] and automatically
connects with the supplied `options`.

The options are passed to both the [`net.Socket`][] constructor and the
[`socket.connect`][] method.

Passing `timeout` as an option will call [`socket.setTimeout()`][] after the socket is created, but before it is connecting.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

Here is an example of a client of the previously described echo server:

```js
const net = require('net');
const client = net.createConnection({port: 8124}, () => {
  //'connect' listener
  console.log('connected to server!');
  client.write('world!\r\n');
});
client.on('data', (data) => {
  console.log(data.toString());
  client.end();
});
client.on('end', () => {
  console.log('disconnected from server');
});
```

To connect on the socket `/tmp/echo.sock` the second line would just be
changed to

```js
const client = net.connect({path: '/tmp/echo.sock'});
```

## net.createConnection(path[, connectListener])
<!-- YAML
added: v0.1.90
-->

A factory function, which returns a new unix [`net.Socket`][] and automatically
connects to the supplied `path`.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

## net.createConnection(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

A factory function, which returns a new [`net.Socket`][] and automatically
connects to the supplied `port` and `host`.

If `host` is omitted, `'localhost'` will be assumed.

The `connectListener` parameter will be added as a listener for the
[`'connect'`][] event once.

## net.createServer([options][, connectionListener])
<!-- YAML
added: v0.5.0
-->

Creates a new TCP or local server.

* `options` {Object}
  * `allowHalfOpen` {boolean} Default to `false`. Indicates whether half-opened
    TCP connections are allowed.
  * `pauseOnConnect` {boolean} Default to `false`. Indicates whether the socket
    should be paused on incoming connections.
* `connectionListener` {Function} Automatically set as a listener for the
  [`'connection'`][] event

If `allowHalfOpen` is set to `true`, when the other end of the socket
sends a FIN packet, the server will only send a FIN packet back when
[`socket.end()`][] is explicitly called, until then the connection is
half-closed (non-readable but still writable). See [`'end'`][] event
and [RFC 1122][half-closed] for more information.

If `pauseOnConnect` is set to `true`, then the socket associated with each
incoming connection will be paused, and no data will be read from its handle.
This allows connections to be passed between processes without any data being
read by the original process. To begin reading data from a paused socket, call
[`socket.resume()`][].

The server can be a TCP server or a local server, depending on what it
[`listen()`][`server.listen()`] to.

Here is an example of an TCP echo server which listens for connections
on port 8124:

```js
const net = require('net');
const server = net.createServer((c) => {
  // 'connection' listener
  console.log('client connected');
  c.on('end', () => {
    console.log('client disconnected');
  });
  c.write('hello\r\n');
  c.pipe(c);
});
server.on('error', (err) => {
  throw err;
});
server.listen(8124, () => {
  console.log('server bound');
});
```

Test this by using `telnet`:

```sh
telnet localhost 8124
```

To listen on the socket `/tmp/echo.sock` the third line from the last would
just be changed to

```js
server.listen('/tmp/echo.sock', () => {
  console.log('server bound');
});
```

Use `nc` to connect to a UNIX domain socket server:

```js
nc -U /tmp/echo.sock
```

## net.isIP(input)
<!-- YAML
added: v0.3.0
-->

Tests if input is an IP address. Returns 0 for invalid strings,
returns 4 for IP version 4 addresses, and returns 6 for IP version 6 addresses.


## net.isIPv4(input)
<!-- YAML
added: v0.3.0
-->

Returns true if input is a version 4 IP address, otherwise returns false.


## net.isIPv6(input)
<!-- YAML
added: v0.3.0
-->

Returns true if input is a version 6 IP address, otherwise returns false.

[`'close'`]: #net_event_close
[`'connect'`]: #net_event_connect
[`'connection'`]: #net_event_connection
[`'data'`]: #net_event_data
[`'drain'`]: #net_event_drain
[`'end'`]: #net_event_end
[`'error'`]: #net_event_error_1
[`'listening'`]: #net_event_listening
[`'timeout'`]: #net_event_timeout
[`child_process.fork()`]: child_process.html#child_process_child_process_fork_modulepath_args_options
[`connect()`]: #net_socket_connect_options_connectlistener
[`dns.lookup()`]: dns.html#dns_dns_lookup_hostname_options_callback
[`dns.lookup()` hints]: dns.html#dns_supported_getaddrinfo_flags
[`socket.end()`]: #net_socket_end_data_encoding
[`EventEmitter`]: events.html#events_class_eventemitter
[`net.createServer()`]: #net_net_createserver_options_connectionlistener
[`net.Server`]: #net_class_net_server
[`net.Socket`]: #net_class_net_socket
[`server.getConnections()`]: #net_server_getconnections_callback
[`server.listen()`]: #net_server_listen
[`server.listen(handle)`]: #net_server_listen_handle_backlog_callback
[`server.listen(options)`]: #net_server_listen_options_callback
[`server.listen(port, host)`]: #net_server_listen_port_host_backlog_callback
[`server.listen(path)`]: #net_server_listen_path_backlog_callback
[`server.close()`]: #net_server_close_callback
[`socket.connect(options, connectListener)`]: #net_socket_connect_options_connectlistener
[`socket.connect`]: #net_socket_connect_options_connectlistener
[`socket.destroy()`]: #net_socket_destroy_exception
[`socket.setTimeout()`]: #net_socket_settimeout_timeout_callback
[`socket.resume()`]: #net_socket_resume
[`socket.pause()`]: #net_socket_pause
[`stream.setEncoding()`]: stream.html#stream_readable_setencoding_encoding
[half-closed]: https://tools.ietf.org/html/rfc1122#section-4.2.2.13
[Readable Stream]: stream.html#stream_class_stream_readable
[socket(7)]: http://man7.org/linux/man-pages/man7/socket.7.html
[unspecified IPv6 address]: https://en.wikipedia.org/wiki/IPv6_address#Unspecified_address
[unspecified IPv4 address]: https://en.wikipedia.org/wiki/0.0.0.0
