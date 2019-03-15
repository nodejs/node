// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-label.h"

#include "src/interpreter/bytecode-array-builder.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeLabel* BytecodeLabels::New() {
  DCHECK(!is_bound());
  labels_.emplace_back(BytecodeLabel());
  return &labels_.back();
}

void BytecodeLabels::Bind(BytecodeArrayBuilder* builder) {
  DCHECK(!is_bound_);
  is_bound_ = true;
  for (auto& label : labels_) {
    builder->Bind(&label);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
