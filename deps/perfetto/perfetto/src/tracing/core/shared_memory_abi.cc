/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "perfetto/ext/tracing/core/shared_memory_abi.h"

#include "perfetto/base/build_config.h"
#include "perfetto/base/time.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <sys/mman.h>
#endif

#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/basic_types.h"

namespace perfetto {

namespace {

constexpr int kRetryAttempts = 64;

inline void WaitBeforeNextAttempt(int attempt) {
  if (attempt < kRetryAttempts / 2) {
    std::this_thread::yield();
  } else {
    base::SleepMicroseconds((unsigned(attempt) / 10) * 1000);
  }
}

// Returns the largest 4-bytes aligned chunk size <= |page_size| / |divider|
// for each divider in PageLayout.
constexpr size_t GetChunkSize(size_t page_size, size_t divider) {
  return ((page_size - sizeof(SharedMemoryABI::PageHeader)) / divider) & ~3UL;
}

// Initializer for the const |chunk_sizes_| array.
std::array<uint16_t, SharedMemoryABI::kNumPageLayouts> InitChunkSizes(
    size_t page_size) {
  static_assert(SharedMemoryABI::kNumPageLayouts ==
                    base::ArraySize(SharedMemoryABI::kNumChunksForLayout),
                "kNumPageLayouts out of date");
  std::array<uint16_t, SharedMemoryABI::kNumPageLayouts> res = {};
  for (size_t i = 0; i < SharedMemoryABI::kNumPageLayouts; i++) {
    size_t num_chunks = SharedMemoryABI::kNumChunksForLayout[i];
    size_t size = num_chunks == 0 ? 0 : GetChunkSize(page_size, num_chunks);
    PERFETTO_CHECK(size <= std::numeric_limits<uint16_t>::max());
    res[i] = static_cast<uint16_t>(size);
  }
  return res;
}

inline void ClearChunkHeader(SharedMemoryABI::ChunkHeader* header) {
  header->writer_id.store(0u, std::memory_order_relaxed);
  header->chunk_id.store(0u, std::memory_order_relaxed);
  header->packets.store({}, std::memory_order_release);
}

}  // namespace

// static
constexpr uint32_t SharedMemoryABI::kNumChunksForLayout[];
constexpr const char* SharedMemoryABI::kChunkStateStr[];
constexpr const size_t SharedMemoryABI::kInvalidPageIdx;
constexpr const size_t SharedMemoryABI::kMaxPageSize;
constexpr const size_t SharedMemoryABI::kPacketSizeDropPacket;

SharedMemoryABI::SharedMemoryABI() = default;

SharedMemoryABI::SharedMemoryABI(uint8_t* start,
                                 size_t size,
                                 size_t page_size) {
  Initialize(start, size, page_size);
}

void SharedMemoryABI::Initialize(uint8_t* start,
                                 size_t size,
                                 size_t page_size) {
  start_ = start;
  size_ = size;
  page_size_ = page_size;
  num_pages_ = size / page_size;
  chunk_sizes_ = InitChunkSizes(page_size);
  static_assert(sizeof(PageHeader) == 8, "PageHeader size");
  static_assert(sizeof(ChunkHeader) == 8, "ChunkHeader size");
  static_assert(sizeof(ChunkHeader::chunk_id) == sizeof(ChunkID),
                "ChunkID size");

  static_assert(sizeof(ChunkHeader::Packets) == 2, "ChunkHeader::Packets size");
  static_assert(alignof(ChunkHeader) == kChunkAlignment,
                "ChunkHeader alignment");

  // In theory std::atomic does not guarantee that the underlying type
  // consists only of the actual atomic word. Theoretically it could have
  // locks or other state. In practice most implementations just implement
  // them without extra state. The code below overlays the atomic into the
  // SMB, hence relies on this implementation detail. This should be fine
  // pragmatically (Chrome's base makes the same assumption), but let's have a
  // check for this.
  static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t) &&
                    sizeof(std::atomic<uint16_t>) == sizeof(uint16_t),
                "Incompatible STL <atomic> implementation");

  // Chec that the kAllChunks(Complete,Free) are consistent with the
  // ChunkState enum values.

  // These must be zero because rely on zero-initialized memory being
  // interpreted as "free".
  static_assert(kChunkFree == 0 && kAllChunksFree == 0,
                "kChunkFree/kAllChunksFree and must be 0");

  static_assert((kAllChunksComplete & kChunkMask) == kChunkComplete,
                "kAllChunksComplete out of sync with kChunkComplete");

  // Sanity check the consistency of the kMax... constants.
  static_assert(sizeof(ChunkHeader::writer_id) == sizeof(WriterID),
                "WriterID size");
  ChunkHeader chunk_header{};
  chunk_header.chunk_id.store(static_cast<uint32_t>(-1));
  PERFETTO_CHECK(chunk_header.chunk_id.load() == kMaxChunkID);

  chunk_header.writer_id.store(static_cast<uint16_t>(-1));
  PERFETTO_CHECK(kMaxWriterID <= chunk_header.writer_id.load());

  PERFETTO_CHECK(page_size >= base::kPageSize);
  PERFETTO_CHECK(page_size <= kMaxPageSize);
  PERFETTO_CHECK(page_size % base::kPageSize == 0);
  PERFETTO_CHECK(reinterpret_cast<uintptr_t>(start) % base::kPageSize == 0);
  PERFETTO_CHECK(size % page_size == 0);
}

