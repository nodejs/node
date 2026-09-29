//===-- Shared memory RPC client / server interface -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a remote procedure call mechanism to communicate between
// heterogeneous devices that can share an address space atomically. We provide
// a client and a server to facilitate the remote call. The client makes
// requests to the server using a shared communication channel. We use separate
// atomic signals to indicate which side, the client or the server is in
// ownership of the buffer.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_RPC_H
#define LLVM_LIBC_SHARED_RPC_H

#include "rpc_util.h"

/// Use scoped atomic variants if they are available for the target.
#if !__has_builtin(__scoped_atomic_load_n)
#ifdef _MSC_VER // MSVC atomic support.
#include <intrin.h>
#define __scoped_atomic_load_n(src, ord, scp)                                  \
  __iso_volatile_load32((const volatile int32_t *)(src))
#define __scoped_atomic_store_n(dst, src, ord, scp)                            \
  (sizeof(*(dst)) == 4                                                         \
       ? __iso_volatile_store32((volatile int32_t *)(dst), (__int32)(src))     \
       : __iso_volatile_store64((volatile int64_t *)(dst), (__int64)(src)))
#define __scoped_atomic_fetch_or(src, val, ord, scp)                           \
  _InterlockedOr((volatile long *)(src), (long)(val))
#define __scoped_atomic_fetch_and(src, val, ord, scp)                          \
  _InterlockedAnd((volatile long *)(src), (long)(val))
#else // GNU atomic support.
#define __scoped_atomic_load_n(src, ord, scp) __atomic_load_n(src, ord)
#define __scoped_atomic_store_n(dst, src, ord, scp)                            \
  __atomic_store_n(dst, src, ord)
#define __scoped_atomic_fetch_or(src, val, ord, scp)                           \
  __atomic_fetch_or(src, val, ord)
#define __scoped_atomic_fetch_and(src, val, ord, scp)                          \
  __atomic_fetch_and(src, val, ord)
#define __scoped_atomic_fetch_add(src, val, ord, scp)                          \
  __atomic_fetch_add(src, val, ord)
#define __scoped_atomic_fetch_sub(src, val, ord, scp)                          \
  __atomic_fetch_sub(src, val, ord)
#endif
#endif
#if !__has_builtin(__scoped_atomic_thread_fence)
#ifdef _MSC_VER
#define __scoped_atomic_thread_fence(ord, scp) _ReadWriteBarrier()
#else
#define __scoped_atomic_thread_fence(ord, scp) __atomic_thread_fence(ord)
#endif
#endif

