# UDP / Datagram Sockets

    Stability: 3 - Stable

<!-- name=dgram -->

Datagram sockets are available through `require('dgram')`.

## dgram.createSocket(type, [callback])

* `type` String. Either 'udp4' or 'udp6'
* `callback` Function. Attached as a listener to `message` events.
  Optional
* Returns: Socket object

Creates a datagram Socket of the specified types.  Valid types are `udp4`
and `udp6`.

Takes an optional callback which is added as a listener for `message` events.

Call `socket.bind` if you want to receive datagrams. `socket.bind()` will bind
to the "all interfaces" address on a random port (it does the right thing for
both `udp4` and `udp6` sockets). You can then retrieve the address and port
with `socket.address().address` and `socket.address().port`.

## Class: Socket

The dgram Socket class encapsulates the datagram functionality.  It
should be created via `dgram.createSocket(type, [callback])`.

### Event: 'message'

* `msg` Buffer object. The message
* `rinfo` Object. Remote address information

Emitted when a new datagram is available on a socket.  `msg` is a `Buffer` and `rinfo` is
an object with the sender's address information and the number of bytes in the datagram.

### Event: 'listening'

Emitted when a socket starts listening for datagrams.  This happens as soon as UDP sockets
are created.

### Event: 'close'

Emitted when a socket is closed with `close()`.  No new `message` events will be emitted
on this socket.

### Event: 'error'

* `exception` Error object

Emitted when an error occurs.

### dgram.send(buf, offset, length, port, address, [callback])

* `buf` Buffer object.  Message to be sent
* `offset` Integer. Offset in the buffer where the message starts.
* `length` Integer. Number of bytes in the message.
* `port` Integer. destination port
* `address` String. destination IP
* `callback` Function. Callback when message is done being delivered.
  Optional.

For UDP sockets, the destination port and IP address must be specified.  A string
may be supplied for the `address` parameter, and it will be resolved with DNS.  An
optional callback may be specified to detect any DNS errors and when `buf` may be
re-used.  Note that DNS lookups will delay the time that a send takes place, at
least until the next tick.  The only way to know for sure that a send has taken place
is to use the callback.

If the socket has not been previously bound with a call to `bind`, it's
assigned a random port number and bound to the "all interfaces" address
(0.0.0.0 for `udp4` sockets, ::0 for `udp6` sockets).

Example of sending a UDP packet to a random port on `localhost`;

    var dgram = require('dgram');
    var message = new Buffer("Some bytes");
    var client = dgram.createSocket("udp4");
    client.send(message, 0, message.length, 41234, "localhost", function(err, bytes) {
      client.close();
    });

**A Note about UDP datagram size**

The maximum size of an `IPv4/v6` datagram depends on the `MTU` (_Maximum Transmission Unit_)
and on the `Payload Length` field size.

- The `Payload Length` field is `16 bits` wide, which means that a normal payload
  cannot be larger than 64K octets including internet header and data
  (65,507 bytes = 65,535 − 8 bytes UDP header − 20 bytes IP header);
  this is generally true for loopback interfaces, but such long datagrams
  are impractical for most hosts and networks.

- The `MTU` is the largest size a given link layer technology can support for datagrams.
  For any link, `IPv4` mandates a minimum `MTU` of `68` octets, while the recommended `MTU`
  for IPv4 is `576` (typically recommended as the `MTU` for dial-up type applications),
  whether they arrive whole or in fragments.

  For `IPv6`, the minimum `MTU` is `1280` octets, however, the mandatory minimum
  fragment reassembly buffer size is `1500` octets.
  The value of `68` octets is very small, since most current link layer technologies have
  a minimum `MTU` of `1500` (like Ethernet).

Note that it's impossible to know in advance the MTU of each link through which
a packet might travel, and that generally sending a datagram greater than
the (receiver) `MTU` won't work (the packet gets silently dropped, without
informing the source that the data did not reach its intended recipient).

### dgram.bind(port, [address])

* `port` Integer
* `address` String, Optional

For UDP sockets, listen for datagrams on a named `port` and optional `address`. If
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

Close the underlying socket and stop listening for data on it.

### dgram.address()

Returns an object containing the address information for a socket.  For UDP sockets,
this object will contain `address` and `port`.

### dgram.setBroadcast(flag)

* `flag` Boolean

Sets or clears the `SO_BROADCAST` socket option.  When this option is set, UDP packets
may be sent to a local interface's broadcast address.

### dgram.setTTL(ttl)

* `ttl` Integer

Sets the `IP_TTL` socket option.  TTL stands for "Time to Live," but in this context it
specifies the number of IP hops that a packet is allowed to go through.  Each router or
gateway that forwards a packet decrements the TTL.  If the TTL is decremented to 0 by a
router, it will not be forwarded.  Changing TTL values is typically done for network
probes or when multicasting.

The argument to `setTTL()` is a number of hops between 1 and 255.  The default on most
systems is 64.

### dgram.setMulticastTTL(ttl)

* `ttl` Integer

Sets the `IP_MULTICAST_TTL` socket option.  TTL stands for "Time to Live," but in this
context it specifies the number of IP hops that a packet is allowed to go through,
specifically for multicast traffic.  Each router or gateway that forwards a packet
decrements the TTL. If the TTL is decremented to 0 by a router, it will not be forwarded.

The argument to `setMulticastTTL()` is a number of hops between 0 and 255.  The default on most
systems is 64.

### dgram.setMulticastLoopback(flag)

* `flag` Boolean

Sets or clears the `IP_MULTICAST_LOOP` socket option.  When this option is set, multicast
packets will also be received on the local interface.

### dgram.addMembership(multicastAddress, [multicastInterface])

* `multicastAddress` String
* `multicastInterface` String, Optional

Tells the kernel to join a multicast group with `IP_ADD_MEMBERSHIP` socket option.

If `multicastInterface` is not specified, the OS will try to add membership to all valid
interfaces.

### dgram.dropMembership(multicastAddress, [multicastInterface])

* `multicastAddress` String
* `multicastInterface` String, Optional

Opposite of `addMembership` - tells the kernel to leave a multicast group with
`IP_DROP_MEMBERSHIP` socket option. This is automatically called by the kernel
when the socket is closed or process terminates, so most apps will never need to call
this.

If `multicastInterface` is not specified, the OS will try to drop membership to all valid
interfaces.
