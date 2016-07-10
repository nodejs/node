# net

    Stability: 2 - Stable

The `net` module provides an API for implementing network clients and servers.
It can be accessed using:

```js
const net = require('net');
```

## Class: net.Server
<!-- YAML
added: v0.1.90
-->

The `net.Server` class is used to implement TCP or local servers.

All `net.Server` objects are [`EventEmitter`][] instances.

### Event: 'close'
<!-- YAML
added: v0.5.0
-->

The `'close'` event is emitted when the server closes. If active connections
exist, this event is not emitted until all such connections are ended.

The listener callback is called without passing any arguments.

```js
server.on('close', () => {
  console.log('The server has been closed.');
});
```

### Event: 'connection'
<!-- YAML
added: v0.1.90
-->

The `'connection'` event is emitted when a new connection is established.

The listener callback is called with the `net.Socket` object for the connection
passed as the only argument.

```js
server.on('connection', (socket) => {
  console.log('A connection has been established');
});
```

### Event: 'error'
<!-- YAML
added: v0.1.90
-->

The `'error'` event is emitted when an error occurs. The [`'close'`][] event
will be emitted immediately following the `'error'` event.

The listener callback is called with the `Error` object passed as the only
argument.

```js
server.on('error', (error) => {
  console.log('An error occurred!', error);
});
```

### Event: 'listening'
<!-- YAML
added: v0.1.90
-->

The `'listening'` event is emitted when the server has been bound by calling the
`server.listen()` method.

The listener callback is called without passing any arguments:

```js
server.on('listening', () => {
  console.log('The server is now listening for connections!');
});
```

### server.address()
<!-- YAML
added: v0.1.90
-->

The `server.address()` method returns the address to which the server is
currently bound.

If the server is bound to a network host and port, an object will be returned
with the following properties:

* `port` {number}
* `family` {String} Will be either `'IPv4'` or `'IPv6'`
* `address` {String} The IPv4 or IPv6 network address

For example:

```js
{
  port: 12346,
  family: 'IPv4',
  address: '127.0.0.1'
}
```

If the server is bound to a path, that path will be returned as a string. For
instance:

```js
const net = require('net');

const myServer = net.createServer();

myServer.listen('/tmp/path', () => {
  console.log(myServer.address());
    // Prints: /tmp/path
});
```

The `server.address()` method should not be called until after the `listening`
event has been emitted, as illustrated in the following example:

```js
const server = net.createServer((socket) => {
  socket.end('goodbye\n');
}).on('error', (err) => throw err);

// grab a random port.
server.listen(() => {
  address = server.address();
  console.log('opened server on %j', address);
});
```

### server.close([callback])
<!-- YAML
added: v0.1.90
-->

* `callback` {Function}

The `server.close()` method stops the server from accepting new connections
while maintaining existing connections that have not yet completed. Once the
existing connections have closed, the server will emit the [`'close'`][]
event and call the optional `callback` function (if given) with `null` passed
as the only argument.

If `server.close()` is called while the server *is not currently listening for
new connections*, the `callback` is called with an `Error` passed as the only
argument.

```js
server.close((err) => {
  if (err) throw err;
  console.log('The server is now closed!');
});
```

### server.connections
<!-- YAML
added: v0.2.0
deprecated: v0.9.7
-->

    Stability: 0 - Deprecated: Use [`server.getConnections()`][] instead.

A property indicating the number of concurrent connections on the server.

The value is `null` when a socket is sent to a child using
[`child_process.fork()`][]. To poll forks and get current number of active
connections use the `server.getConnections()` method instead.

### server.getConnections(callback)
<!-- YAML
added: v0.9.7
-->

* `callback` {Function}

The `server.getConnections()` method asynchronously returns the number of
concurrent connections on the server, including those held by child processes
opened using [`child_process.fork()`][].

The `callback` function is invoked with two arguments:

* `err` {Error} `null` if the operation was successful or `Error` if an
  error occurred.
* `count` {number} The number of connections.

