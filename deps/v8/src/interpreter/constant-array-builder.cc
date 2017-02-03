// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/constant-array-builder.h"

#include <functional>
#include <set>

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

void ConstantArrayBuilder::ConstantArraySlice::InsertAt(size_t index,
                                                        Handle<Object> object) {
  DCHECK_GE(index, start_index());
  DCHECK_LT(index, start_index() + size());
  constants_[index - start_index()] = object;
}

bool ConstantArrayBuilder::ConstantArraySlice::AllElementsAreUnique() const {
  std::set<Object*> elements;
  for (auto constant : constants_) {
    if (elements.find(*constant) != elements.end()) return false;
    elements.insert(*constant);
  }
  return true;
}

STATIC_CONST_MEMBER_DEFINITION const size_t ConstantArrayBuilder::k8BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k16BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k32BitCapacity;

ConstantArrayBuilder::ConstantArrayBuilder(Zone* zone,
                                           Handle<Object> the_hole_value)
    : constants_map_(16, base::KeyEqualityMatcher<Address>(),
                     ZoneAllocationPolicy(zone)),
      smi_map_(zone),
      smi_pairs_(zone),
      zone_(zone),
      the_hole_value_(the_hole_value) {
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

ConstantArrayBuilder::ConstantArraySlice* ConstantArrayBuilder::IndexToSlice(
    size_t index) const {
  for (ConstantArraySlice* slice : idx_slice_) {
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
    return the_hole_value();
  }
}

Handle<FixedArray> ConstantArrayBuilder::ToFixedArray(Isolate* isolate) {
  // First insert reserved SMI values.
  for (auto reserved_smi : smi_pairs_) {
    InsertAllocatedEntry(reserved_smi.second,
                         handle(reserved_smi.first, isolate));
  }

  Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(
      static_cast<int>(size()), PretenureFlag::TENURED);
  int array_index = 0;
  for (const ConstantArraySlice* slice : idx_slice_) {
    if (array_index == fixed_array->length()) {
      break;
    }
    DCHECK(array_index == 0 ||
           base::bits::IsPowerOfTwo32(static_cast<uint32_t>(array_index)));
    // Different slices might contain the same element due to reservations, but
    // all elements within a slice should be unique. If this DCHECK fails, then
    // the AST nodes are not being internalized within a CanonicalHandleScope.
    DCHECK(slice->AllElementsAreUnique());
    // Copy objects from slice into array.
    for (size_t i = 0; i < slice->size(); ++i) {
      fixed_array->set(array_index++, *slice->At(slice->start_index() + i));
    }
    // Insert holes where reservations led to unused slots.
    size_t padding =
        std::min(static_cast<size_t>(fixed_array->length() - array_index),
                 slice->capacity() - slice->size());
    for (size_t i = 0; i < padding; i++) {
      fixed_array->set(array_index++, *the_hole_value());
    }
  }
  DCHECK_EQ(array_index, fixed_array->length());
  return fixed_array;
}

size_t ConstantArrayBuilder::Insert(Handle<Object> object) {
  return constants_map_
      .LookupOrInsert(object.address(), ObjectHash(object.address()),
                      [&]() { return AllocateIndex(object); },
                      ZoneAllocationPolicy(zone_))
      ->value;
}

ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateIndex(
    Handle<Object> object) {
  for (size_t i = 0; i < arraysize(idx_slice_); ++i) {
    if (idx_slice_[i]->available() > 0) {
      return static_cast<index_t>(idx_slice_[i]->Allocate(object));
    }
  }
  UNREACHABLE();
  return kMaxUInt32;
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

size_t ConstantArrayBuilder::AllocateEntry() {
  return AllocateIndex(the_hole_value());
}

void ConstantArrayBuilder::InsertAllocatedEntry(size_t index,
                                                Handle<Object> object) {
  DCHECK_EQ(the_hole_value().address(), At(index).address());
  ConstantArraySlice* slice = IndexToSlice(index);
  slice->InsertAt(index, object);
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

ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateReservedEntry(
    Smi* value) {
  index_t index = static_cast<index_t>(AllocateEntry());
  smi_map_[value] = index;
  smi_pairs_.push_back(std::make_pair(value, index));
  return index;
}

size_t ConstantArrayBuilder::CommitReservedEntry(OperandSize operand_size,
                                                 Smi* value) {
  DiscardReservedEntry(operand_size);
  size_t index;
  auto entry = smi_map_.find(value);
  if (entry == smi_map_.end()) {
    index = AllocateReservedEntry(value);
  } else {
    ConstantArraySlice* slice = OperandSizeToSlice(operand_size);
    index = entry->second;
    if (index > slice->max_index()) {
      // The object is already in the constant array, but may have an
      // index too big for the reserved operand_size. So, duplicate
      // entry with the smaller operand size.
      index = AllocateReservedEntry(value);
    }
    DCHECK_LE(index, slice->max_index());
  }
  return index;
}

void ConstantArrayBuilder::DiscardReservedEntry(OperandSize operand_size) {
  OperandSizeToSlice(operand_size)->Unreserve();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
