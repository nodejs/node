// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_SAFEPOINT_TABLE_H_
#define V8_CODEGEN_SAFEPOINT_TABLE_H_

#include "src/base/bit-field.h"
#include "src/base/export-template.h"
#include "src/codegen/safepoint-table-base.h"
#include "src/common/assert-scope.h"
#include "src/utils/allocation.h"
#include "src/utils/bit-vector.h"
#include "src/utils/utils.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class GcSafeCode;

namespace wasm {
class WasmCode;
}  // namespace wasm

class SafepointEntry : public SafepointEntryBase {
 public:
  SafepointEntry() = default;
  ~SafepointEntry() { ReleaseData(); }

  // Only used by DCHECKs.
  bool operator==(const SafepointEntry& other) const {
    return this->SafepointEntryBase::operator==(other) &&
           tagged_register_indexes_ == other.tagged_register_indexes_ &&
           tagged_slots() == other.tagged_slots();
  }

  uint32_t tagged_register_indexes() const {
    DCHECK(is_initialized());
    return tagged_register_indexes_;
  }

  base::Vector<const uint8_t> tagged_slots() const {
    DCHECK(is_initialized());
    DCHECK_NOT_NULL(tagged_slots_.data());
    return tagged_slots_.as_vector();
  }

  // For clearing cache entries.
  void ReleaseData() { tagged_slots_.ReleaseData(); }

  void CopyFrom(const SafepointEntry& src) {
    SafepointEntryBase::operator=(src);
    tagged_register_indexes_ = src.tagged_register_indexes_;
    tagged_slots_.ReleaseData();
    tagged_slots_ = base::OwnedCopyOf(src.tagged_slots());
  }

 private:
  friend class SafepointTable;

  // Copies are expensive, so should use the explicit {CopyFrom()}.
  SafepointEntry(const SafepointEntry&) = delete;
  SafepointEntry& operator=(const SafepointEntry&) = delete;

  void ResetTaggedSlots(uint32_t num_tagged_slots) {
    size_t num_bytes = (num_tagged_slots + kBitsPerByte - 1) / kBitsPerByte;
    if (tagged_slots_.size() == 0) {
      tagged_slots_ = base::OwnedVector<uint8_t>::New(num_bytes);
    } else {
      std::fill(tagged_slots_.begin(), tagged_slots_.end(), 0);
    }
    tagged_register_indexes_ = 0;
  }

  uint32_t tagged_register_indexes_ = 0;
  base::OwnedVector<uint8_t> tagged_slots_;
};

// A wrapper class for accessing the safepoint table embedded into the
// InstructionStream object.
// This is a "one-shot" object: any of its UpperCamelCase methods consume it.
class SafepointTable {
 public:
  // The isolate and pc arguments are used for figuring out whether pc
  // belongs to the embedded or un-embedded code blob.
  SafepointTable(Isolate* isolate, Address pc, Tagged<InstructionStream> code);
  SafepointTable(Isolate* isolate, Address pc, Tagged<Code> code);
  SafepointTable(Isolate* isolate, Address pc, Tagged<GcSafeCode> code);
#if V8_ENABLE_WEBASSEMBLY
  explicit SafepointTable(const wasm::WasmCode* code);
#endif  // V8_ENABLE_WEBASSEMBLY

  SafepointTable(const SafepointTable&) = delete;
  SafepointTable& operator=(const SafepointTable&) = delete;

  int stack_slots() const { return stack_slots_; }

  int FindReturnPC(int pc_offset);

  // Returns the entry for the given pc. The underlying {SafepointEntry} is
  // ephemeral; don't let the reference outlive the {SafepointTable} instance!
  V8_EXPORT_PRIVATE SafepointEntry& FindEntry(Address pc);
  // Same, but doesn't populate the result's {tagged_slots()} to save time.
  SafepointEntry& FindEntry_NoStackSlots(Address pc);

  void Print(std::ostream&);

  static SafepointTable ForTest(Address instruction_start,
                                Address safepoint_table_address) {
    return SafepointTable(instruction_start, safepoint_table_address);
  }

 private:
  // Layout information.
#define FIELD_LIST(V)                                           \
  V(kStackSlotsOffset, sizeof(SafepointTableStackSlotsField_t)) \
  V(kConfigOffset, kUInt32Size)                                 \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(0, FIELD_LIST)
#undef FIELD_LIST

  static_assert(kStackSlotsOffset == kSafepointTableStackSlotsOffset);

  // The {Assembler} class is limited to {int}-sized Code objects anyway,
  // we can store up to half of that for safepoint table size.
  using ByteLengthField = base::BitField<uint32_t, 0, 30>;
  using HasTaggedRegistersField = ByteLengthField::Next<bool, 1>;
  using HasDeoptDataField = HasTaggedRegistersField::Next<bool, 1>;