```js
server.getConnections((err, count) => {
  if (err) throw err;
  console.log(`There are ${count} active connections for this server.`);
});
```

### server.listen(handle[, backlog][, callback])
<!-- YAML
added: v0.5.10
-->

* `handle` {Object}
* `backlog` {Number} Defaults to `511`.
* `callback` {Function}

The `server.listen()` method instructs the server to begin accepting connections
on the given `handle`. The `handle` object may either be any object with an
underlying `_handle` member (such as a `net.Socket` or another `net.Server`),
or an object with a `fd` property whose value is a numeric file descriptor
(e.g. `{fd: <n>}`).

It is presumed that the given `handle` has already been bound to a port or
domain socket.

Passing a `handle` with a `fd` (file descriptor) is not supported on Windows.

The `server.listen()` method is asynchronous. When the server has been bound,
the [`'listening'`][] event will be emitted.

If a `callback` function is given, it will be added as a listener for the
[`'listening'`][] event.

The `backlog` argument is the maximum size of the queue of pending connections.
The maximum possible value will be determined by the operating system through
`sysctl` settings such as `tcp_max_syn_backlog` and `somaxconn` on Linux.

### server.listen(options[, callback])
<!-- YAML
added: v0.11.14
-->

* `options` {Object} - Required. Supports the following properties:
  * `port` {Number} - Optional.
  * `host` {String} - Optional.
  * `path` {String} - Optional.
  * `backlog` {Number} - Optional. Defaults to `511`.
  * `exclusive` {Boolean} - Optional. Defaults to `false`.
* `callback` {Function} - Optional.

The `server.listen()` method instructs the server to begin accepting connections
on either the given `host` and `port` or UNIX socket `path`.

If the `host` is omitted, the server will accept connections on any IPv6 address
(`::`) when IPv6 is available, or any IPv4 address (`0.0.0.0`) otherwise. A
`port` value of zero will assign a random port. Alternatively, the `path` option
can be used to specify a UNIX socket.

The `backlog` option is the maximum size of the queue of pending connections.
The maximum possible value will be determined by the operating system through
`sysctl` settings such as `tcp_max_syn_backlog` and `somaxconn` on Linux.

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

The `server.listen()` method is asynchronous. When the server has been bound,
the [`'listening'`][] event will be emitted.

If a `callback` function is given, it will be added as a listener for the
[`'listening'`][] event.

### server.listen(path[, backlog][, callback])
<!-- YAML
added: v0.1.90
-->

* `path` {String}
* `backlog` {Number}
* `callback` {Function}

The `server.listen()` method instructs the server to begin accepting connections
on the given socket `path`.

The `server.listen()` method is asynchronous. When the server has been bound,
the [`'listening'`][] event will be emitted.

If a `callback` function is given, it will be added as a listener for the
[`'listening'`][] event.

On UNIX, the local domain is usually known as the UNIX domain. The `path` is a
filesystem path name that is truncated to `sizeof(sockaddr_un.sun_path)`
bytes, decreased by 1. This varies on different operating system between 91 and
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

The `backlog` argument is the maximum size of the queue of pending connections.
The maximum possible value will be determined by the operating system.

### server.listen(port[, hostname][, backlog][, callback])
<!-- YAML
added: v0.1.90
-->

The `server.listen()` method instructs the server to begin accepting connections
on the specified `port` and `hostname`. If the `hostname` is omitted, the server
will accept connections on any IPv6 address (`::`) when IPv6 is available, or
any IPv4 address (`0.0.0.0`) otherwise. Use a port value of `0` to have the
operating system assign an available port.

The `backlog` argument is the maximum size of the queue of pending connections.
The maximum possible value will be determined by the operating system.

The `server.listen()` method is asynchronous. When the server has been bound,
the [`'listening'`][] event will be emitted.

If a `callback` function is given, it will be added as a listener for the
[`'listening'`][] event.

An `EADDRINUSE` error will occur if another server is already bound to the
requested `port`. One possible way to mitigate such errors is to wait a
second and then try again as illustrated in the following example:

