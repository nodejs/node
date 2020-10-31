/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ABI_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ABI_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bitset>
#include <thread>
#include <type_traits>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_utils.h"

namespace perfetto {

// This file defines the binary interface of the memory buffers shared between
// Producer and Service. This is a long-term stable ABI and has to be backwards
// compatible to deal with mismatching Producer and Service versions.
//
// Overview
// --------
// SMB := "Shared Memory Buffer".
// In the most typical case of a multi-process architecture (i.e. Producer and
// Service are hosted by different processes), a Producer means almost always
// a "client process producing data" (almost: in some cases a process might host
// > 1 Producer, if it links two libraries, independent of each other, that both
// use Perfetto tracing).
// The Service has one SMB for each Producer.
// A producer has one or (typically) more data sources. They all share the same
// SMB.
// The SMB is a staging area to decouple data sources living in the Producer
// and allow them to do non-blocking async writes.
// The SMB is *not* the ultimate logging buffer seen by the Consumer. That one
// is larger (~MBs) and not shared with Producers.
// Each SMB is small, typically few KB. Its size is configurable by the producer
// within a max limit of ~MB (see kMaxShmSize in tracing_service_impl.cc).
// The SMB is partitioned into fixed-size Page(s). The size of the Pages are
// determined by each Producer at connection time and cannot be changed.
// Hence, different producers can have SMB(s) that have a different Page size
// from each other, but the page size will be constant throughout all the
// lifetime of the SMB.
// Page(s) are partitioned by the Producer into variable size Chunk(s):
//
// +------------+      +--------------------------+
// | Producer 1 |  <-> |      SMB 1 [~32K - 1MB]  |
// +------------+      +--------+--------+--------+
//                     |  Page  |  Page  |  Page  |
//                     +--------+--------+--------+
//                     | Chunk  |        | Chunk  |
//                     +--------+  Chunk +--------+ <----+
//                     | Chunk  |        | Chunk  |      |
//                     +--------+--------+--------+      +---------------------+
//                                                       |       Service       |
// +------------+      +--------------------------+      +---------------------+
// | Producer 2 |  <-> |      SMB 2 [~32K - 1MB]  |     /| large ring buffers  |
// +------------+      +--------+--------+--------+ <--+ | (100K - several MB) |
//                     |  Page  |  Page  |  Page  |      +---------------------+
//                     +--------+--------+--------+
//                     | Chunk  |        | Chunk  |
//                     +--------+  Chunk +--------+
//                     | Chunk  |        | Chunk  |
//                     +--------+--------+--------+
//
// * Sizes of both SMB and ring buffers are purely indicative and decided at
// configuration time by the Producer (for SMB sizes) and the Consumer (for the
// final ring buffer size).

// Page
// ----
// A page is a portion of the shared memory buffer and defines the granularity
// of the interaction between the Producer and tracing Service. When scanning
// the shared memory buffer to determine if something should be moved to the
// central logging buffers, the Service most of the times looks at and moves
// whole pages. Similarly, the Producer sends an IPC to invite the Service to
// drain the shared memory buffer only when a whole page is filled.
// Having fixed the total SMB size (hence the total memory overhead), the page
// size is a triangular tradeoff between:
// 1) IPC traffic: smaller pages -> more IPCs.
// 2) Producer lock freedom: larger pages -> larger chunks -> data sources can
//    write more data without needing to swap chunks and synchronize.
// 3) Risk of write-starving the SMB: larger pages -> higher chance that the
//    Service won't manage to drain them and the SMB remains full.
// The page size, on the other side, has no implications on wasted memory due to
// fragmentations (see Chunk below).
// The size of the page is chosen by the Service at connection time and stays
// fixed throughout all the lifetime of the Producer. Different producers (i.e.
// ~ different client processes) can use different page sizes.
// The page size must be an integer multiple of 4k (this is to allow VM page
// stealing optimizations) and obviously has to be an integer divisor of the
// total SMB size.

// Chunk
// -----
// A chunk is a portion of a Page which is written and handled by a Producer.
// A chunk contains a linear sequence of TracePacket(s) (the root proto).
// A chunk cannot be written concurrently by two data sources. Protobufs must be
// encoded as contiguous byte streams and cannot be interleaved. Therefore, on
// the Producer side, a chunk is almost always owned exclusively by one thread
// (% extremely peculiar slow-path cases).
// Chunks are essentially single-writer single-thread lock-free arenas. Locking
// happens only when a Chunk is full and a new one needs to be acquired.
// Locking happens only within the scope of a Producer process. There is no
// inter-process locking. The Producer cannot lock the Service and viceversa.
// In the worst case, any of the two can starve the SMB, by marking all chunks
// as either being read or written. But that has the only side effect of
// losing the trace data.
// The Producer can decide to partition each page into a number of limited
// configurations (e.g., 1 page == 1 chunk, 1 page == 2 chunks and so on).

// TracePacket
// -----------
// Is the atom of tracing. Putting aside pages and chunks a trace is merely a
// sequence of TracePacket(s). TracePacket is the root protobuf message.
// A TracePacket can span across several chunks (hence even across several
// pages). A TracePacket can therefore be >> chunk size, >> page size and even
// >> SMB size. The Chunk header carries metadata to deal with the TracePacket
// splitting case.

// Use only explicitly-sized types below. DO NOT use size_t or any architecture
// dependent size (e.g. size_t) in the struct fields. This buffer will be read
// and written by processes that have a different bitness in the same OS.
// Instead it's fine to assume little-endianess. Big-endian is a dream we are
// not currently pursuing.

class SharedMemoryABI {
 public:
  // This is due to Chunk::size being 16 bits.
  static constexpr size_t kMaxPageSize = 64 * 1024;

