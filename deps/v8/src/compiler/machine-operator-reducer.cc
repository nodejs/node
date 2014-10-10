// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator-reducer.h"

#include "src/base/bits.h"
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
    case IrOpcode::kInt32Div: {
      Int32BinopMatcher m(node);
      if (m.right().Is(1)) return Replace(m.left().node());  // x / 1 => x
      // TODO(turbofan): if (m.left().Is(0))
      // TODO(turbofan): if (m.right().IsPowerOf2())
      // TODO(turbofan): if (m.right().Is(0))
      // TODO(turbofan): if (m.LeftEqualsRight())
      if (m.IsFoldable() && !m.right().Is(0)) {  // K / K => K
        if (m.right().Is(-1)) return ReplaceInt32(-m.left().Value());
        return ReplaceInt32(m.left().Value() / m.right().Value());
      }
      if (m.right().Is(-1)) {  // x / -1 => 0 - x
        node->set_op(machine()->Int32Sub());
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, m.left().node());
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kInt32UDiv: {
      Uint32BinopMatcher m(node);
      if (m.right().Is(1)) return Replace(m.left().node());  // x / 1 => x
      // TODO(turbofan): if (m.left().Is(0))
      // TODO(turbofan): if (m.right().Is(0))
      // TODO(turbofan): if (m.LeftEqualsRight())
      if (m.IsFoldable() && !m.right().Is(0)) {  // K / K => K
        return ReplaceInt32(m.left().Value() / m.right().Value());
      }
      if (m.right().IsPowerOf2()) {  // x / 2^n => x >> n
        node->set_op(machine()->Word32Shr());
        node->ReplaceInput(1, Int32Constant(WhichPowerOf2(m.right().Value())));
        return Changed(node);
      }
      break;
    }
    case IrOpcode::kInt32Mod: {
      Int32BinopMatcher m(node);
      if (m.right().Is(1)) return ReplaceInt32(0);   // x % 1  => 0
      if (m.right().Is(-1)) return ReplaceInt32(0);  // x % -1 => 0
      // TODO(turbofan): if (m.left().Is(0))
      // TODO(turbofan): if (m.right().IsPowerOf2())
      // TODO(turbofan): if (m.right().Is(0))
      // TODO(turbofan): if (m.LeftEqualsRight())
      if (m.IsFoldable() && !m.right().Is(0)) {  // K % K => K
        return ReplaceInt32(m.left().Value() % m.right().Value());
      }
      break;
    }
    case IrOpcode::kInt32UMod: {
      Uint32BinopMatcher m(node);
      if (m.right().Is(1)) return ReplaceInt32(0);  // x % 1 => 0
      // TODO(turbofan): if (m.left().Is(0))
      // TODO(turbofan): if (m.right().Is(0))
      // TODO(turbofan): if (m.LeftEqualsRight())
      if (m.IsFoldable() && !m.right().Is(0)) {  // K % K => K
        return ReplaceInt32(m.left().Value() % m.right().Value());
      }
      if (m.right().IsPowerOf2()) {  // x % 2^n => x & 2^n-1
        node->set_op(machine()->Word32And());
        node->ReplaceInput(1, Int32Constant(m.right().Value() - 1));
        return Changed(node);
      }
      break;
    }
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
    case IrOpcode::kTruncateFloat64ToInt32: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(DoubleToInt32(m.Value()));
      if (m.IsChangeInt32ToFloat64()) return Replace(m.node()->InputAt(0));
      break;
    }
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