```js
server.on('error', (e) => {
  if (e.code == 'EADDRINUSE') {
    console.log('Address in use, retrying...');
    setTimeout(() => {
      server.close();
      server.listen(PORT, HOST);
    }, 1000);
  }
});
```

*Note*: All `net.Socket` objects in Node.js are set to use the `SO_REUSEADDR`
flag by default.

### server.listening
<!-- YAML
added: v5.7.0
-->

A boolean indicating whether or not the server is listening for connections.

### server.maxConnections
<!-- YAML
added: v0.2.0
-->

The `server.maxConnections` property can be set to specify the maximum number
of connections the server will accept. Once the current number of connections
reaches this threshold, all additional connection requests will be rejected.
The default is `undefined`.

*Note*: Setting this property *after* a socket has been sent to a child
using [`child_process.fork()`][] is *not recommended*.

### server.ref()
<!-- YAML
added: v0.9.1
-->

Calling the `server.ref()` method will cause the Node.js event loop to continue
so long as the `server` is not closed. Multiple calls to `server.ref()` will
have no effect.

Returns a reference to `server`.

### server.unref()
<!-- YAML
added: v0.9.1
-->

Calling the `server.unref()` method ensures that the Node.js event loop will
not be required to continue if this `server` is still open. Multiple calls to
`server.unref()` will have no effect.

Returns a reference to `server`.

## Class: net.Socket
<!-- YAML
added: v0.3.4
-->

The `net.Socket` object is an abstraction of a TCP or local socket. All
`net.Socket` instances implement a [Duplex][] Stream interface.

Instances may be created by users (e.g. using [`net.connect()`][]) or by
Node.js and passed to the user through the `'connection'` event.

All `net.Socket` objects are [`EventEmitter`][] instances.

### new net.Socket([options])
<!-- YAML
added: v0.3.4
-->

* `options` {Object}
  * `fd` {number} Used to specify a numeric file descriptor of an existing
    socket. Defaults to `null`.
  * `allowHalfOpen` {boolean} If `true`, allows the socket to be in a "half
    open" state. Defaults to `false`.
  * `readable` {boolean} If `fd` is specified, setting `readable` to `true`
    allows data from the socket to be read. Defaults to `false`.
  * `writable` {boolean} If `fd` is specified, setting `writabl` to `true`
    allows data to be written to the socket. Defaults to `false`.

Construct a new `net.Socket` instance.

### Event: 'close'
<!-- YAML
added: v0.1.90
-->

The `'close'` event is emitted once the socket is closed and can no longer be
used to send or receive data.

The listener callback is called with a single boolean `had_error` argument.
If `true`, then the socket was closed due to a transmission error.

```js
socket.on('close', (had_error) => {
  if (had_error)
    console.log('There was a transmission error!');
  else
    console.log('The connection closed normally.');
});
```

### Event: 'connect'
<!-- YAML
added: v0.1.90
-->

The `'connect'` event is emitted when a socket connection is successfully
established. See [`net.connect()`][].

The listener callback is called without passing any arguments.

```js
socket.on('connect', () => {
  console.log('The socket is connected!');
});
```

### Event: 'data'
<!-- YAML
added: v0.1.90
-->

The `'data'` event is emitted when data is received.

The listener callback will be invoked with a single `data` argument that will
be either a `Buffer` or string (if an encoding is set using
`socket.setEncoding()`). See the [Readable][] stream documentation for more
information.

*Note*: __Data will be lost__ if there is no listener when a `net.Socket`
emits a `'data'` event.

### Event: 'drain'
<!-- YAML
added: v0.1.90
-->

The `'drain'` event is emitted when the `net.Socket` instances write buffer
becomes empty. This can be used to throttle writes to the socket. See the
[Writable][] stream documentation for more information.

### Event: 'end'
<!-- YAML
added: v0.1.90
-->

The `'end'` event is emitted when the other end of the socket sends a `FIN`
packet.

By default (when the `allowHalfOpen` option is `false`) the socket will destroy
its underlying file descriptor once it has completely written out its pending
write queue.  However, by setting `allowHalfOpen` to `true`, the socket will not
automatically end its side of the socket allowing additional data to be written.

