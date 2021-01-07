// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_SAFEPOINT_TABLE_H_
#define V8_CODEGEN_SAFEPOINT_TABLE_H_

#include "src/base/memory.h"
#include "src/common/assert-scope.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"
#include "src/zone/zone-chunk-list.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

namespace wasm {
class WasmCode;
}  // namespace wasm

class SafepointEntry {
 public:
  SafepointEntry() : deopt_index_(0), bits_(nullptr), trampoline_pc_(-1) {}

  SafepointEntry(unsigned deopt_index, uint8_t* bits, int trampoline_pc)
      : deopt_index_(deopt_index), bits_(bits), trampoline_pc_(trampoline_pc) {
    DCHECK(is_valid());
  }

  bool is_valid() const { return bits_ != nullptr; }

  bool Equals(const SafepointEntry& other) const {
    return deopt_index_ == other.deopt_index_ && bits_ == other.bits_;
  }

  void Reset() {
    deopt_index_ = 0;
    bits_ = nullptr;
  }

  int trampoline_pc() { return trampoline_pc_; }

  static const unsigned kNoDeoptIndex = kMaxUInt32;

  int deoptimization_index() const {
    DCHECK(is_valid() && has_deoptimization_index());
    return deopt_index_;
  }

  bool has_deoptimization_index() const {
    DCHECK(is_valid());
    return deopt_index_ != kNoDeoptIndex;
  }

  uint8_t* bits() {
    DCHECK(is_valid());
    return bits_;
  }

 private:
  unsigned deopt_index_;
  uint8_t* bits_;
  // It needs to be an integer as it is -1 for eager deoptimizations.
  int trampoline_pc_;
};

class SafepointTable {
 public:
  explicit SafepointTable(Code code);
  explicit SafepointTable(const wasm::WasmCode* code);

  SafepointTable(const SafepointTable&) = delete;
  SafepointTable& operator=(const SafepointTable&) = delete;

  int size() const {
    return kHeaderSize + (length_ * (kFixedEntrySize + entry_size_));
  }
  unsigned length() const { return length_; }
  unsigned entry_size() const { return entry_size_; }

  unsigned GetPcOffset(unsigned index) const {
    DCHECK(index < length_);
    return base::Memory<uint32_t>(GetPcOffsetLocation(index));
  }

  int GetTrampolinePcOffset(unsigned index) const {
    DCHECK(index < length_);
    return base::Memory<int>(GetTrampolineLocation(index));
  }

  unsigned find_return_pc(unsigned pc_offset);

  SafepointEntry GetEntry(unsigned index) const {
    DCHECK(index < length_);
    unsigned deopt_index =
        base::Memory<uint32_t>(GetEncodedInfoLocation(index));
    uint8_t* bits = &base::Memory<uint8_t>(entries() + (index * entry_size_));
    int trampoline_pc =
        has_deopt_ ? base::Memory<int>(GetTrampolineLocation(index)) : -1;
    return SafepointEntry(deopt_index, bits, trampoline_pc);
  }

  // Returns the entry for the given pc.
  SafepointEntry FindEntry(Address pc) const;

  void PrintEntry(unsigned index, std::ostream& os) const;  // NOLINT

 private:
  SafepointTable(Address instruction_start, Address safepoint_table_address,
                 uint32_t stack_slots, bool has_deopt);

  static const uint8_t kNoRegisters = 0xFF;

  // Layout information.
  static const int kLengthOffset = 0;
  static const int kEntrySizeOffset = kLengthOffset + kIntSize;
  static const int kHeaderSize = kEntrySizeOffset + kIntSize;
  static const int kPcOffset = 0;
  static const int kEncodedInfoOffset = kPcOffset + kIntSize;
  static const int kTrampolinePcOffset = kEncodedInfoOffset + kIntSize;
  static const int kFixedEntrySize = kTrampolinePcOffset + kIntSize;

