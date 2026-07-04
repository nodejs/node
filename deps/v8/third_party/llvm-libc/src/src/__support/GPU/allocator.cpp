//===-- GPU memory allocator implementation ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a parallel allocator intended for use on a GPU device.
// The core algorithm is slab allocator using a random walk over a bitfield for
// maximum parallel progress. Slab handling is done by a wait-free reference
// counted guard. The first use of a slab will create it from system memory for
// re-use. The last use will invalidate it and free the memory.
//
//===----------------------------------------------------------------------===//

#include "allocator.h"

#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/new.h"
#include "src/__support/GPU/fixedbuffer.h"
#include "src/__support/GPU/utils.h"
#include "src/__support/RPC/rpc_client.h"
#include "src/__support/threads/sleep.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {

constexpr static uint64_t MAX_SIZE = /* 64 GiB */ 64ull * 1024 * 1024 * 1024;
constexpr static uint64_t SLAB_SIZE = /* 2 MiB */ 2ull * 1024 * 1024;
constexpr static uint64_t ARRAY_SIZE = MAX_SIZE / SLAB_SIZE;
constexpr static uint64_t SLAB_ALIGNMENT = SLAB_SIZE - 1;
constexpr static uint32_t BITS_IN_WORD = sizeof(uint32_t) * 8;
constexpr static uint32_t BITS_IN_DWORD = sizeof(uint64_t) * 8;
constexpr static uint32_t MIN_SIZE = 16;
constexpr static uint32_t MIN_ALIGNMENT = MIN_SIZE - 1;

// The number of times to attempt claiming an in-progress slab allocation.
constexpr static uint32_t MAX_TRIES = 128;

// The number of previously allocated slabs we will keep in memory.
constexpr static uint32_t CACHED_SLABS = 8;

// Configuration for whether or not we will return unused slabs to memory.
constexpr static bool RECLAIM = true;

static_assert(!(ARRAY_SIZE & (ARRAY_SIZE - 1)), "Must be a power of two");

