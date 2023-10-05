// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIELD_TYPE_H_
#define V8_OBJECTS_FIELD_TYPE_H_

#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class FieldType : public Object {
 public:
  static Tagged<FieldType> None();
  static Tagged<FieldType> Any();
  V8_EXPORT_PRIVATE static Handle<FieldType> None(Isolate* isolate);
  V8_EXPORT_PRIVATE static Handle<FieldType> Any(Isolate* isolate);
  V8_EXPORT_PRIVATE static Tagged<FieldType> Class(Tagged<Map> map);
  V8_EXPORT_PRIVATE static Handle<FieldType> Class(Handle<Map> map,
                                                   Isolate* isolate);
  V8_EXPORT_PRIVATE static Tagged<FieldType> cast(Tagged<Object> object);
  static constexpr Tagged<FieldType> unchecked_cast(Tagged<Object> object) {
    return Tagged<FieldType>(object.ptr());
  }

  bool NowContains(Tagged<Object> value) const;

  bool NowContains(Handle<Object> value) const { return NowContains(*value); }

  Tagged<Map> AsClass() const;
  bool NowStable() const;
  bool NowIs(Tagged<FieldType> other) const;
  bool NowIs(Handle<FieldType> other) const;

  V8_EXPORT_PRIVATE bool Equals(Tagged<FieldType> other) const;
  V8_EXPORT_PRIVATE void PrintTo(std::ostream& os) const;

 private:
  friend class Tagged<FieldType>;

  explicit constexpr FieldType(Address ptr) : Object(ptr) {
    // TODO(leszeks): Typecheck that this is Map or Smi.
  }
  explicit constexpr FieldType(Address ptr, SkipTypeCheckTag) : Object(ptr) {}
};

bool IsClass(Tagged<FieldType> obj);
inline bool IsNone(Tagged<FieldType> obj) { return obj == FieldType::None(); }
inline bool IsAny(Tagged<FieldType> obj) { return obj == FieldType::Any(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FIELD_TYPE_H_
