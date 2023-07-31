// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-serializer.h"

#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/read-only-serializer-deserializer.h"

namespace v8 {
namespace internal {

namespace {

struct ReadOnlySegmentForSerialization {
  ReadOnlySegmentForSerialization(Isolate* isolate, const ReadOnlyPage* page,
                                  Address segment_start, size_t segment_size)
      : page(page),
        segment_start(segment_start),
        segment_size(segment_size),
        segment_offset(segment_start - page->area_start()),
        contents(new uint8_t[segment_size]),
        tagged_slots(segment_size / kTaggedSize) {
    // .. because tagged_slots records a bit for each slot:
    DCHECK(IsAligned(segment_size, kTaggedSize));

    MemCopy(contents.get(), reinterpret_cast<void*>(segment_start),
            segment_size);
    WipeCodeInstructionStart(isolate);
    if (!V8_STATIC_ROOTS_BOOL) EncodeTaggedSlots(isolate);
  }

  void WipeCodeInstructionStart(Isolate* isolate) {
    // Iterate the RO page and the contents copy in lockstep, wiping fields
    // in contents as we go along.
    //
    // See also ObjectSerializer::OutputRawData.
    DCHECK_GE(segment_start, page->area_start());
    const Address segment_end = segment_start + segment_size;
    ReadOnlyPageObjectIterator it(page, segment_start);
    for (HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
      if (o.address() >= segment_end) break;
      if (!o.IsCode()) continue;

      size_t o_offset = o.ptr() - segment_start;
      Address o_dst = reinterpret_cast<Address>(contents.get()) + o_offset;
      Code code = Code::cast(Object(o_dst));
      code.ClearInstructionStartForSerialization(isolate);
    }
  }

  void EncodeTaggedSlots(Isolate* isolate);

  const ReadOnlyPage* const page;
  const Address segment_start;
  const size_t segment_size;
  const size_t segment_offset;
  // The (mutated) off-heap copy of the on-heap segment.
  std::unique_ptr<uint8_t[]> contents;
  // The relocation table.
  ro::BitSet tagged_slots;

  friend class EncodeRelocationsVisitor;
};

ro::EncodedTagged_t Encode(Isolate* isolate, HeapObject o) {
  Address o_address = o.address();
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromAddress(o_address);

  int page_index = 0;
  for (ReadOnlyPage* page : isolate->heap()->read_only_space()->pages()) {
    if (chunk == page) break;
    ++page_index;
  }

  ro::EncodedTagged_t encoded;
  DCHECK_LT(page_index, 1UL << ro::EncodedTagged_t::kPageIndexBits);
  encoded.page_index = page_index;
  uint32_t chunk_offset = static_cast<int>(chunk->Offset(o_address));
  DCHECK(IsAligned(chunk_offset, kTaggedSize));
  DCHECK_LT(chunk_offset / kTaggedSize,
            1UL << ro::EncodedTagged_t::kOffsetBits);
  encoded.offset = chunk_offset / kTaggedSize;

  return encoded;
}

// If relocations are needed, this class
// - encodes all tagged slots s.t. valid pointers can be reconstructed during
//   deserialization, and
// - records the location of all tagged slots in a table.
class EncodeRelocationsVisitor final : public ObjectVisitor {
 public:
  EncodeRelocationsVisitor(Isolate* isolate,
                           ReadOnlySegmentForSerialization* segment)
      : isolate_(isolate), segment_(segment) {
    DCHECK(!V8_STATIC_ROOTS_BOOL);
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    for (MaybeObjectSlot slot = start; slot < end; slot++) {
      ProcessSlot(slot);
    }
  }

  void VisitMapPointer(HeapObject host) override {
    ProcessSlot(host.RawMaybeWeakField(HeapObject::kMapOffset));
  }

  // Sanity-checks:
  void VisitInstructionStreamPointer(Code host,
                                     InstructionStreamSlot slot) override {
    // RO space contains only builtin Code objects.
    DCHECK(!host.has_instruction_stream());
  }
  void VisitCodeTarget(InstructionStream, RelocInfo*) override {
    UNREACHABLE();
  }
  void VisitEmbeddedPointer(InstructionStream, RelocInfo*) override {
    UNREACHABLE();
  }
  void VisitExternalReference(InstructionStream, RelocInfo*) override {
    UNREACHABLE();
  }
  void VisitInternalReference(InstructionStream, RelocInfo*) override {
    UNREACHABLE();
  }
  void VisitOffHeapTarget(InstructionStream, RelocInfo*) override {
    UNREACHABLE();
  }
  void VisitExternalPointer(HeapObject, ExternalPointerSlot,
                            ExternalPointerTag) override {
    UNREACHABLE();
  }

