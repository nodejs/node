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

template <typename T>
class Handle;

class FieldType : public Object {
 public:
  static FieldType None();
  static FieldType Any();
  V8_EXPORT_PRIVATE static Handle<FieldType> None(Isolate* isolate);
  V8_EXPORT_PRIVATE static Handle<FieldType> Any(Isolate* isolate);
  V8_EXPORT_PRIVATE static FieldType Class(Map map);
  V8_EXPORT_PRIVATE static Handle<FieldType> Class(Handle<Map> map,
                                                   Isolate* isolate);
  V8_EXPORT_PRIVATE static FieldType cast(Object object);
  static FieldType unchecked_cast(Object object) {
    return FieldType(object.ptr());
  }

  bool NowContains(Object value) const;

  bool NowContains(Handle<Object> value) const { return NowContains(*value); }

  bool IsClass() const;
  Map AsClass() const;
  bool IsNone() const { return *this == None(); }
  bool IsAny() const { return *this == Any(); }
  bool NowStable() const;
  bool NowIs(FieldType other) const;
  bool NowIs(Handle<FieldType> other) const;

  V8_EXPORT_PRIVATE bool Equals(FieldType other) const;
  V8_EXPORT_PRIVATE void PrintTo(std::ostream& os) const;

 private:
  explicit constexpr FieldType(Address ptr) : Object(ptr) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FIELD_TYPE_H_
