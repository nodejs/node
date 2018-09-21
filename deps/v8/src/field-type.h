// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIELD_TYPE_H_
#define V8_FIELD_TYPE_H_

#include "src/objects.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class FieldType : public Object {
 public:
  static FieldType* None();
  static FieldType* Any();
  static Handle<FieldType> None(Isolate* isolate);
  static Handle<FieldType> Any(Isolate* isolate);
  static FieldType* Class(i::Map* map);
  static Handle<FieldType> Class(i::Handle<i::Map> map, Isolate* isolate);
  static FieldType* cast(Object* object);

  bool NowContains(Object* value);

  bool NowContains(Handle<Object> value) { return NowContains(*value); }

  bool IsClass();
  Map* AsClass();
  bool IsNone() { return this == None(); }
  bool IsAny() { return this == Any(); }
  bool NowStable();
  bool NowIs(FieldType* other);
  bool NowIs(Handle<FieldType> other);

  void PrintTo(std::ostream& os);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FIELD_TYPE_H_
