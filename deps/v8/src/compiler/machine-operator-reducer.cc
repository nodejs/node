// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator-reducer.h"

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen.h"
#include "src/compiler/diamond.h"
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
  unsigned const shift = base::bits::CountTrailingZeros32(divisor);
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
    case IrOpcode::kWord32Shl:
      return ReduceWord32Shl(node);
    case IrOpcode::kWord32Shr: {
      Uint32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x >>> 0 => x
      if (m.IsFoldable()) {                                  // K >>> K => K
        return ReplaceInt32(m.left().Value() >> m.right().Value());
      }
      return ReduceWord32Shifts(node);
    }
    case IrOpcode::kWord32Sar: {
      Int32BinopMatcher m(node);
      if (m.right().Is(0)) return Replace(m.left().node());  // x >> 0 => x
      if (m.IsFoldable()) {                                  // K >> K => K
        return ReplaceInt32(m.left().Value() >> m.right().Value());
      }
      if (m.left().IsWord32Shl()) {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.left().IsLoad()) {
          LoadRepresentation const rep =
              OpParameter<LoadRepresentation>(mleft.left().node());
          if (m.right().Is(24) && mleft.right().Is(24) && rep == kMachInt8) {
            // Load[kMachInt8] << 24 >> 24 => Load[kMachInt8]
            return Replace(mleft.left().node());
          }
          if (m.right().Is(16) && mleft.right().Is(16) && rep == kMachInt16) {
            // Load[kMachInt16] << 16 >> 16 => Load[kMachInt8]
            return Replace(mleft.left().node());
          }
        }
      }
      return ReduceWord32Shifts(node);
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
    case IrOpcode::kInt32Add:
      return ReduceInt32Add(node);
    case IrOpcode::kInt32Sub:
      return ReduceInt32Sub(node);
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
        Reduction reduction = ReduceWord32Shl(node);
        return reduction.Changed() ? reduction : Changed(node);
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
        return ReplaceFloat64(std::numeric_limits<double>::quiet_NaN());
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


