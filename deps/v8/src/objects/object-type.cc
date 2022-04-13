// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/object-type.h"

#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

Address CheckObjectType(Address raw_value, Address raw_type,
                        Address raw_location) {
#ifdef DEBUG
  Object value(raw_value);
  Smi type(raw_type);
  String location = String::cast(Object(raw_location));
  const char* expected;
  switch (static_cast<ObjectType>(type.value())) {
#define TYPE_CASE(Name)                                 \
  case ObjectType::k##Name:                             \
    if (value.Is##Name()) return Smi::FromInt(0).ptr(); \
    expected = #Name;                                   \
    break;
#define TYPE_STRUCT_CASE(NAME, Name, name)              \
  case ObjectType::k##Name:                             \
    if (value.Is##Name()) return Smi::FromInt(0).ptr(); \
    expected = #Name;                                   \
    break;

    TYPE_CASE(Object)
    TYPE_CASE(Smi)
    TYPE_CASE(TaggedIndex)
    TYPE_CASE(HeapObject)
    OBJECT_TYPE_LIST(TYPE_CASE)
    HEAP_OBJECT_TYPE_LIST(TYPE_CASE)
    STRUCT_LIST(TYPE_STRUCT_CASE)
#undef TYPE_CASE
#undef TYPE_STRUCT_CASE
  }
  std::stringstream value_description;
  value.Print(value_description);
  FATAL(
      "Type cast failed in %s\n"
      "  Expected %s but found %s",
      location.ToAsciiArray(), expected, value_description.str().c_str());
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8