namespace rpc {

/// Generic codes that can be used when implementing the server.
enum RPCStatus {
  RPC_SUCCESS = 0x0,
  RPC_ERROR = 0x1000,
  RPC_UNHANDLED_OPCODE = 0x1001,
};

/// A fixed size channel used to communicate between the RPC client and server.
struct Buffer {
  uint64_t data[8];
};
static_assert(sizeof(Buffer) == 64, "Buffer size mismatch");

/// A target specific struct containing a doorbell to wake the server-side
/// thread.
struct alignas(64) Doorbell {
  uint64_t *value;
  uint64_t *mailbox;
  uint32_t event_id;
};

/// The information associated with a packet. This indicates which operations to
/// perform and which threads are active in the slots.
struct Header {
  uint64_t mask;
  uint32_t opcode;
};

/// The maximum number of parallel ports that the RPC interface can support.
/// This should be greater than the expected hardware's occupancy to ensure the
/// interface is non-blocking.
constexpr static uint64_t MAX_PORT_COUNT = 16384;

/// A common process used to synchronize communication between a client and a
/// server. The process contains a read-only inbox and a write-only outbox used
/// for signaling ownership of the shared buffer between both sides. We assign
/// ownership of the buffer to the client if the inbox and outbox bits match,
/// otherwise it is owned by the server.
///
/// This process is designed to allow the client and the server to exchange data
/// using a fixed size packet in a mostly arbitrary order using the 'send' and
/// 'recv' operations. The following restrictions to this scheme apply:
///   - The client will always start with a 'send' operation.
///   - The server will always start with a 'recv' operation.
///   - Every 'send' or 'recv' call is mirrored by the other process.
template <bool Invert> struct Process {
  RPC_ATTRS Process() = default;
  RPC_ATTRS Process(const Process &) = delete;
  RPC_ATTRS Process &operator=(const Process &) = delete;
  RPC_ATTRS Process(Process &&) = default;
  RPC_ATTRS Process &operator=(Process &&) = default;
  RPC_ATTRS ~Process() = default;

  const uint32_t port_count = 0;
  Doorbell *const doorbell = nullptr;
  const uint32_t *const inbox = nullptr;
  uint32_t *const outbox = nullptr;
  Header *const header = nullptr;
  Buffer *const packet = nullptr;

  static constexpr uint64_t NUM_BITS_IN_WORD = sizeof(uint32_t) * 8;
  uint32_t lock[MAX_PORT_COUNT / NUM_BITS_IN_WORD] = {0};

  RPC_ATTRS Process(uint32_t port_count, void *buffer)
      : port_count(port_count), doorbell(reinterpret_cast<Doorbell *>(
                                    advance(buffer, doorbell_offset()))),
        inbox(reinterpret_cast<uint32_t *>(
            advance(buffer, inbox_offset(port_count)))),
        outbox(reinterpret_cast<uint32_t *>(
            advance(buffer, outbox_offset(port_count)))),
        header(reinterpret_cast<Header *>(
            advance(buffer, header_offset(port_count)))),
        packet(reinterpret_cast<Buffer *>(
            advance(buffer, buffer_offset(port_count)))) {}

  /// Allocate a memory buffer sufficient to store the following equivalent
  /// representation in memory.
  ///
  /// struct Equivalent {
  ///   Doorbell doorbell;
  ///   Atomic<uint32_t> primary[port_count];
  ///   Atomic<uint32_t> secondary[port_count];
  ///   Header header[port_count];
  ///   Buffer packet[port_count][lane_size];
  /// };
  RPC_ATTRS static constexpr uint64_t allocation_size(uint32_t port_count,
                                                      uint32_t lane_size) {
    return buffer_offset(port_count) + buffer_bytes(port_count, lane_size);
  }

  /// Ring the doorbell if the protocol was configured with one.
  RPC_ATTRS void notify(uint64_t lane_mask) const {
#ifndef _MSC_VER
    if (!doorbell->value)
      return;

    uint32_t event_id = rpc::broadcast_value(lane_mask, doorbell->event_id);
    if (rpc::is_first_lane(lane_mask)) {
      if (!__scoped_atomic_fetch_add(doorbell->value, 1UL, __ATOMIC_RELAXED,
                                     __MEMORY_SCOPE_SYSTEM)) {
        __scoped_atomic_store_n(doorbell->mailbox,
                                static_cast<uint64_t>(doorbell->event_id),
                                __ATOMIC_RELAXED, __MEMORY_SCOPE_SYSTEM);
        __scoped_atomic_thread_fence(__ATOMIC_RELEASE, __MEMORY_SCOPE_SYSTEM);
        signal_interrupt(event_id);
      }
    }
#endif
  }

  /// Decrement the doorbell signal if the protocol is using one.
  RPC_ATTRS void finish(uint64_t lane_mask) const {
#ifndef _MSC_VER
    if (!doorbell->value)
      return;

    if (rpc::is_first_lane(lane_mask))
      __scoped_atomic_fetch_sub(doorbell->value, 1UL, __ATOMIC_RELAXED,
                                __MEMORY_SCOPE_SYSTEM);
#endif
  }

  /// Retrieve the inbox state from memory shared between processes.
  RPC_ATTRS uint32_t load_inbox(uint64_t lane_mask, uint32_t index) const {
    return rpc::broadcast_value(
        lane_mask, __scoped_atomic_load_n(&inbox[index], __ATOMIC_RELAXED,
                                          __MEMORY_SCOPE_SYSTEM));
  }

  /// Retrieve the outbox state from memory shared between processes.
  RPC_ATTRS uint32_t load_outbox(uint64_t lane_mask, uint32_t index) const {
    return rpc::broadcast_value(
        lane_mask, __scoped_atomic_load_n(&outbox[index], __ATOMIC_RELAXED,
                                          __MEMORY_SCOPE_SYSTEM));
  }

  /// Signal to the other process that this one is finished with the buffer.
  /// Equivalent to loading outbox followed by store of the inverted value
  /// The outbox is write only by this warp and tracking the value locally is
  /// cheaper than calling load_outbox to get the value to store.
  RPC_ATTRS uint32_t invert_outbox(uint64_t lane_mask, uint32_t index,
                                   uint32_t current_outbox) {
    uint32_t inverted_outbox = !current_outbox;
    rpc::sync_lane(lane_mask);
    __scoped_atomic_thread_fence(__ATOMIC_RELEASE, __MEMORY_SCOPE_SYSTEM);
    if (rpc::is_first_lane(lane_mask))
      __scoped_atomic_store_n(&outbox[index], inverted_outbox, __ATOMIC_RELAXED,
                              __MEMORY_SCOPE_SYSTEM);
    return inverted_outbox;
  }

  /// Given the current outbox and inbox values, wait until the inbox changes
  /// to indicate that this thread owns the buffer element.
  RPC_ATTRS void wait_for_ownership(uint64_t lane_mask, uint32_t index,
                                    uint32_t out, uint32_t in) {
    while (buffer_unavailable(in, out)) {
      sleep_briefly();
      in = load_inbox(lane_mask, index);
    }
    __scoped_atomic_thread_fence(__ATOMIC_ACQUIRE, __MEMORY_SCOPE_SYSTEM);
  }

  /// The packet is a linearly allocated array of buffers used to communicate
  /// with the other process. This function returns the appropriate slot in this
  /// array such that the process can operate on an entire warp or wavefront.
  RPC_ATTRS Buffer *get_packet(uint32_t index, uint32_t lane_size) {
    return &packet[index * lane_size];
  }

  /// Determines if this process needs to wait for ownership of the buffer. We
  /// invert the condition on one of the processes to indicate that if one
  /// process owns the buffer then the other does not.
  RPC_ATTRS static bool buffer_unavailable(uint32_t in, uint32_t out) {
    bool cond = in != out;
    return Invert ? !cond : cond;
  }

  /// Attempt to claim the lock at index. Return true on lock taken.
  /// lane_mask is a bitmap of the threads in the warp that would hold the
  /// single lock on success, e.g. the result of rpc::get_lane_mask()
  /// The lock is held when the n-th bit of the lock bitfield is set.
  RPC_ATTRS bool try_lock(uint64_t lane_mask, uint32_t index) {
    // On AMDGPU, test and set to the n-th lock bit and a sync_lane would
    // suffice On NVIDIA with ITS we need to handle differences between the
    // threads running and the threads that were detected in the previous call
    // to get_lane_mask()
    //
    // All threads in lane_mask try to claim the lock. At most one can succeed.
    // There may be threads active which are not in lane mask which must not
    // succeed in taking the lock, as otherwise it will leak. This is handled
    // by making threads which are not in lane_mask or with 0, a no-op.
    uint32_t id = rpc::get_lane_id();
    bool id_in_lane_mask = lane_mask & (1ul << id);

    // All threads in the warp call fetch_or. Possibly at the same time.
    bool before = set_nth(lock, index, id_in_lane_mask);
    uint64_t packed = rpc::ballot(lane_mask, before);

    // If every bit set in lane_mask is also set in packed, every single thread
    // in the warp failed to get the lock. Ballot returns unset for threads not
    // in the lane mask.
    //
    // Cases, per thread:
    // mask==0 -> unspecified before, discarded by ballot -> 0
    // mask==1 and before==0 (success), set zero by ballot -> 0
    // mask==1 and before==1 (failure), set one by ballot -> 1
    //
    // mask != packed implies at least one of the threads got the lock
    // atomic semantics of fetch_or mean at most one of the threads for the lock

    // If holding the lock then the caller can load values knowing said loads
    // won't move past the lock. No such guarantee is needed if the lock acquire
    // failed. This conditional branch is expected to fold in the caller after
    // inlining the current function.
    bool holding_lock = lane_mask != packed;
    if (holding_lock)
      __scoped_atomic_thread_fence(__ATOMIC_ACQUIRE, __MEMORY_SCOPE_DEVICE);
    return holding_lock;
  }

  /// Unlock the lock at index. We need a lane sync to keep this function
  /// convergent, otherwise the compiler will sink the store and deadlock.
  RPC_ATTRS void unlock(uint64_t lane_mask, uint32_t index) {
    // Do not move any writes past the unlock.
    __scoped_atomic_thread_fence(__ATOMIC_RELEASE, __MEMORY_SCOPE_DEVICE);

    // Use exactly one thread to clear the nth bit in the lock array Must
    // restrict to a single thread to avoid one thread dropping the lock, then
    // an unrelated warp claiming the lock, then a second thread in this warp
    // dropping the lock again.
    clear_nth(lock, index, rpc::is_first_lane(lane_mask));
    rpc::sync_lane(lane_mask);
  }

  /// Number of bytes to allocate for an inbox or outbox.
  RPC_ATTRS static constexpr uint64_t mailbox_bytes(uint32_t port_count) {
    return port_count * sizeof(uint32_t);
  }

  /// Number of bytes to allocate for the buffer containing the packets.
  RPC_ATTRS static constexpr uint64_t buffer_bytes(uint32_t port_count,
                                                   uint32_t lane_size) {
    return port_count * lane_size * sizeof(Buffer);
  }

  /// The offset to the doorbell interface.
  RPC_ATTRS static constexpr uint64_t doorbell_offset() { return 0; }

  /// Offset of the inbox in memory. This is the same as the outbox if inverted.
  RPC_ATTRS static constexpr uint64_t inbox_offset(uint32_t port_count) {
    return sizeof(Doorbell) + (Invert ? mailbox_bytes(port_count) : 0);
  }

  /// Offset of the outbox in memory. This is the same as the inbox if inverted.
  RPC_ATTRS static constexpr uint64_t outbox_offset(uint32_t port_count) {
    return sizeof(Doorbell) + (Invert ? 0 : mailbox_bytes(port_count));
  }

  /// Offset of the header containing the opcode and mask after the mailboxes.
  RPC_ATTRS static constexpr uint64_t header_offset(uint32_t port_count) {
    return align_up(sizeof(Doorbell) + 2 * mailbox_bytes(port_count),
                    alignof(Header));
  }

  /// Offset of the buffer containing the packets after the inbox and outbox.
  RPC_ATTRS static constexpr uint64_t buffer_offset(uint32_t port_count) {
    return align_up(header_offset(port_count) + port_count * sizeof(Header),
                    alignof(Buffer));
  }

  /// Conditionally set the n-th bit in the atomic bitfield.
  RPC_ATTRS static constexpr uint32_t set_nth(uint32_t *bits, uint32_t index,
                                              bool cond) {
    uint32_t slot = index / NUM_BITS_IN_WORD;
    uint32_t bit = index % NUM_BITS_IN_WORD;
    return __scoped_atomic_fetch_or(&bits[slot],
                                    static_cast<uint32_t>(cond) << bit,
                                    __ATOMIC_RELAXED, __MEMORY_SCOPE_DEVICE) &
           (1u << bit);
  }

  /// Conditionally clear the n-th bit in the atomic bitfield.
  RPC_ATTRS static constexpr uint32_t clear_nth(uint32_t *bits, uint32_t index,
                                                bool cond) {
    uint32_t slot = index / NUM_BITS_IN_WORD;
    uint32_t bit = index % NUM_BITS_IN_WORD;
    return __scoped_atomic_fetch_and(&bits[slot],
                                     ~0u ^ (static_cast<uint32_t>(cond) << bit),
                                     __ATOMIC_RELAXED, __MEMORY_SCOPE_DEVICE) &
           (1u << bit);
  }
};

/// Invokes a function across every active buffer across the total lane size.
template <typename F>
RPC_ATTRS static void invoke_rpc(F &&fn, uint32_t lane_size, uint64_t lane_mask,
                                 Buffer *slot) {
  if constexpr (is_process_gpu()) {
    fn(&slot[rpc::get_lane_id()], rpc::get_lane_id());
  } else {
    for (uint32_t i = 0; i < lane_size; i += rpc::get_num_lanes())
      if (lane_mask & (1ul << i))
        fn(&slot[i], i);
  }
}

/// The port provides the interface to communicate between the multiple
/// processes. A port is conceptually an index into the memory provided by the
/// underlying process that is guarded by a lock bit.
template <bool T> struct Port {
  RPC_ATTRS Port(Process<T> &process, uint64_t lane_mask, uint32_t lane_size,
                 uint32_t index, uint32_t out)
      : process(process), lane_mask(lane_mask), lane_size(lane_size),
        index(index), out(out), receive(false), owns_buffer(true) {}
  RPC_ATTRS ~Port() { close(); }

private:
  RPC_ATTRS Port(const Port &) = delete;
  RPC_ATTRS Port &operator=(const Port &) = delete;
  RPC_ATTRS Port(Port &&) = delete;
  RPC_ATTRS Port &operator=(Port &&) = delete;

  friend struct Client;
  friend struct Server;
  friend struct rpc::optional<Port<T>>;

public:
  template <typename U> RPC_ATTRS void recv(U use);
  template <typename F> RPC_ATTRS void send(F fill);
  template <typename F, typename U> RPC_ATTRS void send_and_recv(F fill, U use);
  template <typename W> RPC_ATTRS void recv_and_send(W work);
  RPC_ATTRS void send_n(const void *const *src, uint64_t *size);
  RPC_ATTRS void send_n(const void *src, uint64_t size);
  template <typename A>
  RPC_ATTRS void recv_n(void **dst, uint64_t *size, A &&alloc);

  template <typename Ty> RPC_ATTRS void send_n(const Ty *src);
  template <typename Ty> RPC_ATTRS void recv_n(Ty *dst);

  RPC_ATTRS uint32_t get_opcode() const { return process.header[index].opcode; }

  RPC_ATTRS uint32_t get_index() const { return index; }

  RPC_ATTRS uint64_t get_lane_mask() const {
    if constexpr (T)
      return process.header[index].mask;
    return lane_mask;
  }

private:
  RPC_ATTRS void close() {
    // Wait for all lanes to finish using the port.
    rpc::sync_lane(lane_mask);

    // The server is passive, if it owns the buffer when it closes we need to
    // give ownership back to the client.
    if (owns_buffer && T)
      out = process.invert_outbox(lane_mask, index, out);
    process.unlock(lane_mask, index);
    if constexpr (T)
      process.finish(lane_mask);
  }

  Process<T> &process;
  uint64_t lane_mask;
  uint32_t lane_size;
  uint32_t index;
  uint32_t out;
  bool receive;
  bool owns_buffer;
};

/// The RPC client used to make requests to the server.
struct Client {
  RPC_ATTRS Client() = default;
  RPC_ATTRS Client(const Client &) = delete;
  RPC_ATTRS Client &operator=(const Client &) = delete;
  RPC_ATTRS ~Client() = default;