Reduction MachineOperatorReducer::ReduceInt32Add(Node* node) {
  DCHECK_EQ(IrOpcode::kInt32Add, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x + 0 => x
  if (m.IsFoldable()) {                                  // K + K => K
    return ReplaceUint32(bit_cast<uint32_t>(m.left().Value()) +
                         bit_cast<uint32_t>(m.right().Value()));
  }
  return NoChange();
}


Reduction MachineOperatorReducer::ReduceInt32Sub(Node* node) {
  DCHECK_EQ(IrOpcode::kInt32Sub, node->opcode());
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) return Replace(m.left().node());  // x - 0 => x
  if (m.IsFoldable()) {                                  // K - K => K
    return ReplaceInt32(static_cast<uint32_t>(m.left().Value()) -
                        static_cast<uint32_t>(m.right().Value()));
  }
  if (m.LeftEqualsRight()) return ReplaceInt32(0);  // x - x => 0
  if (m.right().HasValue()) {                       // x - K => x + -K
    node->set_op(machine()->Int32Add());
    node->ReplaceInput(1, Int32Constant(-m.right().Value()));
    Reduction const reduction = ReduceInt32Add(node);
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


Reduction MachineOperatorReducer::ReduceWord32Shifts(Node* node) {
  DCHECK((node->opcode() == IrOpcode::kWord32Shl) ||
         (node->opcode() == IrOpcode::kWord32Shr) ||
         (node->opcode() == IrOpcode::kWord32Sar));
  if (machine()->Word32ShiftIsSafe()) {
    // Remove the explicit 'and' with 0x1f if the shift provided by the machine
    // instruction matches that required by JavaScript.
    Int32BinopMatcher m(node);
    if (m.right().IsWord32And()) {
      Int32BinopMatcher mright(m.right().node());
      if (mright.right().Is(0x1f)) {
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
        node->ReplaceInput(1,
                           Uint32Constant(~((1U << m.right().Value()) - 1U)));
        Reduction reduction = ReduceWord32And(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
    }
  }
  return ReduceWord32Shifts(node);
}


Reduction MachineOperatorReducer::ReduceWord32And(Node* node) {
  DCHECK_EQ(IrOpcode::kWord32And, node->opcode());
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
      Reduction const reduction = ReduceWord32And(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  if (m.right().IsNegativePowerOf2()) {
    int32_t const mask = m.right().Value();
    if (m.left().IsWord32Shl()) {
      Uint32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue() &&
          mleft.right().Value() >= base::bits::CountTrailingZeros32(mask)) {
        // (x << L) & (-1 << K) => x << L iff K >= L
        return Replace(mleft.node());
      }
    } else if (m.left().IsInt32Add()) {
      Int32BinopMatcher mleft(m.left().node());
      if (mleft.right().HasValue() &&
          (mleft.right().Value() & mask) == mleft.right().Value()) {
        // (x + (K << L)) & (-1 << L) => (x & (-1 << L)) + (K << L)
        node->set_op(machine()->Int32Add());
        node->ReplaceInput(0, Word32And(mleft.left().node(), m.right().node()));
        node->ReplaceInput(1, mleft.right().node());
        Reduction const reduction = ReduceInt32Add(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
      if (mleft.left().IsInt32Mul()) {
        Int32BinopMatcher mleftleft(mleft.left().node());
        if (mleftleft.right().IsMultipleOf(-mask)) {
          // (y * (K << L) + x) & (-1 << L) => (x & (-1 << L)) + y * (K << L)
          node->set_op(machine()->Int32Add());
          node->ReplaceInput(0,
                             Word32And(mleft.right().node(), m.right().node()));
          node->ReplaceInput(1, mleftleft.node());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.right().IsInt32Mul()) {
        Int32BinopMatcher mleftright(mleft.right().node());
        if (mleftright.right().IsMultipleOf(-mask)) {
          // (x + y * (K << L)) & (-1 << L) => (x & (-1 << L)) + y * (K << L)
          node->set_op(machine()->Int32Add());
          node->ReplaceInput(0,
                             Word32And(mleft.left().node(), m.right().node()));
          node->ReplaceInput(1, mleftright.node());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.left().IsWord32Shl()) {
        Int32BinopMatcher mleftleft(mleft.left().node());
        if (mleftleft.right().Is(base::bits::CountTrailingZeros32(mask))) {
          // (y << L + x) & (-1 << L) => (x & (-1 << L)) + y << L
          node->set_op(machine()->Int32Add());
          node->ReplaceInput(0,
                             Word32And(mleft.right().node(), m.right().node()));
          node->ReplaceInput(1, mleftleft.node());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
      if (mleft.right().IsWord32Shl()) {
        Int32BinopMatcher mleftright(mleft.right().node());
        if (mleftright.right().Is(base::bits::CountTrailingZeros32(mask))) {
          // (x + y << L) & (-1 << L) => (x & (-1 << L)) + y << L
          node->set_op(machine()->Int32Add());
          node->ReplaceInput(0,
                             Word32And(mleft.left().node(), m.right().node()));
          node->ReplaceInput(1, mleftright.node());
          Reduction const reduction = ReduceInt32Add(node);
          return reduction.Changed() ? reduction : Changed(node);
        }
      }
    }
  }
  return NoChange();
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

  Node* shl = NULL;
  Node* shr = NULL;
  // Recognize rotation, we are matching either:
  //  * x << y | x >>> (32 - y) => x ror (32 - y), i.e  x rol y
  //  * x << (32 - y) | x >>> y => x ror y
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
    Node* sub = NULL;
    Node* y = NULL;
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

  node->set_op(machine()->Word32Ror());
  node->ReplaceInput(0, mshl.left().node());
  node->ReplaceInput(1, mshr.right().node());
  return Changed(node);
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
