# UDP / Datagram Sockets

> Stability: 2 - Stable

<!-- name=dgram -->

The `dgram` module provides an implementation of UDP Datagram sockets.

```js
const dgram = require('dgram');
const server = dgram.createSocket('udp4');

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
  console.log(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
});

server.on('listening', () => {
  var address = server.address();
  console.log(`server listening ${address.address}:${address.port}`);
});

server.bind(41234);
// server listening 0.0.0.0:41234
```

## Class: dgram.Socket
<!-- YAML
added: v0.1.99
-->

The `dgram.Socket` object is an [`EventEmitter`][] that encapsulates the
datagram functionality.

New instances of `dgram.Socket` are created using [`dgram.createSocket()`][].
The `new` keyword is not to be used to create `dgram.Socket` instances.

### Event: 'close'
<!-- YAML
added: v0.1.99
-->

The `'close'` event is emitted after a socket is closed with [`close()`][].
Once triggered, no new `'message'` events will be emitted on this socket.

### Event: 'error'
<!-- YAML
added: v0.1.99
-->

* `exception` {Error}

The `'error'` event is emitted whenever any error occurs. The event handler
function is passed a single Error object.

### Event: 'listening'
<!-- YAML
added: v0.1.99
-->

The `'listening'` event is emitted whenever a socket begins listening for
datagram messages. This occurs as soon as UDP sockets are created.

### Event: 'message'
<!-- YAML
added: v0.1.99
-->

The `'message'` event is emitted when a new datagram is available on a socket.
The event handler function is passed two arguments: `msg` and `rinfo`.
* `msg` {Buffer} - The message
* `rinfo` {Object} - Remote address information
  * `address` {string} The sender address
  * `family` {string} The address family (`'IPv4'` or `'IPv6'`)
  * `port` {number} The sender port
  * `size` {number} The message size

### socket.addMembership(multicastAddress[, multicastInterface])
<!-- YAML
added: v0.6.9
-->

* `multicastAddress` {string}
* `multicastInterface` {string}, Optional

Tells the kernel to join a multicast group at the given `multicastAddress` and
`multicastInterface` using the `IP_ADD_MEMBERSHIP` socket option. If the
`multicastInterface` argument is not specified, the operating system will choose
one interface and will add membership to it. To add membership to every
available interface, call `addMembership` multiple times, once per interface.

### socket.address()
<!-- YAML
added: v0.1.99
-->

Returns an object containing the address information for a socket.
For UDP sockets, this object will contain `address`, `family` and `port`
properties.

### socket.bind([port][, address][, callback])
<!-- YAML
added: v0.1.99
-->

* `port` {number} - Integer, Optional
* `address` {string}, Optional
* `callback` {Function} with no parameters, Optional. Called when
  binding is complete.

For UDP sockets, causes the `dgram.Socket` to listen for datagram
messages on a named `port` and optional `address`. If `port` is not
specified or is `0`, the operating system will attempt to bind to a
random port. If `address` is not specified, the operating system will
attempt to listen on all addresses.  Once binding is complete, a
`'listening'` event is emitted and the optional `callback` function is
called.

Note that specifying both a `'listening'` event listener and passing a
`callback` to the `socket.bind()` method is not harmful but not very
useful.

A bound datagram socket keeps the Node.js process running to receive
datagram messages.

If binding fails, an `'error'` event is generated. In rare case (e.g.
attempting to bind with a closed socket), an [`Error`][] may be thrown.

Example of a UDP server listening on port 41234:

```js
const dgram = require('dgram');
const server = dgram.createSocket('udp4');

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
  console.log(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
});

server.on('listening', () => {
  var address = server.address();
  console.log(`server listening ${address.address}:${address.port}`);
});

server.bind(41234);
// server listening 0.0.0.0:41234
```

### socket.bind(options[, callback])
<!-- YAML
added: v0.11.14
-->

* `options` {Object} - Required. Supports the following properties:
  * `port` {number} - Optional.
  * `address` {string} - Optional.
  * `exclusive` {boolean} - Optional.
