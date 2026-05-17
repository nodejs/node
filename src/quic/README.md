# Node.js QUIC Implementation (`src/quic/`)

This directory contains the C++ implementation of the Node.js experimental QUIC
support (`--experimental-quic`). The implementation builds on three external
libraries: **ngtcp2** (QUIC transport), **nghttp3** (HTTP/3 framing), and
**OpenSSL** (TLS 1.3).

## Architecture Overview

The stack is layered as:

```text
┌─────────────────────────────────────────────┐
│  JavaScript API  (lib/internal/quic/)       │
├─────────────────────────────────────────────┤
│  Endpoint      — UDP socket, packet I/O     │
│  Session       — QUIC connection (ngtcp2)   │
│  Application   — ALPN protocol logic        │
│  Stream        — Bidirectional data flow    │
├─────────────────────────────────────────────┤
│  ngtcp2 / nghttp3 / OpenSSL                 │
├─────────────────────────────────────────────┤
│  libuv         — UDP, timers, thread pool   │
└─────────────────────────────────────────────┘
```

An **Endpoint** binds a UDP socket and dispatches incoming packets to
**Sessions**. Each Session wraps an `ngtcp2_conn` and delegates
protocol-specific behavior to an **Application** (selected by ALPN
negotiation). Sessions contain **Streams** — bidirectional or unidirectional
data channels that carry application data.

## File Map

### Foundation

| File          | Purpose                                                            |
| ------------- | ------------------------------------------------------------------ |
| `guard.h`     | OpenSSL QUIC guard macro                                           |
| `defs.h`      | Core enums, typedefs, constants, macros                            |
| `arena.h`     | Block-based arena allocator (header-only template)                 |
| `data.h/cc`   | `Path`, `PathStorage`, `Store`, `QuicError`                        |
| `cid.h/cc`    | `CID` — Connection ID with hash, factory, map alias                |
| `tokens.h/cc` | `TokenSecret`, `StatelessResetToken`, `RetryToken`, `RegularToken` |

### Security

| File                 | Purpose                                                     |
| -------------------- | ----------------------------------------------------------- |
| `tlscontext.h/cc`    | `TLSContext`, `TLSSession` — OpenSSL integration, SNI, ALPN |
| `sessionticket.h/cc` | `SessionTicket` — TLS 1.3 session resumption and 0-RTT      |

### Core

| File               | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `endpoint.h/cc`    | `Endpoint` — UDP binding, packet dispatch, retry/validation  |
| `session.h/cc`     | `Session` — QUIC connection state machine (\~3,500 lines)    |
| `streams.h/cc`     | `Stream`, `Outbound`, `PendingStream` — data flow            |
| `application.h/cc` | `Session::Application` base + `DefaultApplication`           |
| `http3.h/cc`       | `Http3ApplicationImpl` — nghttp3 integration (\~1,400 lines) |

### Infrastructure

| File                    | Purpose                                                       |
| ----------------------- | ------------------------------------------------------------- |
| `bindingdata.h/cc`      | `BindingData` — JS binding state, callback scopes, allocators |
| `session_manager.h/cc`  | `SessionManager` — per-Realm CID→Session routing              |
| `transportparams.h/cc`  | `TransportParams` — QUIC transport parameter encoding         |
| `packet.h/cc`           | `Packet` — arena-allocated outbound packets                   |
| `preferredaddress.h/cc` | `PreferredAddress` — server preferred address helper          |
| `quic.cc`               | Module entry point (binding registration)                     |

## Key Design Patterns

### SendPendingDataScope (RAII Send Coalescing)

Every entry point that may generate outbound data creates a
`SendPendingDataScope`. Scopes nest — an internal depth counter ensures
`Application::SendPendingData()` is called exactly once, when the outermost
scope exits:

```cpp
{
  SendPendingDataScope outer(session);   // depth 1
  {
    SendPendingDataScope inner(session); // depth 2
    // ... generate data ...
  }                                      // depth 1 — no send yet
}                                        // depth 0 — SendPendingData() fires
```

This is used in `Session::Receive`, `Endpoint::Connect`, `Session::Close`,
`Session::ResumeStream`, and all stream write operations.

### NgTcp2CallbackScope / NgHttp3CallbackScope

Per-session RAII guards that prevent re-entrant calls into ngtcp2/nghttp3.
While active, `can_send_packets()` returns false, blocking the send loop.
If `Destroy()` is called during a callback (e.g., via JS `MakeCallback`),
destruction is deferred until the scope exits, preventing use-after-free.

### Bob Protocol (Pull-Based Streaming)

