// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_DATA_H_
#define V8_SNAPSHOT_EMBEDDED_DATA_H_

#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/globals.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

class Code;
class Isolate;

// Wraps an off-heap instruction stream.
// TODO(jgruber,v8:6666): Remove this class.
class InstructionStream final : public AllStatic {
 public:
  // Returns true, iff the given pc points into an off-heap instruction stream.
  static bool PcIsOffHeap(Isolate* isolate, Address pc);

  // Returns the corresponding Code object if it exists, and nullptr otherwise.
  static Code TryLookupCode(Isolate* isolate, Address address);

  // During snapshot creation, we first create an executable off-heap area
  // containing all off-heap code. The area is guaranteed to be contiguous.
  // Note that this only applies when building the snapshot, e.g. for
  // mksnapshot. Otherwise, off-heap code is embedded directly into the binary.
  static void CreateOffHeapInstructionStream(Isolate* isolate, uint8_t** data,
                                             uint32_t* size);
  static void FreeOffHeapInstructionStream(uint8_t* data, uint32_t size);
};

class EmbeddedData final {
 public:
  static EmbeddedData FromIsolate(Isolate* isolate);

  static EmbeddedData FromBlob() {
    return EmbeddedData(Isolate::CurrentEmbeddedBlob(),
                        Isolate::CurrentEmbeddedBlobSize());
  }

  static EmbeddedData FromBlob(Isolate* isolate) {
    return EmbeddedData(isolate->embedded_blob(),
                        isolate->embedded_blob_size());
  }

  const uint8_t* data() const { return data_; }
  uint32_t size() const { return size_; }

  void Dispose() { delete[] data_; }

  Address InstructionStartOfBuiltin(int i) const;
  uint32_t InstructionSizeOfBuiltin(int i) const;

  bool ContainsBuiltin(int i) const { return InstructionSizeOfBuiltin(i) > 0; }

  uint32_t AddressForHashing(Address addr) {
    Address start = reinterpret_cast<Address>(data_);
    DCHECK(IsInRange(addr, start, start + size_));
    return static_cast<uint32_t>(addr - start);
  }

  // Padded with kCodeAlignment.
  uint32_t PaddedInstructionSizeOfBuiltin(int i) const {
    uint32_t size = InstructionSizeOfBuiltin(i);
    return (size == 0) ? 0 : PadAndAlign(size);
  }

  size_t CreateHash() const;
  size_t Hash() const {
    return *reinterpret_cast<const size_t*>(data_ + HashOffset());
  }

  struct Metadata {
    // Blob layout information.
    uint32_t instructions_offset;
    uint32_t instructions_length;
  };
  STATIC_ASSERT(offsetof(Metadata, instructions_offset) == 0);
  STATIC_ASSERT(offsetof(Metadata, instructions_length) == kUInt32Size);
  STATIC_ASSERT(sizeof(Metadata) == kUInt32Size + kUInt32Size);

  // The layout of the blob is as follows:
  //
  // [0] hash of the remaining blob
  // [1] metadata of instruction stream 0
  // ... metadata
  // ... instruction streams

  static constexpr uint32_t kTableSize = Builtins::builtin_count;
  static constexpr uint32_t HashOffset() { return 0; }
  static constexpr uint32_t HashSize() { return kSizetSize; }
  static constexpr uint32_t MetadataOffset() {
    return HashOffset() + HashSize();
  }
  static constexpr uint32_t MetadataSize() {
    return sizeof(struct Metadata) * kTableSize;
  }
  static constexpr uint32_t RawDataOffset() {
    return PadAndAlign(MetadataOffset() + MetadataSize());
  }

 private:
  EmbeddedData(const uint8_t* data, uint32_t size) : data_(data), size_(size) {
    DCHECK_NOT_NULL(data);
    DCHECK_LT(0, size);
  }

  const Metadata* Metadata() const {
    return reinterpret_cast<const struct Metadata*>(data_ + MetadataOffset());
  }
  const uint8_t* RawData() const { return data_ + RawDataOffset(); }

  static constexpr int PadAndAlign(int size) {
    // Ensure we have at least one byte trailing the actual builtin
    // instructions which we can later fill with int3.
    return RoundUp<kCodeAlignment>(size + 1);
  }

  void PrintStatistics() const;

  const uint8_t* data_;
  uint32_t size_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_DATA_H_
