// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECT_TYPE_H_
#define V8_OBJECTS_OBJECT_TYPE_H_

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/objects-definitions.h"

namespace v8 {
namespace internal {

#define ENUM_ELEMENT(Name) k##Name,
#define ENUM_STRUCT_ELEMENT(NAME, Name, name) k##Name,
enum class ObjectType {
  ENUM_ELEMENT(Object)                 //
  ENUM_ELEMENT(Smi)                    //
  ENUM_ELEMENT(TaggedIndex)            //
  ENUM_ELEMENT(HeapObject)             //
  OBJECT_TYPE_LIST(ENUM_ELEMENT)       //
  HEAP_OBJECT_TYPE_LIST(ENUM_ELEMENT)  //
  STRUCT_LIST(ENUM_STRUCT_ELEMENT)     //
};
#undef ENUM_ELEMENT
#undef ENUM_STRUCT_ELEMENT

// {raw_value} must be a tagged Object.
// {raw_type} must be a tagged Smi.
// {raw_location} must be a tagged String.
// Returns a tagged Smi.
V8_EXPORT_PRIVATE Address CheckObjectType(Address raw_value, Address raw_type,
                                          Address raw_location);

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECT_TYPE_H_