  V8_EXPORT_PRIVATE SafepointTable(Address instruction_start,
                                   Address safepoint_table_address);

  // Iteration methods.
  void ResetIteration();
  bool has_more() const { return ptr_ < end_; }

  template <bool update_tagged_slots>
  void Advance();
  template <bool update_tagged_slots>
  void FindEntryImpl(Address pc);

  DISALLOW_GARBAGE_COLLECTION(no_gc_)

  const Address instruction_start_;

  // Safepoint table layout.
  const uint8_t* safepoint_table_address_;
  const SafepointTableStackSlotsField_t stack_slots_;
  const uint32_t byte_length_;  // Includes header.
  const bool has_tagged_registers_;
  const bool has_deopt_data_;

  // Iteration support.
  const uint8_t* ptr_;
  const uint8_t* end_;
  SafepointEntry entry_;
  // Potentially different from the corresponding fields in {entry_}, because
  // they're optional. In these standalone fields we store the last existing
  // value.
  int last_deopt_index_;
  int last_trampoline_;
  // Iteration terminates when the next pc would be too large, so we pre-fetch
  // that.
  int next_pc_;

  friend class SafepointTableBuilder;
  friend class SafepointEntry;
};

// The SafepointTableBuilder is optimized for low memory consumption (both
// in the Zone during compilation and in the final Code object), so it
// streams entries into a compressed temporary format, and then compresses them
// even further when emitting the final encoded representation.
// The design builds on the assumption that stack slots close to the SP tend
// to change, whereas stack slots close to the FP tend to maintain their
// taggedness. This is particularly true for Liftoff, which due to its
// non-optimizing approach also has a particular tendency to create large
// stack frames.
//
// Callers start by calling {DefineSafepoint()}, and in the returned
// {Safepoint} instance they build up a full bit vector, where set bits
// indicate tagged stack slots. We store this vector in two rotating
// {EntryBuilder} instances.
// Example:
//     index=0: 0001001 ← highest index
//              ↑     ↑
//              fp    sp
// The {~Safepoint} destructor compares the bit vector to the previous
// safepoint's vector (in the other {EntryBuilder} instance), determines
// their identical prefix, makes a copy of the differing tail, and commits
// that to the {SafepointTableBuilder}'s list of {entries_} as a
// {CompressedEntry}.
// Example: Assume the following complete vectors, where we start with an
//          implicit all-zero vector:
//     implicit start: 0000000
//     first/previous: 0001001
//     second/current: 0001010
// We would then store the following two entries:
//     {common prefix: 3 bits, stored tail: 1001}
//     {common prefix: 5 bits, stored tail: 10}
// We don't need to care whether the last trailing zero is stored or not; the
// current implementation would store it because {GrowableBitVector} never
// shrinks.
//
// Callers may then call {UpdateDeoptimizationInfo()} as needed to complete
// the information stored in each {CompressedEntry}.
//
// Finally, the actual safepoint table is created by calling {Emit()}.
// This needs to reverse the order of bits, so index=0 is the "sp" start
// of the vector, and the highest index is the "fp" end of it. For decoding
// convenience, we also round up the size of the stored prefix to the nearest
// byte.
//
// The layout of a full safepoint table is as follows, where "uleb" denotes
// an LEB-encoded uint32. We store most things as deltas to the previous value
// to keep the encoded LEBs small.
//  - uint32 stack_slot_count: described stack size of each entry
//  - uint32 bit field: (byte_length: size in bytes, has_tagged_registers: bool,
//                       has_deopt_data: bool)
//  - sequence of entries:
//     - uleb pc_delta: distance from last entry's pc
//     - if the table has deopt data:
//       - if this entry has a deopt index:
//         - uleb deopt_index_delta: distance from last deopt index, >0.
//         - uleb trampoline_delta: distance from last trampoline pc
//       - else:
//         - zero
//     - if the table has tagged registers:
//       - uleb register_indexes
//     - encoded stack map delta (see below)
// The entry sequence's deltas assume an implicit all-zero entry as a starting
// point; similarly the sequence of stack maps assumes an all-zero vector to
// which the first entry applies its changed prefix.
//
// The stack map delta can be encoded in one of two forms, which can be
// distinguished by their first byte:
// xyyyyyy0 -> "full encoding": the first byte is an LEB-encoded Smi storing
//             the number of bytes that follow as a byte vector. (Corollary:
//             if the first byte is all-zero, that means "no changes".)
//             In the notation here, "x" is the LEB bit indicating whether
//             another byte is coming, and "y" are payload bits.
// xyyyyy11 -> optional first part of "compact encoding": an LEB-encoded
//             number of bits to skip before the second part is applied.
// xyyyyy01 -> non-optional second part of "compact encoding": an LEB-encoded
//             arbitrary-length patch to apply.
//             Due to the two tag bits, both parts of the compact encoding
//             have 5 payload bits in the first byte, and 7 payload bits in
//             every following byte.
// Both the full and the compact encoding store a "patch" as an xor mask:
// it indicates the bits that have changed, so applying it means xor'ing it
// onto the previous stack map. The primary motivation for this is that it
// supports compact storage and fast application of any partially-used last
// byte: if it only sets two bits, then it only flips two bits, and we don't
// need to worry about explicitly preserving any other bits.
// The "compact encoding" is expected to support the vast majority of cases
// that occur in real-world code with fewer bytes than the "full encoding".
// It is the encoder's job to choose wisely.
//
// Since each table entry is encoded as a delta from the previous entry, we
// offer no indexed/random access to entries, only forward iteration.