namespace impl {
// Allocates more memory from the system through the RPC interface. All
// allocations from the system MUST be aligned on a 2MiB barrier. The default
// HSA allocator has this behavior for any allocation >= 2MiB and the CUDA
// driver provides an alignment field for virtual memory allocations.
static void *rpc_allocate(uint64_t size) {
  void *ptr = nullptr;
  rpc::Client::Port port = rpc::client.open<LIBC_MALLOC>();
  port.send_and_recv(
      [=](rpc::Buffer *buffer, uint32_t) { buffer->data[0] = size; },
      [&](rpc::Buffer *buffer, uint32_t) {
        ptr = reinterpret_cast<void *>(buffer->data[0]);
      });
  return ptr;
}

// Deallocates the associated system memory.
static void rpc_free(void *ptr) {
  rpc::Client::Port port = rpc::client.open<LIBC_FREE>();
  port.send([=](rpc::Buffer *buffer, uint32_t) {
    buffer->data[0] = reinterpret_cast<uintptr_t>(ptr);
  });
}

// Convert a potentially disjoint bitmask into an increasing integer per-lane
// for use with indexing between gpu lanes.
static inline uint32_t lane_count(uint64_t lane_mask, uint32_t id) {
  return cpp::popcount(lane_mask & ((uint64_t(1) << id) - 1));
}

// Obtain an initial value to seed a random number generator. We use the rounded
// multiples of the golden ratio from xorshift* as additional spreading.
static inline uint32_t entropy() {
  return (static_cast<uint32_t>(gpu::processor_clock() | 1u) ^
          gpu::get_thread_id_x() ^ (gpu::get_block_id_x() * 0x9e3779b9u));
}

// Generate a random number and update the state using the xorshift32* PRNG.
static inline uint32_t xorshift32(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state * 0x9e3779bb;
}

// Rounds the input value to the closest permitted chunk size. Here we accept
// the sum of the closest three powers of two. For a 2MiB slab size this is 48
// different chunk sizes. This gives us average internal fragmentation of 87.5%.
static inline constexpr uint32_t get_chunk_size(uint32_t x) {
  uint32_t y = x < MIN_SIZE ? MIN_SIZE : x;
  uint32_t pow2 = BITS_IN_WORD - cpp::countl_zero(y - 1);

  uint32_t s0 = 0b0100 << (pow2 - 3);
  uint32_t s1 = 0b0110 << (pow2 - 3);
  uint32_t s2 = 0b0111 << (pow2 - 3);
  uint32_t s3 = 0b1000 << (pow2 - 3);

  if (s0 > y)
    return (s0 + MIN_ALIGNMENT) & ~MIN_ALIGNMENT;
  if (s1 > y)
    return (s1 + MIN_ALIGNMENT) & ~MIN_ALIGNMENT;
  if (s2 > y)
    return (s2 + MIN_ALIGNMENT) & ~MIN_ALIGNMENT;
  return (s3 + MIN_ALIGNMENT) & ~MIN_ALIGNMENT;
}

// Converts a chunk size into an index suitable for a statically sized array.
static inline constexpr uint32_t get_chunk_id(uint32_t x) {
  if (x <= MIN_SIZE)
    return 0;
  uint32_t y = x >> 4;
  if (x < MIN_SIZE << 2)
    return cpp::popcount(y);
  return cpp::popcount(y) + 3 * (BITS_IN_WORD - cpp::countl_zero(y)) - 7;
}

// Perform a lane parallel memset on a uint32_t pointer.
static inline void uniform_memset(uint32_t *s, uint32_t c, uint32_t n,
                                  uint64_t lane_mask) {
  uint32_t workers = cpp::popcount(lane_mask);
  for (uint32_t i = impl::lane_count(lane_mask, gpu::get_lane_id()); i < n;
       i += workers)
    s[i] = c;
}

// Indicates that the provided value is a power of two.
static inline constexpr bool is_pow2(uint64_t x) {
  return x && (x & (x - 1)) == 0;
}

// Where this chunk size should start looking in the global array. Small
// allocations are much more likely than large ones, so we give them the most
// space. We use a cubic easing function normalized on the possible chunks.
static inline constexpr uint32_t get_start_index(uint32_t chunk_size) {
  constexpr uint32_t max_chunk = impl::get_chunk_id(SLAB_SIZE / 2);
  uint64_t norm =
      (1 << 16) - (impl::get_chunk_id(chunk_size) << 16) / max_chunk;
  uint64_t bias = (norm * norm * norm) >> 32;
  uint64_t inv = (1 << 16) - bias;
  return static_cast<uint32_t>(((ARRAY_SIZE - 1) * inv) >> 16);
}

// Returns the id of the lane below this one that acts as its leader.
static inline uint32_t get_leader_id(uint64_t ballot, uint32_t id) {
  uint64_t mask = id < BITS_IN_DWORD - 1 ? ~0ull << (id + 1) : 0;
  return BITS_IN_DWORD - cpp::countl_zero(ballot & ~mask) - 1;
}

// We use a sentinal value to indicate a failed or in-progress allocation.
template <typename T> bool is_sentinel(const T &x) {
  if constexpr (cpp::is_pointer_v<T>)
    return reinterpret_cast<uintptr_t>(x) ==
           cpp::numeric_limits<uintptr_t>::max();
  else
    return x == cpp::numeric_limits<T>::max();
}

// Returns the current lane's position in the lane mask.
uint64_t id_in_mask() { return 1ull << gpu::get_lane_id(); }

} // namespace impl

/// A slab allocator used to hand out identically sized slabs of memory.
/// Allocation is done through random walks of a bitfield until a free bit is
/// encountered. This reduces contention and is highly parallel on a GPU.
///
/// 0       4           8       16                 ...                     2 MiB
/// ┌────────┬──────────┬────────┬──────────────────┬──────────────────────────┐
/// │ chunk  │  index   │  pad   │    bitfield[]    │         memory[]         │
/// └────────┴──────────┴────────┴──────────────────┴──────────────────────────┘
///
/// The size of the bitfield is the slab size divided by the chunk size divided
/// by the number of bits per word. We pad the interface to ensure 16 byte
/// alignment and to indicate that if the pointer is not aligned by 2MiB it
/// belongs to a slab rather than the global allocator.
struct Slab {
  // Header metadata for the slab, aligned to the minimum alignment.
  struct alignas(MIN_SIZE) Header {
    uint32_t chunk_size;
    uint32_t global_index;
    uint32_t cached_chunk_size;
  };