The listener callback is called without passing any arguments.

```js
socket.on('end', () => {
  console.log('The socket has ended!');
});
```

### Event: 'error'
<!-- YAML
added: v0.1.90
-->

The `'error'` event is emitted when an error occurs. The `'close'` event will be
emitted immediately following this event.

The listener callback is called with the `Error` passed as the only argument.

```js
socket.on('error', (error) => {
  console.log('There was an error!', error);
});
```

### Event: 'lookup'
<!-- YAML
added: v0.11.3
-->

The `'lookup'` event is emitted after the `net.Socket` has resolved the
`hostname` given when connection, but *before* the connection is established.
This event is not applicable when using UNIX sockets.

The callback function is called with the following arguments:

* `err` {Error|Null} The error object.  See [`dns.lookup()`][].
* `address` {String} The IP address.
* `family` {String|Null} The address type.  See [`dns.lookup()`][].
* `host` {String} The hostname.

```js
socket.on('lookup', (err, address, family, host) => {
  if (err) throw err;
  console.log(`${host} resolved to:`, address, family);
});
```

### Event: 'timeout'
<!-- YAML
added: v0.1.90
-->

The `'timeout'` event is emitted if the `net.Socket` connection times out due
to inactivity. This event is only a notification that the `net.Socket` has been
idle. The user must manually close the connection.

The listener callback is called without passing any arguments.

```js
socket.on('timeout', () => {
  console.log('There has been no activity on the connection.');
});
```

See also: [`socket.setTimeout()`][]

### socket.address()
<!-- YAML
added: v0.1.90
-->

The `socket.address()` method returns the address to which the socket is
bound, the address family name, and port of the socket as reported by the
operating system.

The object returned has the following properties:

* `port` {number}
* `family` {String} Either `'IPv4'` or `'IPv6'`
* `address` {String} The IPv4 or IPv6 address

### socket.bufferSize
<!-- YAML
added: v0.3.8
-->

The read-only `socket.bufferSize` property specifies the current amount of data
currently pending in the `net.Socket` instances write buffer.

The `net.Socket` class has been designed such that calls to `socket.write()`
will always work so long as the connection is open. Data written to the socket
is buffered internally first, then written out to the underlying connection. If
the data is written to the buffer faster than it can be passed to the connection
(e.g. when using a slow network connection), the size of the internal buffer
can grow.

The value of `socket.bufferSize` represents the amount of data (in bytes)
pending in the queue.

In order to avoid large or growing `socket.bufferSize` numbers, attempts should
be made to throttle the flow of data written to the socket.

### socket.bytesRead
<!-- YAML
added: v0.5.3
-->

The `socket.bytesRead` property specifies the total number of bytes received.

### socket.bytesWritten
<!-- YAML
added: v0.5.3
-->

The `socket.bytesWritten` property specifies the total number of bytes written.

### socket.connect(options[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `options` {Object}
  * `path`: Path the client should connect to
  * `port` {number} Port the client should connect to
  * `host` {String} Host the client should connect to
     Defaults to `'localhost'`
  * `localAddress` {String} Local interface to bind to for network connections
  * `localPort` {number} Local port to bind to for network connections
  * `family` {number} Version of IP stack. Defaults to `4`
  * `hints` {number} [`dns.lookup()` hints][]. Defaults to `0`
  * `lookup` {Function} Custom lookup function. Defaults to `dns.lookup`
* `connectListener` {Function}

The `socket.connect()` method opens the connection for a given socket using
either the given `path`, or the `port` and `host`.

In most normal situations it is not necessary to call this method directly
as other methods such as `net.createConnection()` will call `socket.connect()`
automatically.

This method operates asynchronously. The [`'connect'`][] event is emitted when
the connection is successfully established. If an error occurs while attempting
to open the connection, the [`'error'`][] event will be emitted.

If the `connectListener` argument is given, the callback will be added as a
listener for the [`'connect'`][] event.

