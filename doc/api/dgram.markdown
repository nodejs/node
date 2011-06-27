## UDP / Datagram Sockets

Datagram sockets are available through `require('dgram')`.  Datagrams are most commonly
handled as IP/UDP messages but they can also be used over Unix domain sockets.

### Event: 'message'

`function (msg, rinfo) { }`

Emitted when a new datagram is available on a socket.  `msg` is a `Buffer` and `rinfo` is
an object with the sender's address information and the number of bytes in the datagram.

### Event: 'listening'

`function () { }`

Emitted when a socket starts listening for datagrams.  This happens as soon as UDP sockets
are created.  Unix domain sockets do not start listening until calling `bind()` on them.

### Event: 'close'

`function () { }`

Emitted when a socket is closed with `close()`.  No new `message` events will be emitted
on this socket.

### dgram.createSocket(type, [callback])

Creates a datagram socket of the specified types.  Valid types are:
`udp4`, `udp6`, and `unix_dgram`.

Takes an optional callback which is added as a listener for `message` events.

### dgram.send(buf, offset, length, path, [callback])

For Unix domain datagram sockets, the destination address is a pathname in the filesystem.
An optional callback may be supplied that is invoked after the `sendto` call is completed
by the OS.  It is not safe to re-use `buf` until the callback is invoked.  Note that
unless the socket is bound to a pathname with `bind()` there is no way to receive messages
on this socket.

Example of sending a message to syslogd on OSX via Unix domain socket `/var/run/syslog`:

    var dgram = require('dgram');
    var message = new Buffer("A message to log.");
    var client = dgram.createSocket("unix_dgram");
    client.send(message, 0, message.length, "/var/run/syslog",
      function (err, bytes) {
        if (err) {
          throw err;
        }
        console.log("Wrote " + bytes + " bytes to socket.");
    });

### dgram.send(buf, offset, length, port, address, [callback])

For UDP sockets, the destination port and IP address must be specified.  A string
may be supplied for the `address` parameter, and it will be resolved with DNS.  An
optional callback may be specified to detect any DNS errors and when `buf` may be
re-used.  Note that DNS lookups will delay the time that a send takes place, at
least until the next tick.  The only way to know for sure that a send has taken place
is to use the callback.

Example of sending a UDP packet to a random port on `localhost`;

    var dgram = require('dgram');
    var message = new Buffer("Some bytes");
    var client = dgram.createSocket("udp4");
    client.send(message, 0, message.length, 41234, "localhost");
    client.close();


### dgram.bind(path)

For Unix domain datagram sockets, start listening for incoming datagrams on a
socket specified by `path`. Note that clients may `send()` without `bind()`,
but no datagrams will be received without a `bind()`.

Example of a Unix domain datagram server that echoes back all messages it receives:

    var dgram = require("dgram");
    var serverPath = "/tmp/dgram_server_sock";
    var server = dgram.createSocket("unix_dgram");

    server.on("message", function (msg, rinfo) {
      console.log("got: " + msg + " from " + rinfo.address);
      server.send(msg, 0, msg.length, rinfo.address);
    });

    server.on("listening", function () {
      console.log("server listening " + server.address().address);
    })

    server.bind(serverPath);

Example of a Unix domain datagram client that talks to this server:

    var dgram = require("dgram");
    var serverPath = "/tmp/dgram_server_sock";
    var clientPath = "/tmp/dgram_client_sock";

    var message = new Buffer("A message at " + (new Date()));

    var client = dgram.createSocket("unix_dgram");

    client.on("message", function (msg, rinfo) {
      console.log("got: " + msg + " from " + rinfo.address);
    });

    client.on("listening", function () {
      console.log("client listening " + client.address().address);
      client.send(message, 0, message.length, serverPath);
    });

    client.bind(clientPath);

### dgram.bind(port, [address])

For UDP sockets, listen for datagrams on a named `port` and optional `address`.  If
`address` is not specified, the OS will try to listen on all addresses.

Example of a UDP server listening on port 41234:

    var dgram = require("dgram");

    var server = dgram.createSocket("udp4");

    server.on("message", function (msg, rinfo) {
      console.log("server got: " + msg + " from " +
        rinfo.address + ":" + rinfo.port);
    });

    server.on("listening", function () {
      var address = server.address();
      console.log("server listening " +
          address.address + ":" + address.port);
    });

    server.bind(41234);
    // server listening 0.0.0.0:41234


### dgram.close()

Close the underlying socket and stop listening for data on it.  UDP sockets
automatically listen for messages, even if they did not call `bind()`.

### dgram.address()

Returns an object containing the address information for a socket.  For UDP sockets,
this object will contain `address` and `port`.  For Unix domain sockets, it will contain
only `address`.

### dgram.setBroadcast(flag)

Sets or clears the `SO_BROADCAST` socket option.  When this option is set, UDP packets
may be sent to a local interface's broadcast address.

### dgram.setTTL(ttl)

Sets the `IP_TTL` socket option.  TTL stands for "Time to Live," but in this context it
specifies the number of IP hops that a packet is allowed to go through.  Each router or
gateway that forwards a packet decrements the TTL.  If the TTL is decremented to 0 by a
router, it will not be forwarded.  Changing TTL values is typically done for network
probes or when multicasting.

The argument to `setTTL()` is a number of hops between 1 and 255.  The default on most
systems is 64.

### dgram.setMulticastTTL(ttl)

Sets the `IP_MULTICAST_TTL` socket option.  TTL stands for "Time to Live," but in this
context it specifies the number of IP hops that a packet is allowed to go through,
specifically for multicast traffic.  Each router or gateway that forwards a packet
decrements the TTL. If the TTL is decremented to 0 by a router, it will not be forwarded.

The argument to `setMulticastTTL()` is a number of hops between 0 and 255.  The default on most
systems is 64.

### dgram.setMulticastLoopback(flag)

Sets or clears the `IP_MULTICAST_LOOP` socket option.  When this option is set, multicast
packets will also be received on the local interface.

### dgram.addMembership(multicastAddress, [multicastInterface])

Tells the kernel to join a multicast group with `IP_ADD_MEMBERSHIP` socket option.

If `multicastInterface` is not specified, the OS will try to add membership to all valid
interfaces.

### dgram.dropMembership(multicastAddress, [multicastInterface])

Opposite of `addMembership` - tells the kernel to leave a multicast group with
`IP_DROP_MEMBERSHIP` socket option. This is automatically called by the kernel
when the socket is closed or process terminates, so most apps will never need to call
this.

If `multicastInterface` is not specified, the OS will try to drop membership to all valid
interfaces.
