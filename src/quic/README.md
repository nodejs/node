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

HTTP/3 is an application of the HTTP protocol semantics on top of QUIC. The
two are not exactly the same thing, however. It is possible (and will be
common) to implement applications of QUIC that have nothing to do with
HTTP/3, but HTTP/3 will always be implemented on top of QUIC.

At the time I'm writing this, the QUIC RFC's have been finished, but the
HTTP/3 specification is still under development. I'd recommend also reading
through these draft specifications before continuing on:

* [draft-ietf-quic-http-34][]: Hypertext Transfer Protocol Version 3 (HTTP/3)
* [draft-ietf-quic-qpack-21][]: QPACK: Header Compression for HTTP/3

The IETF working group is working on a number of other documents that you'll
also want to get familiar with. Check those out here:

* [https://datatracker.ietf.org/wg/quic/documents/]()

This guide will first deal with explaining QUIC and the QUIC implementation in
general, and then will address HTTP/3.

### So if QUIC is not HTTP/3, what is it?

QUIC is a stateful, connection-oriented, client-server UDP protocol that
includes flow-control, multiplexed streams, network-path migration,
low-latency connection establishment, and integrated TLS 1.3.

A QUIC connection is always initiated by the client and starts with a
handshake phase that includes a TLS 1.3 CLIENT-HELLO and a set of configuration
parameters that QUIC calls "transport parameters". [RFC 9001][] details exactly
how QUIC uses TLS 1.3.

The TLS handshake establishes cryptographic keys that will be used to encrypt
all QUIC protocol data that is passed back and forth. As I will explain in a
bit, it is possible for the client and server to initiate communication before
these keys have been fully negotiated, but for now we'll ignore that and focus
on the fully-protected case.

After the TLS 1.3 handshake is completed, and the packet protection keys have
been established, the QUIC protocol primarily consists of opening undirectional
or bidirectional streams of data between the client and server. An important
characteristic is that streams can be opened by *either* of the two peers.

Unsurprisingly, a unidirectional stream allows sending data in only one
direction on the connection. The initiating peer transmits the data, the
other peer receives it. Bidirectional streams allow both endpoints to send
and receive data. QUIC streams are nothing more than a sequence of bytes.
There are no headers, no trailers, just a sequence of octets that are
spread out of one or more QUIC packets encoded into one or more UDP packets.

The simplistic life cycle of a QUIC connection, then, is:

* Initiate the connection with a TLS Handshake.
* Open one or more streams to transmit data.
* Close the connection when done transmitting data.

Nice and simple, right? Well, there's a bit more that happens in there.

QUIC includes built-in reliability and flow-control mechanisms. Because
UDP is notoriously unreliable, every QUIC packet sent by an endpoint is
identified by a monotonically increasing packet number. Each endpoint
keeps track of which packets it has received and sends an acknowledgement
to the other endpoint. If the sending endpoint does not receive an
acknowledgement for a packet it has sent within some specified period of
time, then the endpoint will assume the packet was lost and will retransmit
it. As I'll show later, this causes some complexity in the implementation
when it comes to how long data must be retained in memory.

For flow-control, QUIC implements separate congestion windows (cwnd) for
both the overall connection *and* per individual stream. To keep it simple,
the receiving endpoint tells the sending endpoint how much additional data
it is willing to accept right now. The sending endpoint could choose to
send more but the receiving endpoint could also choose to ignore that
additional data or even close the connection if the sender does not is not
cooperating.

There are also a number of built in mechanisms that endpoints can use to
validate that a usable network path exists between the endpoints. This is
a particularly important and unique characteristic of QUIC given that it
is built on UDP. Unlike TCP, where a connection establishes a persistent
flow of data that is tightly bound to a specific network path, UDP traffic
is much more flexible. With QUIC, it is possible to start a connection on
one network connection but transition it to another (for instance, when
a mobile device changes from a WiFi connection to a mobile data connection).
When such a transition happens, the QUIC endpoints must verify that they
can still successfully send packets to each other securely.

All of this adds additional complexity to the protocol implementation.
With TCP, which also includes flow control, reliability, and so forth,
Node.js can rely on the operating system handling everything. QUIC,
however, is designed to be implementable in user-space -- that is, it is
designed such that, to the kernel, it looks like ordinary UDP traffic.
This is a good thing, in general, but it means the Node.js implementation
needs to be quite a bit more complex than "regular" UDP datagrams or
TCP connections that are abstracted behind libuv APIs and operating
system syscalls.

There are lots of details we could go into but for now, let's turn the
focus to how the QUIC implementation here is structured and how it
operates.

## Code organization

Thankfully for us, there are open source libraries that implement most of the
complex details of QUIC packet serialization and deserialization, flow control,
data loss, connection state management, and more. We've chosen to use the
[ngtcp2][] and [nghttp3][] libraries. These can be found in the `deps` folder
along with the other vendored-in dependencies.

The directory in which you've found this README.md (`src/quic`) contains the
C++ code that bridges `ngtcp2` and `nghttp3` into the Node.js environment.
These provide the *internal* API surface and underlying implementation that
supports the public JavaScript API that can be found in `lib/internal/quic`.

So, in summary:

* `deps/ngtcp2` and `deps/nghttp3` provide the low-level protocol
  implementation.
* `src/quic` provides the Node.js-specific I/O, state management, and
  internal API.
* `lib/internal/quic` provides the public JavaScript API.

Documentation for the public API can be found in `doc/api/quic.md`, and tests
can be found in `test/parallel/test-quic-*.js`

## The Internals

The Node.js QUIC implementation is built around a model that tracks closely
with the key elements of the protocol. There are three main components:

* The `Endpoint` represents a binding to a local UDP socket. The `Endpoint`
  is ultimately responsible for:
  * Transmitting and receiving UDP packets, and verifying that received packets
    are valid QUIC packets; and
  * Creating new client and server `Sessions`.
* A `Session` represents either the client-side or server-side of a QUIC
  connection. The `Session` is responsible for persisting the current state
  of the connection, including the TLS 1.3 packet protection keys, the current
  set of open streams, and handling the serialization/deserialization of QUIC
  packes.
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

First, and most importantly, the QUIC `Endpoint` class is designed to be
*shareable across Node.js Worker Threads*.

Within the internal API, and `Endpoint` actually consists of three primary
elements:

* An `node::quic::Endpoint::UDP` object that directly wraps the `uv_udp_t`
  handle.
* An `node::quic::Endpoint` object that owns the `Endpoint::UDP` instance and
  performs all of the protocol-specific work.
* And an `node::quic::EndpointWrap` object that inherits from `BaseObject` and
  connects the `Endpoint` to a specific Node.js `Environment`/`v8::Isolate`
  (remember, the Node.js main thread, and each individual Worker Thread, all
  have their own separate `Environment` and `v8::Isolate`).

I'm going to simplify things a bit here and say that, in the following
description, whenever I say "thread" that applies to both the Node.js main
thread and to Worker Threads.

When user-code creates an `Endpoint` in JavaScript (we'll explore the
JavaScript API later), Node.js creates a new `node::quic::EndpointWrap`
object in the current thread. That new `node::quic::EndpointWrap` will
create a new `node::quic::Endpoint` instance that is held by a stdlib
`std::shared_ptr`. The `node::quic::EndpointWrap` will register itself
as a listener for various events emitted by the `node::quic::Endpoint`.

Using the Channel Messaging API (e.g. `MessageChannel` and `MessagePort`),
or `BroadcastChannel`, it is possible to create a pseudo-clone of the
`node::quic::EndpointWrap` within a different thread that shares a
`std::shared_ptr` reference to the same `node::quic::Endpoint` reference.

What I mean by "pseudo-clone" here is that the newly created
`node::quic::EndpointWrap` in the new thread will not share or copy
any other state with the original `node::quic::EndpointWrap` beyond
the shared `node::quic::Endpoint`. Each `node::quic::EndpointWrap` will
maintain it's own separate set of client and server `Session` instances
and are capable of operating largely independent of each other in their
separate event loops.

When the `node::quic::Endpoint::UDP` receives a UDP packet, it will
immediately forward that on to the owning `node::quic::Endpoint`. The
`node::quic::Endpoint` will perform some basic validation checks on the
packet to determine first, whether it is acceptable as a QUIC packet and
second, whether it is a new initial packet (intended to create a new
QUIC connection) or a packet for an already established QUIC connection.

If the received packet is an initial packet, the `node::quic::Endpoint`
will forward the packet to the first available listening
`node::quic::EndpointWrap` that is willing to accept it. Essentially,
this means that while a UDP packet will be accepted by the event loop
in the thread in which the original `Endpoint` was created, the handling
of that packet may occur in a completely different thread. Once a
`node::quic::EndpointWrap` has accepted an initial packet, it will
notify the `node::quic::Endpoint` that all further packets received for
that connection are to be forwarded directly to it.

*Code Review Note*: Currently, this cross-thread packet handoff uses a
queue locked with a mutex. We can explore whether a non-locking data
structure would perform better.

When there is an outbound QUIC packet to be sent, the `node::quic::EndpointWrap`
will forward that on to the shared `node::quic::Endpoint` for transmission.
Again, this means that the actual network I/O occurs in the originating
thread.

This cross-thread sharing is not supported by the existing `UDPWrap` and
is the main why `UDPWrap` is not used. It is not the only reason, however.
`UDPWrap` exposes additional configuration options on the UDP socket that
simply do not make sense with QUIC and exposes an API surface that is not
needed with QUIC (`UDPWrap` is largely geared towards giving user-code
direct access to the UDP socket which is something that we never want with
QUIC).

The shared `node::quic::Endpoint` will be destroyed either when all
`node::quic::EndpointWrap` instances holding a reference to it are destroyed
or the owning thread is terminated. If the owning thread is terminated and
the `node::quic::Endpoint` is destroyed while there are still active
`node::quic::EndpointWrap` instances holding a reference to it, those will
be notified and closed immediately, terminating all associated `Session`s.
Likewise, if there is an error reading or writing from the UDP socket, the
`node.quic::Endpoint` will be destroyed.

#### Network-Path Validation

Aside from managing the lifecycle of the UDP handle, and managing the routing
of packets to and from `node::quic::EndpointWrap` instances, the
`node::quic::Endpoint` is also responsible for performing initial network
path validation and keeping track of validated remote socket addresses.

By default, the first time the `node::quic::Endpoint` receives an initial
QUIC packet from a peer, it will respond first with a "retry challenge".
A retry is a special QUIC packet that contains a cryptographic token that
the client peer must receive and return in subsequent packets in order to
demonstrate that the network path between the two peers is viable. Once
the path has been verified, the `node::quic::Endpoint` uses an LRU cache
to remember the socket address so that it can skip validation on future
connections. That cache is transient and only persists as long as the
`node::quic::Endpoint` instance exists. Also, the cache is not shared among
multiple `node::quic::Endpoint` instances, so the path verification may be
repeated.

The retry token is cryptographically generated and incorporates protections
that protect it against abuse or forgery. For the client peer, the token is
opaque and should appear random. For the server that created it, however,
the token will incorporate a timestamp and the remote socket address so that
attempts at forging, reusing, or re-routing packets to alternative endpoints
can be detected.

The retry does add an additional network round-trip to the establishment
of the connection, but a necessary one. The cost should be minimal, but there
is a configuration option that disables initial network-path validation.
This can be a good (albeit minor) performance optimization when using QUIC
on trusted network segments.

#### Stateless Reset

Bad things happen. Sometimes a client or server crashes abruptly in the middle
of a connection. When this happens, the `Endpoint` and `Session` will be lost
along with all associated state. Because QUIC is designed to be resilient
across physical network connections, such a crash does not automatically mean
that the remote peer will become immediately aware of the failure. To deal
with such situations, QUIC supports a mechanism called "Stateless Reset",
how it works is fairly simple.

Each peer in a QUIC connection uses a unique identifier called a "connection ID"
to identify the QUIC connection. Optionally associated with every connection ID
is a cryptographically generated stateless reset token that can be reliably
and deterministically reproduced by a peer in the event it has lost all state
related to a connection. The peer that has failed will send that token back to
the remote peer discreetly wrapped in a packet that looks just like any other
valid QUIC packet. When the remote peer receives it, it will recognize the
failure that has occured and will shut things down on it's end also.

Obviously there are some security ramifications of such a mechanism.
Packets that contain stateless reset tokens are not encrypted (they can't be
because the peer lost all of it's state, including the TLS 1.3 packet
protection keys). It is important that Stateless Reset tokens be difficult
for an attacker to guess and incorporate information that only the peers
*should* know.

Stateless Reset is enabled by default, but there is a configuration option that
disables it.

#### Implicit UDP socket binding and graceful close

When an `Endpoint` is created the local UDP socket is not bound until the
`Endpoint` is told to create a new client `Session` or listen for new
QUIC initial packets to create a server `Session`. Unlike `UDPWrap`, which
requires the user-code to explicitly bind the UDP socket before using it,
`Endpoint` will automatically bind when necessary.

When user code calls `endpoint.close()` in the JavaScript API, all existing
`Session` instances associated with the `node::quic::EndpointWrap` will be
permitted to close naturally. New client and server `Session` instances won't
be created. As soon as the last remaining `Session` closes and the `Endpoint`
receives notification that all UDP packets that have already been dispatched
to the `uv_udp_t` have been sent, the `Endpoint` will be destroyed and will
no longer be usable. It is possible to abruptly destroy the `Endpoint` when
an error occurs, but the default is for all `Endpoint`s to close gracefully.

The bound `UDP` socket (and the underlying `uv_udp_t` handle) will be closed
automatically when the `Endpoint` is destroyed.

### `Session`

A `Session` represents one-half of a QUIC Connection. It encapsulates and
manages the local state for either the client or server side of a connection.
More specifically, the `Session` wraps the `ngtcp2_connection` object. We'll
get to that in a bit.

Unlike `Endpoint`, a `Session` is always limited to a single thread. It is
always associated with an owning `node::quic::EndpointWrap`. `Session`
instances cannot be cloned or transferred to other threads. When the owning
`node::quic::EndpointWrap` is destroyed, all owned `Session`s are also
destroyed.

`Session` instances have a fairly large API surface API but they are actually
fairly simple. They are composed of a couple of key elements:

* A `node::quic::Session::CryptoContext` that encapsulates the state and
  operation of the TLS 1.3 handshake.
* A `node::quic::Session::Application` implementation that handles the
  QUIC application-specific semantics (for instance, the bits specific to
  HTTP/3), and
* A collection of open `Stream` instances owned by the `Session`.

When a new client `Session` is created, the TLS handshake is started
immediately with a new QUIC initial packet being generated and dispatched
to the owning `node::quic::EndpointWrap`. On the server-side, when a new
initial packet is received by the `node::quic::EndpointWrap` the server-side
of the handshake starts automatically. This is all handled internally by the
`node::quic::Session::CryptoContext`. Aside from driving the TLS handshake,
the `CryptoContext` maintains the negotiated packet protection keys used
throughout the lifetime of the `Session`.

*Code review note*: This is a key difference between QUIC and a TLS-protected
TCP connection. Specifically, with TCP+TLS, the TLS session is associated with
the actual physical network connection. When the TCP network connection changes,
the TLS session is destroyed. With QUIC, however, a Connection is independent of
the actual network binding. In theory, you could start a QUIC Connection over a
UDP socket and move it to a Unix Domain Socket (for example) and the TLS session
remains unchanged. Note, however, none of this is designed to work on UDS yet,
but there's no reason it couldn't be.

#### The Application

Every `Session` uses QUIC to implement an Application. HTTP/3 is an example.
The application for a `Session` is identified using the TLS 1.3 ALPN extension
and is always included as part of the initial QUIC packet. As soon as the
`CryptoContext` has confirmed the TLS handshake, an appropriate
`node::quic::Session::Application` implementation is selected. Curently there
are two such implementations:

* `node::quic::DefaultApplication` - Implements generic QUIC protocol handling
  that defers application-specific semantics to user-code in JavaScript.
  Eseentially, this just means receiving and sending stream data as generically
  as possible without applying any application-specific semantics in the `Session`.
  More on this later.
* `node::quic::Http3Application` - Implements HTTP/3 specific semantics.

Things are designed such that we can add new `Application` implementations at
any time in the future.

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
client and server peers. These will have only limited protection as they
are transmitted before the TLS keys can be negotiated. In the implementation,
we call this "early data".

### `Stream`

A `Stream` is a undirectional or bidirectional data flow within a `Session`.
They are conceptually similar to a `Duplex` but are implemented very
differently.

There are two main elements of a `Stream`: the `Source` and the `Consumer`.
Before we get into the details of each, let's talk a bit about the lifecycle of
a `Stream`.

#### Lifecycle

Once a QUIC Connection has been established, either peer is permitted to open a
`Stream`, subject to limits set by the receiving peer. Specifically, the remote
peer sets a limit on the number of open unidirectional and bidirectional
`Stream`s that the peer can concurrently create.

A `Stream` is created by sending data. If no data is sent, the `Stream` is never
created.

As mentioned previous, A `Stream` can be undirectional (data can only be sent by
the peer that initiated it) or bidirectional (data can be sent by both peers).

Every `Stream` has a numeric identifier that uniquely identifies it only within
the scope of the owning `Session`. The stream ID indentifies whether the stream
was initiated by the client or server, and identifies whether it is
bidirectional or unidirectional.

A peer can transmit data indefinitely on a `Stream`. The final packet of data
will include `FIN` flag that signals the peer is done. Alternatively, the
receiving peer can send a `STOP_SENDING` signal asking the peer to stop.
Each peer can close it's end of the stream independently of the other, and
may do so at different times. There are some complexities involved here
relating to flow control and reliability that we'll get into shortly, but the
key things to remember is that each peer maintains it's own independent view
of the `Stream`s open/close state. For instance, a peer that initiates a
unidirectional `Stream` can consider that `Stream` to be closed immediately
after sending the final chunk of data, even if the remote peer has not fully
received or processed that data.

A `node::quic::Session` will create a new `node::quic::Stream` the first time
it receives a QUIC `STREAM` packet with a `Stream` identifier it has not
previously seen.

As with `Endpoint` and `Session`, `Stream` instances support a graceful shutdown
that allow the flow of data to complete naturally before destroying the
`node::quic::Stream` instance and freeing resources. When an error occurs,
however, the `Stream` can be destroyed abruptly.

#### Consumers

When the `Session` receives data for a `Stream`, it will push that data into
the `Stream` for processing. It will do so first by passing the data through
the `Application` so that any Application-protocol specific semantics can be
applied. By the time the `node::quic::Stream` receives the data, it can be
safely assumed that any Application specific semantics understood by Node.js
have been applied.

If a `node::quic::Buffer::Consumer` instance has been attached to the
`node::quic::Stream` when the data is received, the `node::quic::Stream`
will immediately forward that data on to the `node::quic::Buffer::Consumer`.
If a `node::quic::Buffer::Consumer` has not been attached, the received
data will be accumulated in an internal queue until a Consumer is attached.

This queue will be retained even after the `Stream`, and even the `Session`
has been closed. The data will be freed only after it is consumed or the
`Stream` is garbage collected and destroyed.

Currently, there is only one `node::quic::Buffer::Consumer` implemented
(the `node::quic::JSQuicBufferConsumer`). All it does is take the received
chunks of data and forwards them out as `ArrayBuffer` instances to the
JavaScript API. We'll explore how those are used later when we discuss the
details of the JavaScript API.

#### Sources and Buffers

`Source`s are quite a bit more complicated than Consumers.

A `Source` provides the outbound data that is to be sent by the `Session` for
a `Stream`. They are complicated largely because there are several ways in
which user code might want to sent data, and by the fact that we can never
really know how much `Stream` data is going to be encoded in any given QUIC
packet the `Session` is asked to send at any given time, and by the fact that
sent data must be retained in memory until an explicit acknowledgement has
been received from the remote peer indicating that the data have been
successfully received.

Core to implementing a Source is the `node::quic::Buffer` object. For
simplicity, I'll just refer to this a `Buffer` from now on.

Internally, a `Buffer` maintains zero or more separate chunks of data.
These may or may not be contiguous in memory. Externally, the `Buffer`
makes these chunks of memory *appear* contiguous.

The `Buffer` maintains two data pointers:

* The `read` pointer identifies the largest offset of data that has been
  encoded at least once into a QUIC packet. When the `read` pointer reaches
  the logical end of the `Buffer`s encapsulated data, the `Buffer`s data
  has been transmitted at least once to the remote peer. However, we're
  not done with the data yet, and we must keep it in memory *without changing
  the pointers of that data in memory*. The ngtcp2 library will automatically
  handle retransmitting ranges of that data if packet loss is suspected.
* The `ack` pointer identifies the largest offset of data that has been
  acknowledged as received by the remote peer. Buffered data can only be
  freed from memory once it has been acknowledged (that is, the ack pointer
  has moved beyond the data offset).

User code may provide all of the data to be sent on a `Stream` synchronously,
all at once, or may space it out asynchronously over time. Regardless of how
the data fills the `Buffer`, it must always be read in a reliable and consistent
way. Because of flow-control, retransmissions, and a range of other factors, we
can never know whenever a QUIC packet is encoded, exactly how much `Stream`
data will be included. To handle this, we provide an API that allows `Session`
and `ngtcp2` to pull data from the `Buffer` on-demand, even if the `Source`
is asynchronously pushing data in.

Sources inherit from `node::quic::Buffer::Source`. There are a handful of
Source types implemented:

* `node::quic::NullSource` - Essentially, no data will be provided.
* `node::quic::ArrayBufferViewSource` - The `Buffer` data is provided all at
  once in a single `ArrayBuffer` or `ArrayBufferView` (that is, any
  `TypedArray` or `DataView`). When `Stream` data is provided as a JavaScript
  string, this Source implementation is also used.
* `node::quic::BlobSource` - The `Buffer` data is provided all at once in a
  `node::Blob` instance.
* `node::quic::StreamSource` - The `Buffer` data is provided asynchronously
  in multiple chunks by a JavaScript stream. That can be either a Node.js
  `stream.Readable` or a Web `ReadableStream`.
* `node::quic::StreamBaseSource` - The `Buffer` data is provided asynchronously
  in multiple chunks by a `node::StreamBase` instance.

All `Stream` instances are created with no `Source` attached. During the
`Stream`s lifetime, no more than a single `Source` can be attached. Once
attached, the outbound flow of data will begin. Once all of the data provided
by the `Source` has been encoded in QUIC packets transmitted to the peer
(even if not yet acknowledged) the writable side of the `Stream` can be
considered to be closed. The `Stream` and the `Buffer`, however, must be
retained in memory until the proper acknowledgements have been received.

*Code Review Note*: The `Session` also uses `Buffer` during the TLS 1.3
handshake to retain handshake data in memory until it is acknowledged.

#### A Word On Bob Streams

`Buffer` and `Stream` use a different stream API mechanism than other existing
things in Node.js such as `StreamBase`. This model, affectionately known
as "Bob" is a simple pull stream API defined in the `src/node_bob.h` header.

The Bob API was first conceived in the hallways of one of the first Node.js
Interactive Conferences in Vancouver, British Colombia. It was further
developed by Node.js core contributor Fishrock123 as a separate project,
and integrated into Node.js as part of the initial prototype QUIC
implementation.

The API works by pairing a Source with a Sink. The Source provides data,
the Sink consumes it. The Sink will repeatedly call the Source's `Pull()`
method until there is no more data to consume. The API is capable of
working in synchronous and asynchronous modes, and includes backpressure
signals for when the Source does not yet have data to provide.

While `StreamBase` is the more common way in which streaming data is processed
in Node.js, it is just simply not compatible with the way we need to be able
to push data into a QUIC connection.

#### Headers

QUIC Streams are *just* a stream of bytes. At the protocol level, QUIC has
no concept of headers. However, QUIC applications like HTTP/3 do have a
concept of Headers. We'll explain how exactly HTTP/3 accomplishes that
in a bit.

In order to more easily support HTTP/3 and future applications that are
known to support Headers, the `node::quic::Stream` object includes a
generic mechanism for accumulating blocks of headers associated with the
`Stream`. When a `Session` is using the `node::quic::DefaultApplication`,
the Header APIs will be unused. If the application supports headers, it
will be up to the JavaScript layer to work that out.

For the `node::quic::Http3Application` implementation, however, Headers
will be processed and accumulated in the `node::quic::Stream`. These headers
are simple name+value pairs with additional flags. Three kinds of header
blocks are supported:

* `Informational` (or `Hints`) - These correspond to 1xx HTTP response
  headers.
* `Initial` - Corresponding to HTTP request and response headers.
* `Trailing` - Corresponding to HTTP trailers.

### Putting it all together

* An `Endpoint` is created. This causes a `node::quic::EndpointWrap` to be
  created, which in turn wraps a shared `node::quic::Endpoint`, which owns
  a `node:quic::Endpoint::UDP`.
* A client `Session` is created. This creates a `node::quic::Session` that
  is owned by the `node::quic::Endpoint`. The `node::quic:Session` uses it's
  `node::quic::Session::CryptoContext` to kick of the TLS handshake.
* When the TLS handshake is completed, the `node::quic::Session::Application`
  will be selected by looking at the ALPN TLS extension. Whenever the
  `node::quic::EndpointWrap` receives a QUIC packet, it is passed to the
  appropriate `node::quic::Session` and routed into the
  `node::quic::Session::Application` for application specific handling.
* Whenever a QUIC packet is to be sent, the `node::quic::Session::Application`
  is responsible for preparing the packet with application specific handling
  enforced. The created packets are then passed down to the
  `node::quic::EndpointWrap` for transmission.
* The `Session` is used to create a new `Stream`, which creates an underlying
  `node::quic::Stream`.
* As soon as a `Source` is attached to the `Stream`, the `Session` will start
  transmitting any available data for the `Stream`.
* As the `Session` receives data from the remote peer, that is passed through
  the `Application` and on to the internal queue inside the `Stream` until
  a `Consumer` is attached.

### The Rest

There are a range of additional utility classes and functions used throughout
the implementation. I won't go into every one of those here but I want to
touch on a few of the more critical and visible ones.

#### `node::quic::BindingState`

The `node::quic::BindingState` maintains persistent state for the
`internalBinding('quic')` internal module. These include commonly
used strings, constructor templates, and callback functions that
are used to push events out to the JavaScript side. Collecting all
of this in the `BindingState` keeps us from having to add a whole
bunch of QUIC specific stuff to the `node::Environment`. You'll
see this used extensively throughout the implementation in `src/quic`.

#### `node::quic::CID`

Encapsulates a QUIC Connection ID. This is pretty simple. It just
makes it easier to work with a connection identifier.

#### `node::quic::Packet`

Encapulates an encoded QUIC packet that is to be sent by the `Endpoint`.
The lifecycle here is simple: Create a `node::quic::Packet`, encode
QUIC data into it, pass it off to the `node::quic::Endpoint` to send,
then destroy it once the `uv_udp_t` indicates that the packet has been
successfully sent.

#### `node::quic::RoutableConnectionIDStrategy`

By default, QUIC connection IDs are random byte sequences and they are generally
considered to be opaque, carrying no identifying information about either of the
two peers. However, the IETF is working on
[draft-ietf-quic-load-balancers-07][], a specification for creating "Routable
Connection IDs" that make it possible for intelligent QUIC intermedaries to
encode routing information within a Connection ID.

At the time I am writing this, I have not yet fully implemented support for this
draft QUIC extension, but the fundamental mechanism necessary to support it has
been started in the form of the `node::quic::RoutableConnectionIDStrategy`.

When fully implemented, this will allow alternative strategies for creating
Connection IDs to be plugged in by user-code.

#### `node::quic::LogStream`

The `node::quic::LogStream` is a utility `StreamBase` implementation that pushes
diagnostic keylog and QLog data out to the JavaScript API.

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
These are "control" streams that manage the state of the HTTP connection.
For each HTTP request, the client will initiate a bidirectional stream with
the server. The HTTP request headers, payload, and trailing footers will be
transmitted using that bidirectional stream with some associated control data
being sent over the unidirectional control streams.

It's important to understand the unidirectional control streams are handled
completely internally by the `node::quic::Session` and the
`node::quic::Http2Application`. They are never exposed to the JavaScript
API layer.

HTTP/3 supports the notion of push streams. These are essentially *assumed*
HTTP requests that are initiated by the server as part of an associated
HTTP client request. With QUIC, these are implemented as QUIC unidirectional
`Stream` instances initiated by the server.

Fortunately, there is a lot of complexity involved in the implementation of
HTTP/3 that is hidden away and handled by the `nghttp3` dependency.

As we'll see in a bit, at the JavaScript API level there is no HTTP/3 specific
API. It's all just QUIC. The HTTP/3 semantics are applied internally, as
transparently as possible.

## The JavaScript API

The QUIC JavaScript API closely follows the structure laid out by the internals.

There are the same three fundamental components: `Endpoint`, `Session`, and
`Stream`.

Unlike older Node.js APIs, these build on more modern Web Platform API
compatible pieces such as `EventTarget`, Web Streams, and Promises. Overall,
the API style diverges quite a bit from the older, more specific `net` module.

The best way to get familiar with the JavaScript API is to read the user docs in
`doc/api/quic.md`. Here I'll just hit a few of the high points.

`Endpoint` is the entry point to the entire API. Whether you are creating a
QUIC client or server, you start by creating an `Endpoint`. An `Endpoint` object
can be used to create client `Session` instances or listen for new server
`Session` instances and essentially nothing else.

`Session` instances will use events and promises to communicate various
lifecycle events and states, but otherwise is just used to initiate new `Stream`
instances or receive peer-initiated `Stream`s.

`Stream` instances allow sending and receiving data. It's really that simple.
Where a QUIC `Stream` instance differs from other Node.js APIs like `Socket`
is that a QUIC `Stream` can be consumed as either a legacy Node.js
`stream.Readable` or a Web-compatible `ReadableStream`. The API has been kept
intentionally very limited.

At the JavaScript layer, there is no HTTP/3 specific API layer. It's all just
the same set of objects. The `Stream` object has some getters to get HTTP
headers that are received. Those getters just return undefined if the underlying
QUIC application does not support headers. Everything else remains the same.
This was a very intentional decision to keep us from having introduce Yet
Another HTTP API for Node.js given that the HTTP/1 and HTTP/2 implementations
were largely forced to define their own distinct APIs.

### Configuration

Another of the unique characteristics of the QUIC JavaScript API is that there
are distinct "Config" objects that encapuslate all of the configuration options
-- and QUIC has a *lot* of options. By separating these out we make the
implementation much cleaner by separating out option validation, defaults,
and so on. These config objects are backed by objects and structs at the C++
level that make it more efficient to pass those configurations down to the
native layer.

### Stats

Each of the `Endpoint`, `Session`, and `Stream` objects expose an additional
`stats` property that provides access to statistics actively collected while
the objects are in use. These can be used not only to monitor the performance
and behavior of the QUIC implementation, but also dynamically respond and
adjust to the current load on the endpoints.

### TLS and QUIC

One of the more important characteristics of QUIC is that TLS 1.3 is built
directly into the protocol. The initial QUIC packet sent by a client is
always a TLS CLIENT_HELLO.

In our implementation, the TLS details are encapsulated in the
`node::quic::Session::CryptoContext` defined in `quic/session.h` and
implemented in `quic/session.cc`, with helper functions provided by
`quic/crypto.h`/`quic/crypto.cc`. There are also helpful support utilities
provided by the `ngtcp2` dependency.

If you're already familiar with the existing TLSWrap in Node.js, it is worth
pointing out that the TLS implementation in QUIC is entirely different and
not at all the same. Currently both implemetations make use of the
`node::crypto::SecureContext` class to provide the configuration details
for the TLS session but that's about where the similarities end.

With QUIC, when an initial QUIC packet containing a CLIENT_HELLO is received,
that is first given to `ngtcp2` for processing. The dependency will perform
some processing then let us know through a callback that some amount of
crypto data extracted from the packet was received. That crypto data is fed
directly into OpenSSL to start the TLS handshake process. OpenSSL will do
it's thing and will produce a response SERVER_HELLO if the crypto data is
valid. The outbound crypto data will be stored in a `node::quic::Buffer`
and serialized out into a QUIC packet.

In the simplest of cases, a TLS handshake can be completed in a single network
round trip. It is possible, however, for the client or server to apply
additional TLS extensions (such as the server asking the client for certificate
authentication or certificate status using OCSP) that could extend the
handshake out over multiple messages.

On the server-side, the implementation allows user code to hook the start
of the TLS handshake to provide a different TLS `SecureContext` if it
desires based on the initial information in the CLIENT_HELLO, or to
provide a response to an OCSP request. Those hooks will halt the completion
of the TLS handshake until the user code explicitly resumes it.

At the JavaScript layer, it is possible to enable TLS key log output that
can be fed into a tool like Wireshark to analyze the QUIC traffic. Unlike
the existing `tls` module in Node.js, which emits keylogs as events, the
QUIC implementation uses a `node.Readable` to push those out, making it
easier to pipe those out to a file for consumption.

[HTTP/3]: https://www.ietf.org/archive/id/draft-ietf-quic-http-34.html
[RFC 8999]: https://www.rfc-editor.org/rfc/rfc8999.html
[RFC 9000]: https://www.rfc-editor.org/rfc/rfc9000.html
[RFC 9001]: https://www.rfc-editor.org/rfc/rfc9001.html
[RFC 9002]: https://www.rfc-editor.org/rfc/rfc9002.html
[draft-ietf-quic-http-34]: https://www.ietf.org/archive/id/draft-ietf-quic-http-34.html
[draft-ietf-quic-load-balancers-07]: https://www.ietf.org/archive/id/draft-ietf-quic-load-balancers-07.html
[draft-ietf-quic-qpack-21]: https://www.ietf.org/archive/id/draft-ietf-quic-qpack-21.html
[nghttp3]: https://github.com/ngtcp2/nghttp3
[ngtcp2]: https://github.com/ngtcp2/ngtcp2