  // Initialize the slab with its chunk size and index in the global table for
  // use when freeing.
  Slab(uint32_t chunk_size, uint32_t global_index) {
    Header *header = reinterpret_cast<Header *>(memory);
    header->cached_chunk_size = cpp::numeric_limits<uint32_t>::max();
    header->chunk_size = chunk_size;
    header->global_index = global_index;
  }

  // Reset the memory with a new index and chunk size, not thread safe.
  Slab *reset(uint32_t chunk_size, uint32_t global_index) {
    Header *header = reinterpret_cast<Header *>(memory);
    header->cached_chunk_size = header->chunk_size;
    header->chunk_size = chunk_size;
    header->global_index = global_index;
    return this;
  }

  // Set the necessary bitfield bytes to zero in parallel using many lanes. This
  // must be called before the bitfield can be accessed safely, memory is not
  // guaranteed to be zero initialized in the current implementation.
  void initialize(uint64_t lane_mask) {
    // If this is a re-used slab the memory is already set to zero.
    if (get_cached_chunk_size() <= get_chunk_size())
      return;

    impl::uniform_memset(get_bitfield(), 0, bitfield_words(get_chunk_size()),
                         lane_mask);
  }

  // Get the number of chunks that can theoretically fit inside this slab.
  constexpr static uint32_t num_chunks(uint32_t chunk_size) {
    return SLAB_SIZE / chunk_size;
  }

  // Get the number of uint32_t words needed for the bitfield.
  constexpr static uint32_t bitfield_words(uint32_t chunk_size) {
    return (num_chunks(chunk_size) + BITS_IN_WORD - 1) / BITS_IN_WORD;
  }

  // Get the number of bytes reserved for the bitfield region with padding.
  constexpr static uint32_t bitfield_bytes(uint32_t chunk_size) {
    return __builtin_align_up(bitfield_words(chunk_size) *
                                  uint32_t(sizeof(uint32_t)),
                              __GCC_DESTRUCTIVE_SIZE << 1);
  }

  // The actual amount of memory available excluding the bitfield and metadata.
  constexpr static uint32_t available_bytes(uint32_t chunk_size) {
    return SLAB_SIZE - bitfield_bytes(chunk_size) - sizeof(Header);
  }

  // The length in bits of the bitfield.
  constexpr static uint32_t usable_bits(uint32_t chunk_size) {
    return available_bytes(chunk_size) / chunk_size;
  }

  // Get the location in the memory where we will store the chunk size.
  uint32_t get_chunk_size() const {
    return reinterpret_cast<const Header *>(memory)->chunk_size;
  }

  // Get the chunk size that was previously used.
  uint32_t get_cached_chunk_size() const {
    return reinterpret_cast<const Header *>(memory)->cached_chunk_size;
  }

  // Get the location in the memory where we will store the global index.
  uint32_t get_global_index() const {
    return reinterpret_cast<const Header *>(memory)->global_index;
  }

  // Get a pointer to where the bitfield is located in the memory.
  uint32_t *get_bitfield() {
    return reinterpret_cast<uint32_t *>(memory + sizeof(Header));
  }

  // Get a pointer to where the actual memory to be allocated lives.
  uint8_t *get_memory(uint32_t chunk_size) {
    return reinterpret_cast<uint8_t *>(get_bitfield()) +
           bitfield_bytes(chunk_size);
  }

  // Get a pointer to the actual memory given an index into the bitfield.
  void *ptr_from_index(uint32_t index, uint32_t chunk_size) {
    return get_memory(chunk_size) + index * chunk_size;
  }

  // Convert a pointer back into its bitfield index using its offset.
  uint32_t index_from_ptr(void *ptr, uint32_t chunk_size) {
    return static_cast<uint32_t>(reinterpret_cast<uint8_t *>(ptr) -
                                 get_memory(chunk_size)) /
           chunk_size;
  }

