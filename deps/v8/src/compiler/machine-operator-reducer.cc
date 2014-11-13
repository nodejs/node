// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator-reducer.h"

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen.h"
#include "src/compiler/diamond.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineOperatorReducer::MachineOperatorReducer(JSGraph* jsgraph)
    : jsgraph_(jsgraph) {}


MachineOperatorReducer::~MachineOperatorReducer() {}


Node* MachineOperatorReducer::Float32Constant(volatile float value) {
  return graph()->NewNode(common()->Float32Constant(value));
}


Node* MachineOperatorReducer::Float64Constant(volatile double value) {
  return jsgraph()->Float64Constant(value);
}


Node* MachineOperatorReducer::Int32Constant(int32_t value) {
  return jsgraph()->Int32Constant(value);
}


Node* MachineOperatorReducer::Int64Constant(int64_t value) {
  return graph()->NewNode(common()->Int64Constant(value));
}


Node* MachineOperatorReducer::Word32And(Node* lhs, uint32_t rhs) {
  return graph()->NewNode(machine()->Word32And(), lhs, Uint32Constant(rhs));
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
  return graph()->NewNode(machine()->Int32Add(), lhs, rhs);
}


Node* MachineOperatorReducer::Int32Sub(Node* lhs, Node* rhs) {
  return graph()->NewNode(machine()->Int32Sub(), lhs, rhs);
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
  DCHECK_LT(0, divisor);
  base::MagicNumbersForDivision<uint32_t> const mag =
      base::UnsignedDivisionByConstant(bit_cast<uint32_t>(divisor));
  Node* quotient = graph()->NewNode(machine()->Uint32MulHigh(), dividend,
                                    Uint32Constant(mag.multiplier));
  if (mag.add) {
    DCHECK_LE(1, mag.shift);
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
      return ReduceProjection(OpParameter<size_t>(node), node->InputAt(0));
    case IrOpcode::kWord32And: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.right().node());  // x & 0  => 0
      if (m.right().Is(-1)) return Replace(m.left().node());  // x & -1 => x
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
          return Changed(node);
        }
      }
      break;
    }
    case IrOpcode::kWord32Or: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());    // x | 0  => x
      if (m.right().Is(-1)) return Replace(m.right().node());  // x | -1 => -1
      if (m.IsFoldable()) {                                    // K | K  => K
        return ReplaceInt32(m.left().Value() | m.right().Value());
      }
      if (m.LeftEqualsRight()) return Replace(m.left().node());  // x | x => x
      if (m.left().IsWord32Shl() && m.right().IsWord32Shr()) {
        Int32BinopMatcher mleft(m.left().node());
        Int32BinopMatcher mright(m.right().node());
        if (mleft.left().node() == mright.left().node()) {
          // (x << y) | (x >> (32 - y)) => x ror y
          if (mright.right().IsInt32Sub()) {
            Int32BinopMatcher mrightright(mright.right().node());
            if (mrightright.left().Is(32) &&
                mrightright.right().node() == mleft.right().node()) {
              node->set_op(machine()->Word32Ror());
              node->ReplaceInput(0, mleft.left().node());
              node->ReplaceInput(1, mleft.right().node());
              return Changed(node);
            }
          }
          // (x << K) | (x >> (32 - K)) => x ror K
          if (mleft.right().IsInRange(0, 31) &&
              mright.right().Is(32 - mleft.right().Value())) {
            node->set_op(machine()->Word32Ror());
            node->ReplaceInput(0, mleft.left().node());
            node->ReplaceInput(1, mleft.right().node());
            return Changed(node);
          }
        }
      }
      if (m.left().IsWord32Shr() && m.right().IsWord32Shl()) {
        // (x >> (32 - y)) | (x << y)  => x ror y
        Int32BinopMatcher mleft(m.left().node());
        Int32BinopMatcher mright(m.right().node());
        if (mleft.left().node() == mright.left().node()) {
          if (mleft.right().IsInt32Sub()) {
            Int32BinopMatcher mleftright(mleft.right().node());
            if (mleftright.left().Is(32) &&
                mleftright.right().node() == mright.right().node()) {
              node->set_op(machine()->Word32Ror());
              node->ReplaceInput(0, mright.left().node());
              node->ReplaceInput(1, mright.right().node());
              return Changed(node);
            }
          }
          // (x >> (32 - K)) | (x << K) => x ror K
          if (mright.right().IsInRange(0, 31) &&
              mleft.right().Is(32 - mright.right().Value())) {
            node->set_op(machine()->Word32Ror());
            node->ReplaceInput(0, mright.left().node());
            node->ReplaceInput(1, mright.right().node());
            return Changed(node);
          }
        }
      }
      break;
    }
    case IrOpcode::kWord32Xor: {
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
      break;
    }
    case IrOpcode::kWord32Shl: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x << 0 => x
      if (m.IsFoldable()) {                                  // K << K => K
        return ReplaceInt32(m.left().Value() << m.right().Value());
      }
      if (m.right().IsInRange(1, 31)) {
        // (x >>> K) << K => x & ~(2^K - 1)
        // (x >> K) << K => x & ~(2^K - 1)
        if (m.left().IsWord32Sar() || m.left().IsWord32Shr()) {
          Int32BinopMatcher mleft(m.left().node());
          if (mleft.right().Is(m.right().Value())) {
            node->set_op(machine()->Word32And());
            node->ReplaceInput(0, mleft.left().node());
            node->ReplaceInput(
                1, Uint32Constant(~((1U << m.right().Value()) - 1U)));
            return Changed(node);
          }
        }
      }
      break;
    }
    case IrOpcode::kWord32Shr: {
      Uint32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x >>> 0 => x
      if (m.IsFoldable()) {                                  // K >>> K => K
        return ReplaceInt32(m.left().Value() >> m.right().Value());
      }
      break;
    }
    case IrOpcode::kWord32Sar: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x >> 0 => x
      if (m.IsFoldable()) {                                  // K >> K => K
        return ReplaceInt32(m.left().Value() >> m.right().Value());
      }
      break;
    }
    case IrOpcode::kWord32Ror: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x ror 0 => x
      if (m.IsFoldable()) {                                  // K ror K => K
        return ReplaceInt32(
            base::bits::RotateRight32(m.left().Value(), m.right().Value()));
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
    case IrOpcode::kInt32Add: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x + 0 => x
      if (m.IsFoldable()) {                                  // K + K => K
        return ReplaceInt32(static_cast<uint32_t>(m.left().Value()) +
                            static_cast<uint32_t>(m.right().Value()));
      }
      break;
    }
    case IrOpcode::kInt32Sub: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x - 0 => x
      if (m.IsFoldable()) {                                  // K - K => K
        return ReplaceInt32(static_cast<uint32_t>(m.left().Value()) -
                            static_cast<uint32_t>(m.right().Value()));
      }
      if (m.LeftEqualsRight()) return ReplaceInt32(0);  // x - x => 0
      break;
    }
    case IrOpcode::kInt32Mul: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.right().node());  // x * 0 => 0
      if (m.right().Is(1)) return Replace(m.left().node());   // x * 1 => x
      if (m.IsFoldable()) {                                   // K * K => K
        return ReplaceInt32(m.left().Value() * m.right().Value());
      }
      if (m.right().Is(-1)) {  // x * -1 => 0 - x
        node->set_op(machine()->Int32Sub());
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, m.left().node());
        return Changed(node);
      }
      if (m.right().IsPowerOf2()) {  // x * 2^n => x << n
        node->set_op(machine()->Word32Shl());
        node->ReplaceInput(1, Int32Constant(WhichPowerOf2(m.right().Value())));
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
      if (m.left().IsInt32Sub() && m.right().Is(0)) {  // x - y < 0 => x < y
        Int32BinopMatcher msub(m.left().node());
        node->ReplaceInput(0, msub.left().node());
        node->ReplaceInput(1, msub.right().node());
        return Changed(node);
      }
      if (m.left().Is(0) && m.right().IsInt32Sub()) {  // 0 < x - y => y < x
        Int32BinopMatcher msub(m.right().node());
        node->ReplaceInput(0, msub.right().node());
        node->ReplaceInput(1, msub.left().node());
        return Changed(node);
      }
      if (m.LeftEqualsRight()) return ReplaceBool(false);  // x < x => false
      break;
    }
    case IrOpcode::kInt32LessThanOrEqual: {
      Int32BinopMatcher m(node);
      if (m.IsFoldable()) {  // K <= K => K
        return ReplaceBool(m.left().Value() <= m.right().Value());
      }
      if (m.left().IsInt32Sub() && m.right().Is(0)) {  // x - y <= 0 => x <= y
        Int32BinopMatcher msub(m.left().node());
        node->ReplaceInput(0, msub.left().node());
        node->ReplaceInput(1, msub.right().node());
        return Changed(node);
      }
      if (m.left().Is(0) && m.right().IsInt32Sub()) {  // 0 <= x - y => y <= x
        Int32BinopMatcher msub(m.right().node());
        node->ReplaceInput(0, msub.right().node());
        node->ReplaceInput(1, msub.left().node());
        return Changed(node);
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
          const uint32_t k = mleft.right().Value() & 0x1f;
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
    case IrOpcode::kFloat64Add: {
      Float64BinopMatcher m(node);
      if (m.right().IsNaN()) {  // x + NaN => NaN
        return Replace(m.right().node());
      }
      if (m.IsFoldable()) {  // K + K => K
        return ReplaceFloat64(m.left().Value() + m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Sub: {
      Float64BinopMatcher m(node);
      if (m.right().Is(0) && (Double(m.right().Value()).Sign() > 0)) {
        return Replace(m.left().node());  // x - 0 => x
      }
      if (m.right().IsNaN()) {  // x - NaN => NaN
        return Replace(m.right().node());
      }
      if (m.left().IsNaN()) {  // NaN - x => NaN
        return Replace(m.left().node());
      }
      if (m.IsFoldable()) {  // K - K => K
        return ReplaceFloat64(m.left().Value() - m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Mul: {
      Float64BinopMatcher m(node);
      if (m.right().Is(-1)) {  // x * -1.0 => -0.0 - x
        node->set_op(machine()->Float64Sub());
        node->ReplaceInput(0, Float64Constant(-0.0));
        node->ReplaceInput(1, m.left().node());
        return Changed(node);
      }
      if (m.right().Is(1)) return Replace(m.left().node());  // x * 1.0 => x
      if (m.right().IsNaN()) {                               // x * NaN => NaN
        return Replace(m.right().node());
      }
      if (m.IsFoldable()) {  // K * K => K
        return ReplaceFloat64(m.left().Value() * m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Div: {
      Float64BinopMatcher m(node);
      if (m.right().Is(1)) return Replace(m.left().node());  // x / 1.0 => x
      if (m.right().IsNaN()) {                               // x / NaN => NaN
        return Replace(m.right().node());
      }
      if (m.left().IsNaN()) {  // NaN / x => NaN
        return Replace(m.left().node());
      }
      if (m.IsFoldable()) {  // K / K => K
        return ReplaceFloat64(m.left().Value() / m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Mod: {
      Float64BinopMatcher m(node);
      if (m.right().Is(0)) {  // x % 0 => NaN
        return ReplaceFloat64(base::OS::nan_value());
      }
      if (m.right().IsNaN()) {  // x % NaN => NaN
        return Replace(m.right().node());
      }
      if (m.left().IsNaN()) {  // NaN % x => NaN
        return Replace(m.left().node());
      }
      if (m.IsFoldable()) {  // K % K => K
        return ReplaceFloat64(modulo(m.left().Value(), m.right().Value()));
      }
      break;
    }
    case IrOpcode::kChangeFloat32ToFloat64: {
      Float32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(m.Value());
      break;
    }
    case IrOpcode::kChangeFloat64ToInt32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(FastD2I(m.Value()));
      if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
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
    case IrOpcode::kTruncateFloat64ToInt32:
      return ReduceTruncateFloat64ToInt32(node);
    case IrOpcode::kTruncateInt64ToInt32: {
      Int64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(static_cast<int32_t>(m.Value()));
      if (m.IsChangeInt32ToInt64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kTruncateFloat64ToFloat32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat32(DoubleToFloat32(m.Value()));
      if (m.IsChangeFloat32ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kStore:
      return ReduceStore(node);
    default:
      break;
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
    node->set_op(machine()->Int32Sub());
    node->ReplaceInput(0, Int32Constant(0));
    node->ReplaceInput(1, m.left().node());
    node->TrimInputCount(2);
    return Changed(node);
  }
  if (m.right().HasValue()) {
    int32_t const divisor = m.right().Value();
    Node* const dividend = m.left().node();
    Node* quotient = dividend;
    if (base::bits::IsPowerOfTwo32(Abs(divisor))) {
      uint32_t const shift = WhichPowerOf2Abs(divisor);
      DCHECK_NE(0, shift);
      if (shift > 1) {
        quotient = Word32Sar(quotient, 31);
      }
      quotient = Int32Add(Word32Shr(quotient, 32u - shift), dividend);
      quotient = Word32Sar(quotient, shift);
    } else {
      quotient = Int32Div(quotient, Abs(divisor));
    }
    if (divisor < 0) {
      node->set_op(machine()->Int32Sub());
      node->ReplaceInput(0, Int32Constant(0));
      node->ReplaceInput(1, quotient);
      node->TrimInputCount(2);
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
    if (base::bits::IsPowerOfTwo32(divisor)) {  // x / 2^n => x >> n
      node->set_op(machine()->Word32Shr());
      node->ReplaceInput(1, Uint32Constant(WhichPowerOf2(m.right().Value())));
      node->TrimInputCount(2);
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
    int32_t const divisor = Abs(m.right().Value());
    if (base::bits::IsPowerOfTwo32(divisor)) {
      uint32_t const mask = divisor - 1;
      Node* const zero = Int32Constant(0);
      node->set_op(common()->Select(kMachInt32, BranchHint::kFalse));
      node->ReplaceInput(
          0, graph()->NewNode(machine()->Int32LessThan(), dividend, zero));
      node->ReplaceInput(
          1, Int32Sub(zero, Word32And(Int32Sub(zero, dividend), mask)));
      node->ReplaceInput(2, Word32And(dividend, mask));
    } else {
      Node* quotient = Int32Div(dividend, divisor);
      node->set_op(machine()->Int32Sub());
      DCHECK_EQ(dividend, node->InputAt(0));
      node->ReplaceInput(1, Int32Mul(quotient, Int32Constant(divisor)));
      node->TrimInputCount(2);
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
    if (base::bits::IsPowerOfTwo32(divisor)) {  // x % 2^n => x & 2^n-1
      node->set_op(machine()->Word32And());
      node->ReplaceInput(1, Uint32Constant(m.right().Value() - 1));
    } else {
      Node* quotient = Uint32Div(dividend, divisor);
      node->set_op(machine()->Int32Sub());
      DCHECK_EQ(dividend, node->InputAt(0));
      node->ReplaceInput(1, Int32Mul(quotient, Uint32Constant(divisor)));
    }
    node->TrimInputCount(2);
    return Changed(node);
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceTruncateFloat64ToInt32(Node* node) {
  Float64Matcher m(node->InputAt(0));
  if (m.HasValue()) return ReplaceInt32(DoubleToInt32(m.Value()));
  if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
  if (m.IsPhi()) {
    Node* const phi = m.node();
    DCHECK_EQ(kRepFloat64, RepresentationOf(OpParameter<MachineType>(phi)));
    if (phi->OwnedBy(node)) {
      // TruncateFloat64ToInt32(Phi[Float64](x1,...,xn))
      //   => Phi[Int32](TruncateFloat64ToInt32(x1),
      //                 ...,
      //                 TruncateFloat64ToInt32(xn))
      const int value_input_count = phi->InputCount() - 1;
      for (int i = 0; i < value_input_count; ++i) {
        Node* input = graph()->NewNode(machine()->TruncateFloat64ToInt32(),
                                       phi->InputAt(i));
        // TODO(bmeurer): Reschedule input for reduction once we have Revisit()
        // instead of recursing into ReduceTruncateFloat64ToInt32() here.
        Reduction reduction = ReduceTruncateFloat64ToInt32(input);
        if (reduction.Changed()) input = reduction.replacement();
        phi->ReplaceInput(i, input);
      }
      phi->set_op(common()->Phi(kMachInt32, value_input_count));
      return Replace(phi);
    }
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceStore(Node* node) {
  MachineType const rep =
      RepresentationOf(StoreRepresentationOf(node->op()).machine_type());
  Node* const value = node->InputAt(2);
  switch (value->opcode()) {
    case IrOpcode::kWord32And: {
      Uint32BinopMatcher m(value);
      if (m.right().HasValue() &&
          ((rep == kRepWord8 && (m.right().Value() & 0xff) == 0xff) ||
           (rep == kRepWord16 && (m.right().Value() & 0xffff) == 0xffff))) {
        node->ReplaceInput(2, m.left().node());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kWord32Sar: {
      Int32BinopMatcher m(value);
      if (m.left().IsWord32Shl() &&
          ((rep == kRepWord8 && m.right().IsInRange(1, 24)) ||
           (rep == kRepWord16 && m.right().IsInRange(1, 16)))) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().Is(m.right().Value())) {
          node->ReplaceInput(2, mleft.left().node());
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
        return ReplaceInt32((index == 0) ? val : ovf);
      }
      if (m.right().Is(0)) {
        return (index == 0) ? Replace(m.left().node()) : ReplaceInt32(0);
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
        return ReplaceInt32((index == 0) ? val : ovf);
      }
      if (m.right().Is(0)) {
        return (index == 0) ? Replace(m.left().node()) : ReplaceInt32(0);
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}


CommonOperatorBuilder* MachineOperatorReducer::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* MachineOperatorReducer::machine() const {
  return jsgraph()->machine();
}


Graph* MachineOperatorReducer::graph() const { return jsgraph()->graph(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
