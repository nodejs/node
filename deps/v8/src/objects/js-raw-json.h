// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_RAW_JSON_H_
#define V8_OBJECTS_JS_RAW_JSON_H_

#include "src/execution/isolate.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-raw-json-tq.inc"

V8_OBJECT class JSRawJson : public JSObject {
 public:
  // Returns whether this raw JSON object has the initial layout and the
  // "rawJSON" property can be directly accessed at sizeof(JSRawJson).
  inline bool HasInitialLayout(Isolate* isolate) const;

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSRawJson> Create(
      Isolate* isolate, Handle<Object> text);

  DECL_PRINTER(JSRawJson)
  DECL_VERIFIER(JSRawJson)
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RAW_JSON_H_
