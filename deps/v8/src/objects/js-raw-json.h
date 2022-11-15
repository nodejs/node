// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_RAW_JSON_H_
#define V8_OBJECTS_JS_RAW_JSON_H_

#include "src/execution/isolate.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-raw-json-tq.inc"

class JSRawJson : public TorqueGeneratedJSRawJson<JSRawJson, JSObject> {
 public:
  // Layout description.
#define JS_RAW_JSON_FIELDS(V)    \
  V(kRawJsonOffset, kTaggedSize) \
  /* Total size. */              \
  V(kSize, 0)
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_RAW_JSON_FIELDS)
#undef JS_RAW_JSON_FIELDS

  static const int kRawJsonIndex = 0;

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSRawJson> Create(
      Isolate* isolate, Handle<Object> text);

  DECL_PRINTER(JSRawJson)

  TQ_OBJECT_CONSTRUCTORS(JSRawJson)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RAW_JSON_H_
