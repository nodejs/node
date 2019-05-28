// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SAFEPOINT_TABLE_H_
#define V8_SAFEPOINT_TABLE_H_

#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/utils.h"
#include "src/v8memory.h"
#include "src/zone/zone-chunk-list.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class Register;

class SafepointEntry {
 public:
  SafepointEntry() : info_(0), bits_(nullptr), trampoline_pc_(-1) {}

  SafepointEntry(unsigned info, uint8_t* bits, int trampoline_pc)
      : info_(info), bits_(bits), trampoline_pc_(trampoline_pc) {
    DCHECK(is_valid());
  }

  bool is_valid() const { return bits_ != nullptr; }

  bool Equals(const SafepointEntry& other) const {
    return info_ == other.info_ && bits_ == other.bits_;
  }

  void Reset() {
    info_ = 0;
    bits_ = nullptr;
  }

  int trampoline_pc() { return trampoline_pc_; }

  static const int kSaveDoublesFieldBits = 1;
  static const int kDeoptIndexBits = 32 - kSaveDoublesFieldBits;

  class DeoptimizationIndexField : public BitField<int, 0, kDeoptIndexBits> {};
  class SaveDoublesField
      : public BitField<bool, DeoptimizationIndexField::kNext,
                        kSaveDoublesFieldBits> {};

  int deoptimization_index() const {
    DCHECK(is_valid() && has_deoptimization_index());
    return DeoptimizationIndexField::decode(info_);
  }

  bool has_deoptimization_index() const {
    DCHECK(is_valid());
    return DeoptimizationIndexField::decode(info_) !=
           DeoptimizationIndexField::kMax;
  }

  bool has_doubles() const {
    DCHECK(is_valid());
    return SaveDoublesField::decode(info_);
  }

  uint8_t* bits() {
    DCHECK(is_valid());
    return bits_;
  }

  bool HasRegisters() const;
  bool HasRegisterAt(int reg_index) const;

 private:
  unsigned info_;
  uint8_t* bits_;
  // It needs to be an integer as it is -1 for eager deoptimizations.
  int trampoline_pc_;
};

class SafepointTable {
 public:
  explicit SafepointTable(Code code);
  explicit SafepointTable(Address instruction_start,
                          size_t safepoint_table_offset, uint32_t stack_slots,
                          bool has_deopt = false);

  int size() const {
    return kHeaderSize + (length_ * (kFixedEntrySize + entry_size_));
  }
  unsigned length() const { return length_; }
  unsigned entry_size() const { return entry_size_; }

  unsigned GetPcOffset(unsigned index) const {
    DCHECK(index < length_);
    return Memory<uint32_t>(GetPcOffsetLocation(index));
  }

  int GetTrampolinePcOffset(unsigned index) const {
    DCHECK(index < length_);
    return Memory<int>(GetTrampolineLocation(index));
  }

  unsigned find_return_pc(unsigned pc_offset);

  SafepointEntry GetEntry(unsigned index) const {
    DCHECK(index < length_);
    unsigned info = Memory<uint32_t>(GetEncodedInfoLocation(index));
    uint8_t* bits = &Memory<uint8_t>(entries_ + (index * entry_size_));
    int trampoline_pc =
        has_deopt_ ? Memory<int>(GetTrampolineLocation(index)) : -1;
    return SafepointEntry(info, bits, trampoline_pc);
  }

  // Returns the entry for the given pc.
  SafepointEntry FindEntry(Address pc) const;

  void PrintEntry(unsigned index, std::ostream& os) const;  // NOLINT

 private:
  static const uint8_t kNoRegisters = 0xFF;

  // Layout information
  static const int kLengthOffset = 0;
  static const int kEntrySizeOffset = kLengthOffset + kIntSize;
  static const int kHeaderSize = kEntrySizeOffset + kIntSize;
  static const int kPcOffset = 0;
  static const int kEncodedInfoOffset = kPcOffset + kIntSize;
  static const int kTrampolinePcOffset = kEncodedInfoOffset + kIntSize;
  static const int kFixedEntrySize = kTrampolinePcOffset + kIntSize;

  Address GetPcOffsetLocation(unsigned index) const {
    return pc_and_deoptimization_indexes_ + (index * kFixedEntrySize);
  }

  Address GetEncodedInfoLocation(unsigned index) const {
    return GetPcOffsetLocation(index) + kEncodedInfoOffset;
  }

