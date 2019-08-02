// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator-reducer.h"
#include <cmath>

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/ieee754.h"
#include "src/base/overflowing-math.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/numbers/conversions-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineOperatorReducer::MachineOperatorReducer(Editor* editor,
                                               MachineGraph* mcgraph,
                                               bool allow_signalling_nan)
    : AdvancedReducer(editor),
      mcgraph_(mcgraph),
      allow_signalling_nan_(allow_signalling_nan) {}

MachineOperatorReducer::~MachineOperatorReducer() = default;


Node* MachineOperatorReducer::Float32Constant(volatile float value) {
  return graph()->NewNode(common()->Float32Constant(value));
}


Node* MachineOperatorReducer::Float64Constant(volatile double value) {
  return mcgraph()->Float64Constant(value);
}


Node* MachineOperatorReducer::Int32Constant(int32_t value) {
  return mcgraph()->Int32Constant(value);
}


Node* MachineOperatorReducer::Int64Constant(int64_t value) {
  return graph()->NewNode(common()->Int64Constant(value));
}

Node* MachineOperatorReducer::Float64Mul(Node* lhs, Node* rhs) {
  return graph()->NewNode(machine()->Float64Mul(), lhs, rhs);
}

Node* MachineOperatorReducer::Float64PowHalf(Node* value) {
  value =
      graph()->NewNode(machine()->Float64Add(), Float64Constant(0.0), value);
  Diamond d(graph(), common(),
            graph()->NewNode(machine()->Float64LessThanOrEqual(), value,
                             Float64Constant(-V8_INFINITY)),
            BranchHint::kFalse);
  return d.Phi(MachineRepresentation::kFloat64, Float64Constant(V8_INFINITY),
               graph()->NewNode(machine()->Float64Sqrt(), value));
}

Node* MachineOperatorReducer::Word32And(Node* lhs, Node* rhs) {
  Node* const node = graph()->NewNode(machine()->Word32And(), lhs, rhs);
  Reduction const reduction = ReduceWord32And(node);
  return reduction.Changed() ? reduction.replacement() : node;
}


Node* MachineOperatorReducer::Word32Sar(Node* lhs, uint32_t rhs) {
  if (rhs == 0) return lhs;
  return graph()->NewNode(machine()->Word32Sar(), lhs, Uint32Constant(rhs));
}


Node* MachineOperatorReducer::Word32Shr(Node* lhs, uint32_t rhs) {
  if (rhs == 0) return lhs;
  return graph()->NewNode(machine()->Word32Shr(), lhs, Uint32Constant(rhs));
}


Node* MachineOperatorReducer::Word32Equal(Node* lhs, Node* rhs) {
  return graph()->NewNode(machine()->Word32Equal(), lhs, rhs);
}


Node* MachineOperatorReducer::Int32Add(Node* lhs, Node* rhs) {
  Node* const node = graph()->NewNode(machine()->Int32Add(), lhs, rhs);
  Reduction const reduction = ReduceInt32Add(node);
  return reduction.Changed() ? reduction.replacement() : node;
}


Node* MachineOperatorReducer::Int32Sub(Node* lhs, Node* rhs) {
  Node* const node = graph()->NewNode(machine()->Int32Sub(), lhs, rhs);
  Reduction const reduction = ReduceInt32Sub(node);
  return reduction.Changed() ? reduction.replacement() : node;
}


Node* MachineOperatorReducer::Int32Mul(Node* lhs, Node* rhs) {
  return graph()->NewNode(machine()->Int32Mul(), lhs, rhs);
}


Node* MachineOperatorReducer::Int32Div(Node* dividend, int32_t divisor) {
  DCHECK_NE(0, divisor);
  DCHECK_NE(std::numeric_limits<int32_t>::min(), divisor);
  base::MagicNumbersForDivision<uint32_t> const mag =
      base::SignedDivisionByConstant(bit_cast<uint32_t>(divisor));
  Node* quotient = graph()->NewNode(machine()->Int32MulHigh(), dividend,
                                    Uint32Constant(mag.multiplier));
  if (divisor > 0 && bit_cast<int32_t>(mag.multiplier) < 0) {
    quotient = Int32Add(quotient, dividend);
  } else if (divisor < 0 && bit_cast<int32_t>(mag.multiplier) > 0) {
    quotient = Int32Sub(quotient, dividend);
  }
  return Int32Add(Word32Sar(quotient, mag.shift), Word32Shr(dividend, 31));
}


Node* MachineOperatorReducer::Uint32Div(Node* dividend, uint32_t divisor) {
  DCHECK_LT(0u, divisor);
  // If the divisor is even, we can avoid using the expensive fixup by shifting
  // the dividend upfront.
  unsigned const shift = base::bits::CountTrailingZeros(divisor);
  dividend = Word32Shr(dividend, shift);
  divisor >>= shift;
  // Compute the magic number for the (shifted) divisor.
  base::MagicNumbersForDivision<uint32_t> const mag =
      base::UnsignedDivisionByConstant(divisor, shift);
  Node* quotient = graph()->NewNode(machine()->Uint32MulHigh(), dividend,
                                    Uint32Constant(mag.multiplier));
  if (mag.add) {
    DCHECK_LE(1u, mag.shift);
    quotient = Word32Shr(
        Int32Add(Word32Shr(Int32Sub(dividend, quotient), 1), quotient),
        mag.shift - 1);
  } else {
    quotient = Word32Shr(quotient, mag.shift);
  }
  return quotient;
}


