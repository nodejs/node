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

enum class RelocInfoStatus { kMustRecord, kMustOmitForDuplicate };

// Pools are emitted near the end of the instruction stream. Constants in the
// pool may be addresses of functions that gets relocated; if so, a relocation
// info entry is associated to the constant pool entry.
class ConstantPool {
 public:
  explicit ConstantPool(Assembler* assm) : assm_(assm) {}

  bool IsEmpty() const { return deduped_entry_count_ == 0; }
  void Clear();

  // Records a constant pool entry. Returns whether we need to write RelocInfo.
  RelocInfoStatus RecordEntry64(uint64_t data, RelocInfo::Mode rmode);

  // Emit the constant pool here. After emitting the pool, no more constants
  // can be recorded.
  void Emit();

 private:
  size_t EntryCount() const { return deduped_entry_count_; }

  void EmitEntries();
  void EmitPrologue(int size);

  // Size of the prologue in bytes.
  int SizeOfPrologue() const;
  int SizeOfPool(int padding) const;

  RelocInfoStatus RecordKey(ConstantPoolKey key, int offset);
  RelocInfoStatus GetRelocInfoStatusFor(const ConstantPoolKey& key);
  void Emit(const ConstantPoolKey& key);
  void SetLoadOffsetToConstPoolEntry(int load_offset, int entry_offset,
                                     const ConstantPoolKey& key);

  // Alignment and padding in bytes if emitted at the given {pc_offset}.
  int AlignmentIfEmittedAt(int pc_offset) const;
  int PaddingIfEmittedAt(int pc_offset) const;

  // Size in bytes of the constant pool if emitted at the given {pc_offset}.
  // Depending on parameters, the size will include the branch over the pool
  // and padding for alignment.
  int SizeIfEmittedAt(int pc_offset) const {
    return SizeOfPool(PaddingIfEmittedAt(pc_offset));
  }

  Assembler* assm_;

  // Only emit the constant pool once near the end of the instruction stream.
  bool emitted_ = false;

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
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_POOL_RISCV_H_
