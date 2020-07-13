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
FieldType FieldType::None() { return FieldType(Smi::FromInt(2).ptr()); }

// static
FieldType FieldType::Any() { return FieldType(Smi::FromInt(1).ptr()); }

// static
Handle<FieldType> FieldType::None(Isolate* isolate) {
  return handle(None(), isolate);
}

// static
Handle<FieldType> FieldType::Any(Isolate* isolate) {
  return handle(Any(), isolate);
}

// static
FieldType FieldType::Class(Map map) { return FieldType::cast(map); }

// static
Handle<FieldType> FieldType::Class(Handle<Map> map, Isolate* isolate) {
  return handle(Class(*map), isolate);
}

// static
FieldType FieldType::cast(Object object) {
  DCHECK(object == None() || object == Any() || object.IsMap());
  return FieldType(object.ptr());
}

bool FieldType::IsClass() const { return this->IsMap(); }

Map FieldType::AsClass() const {
  DCHECK(IsClass());
  return Map::cast(*this);
}

bool FieldType::NowStable() const {
  return !this->IsClass() || AsClass().is_stable();
}

bool FieldType::NowIs(FieldType other) const {
  if (other.IsAny()) return true;
  if (IsNone()) return true;
  if (other.IsNone()) return false;
  if (IsAny()) return false;
  DCHECK(IsClass());
  DCHECK(other.IsClass());
  return *this == other;
}

bool FieldType::Equals(FieldType other) const {
  if (IsAny() && other.IsAny()) return true;
  if (IsNone() && other.IsNone()) return true;
  if (IsClass() && other.IsClass()) {
    return *this == other;
  }
  return false;
}

bool FieldType::NowIs(Handle<FieldType> other) const { return NowIs(*other); }

void FieldType::PrintTo(std::ostream& os) const {
  if (IsAny()) {
    os << "Any";
  } else if (IsNone()) {
    os << "None";
  } else {
    DCHECK(IsClass());
    os << "Class(" << reinterpret_cast<void*>(AsClass().ptr()) << ")";
  }
}

bool FieldType::NowContains(Object value) const {
  if (*this == Any()) return true;
  if (*this == None()) return false;
  if (!value.IsHeapObject()) return false;
  return HeapObject::cast(value).map() == Map::cast(*this);
}

}  // namespace internal
}  // namespace v8
