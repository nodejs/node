// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/ptr-compr.h"

namespace v8::internal {

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE

uintptr_t V8HeapCompressionScheme::base_ = kNullAddress;

#ifdef V8_EXTERNAL_CODE_SPACE
uintptr_t ExternalCodeCompressionScheme::base_ = kNullAddress;
#endif  // V8_EXTERNAL_CODE_SPACE

#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE

}  // namespace v8::internal
