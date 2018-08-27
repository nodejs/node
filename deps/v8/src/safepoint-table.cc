// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/safepoint-table.h"

#include "src/assembler-inl.h"
#include "src/deoptimizer.h"
#include "src/disasm.h"
#include "src/frames-inl.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {


bool SafepointEntry::HasRegisters() const {
  DCHECK(is_valid());
  DCHECK(IsAligned(kNumSafepointRegisters, kBitsPerByte));
  const int num_reg_bytes = kNumSafepointRegisters >> kBitsPerByteLog2;
  for (int i = 0; i < num_reg_bytes; i++) {
    if (bits_[i] != SafepointTable::kNoRegisters) return true;
  }
  return false;
}


bool SafepointEntry::HasRegisterAt(int reg_index) const {
  DCHECK(is_valid());
  DCHECK(reg_index >= 0 && reg_index < kNumSafepointRegisters);
  int byte_index = reg_index >> kBitsPerByteLog2;
  int bit_index = reg_index & (kBitsPerByte - 1);
  return (bits_[byte_index] & (1 << bit_index)) != 0;
}

SafepointTable::SafepointTable(Address instruction_start,
                               size_t safepoint_table_offset,
                               uint32_t stack_slots, bool has_deopt)
    : instruction_start_(instruction_start),
      stack_slots_(stack_slots),
      has_deopt_(has_deopt) {
  Address header = instruction_start_ + safepoint_table_offset;
  length_ = Memory::uint32_at(header + kLengthOffset);
  entry_size_ = Memory::uint32_at(header + kEntrySizeOffset);
  pc_and_deoptimization_indexes_ = header + kHeaderSize;
  entries_ = pc_and_deoptimization_indexes_ + (length_ * kFixedEntrySize);
  DCHECK_GT(entry_size_, 0);
  STATIC_ASSERT(SafepointEntry::DeoptimizationIndexField::kMax ==
                Safepoint::kNoDeoptimizationIndex);
}

SafepointTable::SafepointTable(Code* code)
    : SafepointTable(code->InstructionStart(), code->safepoint_table_offset(),
                     code->stack_slots(), true) {}

unsigned SafepointTable::find_return_pc(unsigned pc_offset) {
  for (unsigned i = 0; i < length(); i++) {
    if (GetTrampolinePcOffset(i) == static_cast<int>(pc_offset)) {
      return GetPcOffset(i);
    } else if (GetPcOffset(i) == pc_offset) {
      return pc_offset;
    }
  }
  UNREACHABLE();
  return 0;
}

SafepointEntry SafepointTable::FindEntry(Address pc) const {
  unsigned pc_offset = static_cast<unsigned>(pc - instruction_start_);
  // We use kMaxUInt32 as sentinel value, so check that we don't hit that.
  DCHECK_NE(kMaxUInt32, pc_offset);
  unsigned len = length();
  // If pc == kMaxUInt32, then this entry covers all call sites in the function.
  if (len == 1 && GetPcOffset(0) == kMaxUInt32) return GetEntry(0);
  for (unsigned i = 0; i < len; i++) {
    // TODO(kasperl): Replace the linear search with binary search.
    if (GetPcOffset(i) == pc_offset ||
        (has_deopt_ &&
         GetTrampolinePcOffset(i) == static_cast<int>(pc_offset))) {
      return GetEntry(i);
    }
  }
  UNREACHABLE();
  return SafepointEntry();
}


void SafepointTable::PrintEntry(unsigned index,
                                std::ostream& os) const {  // NOLINT
  disasm::NameConverter converter;
  SafepointEntry entry = GetEntry(index);
  uint8_t* bits = entry.bits();

  // Print the stack slot bits.
  if (entry_size_ > 0) {
    DCHECK(IsAligned(kNumSafepointRegisters, kBitsPerByte));
    const int first = kNumSafepointRegisters >> kBitsPerByteLog2;
    int last = entry_size_ - 1;
    for (int i = first; i < last; i++) PrintBits(os, bits[i], kBitsPerByte);
    int last_bits = stack_slots_ - ((last - first) * kBitsPerByte);
    PrintBits(os, bits[last], last_bits);

    // Print the registers (if any).
    if (!entry.HasRegisters()) return;
    for (int j = 0; j < kNumSafepointRegisters; j++) {
      if (entry.HasRegisterAt(j)) {
        os << " | " << converter.NameOfCPURegister(j);
      }
    }
  }
}


void SafepointTable::PrintBits(std::ostream& os,  // NOLINT
                               uint8_t byte, int digits) {
  DCHECK(digits >= 0 && digits <= kBitsPerByte);
  for (int i = 0; i < digits; i++) {
    os << (((byte & (1 << i)) == 0) ? "0" : "1");
  }
}


void Safepoint::DefinePointerRegister(Register reg, Zone* zone) {
  registers_->Add(reg.code(), zone);
}


Safepoint SafepointTableBuilder::DefineSafepoint(
    Assembler* assembler,
    Safepoint::Kind kind,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  DCHECK_GE(arguments, 0);
  deoptimization_info_.Add(
      DeoptimizationInfo(zone_, assembler->pc_offset(), arguments, kind),
      zone_);
  if (deopt_mode == Safepoint::kNoLazyDeopt) {
    last_lazy_safepoint_ = deoptimization_info_.length();
  }
  DeoptimizationInfo& new_info = deoptimization_info_.last();
  return Safepoint(new_info.indexes, new_info.registers);
}


void SafepointTableBuilder::RecordLazyDeoptimizationIndex(int index) {
  while (last_lazy_safepoint_ < deoptimization_info_.length()) {
    deoptimization_info_[last_lazy_safepoint_++].deopt_index = index;
  }
}

