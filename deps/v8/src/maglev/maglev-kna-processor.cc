// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-kna-processor.h"

#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "src/numbers/conversions-inl.h"

namespace v8::internal::maglev {

ProcessResult RecomputeKnownNodeAspectsProcessor::OnContradiction() {
  ReduceResult result = ReduceResult::Done();
  if (current_node()->properties().can_eager_deopt()) {
    // TODO(marja): Ideally, we would detect the empty type already when
    // MaglevGraphOptimizer processes the node and just insert an Abort here.
    // However, the current MaglevGraphOptimizer is incomplete, and sometimes
    // this is where empty types pop up the first time, thus we need to deopt
    // gracefully.
    result = reducer_.EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);
  } else {
    // We should never produce an empty type here but still execute the
    // code we generate after. This can be reached from AssumeType and from
    // re-establishing recorded facts on loads and inlined-call-result
    // wrappers.
    CHECK(current_node()->opcode() == Opcode::kAssumeType ||
          current_node()->opcode() == Opcode::kAssumeMap ||
          current_node()->opcode() == Opcode::kLoadTaggedField ||
          current_node()->opcode() == Opcode::kReturnedValue ||
          current_node()->opcode() == Opcode::kIdentity);
    result = reducer_.BuildAbort(AbortReason::kUnreachable);
  }
  CHECK(result.IsDoneWithAbort());
  return ProcessResult::kTruncateBlock;
}

ProcessResult RecomputeKnownNodeAspectsProcessor::RecordType(ValueNode* node,
                                                             NodeType type) {
  if (known_node_aspects().EnsureType(broker(), node, type) ==
      EnsureTypeResult::kContradiction) {
    return OnContradiction();
  }
  return ProcessResult::kContinue;
}

ProcessResult RecomputeKnownNodeAspectsProcessor::RecordMaps(
    ValueNode* object, const compiler::ZoneRefSet<Map>& maps) {
  auto merger =
      KnownMapsMerger<compiler::ZoneRefSet<Map>>(broker(), zone(), maps);
  merger.IntersectWithKnownNodeAspects(object, known_node_aspects());
  if (merger.intersect_set().is_empty()) {
    return OnContradiction();
  }
  if (!merger.UpdateKnownNodeAspects(object, known_node_aspects())) {
    return OnContradiction();
  }
  return ProcessResult::kContinue;
}

ProcessResult RecomputeKnownNodeAspectsProcessor::ProcessNode(AssumeMap* node) {
  return RecordMaps(node->ObjectInput().node(), node->maps());
}

#define DEFINE_PROCESS_SAFE_CONV(Node, Alt, Type)                              \
  ProcessResult RecomputeKnownNodeAspectsProcessor::ProcessNode(Node* node) {  \
    NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));                  \
    if (!info->alternative().Alt()) {                                          \
      /* TODO(victorgomes): What happens if we we have an alternative already? \
       * Should we remove this one as well? */                                 \
      info->alternative().set_##Alt(node);                                     \
    }                                                                          \
    info->IntersectType(NodeType::k##Type);                                    \
    if (info->type() == NodeType::kNone) {                                     \
      if constexpr (Node::kProperties.can_eager_deopt()) {                     \
        ReduceResult result =                                                  \
            reducer_.EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);    \
        CHECK(result.IsDoneWithAbort());                                       \
        return ProcessResult::kTruncateBlock;                                  \
      } else {                                                                 \
        /* These safe conversion nodes cannot deopt, but they shouldn't        \
         * produce empty types either.*/                                       \
        CHECK(node->opcode() == Opcode::kChangeInt32ToFloat64 ||               \
              node->opcode() == Opcode::kChangeInt32ToHoleyFloat64);           \
        UNREACHABLE();                                                         \
      }                                                                        \
    }                                                                          \
    return ProcessResult::kContinue;                                           \
  }

SAFE_CONVERSION_LIST(DEFINE_PROCESS_SAFE_CONV)

#undef DEFINE_PROCESS_SAFE_CONV

ProcessResult RecomputeKnownNodeAspectsProcessor::ProcessNode(
    CheckedNumberOrOddballToFloat64* node) {
  NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));
  info->IntersectType(
      GetAllowedTypeFromConversionType(node->conversion_type()));
  if (!info->alternative().float64() &&
      NodeTypeIs(info->type(), NodeType::kNumber)) {
    info->alternative().set_float64(node);
  }
  if (info->type() == NodeType::kNone) {
    ReduceResult result =
        reducer_.EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);
    CHECK(result.IsDoneWithAbort());
    return ProcessResult::kTruncateBlock;
  }
  return ProcessResult::kContinue;
}

ProcessResult RecomputeKnownNodeAspectsProcessor::ProcessNode(
    UnsafeNumberOrOddballToFloat64* node) {
  NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));
  if (!info->alternative().float64() &&
      NodeTypeIs(info->type(), NodeType::kNumber)) {
    info->alternative().set_float64(node);
  }
  return ProcessResult::kContinue;
}

ProcessResult RecomputeKnownNodeAspectsProcessor::ProcessNode(
    HoleyFloat64ToSilencedFloat64* node) {
  NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));
  if (!info->alternative().float64() &&
      NodeTypeIs(info->type(), NodeType::kNumber)) {
    info->alternative().set_float64(node);
  }
  return ProcessResult::kContinue;
}

template ReduceResult
    MaglevReducer<RecomputeKnownNodeAspectsProcessor>::EmitUnconditionalDeopt(
        DeoptimizeReason);

}  // namespace v8::internal::maglev
