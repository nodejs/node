// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/field-type.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

// static
FieldType* FieldType::None() {
  // Do not Smi::kZero here or for Any(), as that may translate
  // as `nullptr` which is not a valid value for `this`.
  return reinterpret_cast<FieldType*>(Smi::FromInt(2));
}

// static
FieldType* FieldType::Any() {
  return reinterpret_cast<FieldType*>(Smi::FromInt(1));
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
FieldType* FieldType::Class(i::Map* map) { return FieldType::cast(map); }

// static
Handle<FieldType> FieldType::Class(i::Handle<i::Map> map, Isolate* isolate) {
  return handle(Class(*map), isolate);
}

// static
FieldType* FieldType::cast(Object* object) {
  DCHECK(object == None() || object == Any() || object->IsMap());
  return reinterpret_cast<FieldType*>(object);
}

bool FieldType::IsClass() { return this->IsMap(); }

Map* FieldType::AsClass() {
  DCHECK(IsClass());
  return Map::cast(this);
}

bool FieldType::NowStable() {
  return !this->IsClass() || AsClass()->is_stable();
}

bool FieldType::NowIs(FieldType* other) {
  if (other->IsAny()) return true;
  if (IsNone()) return true;
  if (other->IsNone()) return false;
  if (IsAny()) return false;
  DCHECK(IsClass());
  DCHECK(other->IsClass());
  return this == other;
}

bool FieldType::NowIs(Handle<FieldType> other) { return NowIs(*other); }

void FieldType::PrintTo(std::ostream& os) {
  if (IsAny()) {
    os << "Any";
  } else if (IsNone()) {
    os << "None";
  } else {
    DCHECK(IsClass());
    os << "Class(" << static_cast<void*>(AsClass()) << ")";
  }
}

bool FieldType::NowContains(Object* value) {
  if (this == Any()) return true;
  if (this == None()) return false;
  if (!value->IsHeapObject()) return false;
  return HeapObject::cast(value)->map() == Map::cast(this);
}

}  // namespace internal
}  // namespace v8