unsigned SafepointTableBuilder::GetCodeOffset() const {
  DCHECK(emitted_);
  return offset_;
}

int SafepointTableBuilder::UpdateDeoptimizationInfo(int pc, int trampoline,
                                                    int start) {
  int index = -1;
  for (int i = start; i < deoptimization_info_.length(); i++) {
    if (static_cast<int>(deoptimization_info_[i].pc) == pc) {
      index = i;
      break;
    }
  }
  CHECK_GE(index, 0);
  DCHECK(index < deoptimization_info_.length());
  deoptimization_info_[index].trampoline = trampoline;
  return index;
}

void SafepointTableBuilder::Emit(Assembler* assembler, int bits_per_entry) {
  RemoveDuplicates();

  // Make sure the safepoint table is properly aligned. Pad with nops.
  assembler->Align(kIntSize);
  assembler->RecordComment(";;; Safepoint table.");
  offset_ = assembler->pc_offset();

  // Take the register bits into account.
  bits_per_entry += kNumSafepointRegisters;

  // Compute the number of bytes per safepoint entry.
  int bytes_per_entry =
      RoundUp(bits_per_entry, kBitsPerByte) >> kBitsPerByteLog2;

  // Emit the table header.
  int length = deoptimization_info_.length();
  assembler->dd(length);
  assembler->dd(bytes_per_entry);

  // Emit sorted table of pc offsets together with deoptimization indexes.
  for (int i = 0; i < length; i++) {
    const DeoptimizationInfo& info = deoptimization_info_[i];
    assembler->dd(info.pc);
    assembler->dd(EncodeExceptPC(info));
    assembler->dd(info.trampoline);
  }

  // Emit table of bitmaps.
  ZoneList<uint8_t> bits(bytes_per_entry, zone_);
  for (int i = 0; i < length; i++) {
    ZoneList<int>* indexes = deoptimization_info_[i].indexes;
    ZoneList<int>* registers = deoptimization_info_[i].registers;
    bits.Clear();
    bits.AddBlock(0, bytes_per_entry, zone_);

    // Run through the registers (if any).
    DCHECK(IsAligned(kNumSafepointRegisters, kBitsPerByte));
    if (registers == nullptr) {
      const int num_reg_bytes = kNumSafepointRegisters >> kBitsPerByteLog2;
      for (int j = 0; j < num_reg_bytes; j++) {
        bits[j] = SafepointTable::kNoRegisters;
      }
    } else {
      for (int j = 0; j < registers->length(); j++) {
        int index = registers->at(j);
        DCHECK(index >= 0 && index < kNumSafepointRegisters);
        int byte_index = index >> kBitsPerByteLog2;
        int bit_index = index & (kBitsPerByte - 1);
        bits[byte_index] |= (1 << bit_index);
      }
    }

    // Run through the indexes and build a bitmap.
    for (int j = 0; j < indexes->length(); j++) {
      int index = bits_per_entry - 1 - indexes->at(j);
      int byte_index = index >> kBitsPerByteLog2;
      int bit_index = index & (kBitsPerByte - 1);
      bits[byte_index] |= (1U << bit_index);
    }

    // Emit the bitmap for the current entry.
    for (int k = 0; k < bytes_per_entry; k++) {
      assembler->db(bits[k]);
    }
  }
  emitted_ = true;
}

uint32_t SafepointTableBuilder::EncodeExceptPC(const DeoptimizationInfo& info) {
  return SafepointEntry::DeoptimizationIndexField::encode(info.deopt_index) |
         SafepointEntry::ArgumentsField::encode(info.arguments) |
         SafepointEntry::SaveDoublesField::encode(info.has_doubles);
}

void SafepointTableBuilder::RemoveDuplicates() {
  // If the table contains more than one entry, and all entries are identical
  // (except for the pc), replace the whole table by a single entry with pc =
  // kMaxUInt32. This especially compacts the table for wasm code without tagged
  // pointers and without deoptimization info.

  int length = deoptimization_info_.length();
  if (length < 2) return;

  // Check that all entries (1, length] are identical to entry 0.
  const DeoptimizationInfo& first_info = deoptimization_info_[0];
  for (int i = 1; i < length; ++i) {
    if (!IsIdenticalExceptForPc(first_info, deoptimization_info_[i])) return;
  }

  // If we get here, all entries were identical. Rewind the list to just one
  // entry, and set the pc to kMaxUInt32.
  deoptimization_info_.Rewind(1);
  deoptimization_info_[0].pc = kMaxUInt32;
}

bool SafepointTableBuilder::IsIdenticalExceptForPc(
    const DeoptimizationInfo& info1, const DeoptimizationInfo& info2) const {
  if (info1.arguments != info2.arguments) return false;
  if (info1.has_doubles != info2.has_doubles) return false;

  if (info1.deopt_index != info2.deopt_index) return false;

  ZoneList<int>* indexes1 = info1.indexes;
  ZoneList<int>* indexes2 = info2.indexes;
  if (indexes1->length() != indexes2->length()) return false;
  for (int i = 0; i < indexes1->length(); ++i) {
    if (indexes1->at(i) != indexes2->at(i)) return false;
  }

  ZoneList<int>* registers1 = info1.registers;
  ZoneList<int>* registers2 = info2.registers;
  if (registers1) {
    if (!registers2) return false;
    if (registers1->length() != registers2->length()) return false;
    for (int i = 0; i < registers1->length(); ++i) {
      if (registers1->at(i) != registers2->at(i)) return false;
    }
  } else if (registers2) {
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace v8
