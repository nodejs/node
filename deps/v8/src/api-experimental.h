// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_EXPERIMENTAL_H_
#define V8_API_EXPERIMENTAL_H_

#include "src/handles.h"

namespace v8 {
namespace internal {
class Code;
}  // internal;
namespace experimental {
class FastAccessorBuilder;
}  // experimental

namespace internal {
namespace experimental {

v8::internal::MaybeHandle<v8::internal::Code> BuildCodeFromFastAccessorBuilder(
    v8::experimental::FastAccessorBuilder* fast_handler);

}  // namespace experimental
}  // namespace internal
}  // namespace v8

#endif  // V8_API_EXPERIMENTAL_H_