* `callback` {Function} - Optional.

For UDP sockets, causes the `dgram.Socket` to listen for datagram
messages on a named `port` and optional `address` that are passed as
properties of an `options` object passed as the first argument. If
`port` is not specified or is `0`, the operating system will attempt
to bind to a random port. If `address` is not specified, the operating
system will attempt to listen on all addresses.  Once binding is
complete, a `'listening'` event is emitted and the optional `callback`
function is called.

Note that specifying both a `'listening'` event listener and passing a
`callback` to the `socket.bind()` method is not harmful but not very
useful.

The `options` object may contain an additional `exclusive` property that is
use when using `dgram.Socket` objects with the [`cluster`] module. When
`exclusive` is set to `false` (the default), cluster workers will use the same
underlying socket handle allowing connection handling duties to be shared.
When `exclusive` is `true`, however, the handle is not shared and attempted
port sharing results in an error.

A bound datagram socket keeps the Node.js process running to receive
datagram messages.

If binding fails, an `'error'` event is generated. In rare case (e.g.
attempting to bind with a closed socket), an [`Error`][] may be thrown.

An example socket listening on an exclusive port is shown below.

```js
socket.bind({
  address: 'localhost',
  port: 8000,
  exclusive: true
});
```

### socket.close([callback])
<!-- YAML
added: v0.1.99
-->

Close the underlying socket and stop listening for data on it. If a callback is
provided, it is added as a listener for the [`'close'`][] event.

### socket.dropMembership(multicastAddress[, multicastInterface])
<!-- YAML
added: v0.6.9
-->

* `multicastAddress` {string}
* `multicastInterface` {string}, Optional

Instructs the kernel to leave a multicast group at `multicastAddress` using the
`IP_DROP_MEMBERSHIP` socket option. This method is automatically called by the
kernel when the socket is closed or the process terminates, so most apps will
never have reason to call this.

If `multicastInterface` is not specified, the operating system will attempt to
drop membership on all valid interfaces.

### socket.ref()
<!-- YAML
added: v0.9.1
-->

By default, binding a socket will cause it to block the Node.js process from
exiting as long as the socket is open. The `socket.unref()` method can be used
to exclude the socket from the reference counting that keeps the Node.js
process active. The `socket.ref()` method adds the socket back to the reference
counting and restores the default behavior.

Calling `socket.ref()` multiples times will have no additional effect.

The `socket.ref()` method returns a reference to the socket so calls can be
chained.

### socket.send(msg, [offset, length,] port [, address] [, callback])
<!-- YAML
added: v0.1.99
changes:
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/5929
    description: On success, `callback` will now be called with an `error`
                 argument of `null` rather than `0`.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/10473
    description: The `address` parameter is always optional now.
  - version: v5.7.0
    pr-url: https://github.com/nodejs/node/pull/4374
    description: The `msg` parameter can be an array now. Also, the `offset`
                 and `length` parameters are optional now.
-->

* `msg` {Buffer|string|array} Message to be sent
* `offset` {number} Integer. Optional. Offset in the buffer where the message starts.
* `length` {number} Integer. Optional. Number of bytes in the message.
* `port` {number} Integer. Destination port.
* `address` {string} Destination hostname or IP address. Optional.
* `callback` {Function} Called when the message has been sent. Optional.

Broadcasts a datagram on the socket. The destination `port` and `address` must
be specified.

The `msg` argument contains the message to be sent.
Depending on its type, different behavior can apply. If `msg` is a `Buffer`,
the `offset` and `length` specify the offset within the `Buffer` where the
message begins and the number of bytes in the message, respectively.
If `msg` is a `String`, then it is automatically converted to a `Buffer`
with `'utf8'` encoding. With messages that
contain  multi-byte characters, `offset` and `length` will be calculated with
respect to [byte length][] and not the character position.
If `msg` is an array, `offset` and `length` must not be specified.

The `address` argument is a string. If the value of `address` is a host name,
DNS will be used to resolve the address of the host.  If `address` is not
provided or otherwise falsy, `'127.0.0.1'` (for `udp4` sockets) or `'::1'`
(for `udp6` sockets) will be used by default.