  // "14" is the max number that can be encoded in a 32 bit atomic word using
  // 2 state bits per Chunk and leaving 4 bits for the page layout.
  // See PageLayout below.
  static constexpr size_t kMaxChunksPerPage = 14;

  // Each TracePacket in the Chunk is prefixed by a 4 bytes redundant VarInt
  // (see proto_utils.h) stating its size.
  static constexpr size_t kPacketHeaderSize = 4;

  // TraceWriter specifies this invalid packet/fragment size to signal to the
  // service that a packet should be discarded, because the TraceWriter couldn't
  // write its remaining fragments (e.g. because the SMB was exhausted).
  static constexpr size_t kPacketSizeDropPacket =
      protozero::proto_utils::kMaxMessageLength;

  // Chunk states and transitions:
  //    kChunkFree  <----------------+
  //         |  (Producer)           |
  //         V                       |
  //  kChunkBeingWritten             |
  //         |  (Producer)           |
  //         V                       |
  //  kChunkComplete                 |
  //         |  (Service)            |
  //         V                       |
  //  kChunkBeingRead                |
  //        |   (Service)            |
  //        +------------------------+
  enum ChunkState : uint32_t {
    // The Chunk is free. The Service shall never touch it, the Producer can
    // acquire it and transition it into kChunkBeingWritten.
    kChunkFree = 0,

    // The Chunk is being used by the Producer and is not complete yet.
    // The Service shall never touch kChunkBeingWritten pages.
    kChunkBeingWritten = 1,

    // The Service is moving the page into its non-shared ring buffer. The
    // Producer shall never touch kChunkBeingRead pages.
    kChunkBeingRead = 2,

    // The Producer is done writing the page and won't touch it again. The
    // Service can now move it to its non-shared ring buffer.
    // kAllChunksComplete relies on this being == 3.
    kChunkComplete = 3,
  };
  static constexpr const char* kChunkStateStr[] = {"Free", "BeingWritten",
                                                   "BeingRead", "Complete"};

  enum PageLayout : uint32_t {
    // The page is fully free and has not been partitioned yet.
    kPageNotPartitioned = 0,

    // TODO(primiano): Aligning a chunk @ 16 bytes could allow to use faster
    // intrinsics based on quad-word moves. Do the math and check what is the
    // fragmentation loss.

