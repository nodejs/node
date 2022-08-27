# A QUIC Introduction

Welcome! You've found the source of the Node.js QUIC implementation. This guide
will start you on your journey to understanding how this implementation works.

## First, what is QUIC?

QUIC is a UDP-based transport protocol developed by the IETF and published as
[RFC 9000][]. I strongly recommend that you take the time to read through that
specification before continuing as it will introduce many of the underlying
concepts.

Just go ahead and go read all of the following QUIC related specs now:

* [RFC 8999][]: Version-Independent Properties of QUIC
* [RFC 9000][]: QUIC: A UDP-Based Multiplexed and Secure Transport
* [RFC 9001][]: Using TLS to Secure QUIC
* [RFC 9002][]: QUIC Loss Detection and Congestion Control

### Isn't QUIC just HTTP/3?

HTTP/3 is an application of the HTTP protocol semantics on top of QUIC. The two
are not the same thing. It is possible (and will be common) to implement
applications of QUIC that have nothing to do with HTTP/3, but HTTP/3 will always
be implemented on top of QUIC.

At the time I'm writing this, the QUIC RFC's have been finished, but the HTTP/3
specification is still under development. I'd recommend also reading through
these draft specifications before continuing on:

* [draft-ietf-quic-http-34][]: Hypertext Transfer Protocol Version 3 (HTTP/3)
* [draft-ietf-quic-qpack-21][]: QPACK: Header Compression for HTTP/3

The IETF working group is working on a number of other documents that you'll
also want to get familiar with. Check those out here:

* [https://datatracker.ietf.org/wg/quic/documents/]()

This guide will first deal with explaining QUIC and the QUIC implementation in
general, and then will address HTTP/3.

### So if QUIC is not HTTP/3, what is it?

QUIC is a stateful, connection-oriented, client-server UDP protocol that
includes flow-control, multiplexed streams, network-path migration, low-latency
connection establishment, and integrated TLS 1.3.

A QUIC connection is always initiated by the client and starts with a handshake
phase that includes a TLS 1.3 CLIENT-HELLO and a set of configuration
parameters that QUIC calls "transport parameters". [RFC 9001][] details exactly
how QUIC uses TLS 1.3.

The TLS handshake establishes cryptographic keys that will be used to encrypt
all QUIC protocol data that is passed back and forth. As I will explain in a
bit, it is possible for the client and server start exchanging data before
these keys have been fully negotiated, but for now we'll ignore that and focus
on the fully-protected case.

After the TLS 1.3 handshake is completed, and the packet protection keys have
been established, the QUIC protocol primarily consists of opening undirectional
or bidirectional streams of data between the client and server. An important
characteristic is that streams can be opened by _either_ of the two peers.

Unsurprisingly, a unidirectional stream allows sending data in only one
direction on the connection. Bidirectional streams allow both endpoints to send
and receive data. QUIC streams are nothing more than a sequence of bytes. There
are no headers, no trailers, just a sequence of octets that are spread out of
one or more QUIC packets encoded into one or more UDP packets.

The simplistic life cycle of a QUIC connection, then, is:

* Initiate the connection with a TLS Handshake.
* Open one or more streams to transmit data.
* Close the connection when done transmitting data.

Nice and simple, right? Well, there's a bit more that happens in there.

QUIC includes built-in reliability and flow-control mechanisms. Because UDP is
notoriously unreliable, every QUIC packet sent by an endpoint is identified by
a monotonically increasing packet number. Each endpoint keeps track of which
packets it has received and sends an acknowledgement to the other endpoint. If
the sending endpoint does not receive an acknowledgement for a packet it has
sent within some specified period of time, then the endpoint will assume the
packet was lost and will retransmit it. As I'll show later, this causes some
complexity in the implementation when it comes to how long data must be
retained in memory.

For flow-control, QUIC implements separate congestion windows (cwnd) for both
the overall connection _and_ per individual stream. To keep it simple, the
receiving endpoint tells the sending endpoint how much additional data it is
willing to accept right now. The sending endpoint could choose to send more but
the receiving endpoint could also choose to ignore that additional data or even
close the connection if the sender does not is not cooperating.

There are also a number of built in mechanisms that endpoints can use to
validate that a usable network path exists between the endpoints. This is a
particularly important and unique characteristic of QUIC given that it is built
on UDP. Unlike TCP, where a connection establishes a persistent flow of data
that is tightly bound to a specific network path, UDP traffic is much more
flexible. With QUIC, it is possible to start a connection on one network
connection but transition it to another (for instance, when a mobile device
changes from a WiFi connection to a mobile data connection). When such a
transition happens, the QUIC endpoints must verify that they can still
successfully send packets to each other securely (this implementation currently
does not support connection migration).

All of this adds additional complexity to the protocol implementation. With
TCP, which also includes flow control, reliability, and so forth, Node.js can
rely on the operating system handling everything. QUIC, however, is designed to
be implementable in user-space -- that is, it is designed such that, to the
kernel, it looks like ordinary UDP traffic. This is a good thing, in general,
but it means the Node.js implementation needs to be quite a bit more complex
than "regular" UDP datagrams or TCP connections that are abstracted behind
libuv APIs and operating system syscalls.

There are lots of details we could go into but for now, let's turn the focus to
how the QUIC implementation here is structured and how it operates.

## Code organization

Thankfully for us, there are open source libraries that implement most of the
complex details of QUIC packet serialization and deserialization, flow control,
data loss, connection state management, and more. We've chosen to use the
[ngtcp2][] and [nghttp3][] libraries. These can be found in the `deps` folder
along with the other vendored-in dependencies.

The directory in which you've found this README.md (`src/quic`) contains the
C++ code that bridges `ngtcp2` and `nghttp3` into the Node.js environment.
These provide the _internal_ API surface and underlying implementation that
supports the intermediate JavaScript API that can be found in
`lib/internal/quic`.

So, in summary:

* `deps/ngtcp2` and `deps/nghttp3` provide the low-level protocol
  implementation.
* `src/quic` provides the Node.js-specific I/O, state management, and internal
  API.
* `lib/internal/quic` provides the intermediate JavaScript API.

## The Internals

The Node.js QUIC implementation is built around a model that tracks closely
with the key elements of the protocol. There are three main components:

* The `Endpoint` represents a binding to a local UDP socket. The `Endpoint` is
  ultimately responsible for:
  * Transmitting and receiving UDP packets, and verifying that received packets
    are valid QUIC packets; and
  * Creating new client and server `Sessions`.
* A `Session` represents either the client-side or server-side of a QUIC
  connection. The `Session` is responsible for persisting the current state of
  the connection, including the TLS 1.3 packet protection keys, the current set
  of open streams, and handling the serialization/deserialization of QUIC
  packets.
* A `Stream` represents an individual unidirectional, or bidirectional flow of
  data through a `Session`.

### `Endpoint`

You can think of the `Endpoint` as the entry point into the QUIC protocol.
Creating the `Endpoint` is always the first step, regardless of whether you'll
be acting as a client or a server. The `Endpoint` manages a local binding to a
`uv_udp_t` UDP socket handle. It controls the full lifecycle of that socket and
manages the flow of data through that socket. If you're already familiar with
Node.js internals, the `Endpoint` has many similarities to the `UDPWrap` class
that underlies the UDP/Datagram module. However, there are a some important
differences.

Within the internal API, and `Endpoint` consists of three primary elements:

* An `Endpoint::UDP` object that directly wraps the `uv_udp_t` handle. This is
  internal to the `Endpoint`.
* An `Endpoint` that performs all of the protocol-specific work.
* An `EndpointOptions` objet that is used to configure the Endpoint.

When the `Endpoint` is told to listen as a server, or connect as a client, it's
internal `UDP` instance will automatically bind to the local UDP port.

When the `Endpoint::UDP` receives a packet, it will immediately forward that on
to the owning `Endpoint`, which will perform some basic validation checks on
the packet to first determine whether it is an acceptable QUIC packet. Second
it will determine if it is a new initial packet (intended to create a new QUIC
connection) or a packet for an already established QUIC connection.

If the received packet is a valid initial packet, the `Endpoint` will create a
new server-side `Session`. If the packet is valid but not an initial packet, it
will forward it to the appropriate existing `Session`.

#### Network-Path Validation

Aside from managing the lifecycle of the UDP handle, and managing the routing
of packets the `Endpoint` is also responsible for performing initial network
path validation and keeping track of validated remote socket addresses.

By default, the first time the `Endpoint` receives an initial QUIC packet from
a peer, it will respond with a "retry challenge". A retry is a special QUIC
packet that contains a cryptographic token that the client must receive and
return in subsequent packets in order to demonstrate that the network path
between the two peers is viable. Once the path has been verified, the
`Endpoint` uses an LRU cache to remember the socket address so that it can
skip validation on future connections. That cache is transient and only
persists as long as the `Endpoint` instance exists. Also, the cache is not
shared among multiple `Endpoint` instances, so the path verification may be
repeated.

The retry token is cryptographically generated and incorporates protections
that protect it against abuse or forgery. For the client peer, the token is
opaque and should appear random. For the server that created it, however, the
token will incorporate a timestamp and the remote socket address so that
attempts at forging, reusing, or re-routing packets to alternative endpoints
can be detected.

The retry does add an additional network round-trip to the establishment of the
connection, but a necessary one. The cost should be minimal, but there is a
configuration option that disables initial network-path validation. This can be
a good (albeit minor) performance optimization when using QUIC on trusted
network segments.

#### Stateless Reset

Bad things happen. Sometimes a client or server crashes abruptly in the middle
of a connection. When this happens, the `Endpoint` and `Session` will be lost
along with all associated state. Because QUIC is designed to be resilient
across physical network connections, such a crash means the remote peer will
not become immediately aware of the failure. To deal with such situations, QUIC
supports a mechanism called "Stateless Reset", how it works is fairly simple.

Each peer in a QUIC connection uses a unique identifier called a "connection
ID" to identify the QUIC connection. Optionally associated with every
connection ID is a cryptographically generated stateless reset token that can
be reliably and deterministically reproduced by a peer in the event it has lost
all state related to a connection. The peer that has failed will send that
token back to the remote peer discreetly wrapped in a packet that looks just
like any other valid QUIC packet. When the remote peer receives it, it will
recognize the failure that has occured and will shut things down on it's end
also.

Obviously there are some security ramifications of such a mechanism. Packets
that contain stateless reset tokens are not encrypted (they can't be because
the peer lost all of it's state, including the TLS 1.3 packet protection
keys). It is important that Stateless Reset tokens be difficult for an attacker
to guess and incorporate information that only the peers _should_ know.

Stateless Reset is enabled by default, but there is a configuration option that
disables it.

#### Implicit UDP socket binding and graceful close

When an `Endpoint` is created the local UDP socket is not bound until the
`Endpoint` is told to create a new client `Session` or listen for new QUIC
initial packets to create a server `Session`. Unlike `UDPWrap`, which requires
the user-code to explicitly bind the UDP socket before using it, `Endpoint`
will automatically bind when necessary.

When the `Endpoint` is gracefully closed, all existing `Session` instances
associated with the `Endpoint` will be permitted to close naturally. New client
and server `Session` instances won't be created. As soon as the last remaining
`Session` closes and the `Endpoint` receives notification that all UDP packets
that have already been dispatched to the `uv_udp_t` have been sent, the
`Endpoint` will be destroyed and will no longer be usable. It is possible to
abruptly destroy the `Endpoint` when an error occurs, but the default is for
all `Endpoint`s to close gracefully.

The bound `UDP` socket (and the underlying `uv_udp_t` handle) will be closed
automatically when the `Endpoint` is destroyed.

### `Session`

A `Session` encapsulates and manages the local state for either the client or
server side of a connection. More specifically, the `Session` wraps the
`ngtcp2_connection` object. We'll get to that in a bit.

`Session` instances have a fairly large API surface API but they are actually
fairly simple. They are composed of a couple of key elements:

* A `CryptoContext` that encapsulates the state and operation of the TLS 1.3
  handshake.
* A `Session::Application` implementation that handles the QUIC application-
  specific semantics (for instance, the bits specific to HTTP/3), and
* A collection of open `Stream` instances owned by the `Session`.

When a new client `Session` is created, the TLS handshake is started
immediately with a new QUIC initial packet being generated and dispatched to
the owning `Endpoint`. On the server-side, when a new initial packet is
received by the `Endpoint` the server-side of the handshake starts
automatically. This is all handled internally by the `CryptoContext`. Aside
from driving the TLS handshake, the `CryptoContext` maintains the negotiated
packet protection keys used throughout the lifetime of the `Session`.

_Code review note_: This is a key difference between QUIC and a TLS-protected
TCP connection. Specifically, with TCP+TLS, the TLS session is associated with
the actual physical network connection. When the TCP network connection
changes, the TLS session is destroyed. With QUIC, however, a Connection is
independent of the actual network binding. In theory, you could start a QUIC
Connection over a UDP socket and move it to a Unix Domain Socket (for example)
and the TLS session remains unchanged. Note, however, none of this is designed
to work on UDS yet, but there's no reason it couldn't be.

#### The Application

The `Session::Application` implements the application-specific semantics of a
QUIC-based protocol. HTTP/3 is an example. The application for a `Session` is
identified using the TLS 1.3 ALPN extension and is always included as part of
the initial QUIC packet. As soon as the `CryptoContext` has confirmed the TLS
handshake, an appropriate `Session::Application` implementation is selected.
Curently there are two such implementations:

* `DefaultApplication` - Implements generic QUIC protocol handling that defers
  application-specific semantics to user-code in JavaScript. Eseentially, this
  just means receiving and sending stream data as generically as possible
  without applying any application-specific semantics in the `Session`. More on
  this later. Any non-recognized alpn identifier will use the default.
* `Http3Application` - Implements HTTP/3 specific semantics.

Things are designed such that we can add new `Application` implementations in
the future.

#### Streams

Every `Session` owns zero or more `Streams`. These will be covered in greater
detail later. For now it's just important to know that a `Stream` can never
outlive it's owning `Session`. Like the `Endpoint`, a `Session` supports a
graceful shutdown mode that will allow all currently opened `Stream`s to end
naturally before destroying the `Session`. During graceful shutdown, creating
new `Stream`s will not be allowed, and any attempt by the remote peer to start
a new `Stream` will be ignored.

#### Early Data

By design most QUIC packets for a `Session` are encrypted. In typical use, a
`Stream` should only be created once the TLS handshake has been completed and
the encryption keys have been fully negotiated. That, however, does require at
least one full network round trip ('1RTT') before any application data can be
sent. In some cases, that full round trip means additional latency that can be
avoided if you're willing to sacrifice a little bit of security.

QUIC supports what is called 0RTT and 0.5RTT `Stream`s. That is, `Stream`
instances initiated within the initial set of QUIC packets exchanged by the
client and server peers. These will have only limited protection as they are
transmitted before the TLS keys can be negotiated. In the implementation, we
call this "early data".

Client `Session`s support 0RTT early data if they use TLS Session Resumption.

Server `Session`s always support 0.5RTT early sessions. That is, they can
always initiate streams once the TLS handshake has started.

### `Stream`

A `Stream` is a undirectional or bidirectional data flow within a `Session`.
They are conceptually similar to a `Duplex` but are implemented very
differently.

Let's talk a bit about the lifecycle of a `Stream`.

#### Lifecycle

Once a QUIC Connection has been established, either peer is permitted to open
a `Stream`, subject to limits set by the receiving peer. Specifically, the
remote peer sets a limit on the number of open unidirectional and bidirectional
`Stream`s that the peer can concurrently create.

A `Stream` is created by sending data. If no data is sent, the `Stream` is
never created.

As mentioned previous, A `Stream` can be undirectional (data can only be sent
by the peer that initiated it) or bidirectional (data can be sent by both
peers).

Every `Stream` has a numeric identifier that uniquely identifies it only within
the scope of the owning `Session`. The stream ID indentifies whether the stream
was initiated by the client or server, and identifies whether it is
bidirectional or unidirectional.

A peer can transmit data indefinitely on a `Stream`. The final packet of data
will include `FIN` flag that signals the peer is done. Alternatively, the
receiving peer can send a `STOP_SENDING` signal asking the peer to stop. Each
peer can close it's end of the stream independently of the other, and may do so
at different times. There are some complexities involved here relating to flow
control and reliability that we'll get into shortly, but the key things to
remember is that each peer maintains it's own independent view of the `Stream`s
open/close state. For instance, a peer that initiates a unidirectional `Stream`
can consider that `Stream` to be closed immediately after sending the final
chunk of data, even if the remote peer has not fully received or processed that
data.

A `Session` will create a new `Stream` the first time it receives a QUIC
`STREAM` packet with an identifier it has not previously seen.

As with `Endpoint` and `Session`, `Stream` instances support a graceful
shutdown that allow the flow of data to complete naturally before destroying
the `Stream` instance and freeing resources. When an error occurs, however, the
`Stream` can be destroyed abruptly.

When the `Session` receives data for a `Stream`, it will push that data into
the `Stream` for processing. It will do so first by passing the data through
the `Session::Application` so that any Application-protocol specific semantics
can be applied. By the time the `Stream` receives the data, it can be safely
assumed that any Application specific semantics understood by Node.js have been
applied.

As the `Stream` receives data it will either push that directly out to
JavaScript if there is a `'data'` event listener attached, or will store the
data in an internal buffer until a `'data'` listener is attached. A `Stream`
can also be temporarily paused which will cause the inbound data to be buffered
also.

This queue will be retained even after the `Stream`, and even the `Session` has
been closed. The data will be freed only after it is consumed or the `Stream`
is garbage collected and destroyed.

#### Sources and Buffers

A `Source` provides the outbound data that is to be sent by the `Session` for a
`Stream`. They are complicated largely because there are several ways in which
user code might want to sent data, and by the fact that we can never really
know how much `Stream` data is going to be encoded in any given QUIC packet the
`Session` is asked to send at any given time, and by the fact that sent data
must be retained in memory until an explicit acknowledgement has been received
from the remote peer indicating that the data have been successfully received.

Central to implementing a Source is the `Buffer`.

Internally, a `Buffer` maintains zero or more separate chunks of data. These
may or may not be contiguous in memory. Externally, the `Buffer` makes these
chunks of memory _appear_ contiguous.

The `Buffer` maintains two data pointers:

* The `read` pointer identifies the largest offset of data that has been
  encoded at least once into a QUIC packet. When the `read` pointer reaches the
  logical end of the `Buffer`s encapsulated data, the `Buffer`s data has been
  transmitted at least once to the remote peer. However, we're not done with
  the data yet, and we must keep it in memory _without changing the pointers of
  that data in memory_. The ngtcp2 library will automatically handle
  retransmitting ranges of that data if packet loss is suspected.
* The `ack` pointer identifies the largest offset of data that has been
  acknowledged as received by the remote peer. Buffered data can only be freed
  from memory once it has been acknowledged (that is, the ack pointer has moved
  beyond the data offset).

User code may provide all of the data to be sent on a `Stream` synchronously,
all at once, or may space it out asynchronously over time. Regardless of how
the data fills the `Buffer`, it must always be read in a reliable and
consistent way. Because of flow-control, retransmissions, and a range of other
factors, we can never know whenever a QUIC packet is encoded, exactly how much
`Stream` data will be included. To handle this, we provide an API that allows
`Session` and `ngtcp2` to pull data from the `Buffer` on-demand, even if the
`Source` is asynchronously pushing data in.

Sources inherit from `Buffer::Source`. There are a handful of Source types
implemented:

* `NullSource` - Essentially, no data will be provided.
* `ArrayBufferViewSource` - The `Buffer` data is provided all at once in a
  single `ArrayBuffer` or `ArrayBufferView` (that is, any `TypedArray` or
  `DataView`). When `Stream` data is provided as a JavaScript string, this
  Source implementation is also used.
* `BlobSource` - The `Buffer` data is provided all at once in a `node::Blob`
  instance.
* `StreamSource` - The `Buffer` data is provided asynchronously in multiple
  chunks by a JavaScript stream. That can be either a Node.js `stream.Readable`
  or a Web `ReadableStream`.
* `StreamBaseSource` - The `Buffer` data is provided asynchronously in multiple
  chunks by a `node::StreamBase` instance (such as `FileHandle`).

All `Stream` instances are created with no `Source` attached. During the
`Stream`s lifetime, no more than a single `Source` can be attached. Once
attached, the outbound flow of data will begin. Once all of the data provided
by the `Source` has been encoded in QUIC packets transmitted to the peer (even
if not yet acknowledged) the writable side of the `Stream` can be considered to
be closed. The `Stream` and the `Buffer`, however, must be retained in memory
until the proper acknowledgements have been received.

#### A Word On Bob Streams

`Buffer` and `Stream` use a different stream API mechanism than other existing
things in Node.js such as `StreamBase`. This model, affectionately known as
"Bob" is a simple pull stream API defined in the `src/node_bob.h` header.

The Bob API was first conceived in the hallways of one of the first Node.js
Interactive Conferences in Vancouver, British Colombia. It was further
developed by Node.js core contributor Fishrock123 as a separate project, and
integrated into Node.js as part of the initial prototype QUIC implementation.

The API works by pairing a Source with a Sink. The Source provides data, the
Sink consumes it. The Sink will repeatedly call the Source's `Pull()` method
until there is no more data to consume. The API is capable of working in
synchronous and asynchronous modes, and includes backpressure signals for when
the Source does not yet have data to provide.

While `StreamBase` is the more common way in which streaming data is processed
in Node.js, it is just simply not compatible with the way we need to be able to
push data into a QUIC connection.

#### Headers

QUIC Streams are _just_ a stream of bytes. At the protocol level, QUIC has no
concept of headers. However, QUIC applications like HTTP/3 do have a concept
of Headers. We'll explain how exactly HTTP/3 accomplishes that in a bit.

In order to more easily support HTTP/3 and future applications that are known
to support Headers, the `Stream` object includes a generic mechanism for
accumulating blocks of headers associated with the `Stream`. When a `Session`
is using the `DefaultApplication`, the Header APIs will be unused. If the
application supports headers, it will be up to the JavaScript layer to work
that out.

For the `Http3Application` implementation, however, Headers will be processed
and accumulated in the `Stream`. These headers are simple name+value pairs with
additional flags. Three kinds of header blocks are supported:

* `Informational` (or `Hints`) - These correspond to 1xx HTTP response headers.
* `Initial` - Corresponding to HTTP request and response headers.
* `Trailing` - Corresponding to HTTP trailers.

### The Rest

There are a range of additional utility classes and functions used throughout
the implementation. I won't go into every one of those here but I want to
touch on a few of the more critical and visible ones.

#### `BindingState`

The `BindingState` maintains persistent state for the `internalBinding('quic')`
internal module. These include commonly used strings, constructor templates,
and callback functions that are used to push events out to the JavaScript side.
Collecting all of this in the `BindingState` keeps us from having to add a
whole bunch of QUIC specific stuff to the `node::Environment`. You'll see this
used extensively throughout the implementation in `src/quic`.

#### `CID`

Encapsulates a QUIC Connection ID. This is pretty simple. It just makes it
easier to work with a connection identifier.

#### `Packet`

Encapulates an encoded QUIC packet that is to be sent by the `Endpoint`. The
lifecycle here is simple: Create a `Packet`, encode QUIC data into it, pass it
off to the `Endpoint` to send, then destroy it once the `uv_udp_t` indicates
that the packet has been successfully sent.

#### `LogStream`

The `LogStream` is a utility `StreamBase` implementation that pushes diagnostic
keylog and QLog data out to the JavaScript API.

QLog is a QUIC-specific logging format being defined by the IETF working group.
You can read more about it here:

* [https://www.ietf.org/archive/id/draft-ietf-quic-load-balancers-07.html]()
* [https://www.ietf.org/archive/id/draft-ietf-quic-qlog-quic-events-00.html]()
* [https://www.ietf.org/archive/id/draft-ietf-quic-qlog-h3-events-00.html]()

These are disabled by default. A `Session` can be configured to emit diagnostic
logs.

## HTTP/3

[HTTP/3][] layers on top of the QUIC protocol. It essentially just maps HTTP
request and response messages over multiple QUIC streams.

Upon completion of the QUIC TLS handshake, each of the two peers (HTTP client
and server) will create three new unidirectional streams in each direction.
These are "control" streams that manage the state of the HTTP connection. For
each HTTP request, the client will initiate a bidirectional stream with the
server. The HTTP request headers, payload, and trailing footers will be
transmitted using that bidirectional stream with some associated control data
being sent over the unidirectional control streams.

It's important to understand the unidirectional control streams are handled
completely internally by the `Session` and the `Http3Application`. They are
never exposed to the JavaScript API layer.

Fortunately, there is a lot of complexity involved in the implementation of
HTTP/3 that is hidden away and handled by the `nghttp3` dependency.

As we'll see in a bit, at the JavaScript API level there is no HTTP/3 specific
API. It's all just QUIC. The HTTP/3 semantics are applied internally, as
transparently as possible.

## The JavaScript API

The QUIC JavaScript API closely follows the structure laid out by the
internals.

There are the same three fundamental components: `Endpoint`, `Session`, and
`Stream`.

Unlike older Node.js APIs, these build on more modern Web Platform API
compatible pieces such as `EventTarget`. Overall, the API style diverges quite
a bit from the older, more specific `net` module.

```js
const endpointOptions = new EndpointOptions(
  new SocketAddress({ address: '123.123.123.123', port: 1234}),
  {
    maxConnectionsTotal: 100,
    validateAddress: true,
  }
);

const key = getPrivateKeySomehow();
const certs = getCertificateSomehow();

const sessionOptions({
  alpn: 'zzz',
  secure: {
    key,
    certs,
  },
});

const server = new Endpoint(endpointOptions);
server.listen(sessionOptions);
```

### Configuration

One unique characteristics of the QUIC JavaScript API is that there are
distinct "Options" objects that encapuslate all of the configuration options --
and QUIC has a _lot_ of options. By separating these out we make the
implementation much cleaner by separating out option validation, defaults, and
so on. These config objects are backed by objects and structs at the C++ level
that make it more efficient to pass those configurations down to the native
layer.

### Stats

Each of the `Endpoint`, `Session`, and `Stream` objects expose an additional
`stats` property that provides access to statistics actively collected while
the objects are in use. These can be used not only to monitor the performance
and behavior of the QUIC implementation, but also dynamically respond and
adjust to the current load on the endpoints.

[HTTP/3]: https://www.ietf.org/archive/id/draft-ietf-quic-http-34.html
[RFC 8999]: https://www.rfc-editor.org/rfc/rfc8999.html
[RFC 9000]: https://www.rfc-editor.org/rfc/rfc9000.html
[RFC 9001]: https://www.rfc-editor.org/rfc/rfc9001.html
[RFC 9002]: https://www.rfc-editor.org/rfc/rfc9002.html
[draft-ietf-quic-http-34]: https://www.ietf.org/archive/id/draft-ietf-quic-http-34.html
[draft-ietf-quic-qpack-21]: https://www.ietf.org/archive/id/draft-ietf-quic-qpack-21.html
[nghttp3]: https://github.com/ngtcp2/nghttp3
[ngtcp2]: https://github.com/ngtcp2/ngtcp2
