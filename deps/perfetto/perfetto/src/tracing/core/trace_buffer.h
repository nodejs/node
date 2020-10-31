/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACING_CORE_TRACE_BUFFER_H_
#define SRC_TRACING_CORE_TRACE_BUFFER_H_

#include <stdint.h>
#include <string.h>

#include <array>
#include <limits>
#include <map>
#include <tuple>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/thread_annotations.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/slice.h"
#include "perfetto/ext/tracing/core/trace_stats.h"

namespace perfetto {

class TracePacket;

// The main buffer, owned by the tracing service, where all the trace data is
// ultimately stored into. The service will own several instances of this class,
// at least one per active consumer (as defined in the |buffers| section of
// trace_config.proto) and will copy chunks from the producer's shared memory
// buffers into here when a CommitData IPC is received.
//
// Writing into the buffer
// -----------------------
// Data is copied from the SMB(s) using CopyChunkUntrusted(). The buffer will
// hence contain data coming from different producers and different writer
// sequences, more specifically:
// - The service receives data by several producer(s), identified by their ID.
// - Each producer writes several sequences identified by the same WriterID.
//   (they correspond to TraceWriter instances in the producer).
// - Each Writer writes, in order, several chunks.
// - Each chunk contains zero, one, or more TracePacket(s), or even just
//   fragments of packets (when they span across several chunks).
//
// So at any point in time, the buffer will contain a variable number of logical
// sequences identified by the {ProducerID, WriterID} tuple. Any given chunk
// will only contain packets (or fragments) belonging to the same sequence.
//
// The buffer operates by default as a ring buffer.
// It has two overwrite policies:
//  1. kOverwrite (default): if the write pointer reaches the read pointer, old
//     unread chunks will be overwritten by new chunks.
//  2. kDiscard: if the write pointer reaches the read pointer, unread chunks
//     are preserved and the new chunks are discarded. Any future write becomes
//     a no-op, even if the reader manages to fully catch up. This is because
//     once a chunk is discarded, the sequence of packets is broken and trying
//     to recover would be too hard (also due to the fact that, at the same
//     time, we allow out-of-order commits and chunk re-writes).
//
// Chunks are (over)written in the same order of the CopyChunkUntrusted() calls.
// When overwriting old content, entire chunks are overwritten or clobbered.
// The buffer never leaves a partial chunk around. Chunks' payload is copied
// as-is, but their header is not and is repacked in order to keep the
// ProducerID around.
//
// Chunks are stored in the buffer next to each other. Each chunk is prefixed by
// an inline header (ChunkRecord), which contains most of the fields of the
// SharedMemoryABI ChunkHeader + the ProducerID + the size of the payload.
// It's a conventional binary object stream essentially, where each ChunkRecord
// tells where it ends and hence where to find the next one, like this:
//
//          .-------------------------. 16 byte boundary
//          | ChunkRecord:   16 bytes |
//          | - chunk id:     4 bytes |
//          | - producer id:  2 bytes |
//          | - writer id:    2 bytes |
//          | - #fragments:   2 bytes |
//    +-----+ - record size:  2 bytes |
//    |     | - flags+pad:    4 bytes |
//    |     +-------------------------+
//    |     |                         |
//    |     :     Chunk payload       :
//    |     |                         |
//    |     +-------------------------+
//    |     |    Optional padding     |
//    +---> +-------------------------+ 16 byte boundary
//          |      ChunkRecord        |
//          :                         :
// Chunks stored in the buffer are always rounded up to 16 bytes (that is
// sizeof(ChunkRecord)), in order to avoid further inner fragmentation.
// Special "padding" chunks can be put in the buffer, e.g. in the case when we
// try to write a chunk of size N while the write pointer is at the end of the
// buffer, but the write pointer is < N bytes from the end (and hence needs to
// wrap over).
// Because of this, the buffer is self-describing: the contents of the buffer
// can be reconstructed by just looking at the buffer content (this will be
// quite useful in future to recover the buffer from crash reports).
//
// However, in order to keep some operations (patching and reading) fast, a
// lookaside index is maintained (in |index_|), keeping each chunk in the buffer
// indexed by their {ProducerID, WriterID, ChunkID} tuple.
//
// Patching data out-of-band
// -------------------------
// This buffer also supports patching chunks' payload out-of-band, after they
// have been stored. This is to allow producers to backfill the "size" fields
// of the protos that spawn across several chunks, when the previous chunks are
// returned to the service. The MaybePatchChunkContents() deals with the fact
// that a chunk might have been lost (because of wrapping) by the time the OOB
// IPC comes.
//
// Reading from the buffer
// -----------------------
// This class supports one reader only (the consumer). Reads are NOT idempotent
// as they move the read cursors around. Reading back the buffer is the most
// conceptually complex part. The ReadNextTracePacket() method operates with
// whole packet granularity. Packets are returned only when all their fragments
// are available.
// This class takes care of:
// - Gluing packets within the same sequence, even if they are not stored
//   adjacently in the buffer.
// - Re-ordering chunks within a sequence (using the ChunkID, which wraps).
// - Detecting holes in packet fragments (because of loss of chunks).
// Reads guarantee that packets for the same sequence are read in FIFO order
// (according to their ChunkID), but don't give any guarantee about the read
// order of packets from different sequences, see comments in
// ReadNextTracePacket() below.
class TraceBuffer {
 public:
  static const size_t InlineChunkHeaderSize;  // For test/fake_packet.{cc,h}.

