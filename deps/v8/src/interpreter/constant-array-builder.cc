// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/constant-array-builder.h"

#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

ConstantArrayBuilder::ConstantArraySlice::ConstantArraySlice(
    Zone* zone, size_t start_index, size_t capacity, OperandSize operand_size)
    : start_index_(start_index),
      capacity_(capacity),
      reserved_(0),
      operand_size_(operand_size),
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
  DCHECK_GE(index, start_index());
  DCHECK_LT(index, start_index() + size());
  return constants_[index - start_index()];
}

STATIC_CONST_MEMBER_DEFINITION const size_t ConstantArrayBuilder::k8BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k16BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k32BitCapacity;

ConstantArrayBuilder::ConstantArrayBuilder(Isolate* isolate, Zone* zone)
    : isolate_(isolate), constants_map_(isolate->heap(), zone) {
  idx_slice_[0] =
      new (zone) ConstantArraySlice(zone, 0, k8BitCapacity, OperandSize::kByte);
  idx_slice_[1] = new (zone) ConstantArraySlice(
      zone, k8BitCapacity, k16BitCapacity, OperandSize::kShort);
  idx_slice_[2] = new (zone) ConstantArraySlice(
      zone, k8BitCapacity + k16BitCapacity, k32BitCapacity, OperandSize::kQuad);
}

size_t ConstantArrayBuilder::size() const {
  size_t i = arraysize(idx_slice_);
  while (i > 0) {
    ConstantArraySlice* slice = idx_slice_[--i];
    if (slice->size() > 0) {
      return slice->start_index() + slice->size();
    }
  }
  return idx_slice_[0]->size();
}

const ConstantArrayBuilder::ConstantArraySlice*
ConstantArrayBuilder::IndexToSlice(size_t index) const {
  for (const ConstantArraySlice* slice : idx_slice_) {
    if (index <= slice->max_index()) {
      return slice;
    }
  }
  UNREACHABLE();
  return nullptr;
}

Handle<Object> ConstantArrayBuilder::At(size_t index) const {
  const ConstantArraySlice* slice = IndexToSlice(index);
  if (index < slice->start_index() + slice->size()) {
    return slice->At(index);
  } else {
    DCHECK_LT(index, slice->capacity());
    return isolate_->factory()->the_hole_value();
  }
}

Handle<FixedArray> ConstantArrayBuilder::ToFixedArray() {
  Handle<FixedArray> fixed_array = isolate_->factory()->NewFixedArray(
      static_cast<int>(size()), PretenureFlag::TENURED);
  int array_index = 0;
  for (const ConstantArraySlice* slice : idx_slice_) {
    if (array_index == fixed_array->length()) {
      break;
    }
    DCHECK(array_index == 0 ||
           base::bits::IsPowerOfTwo32(static_cast<uint32_t>(array_index)));
    // Copy objects from slice into array.
    for (size_t i = 0; i < slice->size(); ++i) {
      fixed_array->set(array_index++, *slice->At(slice->start_index() + i));
    }
    // Insert holes where reservations led to unused slots.
    size_t padding =
        std::min(static_cast<size_t>(fixed_array->length() - array_index),
                 slice->capacity() - slice->size());
    for (size_t i = 0; i < padding; i++) {
      fixed_array->set(array_index++, *isolate_->factory()->the_hole_value());
    }
  }
  DCHECK_EQ(array_index, fixed_array->length());
  constants_map()->Clear();
  return fixed_array;
}

size_t ConstantArrayBuilder::Insert(Handle<Object> object) {
  index_t* entry = constants_map()->Find(object);
  return (entry == nullptr) ? AllocateEntry(object) : *entry;
}

ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateEntry(
    Handle<Object> object) {
  DCHECK(!object->IsOddball());
  index_t* entry = constants_map()->Get(object);
  for (size_t i = 0; i < arraysize(idx_slice_); ++i) {
    if (idx_slice_[i]->available() > 0) {
      size_t index = idx_slice_[i]->Allocate(object);
      *entry = static_cast<index_t>(index);
      return *entry;
      break;
    }
  }
  UNREACHABLE();
  return kMaxUInt32;
}

OperandSize ConstantArrayBuilder::CreateReservedEntry() {
  for (size_t i = 0; i < arraysize(idx_slice_); ++i) {
    if (idx_slice_[i]->available() > 0) {
      idx_slice_[i]->Reserve();
      return idx_slice_[i]->operand_size();
    }
  }
  UNREACHABLE();
  return OperandSize::kNone;
}

ConstantArrayBuilder::ConstantArraySlice*
ConstantArrayBuilder::OperandSizeToSlice(OperandSize operand_size) const {
  ConstantArraySlice* slice = nullptr;
  switch (operand_size) {
    case OperandSize::kNone:
      UNREACHABLE();
      break;
    case OperandSize::kByte:
      slice = idx_slice_[0];
      break;
    case OperandSize::kShort:
      slice = idx_slice_[1];
      break;
    case OperandSize::kQuad:
      slice = idx_slice_[2];
      break;
  }
  DCHECK(slice->operand_size() == operand_size);
  return slice;
}

size_t ConstantArrayBuilder::CommitReservedEntry(OperandSize operand_size,
                                                 Handle<Object> object) {
  DiscardReservedEntry(operand_size);
  size_t index;
  index_t* entry = constants_map()->Find(object);
  if (nullptr == entry) {
    index = AllocateEntry(object);
  } else {
    ConstantArraySlice* slice = OperandSizeToSlice(operand_size);
    if (*entry > slice->max_index()) {
      // The object is already in the constant array, but may have an
      // index too big for the reserved operand_size. So, duplicate
      // entry with the smaller operand size.
      *entry = static_cast<index_t>(slice->Allocate(object));
    }
    index = *entry;
  }
  return index;
}

void ConstantArrayBuilder::DiscardReservedEntry(OperandSize operand_size) {
  OperandSizeToSlice(operand_size)->Unreserve();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
