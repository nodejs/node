// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator-reducer.h"

#include "src/compiler/common-node-cache.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineOperatorReducer::MachineOperatorReducer(Graph* graph)
    : graph_(graph),
      cache_(new (graph->zone()) CommonNodeCache(graph->zone())),
      common_(graph->zone()),
      machine_(graph->zone()) {}


MachineOperatorReducer::MachineOperatorReducer(Graph* graph,
                                               CommonNodeCache* cache)
    : graph_(graph),
      cache_(cache),
      common_(graph->zone()),
      machine_(graph->zone()) {}


Node* MachineOperatorReducer::Int32Constant(int32_t value) {
  Node** loc = cache_->FindInt32Constant(value);
  if (*loc == NULL) {
    *loc = graph_->NewNode(common_.Int32Constant(value));
  }
  return *loc;
}


Node* MachineOperatorReducer::Float64Constant(volatile double value) {
  Node** loc = cache_->FindFloat64Constant(value);
  if (*loc == NULL) {
    *loc = graph_->NewNode(common_.Float64Constant(value));
  }
  return *loc;
}


// Perform constant folding and strength reduction on machine operators.
Reduction MachineOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
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
        graph_->ChangeOperator(node, machine_.Int32Sub());
        node->ReplaceInput(0, Int32Constant(0));
        node->ReplaceInput(1, m.left().node());
        return Changed(node);
      }
      if (m.right().IsPowerOf2()) {  // x * 2^n => x << n
        graph_->ChangeOperator(node, machine_.Word32Shl());
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
        graph_->ChangeOperator(node, machine_.Int32Sub());
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
        graph_->ChangeOperator(node, machine_.Word32Shr());
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
        graph_->ChangeOperator(node, machine_.Word32And());
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
      if (m.IsFoldable()) {  // K + K => K
        return ReplaceFloat64(m.left().Value() + m.right().Value());
      }
      break;
    }
    case IrOpcode::kFloat64Sub: {
      Float64BinopMatcher m(node);
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
    // TODO(turbofan): strength-reduce and fold floating point operations.
    default:
      break;
  }
  return NoChange();
}
}
}
}  // namespace v8::internal::compiler
