// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/objects-inl.h"
#include "src/objects/js-collection-inl.h"

namespace v8 {
namespace internal {

BUILTIN(MapPrototypeClear) {
  HandleScope scope(isolate);
  const char* const kMethodName = "Map.prototype.clear";
  CHECK_RECEIVER(JSMap, map, kMethodName);
  JSMap::Clear(isolate, map);
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(SetPrototypeClear) {
  HandleScope scope(isolate);
  const char* const kMethodName = "Set.prototype.clear";
  CHECK_RECEIVER(JSSet, set, kMethodName);
  JSSet::Clear(isolate, set);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