  // See comment in the header above.
  enum OverwritePolicy { kOverwrite, kDiscard };

  // Argument for out-of-band patches applied through TryPatchChunkContents().
  struct Patch {
    // From SharedMemoryABI::kPacketHeaderSize.
    static constexpr size_t kSize = 4;

    size_t offset_untrusted;
    std::array<uint8_t, kSize> data;
  };

  // Identifiers that are constant for a packet sequence.
  struct PacketSequenceProperties {
    ProducerID producer_id_trusted;
    uid_t producer_uid_trusted;
    WriterID writer_id;
  };

  // Can return nullptr if the memory allocation fails.
  static std::unique_ptr<TraceBuffer> Create(size_t size_in_bytes,
                                             OverwritePolicy = kOverwrite);

  ~TraceBuffer();

  // Copies a Chunk from a producer Shared Memory Buffer into the trace buffer.
  // |src| points to the first packet in the SharedMemoryABI's chunk shared with
  // an untrusted producer. "untrusted" here means: the producer might be
  // malicious and might change |src| concurrently while we read it (internally
  // this method memcpy()-s first the chunk before processing it). None of the
  // arguments should be trusted, unless otherwise stated. We can trust that
  // |src| points to a valid memory area, but not its contents.
  //
  // This method may be called multiple times for the same chunk. In this case,
  // the original chunk's payload will be overridden and its number of fragments
  // and flags adjusted to match |num_fragments| and |chunk_flags|. The service
  // may use this to insert partial chunks (|chunk_complete = false|) before the
  // producer has committed them.
  //
  // If |chunk_complete| is |false|, the TraceBuffer will only consider the
  // first |num_fragments - 1| packets to be complete, since the producer may
  // not have finished writing the latest packet. Reading from a sequence will
  // also not progress past any incomplete chunks until they were rewritten with
  // |chunk_complete = true|, e.g. after a producer's commit.
  //
  // TODO(eseckler): Pass in a PacketStreamProperties instead of individual IDs.
  void CopyChunkUntrusted(ProducerID producer_id_trusted,
                          uid_t producer_uid_trusted,
                          WriterID writer_id,
                          ChunkID chunk_id,
                          uint16_t num_fragments,
                          uint8_t chunk_flags,
                          bool chunk_complete,
                          const uint8_t* src,
                          size_t size);
  // Applies a batch of |patches| to the given chunk, if the given chunk is
  // still in the buffer. Does nothing if the given ChunkID is gone.
  // Returns true if the chunk has been found and patched, false otherwise.
  // |other_patches_pending| is used to determine whether this is the only
  // batch of patches for the chunk or there is more.
  // If |other_patches_pending| == false, the chunk is marked as ready to be
  // consumed. If true, the state of the chunk is not altered.
  bool TryPatchChunkContents(ProducerID,
                             WriterID,
                             ChunkID,
                             const Patch* patches,
                             size_t patches_size,
                             bool other_patches_pending);

