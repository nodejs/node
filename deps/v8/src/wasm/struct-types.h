// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_STRUCT_TYPES_H_
#define V8_WASM_STRUCT_TYPES_H_

#include "src/base/iterator.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/wasm/value-type.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

class StructType : public ZoneObject {
 public:
  StructType(uint32_t field_count, uint32_t* field_offsets,
             const ValueType* reps, const bool* mutabilities)
      : field_count_(field_count),
        field_offsets_(field_offsets),
        reps_(reps),
        mutabilities_(mutabilities) {
    InitializeOffsets();
  }

  uint32_t field_count() const { return field_count_; }

  ValueType field(uint32_t index) const {
    DCHECK_LT(index, field_count_);
    return reps_[index];
  }

  bool mutability(uint32_t index) const {
    DCHECK_LT(index, field_count_);
    return mutabilities_[index];
  }

  // Iteration support.
  base::iterator_range<const ValueType*> fields() const {
    return {reps_, reps_ + field_count_};
  }
  base::iterator_range<const bool*> mutabilities() const {
    return {mutabilities_, mutabilities_ + field_count_};
  }

  bool operator==(const StructType& other) const {
    if (this == &other) return true;
    if (field_count() != other.field_count()) return false;
    return std::equal(fields().begin(), fields().end(),
                      other.fields().begin()) &&
           std::equal(mutabilities().begin(), mutabilities().end(),
                      other.mutabilities().begin());
  }
  bool operator!=(const StructType& other) const { return !(*this == other); }

  uint32_t field_offset(uint32_t index) const {
    DCHECK_LT(index, field_count());
    if (index == 0) return 0;
    return field_offsets_[index - 1];
  }
  uint32_t total_fields_size() const {
    return field_count() == 0 ? 0 : field_offsets_[field_count() - 1];
  }

  void InitializeOffsets() {
    if (field_count() == 0) return;
    uint32_t offset = field(0).value_kind_size();
    for (uint32_t i = 1; i < field_count(); i++) {
      uint32_t field_size = field(i).value_kind_size();
      // TODO(jkummerow): Don't round up to more than kTaggedSize-alignment.
      offset = RoundUp(offset, field_size);
      field_offsets_[i - 1] = offset;
      offset += field_size;
    }
    offset = RoundUp(offset, kTaggedSize);
    field_offsets_[field_count() - 1] = offset;
  }

  // For incrementally building StructTypes.
  class Builder {
   public:
    Builder(Zone* zone, uint32_t field_count)
        : field_count_(field_count),
          zone_(zone),
          cursor_(0),
          buffer_(zone->NewArray<ValueType>(static_cast<int>(field_count))),
          mutabilities_(zone->NewArray<bool>(static_cast<int>(field_count))) {}

    void AddField(ValueType type, bool mutability) {
      DCHECK_LT(cursor_, field_count_);
      mutabilities_[cursor_] = mutability;
      buffer_[cursor_++] = type;
    }

    StructType* Build() {
      DCHECK_EQ(cursor_, field_count_);
      uint32_t* offsets = zone_->NewArray<uint32_t>(field_count_);
      return zone_->New<StructType>(field_count_, offsets, buffer_,
                                    mutabilities_);
    }

   private:
    const uint32_t field_count_;
    Zone* const zone_;
    uint32_t cursor_;
    ValueType* const buffer_;
    bool* const mutabilities_;
  };

  static const size_t kMaxFieldOffset =
      (kV8MaxWasmStructFields - 1) * kMaxValueTypeSize;

 private:
  const uint32_t field_count_;
  uint32_t* const field_offsets_;
  const ValueType* const reps_;
  const bool* const mutabilities_;
};

class ArrayType : public ZoneObject {
 public:
  constexpr explicit ArrayType(ValueType rep, bool mutability)
      : rep_(rep), mutability_(mutability) {}

  ValueType element_type() const { return rep_; }
  bool mutability() const { return mutability_; }

  bool operator==(const ArrayType& other) const {
    return rep_ == other.rep_ && mutability_ == other.mutability_;
  }
  bool operator!=(const ArrayType& other) const {
    return rep_ != other.rep_ || mutability_ != other.mutability_;
  }

 private:
  const ValueType rep_;
  const bool mutability_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STRUCT_TYPES_H_