If the socket has not been previously bound with a call to `bind`, the socket
is assigned a random port number and is bound to the "all interfaces" address
(`'0.0.0.0'` for `udp4` sockets, `'::0'` for `udp6` sockets.)

An optional `callback` function  may be specified to as a way of reporting
DNS errors or for determining when it is safe to reuse the `buf` object.
Note that DNS lookups delay the time to send for at least one tick of the
Node.js event loop.

The only way to know for sure that the datagram has been sent is by using a
`callback`. If an error occurs and a `callback` is given, the error will be
passed as the first argument to the `callback`. If a `callback` is not given,
the error is emitted as an `'error'` event on the `socket` object.

Offset and length are optional, but if you specify one you would need to
specify the other. Also, they are supported only when the first
argument is a `Buffer`.

Example of sending a UDP packet to a random port on `localhost`;

```js
const dgram = require('dgram');
const message = Buffer.from('Some bytes');
const client = dgram.createSocket('udp4');
client.send(message, 41234, 'localhost', (err) => {
  client.close();
});
```

Example of sending a UDP packet composed of multiple buffers to a random port
on `127.0.0.1`;

```js
const dgram = require('dgram');
const buf1 = Buffer.from('Some ');
const buf2 = Buffer.from('bytes');
const client = dgram.createSocket('udp4');
client.send([buf1, buf2], 41234, (err) => {
  client.close();
});
```

Sending multiple buffers might be faster or slower depending on your
application and operating system: benchmark it. Usually it is faster.

**A Note about UDP datagram size**

The maximum size of an `IPv4/v6` datagram depends on the `MTU`
(_Maximum Transmission Unit_) and on the `Payload Length` field size.

- The `Payload Length` field is `16 bits` wide, which means that a normal
  payload exceed 64K octets _including_ the internet header and data
  (65,507 bytes = 65,535 − 8 bytes UDP header − 20 bytes IP header);
  this is generally true for loopback interfaces, but such long datagram
  messages are impractical for most hosts and networks.

- The `MTU` is the largest size a given link layer technology can support for
  datagram messages. For any link, `IPv4` mandates a minimum `MTU` of `68`
  octets, while the recommended `MTU` for IPv4 is `576` (typically recommended
  as the `MTU` for dial-up type applications), whether they arrive whole or in
  fragments.

  For `IPv6`, the minimum `MTU` is `1280` octets, however, the mandatory minimum
  fragment reassembly buffer size is `1500` octets. The value of `68` octets is
  very small, since most current link layer technologies, like Ethernet, have a
  minimum `MTU` of `1500`.

It is impossible to know in advance the MTU of each link through which
a packet might travel. Sending a datagram greater than the receiver `MTU` will
not work because the packet will get silently dropped without informing the
source that the data did not reach its intended recipient.

### socket.setBroadcast(flag)
<!-- YAML
added: v0.6.9
-->

* `flag` {boolean}

Sets or clears the `SO_BROADCAST` socket option.  When set to `true`, UDP
packets may be sent to a local interface's broadcast address.

### socket.setMulticastLoopback(flag)
<!-- YAML
added: v0.3.8
-->

* `flag` {boolean}

Sets or clears the `IP_MULTICAST_LOOP` socket option.  When set to `true`,
multicast packets will also be received on the local interface.

### socket.setMulticastTTL(ttl)
<!-- YAML
added: v0.3.8
-->

* `ttl` {number} Integer

Sets the `IP_MULTICAST_TTL` socket option.  While TTL generally stands for
"Time to Live", in this context it specifies the number of IP hops that a
packet is allowed to travel through, specifically for multicast traffic.  Each
router or gateway that forwards a packet decrements the TTL. If the TTL is
decremented to 0 by a router, it will not be forwarded.

The argument passed to to `socket.setMulticastTTL()` is a number of hops
between 0 and 255. The default on most systems is `1` but can vary.

### socket.setTTL(ttl)
<!-- YAML
added: v0.1.101
-->

