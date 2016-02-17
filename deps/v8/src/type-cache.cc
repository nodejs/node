// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/type-cache.h"

#include "src/base/lazy-instance.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {

namespace {

base::LazyInstance<TypeCache>::type kCache = LAZY_INSTANCE_INITIALIZER;

}  // namespace


// static
TypeCache const& TypeCache::Get() { return kCache.Get(); }

}  // namespace internal
}  // namespace v8
