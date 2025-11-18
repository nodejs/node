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
  ConstantPoolKey key(data, rmode);
  return RecordKey(std::move(key), assm_->pc_offset());
}

RelocInfoStatus ConstantPool::RecordKey(ConstantPoolKey key, int offset) {
  RelocInfoStatus status = GetRelocInfoStatusFor(key);
  if (status == RelocInfoStatus::kMustRecord) {
    size_t count = ++deduped_entry_count_;
    if (count == 1) first_use_ = offset;
    // The next check in position depends on the entry count, so we
    // potentially update the position here.
    MaybeUpdateNextCheckIn();
  }
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

void ConstantPool::EmitAndClear(Jump jump) {
  // Since we do not know how much space the constant pool is going to take
  // up, we cannot handle getting here while the trampoline pool is blocked.
  CHECK(!assm_->pools_blocked());

  // Prevent recursive pool emission.
  int margin = SizeIfEmittedAt(jump, assm_->pc_offset());
  Assembler::BlockPoolsScope block_pools(assm_, ConstantPoolEmission::kSkip,
                                         margin);

  // The pc offset may have changed as a result of blocking pools. We can
  // now go ahead and compute the required padding and the correct size.
  int padding = PaddingIfEmittedAt(jump, assm_->pc_offset());
  int size = SizeOfPool(jump, padding);
  Label before_pool;
  assm_->bind(&before_pool);
  assm_->RecordConstPool(size, block_pools);

  // Emit the constant pool. It is preceded by an optional branch if {jump}
  // is {Jump::kRequired} and a header which will:
  //  1) Encode the size of the constant pool, for use by the disassembler.
  //  2) Terminate the program, to try to prevent execution from accidentally
  //     flowing into the constant pool.
  //  3) Align the pool entries using the computed padding.

  Label after_pool;
  assm_->RecordComment("[ Constant Pool");
  EmitPrologue(size, jump == Jump::kRequired ? &after_pool : nullptr);
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
  assm_->bind(&after_pool);
  DCHECK_EQ(size, assm_->SizeOfCodeGeneratedSince(&before_pool));
  Clear();
}

void ConstantPool::Clear() {
  entries_.clear();
  first_use_ = -1;
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

bool ConstantPool::ShouldEmitNow(Jump jump, size_t margin) const {
  if (IsEmpty()) return false;
  if (EntryCount() > ConstantPool::kApproxMaxEntryCount) return true;

  // We compute {distance}, i.e. the distance from the first instruction
  // accessing an entry in the constant pool to any of the constant pool
  // entries, respectively. This is required because we do not guarantee
  // that entries are emitted in order of reference, i.e. it is possible
  // that the entry with the earliest reference is emitted last.
  int pc_offset = assm_->pc_offset();
  int size = SizeIfEmittedAt(jump, pc_offset);
  size_t pool_end = pc_offset + margin + size;
  size_t distance = pool_end - first_use_;

  if (distance + kCheckInterval >= kMaxDistToPool) {
    // We will be out of range at the next check.
    return true;
  } else if (jump == Jump::kOmitted && distance >= kOpportunityDistToPool) {
    // We can emit the constant pool without a jump here and the distance
    // indicates that this may be a good time.
    return true;
  } else {
    // We ask to get the constant pool emitted if the {distance} exceeds
    // the desired approximate distance to the pool.
    return distance >= kApproxDistToPool;
  }
}

int ConstantPool::AlignmentIfEmittedAt(Jump jump, int pc_offset) const {
  // For now, the alignment does not depend on the {pc_offset}.
  return IsEmpty() ? 0 : kInt64Size;
}

int ConstantPool::PaddingIfEmittedAt(Jump jump, int pc_offset) const {
  int alignment = AlignmentIfEmittedAt(jump, pc_offset);
  if (alignment == 0) return 0;
  int entries_offset = pc_offset + SizeOfPrologue(jump);
  return RoundUp(entries_offset, alignment) - entries_offset;
}

bool ConstantPool::IsInRangeIfEmittedAt(Jump jump, int pc_offset) const {
  // Check that all entries are in range if the pool is emitted at {pc_offset}.
  if (IsEmpty()) return true;
  size_t pool_end = pc_offset + SizeIfEmittedAt(jump, pc_offset);
  return pool_end < first_use_ + kMaxDistToPool;
}

void ConstantPool::EmitPrologue(int size, Label* after) {
  // Encoded constant pool size is expressed in number of 32-bits words,
  // and includes prologue and alignment, but not the jump around the pool
  // and the size of the marker itself. The word count may exceed 12 bits,
  // so 'auipc' is used as the marker.
  const int kAuipcSize = kInstrSize;
  int encoded_size = size - kAuipcSize;
  if (after) {
    assm_->b(after);
    encoded_size -= kInstrSize;  // Jump isn't included in encoded size.
  }
  DCHECK(IsAligned(encoded_size, kInt32Size));
  int encoded_words = encoded_size / kInt32Size;
  DCHECK(is_int20(encoded_words));
  assm_->auipc(zero_reg, encoded_words);
  assm_->EmitPoolGuard();
}

int ConstantPool::SizeOfPrologue(Jump jump) const {
  // Prologue is:
  //   j     L           ;; Optional, only if {jump} is required.
  //   auipc x0, #words  ;; Pool marker, encodes size in 32-bit words.
  //   j     0x0         ;; Pool guard.
  //   <padding>         ;; Optional to align the following constants.
  //   <constants>
  //   <padding>         ;; Optional to round up to full 32-bit words.
  // L:
  return (jump == Jump::kRequired) ? 3 * kInstrSize : 2 * kInstrSize;
}

int ConstantPool::SizeOfPool(Jump jump, int padding) const {
  int prologue_size = SizeOfPrologue(jump);
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

void ConstantPool::Check(Emission force_emit, Jump jump, size_t margin) {
  // Some short sequence of instruction must not be broken up by constant pool
  // emission, such sequences are protected by an Assembler::BlockPoolsScope.
  if (assm_->pools_blocked()) {
    // Something is wrong if emission is forced and blocked at the same time.
    DCHECK_EQ(force_emit, Emission::kIfNeeded);
    return;
  }

  // We emit a constant pool only if :
  //  * it is not empty
  //  * emission is forced by parameter force_emit (e.g. at function end).
  //  * emission is mandatory or opportune according to {ShouldEmitNow}.
  if (!IsEmpty() &&
      (force_emit == Emission::kForced || ShouldEmitNow(jump, margin))) {
    EmitAndClear(jump);
  }

  // Update the last check position and maybe the next one.
  check_last_ = assm_->pc_offset();
  MaybeUpdateNextCheckIn();
}

}  // namespace internal
}  // namespace v8
