# Net

<!--introduced_in=v0.10.0-->
<!--lint disable maximum-line-length-->

> Stability: 2 - Stable

The `net` module provides an asynchronous network API for creating stream-based
TCP or [IPC][] servers ([`net.createServer()`][]) and clients
([`net.createConnection()`][]).

It can be accessed using:

```js
const net = require('net');
```

## IPC Support

The `net` module supports IPC with named pipes on Windows, and UNIX domain
sockets on other operating systems.

### Identifying paths for IPC connections

[`net.connect()`][], [`net.createConnection()`][], [`server.listen()`][] and
[`socket.connect()`][] take a `path` parameter to identify IPC endpoints.

On UNIX, the local domain is also known as the UNIX domain. The path is a
filesystem pathname. It gets truncated to `sizeof(sockaddr_un.sun_path) - 1`,
which varies on different operating system between 91 and 107 bytes.
The typical values are 107 on Linux and 103 on macOS. The path is
subject to the same naming conventions and permissions checks as would be done
on file creation. If the UNIX domain socket (that is visible as a file system
path) is created and used in conjunction with one of Node.js' API abstractions
such as [`net.createServer()`][], it will be unlinked as part of
[`server.close()`][]. On the other hand, if it is created and used outside of
these abstractions, the user will need to manually remove it. The same applies
when the path was created by a Node.js API but the program crashes abruptly.
In short, a UNIX domain socket once successfully created will be visible in the
filesystem, and will persist until unlinked.

On Windows, the local domain is implemented using a named pipe. The path *must*
refer to an entry in `\\?\pipe\` or `\\.\pipe\`. Any characters are permitted,
but the latter may do some processing of pipe names, such as resolving `..`
sequences. Despite how it might look, the pipe namespace is flat. Pipes will
*not persist*. They are removed when the last reference to them is closed.
Unlike UNIX domain sockets, Windows will close and remove the pipe when the
owning process exits.

JavaScript string escaping requires paths to be specified with extra backslash
escaping such as:

```js
net.createServer().listen(
  path.join('\\\\?\\pipe', process.cwd(), 'myctl'));
