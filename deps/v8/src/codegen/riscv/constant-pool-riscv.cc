// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-arch.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/constant-pool.h"

namespace v8 {
namespace internal {

RelocInfoStatus ConstantPool::RecordEntry64(uint64_t data,
                                            RelocInfo::Mode rmode) {
  CHECK(!emitted_);
  ConstantPoolKey key(data, rmode);
  return RecordKey(std::move(key), assm_->pc_offset());
}

RelocInfoStatus ConstantPool::RecordKey(ConstantPoolKey key, int offset) {
  RelocInfoStatus status = GetRelocInfoStatusFor(key);
  if (status == RelocInfoStatus::kMustRecord) deduped_entry_count_++;
  entries_.insert(std::make_pair(key, offset));
  return status;
}

RelocInfoStatus ConstantPool::GetRelocInfoStatusFor(
    const ConstantPoolKey& key) {
  if (key.AllowsDeduplication()) {
    auto existing = entries_.find(key);
    if (existing != entries_.end()) {
      return RelocInfoStatus::kMustOmitForDuplicate;
    }
  }
  return RelocInfoStatus::kMustRecord;
}

void ConstantPool::Emit() {
  if (emitted_) return;
  emitted_ = true;

  // Since we do not know how much space the constant pool is going to take
  // up, we cannot handle getting here while the trampoline pool is blocked.
  CHECK(!assm_->pools_blocked());

  // Prevent recursive pool emission.
  if (IsEmpty()) return;
  int margin = SizeIfEmittedAt(assm_->pc_offset());
  Assembler::BlockPoolsScope block_pools(assm_, margin);

  // The pc offset may have changed as a result of blocking pools. We can
  // now go ahead and compute the required padding and the correct size.
  int padding = PaddingIfEmittedAt(assm_->pc_offset());
  int size = SizeOfPool(padding);
  Label before_pool;
  assm_->bind(&before_pool);
  assm_->RecordConstPool(size, block_pools);

  // Emit the constant pool. It is preceded by a header which will:
  //  1) Encode the size of the constant pool, for use by the disassembler.
  //  2) Terminate the program, to try to prevent execution from accidentally
  //     flowing into the constant pool.
  //  3) Align the pool entries using the computed padding.
  assm_->RecordComment("[ Constant Pool");
  EmitPrologue(size);
  for (int i = 0; i < padding; i++) assm_->db(0xcc);
  EmitEntries();

  // Emit padding data to ensure the constant pool size matches the expected
  // constant count during disassembly. This can only happen if we ended up
  // overestimating the size of the pool in {ComputeSize} due to it being
  // rounded up to kInt32Size.
  int code_size = assm_->SizeOfCodeGeneratedSince(&before_pool);
  if (v8_flags.riscv_c_extension) {
    DCHECK_LE(code_size, size);
    while (code_size < size) {
      assm_->db(0xcc);
      code_size++;
    }
  } else {
    DCHECK_EQ(size, code_size);
  }

  assm_->RecordComment("]");
  DCHECK_EQ(size, assm_->SizeOfCodeGeneratedSince(&before_pool));
  Clear();
}

void ConstantPool::Clear() {
  entries_.clear();
  deduped_entry_count_ = 0;
}

void ConstantPool::EmitEntries() {
  int count = 0;
  USE(count);  // Only used in DCHECK below.
  for (auto iter = entries_.begin(); iter != entries_.end();) {
    DCHECK(IsAligned(assm_->pc_offset(), 8));
    auto range = entries_.equal_range(iter->first);
    bool shared = iter->first.AllowsDeduplication();
    for (auto it = range.first; it != range.second; ++it) {
      SetLoadOffsetToConstPoolEntry(it->second, assm_->pc_offset(), it->first);
      if (!shared) {
        Emit(it->first);
        count++;
      }
    }
    if (shared) {
      Emit(iter->first);
      count++;
    }
    iter = range.second;
  }
  DCHECK_EQ(EntryCount(), count);
}

void ConstantPool::Emit(const ConstantPoolKey& key) { assm_->dq(key.value()); }

int ConstantPool::AlignmentIfEmittedAt(int pc_offset) const {
  if (IsEmpty()) return 0;
  // For now, the alignment does not depend on the {pc_offset}.
#ifdef RISCV_CONSTANT_POOL_ALIGNMENT
  static_assert(base::bits::IsPowerOfTwo(RISCV_CONSTANT_POOL_ALIGNMENT));
  static_assert(RISCV_CONSTANT_POOL_ALIGNMENT >= kInt64Size);
  static_assert(RISCV_CONSTANT_POOL_ALIGNMENT <= RISCV_CODE_ALIGNMENT);
  return RISCV_CONSTANT_POOL_ALIGNMENT;
#else
  return kInt64Size;
#endif
}

int ConstantPool::PaddingIfEmittedAt(int pc_offset) const {
  int alignment = AlignmentIfEmittedAt(pc_offset);
  if (alignment == 0) return 0;
  int entries_offset = pc_offset + SizeOfPrologue();
  return RoundUp(entries_offset, alignment) - entries_offset;
}

void ConstantPool::EmitPrologue(int size) {
  // Encoded constant pool size is expressed in number of 32-bits words,
  // and includes prologue and alignment, but not the size of the marker
  // itself. The word count may exceed 12 bits, so 'auipc' is used as the
  // marker.
  const int kAuipcSize = kInstrSize;
  int encoded_size = size - kAuipcSize;
  DCHECK(IsAligned(encoded_size, kInt32Size));
  int encoded_words = encoded_size / kInt32Size;
  DCHECK(is_int20(encoded_words));
  assm_->auipc(zero_reg, encoded_words);
  assm_->EmitPoolGuard();
}

int ConstantPool::SizeOfPrologue() const {
  // Prologue is:
  //   auipc x0, #words  ;; Pool marker, encodes size in 32-bit words.
  //   j     0x0         ;; Pool guard.
  //   <padding>         ;; Optional to align the following constants.
  //   <constants>
  //   <padding>         ;; Optional to round up to full 32-bit words.
  return 2 * kInstrSize;
}

int ConstantPool::SizeOfPool(int padding) const {
  int prologue_size = SizeOfPrologue();
  int padding_after = RoundUp(padding, kInt32Size) - padding;
  DCHECK(v8_flags.riscv_c_extension || padding_after == 0);
  int entries_size = static_cast<int>(EntryCount() * kInt64Size);
  return prologue_size + padding + entries_size + padding_after;
}

void ConstantPool::SetLoadOffsetToConstPoolEntry(int load_offset,
                                                 int entry_offset,
                                                 const ConstantPoolKey& key) {
  Instr instr_auipc = assm_->instr_at(load_offset);
  Instr instr_load = assm_->instr_at(load_offset + 4);
  // Instructions to patch must be:
  //   auipc rd, 0(rd)
  //   ld    rd, 1(rd)
  DCHECK(assm_->IsAuipc(instr_auipc));
  DCHECK(assm_->IsLoadWord(instr_load));
  DCHECK_EQ(assm_->AuipcOffset(instr_auipc), 0);
  DCHECK_EQ(assm_->LoadOffset(instr_load), 1);
  int32_t distance = entry_offset - load_offset;
  CHECK(is_int32(distance + 0x800));
  int32_t Hi20 = (static_cast<int32_t>(distance) + 0x800) >> 12;
  int32_t Lo12 = static_cast<int32_t>(distance) << 20 >> 20;
  assm_->instr_at_put(load_offset, SetHi20Offset(Hi20, instr_auipc));
  assm_->instr_at_put(load_offset + 4, SetLo12Offset(Lo12, instr_load));
}

}  // namespace internal
}  // namespace v8
