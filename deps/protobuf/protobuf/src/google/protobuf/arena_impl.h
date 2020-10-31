// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file defines an Arena allocator for better allocation performance.

#ifndef GOOGLE_PROTOBUF_ARENA_IMPL_H__
#define GOOGLE_PROTOBUF_ARENA_IMPL_H__

#include <atomic>
#include <limits>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>

#ifdef ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif  // ADDRESS_SANITIZER

#include <google/protobuf/port_def.inc>


namespace google {
namespace protobuf {
namespace internal {

inline size_t AlignUpTo8(size_t n) {
  // Align n to next multiple of 8 (from Hacker's Delight, Chapter 3.)
  return (n + 7) & static_cast<size_t>(-8);
}

using LifecycleId = int64_t;

// This class provides the core Arena memory allocation library. Different
// implementations only need to implement the public interface below.
// Arena is not a template type as that would only be useful if all protos
// in turn would be templates, which will/cannot happen. However separating
// the memory allocation part from the cruft of the API users expect we can
// use #ifdef the select the best implementation based on hardware / OS.
class PROTOBUF_EXPORT ArenaImpl {
 public:
  struct Options {
    size_t start_block_size;
    size_t max_block_size;
    char* initial_block;
    size_t initial_block_size;
    void* (*block_alloc)(size_t);
    void (*block_dealloc)(void*, size_t);

    template <typename O>
    explicit Options(const O& options)
        : start_block_size(options.start_block_size),
          max_block_size(options.max_block_size),
          initial_block(options.initial_block),
          initial_block_size(options.initial_block_size),
          block_alloc(options.block_alloc),
          block_dealloc(options.block_dealloc) {}
  };

  template <typename O>
  explicit ArenaImpl(const O& options) : options_(options) {
    if (options_.initial_block != NULL && options_.initial_block_size > 0) {
      GOOGLE_CHECK_GE(options_.initial_block_size, sizeof(Block))
          << ": Initial block size too small for header.";
      initial_block_ = reinterpret_cast<Block*>(options_.initial_block);
    } else {
      initial_block_ = NULL;
    }

    Init();
  }

  // Destructor deletes all owned heap allocated objects, and destructs objects
  // that have non-trivial destructors, except for proto2 message objects whose
  // destructors can be skipped. Also, frees all blocks except the initial block
  // if it was passed in.
  ~ArenaImpl();

  uint64 Reset();

  uint64 SpaceAllocated() const;
  uint64 SpaceUsed() const;

  void* AllocateAligned(size_t n);

  void* AllocateAlignedAndAddCleanup(size_t n, void (*cleanup)(void*));

  // Add object pointer and cleanup function pointer to the list.
  void AddCleanup(void* elem, void (*cleanup)(void*));

 private:
  void* AllocateAlignedFallback(size_t n);
  void* AllocateAlignedAndAddCleanupFallback(size_t n, void (*cleanup)(void*));
  void AddCleanupFallback(void* elem, void (*cleanup)(void*));

  // Node contains the ptr of the object to be cleaned up and the associated
  // cleanup function ptr.
  struct CleanupNode {
    void* elem;              // Pointer to the object to be cleaned up.
    void (*cleanup)(void*);  // Function pointer to the destructor or deleter.
  };

  // Cleanup uses a chunked linked list, to reduce pointer chasing.
  struct CleanupChunk {
    static size_t SizeOf(size_t i) {
      return sizeof(CleanupChunk) + (sizeof(CleanupNode) * (i - 1));
    }
    size_t size;           // Total elements in the list.
    CleanupChunk* next;    // Next node in the list.
    CleanupNode nodes[1];  // True length is |size|.
  };

  class Block;

  // A thread-unsafe Arena that can only be used within its owning thread.
  class PROTOBUF_EXPORT SerialArena {
   public:
    // The allocate/free methods here are a little strange, since SerialArena is
    // allocated inside a Block which it also manages.  This is to avoid doing
    // an extra allocation for the SerialArena itself.

    // Creates a new SerialArena inside Block* and returns it.
    static SerialArena* New(Block* b, void* owner, ArenaImpl* arena);

    // Destroys this SerialArena, freeing all blocks with the given dealloc
    // function, except any block equal to |initial_block|.
    static uint64 Free(SerialArena* serial, Block* initial_block,
                       void (*block_dealloc)(void*, size_t));

    void CleanupList();
    uint64 SpaceUsed() const;

    void* AllocateAligned(size_t n) {
      GOOGLE_DCHECK_EQ(internal::AlignUpTo8(n), n);  // Must be already aligned.
      GOOGLE_DCHECK_GE(limit_, ptr_);
      if (PROTOBUF_PREDICT_FALSE(static_cast<size_t>(limit_ - ptr_) < n)) {
        return AllocateAlignedFallback(n);
      }
      void* ret = ptr_;
      ptr_ += n;
#ifdef ADDRESS_SANITIZER
      ASAN_UNPOISON_MEMORY_REGION(ret, n);
#endif  // ADDRESS_SANITIZER
      return ret;
    }

    void AddCleanup(void* elem, void (*cleanup)(void*)) {
      if (PROTOBUF_PREDICT_FALSE(cleanup_ptr_ == cleanup_limit_)) {
        AddCleanupFallback(elem, cleanup);
        return;
      }
      cleanup_ptr_->elem = elem;
      cleanup_ptr_->cleanup = cleanup;
      cleanup_ptr_++;
    }