### socket.connect(path[, connectListener])

* `path`: Path the client should connect to
* `connectListener` {Function}

The `socket.connect()` method opens the connection for a given socket using
the given `path`.

This method operates asynchronously. The [`'connect'`][] event is emitted when
the connection is successfully established. If an error occurs while attempting
to open the connection, the [`'error'`][] event will be emitted.

If the `connectListener` argument is given, the callback will be added as a
listener for the [`'connect'`][] event.

### socket.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

* `port` {number} Port the client should connect to
* `host` {String} Host the client should connect. to Defaults to `'localhost'`
* `connectListener` {Function}

The `socket.connect()` method opens the connection for a given socket using
the given `port` and `host`.

This method operates asynchronously. The [`'connect'`][] event is emitted when
the connection is successfully established. If an error occurs while attempting
to open the connection, the [`'error'`][] event will be emitted.

If the `connectListener` argument is given, the callback will be added as a
listener for the [`'connect'`][] event.

### socket.connecting
<!-- YAML
added: v6.1.0
-->

A boolean property that when `true`, indicates that `socket.connect()` has been
called a connection is in the process of being established. The value will be
set to `false` once the connection is established but *before* the `'connect'`
event is emitted. The value will also be `false` before `socket.connect()` is
called.

### socket.destroy([exception])
<!-- YAML
added: v0.1.90
-->

The `socket.destroy()` method ensures that no futher I/O activity can occur
with the socket. This is typically only necessary when errors occur.

If `exception` is specified, an [`'error'`][] event will be emitted and any
listeners for that event will receive `exception` as an argument.

### socket.destroyed

A Boolean value that indicates if the connection is destroyed or not. Once a
connection is destroyed no further data can be transferred using it.

### socket.end([data][, encoding])
<!-- YAML
added: v0.1.90
-->

* `data` {string|Buffer}
* `encoding` {string} A character encoding

The `socket.end()` method causes a `FIN` packet to be sent and the [Writable][]
side of the socket to be closed. This puts to socket into a "half-closed"
state. In this state, it is possible for data to be received via the
[Readable][] side of the socket.

If `data` is specified, it is equivalent to calling
`socket.write(data, encoding)` followed by `socket.end()`.

### socket.localAddress
<!-- YAML
added: v0.9.6
-->

Provides the string representation of the local IP address the remote peer is
connecting on. For example, if the `net.Socket` is listening on `'0.0.0.0'` and
the peer connects on `'192.168.1.1'`, the value would be `'192.168.1.1'`.

### socket.localPort
<!-- YAML
added: v0.9.6
-->

Provides the numeric representation of the local port. For example, `80` or
`21`.

### socket.pause()

The `socket.pause()` method pauses the reading of data. That is, [`'data'`][]
events will not be emitted. See the [Readable][] documentation for more
information.

### socket.ref()
<!-- YAML
added: v0.9.1
-->

Calling the `socket.ref()` method will cause the Node.js event loop to continue
so long as the `socket` is not closed. Multiple calls to `socket.ref()` will
have no effect.

Returns a reference to `socket`.

### socket.remoteAddress
<!-- YAML
added: v0.5.10
-->

Provides the string representation of the remote IP address. For example,
`'74.125.127.100'` or `'2001:4860:a005::68'`. Value may be `undefined` if
the socket is destroyed (for example, if the client disconnected).

### socket.remoteFamily
<!-- YAML
added: v0.11.14
-->

Provides the string representation of the remote IP family. Will be either
`'IPv4'` or `'IPv6'`.

### socket.remotePort
<!-- YAML
added: v0.5.10
-->

Provides the numeric representation of the remote port. For example, `80` or
`21`.

### socket.resume()

The `socket.resume()` method resumes reading after a call to
[`socket.pause()`][]. See the [Readable][] documentation for more information.

### socket.setEncoding([encoding])
<!-- YAML
added: v0.1.90
-->

The `socket.setEncoding()` method sets the encoding for the socket as a
[Readable Stream][]. See [`readable.setEncoding()`][] for more information.

