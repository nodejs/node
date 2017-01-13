// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-types.h"

#include "src/field-type.h"
#include "src/handles-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

// static
HType HType::FromType(Type* type) {
  if (Type::Any()->Is(type)) return HType::Any();
  if (!type->IsInhabited()) return HType::None();
  if (type->Is(Type::SignedSmall())) return HType::Smi();
  if (type->Is(Type::Number())) return HType::TaggedNumber();
  if (type->Is(Type::Null())) return HType::Null();
  if (type->Is(Type::String())) return HType::String();
  if (type->Is(Type::Boolean())) return HType::Boolean();
  if (type->Is(Type::Undefined())) return HType::Undefined();
  if (type->Is(Type::Object())) return HType::JSObject();
  if (type->Is(Type::DetectableReceiver())) return HType::JSReceiver();
  return HType::Tagged();
}


// static
HType HType::FromFieldType(Handle<FieldType> type, Zone* temp_zone) {
  return FromType(type->Convert(temp_zone));
}

// static
HType HType::FromValue(Handle<Object> value) {
  Object* raw_value = *value;
  if (raw_value->IsSmi()) return HType::Smi();
  DCHECK(raw_value->IsHeapObject());
  Isolate* isolate = HeapObject::cast(*value)->GetIsolate();
  if (raw_value->IsNull(isolate)) return HType::Null();
  if (raw_value->IsHeapNumber()) {
    double n = Handle<v8::internal::HeapNumber>::cast(value)->value();
    return IsSmiDouble(n) ? HType::Smi() : HType::HeapNumber();
  }
  if (raw_value->IsString()) return HType::String();
  if (raw_value->IsBoolean()) return HType::Boolean();
  if (raw_value->IsUndefined(isolate)) return HType::Undefined();
  if (raw_value->IsJSArray()) {
    DCHECK(!raw_value->IsUndetectable());
    return HType::JSArray();
  }
  if (raw_value->IsJSObject() && !raw_value->IsUndetectable()) {
    return HType::JSObject();
  }
  return HType::HeapObject();
}


std::ostream& operator<<(std::ostream& os, const HType& t) {
  // Note: The c1visualizer syntax for locals allows only a sequence of the
  // following characters: A-Za-z0-9_-|:
  switch (t.kind_) {
#define DEFINE_CASE(Name, mask) \
  case HType::k##Name:          \
    return os << #Name;
    HTYPE_LIST(DEFINE_CASE)
#undef DEFINE_CASE
  }
  UNREACHABLE();
  return os;
}

}  // namespace internal
}  // namespace v8