    // align4(X) := the largest integer N s.t. (N % 4) == 0 && N <= X.
    // 8 == sizeof(PageHeader).
    kPageDiv1 = 1,   // Only one chunk of size: PAGE_SIZE - 8.
    kPageDiv2 = 2,   // Two chunks of size: align4((PAGE_SIZE - 8) / 2).
    kPageDiv4 = 3,   // Four chunks of size: align4((PAGE_SIZE - 8) / 4).
    kPageDiv7 = 4,   // Seven chunks of size: align4((PAGE_SIZE - 8) / 7).
    kPageDiv14 = 5,  // Fourteen chunks of size: align4((PAGE_SIZE - 8) / 14).

    // The rationale for 7 and 14 above is to maximize the page usage for the
    // likely case of |page_size| == 4096:
    // (((4096 - 8) / 14) % 4) == 0, while (((4096 - 8) / 16 % 4)) == 3. So
    // Div16 would waste 3 * 16 = 48 bytes per page for chunk alignment gaps.

    kPageDivReserved1 = 6,
    kPageDivReserved2 = 7,
    kNumPageLayouts = 8,
  };

  // Keep this consistent with the PageLayout enum above.
  static constexpr uint32_t kNumChunksForLayout[] = {0, 1, 2, 4, 7, 14, 0, 0};

  // Layout of a Page.
  // +===================================================+
  // | Page header [8 bytes]                             |
  // | Tells how many chunks there are, how big they are |
  // | and their state (free, read, write, complete).    |
  // +===================================================+
  // +***************************************************+
  // | Chunk #0 header [8 bytes]                         |
  // | Tells how many packets there are and whether the  |
  // | whether the 1st and last ones are fragmented.     |
  // | Also has a chunk id to reassemble fragments.    |
  // +***************************************************+
  // +---------------------------------------------------+
  // | Packet #0 size [varint, up to 4 bytes]            |
  // + - - - - - - - - - - - - - - - - - - - - - - - - - +
  // | Packet #0 payload                                 |
  // | A TracePacket protobuf message                    |
  // +---------------------------------------------------+
  //                         ...
  // + . . . . . . . . . . . . . . . . . . . . . . . . . +
  // |      Optional padding to maintain aligment        |
  // + . . . . . . . . . . . . . . . . . . . . . . . . . +
  // +---------------------------------------------------+
  // | Packet #N size [varint, up to 4 bytes]            |
  // + - - - - - - - - - - - - - - - - - - - - - - - - - +
  // | Packet #N payload                                 |
  // | A TracePacket protobuf message                    |
  // +---------------------------------------------------+
  //                         ...
  // +***************************************************+
  // | Chunk #M header [8 bytes]                         |
  //                         ...

  // Alignment applies to start offset only. The Chunk size is *not* aligned.
  static constexpr uint32_t kChunkAlignment = 4;
  static constexpr uint32_t kChunkShift = 2;
  static constexpr uint32_t kChunkMask = 0x3;
  static constexpr uint32_t kLayoutMask = 0x70000000;
  static constexpr uint32_t kLayoutShift = 28;
  static constexpr uint32_t kAllChunksMask = 0x0FFFFFFF;

  // This assumes that kChunkComplete == 3.
  static constexpr uint32_t kAllChunksComplete = 0x0FFFFFFF;
  static constexpr uint32_t kAllChunksFree = 0;
  static constexpr size_t kInvalidPageIdx = static_cast<size_t>(-1);

  // There is one page header per page, at the beginning of the page.
  struct PageHeader {
    // |layout| bits:
    // [31] [30:28] [27:26] ... [1:0]
    //  |      |       |     |    |
    //  |      |       |     |    +---------- ChunkState[0]
    //  |      |       |     +--------------- ChunkState[12..1]
    //  |      |       +--------------------- ChunkState[13]
    //  |      +----------------------------- PageLayout (0 == page fully free)
    //  +------------------------------------ Reserved for future use
    std::atomic<uint32_t> layout;

    // If we'll ever going to use this in the future it might come handy
    // reviving the kPageBeingPartitioned logic (look in git log, it was there
    // at some point in the past).
    uint32_t reserved;
  };