 private:
  void ProcessSlot(MaybeObjectSlot slot) {
    MaybeObject o = *slot;
    if (!o.IsStrongOrWeak()) return;  // Smis don't need relocation.
    DCHECK(o.IsStrong());

    int slot_offset = SegmentOffsetOf(slot);
    DCHECK(IsAligned(slot_offset, kTaggedSize));

    // Encode:
    ro::EncodedTagged_t encoded = Encode(isolate_, o.GetHeapObject());
    memcpy(segment_->contents.get() + slot_offset, &encoded,
           ro::EncodedTagged_t::kSize);

    // Record:
    segment_->tagged_slots.set(AsSlot(slot_offset));
  }

  int SegmentOffsetOf(MaybeObjectSlot slot) {
    Address addr = slot.address();
    DCHECK_GE(addr, segment_->segment_start);
    DCHECK_LT(addr, segment_->segment_start + segment_->segment_size);
    return static_cast<int>(addr - segment_->segment_start);
  }

  static constexpr int AsSlot(int byte_offset) {
    return byte_offset / kTaggedSize;
  }

  Isolate* const isolate_;
  ReadOnlySegmentForSerialization* const segment_;
};

void ReadOnlySegmentForSerialization::EncodeTaggedSlots(Isolate* isolate) {
  DCHECK(!V8_STATIC_ROOTS_BOOL);
  EncodeRelocationsVisitor v(isolate, this);
  PtrComprCageBase cage_base(isolate);

  DCHECK_GE(segment_start, page->area_start());
  const Address segment_end = segment_start + segment_size;
  ReadOnlyPageObjectIterator it(page, segment_start,
                                SkipFreeSpaceOrFiller::kNo);
  for (HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
    if (o.address() >= segment_end) break;
    o.Iterate(cage_base, &v);
  }
}

class ReadOnlyHeapImageSerializer {
 public:
  struct MemoryRegion {
    Address start;
    size_t size;
  };

  static void Serialize(Isolate* isolate, SnapshotByteSink* sink,
                        const std::vector<MemoryRegion>& unmapped_regions) {
    ReadOnlyHeapImageSerializer{isolate, sink}.SerializeImpl(unmapped_regions);
  }

 private:
  using Bytecode = ro::Bytecode;

  ReadOnlyHeapImageSerializer(Isolate* isolate, SnapshotByteSink* sink)
      : isolate_(isolate), sink_(sink) {}

  void SerializeImpl(const std::vector<MemoryRegion>& unmapped_regions) {
    DCHECK_EQ(sink_->Position(), 0);

    ReadOnlySpace* ro_space = isolate_->read_only_heap()->read_only_space();
    for (const ReadOnlyPage* page : ro_space->pages()) {
      WritePage(page, unmapped_regions);
    }

    SerializeReadOnlyRootsTable();
    sink_->Put(Bytecode::kFinalizeReadOnlySpace, "space end");
  }

  void WritePage(const ReadOnlyPage* page,
                 const std::vector<MemoryRegion>& unmapped_regions) {
    sink_->Put(Bytecode::kPage, "page begin");
    if (V8_STATIC_ROOTS_BOOL) {
      auto page_addr = reinterpret_cast<Address>(page);
      sink_->PutInt(V8HeapCompressionScheme::CompressAny(page_addr),
                    "page start offset");
    }

    Address pos = page->area_start();

    // If this page contains unmapped regions split it into multiple segments.
    for (auto r = unmapped_regions.begin(); r != unmapped_regions.end(); ++r) {
      // Regions must be sorted and non-overlapping.
      if (r + 1 != unmapped_regions.end()) {
        CHECK(r->start < (r + 1)->start);
        CHECK(r->start + r->size < (r + 1)->start);
      }
      if (base::IsInRange(r->start, pos, page->HighWaterMark())) {
        size_t segment_size = r->start - pos;
        ReadOnlySegmentForSerialization segment(isolate_, page, pos,
                                                segment_size);
        WriteSegment(&segment);
        pos += segment_size + r->size;
      }
    }

    // Pages are shrunk, but memory at the end of the area is still
    // uninitialized and we do not want to include it in the snapshot.
    size_t segment_size = page->HighWaterMark() - pos;
    ReadOnlySegmentForSerialization segment(isolate_, page, pos, segment_size);
    WriteSegment(&segment);

    sink_->Put(Bytecode::kFinalizePage, "page end");
  }