class V8_EXPORT_PRIVATE SafepointTableBuilder
    : public SafepointTableBuilderBase {
 private:
  struct EntryBuilder {
    int pc;
    uint32_t register_indexes;
    GrowableBitVector stack_indexes;

    EntryBuilder() = default;

    void Reset(int new_pc) {
      pc = new_pc;
      stack_indexes.Clear();
      register_indexes = 0;
    }
  };
  struct CompressedEntry {
    int pc;
    int deopt_index = SafepointEntry::kNoDeoptIndex;
    int trampoline = SafepointEntry::kNoTrampolinePC;
    uint32_t register_indexes;
    uint32_t common_bits;
    BitVector* stack_indexes_tail;

    CompressedEntry(int pc, uint32_t register_indexes, uint32_t common_bits,
                    BitVector* stack_indexes_tail)
        : pc(pc),
          register_indexes(register_indexes),
          common_bits(common_bits),
          stack_indexes_tail(stack_indexes_tail) {}
  };

 public:
  explicit SafepointTableBuilder(Zone* zone) : entries_(zone), zone_(zone) {}

  SafepointTableBuilder(const SafepointTableBuilder&) = delete;
  SafepointTableBuilder& operator=(const SafepointTableBuilder&) = delete;

  class Safepoint {
   public:
    void DefineTaggedStackSlot(int index) {
      // Note it is only valid to specify stack slots here that are *not* in
      // the fixed part of the frame (e.g. argc, target, context, stored rbp,
      // return address). Frame iteration handles the fixed part of the frame
      // with custom code, see Turbofan::Iterate.
      entry_->stack_indexes.Add(index, table_->zone_);
    }
    void DefineTaggedStackSlots(const GrowableBitVector& indexes) {
      entry_->stack_indexes.Add(indexes, table_->zone_);
    }

    void DefineTaggedRegister(int reg_code) {
      DCHECK_LT(reg_code,
                kBitsPerByte * sizeof(EntryBuilder::register_indexes));
      entry_->register_indexes |= 1u << reg_code;
      table_->has_tagged_registers_ = true;
    }

    ~Safepoint() { table_->Commit(this); }

   private:
    friend class SafepointTableBuilder;
    Safepoint(EntryBuilder* entry, SafepointTableBuilder* table)
        : entry_(entry), table_(table) {}
    EntryBuilder* const entry_;
    SafepointTableBuilder* const table_;
  };

  // Define a new safepoint for the current position in the body. The
  // `pc_offset` parameter allows to define a different offset than the current
  // pc_offset.
  Safepoint DefineSafepoint(Assembler* assembler, int pc_offset = 0);

  // Emit the safepoint table after the body.
  void Emit(Assembler* assembler, int stack_slot_count);

  // Find the Deoptimization Info with pc offset {pc} and update its
  // trampoline field. Calling this function ensures that the safepoint
  // table contains the trampoline PC {trampoline} that replaced the
  // return PC {pc} on the stack.
  int UpdateDeoptimizationInfo(int pc, int trampoline, int start,
                               int deopt_index);

 private:
  void Commit(Safepoint* safepoint);

  EntryBuilder scratch_entry1_;
  EntryBuilder scratch_entry2_;
  EntryBuilder* current_entry_{&scratch_entry1_};
  EntryBuilder* previous_entry_{&scratch_entry2_};
  ZoneDeque<CompressedEntry> entries_;
  bool has_deopt_data_{false};
  bool has_tagged_registers_{false};
  Zone* zone_;
};

// These are free functions to allow unit testing them. They are only meant
// to be used by the {SafepointTableBuilder} and tests.
V8_EXPORT_PRIVATE BitVector* CompareAndCreateXorPatch(
    Zone* zone, const GrowableBitVector& changed, const GrowableBitVector& base,
    uint32_t* common_prefix_bits);

V8_EXPORT_PRIVATE void EncodeSafepointEntry(int stack_slot_count,
                                            uint32_t common_prefix,
                                            const BitVector* patch,
                                            Assembler* assembler);

template <bool update_tagged_slots>
EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
void DecodeSafepointEntry(const uint8_t** ptr,
                          base::OwnedVector<uint8_t>& tagged_slots);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_SAFEPOINT_TABLE_H_
