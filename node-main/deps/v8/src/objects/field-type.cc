// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/field-type.h"

#include "src/handles/handles-inl.h"
#include "src/objects/map.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// static
Tagged<FieldType> FieldType::None() {
  return Tagged<FieldType>(Smi::FromInt(2).ptr());
}

// static
Tagged<FieldType> FieldType::Any() {
  return Tagged<FieldType>(Smi::FromInt(1).ptr());
}

// static
DirectHandle<FieldType> FieldType::None(Isolate* isolate) {
  return direct_handle(None(), isolate);
}

// static
DirectHandle<FieldType> FieldType::Any(Isolate* isolate) {
  return direct_handle(Any(), isolate);
}

// static
Tagged<FieldType> FieldType::Class(Tagged<Map> map) {
  return Cast<FieldType>(Tagged<Object>(map));
}

// static
DirectHandle<FieldType> FieldType::Class(DirectHandle<Map> map,
                                         Isolate* isolate) {
  return direct_handle(Class(*map), isolate);
}

// static
bool IsClass(Tagged<FieldType> obj) { return IsMap(obj); }

// static
Tagged<Map> FieldType::AsClass(Tagged<FieldType> type) {
  DCHECK(IsClass(type));
  return Cast<Map>(type);
}

// static
DirectHandle<Map> FieldType::AsClass(DirectHandle<FieldType> type) {
  DCHECK(IsClass(*type));
  return Cast<Map>(type);
}

// static
bool FieldType::NowStable(Tagged<FieldType> type) {
  return !IsClass(type) || AsClass(type)->is_stable();
}

// static
bool FieldType::NowIs(Tagged<FieldType> type, Tagged<FieldType> other) {
  if (IsAny(other)) return true;
  if (IsNone(type)) return true;
  if (IsNone(other)) return false;
  if (IsAny(type)) return false;
  DCHECK(IsClass(type));
  DCHECK(IsClass(other));
  return type == other;
}

// static
bool FieldType::Equals(Tagged<FieldType> type, Tagged<FieldType> other) {
  if (IsAny(type) && IsAny(other)) return true;
  if (IsNone(type) && IsNone(other)) return true;
  if (IsClass(type) && IsClass(other)) {
    return type == other;
  }
  return false;
}

// static
bool FieldType::NowIs(Tagged<FieldType> type, DirectHandle<FieldType> other) {
  return NowIs(type, *other);
}

// static
void FieldType::PrintTo(Tagged<FieldType> type, std::ostream& os) {
  if (IsAny(type)) {
    os << "Any";
  } else if (IsNone(type)) {
    os << "None";
  } else {
    DCHECK(IsClass(type));
    os << "Class(" << reinterpret_cast<void*>(AsClass(type).ptr()) << ")";
  }
}

// static
bool FieldType::NowContains(Tagged<FieldType> type, Tagged<Object> value) {
  if (type == Any()) return true;
  if (type == None()) return false;
  if (!IsHeapObject(value)) return false;
  return Cast<HeapObject>(value)->map() == Cast<Map>(type);
}

}  // namespace internal
}  // namespace v8