  static uint32_t ReadLength(Address table) {
    return base::Memory<uint32_t>(table + kLengthOffset);
  }
  static uint32_t ReadEntrySize(Address table) {
    return base::Memory<uint32_t>(table + kEntrySizeOffset);
  }
  Address pc_and_deoptimization_indexes() const {
    return safepoint_table_address_ + kHeaderSize;
  }
  Address entries() const {
    return safepoint_table_address_ + kHeaderSize + (length_ * kFixedEntrySize);
  }

  Address GetPcOffsetLocation(unsigned index) const {
    return pc_and_deoptimization_indexes() + (index * kFixedEntrySize);
  }

  Address GetEncodedInfoLocation(unsigned index) const {
    return GetPcOffsetLocation(index) + kEncodedInfoOffset;
  }

  Address GetTrampolineLocation(unsigned index) const {
    return GetPcOffsetLocation(index) + kTrampolinePcOffset;
  }

  static void PrintBits(std::ostream& os, uint8_t byte, int digits);

  DISALLOW_GARBAGE_COLLECTION(no_gc_)

  const Address instruction_start_;
  const uint32_t stack_slots_;
  const bool has_deopt_;

  // Safepoint table layout.
  const Address safepoint_table_address_;
  const uint32_t length_;
  const uint32_t entry_size_;

  friend class SafepointTableBuilder;
  friend class SafepointEntry;
};

class Safepoint {
 public:
  enum DeoptMode { kNoLazyDeopt, kLazyDeopt };

  static const int kNoDeoptimizationIndex = SafepointEntry::kNoDeoptIndex;

  void DefinePointerSlot(int index) { indexes_->push_back(index); }

 private:
  explicit Safepoint(ZoneChunkList<int>* indexes) : indexes_(indexes) {}
  ZoneChunkList<int>* const indexes_;

  friend class SafepointTableBuilder;
};

class SafepointTableBuilder {
 public:
  explicit SafepointTableBuilder(Zone* zone)
      : deoptimization_info_(zone),
        emitted_(false),
        zone_(zone) {}

  SafepointTableBuilder(const SafepointTableBuilder&) = delete;
  SafepointTableBuilder& operator=(const SafepointTableBuilder&) = delete;

  // Get the offset of the emitted safepoint table in the code.
  unsigned GetCodeOffset() const;

  // Define a new safepoint for the current position in the body.
  Safepoint DefineSafepoint(Assembler* assembler, Safepoint::DeoptMode mode);

  // Emit the safepoint table after the body. The number of bits per
  // entry must be enough to hold all the pointer indexes.
  V8_EXPORT_PRIVATE void Emit(Assembler* assembler, int bits_per_entry);

  // Find the Deoptimization Info with pc offset {pc} and update its
  // trampoline field. Calling this function ensures that the safepoint
  // table contains the trampoline PC {trampoline} that replaced the
  // return PC {pc} on the stack.
  int UpdateDeoptimizationInfo(int pc, int trampoline, int start,
                               unsigned deopt_index);

 private:
  struct DeoptimizationInfo {
    unsigned pc;
    unsigned deopt_index;
    int trampoline;
    ZoneChunkList<int>* indexes;
    DeoptimizationInfo(Zone* zone, unsigned pc)
        : pc(pc),
          deopt_index(Safepoint::kNoDeoptimizationIndex),
          trampoline(-1),
          indexes(zone->New<ZoneChunkList<int>>(
              zone, ZoneChunkList<int>::StartMode::kSmall)) {}
  };

  // Compares all fields of a {DeoptimizationInfo} except {pc} and {trampoline}.
  bool IsIdenticalExceptForPc(const DeoptimizationInfo&,
                              const DeoptimizationInfo&) const;

  // If all entries are identical, replace them by 1 entry with pc = kMaxUInt32.
  void RemoveDuplicates();

  ZoneChunkList<DeoptimizationInfo> deoptimization_info_;

  unsigned offset_;
  bool emitted_;

  Zone* zone_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_SAFEPOINT_TABLE_H_