### socket.setKeepAlive([enable][, initialDelay])
<!-- YAML
added: v0.1.92
-->

* `enable` {boolean} `true` to enable keep-alive, `false` to disable. Defaults
  to `false`.
* `initialDelay` {number} The number of milliseconds to set the delay between
  the last data packet received and the first keepalive probe. Setting `0` for
  `initialDelay` will leave the value unchanged from the default or previous
  setting. Defaults to `0`.

The `socket.setKeepAlive()` method enables or disables keep-alive functionality,
and optionally sets the initial delay before the first keepalive probe is sent
on an idle socket.

Returns a reference to `socket`.

### socket.setNoDelay([noDelay])
<!-- YAML
added: v0.1.90
-->

* `noDelay` {boolean} If `true`, disables Nagle's algorithm. Defaults to
  `true`

The `socket.setNoDelay()` method enables or disables Nagle's algorithm. By
default TCP connections use Nagle's algorithm and buffer data before sending
it off. Setting `true` for `noDelay` will immediately fire off data each time
`socket.write()` is called.

Returns a reference to `socket`.

*Note*: Disabling Nagle's algorithm is primarily useful when working with
protocols that use many small payloads as it reduces or eliminates the
buffering that would otherwise occur.

### socket.setTimeout(timeout[, callback])
<!-- YAML
added: v0.1.90
-->

* `timeout` {number}
* `callback` {Function}

The `socket.setTimeout()` method sets the socket to timeout after `timeout`
milliseconds of inactivity on the socket. By default `net.Socket` instances
do not have a timeout.

When an idle timeout is triggered, the `net.Socket` will emit a [`'timeout'`][]
event but the connection will not be severed. The user must manually
end the socket by call [`socket.end()`][] or [`socket.destroy()`][].

If `timeout` is `0`, then the existing idle timeout is disabled.

The optional `callback` parameter will be added as a one time listener for the
[`'timeout'`][] event.

Returns a reference to `socket`.

### socket.unref()
<!-- YAML
added: v0.9.1
-->

Calling the `socket.unref()` method ensures that the Node.js event loop will
not be required to continue if this `socket` is still open. Multiple calls to
`socket.unref()` will have no effect.

Returns a reference to `socket`.

### socket.write(data[, encoding][, callback])
<!-- YAML
added: v0.1.90
-->

* `data` {String|Buffer} The data to write
* `encoding` {String} The character encoding if `data` is a string. Defaults to
  `'utf8'`.
* `callback` {Function}

The `socket.write()` method queues data to be sent on the socket.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
The [`'drain'`][] event will be emitted when the buffer is again free.

The optional `callback` parameter will be executed when the data is finally
written out.

## net.connect(options[, connectListener])
<!-- YAML
added: v0.7.0
-->

* `options` {Object}
* `connectListener` {Function}

The `net.connect()` method creates a new [`net.Socket`][], automatically opens
the socket, then returns it.

The `options` are passed to both the [`net.Socket`][] constructor and the
[`socket.connect`][] method. The options relevant to each may be used.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

The following example illustrates a client for an echo server:

```js
const net = require('net');

const client = net.connect({port: 8124}, () => {
  // 'connect' listener
  console.log('connected to server!');
  client.write('world!\r\n');
}).on('data', (data) => {
  console.log(data.toString());
  client.end();
}).on('end', () => {
  console.log('disconnected from server');
});
```

## net.connect(path[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `path` {String}
* `connectListener` {Function}

The `net.connect()` method creates a new [`net.Socket`][], automatically opens
the socket using `path`, then returns it.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

## net.connect(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

* `port` {number}
* `host` {String}. Defaults to `localhost`
* `connectListener` {Function}

The `net.connect()` method creates a new [`net.Socket`][], automatically opens
the socket for the given `port` and `host`, then returns it.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

## net.createConnection(options[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `options` {Object}
* `connectListener` {Function}

The `net.createConnection()` method creates a new [`net.Socket`][],
automatically opens the socket, then returns it.

