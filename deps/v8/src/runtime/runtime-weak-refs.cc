// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/execution/arguments-inl.h"
#include "src/objects/js-weak-refs-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ShrinkFinalizationRegistryUnregisterTokenMap) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFinalizationRegistry, finalization_registry, 0);

  if (!finalization_registry->key_map().IsUndefined(isolate)) {
    Handle<SimpleNumberDictionary> key_map =
        handle(SimpleNumberDictionary::cast(finalization_registry->key_map()),
               isolate);
    key_map = SimpleNumberDictionary::Shrink(isolate, key_map);
    finalization_registry->set_key_map(*key_map);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
