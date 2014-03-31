// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "assert-scope.h"
#include "v8.h"

namespace v8 {
namespace internal {

uint32_t PerIsolateAssertBase::GetData(Isolate* isolate) {
  return isolate->per_isolate_assert_data();
}


void PerIsolateAssertBase::SetData(Isolate* isolate, uint32_t data) {
  isolate->set_per_isolate_assert_data(data);
}

} }  // namespace v8::internal
