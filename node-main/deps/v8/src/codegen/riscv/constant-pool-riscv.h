// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV_CONSTANT_POOL_RISCV_H_
#define V8_CODEGEN_RISCV_CONSTANT_POOL_RISCV_H_

#include <map>

#include "src/base/numbers/double.h"
#include "src/codegen/label.h"
#include "src/codegen/reloc-info.h"
#include "src/codegen/riscv/base-constants-riscv.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Instruction;

class ConstantPoolKey {
 public:
  explicit ConstantPoolKey(uint64_t value,
                           RelocInfo::Mode rmode = RelocInfo::NO_INFO)
      : value_(value), rmode_(rmode) {}

  uint64_t value() const { return value_; }
  RelocInfo::Mode rmode() const { return rmode_; }

  bool AllowsDeduplication() const {
    DCHECK_NE(rmode_, RelocInfo::CONST_POOL);
    DCHECK_NE(rmode_, RelocInfo::VENEER_POOL);
    DCHECK_NE(rmode_, RelocInfo::DEOPT_SCRIPT_OFFSET);
    DCHECK_NE(rmode_, RelocInfo::DEOPT_INLINING_ID);
    DCHECK_NE(rmode_, RelocInfo::DEOPT_REASON);
    DCHECK_NE(rmode_, RelocInfo::DEOPT_ID);
    DCHECK_NE(rmode_, RelocInfo::DEOPT_NODE_ID);
    // Code targets can be shared because they aren't patched anymore, and we
    // make sure we emit only one reloc info for them (thus delta patching) will
    // apply the delta only once. At the moment, we do not dedup code targets if
    // they are wrapped in a heap object request (value == 0).
    bool is_sharable_code_target =
        RelocInfo::IsCodeTarget(rmode_) && value() != 0;
    bool is_sharable_embedded_object = RelocInfo::IsEmbeddedObjectMode(rmode_);
    return RelocInfo::IsShareableRelocMode(rmode_) || is_sharable_code_target ||
           is_sharable_embedded_object;
  }

 private:
  const uint64_t value_;
  const RelocInfo::Mode rmode_;
};

// Order for pool entries.
inline bool operator<(const ConstantPoolKey& a, const ConstantPoolKey& b) {
  if (a.rmode() < b.rmode()) return true;
  if (a.rmode() > b.rmode()) return false;
  return a.value() < b.value();
}

inline bool operator==(const ConstantPoolKey& a, const ConstantPoolKey& b) {
  return (a.rmode() == b.rmode()) && (a.value() == b.value());
}

// Constant pool generation
enum class Jump { kOmitted, kRequired };
enum class Emission { kIfNeeded, kForced };
enum class Alignment { kOmitted, kRequired };
enum class RelocInfoStatus { kMustRecord, kMustOmitForDuplicate };
enum class PoolEmissionCheck { kSkip };

// Pools are emitted in the instruction stream, preferably after unconditional
// jumps or after returns from functions (in dead code locations).
// If a long code sequence does not contain unconditional jumps, it is
// necessary to emit the constant pool before the pool gets too far from the
// location it is accessed from. In this case, we emit a jump over the emitted
// constant pool.
// Constants in the pool may be addresses of functions that gets relocated;
// if so, a relocation info entry is associated to the constant pool entry.
class ConstantPool {
 public:
  explicit ConstantPool(Assembler* assm);
  ~ConstantPool();

  // Records a constant pool entry. Returns whether we need to write RelocInfo.
  RelocInfoStatus RecordEntry64(uint64_t data, RelocInfo::Mode rmode);

  size_t EntryCount() const { return deduped_entry_count_; }
  bool IsEmpty() const { return deduped_entry_count_ == 0; }

  // Check if pool will be out of range at {pc_offset}.
  bool IsInImmRangeIfEmittedAt(int pc_offset);
  // Size in bytes of the constant pool. Depending on parameters, the size will
  // include the branch over the pool and alignment padding.
  int ComputeSize(Jump require_jump, Alignment require_alignment) const;

  // Emit the pool at the current pc with a branch over the pool if requested.
  void EmitAndClear(Jump require);
  bool ShouldEmitNow(Jump require_jump, size_t margin = 0) const;
  V8_EXPORT_PRIVATE void Check(Emission force_emission, Jump require_jump,
                               size_t margin = 0);

  V8_EXPORT_PRIVATE void MaybeCheck();
  void Clear();

  // Constant pool emission can be blocked temporarily.
  bool IsBlocked() const;

  // Repeated checking whether the constant pool should be emitted is expensive;
  // only check once a number of bytes have been generated.
  void SetNextCheckIn(size_t bytes);

  // Class for scoping postponing the constant pool generation.
  class V8_EXPORT_PRIVATE V8_NODISCARD BlockScope {
   public:
    // BlockScope immediatelly emits the pool if necessary to ensure that
    // during the block scope at least {margin} bytes can be emitted without
    // pool emission becomming necessary.
    explicit BlockScope(Assembler* pool, size_t margin = 0);
    BlockScope(Assembler* pool, PoolEmissionCheck);
    ~BlockScope();

   private:
    ConstantPool* pool_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockScope);
  };

  // Pool entries are accessed with pc relative load therefore this cannot be
  // more than 1 * MB. Since constant pool emission checks are interval based,
  // and we want to keep entries close to the code, we try to emit every 64KB.

  // Hard limit to the const pool which must not be exceeded.
  static const size_t kMaxDistToPool = 1 * MB;
  // Approximate distance where the pool should be emitted.
  V8_EXPORT_PRIVATE static const size_t kApproxDistToPool = 64 * KB;
  // Approximate distance where the pool may be emitted if no jump is required
  // (due to a recent unconditional jump).
  static const size_t kOpportunityDistToPool = 64 * KB;
  // PC distance between constant pool checks.
  V8_EXPORT_PRIVATE static const size_t kCheckInterval = 128 * kInstrSize;
  // Number of entries in the pool which trigger a check.
  static const size_t kApproxMaxEntryCount = 512;

 private:
  void StartBlock();
  void EndBlock();

  void EmitEntries();
  void EmitPrologue(Alignment require_alignment);
  int PrologueSize(Jump require_jump) const;
  RelocInfoStatus RecordKey(ConstantPoolKey key, int offset);
  RelocInfoStatus GetRelocInfoStatusFor(const ConstantPoolKey& key);
  void Emit(const ConstantPoolKey& key);
  void SetLoadOffsetToConstPoolEntry(int load_offset, Instruction* entry_offset,
                                     const ConstantPoolKey& key);
  Alignment IsAlignmentRequiredIfEmittedAt(Jump require_jump,
                                           int pc_offset) const;

  Assembler* assm_;

  // Keep track of the first instruction requiring a constant pool entry
  // since the previous constant pool was emitted.
  int first_use_ = -1;

  // We sort not according to insertion order, but since we do not insert
  // addresses (for heap objects we insert an index which is created in
  // increasing order), the order is deterministic. We map each entry to the
  // pc offset of the load. We use a multimap because we need to record the
  // pc offset of each load of the same constant so that the immediate of the
  // loads can be back-patched when the pool is emitted.
  std::multimap<ConstantPoolKey, int> entries_;

  // Number of entries after deduping. This is different than {entries_.size()}
  // which represents the total number of entries.
  size_t deduped_entry_count_ = 0;

  int next_check_ = 0;
  int blocked_nesting_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_POOL_RISCV_H_
