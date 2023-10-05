// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/field-type.h"

#include "src/handles/handles-inl.h"
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
Handle<FieldType> FieldType::None(Isolate* isolate) {
  return handle(None(), isolate);
}

// static
Handle<FieldType> FieldType::Any(Isolate* isolate) {
  return handle(Any(), isolate);
}

// static
Tagged<FieldType> FieldType::Class(Tagged<Map> map) {
  return FieldType::cast(Tagged<Object>(map));
}

// static
Handle<FieldType> FieldType::Class(Handle<Map> map, Isolate* isolate) {
  return handle(Class(*map), isolate);
}

// static
Tagged<FieldType> FieldType::cast(Tagged<Object> object) {
  DCHECK(object == None() || object == Any() || IsMap(object));
  return Tagged<FieldType>(FieldType(object.ptr()).ptr());
}

bool IsClass(Tagged<FieldType> obj) { return IsMap(obj); }

Tagged<Map> FieldType::AsClass() const {
  DCHECK(IsClass(*this));
  return Map::cast(*this);
}

bool FieldType::NowStable() const {
  return !IsClass(*this) || AsClass()->is_stable();
}

bool FieldType::NowIs(Tagged<FieldType> other) const {
  if (IsAny(other)) return true;
  if (IsNone(*this)) return true;
  if (IsNone(other)) return false;
  if (IsAny(*this)) return false;
  DCHECK(IsClass(*this));
  DCHECK(IsClass(other));
  return *this == other;
}

bool FieldType::Equals(Tagged<FieldType> other) const {
  if (IsAny(*this) && IsAny(other)) return true;
  if (IsNone(*this) && IsNone(other)) return true;
  if (IsClass(*this) && IsClass(other)) {
    return *this == other;
  }
  return false;
}

bool FieldType::NowIs(Handle<FieldType> other) const { return NowIs(*other); }

void FieldType::PrintTo(std::ostream& os) const {
  if (IsAny(*this)) {
    os << "Any";
  } else if (IsNone(*this)) {
    os << "None";
  } else {
    DCHECK(IsClass(*this));
    os << "Class(" << reinterpret_cast<void*>(AsClass().ptr()) << ")";
  }
}

bool FieldType::NowContains(Tagged<Object> value) const {
  if (*this == Any()) return true;
  if (*this == None()) return false;
  if (!IsHeapObject(value)) return false;
  return HeapObject::cast(value)->map() == Map::cast(*this);
}

}  // namespace internal
}  // namespace v8