  // To read the contents of the buffer the caller needs to:
  //   BeginRead()
  //   while (ReadNextTracePacket(packet_fragments)) { ... }
  // No other calls to any other method should be interleaved between
  // BeginRead() and ReadNextTracePacket().
  // Reads in the TraceBuffer are NOT idempotent.
  void BeginRead();

  // Returns the next packet in the buffer, if any, and the producer_id,
  // producer_uid, and writer_id of the producer/writer that wrote it (as passed
  // in the CopyChunkUntrusted() call). Returns false if no packets can be read
  // at this point. If a packet was read successfully,
  // |previous_packet_on_sequence_dropped| is set to |true| if the previous
  // packet on the sequence was dropped from the buffer before it could be read
  // (e.g. because its chunk was overridden due to the ring buffer wrapping or
  // due to an ABI violation), and to |false| otherwise.
  //
  // This function returns only complete packets. Specifically:
  // When there is at least one complete packet in the buffer, this function
  // returns true and populates the TracePacket argument with the boundaries of
  // each fragment for one packet.
  // TracePacket will have at least one slice when this function returns true.
  // When there are no whole packets eligible to read (e.g. we are still missing
  // fragments) this function returns false.
  // This function guarantees also that packets for a given
  // {ProducerID, WriterID} are read in FIFO order.
  // This function does not guarantee any ordering w.r.t. packets belonging to
  // different WriterID(s). For instance, given the following packets copied
  // into the buffer:
  //   {ProducerID: 1, WriterID: 1}: P1 P2 P3
  //   {ProducerID: 1, WriterID: 2}: P4 P5 P6
  //   {ProducerID: 2, WriterID: 1}: P7 P8 P9
  // The following read sequence is possible:
  //   P1, P4, P7, P2, P3, P5, P8, P9, P6
  // But the following is guaranteed to NOT happen:
  //   P1, P5, P7, P4 (P4 cannot come after P5)
  bool ReadNextTracePacket(TracePacket*,
                           PacketSequenceProperties* sequence_properties,
                           bool* previous_packet_on_sequence_dropped);

  const TraceStats::BufferStats& stats() const { return stats_; }
  size_t size() const { return size_; }

 private:
  friend class TraceBufferTest;

  // ChunkRecord is a Chunk header stored inline in the |data_| buffer, before
  // the chunk payload (the packets' data). The |data_| buffer looks like this:
  // +---------------+------------------++---------------+-----------------+
  // | ChunkRecord 1 | Chunk payload 1  || ChunkRecord 2 | Chunk payload 2 | ...
  // +---------------+------------------++---------------+-----------------+
  // Most of the ChunkRecord fields are copied from SharedMemoryABI::ChunkHeader
  // (the chunk header used in the shared memory buffers).
  // A ChunkRecord can be a special "padding" record. In this case its payload
  // should be ignored and the record should be just skipped.
  //
  // Full page move optimization:
  // This struct has to be exactly (sizeof(PageHeader) + sizeof(ChunkHeader))
  // (from shared_memory_abi.h) to allow full page move optimizations
  // (TODO(primiano): not implemented yet). In the special case of moving a full
  // 4k page that contains only one chunk, in fact, we can just ask the kernel
  // to move the full SHM page (see SPLICE_F_{GIFT,MOVE}) and overlay the
  // ChunkRecord on top of the moved SMB's header (page + chunk header).
  // This special requirement is covered by static_assert(s) in the .cc file.
  struct ChunkRecord {
    explicit ChunkRecord(size_t sz) : flags{0}, is_padding{0} {
      PERFETTO_DCHECK(sz >= sizeof(ChunkRecord) &&
                      sz % sizeof(ChunkRecord) == 0 && sz <= kMaxSize);
      size = static_cast<decltype(size)>(sz);
    }