  Address GetTrampolineLocation(unsigned index) const {
    return GetPcOffsetLocation(index) + kTrampolinePcOffset;
  }

  static void PrintBits(std::ostream& os,  // NOLINT
                        uint8_t byte, int digits);

  DISALLOW_HEAP_ALLOCATION(no_allocation_)
  Address instruction_start_;
  uint32_t stack_slots_;
  unsigned length_;
  unsigned entry_size_;

  Address pc_and_deoptimization_indexes_;
  Address entries_;
  bool has_deopt_;

  friend class SafepointTableBuilder;
  friend class SafepointEntry;

  DISALLOW_COPY_AND_ASSIGN(SafepointTable);
};

class Safepoint {
 public:
  typedef enum {
    kSimple = 0,
    kWithRegisters = 1 << 0,
    kWithDoubles = 1 << 1,
    kWithRegistersAndDoubles = kWithRegisters | kWithDoubles
  } Kind;

  enum DeoptMode {
    kNoLazyDeopt,
    kLazyDeopt
  };

  static const int kNoDeoptimizationIndex =
      SafepointEntry::DeoptimizationIndexField::kMax;

  void DefinePointerSlot(int index) { indexes_->push_back(index); }
  void DefinePointerRegister(Register reg);

 private:
  Safepoint(ZoneChunkList<int>* indexes, ZoneChunkList<int>* registers)
      : indexes_(indexes), registers_(registers) {}
  ZoneChunkList<int>* const indexes_;
  ZoneChunkList<int>* const registers_;

  friend class SafepointTableBuilder;
};

class SafepointTableBuilder {
 public:
  explicit SafepointTableBuilder(Zone* zone)
      : deoptimization_info_(zone),
        emitted_(false),
        last_lazy_safepoint_(0),
        zone_(zone) {}

  // Get the offset of the emitted safepoint table in the code.
  unsigned GetCodeOffset() const;

  // Define a new safepoint for the current position in the body.
  Safepoint DefineSafepoint(Assembler* assembler,
                            Safepoint::Kind kind,
                            Safepoint::DeoptMode mode);

  // Record deoptimization index for lazy deoptimization for the last
  // outstanding safepoints.
  void RecordLazyDeoptimizationIndex(int index);
  void BumpLastLazySafepointIndex() {
    last_lazy_safepoint_ = deoptimization_info_.size();
  }

  // Emit the safepoint table after the body. The number of bits per
  // entry must be enough to hold all the pointer indexes.
  V8_EXPORT_PRIVATE void Emit(Assembler* assembler, int bits_per_entry);

  // Find the Deoptimization Info with pc offset {pc} and update its
  // trampoline field. Calling this function ensures that the safepoint
  // table contains the trampoline PC {trampoline} that replaced the
  // return PC {pc} on the stack.
  int UpdateDeoptimizationInfo(int pc, int trampoline, int start);

 private:
  struct DeoptimizationInfo {
    unsigned pc;
    unsigned deopt_index;
    bool has_doubles;
    int trampoline;
    ZoneChunkList<int>* indexes;
    ZoneChunkList<int>* registers;
    DeoptimizationInfo(Zone* zone, unsigned pc, Safepoint::Kind kind)
        : pc(pc),
          deopt_index(Safepoint::kNoDeoptimizationIndex),
          has_doubles(kind & Safepoint::kWithDoubles),
          trampoline(-1),
          indexes(new (zone) ZoneChunkList<int>(
              zone, ZoneChunkList<int>::StartMode::kSmall)),
          registers(kind & Safepoint::kWithRegisters
                        ? new (zone) ZoneChunkList<int>(
                              zone, ZoneChunkList<int>::StartMode::kSmall)
                        : nullptr) {}
  };

  // Encodes all fields of a {DeoptimizationInfo} except {pc} and {trampoline}.
  uint32_t EncodeExceptPC(const DeoptimizationInfo&);

  // Compares all fields of a {DeoptimizationInfo} except {pc} and {trampoline}.
  bool IsIdenticalExceptForPc(const DeoptimizationInfo&,
                              const DeoptimizationInfo&) const;

  // If all entries are identical, replace them by 1 entry with pc = kMaxUInt32.
  void RemoveDuplicates();

  ZoneChunkList<DeoptimizationInfo> deoptimization_info_;

  unsigned offset_;
  bool emitted_;
  size_t last_lazy_safepoint_;

  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(SafepointTableBuilder);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SAFEPOINT_TABLE_H_
