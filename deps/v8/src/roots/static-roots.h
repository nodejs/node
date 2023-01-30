// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_STATIC_ROOTS_H_
#define V8_ROOTS_STATIC_ROOTS_H_

#include "src/common/globals.h"
#if V8_STATIC_ROOTS_BOOL

namespace v8 {
namespace internal {

// TODO(olivf, v8:13466): Enable and add static roots
constexpr static std::array<Tagged_t, 0> StaticReadOnlyRootsPointerTable = {};

}  // namespace internal
}  // namespace v8
#endif  // V8_STATIC_ROOTS_BOOL
#endif  // V8_ROOTS_STATIC_ROOTS_H_
