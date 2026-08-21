// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SHADOW_REALM_H_
#define V8_OBJECTS_JS_SHADOW_REALM_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class NativeContext;

#include "torque-generated/src/objects/js-shadow-realm-tq.inc"

// ShadowRealm object from the JS ShadowRealm spec proposal:
// https://github.com/tc39/proposal-shadowrealm
V8_OBJECT class JSShadowRealm : public JSObject {
 public:
  inline Tagged<NativeContext> native_context() const;
  inline void set_native_context(Tagged<NativeContext> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSShadowRealm)
  EXPORT_DECL_VERIFIER(JSShadowRealm)

  class BodyDescriptor;

 public:
  TaggedMember<NativeContext> native_context_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SHADOW_REALM_H_
