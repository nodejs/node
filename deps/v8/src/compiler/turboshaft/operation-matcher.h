// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_OPERATION_MATCHER_H_
#define V8_COMPILER_TURBOSHAFT_OPERATION_MATCHER_H_

#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"

namespace v8::internal::compiler::turboshaft {

class OperationMatcher {
 public:
  explicit OperationMatcher(Graph& graph) : graph_(graph) {}

  template <class Op>
  bool Is(OpIndex op_idx) const {
    return graph_.Get(op_idx).Is<Op>();
  }

  template <class Op>
  const underlying_operation_t<Op>* TryCast(OpIndex op_idx) const {
    return graph_.Get(op_idx).TryCast<Op>();
  }

  template <class Op>
  const underlying_operation_t<Op>& Cast(OpIndex op_idx) const {
    return graph_.Get(op_idx).Cast<Op>();
  }

  const Operation& Get(OpIndex op_idx) const { return graph_.Get(op_idx); }

  bool MatchZero(OpIndex matched) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    switch (op->kind) {
      case ConstantOp::Kind::kWord32:
      case ConstantOp::Kind::kWord64:
        return op->integral() == 0;
      case ConstantOp::Kind::kFloat32:
        return op->float32() == 0;
      case ConstantOp::Kind::kFloat64:
        return op->float64() == 0;
      default:
        return false;
    }
  }

  bool MatchIntegralZero(OpIndex matched) const {
    int64_t constant;
    return MatchSignedIntegralConstant(matched, &constant) && constant == 0;
  }

