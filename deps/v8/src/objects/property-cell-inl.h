// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_CELL_INL_H_
#define V8_OBJECTS_PROPERTY_CELL_INL_H_

#include "src/objects/property-cell.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/dependent-code-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/property-cell-tq-inl.inc"

Tagged<DependentCode> PropertyCell::dependent_code() const {
  return dependent_code_.load();
}
void PropertyCell::set_dependent_code(Tagged<DependentCode> value,
                                      WriteBarrierMode mode) {
  dependent_code_.store(this, value, mode);
}

Tagged<Name> PropertyCell::name() const { return name_.load(); }
void PropertyCell::set_name(Tagged<Name> value, WriteBarrierMode mode) {
  name_.store(this, value, mode);
}

Tagged<Smi> PropertyCell::property_details_raw() const {
  return property_details_raw_.load();
}
void PropertyCell::set_property_details_raw(Tagged<Smi> value,
                                            WriteBarrierMode mode) {
  property_details_raw_.store(this, value, mode);
}

Tagged<Smi> PropertyCell::property_details_raw(AcquireLoadTag tag) const {
  return property_details_raw_.Acquire_Load();
}
void PropertyCell::set_property_details_raw(Tagged<Smi> value,
                                            ReleaseStoreTag tag,
                                            WriteBarrierMode mode) {
  property_details_raw_.Release_Store(this, value, mode);
}

Tagged<Object> PropertyCell::value() const { return value_.load(); }
void PropertyCell::set_value(Tagged<Object> value, WriteBarrierMode mode) {
  value_.store(this, value, mode);
}

Tagged<Object> PropertyCell::value(AcquireLoadTag tag) const {
  return value_.Acquire_Load();
}
void PropertyCell::set_value(Tagged<Object> value, ReleaseStoreTag tag,
                             WriteBarrierMode mode) {
  value_.Release_Store(this, value, mode);
}

PropertyDetails PropertyCell::property_details() const {
  return PropertyDetails(Cast<Smi>(property_details_raw()));
}

PropertyDetails PropertyCell::property_details(AcquireLoadTag tag) const {
  return PropertyDetails(Cast<Smi>(property_details_raw(tag)));
}

void PropertyCell::UpdatePropertyDetailsExceptCellType(
    PropertyDetails details) {
  DCHECK(CheckDataIsCompatible(details, value()));
  PropertyDetails old_details = property_details();
  CHECK_EQ(old_details.cell_type(), details.cell_type());
  set_property_details_raw(details.AsSmi(), kReleaseStore);
  // Deopt when making a writable property read-only. The reverse direction
  // is uninteresting because Turbofan does not currently rely on read-only
  // unless the property is also configurable, in which case it will stay
  // read-only forever.
  if (!old_details.IsReadOnly() && details.IsReadOnly()) {
    // TODO(11527): pass Isolate as an argument.
    Isolate* isolate = Isolate::Current();
    DependentCode::DeoptimizeDependencyGroups(
        isolate, Tagged<PropertyCell>(this),
        DependentCode::kPropertyCellChangedGroup);
  }
}

void PropertyCell::Transition(PropertyDetails new_details,
                              DirectHandle<Object> new_value) {
  DCHECK(CanTransitionTo(new_details, *new_value));
  // This code must be in sync with its counterpart in
  // PropertyCellData::Serialize.
  PropertyDetails transition_marker = new_details;
  transition_marker.set_cell_type(PropertyCellType::kInTransition);
  set_property_details_raw(transition_marker.AsSmi(), kReleaseStore);
  set_value(*new_value, kReleaseStore);
  set_property_details_raw(new_details.AsSmi(), kReleaseStore);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_CELL_INL_H_
