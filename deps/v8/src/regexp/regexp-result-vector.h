// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_RESULT_VECTOR_H_
#define V8_REGEXP_REGEXP_RESULT_VECTOR_H_

#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class RegExpResultVectorScope final {
 public:
  explicit RegExpResultVectorScope(Isolate* isolate);
  RegExpResultVectorScope(Isolate* isolate, int size);
  ~RegExpResultVectorScope();

  int32_t* Initialize(int size);

  int32_t* value() const {
    // Exactly one of if_static_ and if_dynamic_ is set.
    DCHECK_EQ(if_static_ == nullptr, if_dynamic_.get() != nullptr);
    return if_static_ != nullptr ? if_static_ : if_dynamic_.get();
  }

 private:
  Isolate* const isolate_;
  std::unique_ptr<int32_t[]> if_dynamic_;
  int32_t* if_static_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_RESULT_VECTOR_H_
