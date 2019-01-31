// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONSTANT_POOL_H_
#define V8_CONSTANT_POOL_H_

#include <map>

#include "src/double.h"
#include "src/globals.h"
#include "src/label.h"
#include "src/reloc-info.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Constant pool support

class ConstantPoolEntry {
 public:
  ConstantPoolEntry() = default;
  ConstantPoolEntry(int position, intptr_t value, bool sharing_ok,
                    RelocInfo::Mode rmode = RelocInfo::NONE)
      : position_(position),
        merged_index_(sharing_ok ? SHARING_ALLOWED : SHARING_PROHIBITED),
        value_(value),
        rmode_(rmode) {}
  ConstantPoolEntry(int position, Double value,
                    RelocInfo::Mode rmode = RelocInfo::NONE)
      : position_(position),
        merged_index_(SHARING_ALLOWED),
        value64_(value.AsUint64()),
        rmode_(rmode) {}

  int position() const { return position_; }
  bool sharing_ok() const { return merged_index_ != SHARING_PROHIBITED; }
  bool is_merged() const { return merged_index_ >= 0; }
  int merged_index() const {
    DCHECK(is_merged());
    return merged_index_;
  }
  void set_merged_index(int index) {
    DCHECK(sharing_ok());
    merged_index_ = index;
    DCHECK(is_merged());
  }
  int offset() const {
    DCHECK_GE(merged_index_, 0);
    return merged_index_;
  }
  void set_offset(int offset) {
    DCHECK_GE(offset, 0);
    merged_index_ = offset;
  }
  intptr_t value() const { return value_; }
  uint64_t value64() const { return value64_; }
  RelocInfo::Mode rmode() const { return rmode_; }

  enum Type { INTPTR, DOUBLE, NUMBER_OF_TYPES };

  static int size(Type type) {
    return (type == INTPTR) ? kSystemPointerSize : kDoubleSize;
  }

  enum Access { REGULAR, OVERFLOWED };

 private:
  int position_;
  int merged_index_;
  union {
    intptr_t value_;
    uint64_t value64_;
  };
  // TODO(leszeks): The way we use this, it could probably be packed into
  // merged_index_ if size is a concern.
  RelocInfo::Mode rmode_;
  enum { SHARING_PROHIBITED = -2, SHARING_ALLOWED = -1 };
};

#if defined(V8_TARGET_ARCH_PPC)

// -----------------------------------------------------------------------------
// Embedded constant pool support

class ConstantPoolBuilder {
 public:
  ConstantPoolBuilder(int ptr_reach_bits, int double_reach_bits);

  // Add pointer-sized constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, intptr_t value,
                                     bool sharing_ok) {
    ConstantPoolEntry entry(position, value, sharing_ok);
    return AddEntry(entry, ConstantPoolEntry::INTPTR);
  }

  // Add double constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, Double value) {
    ConstantPoolEntry entry(position, value);
    return AddEntry(entry, ConstantPoolEntry::DOUBLE);
  }

  // Add double constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, double value) {
    return AddEntry(position, Double(value));
  }

  // Previews the access type required for the next new entry to be added.
  ConstantPoolEntry::Access NextAccess(ConstantPoolEntry::Type type) const;

  bool IsEmpty() {
    return info_[ConstantPoolEntry::INTPTR].entries.empty() &&
           info_[ConstantPoolEntry::INTPTR].shared_entries.empty() &&
           info_[ConstantPoolEntry::DOUBLE].entries.empty() &&
           info_[ConstantPoolEntry::DOUBLE].shared_entries.empty();
  }

  // Emit the constant pool.  Invoke only after all entries have been
  // added and all instructions have been emitted.
  // Returns position of the emitted pool (zero implies no constant pool).
  int Emit(Assembler* assm);

  // Returns the label associated with the start of the constant pool.
  // Linking to this label in the function prologue may provide an
  // efficient means of constant pool pointer register initialization
  // on some architectures.
  inline Label* EmittedPosition() { return &emitted_label_; }

 private:
  ConstantPoolEntry::Access AddEntry(ConstantPoolEntry& entry,
                                     ConstantPoolEntry::Type type);
  void EmitSharedEntries(Assembler* assm, ConstantPoolEntry::Type type);
  void EmitGroup(Assembler* assm, ConstantPoolEntry::Access access,
                 ConstantPoolEntry::Type type);

  struct PerTypeEntryInfo {
    PerTypeEntryInfo() : regular_count(0), overflow_start(-1) {}
    bool overflow() const {
      return (overflow_start >= 0 &&
              overflow_start < static_cast<int>(entries.size()));
    }
    int regular_reach_bits;
    int regular_count;
    int overflow_start;
    std::vector<ConstantPoolEntry> entries;
    std::vector<ConstantPoolEntry> shared_entries;
  };

  Label emitted_label_;  // Records pc_offset of emitted pool
  PerTypeEntryInfo info_[ConstantPoolEntry::NUMBER_OF_TYPES];
};

#endif  // defined(V8_TARGET_ARCH_PPC)

}  // namespace internal
}  // namespace v8

#endif  // V8_CONSTANT_POOL_H_