* `ttl` {number} Integer

Sets the `IP_TTL` socket option. While TTL generally stands for "Time to Live",
in this context it specifies the number of IP hops that a packet is allowed to
travel through.  Each router or gateway that forwards a packet decrements the
TTL.  If the TTL is decremented to 0 by a router, it will not be forwarded.
Changing TTL values is typically done for network probes or when multicasting.

The argument to `socket.setTTL()` is a number of hops between 1 and 255.
The default on most systems is 64 but can vary.

### socket.unref()
<!-- YAML
added: v0.9.1
-->

By default, binding a socket will cause it to block the Node.js process from
exiting as long as the socket is open. The `socket.unref()` method can be used
to exclude the socket from the reference counting that keeps the Node.js
process active, allowing the process to exit even if the socket is still
listening.

Calling `socket.unref()` multiple times will have no addition effect.

The `socket.unref()` method returns a reference to the socket so calls can be
chained.

### Change to asynchronous `socket.bind()` behavior

As of Node.js v0.10, [`dgram.Socket#bind()`][] changed to an asynchronous
execution model. Legacy code that assumes synchronous behavior, as in the
following example:

```js
const s = dgram.createSocket('udp4');
s.bind(1234);
s.addMembership('224.0.0.114');
```

Must be changed to pass a callback function to the [`dgram.Socket#bind()`][]
function:

```js
const s = dgram.createSocket('udp4');
s.bind(1234, () => {
  s.addMembership('224.0.0.114');
});
```

## `dgram` module functions

### dgram.createSocket(options[, callback])
<!-- YAML
added: v0.11.13
-->

* `options` {Object}
* `callback` {Function} Attached as a listener to `'message'` events.
* Returns: {dgram.Socket}

Creates a `dgram.Socket` object. The `options` argument is an object that
should contain a `type` field of either `udp4` or `udp6` and an optional
boolean `reuseAddr` field.

When `reuseAddr` is `true` [`socket.bind()`][] will reuse the address, even if
another process has already bound a socket on it. `reuseAddr` defaults to
`false`. The optional `callback` function is added as a listener for `'message'`
events.

Once the socket is created, calling [`socket.bind()`][] will instruct the
socket to begin listening for datagram messages. When `address` and `port` are
not passed to  [`socket.bind()`][] the method will bind the socket to the "all
interfaces" address on a random port (it does the right thing for both `udp4`
and `udp6` sockets). The bound address and port can be retrieved using
[`socket.address().address`][] and [`socket.address().port`][].

### dgram.createSocket(type[, callback])
<!-- YAML
added: v0.1.99
-->

* `type` {string} - Either 'udp4' or 'udp6'
* `callback` {Function} - Attached as a listener to `'message'` events.
  Optional
* Returns: {dgram.Socket}

Creates a `dgram.Socket` object of the specified `type`. The `type` argument
can be either `udp4` or `udp6`. An optional `callback` function can be passed
which is added as a listener for `'message'` events.

Once the socket is created, calling [`socket.bind()`][] will instruct the
socket to begin listening for datagram messages. When `address` and `port` are
not passed to  [`socket.bind()`][] the method will bind the socket to the "all
interfaces" address on a random port (it does the right thing for both `udp4`
and `udp6` sockets). The bound address and port can be retrieved using
[`socket.address().address`][] and [`socket.address().port`][].

[`EventEmitter`]: events.html
[`Buffer`]: buffer.html
[`'close'`]: #dgram_event_close
[`close()`]: #dgram_socket_close_callback
[`cluster`]: cluster.html
[`dgram.createSocket()`]: #dgram_dgram_createsocket_options_callback
[`dgram.Socket#bind()`]: #dgram_socket_bind_options_callback
[`Error`]: errors.html#errors_class_error
[`socket.address().address`]: #dgram_socket_address
[`socket.address().port`]: #dgram_socket_address
[`socket.bind()`]: #dgram_socket_bind_port_address_callback
[byte length]: buffer.html#buffer_class_method_buffer_bytelength_string_encoding
