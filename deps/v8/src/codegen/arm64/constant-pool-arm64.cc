// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-arch.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/constant-pool.h"

namespace v8 {
namespace internal {

// Constant Pool.

ConstantPool::ConstantPool(Assembler* assm) : assm_(assm) {}
ConstantPool::~ConstantPool() { DCHECK_EQ(blocked_nesting_, 0); }

RelocInfoStatus ConstantPool::RecordEntry(uint32_t data,
                                          RelocInfo::Mode rmode) {
  ConstantPoolKey key(data, rmode);
  CHECK(key.is_value32());
  return RecordKey(std::move(key), assm_->pc_offset());
}

RelocInfoStatus ConstantPool::RecordEntry(uint64_t data,
                                          RelocInfo::Mode rmode) {
  ConstantPoolKey key(data, rmode);
  CHECK(!key.is_value32());
  return RecordKey(std::move(key), assm_->pc_offset());
}

RelocInfoStatus ConstantPool::RecordKey(ConstantPoolKey key, int offset) {
  RelocInfoStatus write_reloc_info = GetRelocInfoStatusFor(key);
  if (write_reloc_info == RelocInfoStatus::kMustRecord) {
    if (key.is_value32()) {
      if (entry32_count_ == 0) first_use_32_ = offset;
      ++entry32_count_;
    } else {
      if (entry64_count_ == 0) first_use_64_ = offset;
      ++entry64_count_;
    }
  }
  entries_.insert(std::make_pair(key, offset));

  if (Entry32Count() + Entry64Count() > ConstantPool::kApproxMaxEntryCount) {
    // Request constant pool emission after the next instruction.
    SetNextCheckIn(1);
  }

  return write_reloc_info;
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
  // Prevent recursive pool emission.
  Assembler::BlockPoolsScope block_pools(assm_, PoolEmissionCheck::kSkip);
  Alignment require_alignment =
      IsAlignmentRequiredIfEmittedAt(require_jump, assm_->pc_offset());
  int size = ComputeSize(require_jump, require_alignment);
  Label size_check;
  assm_->bind(&size_check);
  assm_->RecordConstPool(size);

  // Emit the constant pool. It is preceded by an optional branch if
  // {require_jump} and a header which will:
  //  1) Encode the size of the constant pool, for use by the disassembler.
  //  2) Terminate the program, to try to prevent execution from accidentally
  //     flowing into the constant pool.
  //  3) align the 64bit pool entries to 64-bit.
  // TODO(all): Make the alignment part less fragile. Currently code is
  // allocated as a byte array so there are no guarantees the alignment will
  // be preserved on compaction. Currently it works as allocation seems to be
  // 64-bit aligned.

  Label after_pool;
  if (require_jump == Jump::kRequired) assm_->b(&after_pool);

  assm_->RecordComment("[ Constant Pool");
  EmitPrologue(require_alignment);
  if (require_alignment == Alignment::kRequired) assm_->Align(kInt64Size);
  EmitEntries();
  assm_->RecordComment("]");

  if (after_pool.is_linked()) assm_->bind(&after_pool);

  DCHECK_EQ(assm_->SizeOfCodeGeneratedSince(&size_check), size);
  Clear();
}

void ConstantPool::Clear() {
  entries_.clear();
  first_use_32_ = -1;
  first_use_64_ = -1;
  entry32_count_ = 0;
  entry64_count_ = 0;
  next_check_ = 0;
  old_next_check_ = 0;
}

void ConstantPool::StartBlock() {
  if (blocked_nesting_ == 0) {
    // Prevent constant pool checks from happening by setting the next check to
    // the biggest possible offset.
    old_next_check_ = next_check_;
    next_check_ = kMaxInt;
  }
  ++blocked_nesting_;
}

void ConstantPool::EndBlock() {
  --blocked_nesting_;
  if (blocked_nesting_ == 0) {
    DCHECK(IsInImmRangeIfEmittedAt(assm_->pc_offset()));
    // Restore the old next_check_ value if it's less than the current
    // next_check_. This accounts for any attempt to emit pools sooner whilst
    // pools were blocked.
    next_check_ = std::min(next_check_, old_next_check_);
  }
}

bool ConstantPool::IsBlocked() const { return blocked_nesting_ > 0; }

void ConstantPool::SetNextCheckIn(size_t instructions) {
  next_check_ =
      assm_->pc_offset() + static_cast<int>(instructions * kInstrSize);
}

void ConstantPool::EmitEntries() {
  for (auto iter = entries_.begin(); iter != entries_.end();) {
    DCHECK(iter->first.is_value32() || IsAligned(assm_->pc_offset(), 8));
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

void ConstantPool::Emit(const ConstantPoolKey& key) {
  if (key.is_value32()) {
    assm_->dd(key.value32());
  } else {
    assm_->dq(key.value64());
  }
}

bool ConstantPool::ShouldEmitNow(Jump require_jump, size_t margin) const {
  if (IsEmpty()) return false;
  if (Entry32Count() + Entry64Count() > ConstantPool::kApproxMaxEntryCount) {
    return true;
  }
  // We compute {dist32/64}, i.e. the distance from the first instruction
  // accessing a 32bit/64bit entry in the constant pool to any of the
  // 32bit/64bit constant pool entries, respectively. This is required because
  // we do not guarantee that entries are emitted in order of reference, i.e. it
  // is possible that the entry with the earliest reference is emitted last.
  // The constant pool should be emitted if either of the following is true:
  // (A) {dist32/64} will be out of range at the next check in.
  // (B) Emission can be done behind an unconditional branch and {dist32/64}
  // exceeds {kOpportunityDist*}.
  // (C) {dist32/64} exceeds the desired approximate distance to the pool.
  int worst_case_size = ComputeSize(Jump::kRequired, Alignment::kRequired);
  size_t pool_end_32 = assm_->pc_offset() + margin + worst_case_size;
  size_t pool_end_64 = pool_end_32 - Entry32Count() * kInt32Size;
  if (Entry64Count() != 0) {
    // The 64-bit constants are always emitted before the 32-bit constants, so
    // we subtract the size of the 32-bit constants from {size}.
    size_t dist64 = pool_end_64 - first_use_64_;
    bool next_check_too_late = dist64 + 2 * kCheckInterval >= kMaxDistToPool64;
    bool opportune_emission_without_jump =
        require_jump == Jump::kOmitted && (dist64 >= kOpportunityDistToPool64);
    bool approximate_distance_exceeded = dist64 >= kApproxDistToPool64;
    if (next_check_too_late || opportune_emission_without_jump ||
        approximate_distance_exceeded) {
      return true;
    }
  }
  if (Entry32Count() != 0) {
    size_t dist32 = pool_end_32 - first_use_32_;
    bool next_check_too_late = dist32 + 2 * kCheckInterval >= kMaxDistToPool32;
    bool opportune_emission_without_jump =
        require_jump == Jump::kOmitted && (dist32 >= kOpportunityDistToPool32);
    bool approximate_distance_exceeded = dist32 >= kApproxDistToPool32;
    if (next_check_too_late || opportune_emission_without_jump ||
        approximate_distance_exceeded) {
      return true;
    }
  }
  return false;
}

int ConstantPool::ComputeSize(Jump require_jump,
                              Alignment require_alignment) const {
  int size_up_to_marker = PrologueSize(require_jump);
  int alignment = require_alignment == Alignment::kRequired ? kInstrSize : 0;
  size_t size_after_marker =
      Entry32Count() * kInt32Size + alignment + Entry64Count() * kInt64Size;
  return size_up_to_marker + static_cast<int>(size_after_marker);
}

Alignment ConstantPool::IsAlignmentRequiredIfEmittedAt(Jump require_jump,
                                                       int pc_offset) const {
  int size_up_to_marker = PrologueSize(require_jump);
  if (Entry64Count() != 0 &&
      !IsAligned(pc_offset + size_up_to_marker, kInt64Size)) {
    return Alignment::kRequired;
  }
  return Alignment::kOmitted;
}

bool ConstantPool::IsInImmRangeIfEmittedAt(int pc_offset) {
  // Check that all entries are in range if the pool is emitted at {pc_offset}.
  // This ignores kPcLoadDelta (conservatively, since all offsets are positive),
  // and over-estimates the last entry's address with the pool's end.
  Alignment require_alignment =
      IsAlignmentRequiredIfEmittedAt(Jump::kRequired, pc_offset);
  size_t pool_end_32 =
      pc_offset + ComputeSize(Jump::kRequired, require_alignment);
  size_t pool_end_64 = pool_end_32 - Entry32Count() * kInt32Size;
  bool entries_in_range_32 =
      Entry32Count() == 0 || (pool_end_32 < first_use_32_ + kMaxDistToPool32);
  bool entries_in_range_64 =
      Entry64Count() == 0 || (pool_end_64 < first_use_64_ + kMaxDistToPool64);
  return entries_in_range_32 && entries_in_range_64;
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

}  // namespace internal
}  // namespace v8
