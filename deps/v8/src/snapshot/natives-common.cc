// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with internal or external natives.

#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/snapshot/natives.h"

namespace v8 {
namespace internal {

NativesExternalStringResource::NativesExternalStringResource(NativeType type,
                                                             int index)
    : type_(type), index_(index) {
  Vector<const char> source;
  DCHECK_LE(0, index);
  CHECK_EQ(EXTRAS, type_);
  DCHECK(index < ExtraNatives::GetBuiltinsCount());
  source = ExtraNatives::GetScriptSource(index);
  data_ = source.start();
  length_ = source.length();
}

}  // namespace internal
}  // namespace v8
