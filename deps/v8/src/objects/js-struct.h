// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_STRUCT_H_
#define V8_OBJECTS_JS_STRUCT_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-struct-tq.inc"

class AlwaysSharedSpaceJSObject
    : public TorqueGeneratedAlwaysSharedSpaceJSObject<AlwaysSharedSpaceJSObject,
                                                      JSObject> {
 public:
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<AlwaysSharedSpaceJSObject> shared_obj,
      Handle<Object> key, PropertyDescriptor* desc,
      Maybe<ShouldThrow> should_throw);

  static_assert(kHeaderSize == JSObject::kHeaderSize);
  TQ_OBJECT_CONSTRUCTORS(AlwaysSharedSpaceJSObject)
};

class JSSharedStruct
    : public TorqueGeneratedJSSharedStruct<JSSharedStruct,
                                           AlwaysSharedSpaceJSObject> {
 public:
  DECL_CAST(JSSharedStruct)
  DECL_PRINTER(JSSharedStruct)
  EXPORT_DECL_VERIFIER(JSSharedStruct)

  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(JSSharedStruct)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_STRUCT_H_
