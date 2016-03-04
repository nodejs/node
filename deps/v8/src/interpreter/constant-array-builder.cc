// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/constant-array-builder.h"

#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

ConstantArrayBuilder::ConstantArraySlice::ConstantArraySlice(Zone* zone,
                                                             size_t start_index,
                                                             size_t capacity)
    : start_index_(start_index),
      capacity_(capacity),
      reserved_(0),
      constants_(zone) {}


void ConstantArrayBuilder::ConstantArraySlice::Reserve() {
  DCHECK_GT(available(), 0u);
  reserved_++;
  DCHECK_LE(reserved_, capacity() - constants_.size());
}


void ConstantArrayBuilder::ConstantArraySlice::Unreserve() {
  DCHECK_GT(reserved_, 0u);
  reserved_--;
}


size_t ConstantArrayBuilder::ConstantArraySlice::Allocate(
    Handle<Object> object) {
  DCHECK_GT(available(), 0u);
  size_t index = constants_.size();
  DCHECK_LT(index, capacity());
  constants_.push_back(object);
  return index + start_index();
}


Handle<Object> ConstantArrayBuilder::ConstantArraySlice::At(
    size_t index) const {
  return constants_[index - start_index()];
}


STATIC_CONST_MEMBER_DEFINITION const size_t ConstantArrayBuilder::kMaxCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t ConstantArrayBuilder::kLowCapacity;


ConstantArrayBuilder::ConstantArrayBuilder(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      idx8_slice_(zone, 0, kLowCapacity),
      idx16_slice_(zone, kLowCapacity, kHighCapacity),
      constants_map_(isolate->heap(), zone) {
  STATIC_ASSERT(kMaxCapacity == static_cast<size_t>(kMaxUInt16 + 1));
  DCHECK_EQ(idx8_slice_.start_index(), 0u);
  DCHECK_EQ(idx8_slice_.capacity(), kLowCapacity);
  DCHECK_EQ(idx16_slice_.start_index(), kLowCapacity);
  DCHECK_EQ(idx16_slice_.capacity(), kMaxCapacity - kLowCapacity);
}


size_t ConstantArrayBuilder::size() const {
  if (idx16_slice_.size() > 0) {
    return idx16_slice_.start_index() + idx16_slice_.size();
  } else {
    return idx8_slice_.size();
  }
}


Handle<Object> ConstantArrayBuilder::At(size_t index) const {
  if (index >= idx16_slice_.start_index()) {
    return idx16_slice_.At(index);
  } else if (index < idx8_slice_.size()) {
    return idx8_slice_.At(index);
  } else {
    return isolate_->factory()->the_hole_value();
  }
}


Handle<FixedArray> ConstantArrayBuilder::ToFixedArray(Factory* factory) const {
  Handle<FixedArray> fixed_array =
      factory->NewFixedArray(static_cast<int>(size()), PretenureFlag::TENURED);
  for (int i = 0; i < fixed_array->length(); i++) {
    fixed_array->set(i, *At(static_cast<size_t>(i)));
  }
  return fixed_array;
}


size_t ConstantArrayBuilder::Insert(Handle<Object> object) {
  index_t* entry = constants_map_.Find(object);
  return (entry == nullptr) ? AllocateEntry(object) : *entry;
}


ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateEntry(
    Handle<Object> object) {
  DCHECK(!object->IsOddball());
  size_t index;
  index_t* entry = constants_map_.Get(object);
  if (idx8_slice_.available() > 0) {
    index = idx8_slice_.Allocate(object);
  } else {
    index = idx16_slice_.Allocate(object);
  }
  CHECK_LT(index, kMaxCapacity);
  *entry = static_cast<index_t>(index);
  return *entry;
}


OperandSize ConstantArrayBuilder::CreateReservedEntry() {
  if (idx8_slice_.available() > 0) {
    idx8_slice_.Reserve();
    return OperandSize::kByte;
  } else if (idx16_slice_.available() > 0) {
    idx16_slice_.Reserve();
    return OperandSize::kShort;
  } else {
    UNREACHABLE();
    return OperandSize::kNone;
  }
}


size_t ConstantArrayBuilder::CommitReservedEntry(OperandSize operand_size,
                                                 Handle<Object> object) {
  DiscardReservedEntry(operand_size);
  size_t index;
  index_t* entry = constants_map_.Find(object);
  if (nullptr == entry) {
    index = AllocateEntry(object);
  } else {
    if (operand_size == OperandSize::kByte &&
        *entry >= idx8_slice_.capacity()) {
      // The object is already in the constant array, but has an index
      // outside the range of an idx8 operand so we need to create a
      // duplicate entry in the idx8 operand range to satisfy the
      // commitment.
      *entry = static_cast<index_t>(idx8_slice_.Allocate(object));
    }
    index = *entry;
  }
  DCHECK(operand_size == OperandSize::kShort || index < idx8_slice_.capacity());
  DCHECK_LT(index, kMaxCapacity);
  return index;
}


void ConstantArrayBuilder::DiscardReservedEntry(OperandSize operand_size) {
  switch (operand_size) {
    case OperandSize::kByte:
      idx8_slice_.Unreserve();
      return;
    case OperandSize::kShort:
      idx16_slice_.Unreserve();
      return;
    default:
      UNREACHABLE();
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
