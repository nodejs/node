// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_INL_H_
#define V8_OBJECTS_LITERAL_OBJECTS_INL_H_

#include "src/objects/literal-objects.h"

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

//
// ObjectBoilerplateDescription
//

OBJECT_CONSTRUCTORS_IMPL(ObjectBoilerplateDescription, FixedArray)

CAST_ACCESSOR(ObjectBoilerplateDescription)

SMI_ACCESSORS(ObjectBoilerplateDescription, flags,
              FixedArray::OffsetOfElementAt(kLiteralTypeOffset))

Object ObjectBoilerplateDescription::name(int index) const {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return name(isolate, index);
}

Object ObjectBoilerplateDescription::name(Isolate* isolate, int index) const {
  // get() already checks for out of bounds access, but we do not want to allow
  // access to the last element, if it is the number of properties.
  DCHECK_NE(size(), index);
  return get(isolate, 2 * index + kDescriptionStartIndex);
}

Object ObjectBoilerplateDescription::value(int index) const {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return value(isolate, index);
}

Object ObjectBoilerplateDescription::value(Isolate* isolate, int index) const {
  return get(isolate, 2 * index + 1 + kDescriptionStartIndex);
}

void ObjectBoilerplateDescription::set_key_value(int index, Object key,
                                                 Object value) {
  DCHECK_LT(index, size());
  DCHECK_GE(index, 0);
  set(2 * index + kDescriptionStartIndex, key);
  set(2 * index + 1 + kDescriptionStartIndex, value);
}

int ObjectBoilerplateDescription::size() const {
  DCHECK_EQ(0, (length() - kDescriptionStartIndex -
                (this->has_number_of_properties() ? 1 : 0)) %
                   2);
  // Rounding is intended.
  return (length() - kDescriptionStartIndex) / 2;
}

bool ObjectBoilerplateDescription::has_number_of_properties() const {
  return (length() - kDescriptionStartIndex) % 2 != 0;
}

int ObjectBoilerplateDescription::backing_store_size() const {
  if (has_number_of_properties()) {
    // If present, the last entry contains the number of properties.
    return Smi::ToInt(this->get(length() - 1));
  }
  // If the number is not given explicitly, we assume there are no
  // properties with computed names.
  return size();
}

void ObjectBoilerplateDescription::set_backing_store_size(
    int backing_store_size) {
  DCHECK(has_number_of_properties());
  DCHECK_NE(size(), backing_store_size);
  CHECK(Smi::IsValid(backing_store_size));
  // TODO(ishell): move this value to the header
  set(length() - 1, Smi::FromInt(backing_store_size));
}

//
// ClassBoilerplate
//

OBJECT_CONSTRUCTORS_IMPL(ClassBoilerplate, FixedArray)
CAST_ACCESSOR(ClassBoilerplate)

BIT_FIELD_ACCESSORS(ClassBoilerplate, flags, install_class_name_accessor,
                    ClassBoilerplate::Flags::InstallClassNameAccessorBit)

BIT_FIELD_ACCESSORS(ClassBoilerplate, flags, arguments_count,
                    ClassBoilerplate::Flags::ArgumentsCountBits)

SMI_ACCESSORS(ClassBoilerplate, flags,
              FixedArray::OffsetOfElementAt(kFlagsIndex))

ACCESSORS(ClassBoilerplate, static_properties_template, Object,
          FixedArray::OffsetOfElementAt(kClassPropertiesTemplateIndex))

ACCESSORS(ClassBoilerplate, static_elements_template, Object,
          FixedArray::OffsetOfElementAt(kClassElementsTemplateIndex))

ACCESSORS(ClassBoilerplate, static_computed_properties, FixedArray,
          FixedArray::OffsetOfElementAt(kClassComputedPropertiesIndex))

ACCESSORS(ClassBoilerplate, instance_properties_template, Object,
          FixedArray::OffsetOfElementAt(kPrototypePropertiesTemplateIndex))

ACCESSORS(ClassBoilerplate, instance_elements_template, Object,
          FixedArray::OffsetOfElementAt(kPrototypeElementsTemplateIndex))

ACCESSORS(ClassBoilerplate, instance_computed_properties, FixedArray,
          FixedArray::OffsetOfElementAt(kPrototypeComputedPropertiesIndex))

//
// ArrayBoilerplateDescription
//

OBJECT_CONSTRUCTORS_IMPL(ArrayBoilerplateDescription, Struct)

CAST_ACCESSOR(ArrayBoilerplateDescription)

SMI_ACCESSORS(ArrayBoilerplateDescription, flags, kFlagsOffset)

ACCESSORS(ArrayBoilerplateDescription, constant_elements, FixedArrayBase,
          kConstantElementsOffset)

ElementsKind ArrayBoilerplateDescription::elements_kind() const {
  return static_cast<ElementsKind>(flags());
}

void ArrayBoilerplateDescription::set_elements_kind(ElementsKind kind) {
  set_flags(kind);
}

bool ArrayBoilerplateDescription::is_empty() const {
  return constant_elements().length() == 0;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_INL_H_