```

## Class: net.Server
<!-- YAML
added: v0.1.90
-->

This class is used to create a TCP or [IPC][] server.

### new net.Server([options][, connectionListener])

* `options` {Object} See
  [`net.createServer([options][, connectionListener])`][`net.createServer()`].
* `connectionListener` {Function} Automatically set as a listener for the
  [`'connection'`][] event.
* Returns: {net.Server}

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

* Returns: {Object|string}

Returns the bound `address`, the address `family` name, and `port` of the server
as reported by the operating system if listening on an IP socket
(useful to find which port was assigned when getting an OS-assigned address):
`{ port: 12346, family: 'IPv4', address: '127.0.0.1' }`.

For a server listening on a pipe or UNIX domain socket, the name is returned
as a string.

```js
const server = net.createServer((socket) => {
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

* `callback` {Function} Called when the server is closed
* Returns: {net.Server}

Stops the server from accepting new connections and keeps existing
connections. This function is asynchronous, the server is finally closed
when all connections are ended and the server emits a [`'close'`][] event.
The optional `callback` will be called once the `'close'` event occurs. Unlike
that event, it will be called with an `Error` as its only argument if the server
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
connections, use asynchronous [`server.getConnections()`][] instead.

### server.getConnections(callback)
<!-- YAML
added: v0.9.7
-->

* `callback` {Function}
* Returns: {net.Server}

Asynchronously get the number of concurrent connections on the server. Works
when sockets were sent to forks.

Callback should take two arguments `err` and `count`.

### server.listen()

Start a server listening for connections. A `net.Server` can be a TCP or
an [IPC][] server depending on what it listens to.

Possible signatures:

* [`server.listen(handle[, backlog][, callback])`][`server.listen(handle)`]
* [`server.listen(options[, callback])`][`server.listen(options)`]
* [`server.listen(path[, backlog][, callback])`][`server.listen(path)`]
  for [IPC][] servers
* <a href="#net_server_listen_port_host_backlog_callback">
  <code>server.listen([port[, host[, backlog]]][, callback])</code></a>
  for TCP servers

This function is asynchronous. When the server starts listening, the
[`'listening'`][] event will be emitted. The last parameter `callback`
will be added as a listener for the [`'listening'`][] event.

All `listen()` methods can take a `backlog` parameter to specify the maximum
length of the queue of pending connections. The actual length will be determined
by the OS through sysctl settings such as `tcp_max_syn_backlog` and `somaxconn`
on Linux. The default value of this parameter is 511 (not 512).

All [`net.Socket`][] are set to `SO_REUSEADDR` (see [`socket(7)`][] for
details).

The `server.listen()` method can be called again if and only if there was an
error during the first `server.listen()` call or `server.close()` has been
called. Otherwise, an `ERR_SERVER_ALREADY_LISTEN` error will be thrown.

One of the most common errors raised when listening is `EADDRINUSE`.
This happens when another server is already listening on the requested
`port`/`path`/`handle`. One way to handle this would be to retry
after a certain amount of time:

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
* Returns: {net.Server}

Start a server listening for connections on a given `handle` that has
already been bound to a port, a UNIX domain socket, or a Windows named pipe.

The `handle` object can be either a server, a socket (anything with an
underlying `_handle` member), or an object with an `fd` member that is a
valid file descriptor.

Listening on a file descriptor is not supported on Windows.

#### server.listen(options[, callback])
<!-- YAML
added: v0.11.14
changes:
  - version: v11.4.0
    pr-url: https://github.com/nodejs/node/pull/23798
    description: The `ipv6Only` option is supported.
-->

* `options` {Object} Required. Supports the following properties:
  * `port` {number}
  * `host` {string}
  * `path` {string} Will be ignored if `port` is specified. See
    [Identifying paths for IPC connections][].
  * `backlog` {number} Common parameter of [`server.listen()`][]
    functions.
  * `exclusive` {boolean} **Default:** `false`
  * `readableAll` {boolean} For IPC servers makes the pipe readable
    for all users. **Default:** `false`
  * `writableAll` {boolean} For IPC servers makes the pipe writable
    for all users. **Default:** `false`
  * `ipv6Only` {boolean} For TCP servers, setting `ipv6Only` to `true` will
    disable dual-stack support, i.e., binding to host `::` won't make
    `0.0.0.0` be bound. **Default:** `false`.
* `callback` {Function} Common parameter of [`server.listen()`][]
  functions.
* Returns: {net.Server}

If `port` is specified, it behaves the same as
<a href="#net_server_listen_port_host_backlog_callback">
<code>server.listen([port[, host[, backlog]]][, callback])</code></a>.
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

Starting an IPC server as root may cause the server path to be inaccessible for
unprivileged users. Using `readableAll` and `writableAll` will make the server
accessible for all users.

#### server.listen(path[, backlog][, callback])
<!-- YAML
added: v0.1.90
-->

* `path` {string} Path the server should listen to. See
  [Identifying paths for IPC connections][].
* `backlog` {number} Common parameter of [`server.listen()`][] functions.
* `callback` {Function} Common parameter of [`server.listen()`][] functions.
* Returns: {net.Server}

Start an [IPC][] server listening for connections on the given `path`.

#### server.listen([port[, host[, backlog]]][, callback])
<!-- YAML
added: v0.1.90
-->
* `port` {number}
* `host` {string}
* `backlog` {number} Common parameter of [`server.listen()`][] functions.
* `callback` {Function} Common parameter of [`server.listen()`][] functions.
* Returns: {net.Server}

Start a TCP server listening for connections on the given `port` and `host`.

If `port` is omitted or is 0, the operating system will assign an arbitrary
unused port, which can be retrieved by using `server.address().port`
after the [`'listening'`][] event has been emitted.

If `host` is omitted, the server will accept connections on the
[unspecified IPv6 address][] (`::`) when IPv6 is available, or the
[unspecified IPv4 address][] (`0.0.0.0`) otherwise.

In most operating systems, listening to the [unspecified IPv6 address][] (`::`)
may cause the `net.Server` to also listen on the [unspecified IPv4 address][]
(`0.0.0.0`).

### server.listening
<!-- YAML
added: v5.7.0
-->

* {boolean} Indicates whether or not the server is listening for connections.

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

* Returns: {net.Server}

Opposite of `unref()`, calling `ref()` on a previously `unref`ed server will
*not* let the program exit if it's the only server left (the default behavior).
If the server is `ref`ed calling `ref()` again will have no effect.

### server.unref()
<!-- YAML
added: v0.9.1
-->

* Returns: {net.Server}

Calling `unref()` on a server will allow the program to exit if this is the only
active server in the event system. If the server is already `unref`ed calling
`unref()` again will have no effect.

## Class: net.Socket
<!-- YAML
added: v0.3.4
-->

This class is an abstraction of a TCP socket or a streaming [IPC][] endpoint
(uses named pipes on Windows, and UNIX domain sockets otherwise). A
`net.Socket` is also a [duplex stream][], so it can be both readable and
writable, and it is also an [`EventEmitter`][].

A `net.Socket` can be created by the user and used directly to interact with
a server. For example, it is returned by [`net.createConnection()`][],
so the user can use it to talk to the server.

It can also be created by Node.js and passed to the user when a connection
is received. For example, it is passed to the listeners of a
[`'connection'`][] event emitted on a [`net.Server`][], so the user can use
it to interact with the client.

### new net.Socket([options])
<!-- YAML
added: v0.3.4
-->

* `options` {Object} Available options are:
  * `fd` {number} If specified, wrap around an existing socket with
    the given file descriptor, otherwise a new socket will be created.
  * `allowHalfOpen` {boolean} Indicates whether half-opened TCP connections
    are allowed. See [`net.createServer()`][] and the [`'end'`][] event
    for details. **Default:** `false`.
  * `readable` {boolean} Allow reads on the socket when an `fd` is passed,
    otherwise ignored. **Default:** `false`.
  * `writable` {boolean} Allow writes on the socket when an `fd` is passed,
    otherwise ignored. **Default:** `false`.
* Returns: {net.Socket}

Creates a new socket object.

The newly created socket can be either a TCP socket or a streaming [IPC][]
endpoint, depending on what it [`connect()`][`socket.connect()`] to.

### Event: 'close'
<!-- YAML
added: v0.1.90
-->

* `hadError` {boolean} `true` if the socket had a transmission error.

Emitted once the socket is fully closed. The argument `hadError` is a boolean
which says if the socket was closed due to a transmission error.

### Event: 'connect'
<!-- YAML
added: v0.1.90
-->

Emitted when a socket connection is successfully established.
See [`net.createConnection()`][].

### Event: 'data'
<!-- YAML
added: v0.1.90
-->

* {Buffer|string}

Emitted when data is received. The argument `data` will be a `Buffer` or
`String`. Encoding of data is set by [`socket.setEncoding()`][].

Note that the **data will be lost** if there is no listener when a `Socket`
emits a `'data'` event.

### Event: 'drain'
<!-- YAML
added: v0.1.90
-->

Emitted when the write buffer becomes empty. Can be used to throttle uploads.

See also: the return values of `socket.write()`.

### Event: 'end'
<!-- YAML
added: v0.1.90
-->

Emitted when the other end of the socket sends a FIN packet, thus ending the
readable side of the socket.

By default (`allowHalfOpen` is `false`) the socket will send a FIN packet
back and destroy its file descriptor once it has written out its pending
write queue. However, if `allowHalfOpen` is set to `true`, the socket will
not automatically [`end()`][`socket.end()`] its writable side, allowing the
user to write arbitrary amounts of data. The user must call
[`end()`][`socket.end()`] explicitly to close the connection (i.e. sending a
FIN packet back).

### Event: 'error'
<!-- YAML
added: v0.1.90
-->

* {Error}

Emitted when an error occurs. The `'close'` event will be called directly
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

* `err` {Error|null} The error object. See [`dns.lookup()`][].
* `address` {string} The IP address.
* `family` {string|null} The address type. See [`dns.lookup()`][].
* `host` {string} The hostname.

### Event: 'ready'
<!-- YAML
added: v9.11.0
-->

Emitted when a socket is ready to be used.

Triggered immediately after `'connect'`.

### Event: 'timeout'
<!-- YAML
added: v0.1.90
-->

Emitted if the socket times out from inactivity. This is only to notify that
the socket has been idle. The user must manually close the connection.

See also: [`socket.setTimeout()`][].

### socket.address()
<!-- YAML
added: v0.1.90
-->

* Returns: {Object}

Returns the bound `address`, the address `family` name and `port` of the
socket as reported by the operating system:
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

### socket.connect()

Initiate a connection on a given socket.

Possible signatures:

* [`socket.connect(options[, connectListener])`][`socket.connect(options)`]
* [`socket.connect(path[, connectListener])`][`socket.connect(path)`]
  for [IPC][] connections.
* [`socket.connect(port[, host][, connectListener])`][`socket.connect(port, host)`]
  for TCP connections.
* Returns: {net.Socket} The socket itself.

This function is asynchronous. When the connection is established, the
[`'connect'`][] event will be emitted. If there is a problem connecting,
instead of a [`'connect'`][] event, an [`'error'`][] event will be emitted with
the error passed to the [`'error'`][] listener.
The last parameter `connectListener`, if supplied, will be added as a listener
for the [`'connect'`][] event **once**.

#### socket.connect(options[, connectListener])
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

* `options` {Object}
* `connectListener` {Function} Common parameter of [`socket.connect()`][]
  methods. Will be added as a listener for the [`'connect'`][] event once.
* Returns: {net.Socket} The socket itself.

Initiate a connection on a given socket. Normally this method is not needed,
the socket should be created and opened with [`net.createConnection()`][]. Use
this only when implementing a custom Socket.

For TCP connections, available `options` are:

* `port` {number} Required. Port the socket should connect to.
* `host` {string} Host the socket should connect to. **Default:** `'localhost'`.
* `localAddress` {string} Local address the socket should connect from.
* `localPort` {number} Local port the socket should connect from.
* `family` {number}: Version of IP stack, can be either `4` or `6`.
  **Default:** `4`.
* `hints` {number} Optional [`dns.lookup()` hints][].
* `lookup` {Function} Custom lookup function. **Default:** [`dns.lookup()`][].

For [IPC][] connections, available `options` are:

* `path` {string} Required. Path the client should connect to.
  See [Identifying paths for IPC connections][]. If provided, the TCP-specific
  options above are ignored.

#### socket.connect(path[, connectListener])

* `path` {string} Path the client should connect to. See
  [Identifying paths for IPC connections][].
* `connectListener` {Function} Common parameter of [`socket.connect()`][]
  methods. Will be added as a listener for the [`'connect'`][] event once.
* Returns: {net.Socket} The socket itself.

Initiate an [IPC][] connection on the given socket.

Alias to
[`socket.connect(options[, connectListener])`][`socket.connect(options)`]
called with `{ path: path }` as `options`.

#### socket.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

* `port` {number} Port the client should connect to.
* `host` {string} Host the client should connect to.
* `connectListener` {Function} Common parameter of [`socket.connect()`][]
  methods. Will be added as a listener for the [`'connect'`][] event once.
* Returns: {net.Socket} The socket itself.

Initiate a TCP connection on the given socket.

Alias to
[`socket.connect(options[, connectListener])`][`socket.connect(options)`]
called with `{port: port, host: host}` as `options`.

### socket.connecting
<!-- YAML
added: v6.1.0
-->

If `true`,
[`socket.connect(options[, connectListener])`][`socket.connect(options)`] was
called and has not yet finished. It will stay `true` until the socket becomes
connected, then it is set to `false` and the `'connect'` event is emitted.  Note
that the
[`socket.connect(options[, connectListener])`][`socket.connect(options)`]
callback is a listener for the `'connect'` event.

### socket.destroy([exception])
<!-- YAML
added: v0.1.90
-->

* `exception` {Object}
* Returns: {net.Socket}

Ensures that no more I/O activity happens on this socket. Only necessary in
case of errors (parse error or so).

If `exception` is specified, an [`'error'`][] event will be emitted and any
listeners for that event will receive `exception` as an argument.

### socket.destroyed

* {boolean} Indicates if the connection is destroyed or not. Once a
  connection is destroyed no further data can be transferred using it.

### socket.end([data][, encoding][, callback])
<!-- YAML
added: v0.1.90
-->

* `data` {string|Buffer|Uint8Array}
* `encoding` {string} Only used when data is `string`. **Default:** `'utf8'`.
* `callback` {Function} Optional callback for when the socket is finished.
* Returns: {net.Socket} The socket itself.

Half-closes the socket. i.e., it sends a FIN packet. It is possible the
server will still send some data.

If `data` is specified, it is equivalent to calling
`socket.write(data, encoding)` followed by [`socket.end()`][].

### socket.localAddress
<!-- YAML
added: v0.9.6
-->

The string representation of the local IP address the remote client is
connecting on. For example, in a server listening on `'0.0.0.0'`, if a client
connects on `'192.168.1.1'`, the value of `socket.localAddress` would be
`'192.168.1.1'`.

### socket.localPort
<!-- YAML
added: v0.9.6
-->

The numeric representation of the local port. For example, `80` or `21`.

### socket.pause()

* Returns: {net.Socket} The socket itself.

Pauses the reading of data. That is, [`'data'`][] events will not be emitted.
Useful to throttle back an upload.

### socket.pending
<!-- YAML
added: v11.2.0
-->

* {boolean}

This is `true` if the socket is not connected yet, either because `.connect()`
has not yet been called or because it is still in the process of connecting
(see [`socket.connecting`][]).

### socket.ref()
<!-- YAML
added: v0.9.1
-->

* Returns: {net.Socket} The socket itself.

Opposite of `unref()`, calling `ref()` on a previously `unref`ed socket will
*not* let the program exit if it's the only socket left (the default behavior).
If the socket is `ref`ed calling `ref` again will have no effect.

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

The numeric representation of the remote port. For example, `80` or `21`.

### socket.resume()

* Returns: {net.Socket} The socket itself.

Resumes reading after a call to [`socket.pause()`][].

### socket.setEncoding([encoding])
<!-- YAML
added: v0.1.90
-->

* `encoding` {string}
* Returns: {net.Socket} The socket itself.

Set the encoding for the socket as a [Readable Stream][]. See
[`readable.setEncoding()`][] for more information.

### socket.setKeepAlive([enable][, initialDelay])
<!-- YAML
added: v0.1.92
-->

* `enable` {boolean} **Default:** `false`
* `initialDelay` {number} **Default:** `0`
* Returns: {net.Socket} The socket itself.

Enable/disable keep-alive functionality, and optionally set the initial
delay before the first keepalive probe is sent on an idle socket.

Set `initialDelay` (in milliseconds) to set the delay between the last
data packet received and the first keepalive probe. Setting `0` for
`initialDelay` will leave the value unchanged from the default
(or previous) setting.

### socket.setNoDelay([noDelay])
<!-- YAML
added: v0.1.90
-->

* `noDelay` {boolean} **Default:** `true`
* Returns: {net.Socket} The socket itself.

Disables the Nagle algorithm. By default TCP connections use the Nagle
algorithm, they buffer data before sending it off. Setting `true` for
`noDelay` will immediately fire off data each time `socket.write()` is called.

### socket.setTimeout(timeout[, callback])
<!-- YAML
added: v0.1.90
-->

* `timeout` {number}
* `callback` {Function}
* Returns: {net.Socket} The socket itself.

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
});
```

If `timeout` is 0, then the existing idle timeout is disabled.

The optional `callback` parameter will be added as a one-time listener for the
[`'timeout'`][] event.

### socket.unref()
<!-- YAML
added: v0.9.1
-->

* Returns: {net.Socket} The socket itself.

Calling `unref()` on a socket will allow the program to exit if this is the only
active socket in the event system. If the socket is already `unref`ed calling
`unref()` again will have no effect.

### socket.write(data[, encoding][, callback])
<!-- YAML
added: v0.1.90
-->

* `data` {string|Buffer|Uint8Array}
* `encoding` {string} Only used when data is `string`. **Default:** `utf8`.
* `callback` {Function}
* Returns: {boolean}

Sends data on the socket. The second parameter specifies the encoding in the
case of a string — it defaults to UTF8 encoding.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
[`'drain'`][] will be emitted when the buffer is again free.

The optional `callback` parameter will be executed when the data is finally
written out - this may not be immediately.

See `Writable` stream [`write()`][stream_writable_write] method for more
information.

## net.connect()

Aliases to
[`net.createConnection()`][`net.createConnection()`].

Possible signatures:

* [`net.connect(options[, connectListener])`][`net.connect(options)`]
* [`net.connect(path[, connectListener])`][`net.connect(path)`] for [IPC][]
  connections.
* [`net.connect(port[, host][, connectListener])`][`net.connect(port, host)`]
  for TCP connections.

### net.connect(options[, connectListener])
<!-- YAML
added: v0.7.0
-->
* `options` {Object}
* `connectListener` {Function}

Alias to
[`net.createConnection(options[, connectListener])`][`net.createConnection(options)`].

### net.connect(path[, connectListener])
<!-- YAML
added: v0.1.90
-->
* `path` {string}
* `connectListener` {Function}

Alias to
[`net.createConnection(path[, connectListener])`][`net.createConnection(path)`].

### net.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->
* `port` {number}
* `host` {string}
* `connectListener` {Function}

Alias to
[`net.createConnection(port[, host][, connectListener])`][`net.createConnection(port, host)`].

## net.createConnection()

A factory function, which creates a new [`net.Socket`][],
immediately initiates connection with [`socket.connect()`][],
then returns the `net.Socket` that starts the connection.

When the connection is established, a [`'connect'`][] event will be emitted
on the returned socket. The last parameter `connectListener`, if supplied,
will be added as a listener for the [`'connect'`][] event **once**.

Possible signatures:

* [`net.createConnection(options[, connectListener])`][`net.createConnection(options)`]
* [`net.createConnection(path[, connectListener])`][`net.createConnection(path)`]
  for [IPC][] connections.
* [`net.createConnection(port[, host][, connectListener])`][`net.createConnection(port, host)`]
  for TCP connections.

The [`net.connect()`][] function is an alias to this function.

### net.createConnection(options[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `options` {Object} Required. Will be passed to both the
  [`new net.Socket([options])`][`new net.Socket(options)`] call and the
  [`socket.connect(options[, connectListener])`][`socket.connect(options)`]
  method.
* `connectListener` {Function} Common parameter of the
  [`net.createConnection()`][] functions. If supplied, will be added as
  a listener for the [`'connect'`][] event on the returned socket once.
* Returns: {net.Socket} The newly created socket used to start the connection.

For available options, see
[`new net.Socket([options])`][`new net.Socket(options)`]
and [`socket.connect(options[, connectListener])`][`socket.connect(options)`].

Additional options:

* `timeout` {number} If set, will be used to call
  [`socket.setTimeout(timeout)`][] after the socket is created, but before
  it starts the connection.

Following is an example of a client of the echo server described
in the [`net.createServer()`][] section:

```js
const net = require('net');
const client = net.createConnection({ port: 8124 }, () => {
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
changed to:

```js
const client = net.createConnection({ path: '/tmp/echo.sock' });
```

### net.createConnection(path[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `path` {string} Path the socket should connect to. Will be passed to
  [`socket.connect(path[, connectListener])`][`socket.connect(path)`].
  See [Identifying paths for IPC connections][].
* `connectListener` {Function} Common parameter of the
  [`net.createConnection()`][] functions, an "once" listener for the
  `'connect'` event on the initiating socket. Will be passed to
  [`socket.connect(path[, connectListener])`][`socket.connect(path)`].
* Returns: {net.Socket} The newly created socket used to start the connection.

Initiates an [IPC][] connection.

This function creates a new [`net.Socket`][] with all options set to default,
immediately initiates connection with
[`socket.connect(path[, connectListener])`][`socket.connect(path)`],
then returns the `net.Socket` that starts the connection.

### net.createConnection(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

* `port` {number} Port the socket should connect to. Will be passed to
  [`socket.connect(port[, host][, connectListener])`][`socket.connect(port, host)`].
* `host` {string} Host the socket should connect to. Will be passed to
  [`socket.connect(port[, host][, connectListener])`][`socket.connect(port, host)`].
   **Default:** `'localhost'`.
* `connectListener` {Function} Common parameter of the
  [`net.createConnection()`][] functions, an "once" listener for the
  `'connect'` event on the initiating socket. Will be passed to
  [`socket.connect(path[, connectListener])`][`socket.connect(port, host)`].
* Returns: {net.Socket} The newly created socket used to start the connection.

Initiates a TCP connection.

This function creates a new [`net.Socket`][] with all options set to default,
immediately initiates connection with
[`socket.connect(port[, host][, connectListener])`][`socket.connect(port, host)`],
then returns the `net.Socket` that starts the connection.

## net.createServer([options][, connectionListener])
<!-- YAML
added: v0.5.0
-->

* `options` {Object}
  * `allowHalfOpen` {boolean} Indicates whether half-opened TCP
    connections are allowed. **Default:** `false`.
  * `pauseOnConnect` {boolean} Indicates whether the socket should be
    paused on incoming connections. **Default:** `false`.
* `connectionListener` {Function} Automatically set as a listener for the
  [`'connection'`][] event.
* Returns: {net.Server}

Creates a new TCP or [IPC][] server.

If `allowHalfOpen` is set to `true`, when the other end of the socket
sends a FIN packet, the server will only send a FIN packet back when
[`socket.end()`][] is explicitly called, until then the connection is
half-closed (non-readable but still writable). See [`'end'`][] event
and [RFC 1122][half-closed] (section 4.2.2.13) for more information.

If `pauseOnConnect` is set to `true`, then the socket associated with each
incoming connection will be paused, and no data will be read from its handle.
This allows connections to be passed between processes without any data being
read by the original process. To begin reading data from a paused socket, call
[`socket.resume()`][].

The server can be a TCP server or an [IPC][] server, depending on what it
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

```console
$ telnet localhost 8124
```

To listen on the socket `/tmp/echo.sock` the third line from the last would
just be changed to:

```js
server.listen('/tmp/echo.sock', () => {
  console.log('server bound');
});
```

Use `nc` to connect to a UNIX domain socket server:

```console
$ nc -U /tmp/echo.sock
```

## net.isIP(input)
<!-- YAML
added: v0.3.0
-->

* `input` {string}
* Returns: {integer}

Tests if input is an IP address. Returns `0` for invalid strings,
returns `4` for IP version 4 addresses, and returns `6` for IP version 6
addresses.

## net.isIPv4(input)
<!-- YAML
added: v0.3.0
-->

* `input` {string}
* Returns: {boolean}

Returns `true` if input is a version 4 IP address, otherwise returns `false`.

## net.isIPv6(input)
<!-- YAML
added: v0.3.0
-->

* `input` {string}
* Returns: {boolean}

Returns `true` if input is a version 6 IP address, otherwise returns `false`.

[`'close'`]: #net_event_close
[`'connect'`]: #net_event_connect
[`'connection'`]: #net_event_connection
[`'data'`]: #net_event_data
[`'drain'`]: #net_event_drain
[`'end'`]: #net_event_end
[`'error'`]: #net_event_error_1
[`'listening'`]: #net_event_listening
[`'timeout'`]: #net_event_timeout
[`EventEmitter`]: events.html#events_class_eventemitter
[`child_process.fork()`]: child_process.html#child_process_child_process_fork_modulepath_args_options
[`dns.lookup()` hints]: dns.html#dns_supported_getaddrinfo_flags
[`dns.lookup()`]: dns.html#dns_dns_lookup_hostname_options_callback
[`net.Server`]: #net_class_net_server
[`net.Socket`]: #net_class_net_socket
[`net.connect()`]: #net_net_connect
[`net.connect(options)`]: #net_net_connect_options_connectlistener
[`net.connect(path)`]: #net_net_connect_path_connectlistener
[`net.connect(port, host)`]: #net_net_connect_port_host_connectlistener
[`net.createConnection()`]: #net_net_createconnection
[`net.createConnection(options)`]: #net_net_createconnection_options_connectlistener
[`net.createConnection(path)`]: #net_net_createconnection_path_connectlistener
[`net.createConnection(port, host)`]: #net_net_createconnection_port_host_connectlistener
[`net.createServer()`]: #net_net_createserver_options_connectionlistener
[`new net.Socket(options)`]: #net_new_net_socket_options
[`readable.setEncoding()`]: stream.html#stream_readable_setencoding_encoding
[`server.close()`]: #net_server_close_callback
[`server.getConnections()`]: #net_server_getconnections_callback
[`server.listen()`]: #net_server_listen
[`server.listen(handle)`]: #net_server_listen_handle_backlog_callback
[`server.listen(options)`]: #net_server_listen_options_callback
[`server.listen(path)`]: #net_server_listen_path_backlog_callback
[`socket(7)`]: http://man7.org/linux/man-pages/man7/socket.7.html
[`socket.connect()`]: #net_socket_connect
[`socket.connect(options)`]: #net_socket_connect_options_connectlistener
[`socket.connect(path)`]: #net_socket_connect_path_connectlistener
[`socket.connect(port, host)`]: #net_socket_connect_port_host_connectlistener
[`socket.connecting`]: #net_socket_connecting
[`socket.destroy()`]: #net_socket_destroy_exception
[`socket.end()`]: #net_socket_end_data_encoding_callback
[`socket.pause()`]: #net_socket_pause
[`socket.resume()`]: #net_socket_resume
[`socket.setEncoding()`]: #net_socket_setencoding_encoding
[`socket.setTimeout()`]: #net_socket_settimeout_timeout_callback
[`socket.setTimeout(timeout)`]: #net_socket_settimeout_timeout_callback
[IPC]: #net_ipc_support
[Identifying paths for IPC connections]: #net_identifying_paths_for_ipc_connections
[Readable Stream]: stream.html#stream_class_stream_readable
[duplex stream]: stream.html#stream_class_stream_duplex
[half-closed]: https://tools.ietf.org/html/rfc1122
[stream_writable_write]: stream.html#stream_writable_write_chunk_encoding_callback
[unspecified IPv4 address]: https://en.wikipedia.org/wiki/0.0.0.0
[unspecified IPv6 address]: https://en.wikipedia.org/wiki/IPv6_address#Unspecified_address
