// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_CELL_H_
#define V8_OBJECTS_PROPERTY_CELL_H_

#include "src/objects/heap-object.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class PropertyCell : public HeapObject {
 public:
  // [name]: the name of the global property.
  DECL_ACCESSORS(name, Name)
  // [property_details]: details of the global property.
  DECL_ACCESSORS(property_details_raw, Smi)
  // [value]: value of the global property.
  DECL_ACCESSORS(value, Object)
  // [dependent_code]: dependent code that depends on the type of the global
  // property.
  DECL_ACCESSORS(dependent_code, DependentCode)

  inline PropertyDetails property_details() const;
  inline void set_property_details(PropertyDetails details);

  PropertyCellConstantType GetConstantType();

  // Computes the new type of the cell's contents for the given value, but
  // without actually modifying the details.
  static PropertyCellType UpdatedType(Isolate* isolate,
                                      Handle<PropertyCell> cell,
                                      Handle<Object> value,
                                      PropertyDetails details);
  // Prepares property cell at given entry for receiving given value.
  // As a result the old cell could be invalidated and/or dependent code could
  // be deoptimized. Returns the prepared property cell.
  static Handle<PropertyCell> PrepareForValue(
      Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry,
      Handle<Object> value, PropertyDetails details);

  static Handle<PropertyCell> InvalidateEntry(
      Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry);

  static void SetValueWithInvalidation(Isolate* isolate, const char* cell_name,
                                       Handle<PropertyCell> cell,
                                       Handle<Object> new_value);

  DECL_CAST(PropertyCell)

  // Dispatched behavior.
  DECL_PRINTER(PropertyCell)
  DECL_VERIFIER(PropertyCell)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_PROPERTY_CELL_FIELDS)

  using BodyDescriptor = FixedBodyDescriptor<kNameOffset, kSize, kSize>;

  OBJECT_CONSTRUCTORS(PropertyCell, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_CELL_H_