    bool is_valid() const { return size != 0; }

    // Keep this structure packed and exactly 16 bytes (128 bits) big.

    // [32 bits] Monotonic counter within the same writer_id.
    ChunkID chunk_id = 0;

    // [16 bits] ID of the Producer from which the Chunk was copied from.
    ProducerID producer_id = 0;

    // [16 bits] Unique per Producer (but not within the service).
    // If writer_id == kWriterIdPadding the record should just be skipped.
    WriterID writer_id = 0;

    // Number of fragments contained in the chunk.
    uint16_t num_fragments = 0;

    // Size in bytes, including sizeof(ChunkRecord) itself.
    uint16_t size;

    uint8_t flags : 6;  // See SharedMemoryABI::ChunkHeader::flags.
    uint8_t is_padding : 1;
    uint8_t unused_flag : 1;

    // Not strictly needed, can be reused for more fields in the future. But
    // right now helps to spot chunks in hex dumps.
    char unused[3] = {'C', 'H', 'U'};

    static constexpr size_t kMaxSize =
        std::numeric_limits<decltype(size)>::max();
  };

  // Lookaside index entry. This serves two purposes:
  // 1) Allow a fast lookup of ChunkRecord by their ID (the tuple
  //   {ProducerID, WriterID, ChunkID}). This is used when applying out-of-band
  //   patches to the contents of the chunks after they have been copied into
  //   the TraceBuffer.
  // 2) keep the chunks ordered by their ID. This is used when reading back.
  // 3) Keep metadata about the status of the chunk, e.g. whether the contents
  //    have been read already and should be skipped in a future read pass.
  // This struct should not have any field that is essential for reconstructing
  // the contents of the buffer from a crash dump.
  struct ChunkMeta {
    // Key used for sorting in the map.
    struct Key {
      Key(ProducerID p, WriterID w, ChunkID c)
          : producer_id{p}, writer_id{w}, chunk_id{c} {}

      explicit Key(const ChunkRecord& cr)
          : Key(cr.producer_id, cr.writer_id, cr.chunk_id) {}

      // Note that this sorting doesn't keep into account the fact that ChunkID
      // will wrap over at some point. The extra logic in SequenceIterator deals
      // with that.
      bool operator<(const Key& other) const {
        return std::tie(producer_id, writer_id, chunk_id) <
               std::tie(other.producer_id, other.writer_id, other.chunk_id);
      }

      bool operator==(const Key& other) const {
        return std::tie(producer_id, writer_id, chunk_id) ==
               std::tie(other.producer_id, other.writer_id, other.chunk_id);
      }

      bool operator!=(const Key& other) const { return !(*this == other); }

      // These fields should match at all times the corresponding fields in
      // the |chunk_record|. They are copied here purely for efficiency to avoid
      // dereferencing the buffer all the time.
      ProducerID producer_id;
      WriterID writer_id;
      ChunkID chunk_id;
    };

    enum IndexFlags : uint8_t {
      // If set, the chunk state was kChunkComplete at the time it was copied.
      // If unset, the chunk was still kChunkBeingWritten while copied. When
      // reading from the chunk's sequence, the sequence will not advance past
      // this chunk until this flag is set.
      kComplete = 1 << 0,

      // If set, we skipped the last packet that we read from this chunk e.g.
      // because we it was a continuation from a previous chunk that was dropped
      // or due to an ABI violation.
      kLastReadPacketSkipped = 1 << 1
    };

    ChunkMeta(ChunkRecord* r, uint16_t p, bool complete, uint8_t f, uid_t u)
        : chunk_record{r}, trusted_uid{u}, flags{f}, num_fragments{p} {
      if (complete)
        index_flags = kComplete;
    }

    bool is_complete() const { return index_flags & kComplete; }

    void set_complete(bool complete) {
      if (complete) {
        index_flags |= kComplete;
      } else {
        index_flags &= ~kComplete;
      }
    }