  // Randomly walks the bitfield until it finds a free bit. Allocations attempt
  // to put lanes right next to each other for better caching and convergence.
  void *allocate(uint64_t uniform, uint32_t reserved, uint32_t chunk_size) {
    uint32_t bits = usable_bits(chunk_size);
    uint32_t state = impl::entropy();

    // Try to find the empty bit in the bitfield to finish the allocation. We
    // start at the number of allocations as this is guaranteed to be available
    // until the user starts freeing memory.
    uint32_t start = gpu::shuffle(uniform, cpp::countr_zero(uniform), reserved);
    void *result = nullptr;
    for (uint64_t lane_mask = uniform; lane_mask;
         lane_mask = gpu::ballot(uniform, !result)) {
      if (!result) {
        // Each lane tries to claim one bit in a single contiguous mask.
        uint32_t id = impl::lane_count(lane_mask, gpu::get_lane_id());
        uint32_t index = (start + id) % bits;
        uint32_t slot = index / BITS_IN_WORD;
        uint32_t bit = index % BITS_IN_WORD;

        // Get the mask of bits destined for the same slot and coalesce it.
        uint32_t leader = impl::get_leader_id(
            gpu::ballot(lane_mask, !id || index % BITS_IN_WORD == 0),
            gpu::get_lane_id());
        uint32_t length =
            cpp::popcount(lane_mask) - impl::lane_count(lane_mask, leader);
        uint32_t bitmask =
            static_cast<uint32_t>(
                (uint64_t(1) << cpp::min(length, BITS_IN_WORD)) - 1)
            << bit;

        uint32_t before = 0;
        if (gpu::get_lane_id() == leader)
          before = cpp::AtomicRef(get_bitfield()[slot])
                       .fetch_or(bitmask, cpp::MemoryOrder::RELAXED);
        before = gpu::shuffle(lane_mask, leader, before);
        if (~before & (1u << bit))
          result = ptr_from_index(index, chunk_size);

        uint32_t after = before | bitmask;
        uint64_t waiting = gpu::ballot(lane_mask, !result);
        if (!result) {
          start =
              gpu::shuffle(waiting, cpp::countr_zero(waiting),
                           ~after ? __builtin_align_down(index, BITS_IN_WORD) +
                                        cpp::countr_zero(~after)
                                  : __builtin_align_down(
                                        impl::xorshift32(state), BITS_IN_WORD));
          if (!gpu::shuffle(waiting, cpp::countr_zero(waiting), ~after))
            sleep_briefly();
        }
      }
    }
    return result;
  }

  // Deallocates memory by resetting its corresponding bit in the bitfield.
  void deallocate(void *ptr) {
    uint32_t chunk_size = get_chunk_size();
    uint32_t index = index_from_ptr(ptr, chunk_size);
    uint32_t slot = index / BITS_IN_WORD;
    uint32_t bit = index % BITS_IN_WORD;

    cpp::AtomicRef(get_bitfield()[slot])
        .fetch_and(~(1u << bit), cpp::MemoryOrder::RELAXED);
  }

  // The actual memory the slab will manage. All offsets are calculated at
  // runtime with the chunk size to keep the interface convergent when a warp or
  // wavefront is handling multiple sizes at once.
  uint8_t memory[SLAB_SIZE];
};

// A global cache of previously allocated slabs for efficient reuse.
static FixedBuffer<Slab *, CACHED_SLABS> slab_cache;

/// A wait-free guard around a pointer resource to be created dynamically if
/// space is available and freed once there are no more users.
struct GuardPtr {
private:
  struct RefCounter {
    // Indicates that the object is in its deallocation phase and thus invalid.
    static constexpr uint32_t INVALID = uint32_t(1) << 31;

    // If a read preempts an unlock call we indicate this so the following
    // unlock call can swap out the helped bit and maintain exclusive ownership.
    static constexpr uint32_t HELPED = uint32_t(1) << 30;

    // Resets the reference counter, cannot be reset to zero safely.
    void reset(uint32_t n, uint32_t &count) {
      counter.store(n, cpp::MemoryOrder::RELAXED);
      count = n;
    }