// Perform constant folding and strength reduction on machine operators.
Reduction MachineOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kProjection:
      return ReduceProjection(ProjectionIndexOf(node->op()), node->InputAt(0));
    case IrOpcode::kWord32And:
      return ReduceWord32And(node);
    case IrOpcode::kWord32Or:
      return ReduceWord32Or(node);
    case IrOpcode::kWord32Xor:
      return ReduceWord32Xor(node);
    case IrOpcode::kWord32Shl:
      return ReduceWord32Shl(node);
    case IrOpcode::kWord64Shl:
      return ReduceWord64Shl(node);
    case IrOpcode::kWord32Shr:
      return ReduceWord32Shr(node);
    case IrOpcode::kWord64Shr:
      return ReduceWord64Shr(node);
    case IrOpcode::kWord32Sar:
      return ReduceWord32Sar(node);
    case IrOpcode::kWord64Sar:
      return ReduceWord64Sar(node);
    case IrOpcode::kWord32Ror: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x ror 0 => x
      if (m.IsFoldable()) {                                  // K ror K => K
        return ReplaceInt32(base::bits::RotateRight32(m.left().Value(),
                                                      m.right().Value() & 31));
      }
      break;
    }
    case IrOpcode::kWord32Equal: {
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {  // K == K => K
        return ReplaceBool(m.left().Value() == m.right().Value());
      }
      if (m.left().IsInt32Sub() && m.right().Is(0)) {  // x - y == 0 => x == y
        Int32BinopMatcher msub(m.left().node());
        node->ReplaceInput(0, msub.left().node());
        node->ReplaceInput(1, msub.right().node());
        return Changed(node);
      }
      // TODO(turbofan): fold HeapConstant, ExternalReference, pointer compares
      if (m.LeftEqualsRight()) return ReplaceBool(true);  // x == x => true
      break;
    }
    case IrOpcode::kWord64Equal: {
      Int64BinopMatcher m(node);
      if (m.IsFoldable()) {  // K == K => K
        return ReplaceBool(m.left().Value() == m.right().Value());
      }
      if (m.left().IsInt64Sub() && m.right().Is(0)) {  // x - y == 0 => x == y
        Int64BinopMatcher msub(m.left().node());
        node->ReplaceInput(0, msub.left().node());
        node->ReplaceInput(1, msub.right().node());
        return Changed(node);
      }
      // TODO(turbofan): fold HeapConstant, ExternalReference, pointer compares
      if (m.LeftEqualsRight()) return ReplaceBool(true);  // x == x => true
      break;
    }
    case IrOpcode::kInt32Add:
      return ReduceInt32Add(node);
    case IrOpcode::kInt64Add:
      return ReduceInt64Add(node);
    case IrOpcode::kInt32Sub:
      return ReduceInt32Sub(node);
    case IrOpcode::kInt64Sub:
      return ReduceInt64Sub(node);
    case IrOpcode::kInt32Mul: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.right().node());  // x * 0 => 0
      if (m.right().Is(1)) return Replace(m.left().node());   // x * 1 => x
      if (m.IsFoldable()) {                                   // K * K => K
        return ReplaceInt32(
            base::MulWithWraparound(m.left().Value(), m.right().Value()));
      }
      if (m.right().Is(-1)) {  // x * -1 => 0 - x
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Int32Sub());
        return Changed(node);
      }
      if (m.right().IsPowerOf2()) {  // x * 2^n => x << n
        node->ReplaceInput(1, Int32Constant(WhichPowerOf2(m.right().Value())));
        NodeProperties::ChangeOp(node, machine()->Word32Shl());
        Reduction reduction = ReduceWord32Shl(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
      break;
    }
    case IrOpcode::kInt32MulWithOverflow: {
      Int32BinopMatcher m(node);
      if (m.right().Is(2)) {
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Int32AddWithOverflow());
        return Changed(node);
      }
      if (m.right().Is(-1)) {
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Int32SubWithOverflow());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kInt32Div:
      return ReduceInt32Div(node);
    case IrOpcode::kUint32Div:
      return ReduceUint32Div(node);
    case IrOpcode::kInt32Mod:
      return ReduceInt32Mod(node);
    case IrOpcode::kUint32Mod:
      return ReduceUint32Mod(node);
    case IrOpcode::kInt32LessThan: {
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {  // K < K => K
        return ReplaceBool(m.left().Value() < m.right().Value());
      }
      if (m.LeftEqualsRight()) return ReplaceBool(false);  // x < x => false
      if (m.left().IsWord32Or() && m.right().Is(0)) {
        // (x | K) < 0 => true or (K | x) < 0 => true iff K < 0
        Int32BinopMatcher mleftmatcher(m.left().node());
        if (mleftmatcher.left().IsNegative() ||
            mleftmatcher.right().IsNegative()) {
          return ReplaceBool(true);
        }
      }
      break;
    }
    case IrOpcode::kInt32LessThanOrEqual: {
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {  // K <= K => K
        return ReplaceBool(m.left().Value() <= m.right().Value());
      }
      if (m.LeftEqualsRight()) return ReplaceBool(true);  // x <= x => true
      break;
    }
    case IrOpcode::kUint32LessThan: {
      Uint32BinopMatcher m(node);
      if (m.left().Is(kMaxUInt32)) return ReplaceBool(false);  // M < x => false
      if (m.right().Is(0)) return ReplaceBool(false);          // x < 0 => false
      if (m.IsFoldable()) {                                    // K < K => K
        return ReplaceBool(m.left().Value() < m.right().Value());
      }
      if (m.LeftEqualsRight()) return ReplaceBool(false);  // x < x => false
      if (m.left().IsWord32Sar() && m.right().HasValue()) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().HasValue()) {
          // (x >> K) < C => x < (C << K)
          // when C < (M >> K)
          const uint32_t c = m.right().Value();
          const uint32_t k = mleft.right().Value() & 0x1F;
          if (c < static_cast<uint32_t>(kMaxInt >> k)) {
            node->ReplaceInput(0, mleft.left().node());
            node->ReplaceInput(1, Uint32Constant(c << k));
            return Changed(node);
          }
          // TODO(turbofan): else the comparison is always true.
        }
      }
      break;
    }
    case IrOpcode::kUint32LessThanOrEqual: {
      Uint32BinopMatcher m(node);
      if (m.left().Is(0)) return ReplaceBool(true);            // 0 <= x => true
      if (m.right().Is(kMaxUInt32)) return ReplaceBool(true);  // x <= M => true
      if (m.IsFoldable()) {                                    // K <= K => K
        return ReplaceBool(m.left().Value() <= m.right().Value());
      }
      if (m.LeftEqualsRight()) return ReplaceBool(true);  // x <= x => true
      break;
    }
    case IrOpcode::kFloat32Sub: {
      Float32BinopMatcher m(node);
      if (allow_signalling_nan_ && m.right().Is(0) &&
          (std::copysign(1.0, m.right().Value()) > 0)) {
        return Replace(m.left().node());  // x - 0 => x
      }
      if (m.right().IsNaN()) {  // x - NaN => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat32(m.right().Value() - m.right().Value());
      }
      if (m.left().IsNaN()) {  // NaN - x => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat32(m.left().Value() - m.left().Value());
      }
      if (m.IsFoldable()) {  // L - R => (L - R)
        return ReplaceFloat32(m.left().Value() - m.right().Value());
      }
      if (allow_signalling_nan_ && m.left().IsMinusZero()) {
        // -0.0 - round_down(-0.0 - R) => round_up(R)
        if (machine()->Float32RoundUp().IsSupported() &&
            m.right().IsFloat32RoundDown()) {
          if (m.right().InputAt(0)->opcode() == IrOpcode::kFloat32Sub) {
            Float32BinopMatcher mright0(m.right().InputAt(0));
            if (mright0.left().IsMinusZero()) {
              return Replace(graph()->NewNode(machine()->Float32RoundUp().op(),
                                              mright0.right().node()));
            }
          }
        }
        // -0.0 - R => -R
        node->RemoveInput(0);
        NodeProperties::ChangeOp(node, machine()->Float32Neg());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kFloat64Add: {
      Float64BinopMatcher m(node);
      if (m.IsFoldable()) {  // K + K => K
        return ReplaceFloat64(m.left().Value() + m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Sub: {
      Float64BinopMatcher m(node);
      if (allow_signalling_nan_ && m.right().Is(0) &&
          (Double(m.right().Value()).Sign() > 0)) {
        return Replace(m.left().node());  // x - 0 => x
      }
      if (m.right().IsNaN()) {  // x - NaN => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat64(m.right().Value() - m.right().Value());
      }
      if (m.left().IsNaN()) {  // NaN - x => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat64(m.left().Value() - m.left().Value());
      }
      if (m.IsFoldable()) {  // L - R => (L - R)
        return ReplaceFloat64(m.left().Value() - m.right().Value());
      }
      if (allow_signalling_nan_ && m.left().IsMinusZero()) {
        // -0.0 - round_down(-0.0 - R) => round_up(R)
        if (machine()->Float64RoundUp().IsSupported() &&
            m.right().IsFloat64RoundDown()) {
          if (m.right().InputAt(0)->opcode() == IrOpcode::kFloat64Sub) {
            Float64BinopMatcher mright0(m.right().InputAt(0));
            if (mright0.left().IsMinusZero()) {
              return Replace(graph()->NewNode(machine()->Float64RoundUp().op(),
                                              mright0.right().node()));
            }
          }
        }
        // -0.0 - R => -R
        node->RemoveInput(0);
        NodeProperties::ChangeOp(node, machine()->Float64Neg());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kFloat64Mul: {
      Float64BinopMatcher m(node);
      if (allow_signalling_nan_ && m.right().Is(1))
        return Replace(m.left().node());  // x * 1.0 => x
      if (m.right().Is(-1)) {  // x * -1.0 => -0.0 - x
        node->ReplaceInput(0, Float64Constant(-0.0));
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Float64Sub());
        return Changed(node);
      }
      if (m.right().IsNaN()) {                               // x * NaN => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat64(m.right().Value() - m.right().Value());
      }
      if (m.IsFoldable()) {  // K * K => K
        return ReplaceFloat64(m.left().Value() * m.right().Value());
      }
      if (m.right().Is(2)) {  // x * 2.0 => x + x
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Float64Add());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kFloat64Div: {
      Float64BinopMatcher m(node);
      if (allow_signalling_nan_ && m.right().Is(1))
        return Replace(m.left().node());  // x / 1.0 => x
      // TODO(ahaas): We could do x / 1.0 = x if we knew that x is not an sNaN.
      if (m.right().IsNaN()) {                               // x / NaN => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat64(m.right().Value() - m.right().Value());
      }
      if (m.left().IsNaN()) {  // NaN / x => NaN
        // Do some calculation to make a signalling NaN quiet.
        return ReplaceFloat64(m.left().Value() - m.left().Value());
      }
      if (m.IsFoldable()) {  // K / K => K
        return ReplaceFloat64(
            base::Divide(m.left().Value(), m.right().Value()));
      }
      if (allow_signalling_nan_ && m.right().Is(-1)) {  // x / -1.0 => -x
        node->RemoveInput(1);
        NodeProperties::ChangeOp(node, machine()->Float64Neg());
        return Changed(node);
      }
      if (m.right().IsNormal() && m.right().IsPositiveOrNegativePowerOf2()) {
        // All reciprocals of non-denormal powers of two can be represented
        // exactly, so division by power of two can be reduced to
        // multiplication by reciprocal, with the same result.
        node->ReplaceInput(1, Float64Constant(1.0 / m.right().Value()));
        NodeProperties::ChangeOp(node, machine()->Float64Mul());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kFloat64Mod: {
      Float64BinopMatcher m(node);
      if (m.right().Is(0)) {  // x % 0 => NaN
        return ReplaceFloat64(std::numeric_limits<double>::quiet_NaN());
      }
      if (m.right().IsNaN()) {  // x % NaN => NaN
        return Replace(m.right().node());
      }
      if (m.left().IsNaN()) {  // NaN % x => NaN
        return Replace(m.left().node());
      }
      if (m.IsFoldable()) {  // K % K => K
        return ReplaceFloat64(Modulo(m.left().Value(), m.right().Value()));
      }
      break;
    }
    case IrOpcode::kFloat64Acos: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::acos(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Acosh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::acosh(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Asin: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::asin(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Asinh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::asinh(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Atan: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::atan(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Atanh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::atanh(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Atan2: {
      Float64BinopMatcher m(node);
      if (m.right().IsNaN()) {
        return Replace(m.right().node());
      }
      if (m.left().IsNaN()) {
        return Replace(m.left().node());
      }
      if (m.IsFoldable()) {
        return ReplaceFloat64(
            base::ieee754::atan2(m.left().Value(), m.right().Value()));
      }
      break;
    }
    case IrOpcode::kFloat64Cbrt: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::cbrt(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Cos: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::cos(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Cosh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::cosh(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Exp: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::exp(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Expm1: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::expm1(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Log: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::log(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Log1p: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::log1p(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Log10: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::log10(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Log2: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::log2(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Pow: {
      Float64BinopMatcher m(node);
      if (m.IsFoldable()) {
        return ReplaceFloat64(
            base::ieee754::pow(m.left().Value(), m.right().Value()));
      } else if (m.right().Is(0.0)) {  // x ** +-0.0 => 1.0
        return ReplaceFloat64(1.0);
      } else if (m.right().Is(-2.0)) {  // x ** -2.0 => 1 / (x * x)
        node->ReplaceInput(0, Float64Constant(1.0));
        node->ReplaceInput(1, Float64Mul(m.left().node(), m.left().node()));
        NodeProperties::ChangeOp(node, machine()->Float64Div());
        return Changed(node);
      } else if (m.right().Is(2.0)) {  // x ** 2.0 => x * x
        node->ReplaceInput(1, m.left().node());
        NodeProperties::ChangeOp(node, machine()->Float64Mul());
        return Changed(node);
      } else if (m.right().Is(-0.5)) {
        // x ** 0.5 => 1 / (if x <= -Infinity then Infinity else sqrt(0.0 + x))
        node->ReplaceInput(0, Float64Constant(1.0));
        node->ReplaceInput(1, Float64PowHalf(m.left().node()));
        NodeProperties::ChangeOp(node, machine()->Float64Div());
        return Changed(node);
      } else if (m.right().Is(0.5)) {
        // x ** 0.5 => if x <= -Infinity then Infinity else sqrt(0.0 + x)
        return Replace(Float64PowHalf(m.left().node()));
      }
      break;
    }
    case IrOpcode::kFloat64Sin: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::sin(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Sinh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::sinh(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Tan: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::tan(m.Value()));
      break;
    }
    case IrOpcode::kFloat64Tanh: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(base::ieee754::tanh(m.Value()));
      break;
    }
    case IrOpcode::kChangeFloat32ToFloat64: {
      Float32Matcher m(node->InputAt(0));
      if (m.HasValue()) {
        if (!allow_signalling_nan_ && std::isnan(m.Value())) {
          // Do some calculation to make guarantee the value is a quiet NaN.
          return ReplaceFloat64(m.Value() + m.Value());
        }
        return ReplaceFloat64(m.Value());
      }
      break;
    }
    case IrOpcode::kChangeFloat64ToInt32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(FastD2IChecked(m.Value()));
      if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeFloat64ToInt64: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt64(static_cast<int64_t>(m.Value()));
      if (m.IsChangeInt64ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeFloat64ToUint32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(FastD2UI(m.Value()));
      if (m.IsChangeUint32ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeInt32ToFloat64: {
      Int32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(FastI2D(m.Value()));
      break;
    }
    case IrOpcode::kChangeInt32ToInt64: {
      Int32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt64(m.Value());
      break;
    }
    case IrOpcode::kChangeInt64ToFloat64: {
      Int64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(static_cast<double>(m.Value()));
      if (m.IsChangeFloat64ToInt64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeUint32ToFloat64: {
      Uint32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(FastUI2D(m.Value()));
      break;
    }
    case IrOpcode::kChangeUint32ToUint64: {
      Uint32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt64(static_cast<uint64_t>(m.Value()));
      break;
    }
    case IrOpcode::kTruncateFloat64ToWord32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(DoubleToInt32(m.Value()));
      if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
      return NoChange();
    }
    case IrOpcode::kTruncateInt64ToInt32: {
      Int64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(static_cast<int32_t>(m.Value()));
      if (m.IsChangeInt32ToInt64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kTruncateFloat64ToFloat32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) {
        if (!allow_signalling_nan_ && std::isnan(m.Value())) {
          // Do some calculation to make guarantee the value is a quiet NaN.
          return ReplaceFloat32(DoubleToFloat32(m.Value() + m.Value()));
        }
        return ReplaceFloat32(DoubleToFloat32(m.Value()));
      }
      if (allow_signalling_nan_ && m.IsChangeFloat32ToFloat64())
        return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kRoundFloat64ToInt32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) {
        return ReplaceInt32(DoubleToInt32(m.Value()));
      }
      if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kFloat64InsertLowWord32:
      return ReduceFloat64InsertLowWord32(node);
    case IrOpcode::kFloat64InsertHighWord32:
      return ReduceFloat64InsertHighWord32(node);
    case IrOpcode::kStore:
    case IrOpcode::kUnalignedStore:
      return ReduceStore(node);
    case IrOpcode::kFloat64Equal:
    case IrOpcode::kFloat64LessThan:
    case IrOpcode::kFloat64LessThanOrEqual:
      return ReduceFloat64Compare(node);
    case IrOpcode::kFloat64RoundDown:
      return ReduceFloat64RoundDown(node);
    case IrOpcode::kBitcastTaggedToWord: {
      NodeMatcher m(node->InputAt(0));
      if (m.IsBitcastWordToTaggedSigned()) {
        RelaxEffectsAndControls(node);
        return Replace(m.InputAt(0));
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceInt32Add(Node* node) {
  DCHECK_EQ(IrOpcode::kInt32Add, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x + 0 => x
  if (m.IsFoldable()) {                                  // K + K => K
    return ReplaceInt32(
        base::AddWithWraparound(m.left().Value(), m.right().Value()));
  }
  if (m.left().IsInt32Sub()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.left().Is(0)) {  // (0 - x) + y => y - x
      node->ReplaceInput(0, m.right().node());
      node->ReplaceInput(1, mleft.right().node());
      NodeProperties::ChangeOp(node, machine()->Int32Sub());
      Reduction const reduction = ReduceInt32Sub(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  if (m.right().IsInt32Sub()) {
    Int32BinopMatcher mright(m.right().node());
    if (mright.left().Is(0)) {  // y + (0 - x) => y - x
      node->ReplaceInput(1, mright.right().node());
      NodeProperties::ChangeOp(node, machine()->Int32Sub());
      Reduction const reduction = ReduceInt32Sub(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  // (x + Int32Constant(a)) + Int32Constant(b)) => x + Int32Constant(a + b)
  if (m.right().HasValue() && m.left().IsInt32Add()) {
    Int32BinopMatcher n(m.left().node());
    if (n.right().HasValue() && m.OwnsInput(m.left().node())) {
      node->ReplaceInput(1, Int32Constant(base::AddWithWraparound(
                                m.right().Value(), n.right().Value())));
      node->ReplaceInput(0, n.left().node());
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceInt64Add(Node* node) {
  DCHECK_EQ(IrOpcode::kInt64Add, node->opcode());
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x + 0 => 0
  if (m.IsFoldable()) {
    return ReplaceInt64(
        base::AddWithWraparound(m.left().Value(), m.right().Value()));
  }
  // (x + Int64Constant(a)) + Int64Constant(b)) => x + Int64Constant(a + b)
  if (m.right().HasValue() && m.left().IsInt64Add()) {
    Int64BinopMatcher n(m.left().node());
    if (n.right().HasValue() && m.OwnsInput(m.left().node())) {
      node->ReplaceInput(1, Int64Constant(base::AddWithWraparound(
                                m.right().Value(), n.right().Value())));
      node->ReplaceInput(0, n.left().node());
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceInt32Sub(Node* node) {
  DCHECK_EQ(IrOpcode::kInt32Sub, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x - 0 => x
  if (m.IsFoldable()) {                                  // K - K => K
    return ReplaceInt32(
        base::SubWithWraparound(m.left().Value(), m.right().Value()));
  }
  if (m.LeftEqualsRight()) return ReplaceInt32(0);  // x - x => 0
  if (m.right().HasValue()) {                       // x - K => x + -K
    node->ReplaceInput(
        1, Int32Constant(base::NegateWithWraparound(m.right().Value())));
    NodeProperties::ChangeOp(node, machine()->Int32Add());
    Reduction const reduction = ReduceInt32Add(node);
    return reduction.Changed() ? reduction : Changed(node);
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceInt64Sub(Node* node) {
  DCHECK_EQ(IrOpcode::kInt64Sub, node->opcode());
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x - 0 => x
  if (m.IsFoldable()) {                                  // K - K => K
    return ReplaceInt64(
        base::SubWithWraparound(m.left().Value(), m.right().Value()));
  }
  if (m.LeftEqualsRight()) return Replace(Int64Constant(0));  // x - x => 0
  if (m.right().HasValue()) {                                 // x - K => x + -K
    node->ReplaceInput(
        1, Int64Constant(base::NegateWithWraparound(m.right().Value())));
    NodeProperties::ChangeOp(node, machine()->Int64Add());
    Reduction const reduction = ReduceInt64Add(node);
    return reduction.Changed() ? reduction : Changed(node);
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceInt32Div(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) return Replace(m.left().node());    // 0 / x => 0
  if (m.right().Is(0)) return Replace(m.right().node());  // x / 0 => 0
  if (m.right().Is(1)) return Replace(m.left().node());   // x / 1 => x
  if (m.IsFoldable()) {                                   // K / K => K
    return ReplaceInt32(
        base::bits::SignedDiv32(m.left().Value(), m.right().Value()));
  }
  if (m.LeftEqualsRight()) {  // x / x => x != 0
    Node* const zero = Int32Constant(0);
    return Replace(Word32Equal(Word32Equal(m.left().node(), zero), zero));
  }
  if (m.right().Is(-1)) {  // x / -1 => 0 - x
    node->ReplaceInput(0, Int32Constant(0));
    node->ReplaceInput(1, m.left().node());
    node->TrimInputCount(2);
    NodeProperties::ChangeOp(node, machine()->Int32Sub());
    return Changed(node);
  }
  if (m.right().HasValue()) {
    int32_t const divisor = m.right().Value();
    Node* const dividend = m.left().node();
    Node* quotient = dividend;
    if (base::bits::IsPowerOfTwo(Abs(divisor))) {
      uint32_t const shift = WhichPowerOf2(Abs(divisor));
      DCHECK_NE(0u, shift);
      if (shift > 1) {
        quotient = Word32Sar(quotient, 31);
      }
      quotient = Int32Add(Word32Shr(quotient, 32u - shift), dividend);
      quotient = Word32Sar(quotient, shift);
    } else {
      quotient = Int32Div(quotient, Abs(divisor));
    }
    if (divisor < 0) {
      node->ReplaceInput(0, Int32Constant(0));
      node->ReplaceInput(1, quotient);
      node->TrimInputCount(2);
      NodeProperties::ChangeOp(node, machine()->Int32Sub());
      return Changed(node);
    }
    return Replace(quotient);
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceUint32Div(Node* node) {
  Uint32BinopMatcher m(node);
  if (m.left().Is(0)) return Replace(m.left().node());    // 0 / x => 0
  if (m.right().Is(0)) return Replace(m.right().node());  // x / 0 => 0
  if (m.right().Is(1)) return Replace(m.left().node());   // x / 1 => x
  if (m.IsFoldable()) {                                   // K / K => K
    return ReplaceUint32(
        base::bits::UnsignedDiv32(m.left().Value(), m.right().Value()));
  }
  if (m.LeftEqualsRight()) {  // x / x => x != 0
    Node* const zero = Int32Constant(0);
    return Replace(Word32Equal(Word32Equal(m.left().node(), zero), zero));
  }
  if (m.right().HasValue()) {
    Node* const dividend = m.left().node();
    uint32_t const divisor = m.right().Value();
    if (base::bits::IsPowerOfTwo(divisor)) {  // x / 2^n => x >> n
      node->ReplaceInput(1, Uint32Constant(WhichPowerOf2(m.right().Value())));
      node->TrimInputCount(2);
      NodeProperties::ChangeOp(node, machine()->Word32Shr());
      return Changed(node);
    } else {
      return Replace(Uint32Div(dividend, divisor));
    }
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceInt32Mod(Node* node) {
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) return Replace(m.left().node());    // 0 % x  => 0
  if (m.right().Is(0)) return Replace(m.right().node());  // x % 0  => 0
  if (m.right().Is(1)) return ReplaceInt32(0);            // x % 1  => 0
  if (m.right().Is(-1)) return ReplaceInt32(0);           // x % -1 => 0
  if (m.LeftEqualsRight()) return ReplaceInt32(0);        // x % x  => 0
  if (m.IsFoldable()) {                                   // K % K => K
    return ReplaceInt32(
        base::bits::SignedMod32(m.left().Value(), m.right().Value()));
  }
  if (m.right().HasValue()) {
    Node* const dividend = m.left().node();
    uint32_t const divisor = Abs(m.right().Value());
    if (base::bits::IsPowerOfTwo(divisor)) {
      uint32_t const mask = divisor - 1;
      Node* const zero = Int32Constant(0);
      Diamond d(graph(), common(),
                graph()->NewNode(machine()->Int32LessThan(), dividend, zero),
                BranchHint::kFalse);
      return Replace(
          d.Phi(MachineRepresentation::kWord32,
                Int32Sub(zero, Word32And(Int32Sub(zero, dividend), mask)),
                Word32And(dividend, mask)));
    } else {
      Node* quotient = Int32Div(dividend, divisor);
      DCHECK_EQ(dividend, node->InputAt(0));
      node->ReplaceInput(1, Int32Mul(quotient, Int32Constant(divisor)));
      node->TrimInputCount(2);
      NodeProperties::ChangeOp(node, machine()->Int32Sub());
    }
    return Changed(node);
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceUint32Mod(Node* node) {
  Uint32BinopMatcher m(node);
  if (m.left().Is(0)) return Replace(m.left().node());    // 0 % x => 0
  if (m.right().Is(0)) return Replace(m.right().node());  // x % 0 => 0
  if (m.right().Is(1)) return ReplaceUint32(0);           // x % 1 => 0
  if (m.LeftEqualsRight()) return ReplaceInt32(0);        // x % x  => 0
  if (m.IsFoldable()) {                                   // K % K => K
    return ReplaceUint32(
        base::bits::UnsignedMod32(m.left().Value(), m.right().Value()));
  }
  if (m.right().HasValue()) {
    Node* const dividend = m.left().node();
    uint32_t const divisor = m.right().Value();
    if (base::bits::IsPowerOfTwo(divisor)) {  // x % 2^n => x & 2^n-1
      node->ReplaceInput(1, Uint32Constant(m.right().Value() - 1));
      node->TrimInputCount(2);
      NodeProperties::ChangeOp(node, machine()->Word32And());
    } else {
      Node* quotient = Uint32Div(dividend, divisor);
      DCHECK_EQ(dividend, node->InputAt(0));
      node->ReplaceInput(1, Int32Mul(quotient, Uint32Constant(divisor)));
      node->TrimInputCount(2);
      NodeProperties::ChangeOp(node, machine()->Int32Sub());
    }
    return Changed(node);
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceStore(Node* node) {
  NodeMatcher nm(node);
  MachineRepresentation rep;
  int value_input;
  if (nm.IsStore()) {
    rep = StoreRepresentationOf(node->op()).representation();
    value_input = 2;
  } else {
    DCHECK(nm.IsUnalignedStore());
    rep = UnalignedStoreRepresentationOf(node->op());
    value_input = 2;
  }

  Node* const value = node->InputAt(value_input);

  switch (value->opcode()) {
    case IrOpcode::kWord32And: {
      Uint32BinopMatcher m(value);
      if (m.right().HasValue() && ((rep == MachineRepresentation::kWord8 &&
                                    (m.right().Value() & 0xFF) == 0xFF) ||
                                   (rep == MachineRepresentation::kWord16 &&
                                    (m.right().Value() & 0xFFFF) == 0xFFFF))) {
        node->ReplaceInput(value_input, m.left().node());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kWord32Sar: {
      Int32BinopMatcher m(value);
      if (m.left().IsWord32Shl() && ((rep == MachineRepresentation::kWord8 &&
                                      m.right().IsInRange(1, 24)) ||
                                     (rep == MachineRepresentation::kWord16 &&
                                      m.right().IsInRange(1, 16)))) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().Is(m.right().Value())) {
          node->ReplaceInput(value_input, mleft.left().node());
          return Changed(node);
        }
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceProjection(size_t index, Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kInt32AddWithOverflow: {
      DCHECK(index == 0 || index == 1);
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {
        int32_t val;
        bool ovf = base::bits::SignedAddOverflow32(m.left().Value(),
                                                   m.right().Value(), &val);
        return ReplaceInt32(index == 0 ? val : ovf);
      }
      if (m.right().Is(0)) {
        return Replace(index == 0 ? m.left().node() : m.right().node());
      }
      break;
    }
    case IrOpcode::kInt32SubWithOverflow: {
      DCHECK(index == 0 || index == 1);
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {
        int32_t val;
        bool ovf = base::bits::SignedSubOverflow32(m.left().Value(),
                                                   m.right().Value(), &val);
        return ReplaceInt32(index == 0 ? val : ovf);
      }
      if (m.right().Is(0)) {
        return Replace(index == 0 ? m.left().node() : m.right().node());
      }
      break;
    }
    case IrOpcode::kInt32MulWithOverflow: {
      DCHECK(index == 0 || index == 1);
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {
        int32_t val;
        bool ovf = base::bits::SignedMulOverflow32(m.left().Value(),
                                                   m.right().Value(), &val);
        return ReplaceInt32(index == 0 ? val : ovf);
      }
      if (m.right().Is(0)) {
        return Replace(m.right().node());
      }
      if (m.right().Is(1)) {
        return index == 0 ? Replace(m.left().node()) : ReplaceInt32(0);
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceWord32Shifts(Node* node) {
  DCHECK((node->opcode() == IrOpcode::kWord32Shl) ||
         (node->opcode() == IrOpcode::kWord32Shr) ||
         (node->opcode() == IrOpcode::kWord32Sar));
  if (machine()->Word32ShiftIsSafe()) {
    // Remove the explicit 'and' with 0x1F if the shift provided by the machine
    // instruction matches that required by JavaScript.
    Int32BinopMatcher m(node);
    if (m.right().IsWord32And()) {
      Int32BinopMatcher mright(m.right().node());
      if (mright.right().Is(0x1F)) {
        node->ReplaceInput(1, mright.left().node());
        return Changed(node);
      }
    }
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceWord32Shl(Node* node) {
  DCHECK_EQ(IrOpcode::kWord32Shl, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x << 0 => x
  if (m.IsFoldable()) {                                  // K << K => K
    return ReplaceInt32(
        base::ShlWithWraparound(m.left().Value(), m.right().Value()));
  }
  if (m.right().IsInRange(1, 31)) {
    // (x >>> K) << K => x & ~(2^K - 1)
    // (x >> K) << K => x & ~(2^K - 1)
    if (m.left().IsWord32Sar() || m.left().IsWord32Shr()) {
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().Is(m.right().Value())) {
        node->ReplaceInput(0, mleft.left().node());
        node->ReplaceInput(1,
                           Uint32Constant(~((1U << m.right().Value()) - 1U)));
        NodeProperties::ChangeOp(node, machine()->Word32And());
        Reduction reduction = ReduceWord32And(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
    }
  }
  return ReduceWord32Shifts(node);
}

Reduction MachineOperatorReducer::ReduceWord64Shl(Node* node) {
  DCHECK_EQ(IrOpcode::kWord64Shl, node->opcode());
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x << 0 => x
  if (m.IsFoldable()) {                                  // K << K => K
    return ReplaceInt64(
        base::ShlWithWraparound(m.left().Value(), m.right().Value()));
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceWord32Shr(Node* node) {
  Uint32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x >>> 0 => x
  if (m.IsFoldable()) {                                  // K >>> K => K
    return ReplaceInt32(m.left().Value() >> (m.right().Value() & 31));
  }
  if (m.left().IsWord32And() && m.right().HasValue()) {
    Uint32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t shift = m.right().Value() & 31;
      uint32_t mask = mleft.right().Value();
      if ((mask >> shift) == 0) {
        // (m >>> s) == 0 implies ((x & m) >>> s) == 0
        return ReplaceInt32(0);
      }
    }
  }
  return ReduceWord32Shifts(node);
}

Reduction MachineOperatorReducer::ReduceWord64Shr(Node* node) {
  DCHECK_EQ(IrOpcode::kWord64Shr, node->opcode());
  Uint64BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x >>> 0 => x
  if (m.IsFoldable()) {                                  // K >> K => K
    return ReplaceInt64(m.left().Value() >> (m.right().Value() & 63));
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceWord32Sar(Node* node) {
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x >> 0 => x
  if (m.IsFoldable()) {                                  // K >> K => K
    return ReplaceInt32(m.left().Value() >> (m.right().Value() & 31));
  }
  if (m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.left().IsComparison()) {
      if (m.right().Is(31) && mleft.right().Is(31)) {
        // Comparison << 31 >> 31 => 0 - Comparison
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, mleft.left().node());
        NodeProperties::ChangeOp(node, machine()->Int32Sub());
        Reduction const reduction = ReduceInt32Sub(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
    } else if (mleft.left().IsLoad()) {
      LoadRepresentation const rep =
          LoadRepresentationOf(mleft.left().node()->op());
      if (m.right().Is(24) && mleft.right().Is(24) &&
          rep == MachineType::Int8()) {
        // Load[kMachInt8] << 24 >> 24 => Load[kMachInt8]
        return Replace(mleft.left().node());
      }
      if (m.right().Is(16) && mleft.right().Is(16) &&
          rep == MachineType::Int16()) {
        // Load[kMachInt16] << 16 >> 16 => Load[kMachInt8]
        return Replace(mleft.left().node());
      }
    }
  }
  return ReduceWord32Shifts(node);
}

Reduction MachineOperatorReducer::ReduceWord64Sar(Node* node) {
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x >> 0 => x
  if (m.IsFoldable()) {
    return ReplaceInt64(m.left().Value() >> (m.right().Value() & 63));
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceWord32And(Node* node) {
  DCHECK_EQ(IrOpcode::kWord32And, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.right().node());  // x & 0  => 0
  if (m.right().Is(-1)) return Replace(m.left().node());  // x & -1 => x
  if (m.left().IsComparison() && m.right().Is(1)) {       // CMP & 1 => CMP
    return Replace(m.left().node());
  }
  if (m.IsFoldable()) {                                   // K & K  => K
    return ReplaceInt32(m.left().Value() & m.right().Value());
  }
  if (m.LeftEqualsRight()) return Replace(m.left().node());  // x & x => x
  if (m.left().IsWord32And() && m.right().HasValue()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {  // (x & K) & K => x & K
      node->ReplaceInput(0, mleft.left().node());
      node->ReplaceInput(
          1, Int32Constant(m.right().Value() & mleft.right().Value()));
      Reduction const reduction = ReduceWord32And(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  if (m.right().IsNegativePowerOf2()) {
    int32_t const mask = m.right().Value();
    int32_t const neg_mask = base::NegateWithWraparound(mask);
    if (m.left().IsWord32Shl()) {
      Uint32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue() &&
          (mleft.right().Value() & 0x1F) >=
              base::bits::CountTrailingZeros(mask)) {
        // (x << L) & (-1 << K) => x << L iff L >= K
        return Replace(mleft.node());
      }
    } else if (m.left().IsInt32Add()) {
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue() &&
          (mleft.right().Value() & mask) == mleft.right().Value()) {
        // (x + (K << L)) & (-1 << L) => (x & (-1 << L)) + (K << L)
        node->ReplaceInput(0, Word32And(mleft.left().node(), m.right().node()));
        node->ReplaceInput(1, mleft.right().node());
        NodeProperties::ChangeOp(node, machine()->Int32Add());
        Reduction const reduction = ReduceInt32Add(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
      if (mleft.left().IsInt32Mul()) {
        Int32BinopMatcher mleftleft(mleft.left().node());
        if (mleftleft.right().IsMultipleOf(neg_mask)) {
          // (y * (K << L) + x) & (-1 << L) => (x & (-1 << L)) + y * (K << L)
          node->ReplaceInput(0,
                             Word32And(mleft.right().node(), m.right().node()));
          node->ReplaceInput(1, mleftleft.node());
          NodeProperties::ChangeOp(node, machine()->Int32Add());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.right().IsInt32Mul()) {
        Int32BinopMatcher mleftright(mleft.right().node());
        if (mleftright.right().IsMultipleOf(neg_mask)) {
          // (x + y * (K << L)) & (-1 << L) => (x & (-1 << L)) + y * (K << L)
          node->ReplaceInput(0,
                             Word32And(mleft.left().node(), m.right().node()));
          node->ReplaceInput(1, mleftright.node());
          NodeProperties::ChangeOp(node, machine()->Int32Add());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.left().IsWord32Shl()) {
        Int32BinopMatcher mleftleft(mleft.left().node());
        if (mleftleft.right().Is(base::bits::CountTrailingZeros(mask))) {
          // (y << L + x) & (-1 << L) => (x & (-1 << L)) + y << L
          node->ReplaceInput(0,
                             Word32And(mleft.right().node(), m.right().node()));
          node->ReplaceInput(1, mleftleft.node());
          NodeProperties::ChangeOp(node, machine()->Int32Add());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.right().IsWord32Shl()) {
        Int32BinopMatcher mleftright(mleft.right().node());
        if (mleftright.right().Is(base::bits::CountTrailingZeros(mask))) {
          // (x + y << L) & (-1 << L) => (x & (-1 << L)) + y << L
          node->ReplaceInput(0,
                             Word32And(mleft.left().node(), m.right().node()));
          node->ReplaceInput(1, mleftright.node());
          NodeProperties::ChangeOp(node, machine()->Int32Add());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
    } else if (m.left().IsInt32Mul()) {
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().IsMultipleOf(neg_mask)) {
        // (x * (K << L)) & (-1 << L) => x * (K << L)
        return Replace(mleft.node());
      }
    }
  }
  return NoChange();
}

Reduction MachineOperatorReducer::TryMatchWord32Ror(Node* node) {
  DCHECK(IrOpcode::kWord32Or == node->opcode() ||
         IrOpcode::kWord32Xor == node->opcode());
  Int32BinopMatcher m(node);
  Node* shl = nullptr;
  Node* shr = nullptr;
  // Recognize rotation, we are matching:
  //  * x << y | x >>> (32 - y) => x ror (32 - y), i.e  x rol y
  //  * x << (32 - y) | x >>> y => x ror y
  //  * x << y ^ x >>> (32 - y) => x ror (32 - y), i.e. x rol y
  //  * x << (32 - y) ^ x >>> y => x ror y
  // as well as their commuted form.
  if (m.left().IsWord32Shl() && m.right().IsWord32Shr()) {
    shl = m.left().node();
    shr = m.right().node();
  } else if (m.left().IsWord32Shr() && m.right().IsWord32Shl()) {
    shl = m.right().node();
    shr = m.left().node();
  } else {
    return NoChange();
  }

  Int32BinopMatcher mshl(shl);
  Int32BinopMatcher mshr(shr);
  if (mshl.left().node() != mshr.left().node()) return NoChange();

  if (mshl.right().HasValue() && mshr.right().HasValue()) {
    // Case where y is a constant.
    if (mshl.right().Value() + mshr.right().Value() != 32) return NoChange();
  } else {
    Node* sub = nullptr;
    Node* y = nullptr;
    if (mshl.right().IsInt32Sub()) {
      sub = mshl.right().node();
      y = mshr.right().node();
    } else if (mshr.right().IsInt32Sub()) {
      sub = mshr.right().node();
      y = mshl.right().node();
    } else {
      return NoChange();
    }

    Int32BinopMatcher msub(sub);
    if (!msub.left().Is(32) || msub.right().node() != y) return NoChange();
  }

  node->ReplaceInput(0, mshl.left().node());
  node->ReplaceInput(1, mshr.right().node());
  NodeProperties::ChangeOp(node, machine()->Word32Ror());
  return Changed(node);
}

Reduction MachineOperatorReducer::ReduceWord32Or(Node* node) {
  DCHECK_EQ(IrOpcode::kWord32Or, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());    // x | 0  => x
  if (m.right().Is(-1)) return Replace(m.right().node());  // x | -1 => -1
  if (m.IsFoldable()) {                                    // K | K  => K
    return ReplaceInt32(m.left().Value() | m.right().Value());
  }
  if (m.LeftEqualsRight()) return Replace(m.left().node());  // x | x => x

  return TryMatchWord32Ror(node);
}

Reduction MachineOperatorReducer::ReduceWord32Xor(Node* node) {
  DCHECK_EQ(IrOpcode::kWord32Xor, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x ^ 0 => x
  if (m.IsFoldable()) {                                  // K ^ K => K
    return ReplaceInt32(m.left().Value() ^ m.right().Value());
  }
  if (m.LeftEqualsRight()) return ReplaceInt32(0);  // x ^ x => 0
  if (m.left().IsWord32Xor() && m.right().Is(-1)) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(-1)) {  // (x ^ -1) ^ -1 => x
      return Replace(mleft.left().node());
    }
  }

  return TryMatchWord32Ror(node);
}

Reduction MachineOperatorReducer::ReduceFloat64InsertLowWord32(Node* node) {
  DCHECK_EQ(IrOpcode::kFloat64InsertLowWord32, node->opcode());
  Float64Matcher mlhs(node->InputAt(0));
  Uint32Matcher mrhs(node->InputAt(1));
  if (mlhs.HasValue() && mrhs.HasValue()) {
    return ReplaceFloat64(bit_cast<double>(
        (bit_cast<uint64_t>(mlhs.Value()) & uint64_t{0xFFFFFFFF00000000}) |
        mrhs.Value()));
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceFloat64InsertHighWord32(Node* node) {
  DCHECK_EQ(IrOpcode::kFloat64InsertHighWord32, node->opcode());
  Float64Matcher mlhs(node->InputAt(0));
  Uint32Matcher mrhs(node->InputAt(1));
  if (mlhs.HasValue() && mrhs.HasValue()) {
    return ReplaceFloat64(bit_cast<double>(
        (bit_cast<uint64_t>(mlhs.Value()) & uint64_t{0xFFFFFFFF}) |
        (static_cast<uint64_t>(mrhs.Value()) << 32)));
  }
  return NoChange();
}


namespace {

bool IsFloat64RepresentableAsFloat32(const Float64Matcher& m) {
  if (m.HasValue()) {
    double v = m.Value();
    return DoubleToFloat32(v) == v;
  }
  return false;
}

}  // namespace


Reduction MachineOperatorReducer::ReduceFloat64Compare(Node* node) {
  DCHECK(IrOpcode::kFloat64Equal == node->opcode() ||
         IrOpcode::kFloat64LessThan == node->opcode() ||
         IrOpcode::kFloat64LessThanOrEqual == node->opcode());
  Float64BinopMatcher m(node);
  if (m.IsFoldable()) {
    switch (node->opcode()) {
      case IrOpcode::kFloat64Equal:
        return ReplaceBool(m.left().Value() == m.right().Value());
      case IrOpcode::kFloat64LessThan:
        return ReplaceBool(m.left().Value() < m.right().Value());
      case IrOpcode::kFloat64LessThanOrEqual:
        return ReplaceBool(m.left().Value() <= m.right().Value());
      default:
        UNREACHABLE();
    }
  } else if ((m.left().IsChangeFloat32ToFloat64() &&
              m.right().IsChangeFloat32ToFloat64()) ||
             (m.left().IsChangeFloat32ToFloat64() &&
              IsFloat64RepresentableAsFloat32(m.right())) ||
             (IsFloat64RepresentableAsFloat32(m.left()) &&
              m.right().IsChangeFloat32ToFloat64())) {
    // As all Float32 values have an exact representation in Float64, comparing
    // two Float64 values both converted from Float32 is equivalent to comparing
    // the original Float32s, so we can ignore the conversions. We can also
    // reduce comparisons of converted Float64 values against constants that
    // can be represented exactly as Float32.
    switch (node->opcode()) {
      case IrOpcode::kFloat64Equal:
        NodeProperties::ChangeOp(node, machine()->Float32Equal());
        break;
      case IrOpcode::kFloat64LessThan:
        NodeProperties::ChangeOp(node, machine()->Float32LessThan());
        break;
      case IrOpcode::kFloat64LessThanOrEqual:
        NodeProperties::ChangeOp(node, machine()->Float32LessThanOrEqual());
        break;
      default:
        UNREACHABLE();
    }
    node->ReplaceInput(
        0, m.left().HasValue()
               ? Float32Constant(static_cast<float>(m.left().Value()))
               : m.left().InputAt(0));
    node->ReplaceInput(
        1, m.right().HasValue()
               ? Float32Constant(static_cast<float>(m.right().Value()))
               : m.right().InputAt(0));
    return Changed(node);
  }
  return NoChange();
}

Reduction MachineOperatorReducer::ReduceFloat64RoundDown(Node* node) {
  DCHECK_EQ(IrOpcode::kFloat64RoundDown, node->opcode());
  Float64Matcher m(node->InputAt(0));
  if (m.HasValue()) {
    return ReplaceFloat64(std::floor(m.Value()));
  }
  return NoChange();
}

CommonOperatorBuilder* MachineOperatorReducer::common() const {
  return mcgraph()->common();
}


MachineOperatorBuilder* MachineOperatorReducer::machine() const {
  return mcgraph()->machine();
}

Graph* MachineOperatorReducer::graph() const { return mcgraph()->graph(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
