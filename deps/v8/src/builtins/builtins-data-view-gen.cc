// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-data-view-gen.h"

#include "src/builtins/builtins-utils-gen.h"

namespace v8 {
namespace internal {

// Returns (intptr) a byte length value from [0..JSArrayBuffer::kMaxLength]
// If it fails (due to detached or OOB), it returns -1.
TF_BUILTIN(DataViewGetVariableLength, DataViewBuiltinsAssembler) {
  auto dataview = Parameter<JSDataView>(Descriptor::kDataView);
  CSA_CHECK(this, IsVariableLengthJSArrayBufferView(dataview));

  Label detached_or_oob(this);
  auto buffer =
      LoadObjectField<JSArrayBuffer>(dataview, JSDataView::kBufferOffset);
  TNode<UintPtrT> byte_length = LoadVariableLengthJSArrayBufferViewByteLength(
      dataview, buffer, &detached_or_oob);
  Return(byte_length);

  BIND(&detached_or_oob);
  Return(IntPtrConstant(-1));
}

}  // namespace internal
}  // namespace v8
