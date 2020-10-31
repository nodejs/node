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

#include <google/protobuf/arena.h>

#include <algorithm>
#include <atomic>
#include <limits>

#include <google/protobuf/stubs/mutex.h>

#ifdef ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif  // ADDRESS_SANITIZER

#include <google/protobuf/port_def.inc>

static const size_t kMinCleanupListElements = 8;
static const size_t kMaxCleanupListElements = 64;  // 1kB on 64-bit.

namespace google {
namespace protobuf {
namespace internal {


std::atomic<LifecycleId> ArenaImpl::lifecycle_id_generator_;
#if defined(GOOGLE_PROTOBUF_NO_THREADLOCAL)
ArenaImpl::ThreadCache& ArenaImpl::thread_cache() {
  static internal::ThreadLocalStorage<ThreadCache>* thread_cache_ =
      new internal::ThreadLocalStorage<ThreadCache>();
  return *thread_cache_->Get();
}
#elif defined(PROTOBUF_USE_DLLS)
ArenaImpl::ThreadCache& ArenaImpl::thread_cache() {
  static GOOGLE_THREAD_LOCAL ThreadCache thread_cache_ = {-1, NULL};
  return thread_cache_;
}
#else
GOOGLE_THREAD_LOCAL ArenaImpl::ThreadCache ArenaImpl::thread_cache_ = {-1, NULL};
#endif

void ArenaImpl::Init() {
  lifecycle_id_ =
      lifecycle_id_generator_.fetch_add(1, std::memory_order_relaxed);
  hint_.store(nullptr, std::memory_order_relaxed);
  threads_.store(nullptr, std::memory_order_relaxed);

  if (initial_block_) {
    // Thread which calls Init() owns the first block. This allows the
    // single-threaded case to allocate on the first block without having to
    // perform atomic operations.
    new (initial_block_) Block(options_.initial_block_size, NULL);
    SerialArena* serial =
        SerialArena::New(initial_block_, &thread_cache(), this);
    serial->set_next(NULL);
    threads_.store(serial, std::memory_order_relaxed);
    space_allocated_.store(options_.initial_block_size,
                           std::memory_order_relaxed);
    CacheSerialArena(serial);
  } else {
    space_allocated_.store(0, std::memory_order_relaxed);
  }
}

ArenaImpl::~ArenaImpl() {
  // Have to do this in a first pass, because some of the destructors might
  // refer to memory in other blocks.
  CleanupList();
  FreeBlocks();
}

uint64 ArenaImpl::Reset() {
  // Have to do this in a first pass, because some of the destructors might
  // refer to memory in other blocks.
  CleanupList();
  uint64 space_allocated = FreeBlocks();
  Init();

  return space_allocated;
}

ArenaImpl::Block* ArenaImpl::NewBlock(Block* last_block, size_t min_bytes) {
  size_t size;
  if (last_block) {
    // Double the current block size, up to a limit.
    size = std::min(2 * last_block->size(), options_.max_block_size);
  } else {
    size = options_.start_block_size;
  }
  // Verify that min_bytes + kBlockHeaderSize won't overflow.
  GOOGLE_CHECK_LE(min_bytes, std::numeric_limits<size_t>::max() - kBlockHeaderSize);
  size = std::max(size, kBlockHeaderSize + min_bytes);

  void* mem = options_.block_alloc(size);
  Block* b = new (mem) Block(size, last_block);
  space_allocated_.fetch_add(size, std::memory_order_relaxed);
  return b;
}

ArenaImpl::Block::Block(size_t size, Block* next)
    : next_(next), pos_(kBlockHeaderSize), size_(size) {}

PROTOBUF_NOINLINE
void ArenaImpl::SerialArena::AddCleanupFallback(void* elem,
                                                void (*cleanup)(void*)) {
  size_t size = cleanup_ ? cleanup_->size * 2 : kMinCleanupListElements;
  size = std::min(size, kMaxCleanupListElements);
  size_t bytes = internal::AlignUpTo8(CleanupChunk::SizeOf(size));
  CleanupChunk* list = reinterpret_cast<CleanupChunk*>(AllocateAligned(bytes));
  list->next = cleanup_;
  list->size = size;

  cleanup_ = list;
  cleanup_ptr_ = &list->nodes[0];
  cleanup_limit_ = &list->nodes[size];

  AddCleanup(elem, cleanup);
}

PROTOBUF_FUNC_ALIGN(32)
void* ArenaImpl::AllocateAligned(size_t n) {
  SerialArena* arena;
  if (PROTOBUF_PREDICT_TRUE(GetSerialArenaFast(&arena))) {
    return arena->AllocateAligned(n);
  } else {
    return AllocateAlignedFallback(n);
  }
}

void* ArenaImpl::AllocateAlignedAndAddCleanup(size_t n,
                                              void (*cleanup)(void*)) {
  SerialArena* arena;
  if (PROTOBUF_PREDICT_TRUE(GetSerialArenaFast(&arena))) {
    return arena->AllocateAlignedAndAddCleanup(n, cleanup);
  } else {
    return AllocateAlignedAndAddCleanupFallback(n, cleanup);
  }
}

void ArenaImpl::AddCleanup(void* elem, void (*cleanup)(void*)) {
  SerialArena* arena;
  if (PROTOBUF_PREDICT_TRUE(GetSerialArenaFast(&arena))) {
    arena->AddCleanup(elem, cleanup);
  } else {
    return AddCleanupFallback(elem, cleanup);
  }
}

PROTOBUF_NOINLINE
void* ArenaImpl::AllocateAlignedFallback(size_t n) {
  return GetSerialArena()->AllocateAligned(n);
}

PROTOBUF_NOINLINE
void* ArenaImpl::AllocateAlignedAndAddCleanupFallback(size_t n,
                                                      void (*cleanup)(void*)) {
  return GetSerialArena()->AllocateAlignedAndAddCleanup(n, cleanup);
}

PROTOBUF_NOINLINE
void ArenaImpl::AddCleanupFallback(void* elem, void (*cleanup)(void*)) {
  GetSerialArena()->AddCleanup(elem, cleanup);
}

inline PROTOBUF_ALWAYS_INLINE bool ArenaImpl::GetSerialArenaFast(
    ArenaImpl::SerialArena** arena) {
  // If this thread already owns a block in this arena then try to use that.
  // This fast path optimizes the case where multiple threads allocate from the
  // same arena.
  ThreadCache* tc = &thread_cache();
  if (PROTOBUF_PREDICT_TRUE(tc->last_lifecycle_id_seen == lifecycle_id_)) {
    *arena = tc->last_serial_arena;
    return true;
  }

  // Check whether we own the last accessed SerialArena on this arena.  This
  // fast path optimizes the case where a single thread uses multiple arenas.
  SerialArena* serial = hint_.load(std::memory_order_acquire);
  if (PROTOBUF_PREDICT_TRUE(serial != NULL && serial->owner() == tc)) {
    *arena = serial;
    return true;
  }

  return false;
}

ArenaImpl::SerialArena* ArenaImpl::GetSerialArena() {
  SerialArena* arena;
  if (PROTOBUF_PREDICT_TRUE(GetSerialArenaFast(&arena))) {
    return arena;
  } else {
    return GetSerialArenaFallback(&thread_cache());
  }
}

PROTOBUF_NOINLINE
void* ArenaImpl::SerialArena::AllocateAlignedFallback(size_t n) {
  // Sync back to current's pos.
  head_->set_pos(head_->size() - (limit_ - ptr_));

  head_ = arena_->NewBlock(head_, n);
  ptr_ = head_->Pointer(head_->pos());
  limit_ = head_->Pointer(head_->size());

#ifdef ADDRESS_SANITIZER
  ASAN_POISON_MEMORY_REGION(ptr_, limit_ - ptr_);
#endif  // ADDRESS_SANITIZER

  return AllocateAligned(n);
}

uint64 ArenaImpl::SpaceAllocated() const {
  return space_allocated_.load(std::memory_order_relaxed);
}

uint64 ArenaImpl::SpaceUsed() const {
  SerialArena* serial = threads_.load(std::memory_order_acquire);
  uint64 space_used = 0;
  for (; serial; serial = serial->next()) {
    space_used += serial->SpaceUsed();
  }
  return space_used;
}

uint64 ArenaImpl::SerialArena::SpaceUsed() const {
  // Get current block's size from ptr_ (since we can't trust head_->pos().
  uint64 space_used = ptr_ - head_->Pointer(kBlockHeaderSize);
  // Get subsequent block size from b->pos().
  for (Block* b = head_->next(); b; b = b->next()) {
    space_used += (b->pos() - kBlockHeaderSize);
  }
  // Remove the overhead of the SerialArena itself.
  space_used -= kSerialArenaSize;
  return space_used;
}

uint64 ArenaImpl::FreeBlocks() {
  uint64 space_allocated = 0;
  // By omitting an Acquire barrier we ensure that any user code that doesn't
  // properly synchronize Reset() or the destructor will throw a TSAN warning.
  SerialArena* serial = threads_.load(std::memory_order_relaxed);

  while (serial) {
    // This is inside a block we are freeing, so we need to read it now.
    SerialArena* next = serial->next();
    space_allocated += ArenaImpl::SerialArena::Free(serial, initial_block_,
                                                    options_.block_dealloc);
    // serial is dead now.
    serial = next;
  }

  return space_allocated;
}

uint64 ArenaImpl::SerialArena::Free(ArenaImpl::SerialArena* serial,
                                    Block* initial_block,
                                    void (*block_dealloc)(void*, size_t)) {
  uint64 space_allocated = 0;

  // We have to be careful in this function, since we will be freeing the Block
  // that contains this SerialArena.  Be careful about accessing |serial|.

  for (Block* b = serial->head_; b;) {
    // This is inside the block we are freeing, so we need to read it now.
    Block* next_block = b->next();
    space_allocated += (b->size());

#ifdef ADDRESS_SANITIZER
    // This memory was provided by the underlying allocator as unpoisoned, so
    // return it in an unpoisoned state.
    ASAN_UNPOISON_MEMORY_REGION(b->Pointer(0), b->size());
#endif  // ADDRESS_SANITIZER

    if (b != initial_block) {
      block_dealloc(b, b->size());
    }

    b = next_block;
  }

  return space_allocated;
}

void ArenaImpl::CleanupList() {
  // By omitting an Acquire barrier we ensure that any user code that doesn't
  // properly synchronize Reset() or the destructor will throw a TSAN warning.
  SerialArena* serial = threads_.load(std::memory_order_relaxed);

  for (; serial; serial = serial->next()) {
    serial->CleanupList();
  }
}

void ArenaImpl::SerialArena::CleanupList() {
  if (cleanup_ != NULL) {
    CleanupListFallback();
  }
}

void ArenaImpl::SerialArena::CleanupListFallback() {
  // The first chunk might be only partially full, so calculate its size
  // from cleanup_ptr_. Subsequent chunks are always full, so use list->size.
  size_t n = cleanup_ptr_ - &cleanup_->nodes[0];
  CleanupChunk* list = cleanup_;
  while (true) {
    CleanupNode* node = &list->nodes[0];
    // Cleanup newest elements first (allocated last).
    for (size_t i = n; i > 0; i--) {
      node[i - 1].cleanup(node[i - 1].elem);
    }
    list = list->next;
    if (list == nullptr) {
      break;
    }
    // All but the first chunk are always full.
    n = list->size;
  }
}

ArenaImpl::SerialArena* ArenaImpl::SerialArena::New(Block* b, void* owner,
                                                    ArenaImpl* arena) {
  GOOGLE_DCHECK_EQ(b->pos(), kBlockHeaderSize);  // Should be a fresh block
  GOOGLE_DCHECK_LE(kBlockHeaderSize + kSerialArenaSize, b->size());
  SerialArena* serial =
      reinterpret_cast<SerialArena*>(b->Pointer(kBlockHeaderSize));
  b->set_pos(kBlockHeaderSize + kSerialArenaSize);
  serial->arena_ = arena;
  serial->owner_ = owner;
  serial->head_ = b;
  serial->ptr_ = b->Pointer(b->pos());
  serial->limit_ = b->Pointer(b->size());
  serial->cleanup_ = NULL;
  serial->cleanup_ptr_ = NULL;
  serial->cleanup_limit_ = NULL;
  return serial;
}

PROTOBUF_NOINLINE
ArenaImpl::SerialArena* ArenaImpl::GetSerialArenaFallback(void* me) {
  // Look for this SerialArena in our linked list.
  SerialArena* serial = threads_.load(std::memory_order_acquire);
  for (; serial; serial = serial->next()) {
    if (serial->owner() == me) {
      break;
    }
  }

  if (!serial) {
    // This thread doesn't have any SerialArena, which also means it doesn't
    // have any blocks yet.  So we'll allocate its first block now.
    Block* b = NewBlock(NULL, kSerialArenaSize);
    serial = SerialArena::New(b, me, this);

    SerialArena* head = threads_.load(std::memory_order_relaxed);
    do {
      serial->set_next(head);
    } while (!threads_.compare_exchange_weak(
        head, serial, std::memory_order_release, std::memory_order_relaxed));
  }

  CacheSerialArena(serial);
  return serial;
}

}  // namespace internal

void Arena::CallDestructorHooks() {
  uint64 space_allocated = impl_.SpaceAllocated();
  // Call the reset hook
  if (on_arena_reset_ != NULL) {
    on_arena_reset_(this, hooks_cookie_, space_allocated);
  }

  // Call the destruction hook
  if (on_arena_destruction_ != NULL) {
    on_arena_destruction_(this, hooks_cookie_, space_allocated);
  }
}

void Arena::OnArenaAllocation(const std::type_info* allocated_type,
                              size_t n) const {
  if (on_arena_allocation_ != NULL) {
    on_arena_allocation_(allocated_type, n, hooks_cookie_);
  }
}

}  // namespace protobuf
}  // namespace google