    // Acquire a slot in the reference counter if it is not invalid.
    bool acquire(uint32_t n, uint32_t &count) {
      count = counter.fetch_add(n, cpp::MemoryOrder::RELAXED) + n;
      return (count & INVALID) == 0;
    }

    // Release a slot in the reference counter. This function should only be
    // called following a valid acquire call.
    bool release(uint32_t n) {
      // If this thread caused the counter to reach zero we try to invalidate it
      // and obtain exclusive rights to deconstruct it. If the CAS failed either
      // another thread resurrected the counter and we quit, or a parallel read
      // helped us invalidating it. For the latter, claim that flag and return.
      if (counter.fetch_sub(n, cpp::MemoryOrder::RELAXED) == n && RECLAIM) {
        uint32_t expected = 0;
        if (counter.compare_exchange_strong(expected, INVALID,
                                            cpp::MemoryOrder::RELAXED,
                                            cpp::MemoryOrder::RELAXED))
          return true;
        else if ((expected & HELPED) &&
                 (counter.exchange(INVALID, cpp::MemoryOrder::RELAXED) &
                  HELPED))
          return true;
      }
      return false;
    }

    // Returns the current reference count, potentially helping a releasing
    // thread.
    uint32_t read() {
      auto val = counter.load(cpp::MemoryOrder::RELAXED);
      if (val == 0 && RECLAIM &&
          counter.compare_exchange_strong(val, INVALID | HELPED,
                                          cpp::MemoryOrder::RELAXED))
        return 0;
      return (val & INVALID) ? 0 : val;
    }

    cpp::Atomic<uint32_t> counter{0};
  };

  cpp::Atomic<Slab *> ptr;
  RefCounter ref;

  // Should be called be a single lane for each different pointer.
  template <typename... Args>
  Slab *try_lock_impl(uint32_t n, uint32_t &count, Args &&...args) {
    Slab *expected = ptr.load(cpp::MemoryOrder::RELAXED);
    if (!expected &&
        ptr.compare_exchange_strong(
            expected,
            reinterpret_cast<Slab *>(cpp::numeric_limits<uintptr_t>::max()),
            cpp::MemoryOrder::RELAXED, cpp::MemoryOrder::RELAXED)) {
      count = cpp::numeric_limits<uint32_t>::max();

      Slab *cached = nullptr;
      if (slab_cache.pop(cached))
        return cached->reset(cpp::forward<Args>(args)...);

      void *raw = impl::rpc_allocate(sizeof(Slab));
      if (!raw) {
        ptr.store(nullptr, cpp::MemoryOrder::RELAXED);
        return nullptr;
      }
      return new (raw) Slab(cpp::forward<Args>(args)...);
    }

    // If there is a slab allocation in progress we retry a few times.
    for (uint32_t t = 0; impl::is_sentinel(expected) && t < MAX_TRIES; ++t) {
      sleep_briefly();
      expected = ptr.load(cpp::MemoryOrder::RELAXED);
    }

    if (!expected || impl::is_sentinel(expected))
      return nullptr;

    if (!ref.acquire(n, count))
      return nullptr;

    cpp::atomic_thread_fence(cpp::MemoryOrder::ACQUIRE);
    return RECLAIM ? ptr.load(cpp::MemoryOrder::RELAXED) : expected;
  }

  // Finalize the associated memory and signal that it is ready to use by
  // resetting the counter.
  void finalize(Slab *mem, uint32_t n, uint32_t &count) {
    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
    ptr.store(mem, cpp::MemoryOrder::RELAXED);
    cpp::atomic_thread_fence(cpp::MemoryOrder::ACQUIRE);
    if (!ref.acquire(n, count))
      ref.reset(n, count);
  }

public:
  // Attempt to lock access to the pointer, potentially creating it if empty.
  // The uniform mask represents which lanes share the same pointer. For each
  // uniform value we elect a leader to handle it on behalf of the other lanes.
  template <typename... Args>
  Slab *try_lock(uint64_t lane_mask, uint64_t uniform, uint32_t &count,
                 Args &&...args) {
    count = 0;
    Slab *result = nullptr;
    if (gpu::get_lane_id() == uint32_t(cpp::countr_zero(uniform)))
      result = try_lock_impl(cpp::popcount(uniform), count,
                             cpp::forward<Args>(args)...);
    result = gpu::shuffle(lane_mask, cpp::countr_zero(uniform), result);
    count = gpu::shuffle(lane_mask, cpp::countr_zero(uniform), count);

    if (!result)
      return nullptr;

    // We defer storing the newly allocated slab until now so that we can use
    // multiple lanes to initialize it and release it for use.
    if (impl::is_sentinel(count)) {
      result->initialize(uniform);
      gpu::sync_lane(uniform);
      if (gpu::get_lane_id() == uint32_t(cpp::countr_zero(uniform)))
        finalize(result, cpp::popcount(uniform), count);
      count = gpu::shuffle(uniform, cpp::countr_zero(uniform), count);
    }

    count = count - cpp::popcount(uniform) +
            impl::lane_count(uniform, gpu::get_lane_id());

    return result;
  }