The `options` are passed to both the [`net.Socket`][] constructor and the
[`socket.connect`][] method. The options relevant to each may be used.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

The following example illustrates a client for an echo server:

```js
const net = require('net');

const client = net.createConnection({port: 8124}, () => {
  //'connect' listener
  console.log('connected to server!');
  client.write('world!\r\n');
}).on('data', (data) => {
  console.log(data.toString());
  client.end();
}).on('end', () => {
  console.log('disconnected from server');
});
```

## net.createConnection(path[, connectListener])
<!-- YAML
added: v0.1.90
-->

* `path` {String}
* `connectListener` {Function}

The `net.createConnection()` factory method creates a new [`net.Socket`][],
automatically opens the the socket using `path`, then returns it.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

## net.createConnection(port[, host][, connectListener])
<!-- YAML
added: v0.1.90
-->

* `port` {number}
* `host` {String}. Defaults to `localhost`
* `connectListener` {Function}

The `net.createConnection()` factory method creates a new [`net.Socket`][],
automatically opens the socket for the given `port` and `host`, then returns it.

The `connectListener` parameter will be added as a one-time listener for the
[`'connect'`][] event.

## net.createServer([options][, connectionListener])
<!-- YAML
added: v0.5.0
-->

* `options` {Object}
  * `allowHalfOpen` {boolean} If `true`, the socket associated with each
    incoming connection will not automatically send a `FIN` package when a `FIN`
    packet is received from the peer. The socket becomes non-readable, but still
    writable. Defaults to `false`
  * `pauseOnConnect` {boolean} If `true`, the socket associated with each
    incoming connection will be paused automatically and no data will be read,
    allowing connections to be passed between processes without reading or
    losing data. To begin reading data from a paused socket, call
    `socket.resume()`. Defaults to `false`
* `connectionListener` {Function}

The `net.createServer()` method creates and returns a new `net.Server`.

The `connectionListener` argument is automatically set as a listener for the
[`'connection'`][] event.

The following example implements a simple echo server that listeners for
connections on port `8124`:

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
}).on('error', (err) => {
  throw err;
}).listen(8124, () => {
  console.log('server bound');
});
```

## net.isIP(input)
<!-- YAML
added: v0.3.0
-->

* `input` {String}

The `net.isIP()` method is a simple utility for testing if `input` is an IP
address. Returns `0` for invalid strings, `4` for IPv4 addresses, and `6` for
IPv6 addresses.

## net.isIPv4(input)
<!-- YAML
added: v0.3.0
-->

* `input` {String}

The `net.isIPv4()` method is a simple utility that returns `true` if `input` is
a valid IPv4 address and `false` otherwise.


## net.isIPv6(input)
<!-- YAML
added: v0.3.0
-->

* `input` {String}

The `net.isIPv6()` method is a simple utility that returns `true` if `input` is
a valid IPv6 address and `false` otherwise.

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
[`net.connect()`]: #net_socket_connect_options_connectlistener
[`socket.destroy()`]: #net_socket_destroy
[`dns.lookup()`]: dns.html#dns_dns_lookup_hostname_options_callback
[`dns.lookup()` hints]: dns.html#dns_supported_getaddrinfo_flags
[`socket.end()`]: #net_socket_end_data_encoding
[`EventEmitter`]: events.html#events_class_eventemitter
[`net.Socket`]: #net_class_net_socket
[`socket.pause()`]: #net_socket_pause
[`socket.resume()`]: #net_socket_resume
[`server.getConnections()`]: #net_server_getconnections_callback
[`server.listen(port, host, backlog, callback)`]: #net_server_listen_port_hostname_backlog_callback
[`socket.connect(options, connectListener)`]: #net_socket_connect_options_connectlistener
[`socket.connect`]: #net_socket_connect_options_connectlistener
[`socket.setTimeout()`]: #net_socket_settimeout_timeout_callback
[`stream.setEncoding()`]: stream.html#stream_readable_setencoding_encoding
[Readable]: stream.html#stream_class_stream_readable
[Writable]: stream.html#stream_class_stream_writable