    bool last_read_packet_skipped() const {
      return index_flags & kLastReadPacketSkipped;
    }

    void set_last_read_packet_skipped(bool skipped) {
      if (skipped) {
        index_flags |= kLastReadPacketSkipped;
      } else {
        index_flags &= ~kLastReadPacketSkipped;
      }
    }

    ChunkRecord* const chunk_record;  // Addr of ChunkRecord within |data_|.
    const uid_t trusted_uid;          // uid of the producer.

    // Flags set by TraceBuffer to track the state of the chunk in the index.
    uint8_t index_flags = 0;

    // Correspond to |chunk_record->flags| and |chunk_record->num_fragments|.
    // Copied here for performance reasons (avoids having to dereference
    // |chunk_record| while iterating over ChunkMeta) and to aid debugging in
    // case the buffer gets corrupted.
    uint8_t flags = 0;           // See SharedMemoryABI::ChunkHeader::flags.
    uint16_t num_fragments = 0;  // Total number of packet fragments.

    uint16_t num_fragments_read = 0;  // Number of fragments already read.

    // The start offset of the next fragment (the |num_fragments_read|-th) to be
    // read. This is the offset in bytes from the beginning of the ChunkRecord's
    // payload (the 1st fragment starts at |chunk_record| +
    // sizeof(ChunkRecord)).
    uint16_t cur_fragment_offset = 0;
  };

  using ChunkMap = std::map<ChunkMeta::Key, ChunkMeta>;

  // Allows to iterate over a sub-sequence of |index_| for all keys belonging to
  // the same {ProducerID,WriterID}. Furthermore takes into account the wrapping
  // of ChunkID. Instances are valid only as long as the |index_| is not altered
  // (can be used safely only between adjacent ReadNextTracePacket() calls).
  // The order of the iteration will proceed in the following order:
  // |wrapping_id| + 1 -> |seq_end|, |seq_begin| -> |wrapping_id|.
  // Practical example:
  // - Assume that kMaxChunkID == 7
  // - Assume that we have all 8 chunks in the range (0..7).
  // - Hence, |seq_begin| == c0, |seq_end| == c7
  // - Assume |wrapping_id| = 4 (c4 is the last chunk copied over
  //   through a CopyChunkUntrusted()).
  // The resulting iteration order will be: c5, c6, c7, c0, c1, c2, c3, c4.
  struct SequenceIterator {
    // Points to the 1st key (the one with the numerically min ChunkID).
    ChunkMap::iterator seq_begin;

    // Points one past the last key (the one with the numerically max ChunkID).
    ChunkMap::iterator seq_end;

    // Current iterator, always >= seq_begin && <= seq_end.
    ChunkMap::iterator cur;

    // The latest ChunkID written. Determines the start/end of the sequence.
    ChunkID wrapping_id;

    bool is_valid() const { return cur != seq_end; }

    ProducerID producer_id() const {
      PERFETTO_DCHECK(is_valid());
      return cur->first.producer_id;
    }

    WriterID writer_id() const {
      PERFETTO_DCHECK(is_valid());
      return cur->first.writer_id;
    }

    ChunkID chunk_id() const {
      PERFETTO_DCHECK(is_valid());
      return cur->first.chunk_id;
    }

    ChunkMeta& operator*() {
      PERFETTO_DCHECK(is_valid());
      return cur->second;
    }

    // Moves |cur| to the next chunk in the index.
    // is_valid() will become false after calling this, if this was the last
    // entry of the sequence.
    void MoveNext();

    void MoveToEnd() { cur = seq_end; }
  };

  enum class ReadAheadResult {
    kSucceededReturnSlices,
    kFailedMoveToNextSequence,
    kFailedStayOnSameSequence,
  };

  enum class ReadPacketResult {
    kSucceeded,
    kFailedInvalidPacket,
    kFailedEmptyPacket,
  };

  explicit TraceBuffer(OverwritePolicy);
  TraceBuffer(const TraceBuffer&) = delete;
  TraceBuffer& operator=(const TraceBuffer&) = delete;