  // Release the associated lock on the pointer, potentially destroying it.
  void unlock(uint64_t mask) {
    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
    if (gpu::get_lane_id() == uint32_t(cpp::countr_zero(mask)) &&
        ref.release(cpp::popcount(mask))) {
      Slab *p = ptr.load(cpp::MemoryOrder::RELAXED);
      if (!slab_cache.push(p)) {
        p->~Slab();
        impl::rpc_free(p);
      }
      cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
      ptr.store(nullptr, cpp::MemoryOrder::RELAXED);
    }
  }

  // Get the current value of the reference counter.
  uint32_t use_count() { return ref.read(); }
};

// The global array used to search for a valid slab to allocate from.
static GuardPtr slots[ARRAY_SIZE] = {};

// Keep a cache of the last successful slot for each chunk size. Initialize it
// to an even spread of the total size. Must be updated if the chunking scheme
// changes.
#define S(X) (impl::get_start_index(X))
static cpp::Atomic<uint32_t> indices[] = {
    S(16),     S(32),     S(48),     S(64),     S(96),     S(112),    S(128),
    S(192),    S(224),    S(256),    S(384),    S(448),    S(512),    S(768),
    S(896),    S(1024),   S(1536),   S(1792),   S(2048),   S(3072),   S(3584),
    S(4096),   S(6144),   S(7168),   S(8192),   S(12288),  S(14336),  S(16384),
    S(24576),  S(28672),  S(32768),  S(49152),  S(57344),  S(65536),  S(98304),
    S(114688), S(131072), S(196608), S(229376), S(262144), S(393216), S(458752),
    S(524288), S(786432), S(917504), S(1048576)};
#undef S

// Tries to find a slab in the table that can support the given chunk size.
static Slab *find_slab(uint32_t chunk_size, uint64_t lane_mask,
                       uint64_t &uniform, uint32_t &reserved) {
  // We start at the index of the last successful allocation for this kind.
  uint32_t chunk_id = impl::get_chunk_id(chunk_size);
  uint32_t start = indices[chunk_id].load(cpp::MemoryOrder::RELAXED);
  uint32_t usable = Slab::usable_bits(chunk_size);
  uint32_t base = impl::get_start_index(chunk_size);
  uint64_t id = impl::id_in_mask();

  Slab *result = nullptr;
  for (uint32_t offset = 0;
       gpu::ballot(lane_mask, !result) && offset <= ARRAY_SIZE; ++offset) {
    uint32_t index = !offset ? start : (base + offset - 1) % ARRAY_SIZE;

    bool available = !offset || slots[index].use_count() < usable;
    uint64_t slab_mask = gpu::ballot(lane_mask, !result && available);
    if (slab_mask & id) {
      Slab *slab = slots[index].try_lock(slab_mask, uniform & slab_mask,
                                         reserved, chunk_size, index);

      // If we find a slab with a matching chunk size then we store the result.
      // Otherwise, we need to free the claimed lock and continue. In the case
      // of out-of-memory we receive a sentinel value and return a failure.
      uint64_t locked_mask =
          gpu::ballot(slab_mask, slab && reserved < usable &&
                                     slab->get_chunk_size() == chunk_size);
      uint64_t failed_mask = gpu::ballot(slab_mask, slab) & ~locked_mask;
      if (locked_mask & id) {
        if (index != start)
          indices[chunk_id].store(index, cpp::MemoryOrder::RELAXED);
        uniform = uniform & locked_mask;
        result = slab;
      } else if (failed_mask & id) {
        slots[index].unlock(failed_mask & uniform);
      } else if (!slab && impl::is_sentinel(reserved)) {
        result =
            reinterpret_cast<Slab *>(cpp::numeric_limits<uintptr_t>::max());
      } else {
        sleep_briefly();
      }
    }
  }
  return !impl::is_sentinel(result) ? result : nullptr;
}