  // There is one Chunk header per chunk (hence PageLayout per page) at the
  // beginning of each chunk.
  struct ChunkHeader {
    enum Flags : uint8_t {
      // If set, the first TracePacket in the chunk is partial and continues
      // from |chunk_id| - 1 (within the same |writer_id|).
      kFirstPacketContinuesFromPrevChunk = 1 << 0,

      // If set, the last TracePacket in the chunk is partial and continues on
      // |chunk_id| + 1 (within the same |writer_id|).
      kLastPacketContinuesOnNextChunk = 1 << 1,

      // If set, the last (fragmented) TracePacket in the chunk has holes (even
      // if the chunk is marked as kChunkComplete) that need to be patched
      // out-of-band before the chunk can be read.
      kChunkNeedsPatching = 1 << 2,
    };

    struct Packets {
      // Number of valid TracePacket protobuf messages contained in the chunk.
      // Each TracePacket is prefixed by its own size. This field is
      // monotonically updated by the Producer with release store semantic when
      // the packet at position |count| is started. This last packet may not be
      // considered complete until |count| is incremented for the subsequent
      // packet or the chunk is completed.
      uint16_t count : 10;
      static constexpr size_t kMaxCount = (1 << 10) - 1;

      // See Flags above.
      uint16_t flags : 6;
    };

    // A monotonic counter of the chunk within the scoped of a |writer_id|.
    // The tuple (ProducerID, WriterID, ChunkID) allows to figure out if two
    // chunks are contiguous (and hence a trace packets spanning across them can
    // be glued) or we had some holes due to the ring buffer wrapping.
    // This is set only when transitioning from kChunkFree to kChunkBeingWritten
    // and remains unchanged throughout the remaining lifetime of the chunk.
    std::atomic<uint32_t> chunk_id;

    // ID of the writer, unique within the producer.
    // Like |chunk_id|, this is set only when transitioning from kChunkFree to
    // kChunkBeingWritten.
    std::atomic<uint16_t> writer_id;

    // There is no ProducerID here. The service figures that out from the IPC
    // channel, which is unspoofable.

    // Updated with release-store semantics.
    std::atomic<Packets> packets;
  };

  class Chunk {
   public:
    Chunk();  // Constructs an invalid chunk.

    // Chunk is move-only, to document the scope of the Acquire/Release
    // TryLock operations below.
    Chunk(const Chunk&) = delete;
    Chunk operator=(const Chunk&) = delete;
    Chunk(Chunk&&) noexcept;
    Chunk& operator=(Chunk&&);

    uint8_t* begin() const { return begin_; }
    uint8_t* end() const { return begin_ + size_; }

    // Size, including Chunk header.
    size_t size() const { return size_; }

    // Begin of the first packet (or packet fragment).
    uint8_t* payload_begin() const { return begin_ + sizeof(ChunkHeader); }
    size_t payload_size() const {
      PERFETTO_DCHECK(size_ >= sizeof(ChunkHeader));
      return size_ - sizeof(ChunkHeader);
    }

    bool is_valid() const { return begin_ && size_; }

    // Index of the chunk within the page [0..13] (13 comes from kPageDiv14).
    uint8_t chunk_idx() const { return chunk_idx_; }

    ChunkHeader* header() { return reinterpret_cast<ChunkHeader*>(begin_); }

    uint16_t writer_id() {
      return header()->writer_id.load(std::memory_order_relaxed);
    }

    // Returns the count of packets and the flags with acquire-load semantics.
    std::pair<uint16_t, uint8_t> GetPacketCountAndFlags() {
      auto packets = header()->packets.load(std::memory_order_acquire);
      const uint16_t packets_count = packets.count;
      const uint8_t packets_flags = packets.flags;
      return std::make_pair(packets_count, packets_flags);
    }

    // Increases |packets.count| with release semantics (note, however, that the
    // packet count is incremented *before* starting writing a packet). Returns
    // the new packet count. The increment is atomic but NOT race-free (i.e. no
    // CAS). Only the Producer is supposed to perform this increment, and it's
    // supposed to do that in a thread-safe way (holding a lock). A Chunk cannot
    // be shared by multiple Producer threads without locking. The packet count
    // is cleared by TryAcquireChunk(), when passing the new header for the
    // chunk.
    uint16_t IncrementPacketCount() {
      ChunkHeader* chunk_header = header();
      auto packets = chunk_header->packets.load(std::memory_order_relaxed);
      packets.count++;
      chunk_header->packets.store(packets, std::memory_order_release);
      return packets.count;
    }

