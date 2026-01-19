// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-arch.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/constant-pool.h"

namespace v8 {
namespace internal {

ConstantPool::ConstantPool(Assembler* assm) : assm_(assm) {}
ConstantPool::~ConstantPool() { DCHECK_EQ(blocked_nesting_, 0); }

RelocInfoStatus ConstantPool::RecordEntry64(uint64_t data,
                                            RelocInfo::Mode rmode) {
  ConstantPoolKey key(data, rmode);
  return RecordKey(std::move(key), assm_->pc_offset());
}

RelocInfoStatus ConstantPool::RecordKey(ConstantPoolKey key, int offset) {
  RelocInfoStatus status = GetRelocInfoStatusFor(key);
  if (status == RelocInfoStatus::kMustRecord) {
    size_t count = ++deduped_entry_count_;
    if (count == 1) {
      first_use_ = offset;
    } else if (count > ConstantPool::kApproxMaxEntryCount) {
      // Request constant pool emission after the next instruction.
      SetNextCheckIn(kInstrSize);
    }
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

void ConstantPool::EmitAndClear(Jump require_jump) {
  DCHECK(!IsBlocked());
  // Prevent recursive pool emission. We conservatively assume that we will
  // have to add padding for alignment, so the margin is guaranteed to be
  // at least as large as the actual size of the constant pool.
  int margin = ComputeSize(require_jump, Alignment::kRequired);
  Assembler::BlockPoolsScope block_pools(assm_, PoolEmissionCheck::kSkip,
                                         margin);

  // The pc offset may have changed as a result of blocking pools. We can
  // now go ahead and compute the required alignment and the correct size.
  Alignment require_alignment =
      IsAlignmentRequiredIfEmittedAt(require_jump, assm_->pc_offset());
  int size = ComputeSize(require_jump, require_alignment);
  DCHECK_LE(size, margin);
  Label size_check;
  assm_->bind(&size_check);
  assm_->RecordConstPool(size, block_pools);

  // Emit the constant pool. It is preceded by an optional branch if
  // {require_jump} and a header which will:
  //  1) Encode the size of the constant pool, for use by the disassembler.
  //  2) Terminate the program, to try to prevent execution from accidentally
  //     flowing into the constant pool.
  //  3) align the pool entries to 64-bit.

  DEBUG_PRINTF("\tConstant Pool start\n")
  Label after_pool;
  if (require_jump == Jump::kRequired) assm_->b(&after_pool);

  assm_->RecordComment("[ Constant Pool");
  EmitPrologue(require_alignment);
  if (require_alignment == Alignment::kRequired) assm_->DataAlign(kInt64Size);
  EmitEntries();

  // Emit padding data to ensure the constant pool size matches the expected
  // constant count during disassembly. This can only happen if we ended up
  // overestimating the size of the pool in {ComputeSize}.
  int code_size = assm_->SizeOfCodeGeneratedSince(&size_check);
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
  DEBUG_PRINTF("\tConstant Pool end\n")
  DCHECK_EQ(size, assm_->SizeOfCodeGeneratedSince(&size_check));
  Clear();
}

void ConstantPool::Clear() {
  entries_.clear();
  first_use_ = -1;
  deduped_entry_count_ = 0;
  next_check_ = 0;
}

void ConstantPool::StartBlock() {
  if (blocked_nesting_ == 0) {
    // Prevent constant pool checks from happening by setting the next check to
    // the biggest possible offset.
    next_check_ = kMaxInt;
  }
  ++blocked_nesting_;
}

void ConstantPool::EndBlock() {
  --blocked_nesting_;
  if (blocked_nesting_ == 0) {
    DCHECK(IsInImmRangeIfEmittedAt(assm_->pc_offset()));
    // Make sure a check happens quickly after getting unblocked.
    next_check_ = 0;
  }
}

bool ConstantPool::IsBlocked() const { return blocked_nesting_ > 0; }

void ConstantPool::SetNextCheckIn(size_t bytes) {
  next_check_ = assm_->pc_offset() + static_cast<int>(bytes);
}

void ConstantPool::EmitEntries() {
  for (auto iter = entries_.begin(); iter != entries_.end();) {
    DCHECK(IsAligned(assm_->pc_offset(), 8));
    auto range = entries_.equal_range(iter->first);
    bool shared = iter->first.AllowsDeduplication();
    for (auto it = range.first; it != range.second; ++it) {
      SetLoadOffsetToConstPoolEntry(it->second, assm_->pc(), it->first);
      if (!shared) Emit(it->first);
    }
    if (shared) Emit(iter->first);
    iter = range.second;
  }
}

void ConstantPool::Emit(const ConstantPoolKey& key) { assm_->dq(key.value()); }

bool ConstantPool::ShouldEmitNow(Jump require_jump, size_t margin) const {
  if (IsEmpty()) return false;
  if (EntryCount() > ConstantPool::kApproxMaxEntryCount) return true;
  // We compute {dist}, i.e. the distance from the first instruction accessing
  // an entry in the constant pool to any of the constant pool entries,
  // respectively. This is required because we do not guarantee that entries
  // are emitted in order of reference, i.e. it is possible that the entry with
  // the earliest reference is emitted last. The constant pool should be emitted
  // if either of the following is true:
  // (A) {dist} will be out of range at the next check in.
  // (B) Emission can be done behind an unconditional branch and {dist}
  // exceeds {kOpportunityDistToPool}.
  // (C) {dist} exceeds the desired approximate distance to the pool.
  int worst_case_size = ComputeSize(Jump::kRequired, Alignment::kRequired);
  size_t pool_end = assm_->pc_offset() + margin + worst_case_size;
  size_t dist = pool_end - first_use_;
  bool next_check_too_late = dist + 2 * kCheckInterval >= kMaxDistToPool;
  bool opportune_emission_without_jump =
      require_jump == Jump::kOmitted && (dist >= kOpportunityDistToPool);
  bool approximate_distance_exceeded = dist >= kApproxDistToPool;
  return next_check_too_late || opportune_emission_without_jump ||
         approximate_distance_exceeded;
}

int ConstantPool::ComputeSize(Jump require_jump,
                              Alignment require_alignment) const {
  int prologue_size = PrologueSize(require_jump);
  // TODO(kasperl): It would be nice to just compute the exact amount of
  // padding needed, but that requires knowing the {pc_offset} where the
  // constant pool will be emitted. For now, we will just compute the
  // maximum padding needed and add additional padding after the pool if
  // we overestimated it.
  size_t max_padding = 0;
  if (require_alignment == Alignment::kRequired) {
    size_t instruction_size = kInstrSize;
    if (v8_flags.riscv_c_extension) instruction_size = kShortInstrSize;
    max_padding = kInt64Size - instruction_size;
  }
  size_t entries_size = max_padding + EntryCount() * kInt64Size;
  return prologue_size + static_cast<int>(entries_size);
}

Alignment ConstantPool::IsAlignmentRequiredIfEmittedAt(Jump require_jump,
                                                       int pc_offset) const {
  if (EntryCount() == 0) return Alignment::kOmitted;
  int prologue_size = PrologueSize(require_jump);
  return IsAligned(pc_offset + prologue_size, kInt64Size)
             ? Alignment::kOmitted
             : Alignment::kRequired;
}

bool ConstantPool::IsInImmRangeIfEmittedAt(int pc_offset) {
  // Check that all entries are in range if the pool is emitted at {pc_offset}.
  if (EntryCount() == 0) return true;
  Alignment require_alignment =
      IsAlignmentRequiredIfEmittedAt(Jump::kRequired, pc_offset);
  size_t pool_end = pc_offset + ComputeSize(Jump::kRequired, require_alignment);
  return pool_end < first_use_ + kMaxDistToPool;
}

ConstantPool::BlockScope::BlockScope(Assembler* assm, size_t margin)
    : pool_(&assm->constpool_) {
  pool_->assm_->EmitConstPoolWithJumpIfNeeded(margin);
  pool_->StartBlock();
}

ConstantPool::BlockScope::BlockScope(Assembler* assm, PoolEmissionCheck check)
    : pool_(&assm->constpool_) {
  DCHECK_EQ(check, PoolEmissionCheck::kSkip);
  pool_->StartBlock();
}

ConstantPool::BlockScope::~BlockScope() { pool_->EndBlock(); }

void ConstantPool::MaybeCheck() {
  if (assm_->pc_offset() >= next_check_) {
    Check(Emission::kIfNeeded, Jump::kRequired);
  }
}

void ConstantPool::EmitPrologue(Alignment require_alignment) {
  // Recorded constant pool size is expressed in number of 32-bits words,
  // and includes prologue and alignment, but not the jump around the pool
  // and the size of the marker itself. The word count may exceed 12 bits,
  // so 'auipc' is used as the marker.
  const int kMarkerSize = kInstrSize;  // Size of 'auipc' instruction.
  int size = ComputeSize(Jump::kOmitted, require_alignment) - kMarkerSize;
  int words = RoundUp(size, kInt32Size) / kInt32Size;
  DCHECK(is_int20(words));
  assm_->auipc(zero_reg, words);
  assm_->EmitPoolGuard();
}

int ConstantPool::PrologueSize(Jump require_jump) const {
  // Prologue is:
  //   j     L           ;; Optional, only if {require_jump}.
  //   auipc x0, #words  ;; Pool marker, encodes size in 32-bit words.
  //   j     0x0         ;; Pool guard.
  // L:
  return (require_jump == Jump::kRequired) ? 3 * kInstrSize : 2 * kInstrSize;
}

void ConstantPool::SetLoadOffsetToConstPoolEntry(int load_offset,
                                                 Instruction* entry_offset,
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
  int32_t distance = static_cast<int32_t>(
      reinterpret_cast<Address>(entry_offset) -
      reinterpret_cast<Address>(assm_->toAddress(load_offset)));
  CHECK(is_int32(distance + 0x800));
  int32_t Hi20 = (static_cast<int32_t>(distance) + 0x800) >> 12;
  int32_t Lo12 = static_cast<int32_t>(distance) << 20 >> 20;
  assm_->instr_at_put(load_offset, SetHi20Offset(Hi20, instr_auipc));
  assm_->instr_at_put(load_offset + 4, SetLo12Offset(Lo12, instr_load));
}

void ConstantPool::Check(Emission force_emit, Jump require_jump,
                         size_t margin) {
  // Some short sequence of instruction must not be broken up by constant pool
  // emission, such sequences are protected by a ConstPool::BlockScope.
  if (IsBlocked() || assm_->is_trampoline_pool_blocked()) {
    // Something is wrong if emission is forced and blocked at the same time.
    DCHECK_EQ(force_emit, Emission::kIfNeeded);
    return;
  }

  // We emit a constant pool only if :
  //  * it is not empty
  //  * emission is forced by parameter force_emit (e.g. at function end).
  //  * emission is mandatory or opportune according to {ShouldEmitNow}.
  if (!IsEmpty() && (force_emit == Emission::kForced ||
                     ShouldEmitNow(require_jump, margin))) {
    // Check that the code buffer is large enough before emitting the constant
    // pool (this includes the gap to the relocation information).
    int worst_case_size = ComputeSize(Jump::kRequired, Alignment::kRequired);
    int needed_space = worst_case_size + assm_->kGap;
    while (assm_->buffer_space() <= needed_space) {
      assm_->GrowBuffer();
    }

    // Since we do not know how much space the constant pool is going to take
    // up, we cannot handle getting here while the trampoline pool is blocked.
    CHECK(!assm_->is_trampoline_pool_blocked());
    EmitAndClear(require_jump);
  }
  // Since a constant pool is (now) empty, move the check offset forward by
  // the standard interval.
  SetNextCheckIn(ConstantPool::kCheckInterval);
}

}  // namespace internal
}  // namespace v8