  RPC_ATTRS Client(uint32_t port_count, void *buffer)
      : process(port_count, buffer) {}

  using Port = rpc::Port<false>;
  template <uint32_t opcode> RPC_ATTRS Port open();

private:
  Process<false> process;
};

/// The RPC server used to respond to the client.
struct Server {
  RPC_ATTRS Server() = default;
  RPC_ATTRS Server(const Server &) = delete;
  RPC_ATTRS Server &operator=(const Server &) = delete;
  RPC_ATTRS ~Server() = default;

  RPC_ATTRS Server(uint32_t port_count, void *buffer)
      : process(port_count, buffer) {}

  using Port = rpc::Port<true>;
  RPC_ATTRS rpc::optional<Port> try_open(uint32_t lane_size,
                                         uint32_t start = 0);

  RPC_ATTRS static constexpr uint64_t allocation_size(uint32_t lane_size,
                                                      uint32_t port_count) {
    return Process<true>::allocation_size(port_count, lane_size);
  }

  RPC_ATTRS static constexpr uint64_t doorbell_offset() {
    return Process<true>::doorbell_offset();
  }

private:
  Process<true> process;
};

/// Applies \p fill to the shared buffer and initiates a send operation.
template <bool T> template <typename F> RPC_ATTRS void Port<T>::send(F fill) {
  uint32_t in = owns_buffer ? out ^ T : process.load_inbox(lane_mask, index);

  // We need to wait until we own the buffer before sending.
  process.wait_for_ownership(lane_mask, index, out, in);

  // Apply the \p fill function to initialize the buffer and release the memory.
  invoke_rpc(fill, lane_size, get_lane_mask(),
             process.get_packet(index, lane_size));
  out = process.invert_outbox(lane_mask, index, out);
  owns_buffer = false;
  receive = false;
}

/// Applies \p use to the shared buffer and acknowledges the send.
template <bool T> template <typename U> RPC_ATTRS void Port<T>::recv(U use) {
  // We only exchange ownership of the buffer during a receive if we are waiting
  // for a previous receive to finish.
  if (receive) {
    out = process.invert_outbox(lane_mask, index, out);
    owns_buffer = false;
  }

  uint32_t in = owns_buffer ? out ^ T : process.load_inbox(lane_mask, index);

  // We need to wait until we own the buffer before receiving.
  process.wait_for_ownership(lane_mask, index, out, in);

  // Apply the \p use function to read the memory out of the buffer.
  invoke_rpc(use, lane_size, get_lane_mask(),
             process.get_packet(index, lane_size));
  receive = true;
  owns_buffer = true;
}

/// Combines a send and receive into a single function.
template <bool T>
template <typename F, typename U>
RPC_ATTRS void Port<T>::send_and_recv(F fill, U use) {
  send(fill);
  recv(use);
}

/// Combines a receive and send operation into a single function. The \p work
/// function modifies the buffer in-place and the send is only used to initiate
/// the copy back.
template <bool T>
template <typename W>
RPC_ATTRS void Port<T>::recv_and_send(W work) {
  recv(work);
  send([](Buffer *, uint32_t) { /* no-op */ });
}

/// Helper routine to simplify the interface when sending from the GPU using
/// thread private pointers to the underlying value.
template <bool T>
RPC_ATTRS void Port<T>::send_n(const void *src, uint64_t size) {
  const void **src_ptr = &src;
  uint64_t *size_ptr = &size;
  send_n(src_ptr, size_ptr);
}

/// Sends an arbitrarily sized data buffer \p src across the shared channel in
/// multiples of the packet length.
template <bool T>
RPC_ATTRS void Port<T>::send_n(const void *const *src, uint64_t *size) {
  uint64_t num_sends = 0;
  send([&](Buffer *buffer, uint32_t id) {
    reinterpret_cast<uint64_t *>(buffer->data)[0] = lane_value(size, id);
    num_sends = is_process_gpu() ? lane_value(size, id)
                                 : rpc::max(lane_value(size, id), num_sends);
    uint64_t len =
        lane_value(size, id) > sizeof(Buffer::data) - sizeof(uint64_t)
            ? sizeof(Buffer::data) - sizeof(uint64_t)
            : lane_value(size, id);
    rpc_memcpy(&buffer->data[1], lane_value(src, id), len);
  });
  uint64_t idx = sizeof(Buffer::data) - sizeof(uint64_t);
  uint64_t mask = process.header[index].mask;
  while (rpc::ballot(mask, idx < num_sends)) {
    send([=](Buffer *buffer, uint32_t id) {
      uint64_t len = lane_value(size, id) - idx > sizeof(Buffer::data)
                         ? sizeof(Buffer::data)
                         : lane_value(size, id) - idx;
      if (idx < lane_value(size, id))
        rpc_memcpy(buffer->data, advance(lane_value(src, id), idx), len);
    });
    idx += sizeof(Buffer::data);
  }
}

/// Receives an arbitrarily sized data buffer across the shared channel in
/// multiples of the packet length. The \p alloc function is called with the
/// size of the data so that we can initialize the size of the \p dst buffer.
template <bool T>
template <typename A>
RPC_ATTRS void Port<T>::recv_n(void **dst, uint64_t *size, A &&alloc) {
  uint64_t num_recvs = 0;
  recv([&](Buffer *buffer, uint32_t id) {
    lane_value(size, id) = reinterpret_cast<uint64_t *>(buffer->data)[0];
    lane_value(dst, id) =
        reinterpret_cast<uint8_t *>(alloc(lane_value(size, id)));
    num_recvs = is_process_gpu() ? lane_value(size, id)
                                 : rpc::max(lane_value(size, id), num_recvs);
    uint64_t len =
        lane_value(size, id) > sizeof(Buffer::data) - sizeof(uint64_t)
            ? sizeof(Buffer::data) - sizeof(uint64_t)
            : lane_value(size, id);
    rpc_memcpy(lane_value(dst, id), &buffer->data[1], len);
  });
  uint64_t idx = sizeof(Buffer::data) - sizeof(uint64_t);
  uint64_t mask = process.header[index].mask;
  while (rpc::ballot(mask, idx < num_recvs)) {
    recv([=](Buffer *buffer, uint32_t id) {
      uint64_t len = lane_value(size, id) - idx > sizeof(Buffer::data)
                         ? sizeof(Buffer::data)
                         : lane_value(size, id) - idx;
      if (idx < lane_value(size, id))
        rpc_memcpy(advance(lane_value(dst, id), idx), buffer->data, len);
    });
    idx += sizeof(Buffer::data);
  }
}

/// Simplified version of `send_n` where the size is a known constant.
template <bool T>
template <typename Ty>
RPC_ATTRS void Port<T>::send_n(const Ty *src) {
  for (uint64_t idx = 0; idx < sizeof(Ty); idx += sizeof(Buffer::data)) {
    const uint64_t bytes = rpc::min(sizeof(Ty) - idx, sizeof(Buffer::data));
    send([&](Buffer *buffer, uint32_t id) {
      rpc_memcpy(buffer->data, advance(&lane_value(src, id), idx), bytes);
    });
  }
}

/// Simplified version of `recv_n` where the size is a known constant.
template <bool T>
template <typename Ty>
RPC_ATTRS void Port<T>::recv_n(Ty *dst) {
  for (uint64_t idx = 0; idx < sizeof(Ty); idx += sizeof(Buffer::data)) {
    const uint64_t bytes = rpc::min(sizeof(Ty) - idx, sizeof(Buffer::data));
    recv([&](Buffer *buffer, uint32_t id) {
      rpc_memcpy(advance(&lane_value(dst, id), idx), buffer->data, bytes);
    });
  }
}

/// Continually attempts to open a port to use as the client. The client can
/// only open a port if we find an index that is in a valid sending state. That
/// is, there are send operations pending that haven't been serviced on this
/// port. Each port instance uses an associated \p opcode to tell the server
/// what to do. The Client interface provides the appropriate lane size to the
/// port using the platform's returned value.
template <uint32_t opcode> RPC_ATTRS Client::Port Client::open() {
  // Repeatedly perform a naive linear scan for a port that can be opened to
  // send data.
  for (uint32_t index = 0;; ++index) {
    // Start from the beginning if we run out of ports to check.
    if (index >= process.port_count)
      index = 0;

    // Attempt to acquire the lock on this index. Under NVIDIA's ITS the lanes
    // may reconverge with differing index values, ensure they are convergent.
    uint64_t lane_mask = rpc::get_lane_mask();
    index = rpc::broadcast_value(lane_mask, index);
    if (!process.try_lock(lane_mask, index))
      continue;

    uint32_t in = process.load_inbox(lane_mask, index);
    uint32_t out = process.load_outbox(lane_mask, index);

    // Once we acquire the index we need to check if we are in a valid sending
    // state.
    if (process.buffer_unavailable(in, out)) {
      process.unlock(lane_mask, index);
      continue;
    }

    if (rpc::is_first_lane(lane_mask)) {
      process.header[index].opcode = opcode;
      process.header[index].mask = lane_mask;
    }
    rpc::sync_lane(lane_mask);

    process.notify(lane_mask);
    return Port(process, lane_mask, rpc::get_num_lanes(), index, out);
  }
}

/// Attempts to open a port to use as the server. The server can only open a
/// port if it has a pending receive operation
RPC_ATTRS rpc::optional<typename Server::Port>
Server::try_open(uint32_t lane_size, uint32_t start) {
  if (rpc::get_lane_id() >= lane_size)
    return rpc::nullopt;

  // Perform a naive linear scan for a port that has a pending request.
  for (uint32_t index = start; index < process.port_count; ++index) {
    uint64_t lane_mask = rpc::get_lane_mask();
    uint32_t in = process.load_inbox(lane_mask, index);
    uint32_t out = process.load_outbox(lane_mask, index);

    // The server is passive, if there is no work pending don't bother
    // opening a port.
    if (process.buffer_unavailable(in, out))
      continue;

    // Attempt to acquire the lock on this index.
    if (!process.try_lock(lane_mask, index))
      continue;

    in = process.load_inbox(lane_mask, index);
    out = process.load_outbox(lane_mask, index);

    if (process.buffer_unavailable(in, out)) {
      process.unlock(lane_mask, index);
      continue;
    }

    return rpc::optional<Port>(rpc::in_place, process, lane_mask, lane_size,
                               index, out);
  }
  return rpc::nullopt;
}

#if !__has_builtin(__scoped_atomic_load_n)
#undef __scoped_atomic_load_n
#undef __scoped_atomic_store_n
#undef __scoped_atomic_fetch_or
#undef __scoped_atomic_fetch_and
#undef __scoped_atomic_fetch_add
#undef __scoped_atomic_fetch_sub
#endif
#if !__has_builtin(__scoped_atomic_thread_fence)
#undef __scoped_atomic_thread_fence
#endif

} // namespace rpc

#endif // LLVM_LIBC_SHARED_RPC_H
