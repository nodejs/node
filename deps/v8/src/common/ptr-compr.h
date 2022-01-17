// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_H_
#define V8_COMMON_PTR_COMPR_H_

#include "src/common/globals.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

// See v8:7703 for details about how pointer compression works.
constexpr size_t kPtrComprCageReservationSize = size_t{4} * GB;
constexpr size_t kPtrComprCageBaseAlignment = size_t{4} * GB;

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_COMMON_PTR_COMPR_H_