  bool Initialize(size_t size);

  // Returns an object that allows to iterate over chunks in the |index_| that
  // have the same {ProducerID, WriterID} of
  // |seq_begin.first.{producer,writer}_id|. |seq_begin| must be an iterator to
  // the first entry in the |index_| that has a different {ProducerID, WriterID}
  // from the previous one. It is valid for |seq_begin| to be == index_.end()
  // (i.e. if the index is empty). The iteration takes care of ChunkID wrapping,
  // by using |last_chunk_id_|.
  SequenceIterator GetReadIterForSequence(ChunkMap::iterator seq_begin);

  // Used as a last resort when a buffer corruption is detected.
  void ClearContentsAndResetRWCursors();

  // Adds a padding record of the given size (must be a multiple of
  // sizeof(ChunkRecord)).
  void AddPaddingRecord(size_t);

  // Look for contiguous fragment of the same packet starting from |read_iter_|.
  // If a contiguous packet is found, all the fragments are pushed into
  // TracePacket and the function returns kSucceededReturnSlices. If not, the
  // function returns either kFailedMoveToNextSequence or
  // kFailedStayOnSameSequence, telling the caller to continue looking for
  // packets.
  ReadAheadResult ReadAhead(TracePacket*);

  // Deletes (by marking the record invalid and removing form the index) all
  // chunks from |wptr_| to |wptr_| + |bytes_to_clear|.
  // Returns:
  //   * The size of the gap left between the next valid Chunk and the end of
  //     the deletion range.
  //   * 0 if no next valid chunk exists (if the buffer is still zeroed).
  //   * -1 if the buffer |overwrite_policy_| == kDiscard and the deletion would
  //     cause unread chunks to be overwritten. In this case the buffer is left
  //     untouched.
  // Graphically, assume the initial situation is the following (|wptr_| = 10).
  // |0        |10 (wptr_)       |30       |40                 |60
  // +---------+-----------------+---------+-------------------+---------+
  // | Chunk 1 | Chunk 2         | Chunk 3 | Chunk 4           | Chunk 5 |
  // +---------+-----------------+---------+-------------------+---------+
  //           |_________Deletion range_______|~~return value~~|
  //
  // A call to DeleteNextChunksFor(32) will remove chunks 2,3,4 and return 18
  // (60 - 42), the distance between chunk 5 and the end of the deletion range.
  ssize_t DeleteNextChunksFor(size_t bytes_to_clear);

  // Decodes the boundaries of the next packet (or a fragment) pointed by
  // ChunkMeta and pushes that into |TracePacket|. It also increments the
  // |num_fragments_read| counter.
  // TracePacket can be nullptr, in which case the read state is still advanced.
  // When TracePacket is not nullptr, ProducerID must also be not null and will
  // be updated with the ProducerID that originally wrote the chunk.
  ReadPacketResult ReadNextPacketInChunk(ChunkMeta*, TracePacket*);

  void DcheckIsAlignedAndWithinBounds(const uint8_t* ptr) const {
    PERFETTO_DCHECK(ptr >= begin() && ptr <= end() - sizeof(ChunkRecord));
    PERFETTO_DCHECK(
        (reinterpret_cast<uintptr_t>(ptr) & (alignof(ChunkRecord) - 1)) == 0);
  }

  ChunkRecord* GetChunkRecordAt(uint8_t* ptr) {
    DcheckIsAlignedAndWithinBounds(ptr);
    // We may be accessing a new (empty) record.
    data_.EnsureCommitted(
        static_cast<size_t>(ptr + sizeof(ChunkRecord) - begin()));
    return reinterpret_cast<ChunkRecord*>(ptr);
  }

  void DiscardWrite();

