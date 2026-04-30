// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_CELL_H_
#define V8_OBJECTS_PROPERTY_CELL_H_

#include "src/objects/dependent-code.h"
#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class FixedArray;
class WeakFixedArray;

#include "torque-generated/src/objects/property-cell-tq.inc"

V8_OBJECT class PropertyCell : public HeapObjectLayout {
 public:
  // [name]: the name of the global property.
  inline Tagged<Name> name() const;
  inline void set_name(Tagged<Name> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [property_details]: details of the global property.
  inline Tagged<Smi> property_details_raw() const;
  inline Tagged<Smi> property_details_raw(AcquireLoadTag tag) const;
  inline void set_property_details_raw(
      Tagged<Smi> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_property_details_raw(
      Tagged<Smi> value, ReleaseStoreTag,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline PropertyDetails property_details() const;
  inline PropertyDetails property_details(AcquireLoadTag tag) const;
  inline void UpdatePropertyDetailsExceptCellType(PropertyDetails details);

  // [value]: value of the global property.
  inline Tagged<Object> value() const;
  inline Tagged<Object> value(AcquireLoadTag) const;
  inline void set_value(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_value(Tagged<Object> value, ReleaseStoreTag,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [dependent_code]: code that depends on the type of the global property.
  inline Tagged<DependentCode> dependent_code() const;
  inline void set_dependent_code(Tagged<DependentCode> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Changes the value and/or property details.
  // For global properties:
  inline void Transition(PropertyDetails new_details,
                         DirectHandle<Object> new_value);
  // For protectors:
  void InvalidateProtector(Isolate* isolate);

  static PropertyCellType InitialType(Isolate* isolate, Tagged<Object> value);

  // Computes the new type of the cell's contents for the given value, but
  // without actually modifying the details.
  static PropertyCellType UpdatedType(Isolate* isolate,
                                      Tagged<PropertyCell> cell,
                                      Tagged<Object> value,
                                      PropertyDetails details);

  // Prepares property cell at given entry for receiving given value and sets
  // that value.  As a result the old cell could be invalidated and/or dependent
  // code could be deoptimized. Returns the (possibly new) property cell.
  static Handle<PropertyCell> PrepareForAndSetValue(
      Isolate* isolate, DirectHandle<GlobalDictionary> dictionary,
      InternalIndex entry, DirectHandle<Object> value, PropertyDetails details);

  void ClearAndInvalidate(Isolate* isolate);
  static Handle<PropertyCell> InvalidateAndReplaceEntry(
      Isolate* isolate, DirectHandle<GlobalDictionary> dictionary,
      InternalIndex entry, PropertyDetails new_details,
      DirectHandle<Object> new_value);

  // Whether or not the {details} and {value} fit together. This is an
  // approximation with false positives.
  static bool CheckDataIsCompatible(PropertyDetails details,
                                    Tagged<Object> value);

  DECL_PRINTER(PropertyCell)
  DECL_VERIFIER(PropertyCell)

 private:
  friend class Factory;

#ifdef DEBUG
  // Whether the property cell can transition to the given state. This is an
  // approximation with false positives.
  bool CanTransitionTo(PropertyDetails new_details,
                       Tagged<Object> new_value) const;
#endif  // DEBUG

 public:
  TaggedMember<Name> name_;
  TaggedMember<Smi> property_details_raw_;
  TaggedMember<Object> value_;
  TaggedMember<DependentCode> dependent_code_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<PropertyCell> {
  using BodyDescriptor =
      FixedBodyDescriptor<offsetof(PropertyCell, name_), sizeof(PropertyCell),
                          sizeof(PropertyCell)>;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_CELL_H_