  void WriteSegment(const ReadOnlySegmentForSerialization* segment) {
    sink_->Put(Bytecode::kSegment, "segment begin");
    sink_->PutInt(segment->segment_offset, "segment start offset");
    sink_->PutInt(segment->segment_size, "segment byte size");
    sink_->PutRaw(segment->contents.get(),
                  static_cast<int>(segment->segment_size), "page");
    if (!V8_STATIC_ROOTS_BOOL) {
      sink_->Put(Bytecode::kRelocateSegment, "relocate segment");
      sink_->PutRaw(segment->tagged_slots.data(),
                    static_cast<int>(segment->tagged_slots.size_in_bytes()),
                    "tagged_slots");
    }
  }

  void SerializeReadOnlyRootsTable() {
    sink_->Put(Bytecode::kReadOnlyRootsTable, "read only roots table");
    if (!V8_STATIC_ROOTS_BOOL) {
      ReadOnlyRoots roots(isolate_);
      for (size_t i = 0; i < ReadOnlyRoots::kEntriesCount; i++) {
        RootIndex rudi = static_cast<RootIndex>(i);
        HeapObject rudolf = HeapObject::cast(roots.object_at(rudi));
        ro::EncodedTagged_t encoded = Encode(isolate_, rudolf);
        sink_->PutInt(encoded.ToUint32(), "read only roots entry");
      }
    }
  }

  Isolate* const isolate_;
  SnapshotByteSink* const sink_;
};

std::vector<ReadOnlyHeapImageSerializer::MemoryRegion> GetUnmappedRegions(
    Isolate* isolate) {
#ifdef V8_STATIC_ROOTS
  // WasmNull's payload is aligned to the OS page and consists of
  // WasmNull::kPayloadSize bytes of unmapped memory. To avoid inflating the
  // snapshot size and accessing uninitialized and/or unmapped memory, the
  // serializer skips the padding bytes and the payload.
  ReadOnlyRoots ro_roots(isolate);
  WasmNull wasm_null = ro_roots.wasm_null();
  HeapObject wasm_null_padding = ro_roots.wasm_null_padding();
  CHECK(wasm_null_padding.IsFreeSpace());
  Address wasm_null_padding_start =
      wasm_null_padding.address() + FreeSpace::kHeaderSize;
  std::vector<ReadOnlyHeapImageSerializer::MemoryRegion> unmapped;
  if (wasm_null.address() > wasm_null_padding_start) {
    unmapped.push_back({wasm_null_padding_start,
                        wasm_null.address() - wasm_null_padding_start});
  }
  unmapped.push_back({wasm_null.payload(), WasmNull::kPayloadSize});
  return unmapped;
#else
  return {};
#endif  // V8_STATIC_ROOTS
}

}  // namespace

ReadOnlySerializer::ReadOnlySerializer(Isolate* isolate,
                                       Snapshot::SerializerFlags flags)
    : RootsSerializer(isolate, flags, RootIndex::kFirstReadOnlyRoot) {}

ReadOnlySerializer::~ReadOnlySerializer() {
  OutputStatistics("ReadOnlySerializer");
}

void ReadOnlySerializer::Serialize() {
  DisallowGarbageCollection no_gc;
  ReadOnlyHeapImageSerializer::Serialize(isolate(), &sink_,
                                         GetUnmappedRegions(isolate()));

  ReadOnlyHeapObjectIterator it(isolate()->read_only_heap());
  for (HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
    CheckRehashability(o);
    if (v8_flags.serialization_statistics) {
      CountAllocation(o.map(), o.Size(), SnapshotSpace::kReadOnlyHeap);
    }
  }
}

}  // namespace internal
}  // namespace v8
