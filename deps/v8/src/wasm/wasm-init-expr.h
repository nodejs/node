// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_INIT_EXPR_H_
#define V8_WASM_WASM_INIT_EXPR_H_

#include <memory>

#include "src/wasm/value-type.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmModule;
class WasmFeatures;

// Representation of an initializer expression. Unlike {ConstantExpression} in
// wasm-module.h, this does not use {WireBytesRef}, i.e., it does not depend on
// a wasm module's bytecode representation.
class WasmInitExpr : public ZoneObject {
 public:
  enum Operator {
    kNone,
    kGlobalGet,
    kI32Const,
    kI64Const,
    kF32Const,
    kF64Const,
    kS128Const,
    kRefNullConst,
    kRefFuncConst,
    kStructNewWithRtt,
    kStructNew,
    kStructNewDefaultWithRtt,
    kStructNewDefault,
    kArrayInit,
    kArrayInitStatic,
    kRttCanon,
  };

  union Immediate {
    int32_t i32_const;
    int64_t i64_const;
    float f32_const;
    double f64_const;
    std::array<uint8_t, kSimd128Size> s128_const;
    uint32_t index;
    HeapType::Representation heap_type;
  };

  WasmInitExpr() : kind_(kNone), operands_(nullptr) {
    immediate_.i32_const = 0;
  }
  explicit WasmInitExpr(int32_t v) : kind_(kI32Const), operands_(nullptr) {
    immediate_.i32_const = v;
  }
  explicit WasmInitExpr(int64_t v) : kind_(kI64Const), operands_(nullptr) {
    immediate_.i64_const = v;
  }
  explicit WasmInitExpr(float v) : kind_(kF32Const), operands_(nullptr) {
    immediate_.f32_const = v;
  }
  explicit WasmInitExpr(double v) : kind_(kF64Const), operands_(nullptr) {
    immediate_.f64_const = v;
  }
  explicit WasmInitExpr(uint8_t v[kSimd128Size])
      : kind_(kS128Const), operands_(nullptr) {
    memcpy(immediate_.s128_const.data(), v, kSimd128Size);
  }

  static WasmInitExpr GlobalGet(uint32_t index) {
    WasmInitExpr expr;
    expr.kind_ = kGlobalGet;
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr RefFuncConst(uint32_t index) {
    WasmInitExpr expr;
    expr.kind_ = kRefFuncConst;
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr RefNullConst(HeapType::Representation heap_type) {
    WasmInitExpr expr;
    expr.kind_ = kRefNullConst;
    expr.immediate_.heap_type = heap_type;
    return expr;
  }

  static WasmInitExpr StructNewWithRtt(uint32_t index,
                                       ZoneVector<WasmInitExpr>* elements) {
    WasmInitExpr expr(kStructNewWithRtt, elements);
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr StructNew(uint32_t index,
                                ZoneVector<WasmInitExpr>* elements) {
    WasmInitExpr expr(kStructNew, elements);
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr StructNewDefaultWithRtt(Zone* zone, uint32_t index,
                                              WasmInitExpr rtt) {
    WasmInitExpr expr(kStructNewDefaultWithRtt,
                      zone->New<ZoneVector<WasmInitExpr>>(
                          std::initializer_list<WasmInitExpr>{rtt}, zone));
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr StructNewDefault(uint32_t index) {
    WasmInitExpr expr;
    expr.kind_ = kStructNewDefault;
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr ArrayInit(uint32_t index,
                                ZoneVector<WasmInitExpr>* elements) {
    WasmInitExpr expr(kArrayInit, elements);
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr ArrayInitStatic(uint32_t index,
                                      ZoneVector<WasmInitExpr>* elements) {
    WasmInitExpr expr(kArrayInitStatic, elements);
    expr.immediate_.index = index;
    return expr;
  }

  static WasmInitExpr RttCanon(uint32_t index) {
    WasmInitExpr expr;
    expr.kind_ = kRttCanon;
    expr.immediate_.index = index;
    return expr;
  }

  Immediate immediate() const { return immediate_; }
  Operator kind() const { return kind_; }
  const ZoneVector<WasmInitExpr>* operands() const { return operands_; }

  bool operator==(const WasmInitExpr& other) const {
    if (kind() != other.kind()) return false;
    switch (kind()) {
      case kNone:
        return true;
      case kGlobalGet:
      case kRefFuncConst:
      case kRttCanon:
        return immediate().index == other.immediate().index;
      case kI32Const:
        return immediate().i32_const == other.immediate().i32_const;
      case kI64Const:
        return immediate().i64_const == other.immediate().i64_const;
      case kF32Const:
        return immediate().f32_const == other.immediate().f32_const;
      case kF64Const:
        return immediate().f64_const == other.immediate().f64_const;
      case kS128Const:
        return immediate().s128_const == other.immediate().s128_const;
      case kRefNullConst:
        return immediate().heap_type == other.immediate().heap_type;
      case kStructNewWithRtt:
      case kStructNew:
      case kStructNewDefaultWithRtt:
      case kStructNewDefault:
        if (immediate().index != other.immediate().index) return false;
        DCHECK_EQ(operands()->size(), other.operands()->size());
        for (uint32_t i = 0; i < operands()->size(); i++) {
          if (operands()[i] != other.operands()[i]) return false;
        }
        return true;
      case kArrayInit:
      case kArrayInitStatic:
        if (immediate().index != other.immediate().index) return false;
        if (operands()->size() != other.operands()->size()) return false;
        for (uint32_t i = 0; i < operands()->size(); i++) {
          if (operands()[i] != other.operands()[i]) return false;
        }
        return true;
    }
  }

  V8_INLINE bool operator!=(const WasmInitExpr& other) const {
    return !(*this == other);
  }

  ValueType type(const WasmModule* module,
                 const WasmFeatures& enabled_features) const;

 private:
  WasmInitExpr(Operator kind, const ZoneVector<WasmInitExpr>* operands)
      : kind_(kind), operands_(operands) {}
  Immediate immediate_;
  Operator kind_;
  const ZoneVector<WasmInitExpr>* operands_;
};

ASSERT_TRIVIALLY_COPYABLE(WasmInitExpr);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_INIT_EXPR_H_