  // |src| can be nullptr (in which case |size| must be ==
  // record.size - sizeof(ChunkRecord)), for the case of writing a padding
  // record. |wptr_| is NOT advanced by this function, the caller must do that.
  void WriteChunkRecord(uint8_t* wptr,
                        const ChunkRecord& record,
                        const uint8_t* src,
                        size_t size) {
    // Note: |record.size| will be slightly bigger than |size| because of the
    // ChunkRecord header and rounding, to ensure that all ChunkRecord(s) are
    // multiple of sizeof(ChunkRecord). The invariant is:
    // record.size >= |size| + sizeof(ChunkRecord) (== if no rounding).
    PERFETTO_DCHECK(size <= ChunkRecord::kMaxSize);
    PERFETTO_DCHECK(record.size >= sizeof(record));
    PERFETTO_DCHECK(record.size % sizeof(record) == 0);
    PERFETTO_DCHECK(record.size >= size + sizeof(record));
    PERFETTO_CHECK(record.size <= size_to_end());
    DcheckIsAlignedAndWithinBounds(wptr);

    // We may be writing to this area for the first time.
    data_.EnsureCommitted(static_cast<size_t>(wptr + record.size - begin()));

    // Deliberately not a *D*CHECK.
    PERFETTO_CHECK(wptr + sizeof(record) + size <= end());
    memcpy(wptr, &record, sizeof(record));
    if (PERFETTO_LIKELY(src)) {
      // If the producer modifies the data in the shared memory buffer while we
      // are copying it to the central buffer, TSAN will (rightfully) flag that
      // as a race. However the entire purpose of copying the data into the
      // central buffer is that we can validate it without worrying that the
      // producer changes it from under our feet, so this race is benign. The
      // alternative would be to try computing which part of the buffer is safe
      // to read (assuming a well-behaving client), but the risk of introducing
      // a bug that way outweighs the benefit.
      PERFETTO_ANNOTATE_BENIGN_RACE_SIZED(
          src, size, "Benign race when copying chunk from shared memory.")
      memcpy(wptr + sizeof(record), src, size);
    } else {
      PERFETTO_DCHECK(size == record.size - sizeof(record));
    }
    const size_t rounding_size = record.size - sizeof(record) - size;
    memset(wptr + sizeof(record) + size, 0, rounding_size);
  }

  uint8_t* begin() const { return reinterpret_cast<uint8_t*>(data_.Get()); }
  uint8_t* end() const { return begin() + size_; }
  size_t size_to_end() const { return static_cast<size_t>(end() - wptr_); }

  base::PagedMemory data_;
  size_t size_ = 0;            // Size in bytes of |data_|.
  size_t max_chunk_size_ = 0;  // Max size in bytes allowed for a chunk.
  uint8_t* wptr_ = nullptr;    // Write pointer.

  // An index that keeps track of the positions and metadata of each
  // ChunkRecord.
  ChunkMap index_;

  // Read iterator used for ReadNext(). It is reset by calling BeginRead().
  // It becomes invalid after any call to methods that alters the |index_|.
  SequenceIterator read_iter_;

  // See comments at the top of the file.
  OverwritePolicy overwrite_policy_ = kOverwrite;

  // Only used when |overwrite_policy_ == kDiscard|. This is set the first time
  // a write fails because it would overwrite unread chunks.
  bool discard_writes_ = false;

  // Keeps track of the highest ChunkID written for a given sequence, taking
  // into account a potential overflow of ChunkIDs. In the case of overflow,
  // stores the highest ChunkID written since the overflow.
  //
  // TODO(primiano): should clean up keys from this map. Right now it grows
  // without bounds (although realistically is not a problem unless we have too
  // many producers/writers within the same trace session).
  std::map<std::pair<ProducerID, WriterID>, ChunkID> last_chunk_id_written_;

  // Statistics about buffer usage.
  TraceStats::BufferStats stats_;

#if PERFETTO_DCHECK_IS_ON()
  bool changed_since_last_read_ = false;
#endif

  // When true disable some DCHECKs that have been put in place to detect
  // bugs in the producers. This is for tests that feed malicious inputs and
  // hence mimic a buggy producer.
  bool suppress_sanity_dchecks_for_testing_ = false;
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_TRACE_BUFFER_H_
