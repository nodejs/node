// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/value-type.h"

#include "src/codegen/signature.h"

namespace v8 {
namespace internal {
namespace wasm {

base::Optional<wasm::ValueKind> WasmReturnTypeFromSignature(
    const FunctionSig* wasm_signature) {
  if (wasm_signature->return_count() == 0) {
    return {};
  } else {
    DCHECK_EQ(wasm_signature->return_count(), 1);
    ValueType return_type = wasm_signature->GetReturn(0);
    switch (return_type.kind()) {
      case kI32:
      case kI64:
      case kF32:
      case kF64:
        return {return_type.kind()};
      default:
        UNREACHABLE();
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
