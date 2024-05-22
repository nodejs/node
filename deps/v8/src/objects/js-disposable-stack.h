// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DISPOSABLE_STACK_H_
#define V8_OBJECTS_JS_DISPOSABLE_STACK_H_

#include "src/base/bit-field.h"
#include "src/handles/handles.h"
#include "src/objects/js-objects.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-disposable-stack-tq.inc"

// Valid states for a DisposableStack.
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposablestack-objects
enum class DisposableStackState { kDisposed, kPending };

class JSDisposableStack
    : public TorqueGeneratedJSDisposableStack<JSDisposableStack, JSObject> {
 public:
  DECL_CAST(JSDisposableStack)
  DECL_PRINTER(JSDisposableStack)
  DECL_VERIFIER(JSDisposableStack)

  DEFINE_TORQUE_GENERATED_DISPOSABLE_STACK_STATUS()
  DECL_PRIMITIVE_ACCESSORS(state, DisposableStackState)
  DECL_INT_ACCESSORS(length)

  static void Initialize(Isolate* isolate, Handle<JSDisposableStack> stack);
  static void Add(Isolate* isolate, Handle<JSDisposableStack> disposable_stack,
                  Handle<Object> value, Handle<Object> method);

  static Tagged<Object> DisposeResources(
      Isolate* isolate, Handle<JSDisposableStack> disposable_stack);

  TQ_OBJECT_CONSTRUCTORS(JSDisposableStack)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DISPOSABLE_STACK_H_