namespace gpu {

void *allocate(uint64_t size) {
  if (!size)
    return nullptr;

  // Allocations requiring a full slab or more go directly to memory.
  if (size >= SLAB_SIZE / 2)
    return impl::rpc_allocate(__builtin_align_up(size, SLAB_SIZE));

  // Try to find a slab for the rounded up chunk size and allocate from it.
  uint32_t chunk_size = impl::get_chunk_size(static_cast<uint32_t>(size));
  uint64_t lane_mask = gpu::get_lane_mask();
  uint64_t uniform = gpu::match_any(lane_mask, chunk_size);
  uint32_t reserved = 0;
  Slab *slab = find_slab(chunk_size, lane_mask, uniform, reserved);
  if (!slab)
    return nullptr;

  void *ptr = slab->allocate(uniform, reserved, chunk_size);
  return ptr;
}

void deallocate(void *ptr) {
  if (!ptr)
    return;

  // All non-slab allocations will be aligned on a 2MiB boundary.
  if (__builtin_is_aligned(ptr, SLAB_ALIGNMENT + 1))
    return impl::rpc_free(ptr);

  // The original slab pointer is the 2MiB boundary using the given pointer.
  uint64_t lane_mask = gpu::get_lane_mask();
  Slab *slab = cpp::launder(reinterpret_cast<Slab *>(
      (reinterpret_cast<uintptr_t>(ptr) & ~SLAB_ALIGNMENT)));
  slab->deallocate(ptr);

  // Release the lock associated with the slab.
  uint32_t index = slab->get_global_index();
  uint64_t uniform = gpu::match_any(lane_mask, index);
  slots[index].unlock(uniform);
}

void *reallocate(void *ptr, uint64_t size) {
  if (ptr == nullptr)
    return gpu::allocate(size);

  // Non-slab allocations are considered foreign pointers so we fail.
  if (__builtin_is_aligned(ptr, SLAB_ALIGNMENT + 1))
    return nullptr;

  // The original slab pointer is the 2MiB boundary using the given pointer.
  Slab *slab = cpp::launder(reinterpret_cast<Slab *>(
      (reinterpret_cast<uintptr_t>(ptr) & ~SLAB_ALIGNMENT)));
  if (slab->get_chunk_size() >= size)
    return ptr;

  // If we need a new chunk we reallocate and copy it over.
  void *new_ptr = gpu::allocate(size);
  if (!new_ptr)
    return nullptr;

  inline_memcpy(new_ptr, ptr, slab->get_chunk_size());
  gpu::deallocate(ptr);
  return new_ptr;
}

void *aligned_allocate(uint32_t alignment, uint64_t size) {
  // All alignment values must be a non-zero power of two.
  if (!impl::is_pow2(alignment))
    return nullptr;

  // If the requested alignment is less than what we already provide this is
  // just a normal allocation.
  if (alignment <= MIN_ALIGNMENT + 1)
    return gpu::allocate(size);

  // We can't handle alignments greater than 2MiB so we simply fail.
  if (alignment > SLAB_ALIGNMENT + 1)
    return nullptr;

  // Trying to handle allocation internally would break the assumption that each
  // chunk is identical to eachother. Allocate enough memory with worst-case
  // alignment and then round up. The index logic will round down properly.
  uint64_t rounded = size + alignment - MIN_ALIGNMENT;
  void *ptr = gpu::allocate(rounded);
  return ptr ? __builtin_align_up(ptr, alignment) : ptr;
}

} // namespace gpu
} // namespace LIBC_NAMESPACE_DECL
