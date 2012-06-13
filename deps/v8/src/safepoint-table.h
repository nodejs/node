// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_SAFEPOINT_TABLE_H_
#define V8_SAFEPOINT_TABLE_H_

#include "allocation.h"
#include "heap.h"
#include "v8memory.h"
#include "zone.h"

namespace v8 {
namespace internal {

struct Register;

class SafepointEntry BASE_EMBEDDED {
 public:
  SafepointEntry() : info_(0), bits_(NULL) {}

  SafepointEntry(unsigned info, uint8_t* bits) : info_(info), bits_(bits) {
    ASSERT(is_valid());
  }

  bool is_valid() const { return bits_ != NULL; }

  bool Equals(const SafepointEntry& other) const {
    return info_ == other.info_ && bits_ == other.bits_;
  }

  void Reset() {
    info_ = 0;
    bits_ = NULL;
  }

  int deoptimization_index() const {
    ASSERT(is_valid());
    return DeoptimizationIndexField::decode(info_);
  }

  static const int kArgumentsFieldBits = 3;
  static const int kSaveDoublesFieldBits = 1;
  static const int kDeoptIndexBits =
      32 - kArgumentsFieldBits - kSaveDoublesFieldBits;
  class DeoptimizationIndexField:
    public BitField<int, 0, kDeoptIndexBits> {};  // NOLINT
  class ArgumentsField:
    public BitField<unsigned,
                    kDeoptIndexBits,
                    kArgumentsFieldBits> {};  // NOLINT
  class SaveDoublesField:
    public BitField<bool,
                    kDeoptIndexBits + kArgumentsFieldBits,
                    kSaveDoublesFieldBits> { }; // NOLINT

  int argument_count() const {
    ASSERT(is_valid());
    return ArgumentsField::decode(info_);
  }

  bool has_doubles() const {
    ASSERT(is_valid());
    return SaveDoublesField::decode(info_);
  }

  uint8_t* bits() {
    ASSERT(is_valid());
    return bits_;
  }

  bool HasRegisters() const;
  bool HasRegisterAt(int reg_index) const;

 private:
  unsigned info_;
  uint8_t* bits_;
};


class SafepointTable BASE_EMBEDDED {
 public:
  explicit SafepointTable(Code* code);

  int size() const {
    return kHeaderSize +
           (length_ * (kPcAndDeoptimizationIndexSize + entry_size_)); }
  unsigned length() const { return length_; }
  unsigned entry_size() const { return entry_size_; }

  unsigned GetPcOffset(unsigned index) const {
    ASSERT(index < length_);
    return Memory::uint32_at(GetPcOffsetLocation(index));
  }

  SafepointEntry GetEntry(unsigned index) const {
    ASSERT(index < length_);
    unsigned info = Memory::uint32_at(GetInfoLocation(index));
    uint8_t* bits = &Memory::uint8_at(entries_ + (index * entry_size_));
    return SafepointEntry(info, bits);
  }

  // Returns the entry for the given pc.
  SafepointEntry FindEntry(Address pc) const;

  void PrintEntry(unsigned index) const;

 private:
  static const uint8_t kNoRegisters = 0xFF;

  static const int kLengthOffset = 0;
  static const int kEntrySizeOffset = kLengthOffset + kIntSize;
  static const int kHeaderSize = kEntrySizeOffset + kIntSize;

  static const int kPcSize = kIntSize;
  static const int kDeoptimizationIndexSize = kIntSize;
  static const int kPcAndDeoptimizationIndexSize =
      kPcSize + kDeoptimizationIndexSize;

  Address GetPcOffsetLocation(unsigned index) const {
    return pc_and_deoptimization_indexes_ +
           (index * kPcAndDeoptimizationIndexSize);
  }

  Address GetInfoLocation(unsigned index) const {
    return GetPcOffsetLocation(index) + kPcSize;
  }

  static void PrintBits(uint8_t byte, int digits);

  AssertNoAllocation no_allocation_;
  Code* code_;
  unsigned length_;
  unsigned entry_size_;

  Address pc_and_deoptimization_indexes_;
  Address entries_;

  friend class SafepointTableBuilder;
  friend class SafepointEntry;

  DISALLOW_COPY_AND_ASSIGN(SafepointTable);
};


class Safepoint BASE_EMBEDDED {
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
      (1 << (SafepointEntry::kDeoptIndexBits)) - 1;

  void DefinePointerSlot(int index, Zone* zone) { indexes_->Add(index, zone); }
  void DefinePointerRegister(Register reg, Zone* zone);

 private:
  Safepoint(ZoneList<int>* indexes, ZoneList<int>* registers) :
      indexes_(indexes), registers_(registers) { }
  ZoneList<int>* indexes_;
  ZoneList<int>* registers_;

  friend class SafepointTableBuilder;
};


class SafepointTableBuilder BASE_EMBEDDED {
 public:
  explicit SafepointTableBuilder(Zone* zone)
      : deoptimization_info_(32, zone),
        deopt_index_list_(32, zone),
        indexes_(32, zone),
        registers_(32, zone),
        emitted_(false),
        last_lazy_safepoint_(0),
        zone_(zone) { }

  // Get the offset of the emitted safepoint table in the code.
  unsigned GetCodeOffset() const;

  // Define a new safepoint for the current position in the body.
  Safepoint DefineSafepoint(Assembler* assembler,
                            Safepoint::Kind kind,
                            int arguments,
                            Safepoint::DeoptMode mode);

  // Record deoptimization index for lazy deoptimization for the last
  // outstanding safepoints.
  void RecordLazyDeoptimizationIndex(int index);

  // Emit the safepoint table after the body. The number of bits per
  // entry must be enough to hold all the pointer indexes.
  void Emit(Assembler* assembler, int bits_per_entry);


 private:
  struct DeoptimizationInfo {
    unsigned pc;
    unsigned arguments;
    bool has_doubles;
  };

  uint32_t EncodeExceptPC(const DeoptimizationInfo& info, unsigned index);

  ZoneList<DeoptimizationInfo> deoptimization_info_;
  ZoneList<unsigned> deopt_index_list_;
  ZoneList<ZoneList<int>*> indexes_;
  ZoneList<ZoneList<int>*> registers_;

  unsigned offset_;
  bool emitted_;
  int last_lazy_safepoint_;

  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(SafepointTableBuilder);
};

} }  // namespace v8::internal

#endif  // V8_SAFEPOINT_TABLE_H_
