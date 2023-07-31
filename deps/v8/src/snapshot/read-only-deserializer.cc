// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-deserializer.h"

#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/logging/counters-scopes.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/snapshot/read-only-serializer-deserializer.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

class ReadOnlyHeapImageDeserializer final {
 public:
  static void Deserialize(Isolate* isolate, SnapshotByteSource* source) {
    ReadOnlyHeapImageDeserializer{isolate, source}.DeserializeImpl();
  }

 private:
  using Bytecode = ro::Bytecode;

  ReadOnlyHeapImageDeserializer(Isolate* isolate, SnapshotByteSource* source)
      : source_(source), isolate_(isolate) {}

  void DeserializeImpl() {
    while (true) {
      int bytecode_as_int = source_->Get();
      DCHECK_LT(bytecode_as_int, ro::kNumberOfBytecodes);
      switch (static_cast<Bytecode>(bytecode_as_int)) {
        case Bytecode::kPage:
          DeserializeReadOnlyPage();
          break;
        case Bytecode::kSegment:
          DeserializeReadOnlySegment();
          break;
        case Bytecode::kRelocateSegment:
          UNREACHABLE();  // Handled together with kSegment.
        case Bytecode::kFinalizePage:
          ro_space()->FinalizeExternallyInitializedPage();
          break;
        case Bytecode::kReadOnlyRootsTable:
          DeserializeReadOnlyRootsTable();
          break;
        case Bytecode::kFinalizeReadOnlySpace:
          ro_space()->FinalizeExternallyInitializedSpace();
          return;
      }
    }
  }

  void DeserializeReadOnlyPage() {
    if (V8_STATIC_ROOTS_BOOL) {
      Address pos = isolate_->GetPtrComprCage()->base() + source_->GetInt();
      ro_space()->AllocateNextPageAt(pos);
    } else {
      ro_space()->AllocateNextPage();
    }
  }

  void DeserializeReadOnlySegment() {
    ReadOnlyPage* cur_page = ro_space()->pages().back();

    // Copy over raw contents.
    Address start = cur_page->area_start() + source_->GetInt();
    int size_in_bytes = source_->GetInt();
    CHECK_LE(start + size_in_bytes, cur_page->area_end());
    source_->CopyRaw(reinterpret_cast<void*>(start), size_in_bytes);
    ro_space()->top_ = start + size_in_bytes;

    if (!V8_STATIC_ROOTS_BOOL) {
      uint8_t relocate_marker_bytecode = source_->Get();
      CHECK_EQ(relocate_marker_bytecode, Bytecode::kRelocateSegment);
      int tagged_slots_size_in_bits = size_in_bytes / kTaggedSize;
      // The const_cast is unfortunate, but we promise not to mutate data.
      uint8_t* data =
          const_cast<uint8_t*>(source_->data() + source_->position());
      ro::BitSet tagged_slots(data, tagged_slots_size_in_bits);
      DecodeTaggedSlots(start, tagged_slots);
      source_->Advance(static_cast<int>(tagged_slots.size_in_bytes()));
    }
  }

  Address Decode(ro::EncodedTagged_t encoded) const {
    DCHECK_LT(encoded.page_index, static_cast<int>(ro_space()->pages().size()));
    ReadOnlyPage* page = ro_space()->pages()[encoded.page_index];
    return page->OffsetToAddress(encoded.offset * kTaggedSize);
  }

  void DecodeTaggedSlots(Address segment_start,
                         const ro::BitSet& tagged_slots) {
    DCHECK(!V8_STATIC_ROOTS_BOOL);
    for (size_t i = 0; i < tagged_slots.size_in_bits(); i++) {
      // TODO(jgruber): Depending on sparseness, different iteration methods
      // could be more efficient.
      if (!tagged_slots.contains(static_cast<int>(i))) continue;
      Address slot_addr = segment_start + i * kTaggedSize;
      Address obj_addr = Decode(ro::EncodedTagged_t::FromAddress(slot_addr));
      Address obj_ptr = obj_addr + kHeapObjectTag;

      Tagged_t* dst = reinterpret_cast<Tagged_t*>(slot_addr);
      *dst = COMPRESS_POINTERS_BOOL
                 ? V8HeapCompressionScheme::CompressObject(obj_ptr)
                 : static_cast<Tagged_t>(obj_ptr);
    }
  }

  void DeserializeReadOnlyRootsTable() {
    ReadOnlyRoots roots(isolate_);
    if (V8_STATIC_ROOTS_BOOL) {
      roots.InitFromStaticRootsTable(isolate_->cage_base());
    } else {
      for (size_t i = 0; i < ReadOnlyRoots::kEntriesCount; i++) {
        uint32_t encoded_as_int = source_->GetInt();
        Address rudolf =
            Decode(ro::EncodedTagged_t::FromUint32(encoded_as_int));
        roots.read_only_roots_[i] = rudolf + kHeapObjectTag;
      }
    }
  }

  ReadOnlySpace* ro_space() const {
    return isolate_->read_only_heap()->read_only_space();
  }

  SnapshotByteSource* const source_;
  Isolate* const isolate_;
};

ReadOnlyDeserializer::ReadOnlyDeserializer(Isolate* isolate,
                                           const SnapshotData* data,
                                           bool can_rehash)
    : Deserializer(isolate, data->Payload(), data->GetMagicNumber(), false,
                   can_rehash) {}

void ReadOnlyDeserializer::DeserializeIntoIsolate() {
  NestedTimedHistogramScope histogram_timer(
      isolate()->counters()->snapshot_deserialize_rospace());
  HandleScope scope(isolate());
  ReadOnlyHeap* ro_heap = isolate()->read_only_heap();

  ReadOnlyHeapImageDeserializer::Deserialize(isolate(), source());
  ro_heap->read_only_space()->RepairFreeSpacesAfterDeserialization();
  PostProcessNewObjects();

  ReadOnlyRoots roots(isolate());
  roots.VerifyNameForProtectorsPages();
#ifdef DEBUG
  roots.VerifyNameForProtectors();
#endif

  if (should_rehash()) {
    isolate()->heap()->InitializeHashSeed();
    Rehash();
  }
}

void ReadOnlyDeserializer::PostProcessNewObjects() {
  // Since we are not deserializing individual objects we need to scan the
  // heap and search for objects that need post-processing.
  //
  // See also Deserializer<IsolateT>::PostProcessNewObject.
  PtrComprCageBase cage_base(isolate());
  ReadOnlyHeapObjectIterator it(isolate()->read_only_heap());
  for (HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
    const InstanceType instance_type = o.map(cage_base).instance_type();

    if (should_rehash()) {
      if (InstanceTypeChecker::IsString(instance_type)) {
        String str = String::cast(o);
        str.set_raw_hash_field(Name::kEmptyHashField);
        PushObjectToRehash(handle(str, isolate()));
      } else if (o.NeedsRehashing(instance_type)) {
        PushObjectToRehash(handle(o, isolate()));
      }
    }

    if (InstanceTypeChecker::IsCode(instance_type)) {
      Code code = Code::cast(o);
      code.init_instruction_start(main_thread_isolate(), kNullAddress);
      // RO space only contains builtin Code objects which don't have an
      // attached InstructionStream.
      DCHECK(code.is_builtin());
      DCHECK(!code.has_instruction_stream());
      code.SetInstructionStartForOffHeapBuiltin(
          main_thread_isolate(), EmbeddedData::FromBlob(main_thread_isolate())
                                     .InstructionStartOf(code.builtin_id()));
    }
  }
}

}  // namespace internal
}  // namespace v8
