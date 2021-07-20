// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-init-expr.h"

namespace v8 {
namespace internal {
namespace wasm {

std::ostream& operator<<(std::ostream& os, const WasmInitExpr& expr) {
  os << "(";
  switch (expr.kind()) {
    case WasmInitExpr::kNone:
      UNREACHABLE();
    case WasmInitExpr::kGlobalGet:
      os << "global.get " << expr.immediate().index;
      break;
    case WasmInitExpr::kI32Const:
      os << "i32.const " << expr.immediate().i32_const;
      break;
    case WasmInitExpr::kI64Const:
      os << "i64.const " << expr.immediate().i64_const;
      break;
    case WasmInitExpr::kF32Const:
      os << "f32.const " << expr.immediate().f32_const;
      break;
    case WasmInitExpr::kF64Const:
      os << "f64.const " << expr.immediate().f64_const;
      break;
    case WasmInitExpr::kS128Const:
      os << "s128.const 0x" << std::hex;
      for (uint8_t b : expr.immediate().s128_const) {
        os << b;
      }
      os << std::dec;
      break;
    case WasmInitExpr::kRefNullConst:
      os << "ref.null " << expr.immediate().heap_type;
      break;
    case WasmInitExpr::kRefFuncConst:
      os << "ref.func " << expr.immediate().index;
      break;
    case WasmInitExpr::kRttCanon:
      os << "rtt.canon " << expr.immediate().heap_type;
      break;
    case WasmInitExpr::kRttSub:
      os << "rtt.sub " << *expr.operand();
      break;
  }
  os << ")";
  return os;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
