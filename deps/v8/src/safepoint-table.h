// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "macro-assembler.h"
#include "zone.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

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

  int GetDeoptimizationIndex(unsigned index) const {
    ASSERT(index < length_);
    unsigned value = Memory::uint32_at(GetDeoptimizationLocation(index));
    return DeoptimizationIndexField::decode(value);
  }

  unsigned GetGapCodeSize(unsigned index) const {
    ASSERT(index < length_);
    unsigned value = Memory::uint32_at(GetDeoptimizationLocation(index));
    return GapCodeSizeField::decode(value);
  }

  uint8_t* GetEntry(unsigned index) const {
    ASSERT(index < length_);
    return &Memory::uint8_at(entries_ + (index * entry_size_));
  }

  class GapCodeSizeField: public BitField<unsigned, 0, 8> {};
  class DeoptimizationIndexField: public BitField<int, 8, 24> {};

  static bool HasRegisters(uint8_t* entry);
  static bool HasRegisterAt(uint8_t* entry, int reg_index);

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

  Address GetDeoptimizationLocation(unsigned index) const {
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
};


class Safepoint BASE_EMBEDDED {
 public:
  static const int kNoDeoptimizationIndex = 0x00ffffff;

  void DefinePointerSlot(int index) { indexes_->Add(index); }
  void DefinePointerRegister(Register reg) { registers_->Add(reg.code()); }

 private:
  Safepoint(ZoneList<int>* indexes, ZoneList<int>* registers) :
      indexes_(indexes), registers_(registers) { }
  ZoneList<int>* indexes_;
  ZoneList<int>* registers_;

  friend class SafepointTableBuilder;
};


class SafepointTableBuilder BASE_EMBEDDED {
 public:
  SafepointTableBuilder()
      : deoptimization_info_(32),
        indexes_(32),
        registers_(32),
        emitted_(false) { }

  // Get the offset of the emitted safepoint table in the code.
  unsigned GetCodeOffset() const;

  // Define a new safepoint for the current position in the body.
  Safepoint DefineSafepoint(
      Assembler* assembler,
      int deoptimization_index = Safepoint::kNoDeoptimizationIndex);

  // Define a new safepoint with registers on the stack for the
  // current position in the body and take the number of arguments on
  // top of the registers into account.
  Safepoint DefineSafepointWithRegisters(
      Assembler* assembler,
      int arguments,
      int deoptimization_index = Safepoint::kNoDeoptimizationIndex);

  // Update the last safepoint with the size of the code generated for the gap
  // following it.
  void SetPcAfterGap(int pc) {
    ASSERT(!deoptimization_info_.is_empty());
    int index = deoptimization_info_.length() - 1;
    deoptimization_info_[index].pc_after_gap = pc;
  }

  // Emit the safepoint table after the body. The number of bits per
  // entry must be enough to hold all the pointer indexes.
  void Emit(Assembler* assembler, int bits_per_entry);

 private:
  struct DeoptimizationInfo {
    unsigned pc;
    unsigned deoptimization_index;
    unsigned pc_after_gap;
  };

  uint32_t EncodeDeoptimizationIndexAndGap(DeoptimizationInfo info);

  ZoneList<DeoptimizationInfo> deoptimization_info_;
  ZoneList<ZoneList<int>*> indexes_;
  ZoneList<ZoneList<int>*> registers_;

  bool emitted_;
  unsigned offset_;

  DISALLOW_COPY_AND_ASSIGN(SafepointTableBuilder);
};

} }  // namespace v8::internal

#endif  // V8_SAFEPOINT_TABLE_H_
