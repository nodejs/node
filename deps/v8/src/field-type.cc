// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/field-type.h"

#include "src/handles-inl.h"
#include "src/ostreams.h"
#include "src/types.h"

namespace v8 {
namespace internal {

// static
FieldType* FieldType::None() {
  return reinterpret_cast<FieldType*>(Smi::FromInt(0));
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

Handle<i::Map> FieldType::AsClass() {
  DCHECK(IsClass());
  i::Map* map = Map::cast(this);
  return handle(map, map->GetIsolate());
}

bool FieldType::NowStable() {
  return !this->IsClass() || this->AsClass()->is_stable();
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

Type* FieldType::Convert(Zone* zone) {
  if (IsAny()) return Type::Any();
  if (IsNone()) return Type::None();
  DCHECK(IsClass());
  return Type::Class(AsClass(), zone);
}

void FieldType::PrintTo(std::ostream& os) {
  if (IsAny()) {
    os << "Any";
  } else if (IsNone()) {
    os << "None";
  } else {
    DCHECK(IsClass());
    os << "Class(" << static_cast<void*>(*AsClass()) << ")";
  }
}

}  // namespace internal
}  // namespace v8
