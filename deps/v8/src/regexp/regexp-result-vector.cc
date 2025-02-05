// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-result-vector.h"

#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

RegExpResultVectorScope::RegExpResultVectorScope(Isolate* isolate)
    : isolate_(isolate) {}

RegExpResultVectorScope::RegExpResultVectorScope(Isolate* isolate, int size)
    : isolate_(isolate) {
  Initialize(size);
}

RegExpResultVectorScope::~RegExpResultVectorScope() {
  if (if_static_ != nullptr) {
    // Return ownership of the static vector.
    isolate_->set_regexp_static_result_offsets_vector(if_static_);
  }
}

int32_t* RegExpResultVectorScope::Initialize(int size) {
  DCHECK(if_static_ == nullptr && if_dynamic_.get() == nullptr);
  int32_t* static_vector_or_null =
      isolate_->regexp_static_result_offsets_vector();
  int32_t* result;
  if (size > Isolate::kJSRegexpStaticOffsetsVectorSize ||
      static_vector_or_null == nullptr) {
    result = NewArray<int32_t>(size);
    if_dynamic_.reset(result);
  } else {
    result = static_vector_or_null;
    if_static_ = result;
    // Take ownership of the static vector. See also:
    // RegExpBuiltinsAssembler::TryLoadStaticRegExpResultVector.
    isolate_->set_regexp_static_result_offsets_vector(nullptr);
  }
  // Exactly one of if_static_ and if_dynamic_ is set.
  DCHECK_EQ(if_static_ == nullptr, if_dynamic_.get() != nullptr);
  return result;
}

}  // namespace internal
}  // namespace v8
