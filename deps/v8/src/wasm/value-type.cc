// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/value-type.h"

#include "src/codegen/signature.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace wasm {

base::Optional<wasm::ValueKind> WasmReturnTypeFromSignature(
    const FunctionSig* wasm_signature) {
  if (wasm_signature->return_count() == 0) return {};

  DCHECK_EQ(wasm_signature->return_count(), 1);
  ValueType return_type = wasm_signature->GetReturn(0);
  return {return_type.kind()};
}

#if DEBUG
V8_EXPORT_PRIVATE extern void PrintFunctionSig(const wasm::FunctionSig* sig) {
  std::ostringstream os;
  os << sig->parameter_count() << " parameters:\n";
  for (size_t i = 0; i < sig->parameter_count(); i++) {
    os << "  " << i << ": " << sig->GetParam(i) << "\n";
  }
  os << sig->return_count() << " returns:\n";
  for (size_t i = 0; i < sig->return_count(); i++) {
    os << "  " << i << ": " << sig->GetReturn() << "\n";
  }
  PrintF("%s", os.str().c_str());
}
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8