SharedMemoryABI::Chunk SharedMemoryABI::GetChunkUnchecked(size_t page_idx,
                                                          uint32_t page_layout,
                                                          size_t chunk_idx) {
  const size_t num_chunks = GetNumChunksForLayout(page_layout);
  PERFETTO_DCHECK(chunk_idx < num_chunks);
  // Compute the chunk virtual address and write it into |chunk|.
  const uint16_t chunk_size = GetChunkSizeForLayout(page_layout);
  size_t chunk_offset_in_page = sizeof(PageHeader) + chunk_idx * chunk_size;

  Chunk chunk(page_start(page_idx) + chunk_offset_in_page, chunk_size,
              static_cast<uint8_t>(chunk_idx));
  PERFETTO_DCHECK(chunk.end() <= end());
  return chunk;
}

SharedMemoryABI::Chunk SharedMemoryABI::TryAcquireChunk(
    size_t page_idx,
    size_t chunk_idx,
    ChunkState desired_chunk_state,
    const ChunkHeader* header) {
  PERFETTO_DCHECK(desired_chunk_state == kChunkBeingRead ||
                  desired_chunk_state == kChunkBeingWritten);
  PageHeader* phdr = page_header(page_idx);
  for (int attempt = 0; attempt < kRetryAttempts; attempt++) {
    uint32_t layout = phdr->layout.load(std::memory_order_acquire);
    const size_t num_chunks = GetNumChunksForLayout(layout);

    // The page layout has changed (or the page is free).
    if (chunk_idx >= num_chunks)
      return Chunk();

    // Verify that the chunk is still in a state that allows the transition to
    // |desired_chunk_state|. The only allowed transitions are:
    // 1. kChunkFree -> kChunkBeingWritten (Producer).
    // 2. kChunkComplete -> kChunkBeingRead (Service).
    ChunkState expected_chunk_state =
        desired_chunk_state == kChunkBeingWritten ? kChunkFree : kChunkComplete;
    auto cur_chunk_state = (layout >> (chunk_idx * kChunkShift)) & kChunkMask;
    if (cur_chunk_state != expected_chunk_state)
      return Chunk();

    uint32_t next_layout = layout;
    next_layout &= ~(kChunkMask << (chunk_idx * kChunkShift));
    next_layout |= (desired_chunk_state << (chunk_idx * kChunkShift));
    if (phdr->layout.compare_exchange_strong(layout, next_layout,
                                             std::memory_order_acq_rel)) {
      // Compute the chunk virtual address and write it into |chunk|.
      Chunk chunk = GetChunkUnchecked(page_idx, layout, chunk_idx);
      if (desired_chunk_state == kChunkBeingWritten) {
        PERFETTO_DCHECK(header);
        ChunkHeader* new_header = chunk.header();
        new_header->writer_id.store(header->writer_id,
                                    std::memory_order_relaxed);
        new_header->chunk_id.store(header->chunk_id, std::memory_order_relaxed);
        new_header->packets.store(header->packets, std::memory_order_release);
      }
      return chunk;
    }
    WaitBeforeNextAttempt(attempt);
  }
  return Chunk();  // All our attempts failed.
}

bool SharedMemoryABI::TryPartitionPage(size_t page_idx, PageLayout layout) {
  PERFETTO_DCHECK(layout >= kPageDiv1 && layout <= kPageDiv14);
  uint32_t expected_layout = 0;  // Free page.
  uint32_t next_layout = (layout << kLayoutShift) & kLayoutMask;
  PageHeader* phdr = page_header(page_idx);
  if (!phdr->layout.compare_exchange_strong(expected_layout, next_layout,
                                            std::memory_order_acq_rel)) {
    return false;
  }
  return true;
}

uint32_t SharedMemoryABI::GetFreeChunks(size_t page_idx) {
  uint32_t layout =
      page_header(page_idx)->layout.load(std::memory_order_relaxed);
  const uint32_t num_chunks = GetNumChunksForLayout(layout);
  uint32_t res = 0;
  for (uint32_t i = 0; i < num_chunks; i++) {
    res |= ((layout & kChunkMask) == kChunkFree) ? (1 << i) : 0;
    layout >>= kChunkShift;
  }
  return res;
}