  bool MatchFloat32Constant(OpIndex matched, float* constant) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    if (op->kind != ConstantOp::Kind::kFloat32) return false;
    *constant = op->float32();
    return true;
  }

  bool MatchFloat64Constant(OpIndex matched, double* constant) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    if (op->kind != ConstantOp::Kind::kFloat64) return false;
    *constant = op->float64();
    return true;
  }

  bool MatchFloat(OpIndex matched, double* value) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    if (op->kind == ConstantOp::Kind::kFloat64) {
      *value = op->float64();
      return true;
    } else if (op->kind == ConstantOp::Kind::kFloat32) {
      *value = op->float32();
      return true;
    }
    return false;
  }

  bool MatchFloat(OpIndex matched, double value) const {
    double k;
    if (!MatchFloat(matched, &k)) return false;
    return base::bit_cast<uint64_t>(value) == base::bit_cast<uint64_t>(k) ||
           (std::isnan(k) && std::isnan(value));
  }

  bool MatchNaN(OpIndex matched) const {
    double k;
    return MatchFloat(matched, &k) && std::isnan(k);
  }

  bool MatchTaggedConstant(OpIndex matched, Handle<HeapObject>* tagged) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    if (!(op->kind == any_of(ConstantOp::Kind::kHeapObject,
                             ConstantOp::Kind::kCompressedHeapObject))) {
      return false;
    }
    *tagged = op->handle();
    return true;
  }

  bool MatchWordConstant(OpIndex matched, WordRepresentation rep,
                         uint64_t* unsigned_constant,
                         int64_t* signed_constant = nullptr) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    switch (op->rep.value()) {
      case RegisterRepresentation::Word32():
        if (rep != WordRepresentation::Word32()) return false;
        break;
      case RegisterRepresentation::Word64():
        if (!(rep == any_of(WordRepresentation::Word64(),
                            WordRepresentation::Word32()))) {
          return false;
        }
        break;
      default:
        return false;
    }
    if (unsigned_constant) {
      switch (rep.value()) {
        case WordRepresentation::Word32():
          *unsigned_constant = static_cast<uint32_t>(op->integral());
          break;
        case WordRepresentation::Word64():
          *unsigned_constant = op->integral();
          break;
      }
    }
    if (signed_constant) {
      switch (rep.value()) {
        case WordRepresentation::Word32():
          *signed_constant = static_cast<int32_t>(op->signed_integral());
          break;
        case WordRepresentation::Word64():
          *signed_constant = op->signed_integral();
          break;
      }
    }
    return true;
  }

  bool MatchWordConstant(OpIndex matched, WordRepresentation rep,
                         int64_t* signed_constant) const {
    return MatchWordConstant(matched, rep, nullptr, signed_constant);
  }

  bool MatchWord64Constant(OpIndex matched, uint64_t* constant) const {
    return MatchWordConstant(matched, WordRepresentation::Word64(), constant);
  }

  bool MatchWord32Constant(OpIndex matched, uint32_t* constant) const {
    if (uint64_t value;
        MatchWordConstant(matched, WordRepresentation::Word32(), &value)) {
      *constant = static_cast<uint32_t>(value);
      return true;
    }
    return false;
  }

  bool MatchWord64Constant(OpIndex matched, int64_t* constant) const {
    return MatchWordConstant(matched, WordRepresentation::Word64(), constant);
  }

  bool MatchWord32Constant(OpIndex matched, int32_t* constant) const {
    if (int64_t value;
        MatchWordConstant(matched, WordRepresentation::Word32(), &value)) {
      *constant = static_cast<int32_t>(value);
      return true;
    }
    return false;
  }

  bool MatchSignedIntegralConstant(OpIndex matched, int64_t* constant) const {
    if (const ConstantOp* c = TryCast<ConstantOp>(matched)) {
      if (c->kind == ConstantOp::Kind::kWord32 ||
          c->kind == ConstantOp::Kind::kWord64) {
        *constant = c->signed_integral();
        return true;
      }
    }
    return false;
  }

  bool MatchUnsignedIntegralConstant(OpIndex matched,
                                     uint64_t* constant) const {
    if (const ConstantOp* c = TryCast<ConstantOp>(matched)) {
      if (c->kind == ConstantOp::Kind::kWord32 ||
          c->kind == ConstantOp::Kind::kWord64) {
        *constant = c->integral();
        return true;
      }
    }
    return false;
  }

  bool MatchExternalConstant(OpIndex matched,
                             ExternalReference* reference) const {
    const ConstantOp* op = TryCast<ConstantOp>(matched);
    if (!op) return false;
    if (op->kind != ConstantOp::Kind::kExternal) return false;
    *reference = op->storage.external;
    return true;
  }

  bool MatchChange(OpIndex matched, OpIndex* input, ChangeOp::Kind kind,
                   RegisterRepresentation from,
                   RegisterRepresentation to) const {
    const ChangeOp* op = TryCast<ChangeOp>(matched);
    if (!op || op->kind != kind || op->from != from || op->to != to) {
      return false;
    }
    *input = op->input();
    return true;
  }

  bool MatchWordBinop(OpIndex matched, OpIndex* left, OpIndex* right,
                      WordBinopOp::Kind* kind, WordRepresentation* rep) const {
    const WordBinopOp* op = TryCast<WordBinopOp>(matched);
    if (!op) return false;
    *kind = op->kind;
    *rep = op->rep;
    *left = op->left();
    *right = op->right();
    return true;
  }

  bool MatchWordBinop(OpIndex matched, OpIndex* left, OpIndex* right,
                      WordBinopOp::Kind kind, WordRepresentation rep) const {
    const WordBinopOp* op = TryCast<WordBinopOp>(matched);
    if (!op || kind != op->kind) {
      return false;
    }
    if (!(rep == op->rep ||
          (WordBinopOp::AllowsWord64ToWord32Truncation(kind) &&
           rep == WordRepresentation::Word32() &&
           op->rep == WordRepresentation::Word64()))) {
      return false;
    }
    *left = op->left();
    *right = op->right();
    return true;
  }

  bool MatchWordAdd(OpIndex matched, OpIndex* left, OpIndex* right,
                    WordRepresentation rep) const {
    return MatchWordBinop(matched, left, right, WordBinopOp::Kind::kAdd, rep);
  }

  bool MatchWordSub(OpIndex matched, OpIndex* left, OpIndex* right,
                    WordRepresentation rep) const {
    return MatchWordBinop(matched, left, right, WordBinopOp::Kind::kSub, rep);
  }

  bool MatchBitwiseAnd(OpIndex matched, OpIndex* left, OpIndex* right,
                       WordRepresentation rep) const {
    return MatchWordBinop(matched, left, right, WordBinopOp::Kind::kBitwiseAnd,
                          rep);
  }

  bool MatchEqual(OpIndex matched, OpIndex* left, OpIndex* right,
                  WordRepresentation rep) const {
    const EqualOp* op = TryCast<EqualOp>(matched);
    if (!op || rep != op->rep) return false;
    *left = op->left();
    *right = op->right();
    return true;
  }

  bool MatchComparison(OpIndex matched, OpIndex* left, OpIndex* right,
                       ComparisonOp::Kind* kind,
                       RegisterRepresentation* rep) const {
    const ComparisonOp* op = TryCast<ComparisonOp>(matched);
    if (!op) return false;
    *kind = op->kind;
    *rep = op->rep;
    *left = op->left();
    *right = op->right();
    return true;
  }

  bool MatchFloatUnary(OpIndex matched, OpIndex* input, FloatUnaryOp::Kind kind,
                       FloatRepresentation rep) const {
    const FloatUnaryOp* op = TryCast<FloatUnaryOp>(matched);
    if (!op || op->kind != kind || op->rep != rep) return false;
    *input = op->input();
    return true;
  }

  bool MatchFloatRoundDown(OpIndex matched, OpIndex* input,
                           FloatRepresentation rep) const {
    return MatchFloatUnary(matched, input, FloatUnaryOp::Kind::kRoundDown, rep);
  }

  bool MatchFloatBinary(OpIndex matched, OpIndex* left, OpIndex* right,
                        FloatBinopOp::Kind kind,
                        FloatRepresentation rep) const {
    const FloatBinopOp* op = TryCast<FloatBinopOp>(matched);
    if (!op || op->kind != kind || op->rep != rep) return false;
    *left = op->left();
    *right = op->right();
    return true;
  }

  bool MatchFloatSub(OpIndex matched, OpIndex* left, OpIndex* right,
                     FloatRepresentation rep) const {
    return MatchFloatBinary(matched, left, right, FloatBinopOp::Kind::kSub,
                            rep);
  }

  bool MatchConstantShift(OpIndex matched, OpIndex* input, ShiftOp::Kind* kind,
                          WordRepresentation* rep, int* amount) const {
    const ShiftOp* op = TryCast<ShiftOp>(matched);
    if (uint32_t rhs_constant;
        op && MatchWord32Constant(op->right(), &rhs_constant) &&
        rhs_constant < static_cast<uint64_t>(op->rep.bit_width())) {
      *input = op->left();
      *kind = op->kind;
      *rep = op->rep;
      *amount = static_cast<int>(rhs_constant);
      return true;
    }
    return false;
  }

  bool MatchConstantShift(OpIndex matched, OpIndex* input, ShiftOp::Kind kind,
                          WordRepresentation rep, int* amount) const {
    const ShiftOp* op = TryCast<ShiftOp>(matched);
    if (uint32_t rhs_constant;
        op && op->kind == kind &&
        (op->rep == rep || (ShiftOp::AllowsWord64ToWord32Truncation(kind) &&
                            rep == WordRepresentation::Word32() &&
                            op->rep == WordRepresentation::Word64())) &&
        MatchWord32Constant(op->right(), &rhs_constant) &&
        rhs_constant < static_cast<uint64_t>(rep.bit_width())) {
      *input = op->left();
      *amount = static_cast<int>(rhs_constant);
      return true;
    }
    return false;
  }

  bool MatchConstantRightShift(OpIndex matched, OpIndex* input,
                               WordRepresentation rep, int* amount) const {
    const ShiftOp* op = TryCast<ShiftOp>(matched);
    if (uint32_t rhs_constant;
        op && ShiftOp::IsRightShift(op->kind) && op->rep == rep &&
        MatchWord32Constant(op->right(), &rhs_constant) &&
        rhs_constant < static_cast<uint32_t>(rep.bit_width())) {
      *input = op->left();
      *amount = static_cast<int>(rhs_constant);
      return true;
    }
    return false;
  }

  bool MatchConstantShiftRightArithmeticShiftOutZeros(OpIndex matched,
                                                      OpIndex* input,
                                                      WordRepresentation rep,
                                                      uint16_t* amount) const {
    const ShiftOp* op = TryCast<ShiftOp>(matched);
    if (uint32_t rhs_constant;
        op && op->kind == ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros &&
        op->rep == rep && MatchWord32Constant(op->right(), &rhs_constant) &&
        rhs_constant < static_cast<uint64_t>(rep.bit_width())) {
      *input = op->left();
      *amount = static_cast<uint16_t>(rhs_constant);
      return true;
    }
    return false;
  }

 private:
  Graph& graph_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPERATION_MATCHER_H_
