// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_INL_H_
#define V8_OBJECTS_LITERAL_OBJECTS_INL_H_

#include "src/objects-inl.h"
#include "src/objects/literal-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(ClassBoilerplate)

BIT_FIELD_ACCESSORS(ClassBoilerplate, flags, install_class_name_accessor,
                    ClassBoilerplate::Flags::InstallClassNameAccessorBit)

BIT_FIELD_ACCESSORS(ClassBoilerplate, flags, arguments_count,
                    ClassBoilerplate::Flags::ArgumentsCountBits)

SMI_ACCESSORS(ClassBoilerplate, flags,
              FixedArray::OffsetOfElementAt(kFlagsIndex));

ACCESSORS(ClassBoilerplate, static_properties_template, Object,
          FixedArray::OffsetOfElementAt(kClassPropertiesTemplateIndex));

ACCESSORS(ClassBoilerplate, static_elements_template, Object,
          FixedArray::OffsetOfElementAt(kClassElementsTemplateIndex));

ACCESSORS(ClassBoilerplate, static_computed_properties, FixedArray,
          FixedArray::OffsetOfElementAt(kClassComputedPropertiesIndex));

ACCESSORS(ClassBoilerplate, instance_properties_template, Object,
          FixedArray::OffsetOfElementAt(kPrototypePropertiesTemplateIndex));

ACCESSORS(ClassBoilerplate, instance_elements_template, Object,
          FixedArray::OffsetOfElementAt(kPrototypeElementsTemplateIndex));

ACCESSORS(ClassBoilerplate, instance_computed_properties, FixedArray,
          FixedArray::OffsetOfElementAt(kPrototypeComputedPropertiesIndex));

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_INL_H_