    void* AllocateAlignedAndAddCleanup(size_t n, void (*cleanup)(void*)) {
      void* ret = AllocateAligned(n);
      AddCleanup(ret, cleanup);
      return ret;
    }

    void* owner() const { return owner_; }
    SerialArena* next() const { return next_; }
    void set_next(SerialArena* next) { next_ = next; }

   private:
    void* AllocateAlignedFallback(size_t n);
    void AddCleanupFallback(void* elem, void (*cleanup)(void*));
    void CleanupListFallback();

    ArenaImpl* arena_;       // Containing arena.
    void* owner_;            // &ThreadCache of this thread;
    Block* head_;            // Head of linked list of blocks.
    CleanupChunk* cleanup_;  // Head of cleanup list.
    SerialArena* next_;      // Next SerialArena in this linked list.

    // Next pointer to allocate from.  Always 8-byte aligned.  Points inside
    // head_ (and head_->pos will always be non-canonical).  We keep these
    // here to reduce indirection.
    char* ptr_;
    char* limit_;

    // Next CleanupList members to append to.  These point inside cleanup_.
    CleanupNode* cleanup_ptr_;
    CleanupNode* cleanup_limit_;
  };

  // Blocks are variable length malloc-ed objects.  The following structure
  // describes the common header for all blocks.
  class PROTOBUF_EXPORT Block {
   public:
    Block(size_t size, Block* next);

    char* Pointer(size_t n) {
      GOOGLE_DCHECK(n <= size_);
      return reinterpret_cast<char*>(this) + n;
    }

    Block* next() const { return next_; }
    size_t pos() const { return pos_; }
    size_t size() const { return size_; }
    void set_pos(size_t pos) { pos_ = pos; }

   private:
    Block* next_;  // Next block for this thread.
    size_t pos_;
    size_t size_;
    // data follows
  };

  struct ThreadCache {
#if defined(GOOGLE_PROTOBUF_NO_THREADLOCAL)
    // If we are using the ThreadLocalStorage class to store the ThreadCache,
    // then the ThreadCache's default constructor has to be responsible for
    // initializing it.
    ThreadCache() : last_lifecycle_id_seen(-1), last_serial_arena(NULL) {}
#endif

    // The ThreadCache is considered valid as long as this matches the
    // lifecycle_id of the arena being used.
    LifecycleId last_lifecycle_id_seen;
    SerialArena* last_serial_arena;
  };
  static std::atomic<LifecycleId> lifecycle_id_generator_;
#if defined(GOOGLE_PROTOBUF_NO_THREADLOCAL)
  // Android ndk does not support GOOGLE_THREAD_LOCAL keyword so we use a custom thread
  // local storage class we implemented.
  // iOS also does not support the GOOGLE_THREAD_LOCAL keyword.
  static ThreadCache& thread_cache();
#elif defined(PROTOBUF_USE_DLLS)
  // Thread local variables cannot be exposed through DLL interface but we can
  // wrap them in static functions.
  static ThreadCache& thread_cache();
#else
  static GOOGLE_THREAD_LOCAL ThreadCache thread_cache_;
  static ThreadCache& thread_cache() { return thread_cache_; }
#endif

  void Init();

  // Free all blocks and return the total space used which is the sums of sizes
  // of the all the allocated blocks.
  uint64 FreeBlocks();
  // Delete or Destruct all objects owned by the arena.
  void CleanupList();

  inline void CacheSerialArena(SerialArena* serial) {
    thread_cache().last_serial_arena = serial;
    thread_cache().last_lifecycle_id_seen = lifecycle_id_;
    // TODO(haberman): evaluate whether we would gain efficiency by getting rid
    // of hint_.  It's the only write we do to ArenaImpl in the allocation path,
    // which will dirty the cache line.

    hint_.store(serial, std::memory_order_release);
  }

  std::atomic<SerialArena*>
      threads_;                     // Pointer to a linked list of SerialArena.
  std::atomic<SerialArena*> hint_;  // Fast thread-local block access
  std::atomic<size_t> space_allocated_;  // Total size of all allocated blocks.

  Block* initial_block_;  // If non-NULL, points to the block that came from
                          // user data.

  Block* NewBlock(Block* last_block, size_t min_bytes);

  SerialArena* GetSerialArena();
  bool GetSerialArenaFast(SerialArena** arena);
  SerialArena* GetSerialArenaFallback(void* me);
  LifecycleId lifecycle_id_;  // Unique for each arena. Changes on Reset().

  Options options_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArenaImpl);
  // All protos have pointers back to the arena hence Arena must have
  // pointer stability.
  ArenaImpl(ArenaImpl&&) = delete;
  ArenaImpl& operator=(ArenaImpl&&) = delete;

 public:
  // kBlockHeaderSize is sizeof(Block), aligned up to the nearest multiple of 8
  // to protect the invariant that pos is always at a multiple of 8.
  static const size_t kBlockHeaderSize =
      (sizeof(Block) + 7) & static_cast<size_t>(-8);
  static const size_t kSerialArenaSize =
      (sizeof(SerialArena) + 7) & static_cast<size_t>(-8);
  static_assert(kBlockHeaderSize % 8 == 0,
                "kBlockHeaderSize must be a multiple of 8.");
  static_assert(kSerialArenaSize % 8 == 0,
                "kSerialArenaSize must be a multiple of 8.");
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_ARENA_IMPL_H__