size_t SharedMemoryABI::ReleaseChunk(Chunk chunk,
                                     ChunkState desired_chunk_state) {
  PERFETTO_DCHECK(desired_chunk_state == kChunkComplete ||
                  desired_chunk_state == kChunkFree);

  size_t page_idx;
  size_t chunk_idx;
  std::tie(page_idx, chunk_idx) = GetPageAndChunkIndex(chunk);

  // Reset header fields, so that the service can identify when the chunk's
  // header has been initialized by the producer.
  if (desired_chunk_state == kChunkFree)
    ClearChunkHeader(chunk.header());

  for (int attempt = 0; attempt < kRetryAttempts; attempt++) {
    PageHeader* phdr = page_header(page_idx);
    uint32_t layout = phdr->layout.load(std::memory_order_relaxed);
    const size_t page_chunk_size = GetChunkSizeForLayout(layout);

    // TODO(primiano): this should not be a CHECK, because a malicious producer
    // could crash us by putting the chunk in an invalid state. This should
    // gracefully fail. Keep a CHECK until then.
    PERFETTO_CHECK(chunk.size() == page_chunk_size);
    const uint32_t chunk_state =
        ((layout >> (chunk_idx * kChunkShift)) & kChunkMask);

    // Verify that the chunk is still in a state that allows the transition to
    // |desired_chunk_state|. The only allowed transitions are:
    // 1. kChunkBeingWritten -> kChunkComplete (Producer).
    // 2. kChunkBeingRead -> kChunkFree (Service).
    ChunkState expected_chunk_state;
    if (desired_chunk_state == kChunkComplete) {
      expected_chunk_state = kChunkBeingWritten;
    } else {
      expected_chunk_state = kChunkBeingRead;
    }

    // TODO(primiano): should not be a CHECK (same rationale of comment above).
    PERFETTO_CHECK(chunk_state == expected_chunk_state);
    uint32_t next_layout = layout;
    next_layout &= ~(kChunkMask << (chunk_idx * kChunkShift));
    next_layout |= (desired_chunk_state << (chunk_idx * kChunkShift));

    // If we are freeing a chunk and all the other chunks in the page are free
    // we should de-partition the page and mark it as clear.
    if ((next_layout & kAllChunksMask) == kAllChunksFree)
      next_layout = 0;

    if (phdr->layout.compare_exchange_strong(layout, next_layout,
                                             std::memory_order_acq_rel)) {
      return page_idx;
    }
    WaitBeforeNextAttempt(attempt);
  }
  // Too much contention on this page. Give up. This page will be left pending
  // forever but there isn't much more we can do at this point.
  PERFETTO_DFATAL("Too much contention on page.");
  return kInvalidPageIdx;
}

SharedMemoryABI::Chunk::Chunk() = default;

SharedMemoryABI::Chunk::Chunk(uint8_t* begin, uint16_t size, uint8_t chunk_idx)
    : begin_(begin), size_(size), chunk_idx_(chunk_idx) {
  PERFETTO_CHECK(reinterpret_cast<uintptr_t>(begin) % kChunkAlignment == 0);
  PERFETTO_CHECK(size > 0);
}

SharedMemoryABI::Chunk::Chunk(Chunk&& o) noexcept {
  *this = std::move(o);
}

SharedMemoryABI::Chunk& SharedMemoryABI::Chunk::operator=(Chunk&& o) {
  begin_ = o.begin_;
  size_ = o.size_;
  chunk_idx_ = o.chunk_idx_;
  o.begin_ = nullptr;
  o.size_ = 0;
  o.chunk_idx_ = 0;
  return *this;
}

std::pair<size_t, size_t> SharedMemoryABI::GetPageAndChunkIndex(
    const Chunk& chunk) {
  PERFETTO_DCHECK(chunk.is_valid());
  PERFETTO_DCHECK(chunk.begin() >= start_);
  PERFETTO_DCHECK(chunk.end() <= start_ + size_);

  // TODO(primiano): The divisions below could be avoided if we cached
  // |page_shift_|.
  const uintptr_t rel_addr = static_cast<uintptr_t>(chunk.begin() - start_);
  const size_t page_idx = rel_addr / page_size_;
  const size_t offset = rel_addr % page_size_;
  PERFETTO_DCHECK(offset >= sizeof(PageHeader));
  PERFETTO_DCHECK(offset % kChunkAlignment == 0);
  PERFETTO_DCHECK((offset - sizeof(PageHeader)) % chunk.size() == 0);
  const size_t chunk_idx = (offset - sizeof(PageHeader)) / chunk.size();
  PERFETTO_DCHECK(chunk_idx < kMaxChunksPerPage);
  PERFETTO_DCHECK(chunk_idx < GetNumChunksForLayout(GetPageLayout(page_idx)));
  return std::make_pair(page_idx, chunk_idx);
}

}  // namespace perfetto