Data flows through the stack using the **bob** (Bytes-Over-Buffers) pull
protocol defined in `src/node_bob.h`. The consumer calls `Pull()` on a
source, which responds with one of four status codes:

| Status                | Meaning                                                          |
| --------------------- | ---------------------------------------------------------------- |
| `STATUS_EOS` (0)      | End of stream — no more data                                     |
| `STATUS_CONTINUE` (1) | Data delivered; pull again                                       |
| `STATUS_BLOCK` (2)    | No data now; try later                                           |
| `STATUS_WAIT` (3)     | Async — source will invoke the `next` callback when data arrives |

The `Done` callback passed with each pull signals that the consumer is
finished with the buffer memory, enabling zero-copy transfer.

### Three-Phase Buffer Lifecycle

Data in `Stream::Outbound` moves through three states:

```text
Pulled (uncommitted) → Committed (in-flight) → Acknowledged (freed)
```

* **Uncommitted**: Read from the DataQueue but not yet accepted by ngtcp2
* **Committed**: Accepted into a QUIC packet by `ngtcp2_conn_writev_stream`
* **Acknowledged**: Peer ACKed the data; buffer memory is released

Separate cursors on each buffer entry track progression. This allows ngtcp2
to retry uncommitted data (e.g., after pacing/congestion clears) without
re-reading from the source.

### Application Abstraction

`Session::Application` is a virtual interface that the Session delegates
ALPN-specific behavior to. Two implementations exist:

* **`DefaultApplication`** (`application.cc`): Used for non-HTTP/3 ALPN
  protocols. Maintains its own stream scheduling queue. Streams are scheduled
  via an intrusive linked list.

* **`Http3ApplicationImpl`** (`http3.cc`): Used when ALPN negotiates `h3`.
  Wraps `nghttp3_conn` for HTTP/3 framing, header compression (QPACK),
  server push, and stream prioritization. Manages unidirectional control
  streams internally.

The Application is selected during ALPN negotiation — immediately for
clients (ALPN known upfront), during the `OnSelectAlpn` TLS callback for
servers.

### Thread-Local Allocator

Both ngtcp2 and nghttp3 require custom allocators (`ngtcp2_mem`,
`nghttp3_mem`). These allocator structs must outlive every object they
create. Some nghttp3 objects (notably `rcbuf`s backing V8 external strings)
can survive past `BindingData` destruction during isolate teardown.

The solution uses `thread_local` storage:

```cpp
struct QuicAllocState {
    BindingData* binding = nullptr;  // Nulled in ~BindingData
    ngtcp2_mem ngtcp2;
    nghttp3_mem nghttp3;
};
thread_local QuicAllocState quic_alloc_state;
```

Each allocation prepends its size before the returned pointer. This allows
`free` and `realloc` to report correct sizes for memory tracking. When
`binding` is null (after `BindingData` destruction), allocations still
succeed but memory tracking is silently skipped.

## Session Lifecycle

### Creation

**Client**: `Endpoint::Connect()` builds a `Session::Config` with
`Side::CLIENT`, creates a `TLSContext`, and calls `Session::Create()` →
`ngtcp2_conn_client_new()`. The Application is selected immediately.

**Server**: `Endpoint::Receive()` processes an Initial packet through
address validation (retry tokens, LRU cache), then calls `Session::Create()`
→ `ngtcp2_conn_server_new()`. The Application is selected later, during ALPN
negotiation in the TLS handshake.

### The Receive Path

```text
uv_udp_recv_cb
 → Endpoint::Receive()
    → FindSession(dcid)           // CID lookup across endpoints
       ├── Found → Session::Receive()
       └── Not found:
           ├── Stateless reset?   → process
           ├── Short header?      → SendStatelessReset()
           └── Long header?       → acceptInitialPacket()
                ├── ngtcp2_accept()
                ├── Address validation (retry tokens, LRU)
                └── Session::Create()

Session::Receive()
 → SendPendingDataScope           // will send after processing
 → NgTcp2CallbackScope            // re-entrancy guard
 → ngtcp2_conn_read_pkt()         // decrypt, process frames
    triggers callbacks:
     ├── recv_stream_data → Application::ReceiveStreamData()
     ├── stream_open      → Application::ReceiveStreamOpen()
     ├── acked_stream_data → Application::AcknowledgeStreamData()
     ├── handshake_completed → Session::HandshakeCompleted()
     └── ... others
 → Application::PostReceive()     // deferred operations (e.g., GOAWAY)
```

### The Send Loop