    // Increases |packets.count| to the given |packet_count|, but only if
    // |packet_count| is larger than the current value of |packets.count|.
    // Returns the new packet count. Same atomicity guarantees as
    // IncrementPacketCount().
    uint16_t IncreasePacketCountTo(uint16_t packet_count) {
      ChunkHeader* chunk_header = header();
      auto packets = chunk_header->packets.load(std::memory_order_relaxed);
      if (packets.count < packet_count)
        packets.count = packet_count;
      chunk_header->packets.store(packets, std::memory_order_release);
      return packets.count;
    }

    // Flags are cleared by TryAcquireChunk(), by passing the new header for
    // the chunk.
    void SetFlag(ChunkHeader::Flags flag) {
      ChunkHeader* chunk_header = header();
      auto packets = chunk_header->packets.load(std::memory_order_relaxed);
      packets.flags |= flag;
      chunk_header->packets.store(packets, std::memory_order_release);
    }

   private:
    friend class SharedMemoryABI;
    Chunk(uint8_t* begin, uint16_t size, uint8_t chunk_idx);

    // Don't add extra fields, keep the move operator fast.
    uint8_t* begin_ = nullptr;
    uint16_t size_ = 0;
    uint8_t chunk_idx_ = 0;
  };

  // Construct an instance from an existing shared memory buffer.
  SharedMemoryABI(uint8_t* start, size_t size, size_t page_size);
  SharedMemoryABI();

  void Initialize(uint8_t* start, size_t size, size_t page_size);

  uint8_t* start() const { return start_; }
  uint8_t* end() const { return start_ + size_; }
  size_t size() const { return size_; }
  size_t page_size() const { return page_size_; }
  size_t num_pages() const { return num_pages_; }
  bool is_valid() { return num_pages() > 0; }

  uint8_t* page_start(size_t page_idx) {
    PERFETTO_DCHECK(page_idx < num_pages_);
    return start_ + page_size_ * page_idx;
  }

  PageHeader* page_header(size_t page_idx) {
    return reinterpret_cast<PageHeader*>(page_start(page_idx));
  }

  // Returns true if the page is fully clear and has not been partitioned yet.
  // The state of the page can change at any point after this returns (or even
  // before). The Producer should use this only as a hint to decide out whether
  // it should TryPartitionPage() or acquire an individual chunk.
  bool is_page_free(size_t page_idx) {
    return page_header(page_idx)->layout.load(std::memory_order_relaxed) == 0;
  }

  // Returns true if all chunks in the page are kChunkComplete. As above, this
  // is advisory only. The Service is supposed to use this only to decide
  // whether to TryAcquireAllChunksForReading() or not.
  bool is_page_complete(size_t page_idx) {
    auto layout = page_header(page_idx)->layout.load(std::memory_order_relaxed);
    const uint32_t num_chunks = GetNumChunksForLayout(layout);
    if (num_chunks == 0)
      return false;  // Non partitioned pages cannot be complete.
    return (layout & kAllChunksMask) ==
           (kAllChunksComplete & ((1 << (num_chunks * kChunkShift)) - 1));
  }

  // For testing / debugging only.
  std::string page_header_dbg(size_t page_idx) {
    uint32_t x = page_header(page_idx)->layout.load(std::memory_order_relaxed);
    return std::bitset<32>(x).to_string();
  }

  // Returns the page layout, which is a bitmap that specifies the chunking
  // layout of the page and each chunk's current state. Reads with an
  // acquire-load semantic to ensure a producer's writes corresponding to an
  // update of the layout (e.g. clearing a chunk's header) are observed
  // consistently.
  uint32_t GetPageLayout(size_t page_idx) {
    return page_header(page_idx)->layout.load(std::memory_order_acquire);
  }

  // Returns a bitmap in which each bit is set if the corresponding Chunk exists
  // in the page (according to the page layout) and is free. If the page is not
  // partitioned it returns 0 (as if the page had no free chunks).
  uint32_t GetFreeChunks(size_t page_idx);

