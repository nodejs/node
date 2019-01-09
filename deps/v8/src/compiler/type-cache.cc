// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/type-cache.h"

#include "src/base/lazy-instance.h"

namespace v8 {
namespace internal {
namespace compiler {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(const TypeCache, TypeCache::Get);

}  // namespace compiler
}  // namespace internal
}  // namespace v8
