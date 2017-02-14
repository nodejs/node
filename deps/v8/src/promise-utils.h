// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROMISE_UTILS_H_
#define V8_PROMISE_UTILS_H_

#include "src/objects.h"

namespace v8 {
namespace internal {

// Helper methods for Promise builtins.
class PromiseUtils : public AllStatic {
 public:
  // These get and set the slots on the PromiseResolvingContext, which
  // is used by the resolve/reject promise callbacks.
  static JSObject* GetPromise(Handle<Context> context);
  static Object* GetDebugEvent(Handle<Context> context);
  static bool HasAlreadyVisited(Handle<Context> context);
  static void SetAlreadyVisited(Handle<Context> context);

  static void CreateResolvingFunctions(Isolate* isolate,
                                       Handle<JSObject> promise,
                                       Handle<Object> debug_event,
                                       Handle<JSFunction>* resolve,
                                       Handle<JSFunction>* reject);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_PROMISE_UTILS_H_