  // Tries to atomically partition a page with the given |layout|. Returns true
  // if the page was free and has been partitioned with the given |layout|,
  // false if the page wasn't free anymore by the time we got there.
  // If succeeds all the chunks are atomically set in the kChunkFree state.
  bool TryPartitionPage(size_t page_idx, PageLayout layout);

  // Tries to atomically mark a single chunk within the page as
  // kChunkBeingWritten. Returns an invalid chunk if the page is not partitioned
  // or the chunk is not in the kChunkFree state. If succeeds sets the chunk
  // header to |header|.
  Chunk TryAcquireChunkForWriting(size_t page_idx,
                                  size_t chunk_idx,
                                  const ChunkHeader* header) {
    return TryAcquireChunk(page_idx, chunk_idx, kChunkBeingWritten, header);
  }

  // Similar to TryAcquireChunkForWriting. Fails if the chunk isn't in the
  // kChunkComplete state.
  Chunk TryAcquireChunkForReading(size_t page_idx, size_t chunk_idx) {
    return TryAcquireChunk(page_idx, chunk_idx, kChunkBeingRead, nullptr);
  }

  // The caller must have successfully TryAcquireAllChunksForReading().
  Chunk GetChunkUnchecked(size_t page_idx,
                          uint32_t page_layout,
                          size_t chunk_idx);

  // Puts a chunk into the kChunkComplete state. Returns the page index.
  size_t ReleaseChunkAsComplete(Chunk chunk) {
    return ReleaseChunk(std::move(chunk), kChunkComplete);
  }

  // Puts a chunk into the kChunkFree state. Returns the page index.
  size_t ReleaseChunkAsFree(Chunk chunk) {
    return ReleaseChunk(std::move(chunk), kChunkFree);
  }

  ChunkState GetChunkState(size_t page_idx, size_t chunk_idx) {
    PageHeader* phdr = page_header(page_idx);
    uint32_t layout = phdr->layout.load(std::memory_order_relaxed);
    return GetChunkStateFromLayout(layout, chunk_idx);
  }

  std::pair<size_t, size_t> GetPageAndChunkIndex(const Chunk& chunk);

  uint16_t GetChunkSizeForLayout(uint32_t page_layout) const {
    return chunk_sizes_[(page_layout & kLayoutMask) >> kLayoutShift];
  }

  static ChunkState GetChunkStateFromLayout(uint32_t page_layout,
                                            size_t chunk_idx) {
    return static_cast<ChunkState>((page_layout >> (chunk_idx * kChunkShift)) &
                                   kChunkMask);
  }

  static constexpr uint32_t GetNumChunksForLayout(uint32_t page_layout) {
    return kNumChunksForLayout[(page_layout & kLayoutMask) >> kLayoutShift];
  }

  // Returns a bitmap in which each bit is set if the corresponding Chunk exists
  // in the page (according to the page layout) and is not free. If the page is
  // not partitioned it returns 0 (as if the page had no used chunks). Bit N
  // corresponds to Chunk N.
  static uint32_t GetUsedChunks(uint32_t page_layout) {
    const uint32_t num_chunks = GetNumChunksForLayout(page_layout);
    uint32_t res = 0;
    for (uint32_t i = 0; i < num_chunks; i++) {
      res |= ((page_layout & kChunkMask) != kChunkFree) ? (1 << i) : 0;
      page_layout >>= kChunkShift;
    }
    return res;
  }

 private:
  SharedMemoryABI(const SharedMemoryABI&) = delete;
  SharedMemoryABI& operator=(const SharedMemoryABI&) = delete;

  Chunk TryAcquireChunk(size_t page_idx,
                        size_t chunk_idx,
                        ChunkState,
                        const ChunkHeader*);
  size_t ReleaseChunk(Chunk chunk, ChunkState);

  uint8_t* start_ = nullptr;
  size_t size_ = 0;
  size_t page_size_ = 0;
  size_t num_pages_ = 0;
  std::array<uint16_t, kNumPageLayouts> chunk_sizes_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ABI_H_