```text
SendPendingDataScope::~SendPendingDataScope()
 → Application::SendPendingData()
    Loop (up to max_packet_count):
     ├── GetStreamData()           // pull data from next stream
     │   └── stream->Pull()        // bob pull from Outbound→DataQueue
     ├── WriteVStream()            // ngtcp2_conn_writev_stream()
     │                              encrypts, frames, paces
     ├── if ndatalen > 0: StreamCommit()
     │                              stream->Commit(datalen, fin)
     ├── if nwrite > 0:  Send()    // uv_udp_send()
     ├── if WRITE_MORE:  continue  // room for more in this packet
     ├── if STREAM_DATA_BLOCKED:   // flow control
     │   StreamDataBlocked(), continue
     └── if nwrite == 0:           // pacing/congestion
         ResumeStream() if data pending, return
    On exit: UpdateTimer(), UpdateDataStats()
```

When `nwrite == 0` and the stream had unsent data (payload or FIN), the
stream is re-scheduled via `Application::ResumeStream()` so the next
timer-triggered `SendPendingData` retries it.

### Close Methods

| Method       | Behavior                                                    |
| ------------ | ----------------------------------------------------------- |
| **DEFAULT**  | Destroys all streams, sends CONNECTION\_CLOSE, emits to JS  |
| **SILENT**   | Same but skips CONNECTION\_CLOSE (errors, stateless resets) |
| **GRACEFUL** | Sends GOAWAY (H3), waits for streams to close naturally     |

### Timer

`Session::UpdateTimer()` queries `ngtcp2_conn_get_expiry()` and sets a libuv
timer. When it fires, `OnTimeout()` calls `ngtcp2_conn_handle_expiry()` then
`SendPendingData()` to retransmit lost packets, send PINGs, or retry
pacing-blocked sends.

## Stream Lifecycle

### Creation

**Local streams**: `Session::OpenStream()` calls
`ngtcp2_conn_open_bidi_stream()` or `ngtcp2_conn_open_uni_stream()`. If the
handshake is incomplete or the concurrency limit is reached, the stream is
created in **pending** state and queued. When the peer grants capacity
(`ExtendMaxStreams`), pending streams are fulfilled with real stream IDs.

**Remote streams**: ngtcp2 notifies via callbacks. The Application creates a
`Stream` object and emits it to JavaScript.

### Outbound Data Flow

The `Stream::Outbound` class bridges a `DataQueue` (the data source) to
ngtcp2's packet-writing loop. A `DataQueue::Reader` provides the bob
pull interface.

Supported body source types (via `GetDataQueueFromSource`):

| Source              | Strategy                                     |
| ------------------- | -------------------------------------------- |
| `ArrayBuffer`       | Zero-copy detach, or copy if non-detachable  |
| `SharedArrayBuffer` | Always copy                                  |
| `ArrayBufferView`   | Zero-copy detach of underlying buffer        |
| `Blob`              | Slice of Blob's existing DataQueue           |
| `String`            | UTF-8 encode into BackingStore               |
| `FileHandle`        | `FdEntry` — async file reads via thread pool |

For `FileHandle` bodies, the `FdEntry::ReaderImpl` dispatches `uv_fs_read`
to the libuv thread pool and returns `STATUS_WAIT`. When the read completes,
the callback appends data to the Outbound buffer and calls
`session().ResumeStream(id)` to re-enter the send loop.

### Inbound Data Flow

Received stream data is delivered by ngtcp2 via
`Application::ReceiveStreamData()`, which calls `stream->ReceiveData()`.
Data is appended to the stream's inbound `DataQueue`. The JavaScript side
consumes this via an async iterator (the `stream/iter` `bytes()` helper).
The stream implements `DataQueue::BackpressureListener` to extend the
QUIC flow control window as data is consumed.

### Streaming Mode (Writer API)

When no body is provided at stream creation, the JavaScript `stream.writer`
API uses streaming mode. The Outbound creates a non-idempotent DataQueue.
Each `writeSync()` / `write()` call appends an in-memory entry. The
`endSync()` / `end()` call caps the queue, signaling EOS to the send loop.

## SessionManager

The `SessionManager` is a per-Realm singleton that owns the authoritative
CID→Session mapping. It enables:

* **Cross-endpoint routing**: A session's CIDs are registered globally so
  packets arriving on any endpoint find the right session.
* **Connection migration**: When a session migrates to a new path, the
  SessionManager updates the routing without requiring endpoint-specific
  knowledge.
* **Stateless reset token mapping**: Maps reset tokens to sessions for
  detecting stateless resets on any endpoint.

CID lookup uses a three-tier strategy:

1. Direct SCID match in `SessionManager::sessions_`
2. Cross-endpoint DCID→SCID in `SessionManager::dcid_to_scid_`
3. Per-endpoint DCID→SCID in `Endpoint::dcid_to_scid_` (peer-chosen CIDs)

## Address Validation

The endpoint uses an LRU cache to track validated remote addresses. For
unvalidated addresses:

1. If no token is present, a **Retry** packet is sent with a cryptographic
   token (HKDF-derived, time-limited).
2. The client retransmits the Initial with the retry token.
3. The token is validated; the session is created with the original DCID
   preserved for transport parameter verification.

Regular tokens (from `NEW_TOKEN` frames) follow the same validation path
but without the retry handshake. The LRU cache allows subsequent
connections from the same address to skip validation entirely.

## HTTP/3 Application (`http3.cc`)

The `Http3ApplicationImpl` wraps `nghttp3_conn` and handles:

* **Header compression**: QPACK encoding/decoding via nghttp3's internal
  encoder/decoder streams (unidirectional).
* **Stream management**: Only bidirectional streams are exposed to
  JavaScript. Unidirectional control, encoder, and decoder streams are
  managed internally.
* **FIN management**: `stream_fin_managed_by_application()` returns true.
  nghttp3 controls when FIN is sent based on HTTP/3 framing (DATA frames,
  trailing HEADERS). The `EndWriting()` notification from JavaScript is
  forwarded to `nghttp3_conn_shutdown_stream_write()`.
* **Data read callback**: `on_read_data_callback` pulls data from the
  stream's Outbound during `nghttp3_conn_writev_stream`. Bytes must be
  committed inside the callback (before `StreamCommit`) because QPACK can
  cause re-entrant `read_data` calls.
* **GOAWAY**: `BeginShutdown()` sends a GOAWAY frame. The goaway ID is
  deferred to `PostReceive()` (outside callback scopes) so it can safely
  invoke JavaScript.
* **Settings**: HTTP/3 SETTINGS (max field section size, QPACK capacities,
  CONNECT protocol, datagrams) are negotiated and enforced. Datagram
  support follows RFC 9297 — when the peer's SETTINGS disable datagrams,
  `sendDatagram()` is blocked.
* **0-RTT**: Early data settings are validated during ticket extraction
  (`ValidateTicketData` in `ExtractSessionTicketAppData`). If the server's
  settings changed incompatibly, the ticket is rejected before TLS accepts
  it.

## Error Handling

`QuicError` (`data.h`) encapsulates QUIC error codes with a type namespace
(transport, application, version negotiation, idle close). Factory methods
wrap ngtcp2 error codes, TLS alerts, and application errors.

On the JavaScript side, `convertQuicError()` transforms the C++ error
representation into `ERR_QUIC_TRANSPORT_ERROR` or
`ERR_QUIC_APPLICATION_ERROR` objects. Clean closes (transport NO\_ERROR,
H3 NO\_ERROR, or idle close) resolve `stream.closed`; all other errors
reject it.

## Packet Allocation

Outbound packets are allocated from an `ArenaPool<Packet>` owned by the
Endpoint. The arena provides O(1) allocation from contiguous memory blocks
(128 slots per block), avoiding per-packet heap allocation and V8 object
overhead. Packets are returned to the pool when the UDP send completes
(via the `Packet::Listener::PacketDone` callback).

## Debug Logging

Use the `NODE_DEBUG_NATIVE` environment variable to enable detailed debug
logging:

* `QUIC` - general QUIC events (sessions, streams, packets)
* `NGTCP2` - ngtcp2 callback events and error codes
* `NGHTTP3` - nghttp3 callback events and error codes

```bash
NODE_DEBUG_NATIVE=QUIC,NGTCP2,NGHTTP3 node --experimental-quic ...
```

The debug output will be printed to stderr and can be extremely verbose.

Used in combination with `qlog` and `keylog` options when creating a
`QuicSession`, this can help significantly with debugging and understanding
QUIC behavior and identifying bugs / performance issues in the implementation.

## External Dependencies

| Library | Role                                    | Location                  |
| ------- | --------------------------------------- | ------------------------- |
| ngtcp2  | QUIC transport protocol                 | `deps/ngtcp2/ngtcp2/`     |
| nghttp3 | HTTP/3 framing, QPACK                   | `deps/ngtcp2/nghttp3/`    |
| OpenSSL | TLS 1.3 handshake, encryption           | system or `deps/openssl/` |
| libuv   | UDP sockets, timers, thread pool        | `deps/uv/`                |
| V8      | JavaScript engine, GC, external strings | `deps/v8/`                |
