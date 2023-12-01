// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/logging/counters.h"
#include "src/objects/js-weak-refs-inl.h"

namespace v8 {
namespace internal {

// https://tc39.es/proposal-symbols-as-weakmap-keys/#sec-finalization-registry.prototype.unregister
BUILTIN(FinalizationRegistryUnregister) {
  HandleScope scope(isolate);
  const char* method_name = "FinalizationRegistry.prototype.unregister";

  // 1. Let finalizationGroup be the this value.
  //
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError
  //    exception.
  //
  // 3. If finalizationGroup does not have a [[Cells]] internal slot,
  //    throw a TypeError exception.
  CHECK_RECEIVER(JSFinalizationRegistry, finalization_registry, method_name);

  Handle<Object> unregister_token = args.atOrUndefined(isolate, 1);

  // 4. If CanBeHeldWeakly(unregisterToken) is false, throw a TypeError
  // exception.
  if (!unregister_token->CanBeHeldWeakly()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidWeakRefsUnregisterToken,
                              unregister_token));
  }

  bool success = JSFinalizationRegistry::Unregister(
      finalization_registry, Handle<HeapObject>::cast(unregister_token),
      isolate);

  return *isolate->factory()->ToBoolean(success);
}

}  // namespace internal
}  // namespace v8
