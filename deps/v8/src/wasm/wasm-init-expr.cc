// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-init-expr.h"

#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

ValueType WasmInitExpr::type(const WasmModule* module,
                             const WasmFeatures& enabled_features) const {
  switch (kind()) {
    case kNone:
      return kWasmBottom;
    case kGlobalGet:
      return immediate().index < module->globals.size()
                 ? module->globals[immediate().index].type
                 : kWasmBottom;
    case kI32Const:
      return kWasmI32;
    case kI64Const:
      return kWasmI64;
    case kF32Const:
      return kWasmF32;
    case kF64Const:
      return kWasmF64;
    case kS128Const:
      return kWasmS128;
    case kRefFuncConst: {
      uint32_t heap_type = enabled_features.has_typed_funcref()
                               ? module->functions[immediate().index].sig_index
                               : HeapType::kFunc;
      return ValueType::Ref(heap_type, kNonNullable);
    }
    case kRefNullConst:
      return ValueType::Ref(immediate().heap_type, kNullable);
    case kStructNewWithRtt:
    case kStructNew:
    case kStructNewDefaultWithRtt:
    case kStructNewDefault:
    case kArrayInit:
    case kArrayInitStatic:
      return ValueType::Ref(immediate().index, kNonNullable);
    case kRttCanon:
      return ValueType::Rtt(immediate().heap_type);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
