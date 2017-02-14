// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/promise-utils.h"

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

enum PromiseResolvingFunctionContextSlot {
  kAlreadyVisitedSlot = Context::MIN_CONTEXT_SLOTS,
  kPromiseSlot,
  kDebugEventSlot,
  kPromiseContextLength,
};

JSObject* PromiseUtils::GetPromise(Handle<Context> context) {
  return JSObject::cast(context->get(kPromiseSlot));
}

Object* PromiseUtils::GetDebugEvent(Handle<Context> context) {
  return context->get(kDebugEventSlot);
}

bool PromiseUtils::HasAlreadyVisited(Handle<Context> context) {
  return Smi::cast(context->get(kAlreadyVisitedSlot))->value() != 0;
}

void PromiseUtils::SetAlreadyVisited(Handle<Context> context) {
  context->set(kAlreadyVisitedSlot, Smi::FromInt(1));
}

void PromiseUtils::CreateResolvingFunctions(Isolate* isolate,
                                            Handle<JSObject> promise,
                                            Handle<Object> debug_event,
                                            Handle<JSFunction>* resolve,
                                            Handle<JSFunction>* reject) {
  DCHECK(debug_event->IsTrue(isolate) || debug_event->IsFalse(isolate));
  Handle<Context> context =
      isolate->factory()->NewPromiseResolvingFunctionContext(
          kPromiseContextLength);
  context->set_native_context(*isolate->native_context());
  // We set the closure to be an empty function, same as native context.
  context->set_closure(isolate->native_context()->closure());
  context->set(kAlreadyVisitedSlot, Smi::kZero);
  context->set(kPromiseSlot, *promise);
  context->set(kDebugEventSlot, *debug_event);

  Handle<SharedFunctionInfo> resolve_shared_fun(
      isolate->native_context()->promise_resolve_shared_fun(), isolate);
  Handle<JSFunction> resolve_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          isolate->sloppy_function_without_prototype_map(), resolve_shared_fun,
          isolate->native_context(), TENURED);

  Handle<SharedFunctionInfo> reject_shared_fun(
      isolate->native_context()->promise_reject_shared_fun(), isolate);
  Handle<JSFunction> reject_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          isolate->sloppy_function_without_prototype_map(), reject_shared_fun,
          isolate->native_context(), TENURED);

  resolve_fun->set_context(*context);
  reject_fun->set_context(*context);

  *resolve = resolve_fun;
  *reject = reject_fun;
}

}  // namespace internal
}  // namespace v8
