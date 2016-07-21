// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include <limits>

#include "src/address-map.h"
#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/source-position.h"
#include "src/conversions-inl.h"
#include "src/objects.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

// Macro for outputting trace information from representation inference.
#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_representation) PrintF(__VA_ARGS__); \
  } while (false)

// Representation selection and lowering of {Simplified} operators to machine
// operators are interwined. We use a fixpoint calculation to compute both the
// output representation and the best possible lowering for {Simplified} nodes.
// Representation change insertion ensures that all values are in the correct
// machine representation after this phase, as dictated by the machine
// operators themselves.
enum Phase {
  // 1.) PROPAGATE: Traverse the graph from the end, pushing usage information
  //     backwards from uses to definitions, around cycles in phis, according
  //     to local rules for each operator.
  //     During this phase, the usage information for a node determines the best
  //     possible lowering for each operator so far, and that in turn determines
  //     the output representation.
  //     Therefore, to be correct, this phase must iterate to a fixpoint before
  //     the next phase can begin.
  PROPAGATE,

  // 2.) LOWER: perform lowering for all {Simplified} nodes by replacing some
  //     operators for some nodes, expanding some nodes to multiple nodes, or
  //     removing some (redundant) nodes.
  //     During this phase, use the {RepresentationChanger} to insert
  //     representation changes between uses that demand a particular
  //     representation and nodes that produce a different representation.
  LOWER
};


namespace {

// The {UseInfo} class is used to describe a use of an input of a node.
//
// This information is used in two different ways, based on the phase:
//
// 1. During propagation, the use info is used to inform the input node
//    about what part of the input is used (we call this truncation) and what
//    is the preferred representation.
//
// 2. During lowering, the use info is used to properly convert the input
//    to the preferred representation. The preferred representation might be
//    insufficient to do the conversion (e.g. word32->float64 conv), so we also
//    need the signedness information to produce the correct value.
class UseInfo {
 public:
  UseInfo(MachineRepresentation preferred, Truncation truncation)
      : preferred_(preferred), truncation_(truncation) {}
  static UseInfo TruncatingWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Word32());
  }
  static UseInfo TruncatingWord64() {
    return UseInfo(MachineRepresentation::kWord64, Truncation::Word64());
  }
  static UseInfo Bool() {
    return UseInfo(MachineRepresentation::kBit, Truncation::Bool());
  }
  static UseInfo TruncatingFloat32() {
    return UseInfo(MachineRepresentation::kFloat32, Truncation::Float32());
  }
  static UseInfo TruncatingFloat64() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Float64());
  }
  static UseInfo PointerInt() {
    return kPointerSize == 4 ? TruncatingWord32() : TruncatingWord64();
  }
  static UseInfo AnyTagged() {
    return UseInfo(MachineRepresentation::kTagged, Truncation::Any());
  }

  // Undetermined representation.
  static UseInfo Any() {
    return UseInfo(MachineRepresentation::kNone, Truncation::Any());
  }
  static UseInfo None() {
    return UseInfo(MachineRepresentation::kNone, Truncation::None());
  }
  static UseInfo AnyTruncatingToBool() {
    return UseInfo(MachineRepresentation::kNone, Truncation::Bool());
  }

  MachineRepresentation preferred() const { return preferred_; }
  Truncation truncation() const { return truncation_; }

 private:
  MachineRepresentation preferred_;
  Truncation truncation_;
};


UseInfo TruncatingUseInfoFromRepresentation(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kTagged:
      return UseInfo::AnyTagged();
    case MachineRepresentation::kFloat64:
      return UseInfo::TruncatingFloat64();
    case MachineRepresentation::kFloat32:
      return UseInfo::TruncatingFloat32();
    case MachineRepresentation::kWord64:
      return UseInfo::TruncatingWord64();
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return UseInfo::TruncatingWord32();
    case MachineRepresentation::kBit:
      return UseInfo::Bool();
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return UseInfo::None();
}


UseInfo UseInfoForBasePointer(const FieldAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::PointerInt();
}


UseInfo UseInfoForBasePointer(const ElementAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::PointerInt();
}


#ifdef DEBUG
// Helpers for monotonicity checking.
bool MachineRepresentationIsSubtype(MachineRepresentation r1,
                                    MachineRepresentation r2) {
  switch (r1) {
    case MachineRepresentation::kNone:
      return true;
    case MachineRepresentation::kBit:
      return r2 == MachineRepresentation::kBit ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kWord8:
      return r2 == MachineRepresentation::kWord8 ||
             r2 == MachineRepresentation::kWord16 ||
             r2 == MachineRepresentation::kWord32 ||
             r2 == MachineRepresentation::kWord64 ||
             r2 == MachineRepresentation::kFloat32 ||
             r2 == MachineRepresentation::kFloat64 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kWord16:
      return r2 == MachineRepresentation::kWord16 ||
             r2 == MachineRepresentation::kWord32 ||
             r2 == MachineRepresentation::kWord64 ||
             r2 == MachineRepresentation::kFloat32 ||
             r2 == MachineRepresentation::kFloat64 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kWord32:
      return r2 == MachineRepresentation::kWord32 ||
             r2 == MachineRepresentation::kWord64 ||
             r2 == MachineRepresentation::kFloat64 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kWord64:
      return r2 == MachineRepresentation::kWord64;
    case MachineRepresentation::kFloat32:
      return r2 == MachineRepresentation::kFloat32 ||
             r2 == MachineRepresentation::kFloat64 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kFloat64:
      return r2 == MachineRepresentation::kFloat64 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kSimd128:
      return r2 == MachineRepresentation::kSimd128 ||
             r2 == MachineRepresentation::kTagged;
    case MachineRepresentation::kTagged:
      return r2 == MachineRepresentation::kTagged;
  }
  UNREACHABLE();
  return false;
}


class InputUseInfos {
 public:
  explicit InputUseInfos(Zone* zone) : input_use_infos_(zone) {}

  void SetAndCheckInput(Node* node, int index, UseInfo use_info) {
    if (input_use_infos_.empty()) {
      input_use_infos_.resize(node->InputCount(), UseInfo::None());
    }
    // Check that the new use informatin is a super-type of the old
    // one.
    CHECK(IsUseLessGeneral(input_use_infos_[index], use_info));
    input_use_infos_[index] = use_info;
  }

 private:
  ZoneVector<UseInfo> input_use_infos_;

  static bool IsUseLessGeneral(UseInfo use1, UseInfo use2) {
    return MachineRepresentationIsSubtype(use1.preferred(), use2.preferred()) &&
           use1.truncation().IsLessGeneralThan(use2.truncation());
  }
};

#endif  // DEBUG

}  // namespace


class RepresentationSelector {
 public:
  // Information for each node tracked during the fixpoint.
  class NodeInfo {
   public:
    // Adds new use to the node. Returns true if something has changed
    // and the node has to be requeued.
    bool AddUse(UseInfo info) {
      Truncation old_truncation = truncation_;
      truncation_ = Truncation::Generalize(truncation_, info.truncation());
      return truncation_ != old_truncation;
    }

    void set_queued(bool value) { queued_ = value; }
    bool queued() const { return queued_; }
    void set_visited() { visited_ = true; }
    bool visited() const { return visited_; }
    Truncation truncation() const { return truncation_; }
    void set_output(MachineRepresentation output) { representation_ = output; }

    MachineRepresentation representation() const { return representation_; }

   private:
    bool queued_ = false;   // Bookkeeping for the traversal.
    bool visited_ = false;  // Bookkeeping for the traversal.
    MachineRepresentation representation_ =
        MachineRepresentation::kNone;             // Output representation.
    Truncation truncation_ = Truncation::None();  // Information about uses.
  };

  RepresentationSelector(JSGraph* jsgraph, Zone* zone,
                         RepresentationChanger* changer,
                         SourcePositionTable* source_positions)
      : jsgraph_(jsgraph),
        count_(jsgraph->graph()->NodeCount()),
        info_(count_, zone),
#ifdef DEBUG
        node_input_use_infos_(count_, InputUseInfos(zone), zone),
#endif
        nodes_(zone),
        replacements_(zone),
        phase_(PROPAGATE),
        changer_(changer),
        queue_(zone),
        source_positions_(source_positions),
        type_cache_(TypeCache::Get()) {
  }

  void Run(SimplifiedLowering* lowering) {
    // Run propagation phase to a fixpoint.
    TRACE("--{Propagation phase}--\n");
    phase_ = PROPAGATE;
    EnqueueInitial(jsgraph_->graph()->end());
    // Process nodes from the queue until it is empty.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      NodeInfo* info = GetInfo(node);
      queue_.pop();
      info->set_queued(false);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      VisitNode(node, info->truncation(), nullptr);
      TRACE("  ==> output ");
      PrintOutputInfo(info);
      TRACE("\n");
    }

    // Run lowering and change insertion phase.
    TRACE("--{Simplified lowering phase}--\n");
    phase_ = LOWER;
    // Process nodes from the collected {nodes_} vector.
    for (NodeVector::iterator i = nodes_.begin(); i != nodes_.end(); ++i) {
      Node* node = *i;
      NodeInfo* info = GetInfo(node);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      // Reuse {VisitNode()} so the representation rules are in one place.
      SourcePositionTable::Scope scope(
          source_positions_, source_positions_->GetSourcePosition(node));
      VisitNode(node, info->truncation(), lowering);
    }

    // Perform the final replacements.
    for (NodeVector::iterator i = replacements_.begin();
         i != replacements_.end(); ++i) {
      Node* node = *i;
      Node* replacement = *(++i);
      node->ReplaceUses(replacement);
      // We also need to replace the node in the rest of the vector.
      for (NodeVector::iterator j = i + 1; j != replacements_.end(); ++j) {
        ++j;
        if (*j == node) *j = replacement;
      }
    }
  }

  void EnqueueInitial(Node* node) {
    NodeInfo* info = GetInfo(node);
    info->set_visited();
    info->set_queued(true);
    nodes_.push_back(node);
    queue_.push(node);
  }

  // Enqueue {use_node}'s {index} input if the {use} contains new information
  // for that input node. Add the input to {nodes_} if this is the first time
  // it's been visited.
  void EnqueueInput(Node* use_node, int index,
                    UseInfo use_info = UseInfo::None()) {
    Node* node = use_node->InputAt(index);
    if (phase_ != PROPAGATE) return;
    NodeInfo* info = GetInfo(node);
#ifdef DEBUG
    // Check monotonicity of input requirements.
    node_input_use_infos_[use_node->id()].SetAndCheckInput(use_node, index,
                                                           use_info);
#endif  // DEBUG
    if (!info->visited()) {
      // First visit of this node.
      info->set_visited();
      info->set_queued(true);
      nodes_.push_back(node);
      queue_.push(node);
      TRACE("  initial: ");
      info->AddUse(use_info);
      PrintTruncation(info->truncation());
      return;
    }
    TRACE("   queue?: ");
    PrintTruncation(info->truncation());
    if (info->AddUse(use_info)) {
      // New usage information for the node is available.
      if (!info->queued()) {
        queue_.push(node);
        info->set_queued(true);
        TRACE("   added: ");
      } else {
        TRACE(" inqueue: ");
      }
      PrintTruncation(info->truncation());
    }
  }

  bool lower() { return phase_ == LOWER; }

  void EnqueueUses(Node* node) {
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsValueEdge(edge)) {
        Node* const user = edge.from();
        if (user->id() < count_) {
          // New type information for the node is available.
          NodeInfo* info = GetInfo(user);
          // Enqueue the node only if we are sure it is reachable from
          // the end and it has not been queued yet.
          if (info->visited() && !info->queued()) {
            queue_.push(user);
            info->set_queued(true);
          }
        }
      }
    }
  }

  void SetOutput(Node* node, MachineRepresentation representation) {
    NodeInfo* info = GetInfo(node);
    DCHECK(
        MachineRepresentationIsSubtype(info->representation(), representation));
    info->set_output(representation);
  }

  Type* GetUpperBound(Node* node) { return NodeProperties::GetType(node); }

  bool BothInputsAreSigned32(Node* node) {
    DCHECK_EQ(2, node->InputCount());
    return GetUpperBound(node->InputAt(0))->Is(Type::Signed32()) &&
           GetUpperBound(node->InputAt(1))->Is(Type::Signed32());
  }

  bool BothInputsAreUnsigned32(Node* node) {
    DCHECK_EQ(2, node->InputCount());
    return GetUpperBound(node->InputAt(0))->Is(Type::Unsigned32()) &&
           GetUpperBound(node->InputAt(1))->Is(Type::Unsigned32());
  }

  bool BothInputsAre(Node* node, Type* type) {
    DCHECK_EQ(2, node->InputCount());
    return GetUpperBound(node->InputAt(0))->Is(type) &&
           GetUpperBound(node->InputAt(1))->Is(type);
  }

  void ConvertInput(Node* node, int index, UseInfo use) {
    Node* input = node->InputAt(index);
    // In the change phase, insert a change before the use if necessary.
    if (use.preferred() == MachineRepresentation::kNone)
      return;  // No input requirement on the use.
    NodeInfo* input_info = GetInfo(input);
    MachineRepresentation input_rep = input_info->representation();
    if (input_rep != use.preferred()) {
      // Output representation doesn't match usage.
      TRACE("  change: #%d:%s(@%d #%d:%s) ", node->id(), node->op()->mnemonic(),
            index, input->id(), input->op()->mnemonic());
      TRACE(" from ");
      PrintOutputInfo(input_info);
      TRACE(" to ");
      PrintUseInfo(use);
      TRACE("\n");
      Node* n = changer_->GetRepresentationFor(
          input, input_info->representation(), GetUpperBound(input),
          use.preferred(), use.truncation());
      node->ReplaceInput(index, n);
    }
  }

  void ProcessInput(Node* node, int index, UseInfo use) {
    if (phase_ == PROPAGATE) {
      EnqueueInput(node, index, use);
    } else {
      ConvertInput(node, index, use);
    }
  }

  void ProcessRemainingInputs(Node* node, int index) {
    DCHECK_GE(index, NodeProperties::PastValueIndex(node));
    DCHECK_GE(index, NodeProperties::PastContextIndex(node));
    for (int i = std::max(index, NodeProperties::FirstEffectIndex(node));
         i < NodeProperties::PastEffectIndex(node); ++i) {
      EnqueueInput(node, i);  // Effect inputs: just visit
    }
    for (int i = std::max(index, NodeProperties::FirstControlIndex(node));
         i < NodeProperties::PastControlIndex(node); ++i) {
      EnqueueInput(node, i);  // Control inputs: just visit
    }
  }

  // The default, most general visitation case. For {node}, process all value,
  // context, frame state, effect, and control inputs, assuming that value
  // inputs should have {kRepTagged} representation and can observe all output
  // values {kTypeAny}.
  void VisitInputs(Node* node) {
    int tagged_count = node->op()->ValueInputCount() +
                       OperatorProperties::GetContextInputCount(node->op());
    // Visit value and context inputs as tagged.
    for (int i = 0; i < tagged_count; i++) {
      ProcessInput(node, i, UseInfo::AnyTagged());
    }
    // Only enqueue other inputs (framestates, effects, control).
    for (int i = tagged_count; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
  }

  // Helper for binops of the R x L -> O variety.
  void VisitBinop(Node* node, UseInfo left_use, UseInfo right_use,
                  MachineRepresentation output) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    ProcessInput(node, 0, left_use);
    ProcessInput(node, 1, right_use);
    for (int i = 2; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
    SetOutput(node, output);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, UseInfo input_use, MachineRepresentation output) {
    VisitBinop(node, input_use, input_use, output);
  }

  // Helper for unops of the I -> O variety.
  void VisitUnop(Node* node, UseInfo input_use, MachineRepresentation output) {
    DCHECK_EQ(1, node->InputCount());
    ProcessInput(node, 0, input_use);
    SetOutput(node, output);
  }

  // Helper for leaf nodes.
  void VisitLeaf(Node* node, MachineRepresentation output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput(node, output);
  }

  // Helpers for specific types of binops.
  void VisitFloat64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingFloat64(),
               MachineRepresentation::kFloat64);
  }
  void VisitInt32Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(),
               MachineRepresentation::kWord32);
  }
  void VisitWord32TruncatingBinop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(),
               MachineRepresentation::kWord32);
  }
  void VisitUint32Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(),
               MachineRepresentation::kWord32);
  }
  void VisitInt64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(),
               MachineRepresentation::kWord64);
  }
  void VisitUint64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(),
               MachineRepresentation::kWord64);
  }
  void VisitFloat64Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingFloat64(), MachineRepresentation::kBit);
  }
  void VisitInt32Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), MachineRepresentation::kBit);
  }
  void VisitUint32Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), MachineRepresentation::kBit);
  }
  void VisitInt64Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), MachineRepresentation::kBit);
  }
  void VisitUint64Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), MachineRepresentation::kBit);
  }

  // Infer representation for phi-like nodes.
  MachineRepresentation GetOutputInfoForPhi(Node* node, Truncation use) {
    // Compute the representation.
    Type* type = GetUpperBound(node);
    if (type->Is(Type::None())) {
      return MachineRepresentation::kNone;
    } else if (type->Is(Type::Signed32()) || type->Is(Type::Unsigned32())) {
      return MachineRepresentation::kWord32;
    } else if (use.TruncatesToWord32()) {
      return MachineRepresentation::kWord32;
    } else if (type->Is(Type::Boolean())) {
      return MachineRepresentation::kBit;
    } else if (type->Is(Type::Number())) {
      return MachineRepresentation::kFloat64;
    } else if (use.TruncatesToFloat64()) {
      return MachineRepresentation::kFloat64;
    } else if (type->Is(Type::Internal())) {
      // We mark (u)int64 as Type::Internal.
      // TODO(jarin) This is a workaround for our lack of (u)int64
      // types. This can be removed once we can represent (u)int64
      // unambiguously. (At the moment internal objects, such as the hole,
      // are also Type::Internal()).
      bool is_word64 = GetInfo(node->InputAt(0))->representation() ==
                       MachineRepresentation::kWord64;
#ifdef DEBUG
      // Check that all the inputs agree on being Word64.
      for (int i = 1; i < node->op()->ValueInputCount(); i++) {
        DCHECK_EQ(is_word64, GetInfo(node->InputAt(i))->representation() ==
                                 MachineRepresentation::kWord64);
      }
#endif
      return is_word64 ? MachineRepresentation::kWord64
                       : MachineRepresentation::kTagged;
    }
    return MachineRepresentation::kTagged;
  }

  // Helper for handling selects.
  void VisitSelect(Node* node, Truncation truncation,
                   SimplifiedLowering* lowering) {
    ProcessInput(node, 0, UseInfo::Bool());

    MachineRepresentation output = GetOutputInfoForPhi(node, truncation);
    SetOutput(node, output);

    if (lower()) {
      // Update the select operator.
      SelectParameters p = SelectParametersOf(node->op());
      if (output != p.representation()) {
        NodeProperties::ChangeOp(node,
                                 lowering->common()->Select(output, p.hint()));
      }
    }
    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output, truncation);
    ProcessInput(node, 1, input_use);
    ProcessInput(node, 2, input_use);
  }

  // Helper for handling phis.
  void VisitPhi(Node* node, Truncation truncation,
                SimplifiedLowering* lowering) {
    MachineRepresentation output = GetOutputInfoForPhi(node, truncation);
    SetOutput(node, output);

    int values = node->op()->ValueInputCount();
    if (lower()) {
      // Update the phi operator.
      if (output != PhiRepresentationOf(node->op())) {
        NodeProperties::ChangeOp(node, lowering->common()->Phi(output, values));
      }
    }

    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output, truncation);
    for (int i = 0; i < node->InputCount(); i++) {
      ProcessInput(node, i, i < values ? input_use : UseInfo::None());
    }
  }

  void VisitCall(Node* node, SimplifiedLowering* lowering) {
    const CallDescriptor* desc = CallDescriptorOf(node->op());
    const MachineSignature* sig = desc->GetMachineSignature();
    int params = static_cast<int>(sig->parameter_count());
    // Propagate representation information from call descriptor.
    for (int i = 0; i < node->InputCount(); i++) {
      if (i == 0) {
        // The target of the call.
        ProcessInput(node, i, UseInfo::None());
      } else if ((i - 1) < params) {
        ProcessInput(node, i, TruncatingUseInfoFromRepresentation(
                                  sig->GetParam(i - 1).representation()));
      } else {
        ProcessInput(node, i, UseInfo::None());
      }
    }

    if (sig->return_count() > 0) {
      SetOutput(node,
                desc->GetMachineSignature()->GetReturn().representation());
    } else {
      SetOutput(node, MachineRepresentation::kTagged);
    }
  }

  MachineSemantic DeoptValueSemanticOf(Type* type) {
    CHECK(!type->Is(Type::None()));
    // We only need signedness to do deopt correctly.
    if (type->Is(Type::Signed32())) {
      return MachineSemantic::kInt32;
    } else if (type->Is(Type::Unsigned32())) {
      return MachineSemantic::kUint32;
    } else {
      return MachineSemantic::kAny;
    }
  }

  void VisitStateValues(Node* node) {
    if (phase_ == PROPAGATE) {
      for (int i = 0; i < node->InputCount(); i++) {
        EnqueueInput(node, i, UseInfo::Any());
      }
    } else {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          new (zone->New(sizeof(ZoneVector<MachineType>)))
              ZoneVector<MachineType>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        NodeInfo* input_info = GetInfo(input);
        MachineType machine_type(input_info->representation(),
                                 DeoptValueSemanticOf(GetUpperBound(input)));
        DCHECK(machine_type.representation() !=
                   MachineRepresentation::kWord32 ||
               machine_type.semantic() == MachineSemantic::kInt32 ||
               machine_type.semantic() == MachineSemantic::kUint32);
        (*types)[i] = machine_type;
      }
      NodeProperties::ChangeOp(node,
                               jsgraph_->common()->TypedStateValues(types));
    }
    SetOutput(node, MachineRepresentation::kTagged);
  }

  const Operator* Int32Op(Node* node) {
    return changer_->Int32OperatorFor(node->opcode());
  }

  const Operator* Uint32Op(Node* node) {
    return changer_->Uint32OperatorFor(node->opcode());
  }

  const Operator* Float64Op(Node* node) {
    return changer_->Float64OperatorFor(node->opcode());
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, Type* field_type,
      Node* value) {
    if (base_taggedness == kTaggedBase &&
        field_representation == MachineRepresentation::kTagged) {
      Type* value_type = NodeProperties::GetType(value);
      if (field_type->Is(Type::TaggedSigned()) ||
          value_type->Is(Type::TaggedSigned())) {
        // Write barriers are only for stores of heap objects.
        return kNoWriteBarrier;
      }
      if (field_type->Is(Type::BooleanOrNullOrUndefined()) ||
          value_type->Is(Type::BooleanOrNullOrUndefined())) {
        // Write barriers are not necessary when storing true, false, null or
        // undefined, because these special oddballs are always in the root set.
        return kNoWriteBarrier;
      }
      if (value_type->IsConstant() &&
          value_type->AsConstant()->Value()->IsHeapObject()) {
        Handle<HeapObject> value_object =
            Handle<HeapObject>::cast(value_type->AsConstant()->Value());
        RootIndexMap root_index_map(jsgraph_->isolate());
        int root_index = root_index_map.Lookup(*value_object);
        if (root_index != RootIndexMap::kInvalidRootIndex &&
            jsgraph_->isolate()->heap()->RootIsImmortalImmovable(root_index)) {
          // Write barriers are unnecessary for immortal immovable roots.
          return kNoWriteBarrier;
        }
        if (value_object->IsMap()) {
          // Write barriers for storing maps are cheaper.
          return kMapWriteBarrier;
        }
      }
      if (field_type->Is(Type::TaggedPointer()) ||
          value_type->Is(Type::TaggedPointer())) {
        // Write barriers for heap objects are cheaper.
        return kPointerWriteBarrier;
      }
      NumberMatcher m(value);
      if (m.HasValue()) {
        if (IsSmiDouble(m.Value())) {
          // Storing a smi doesn't need a write barrier.
          return kNoWriteBarrier;
        }
        // The NumberConstant will be represented as HeapNumber.
        return kPointerWriteBarrier;
      }
      return kFullWriteBarrier;
    }
    return kNoWriteBarrier;
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, int field_offset,
      Type* field_type, Node* value) {
    if (base_taggedness == kTaggedBase &&
        field_offset == HeapObject::kMapOffset) {
      return kMapWriteBarrier;
    }
    return WriteBarrierKindFor(base_taggedness, field_representation,
                               field_type, value);
  }

  // Dispatching routine for visiting the node {node} with the usage {use}.
  // Depending on the operator, propagate new usage info to the inputs.
  void VisitNode(Node* node, Truncation truncation,
                 SimplifiedLowering* lowering) {
    switch (node->opcode()) {
      //------------------------------------------------------------------
      // Common operators.
      //------------------------------------------------------------------
      case IrOpcode::kStart:
      case IrOpcode::kDead:
        return VisitLeaf(node, MachineRepresentation::kNone);
      case IrOpcode::kParameter: {
        // TODO(titzer): use representation from linkage.
        ProcessInput(node, 0, UseInfo::None());
        SetOutput(node, MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kInt32Constant:
        return VisitLeaf(node, MachineRepresentation::kWord32);
      case IrOpcode::kInt64Constant:
        return VisitLeaf(node, MachineRepresentation::kWord64);
      case IrOpcode::kFloat32Constant:
        return VisitLeaf(node, MachineRepresentation::kFloat32);
      case IrOpcode::kFloat64Constant:
        return VisitLeaf(node, MachineRepresentation::kFloat64);
      case IrOpcode::kExternalConstant:
        return VisitLeaf(node, MachineType::PointerRepresentation());
      case IrOpcode::kNumberConstant:
        return VisitLeaf(node, MachineRepresentation::kTagged);
      case IrOpcode::kHeapConstant:
        return VisitLeaf(node, MachineRepresentation::kTagged);

      case IrOpcode::kDeoptimizeIf:
      case IrOpcode::kDeoptimizeUnless:
        ProcessInput(node, 0, UseInfo::Bool());
        ProcessInput(node, 1, UseInfo::AnyTagged());
        ProcessRemainingInputs(node, 2);
        break;
      case IrOpcode::kBranch:
        ProcessInput(node, 0, UseInfo::Bool());
        EnqueueInput(node, NodeProperties::FirstControlIndex(node));
        break;
      case IrOpcode::kSwitch:
        ProcessInput(node, 0, UseInfo::TruncatingWord32());
        EnqueueInput(node, NodeProperties::FirstControlIndex(node));
        break;
      case IrOpcode::kSelect:
        return VisitSelect(node, truncation, lowering);
      case IrOpcode::kPhi:
        return VisitPhi(node, truncation, lowering);
      case IrOpcode::kCall:
        return VisitCall(node, lowering);

      //------------------------------------------------------------------
      // JavaScript operators.
      //------------------------------------------------------------------
      case IrOpcode::kJSToNumber: {
        VisitInputs(node);
        // TODO(bmeurer): Optimize somewhat based on input type?
        if (truncation.TruncatesToWord32()) {
          SetOutput(node, MachineRepresentation::kWord32);
          if (lower()) lowering->DoJSToNumberTruncatesToWord32(node, this);
        } else if (truncation.TruncatesToFloat64()) {
          SetOutput(node, MachineRepresentation::kFloat64);
          if (lower()) lowering->DoJSToNumberTruncatesToFloat64(node, this);
        } else {
          SetOutput(node, MachineRepresentation::kTagged);
        }
        break;
      }

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower()) {
          NodeInfo* input_info = GetInfo(node->InputAt(0));
          if (input_info->representation() == MachineRepresentation::kBit) {
            // BooleanNot(x: kRepBit) => Word32Equal(x, #0)
            node->AppendInput(jsgraph_->zone(), jsgraph_->Int32Constant(0));
            NodeProperties::ChangeOp(node, lowering->machine()->Word32Equal());
          } else {
            // BooleanNot(x: kRepTagged) => WordEqual(x, #false)
            node->AppendInput(jsgraph_->zone(), jsgraph_->FalseConstant());
            NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, UseInfo::AnyTruncatingToBool());
          SetOutput(node, MachineRepresentation::kBit);
        }
        break;
      }
      case IrOpcode::kBooleanToNumber: {
        if (lower()) {
          NodeInfo* input_info = GetInfo(node->InputAt(0));
          if (input_info->representation() == MachineRepresentation::kBit) {
            // BooleanToNumber(x: kRepBit) => x
            DeferReplacement(node, node->InputAt(0));
          } else {
            // BooleanToNumber(x: kRepTagged) => WordEqual(x, #true)
            node->AppendInput(jsgraph_->zone(), jsgraph_->TrueConstant());
            NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, UseInfo::AnyTruncatingToBool());
          SetOutput(node, MachineRepresentation::kWord32);
        }
        break;
      }
      case IrOpcode::kNumberEqual:
      case IrOpcode::kNumberLessThan:
      case IrOpcode::kNumberLessThanOrEqual: {
        // Number comparisons reduce to integer comparisons for integer inputs.
        if (BothInputsAreSigned32(node)) {
          // => signed Int32Cmp
          VisitInt32Cmp(node);
          if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        } else if (BothInputsAreUnsigned32(node)) {
          // => unsigned Int32Cmp
          VisitUint32Cmp(node);
          if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        } else {
          // => Float64Cmp
          VisitFloat64Cmp(node);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        break;
      }
      case IrOpcode::kNumberAdd:
      case IrOpcode::kNumberSubtract: {
        if (BothInputsAre(node, Type::Signed32()) &&
            NodeProperties::GetType(node)->Is(Type::Signed32())) {
          // int32 + int32 = int32
          // => signed Int32Add/Sub
          VisitInt32Binop(node);
          if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        } else if (BothInputsAre(node, type_cache_.kAdditiveSafeInteger) &&
                   truncation.TruncatesToWord32()) {
          // safe-int + safe-int = x (truncated to int32)
          // => signed Int32Add/Sub (truncated)
          VisitWord32TruncatingBinop(node);
          if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        } else {
          // => Float64Add/Sub
          VisitFloat64Binop(node);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        break;
      }
      case IrOpcode::kNumberMultiply: {
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node)->Is(Type::Signed32())) {
            // Multiply reduces to Int32Mul if the inputs and the output
            // are integers.
            VisitInt32Binop(node);
            if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
            break;
          }
          if (truncation.TruncatesToWord32() &&
              NodeProperties::GetType(node)->Is(type_cache_.kSafeInteger)) {
            // Multiply reduces to Int32Mul if the inputs are integers,
            // the uses are truncating and the result is in the safe
            // integer range.
            VisitWord32TruncatingBinop(node);
            if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
            break;
          }
        }
        // => Float64Mul
        VisitFloat64Binop(node);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        break;
      }
      case IrOpcode::kNumberDivide: {
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node)->Is(Type::Signed32())) {
          // => signed Int32Div
          VisitInt32Binop(node);
          if (lower()) DeferReplacement(node, lowering->Int32Div(node));
          break;
          }
          if (truncation.TruncatesToWord32()) {
            // => signed Int32Div
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
            break;
          }
        }
        if (BothInputsAreUnsigned32(node) && truncation.TruncatesToWord32()) {
          // => unsigned Uint32Div
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Div(node));
          break;
        }
        // => Float64Div
        VisitFloat64Binop(node);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        break;
      }
      case IrOpcode::kNumberModulus: {
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node)->Is(Type::Signed32())) {
            // => signed Int32Mod
            VisitInt32Binop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
            break;
          }
          if (truncation.TruncatesToWord32()) {
            // => signed Int32Mod
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
            break;
          }
        }
        if (BothInputsAreUnsigned32(node) && truncation.TruncatesToWord32()) {
          // => unsigned Uint32Mod
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
          break;
        }
        // => Float64Mod
        VisitFloat64Binop(node);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        break;
      }
      case IrOpcode::kNumberBitwiseOr:
      case IrOpcode::kNumberBitwiseXor:
      case IrOpcode::kNumberBitwiseAnd: {
        VisitInt32Binop(node);
        if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        break;
      }
      case IrOpcode::kNumberShiftLeft: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shl(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberShiftRight: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Sar(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberShiftRightLogical: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shr(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberClz32: {
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        break;
      }
      case IrOpcode::kNumberImul: {
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        break;
      }
      case IrOpcode::kNumberCeil: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) DeferReplacement(node, lowering->Float64Ceil(node));
        break;
      }
      case IrOpcode::kNumberFloor: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) DeferReplacement(node, lowering->Float64Floor(node));
        break;
      }
      case IrOpcode::kNumberRound: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) DeferReplacement(node, lowering->Float64Round(node));
        break;
      }
      case IrOpcode::kNumberTrunc: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) DeferReplacement(node, lowering->Float64Trunc(node));
        break;
      }
      case IrOpcode::kNumberToInt32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) DeferReplacement(node, node->InputAt(0));
        break;
      }
      case IrOpcode::kNumberToUint32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) DeferReplacement(node, node->InputAt(0));
        break;
      }
      case IrOpcode::kNumberIsHoleNaN: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kBit);
        if (lower()) {
          // NumberIsHoleNaN(x) => Word32Equal(Float64ExtractLowWord32(x),
          //                                   #HoleNaNLower32)
          node->ReplaceInput(0,
                             jsgraph_->graph()->NewNode(
                                 lowering->machine()->Float64ExtractLowWord32(),
                                 node->InputAt(0)));
          node->AppendInput(jsgraph_->zone(),
                            jsgraph_->Int32Constant(kHoleNanLower32));
          NodeProperties::ChangeOp(node, jsgraph_->machine()->Word32Equal());
        }
        break;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        if (lower()) {
          NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
        }
        break;
      }
      case IrOpcode::kStringEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        if (lower()) {
          // StringEqual(x, y) => Call(StringEqualStub, x, y, no-context)
          Operator::Properties properties =
              Operator::kCommutative | Operator::kNoThrow;
          Callable callable = CodeFactory::StringEqual(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          NodeProperties::ChangeOp(node, jsgraph_->common()->Call(desc));
        }
        break;
      }
      case IrOpcode::kStringLessThan: {
        VisitBinop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        if (lower()) {
          // StringLessThan(x, y) => Call(StringLessThanStub, x, y, no-context)
          Operator::Properties properties = Operator::kNoThrow;
          Callable callable = CodeFactory::StringLessThan(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          NodeProperties::ChangeOp(node, jsgraph_->common()->Call(desc));
        }
        break;
      }
      case IrOpcode::kStringLessThanOrEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        if (lower()) {
          // StringLessThanOrEqual(x, y)
          //   => Call(StringLessThanOrEqualStub, x, y, no-context)
          Operator::Properties properties = Operator::kNoThrow;
          Callable callable =
              CodeFactory::StringLessThanOrEqual(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          NodeProperties::ChangeOp(node, jsgraph_->common()->Call(desc));
        }
        break;
      }
      case IrOpcode::kStringToNumber: {
        VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        if (lower()) {
          // StringToNumber(x) => Call(StringToNumberStub, x, no-context)
          Operator::Properties properties = Operator::kNoThrow;
          Callable callable = CodeFactory::StringToNumber(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          node->AppendInput(jsgraph_->zone(), jsgraph_->graph()->start());
          NodeProperties::ChangeOp(node, jsgraph_->common()->Call(desc));
        }
        break;
      }
      case IrOpcode::kAllocate: {
        ProcessInput(node, 0, UseInfo::TruncatingWord32());
        ProcessRemainingInputs(node, 1);
        SetOutput(node, MachineRepresentation::kTagged);
        break;
      }
      case IrOpcode::kLoadField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));
        ProcessRemainingInputs(node, 1);
        SetOutput(node, access.machine_type.representation());
        break;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));
        ProcessInput(node, 1, TruncatingUseInfoFromRepresentation(
                                  access.machine_type.representation()));
        ProcessRemainingInputs(node, 2);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) {
          WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
              access.base_is_tagged, access.machine_type.representation(),
              access.offset, access.type, node->InputAt(1));
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreField(access));
          }
        }
        break;
      }
      case IrOpcode::kLoadBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, UseInfo::PointerInt());        // buffer
        ProcessInput(node, 1, UseInfo::TruncatingWord32());  // offset
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // length
        ProcessRemainingInputs(node, 3);

        MachineRepresentation output;
        if (truncation.TruncatesUndefinedToZeroOrNaN()) {
          if (truncation.TruncatesNaNToZero()) {
            // If undefined is truncated to a non-NaN number, we can use
            // the load's representation.
            output = access.machine_type().representation();
          } else {
            // If undefined is truncated to a number, but the use can
            // observe NaN, we need to output at least the float32
            // representation.
            if (access.machine_type().representation() ==
                MachineRepresentation::kFloat32) {
              output = access.machine_type().representation();
            } else {
              output = MachineRepresentation::kFloat64;
            }
          }
        } else {
          // If undefined is not truncated away, we need to have the tagged
          // representation.
          output = MachineRepresentation::kTagged;
        }
        SetOutput(node, output);
        if (lower()) lowering->DoLoadBuffer(node, output, changer_);
        break;
      }
      case IrOpcode::kStoreBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, UseInfo::PointerInt());        // buffer
        ProcessInput(node, 1, UseInfo::TruncatingWord32());  // offset
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // length
        ProcessInput(node, 3,
                     TruncatingUseInfoFromRepresentation(
                         access.machine_type().representation()));  // value
        ProcessRemainingInputs(node, 4);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) lowering->DoStoreBuffer(node);
        break;
      }
      case IrOpcode::kLoadElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));  // base
        ProcessInput(node, 1, UseInfo::TruncatingWord32());    // index
        ProcessRemainingInputs(node, 2);
        SetOutput(node, access.machine_type.representation());
        break;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));  // base
        ProcessInput(node, 1, UseInfo::TruncatingWord32());    // index
        ProcessInput(node, 2,
                     TruncatingUseInfoFromRepresentation(
                         access.machine_type.representation()));  // value
        ProcessRemainingInputs(node, 3);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) {
          WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
              access.base_is_tagged, access.machine_type.representation(),
              access.type, node->InputAt(2));
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreElement(access));
          }
        }
        break;
      }
      case IrOpcode::kObjectIsCallable:
      case IrOpcode::kObjectIsNumber:
      case IrOpcode::kObjectIsReceiver:
      case IrOpcode::kObjectIsSmi:
      case IrOpcode::kObjectIsString:
      case IrOpcode::kObjectIsUndetectable: {
        ProcessInput(node, 0, UseInfo::AnyTagged());
        SetOutput(node, MachineRepresentation::kBit);
        break;
      }

      //------------------------------------------------------------------
      // Machine-level operators.
      //------------------------------------------------------------------
      case IrOpcode::kLoad: {
        // TODO(jarin) Eventually, we should get rid of all machine stores
        // from the high-level phases, then this becomes UNREACHABLE.
        LoadRepresentation rep = LoadRepresentationOf(node->op());
        ProcessInput(node, 0, UseInfo::AnyTagged());   // tagged pointer
        ProcessInput(node, 1, UseInfo::PointerInt());  // index
        ProcessRemainingInputs(node, 2);
        SetOutput(node, rep.representation());
        break;
      }
      case IrOpcode::kStore: {
        // TODO(jarin) Eventually, we should get rid of all machine stores
        // from the high-level phases, then this becomes UNREACHABLE.
        StoreRepresentation rep = StoreRepresentationOf(node->op());
        ProcessInput(node, 0, UseInfo::AnyTagged());   // tagged pointer
        ProcessInput(node, 1, UseInfo::PointerInt());  // index
        ProcessInput(node, 2,
                     TruncatingUseInfoFromRepresentation(rep.representation()));
        ProcessRemainingInputs(node, 3);
        SetOutput(node, MachineRepresentation::kNone);
        break;
      }
      case IrOpcode::kWord32Shr:
        // We output unsigned int32 for shift right because JavaScript.
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kWord32);
      case IrOpcode::kWord32And:
      case IrOpcode::kWord32Or:
      case IrOpcode::kWord32Xor:
      case IrOpcode::kWord32Shl:
      case IrOpcode::kWord32Sar:
        // We use signed int32 as the output type for these word32 operations,
        // though the machine bits are the same for either signed or unsigned,
        // because JavaScript considers the result from these operations signed.
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kWord32);
      case IrOpcode::kWord32Equal:
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kBit);

      case IrOpcode::kWord32Clz:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kWord32);

      case IrOpcode::kInt32Add:
      case IrOpcode::kInt32Sub:
      case IrOpcode::kInt32Mul:
      case IrOpcode::kInt32MulHigh:
      case IrOpcode::kInt32Div:
      case IrOpcode::kInt32Mod:
        return VisitInt32Binop(node);
      case IrOpcode::kUint32Div:
      case IrOpcode::kUint32Mod:
      case IrOpcode::kUint32MulHigh:
        return VisitUint32Binop(node);
      case IrOpcode::kInt32LessThan:
      case IrOpcode::kInt32LessThanOrEqual:
        return VisitInt32Cmp(node);

      case IrOpcode::kUint32LessThan:
      case IrOpcode::kUint32LessThanOrEqual:
        return VisitUint32Cmp(node);

      case IrOpcode::kInt64Add:
      case IrOpcode::kInt64Sub:
      case IrOpcode::kInt64Mul:
      case IrOpcode::kInt64Div:
      case IrOpcode::kInt64Mod:
        return VisitInt64Binop(node);
      case IrOpcode::kInt64LessThan:
      case IrOpcode::kInt64LessThanOrEqual:
        return VisitInt64Cmp(node);

      case IrOpcode::kUint64LessThan:
        return VisitUint64Cmp(node);

      case IrOpcode::kUint64Div:
      case IrOpcode::kUint64Mod:
        return VisitUint64Binop(node);

      case IrOpcode::kWord64And:
      case IrOpcode::kWord64Or:
      case IrOpcode::kWord64Xor:
      case IrOpcode::kWord64Shl:
      case IrOpcode::kWord64Shr:
      case IrOpcode::kWord64Sar:
        return VisitBinop(node, UseInfo::TruncatingWord64(),
                          MachineRepresentation::kWord64);
      case IrOpcode::kWord64Equal:
        return VisitBinop(node, UseInfo::TruncatingWord64(),
                          MachineRepresentation::kBit);

      case IrOpcode::kChangeInt32ToInt64:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kWord64);
      case IrOpcode::kChangeUint32ToUint64:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kWord64);
      case IrOpcode::kTruncateFloat64ToFloat32:
        return VisitUnop(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kFloat32);
      case IrOpcode::kTruncateFloat64ToWord32:
        return VisitUnop(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kWord32);

      case IrOpcode::kChangeFloat32ToFloat64:
        UNREACHABLE();
        return VisitUnop(node, UseInfo::TruncatingFloat32(),
                         MachineRepresentation::kFloat64);
      case IrOpcode::kChangeInt32ToFloat64:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kFloat64);
      case IrOpcode::kChangeUint32ToFloat64:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kFloat64);
      case IrOpcode::kFloat64Add:
      case IrOpcode::kFloat64Sub:
      case IrOpcode::kFloat64Mul:
      case IrOpcode::kFloat64Div:
      case IrOpcode::kFloat64Mod:
      case IrOpcode::kFloat64Min:
        return VisitFloat64Binop(node);
      case IrOpcode::kFloat64Abs:
      case IrOpcode::kFloat64Sqrt:
      case IrOpcode::kFloat64RoundDown:
      case IrOpcode::kFloat64RoundTruncate:
      case IrOpcode::kFloat64RoundTiesAway:
      case IrOpcode::kFloat64RoundUp:
        return VisitUnop(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kFloat64);
      case IrOpcode::kFloat64Equal:
      case IrOpcode::kFloat64LessThan:
      case IrOpcode::kFloat64LessThanOrEqual:
        return VisitFloat64Cmp(node);
      case IrOpcode::kFloat64ExtractLowWord32:
      case IrOpcode::kFloat64ExtractHighWord32:
        return VisitUnop(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kWord32);
      case IrOpcode::kFloat64InsertLowWord32:
      case IrOpcode::kFloat64InsertHighWord32:
        return VisitBinop(node, UseInfo::TruncatingFloat64(),
                          UseInfo::TruncatingWord32(),
                          MachineRepresentation::kFloat64);
      case IrOpcode::kLoadStackPointer:
      case IrOpcode::kLoadFramePointer:
      case IrOpcode::kLoadParentFramePointer:
        return VisitLeaf(node, MachineType::PointerRepresentation());
      case IrOpcode::kStateValues:
        VisitStateValues(node);
        break;

      // The following opcodes are not produced before representation
      // inference runs, so we do not have any real test coverage.
      // Simply fail here.
      case IrOpcode::kChangeFloat64ToInt32:
      case IrOpcode::kChangeFloat64ToUint32:
      case IrOpcode::kTruncateInt64ToInt32:
        FATAL("Representation inference: unsupported opcodes.");

      default:
        VisitInputs(node);
        // Assume the output is tagged.
        SetOutput(node, MachineRepresentation::kTagged);
        break;
    }
  }

  void DeferReplacement(Node* node, Node* replacement) {
    TRACE("defer replacement #%d:%s with #%d:%s\n", node->id(),
          node->op()->mnemonic(), replacement->id(),
          replacement->op()->mnemonic());

    if (replacement->id() < count_ &&
        GetUpperBound(node)->Is(GetUpperBound(replacement))) {
      // Replace with a previously existing node eagerly only if the type is the
      // same.
      node->ReplaceUses(replacement);
    } else {
      // Otherwise, we are replacing a node with a representation change.
      // Such a substitution must be done after all lowering is done, because
      // changing the type could confuse the representation change
      // insertion for uses of the node.
      replacements_.push_back(node);
      replacements_.push_back(replacement);
    }
    node->NullAllInputs();  // Node is now dead.
  }

  void PrintOutputInfo(NodeInfo* info) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << info->representation();
    }
  }

  void PrintRepresentation(MachineRepresentation rep) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << rep;
    }
  }

  void PrintTruncation(Truncation truncation) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << truncation.description();
    }
  }

  void PrintUseInfo(UseInfo info) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << info.preferred() << ":" << info.truncation().description();
    }
  }

 private:
  JSGraph* jsgraph_;
  size_t const count_;              // number of nodes in the graph
  ZoneVector<NodeInfo> info_;       // node id -> usage information
#ifdef DEBUG
  ZoneVector<InputUseInfos> node_input_use_infos_;  // Debug information about
                                                    // requirements on inputs.
#endif                                              // DEBUG
  NodeVector nodes_;                // collected nodes
  NodeVector replacements_;         // replacements to be done after lowering
  Phase phase_;                     // current phase of algorithm
  RepresentationChanger* changer_;  // for inserting representation changes
  ZoneQueue<Node*> queue_;          // queue for traversing the graph
  // TODO(danno): RepresentationSelector shouldn't know anything about the
  // source positions table, but must for now since there currently is no other
  // way to pass down source position information to nodes created during
  // lowering. Once this phase becomes a vanilla reducer, it should get source
  // position information via the SourcePositionWrapper like all other reducers.
  SourcePositionTable* source_positions_;
  TypeCache const& type_cache_;

  NodeInfo* GetInfo(Node* node) {
    DCHECK(node->id() >= 0);
    DCHECK(node->id() < count_);
    return &info_[node->id()];
  }
};


SimplifiedLowering::SimplifiedLowering(JSGraph* jsgraph, Zone* zone,
                                       SourcePositionTable* source_positions)
    : jsgraph_(jsgraph),
      zone_(zone),
      type_cache_(TypeCache::Get()),
      source_positions_(source_positions) {}


void SimplifiedLowering::LowerAllNodes() {
  RepresentationChanger changer(jsgraph(), jsgraph()->isolate());
  RepresentationSelector selector(jsgraph(), zone_, &changer,
                                  source_positions_);
  selector.Run(this);
}

void SimplifiedLowering::DoJSToNumberTruncatesToFloat64(
    Node* node, RepresentationSelector* selector) {
  DCHECK_EQ(IrOpcode::kJSToNumber, node->opcode());
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);
  Node* throwing;

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    vtrue0 = graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);
    vtrue0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    throwing = vfalse0 = efalse0 =
        graph()->NewNode(ToNumberOperator(), ToNumberCode(), value, context,
                         frame_state, efalse0, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), throwing);

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1;
    {
      vtrue1 =
          graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);
      vtrue1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else if (edge.from()->opcode() == IrOpcode::kIfException) {
        edge.UpdateTo(throwing);
      } else {
        UNREACHABLE();
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

void SimplifiedLowering::DoJSToNumberTruncatesToWord32(
    Node* node, RepresentationSelector* selector) {
  DCHECK_EQ(IrOpcode::kJSToNumber, node->opcode());
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);
  Node* throwing;

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 =
      graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    throwing = vfalse0 = efalse0 =
        graph()->NewNode(ToNumberOperator(), ToNumberCode(), value, context,
                         frame_state, efalse0, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), throwing);

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1 =
        graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
      vfalse1 = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else if (edge.from()->opcode() == IrOpcode::kIfException) {
        edge.UpdateTo(throwing);
      } else {
        UNREACHABLE();
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

void SimplifiedLowering::DoLoadBuffer(Node* node,
                                      MachineRepresentation output_rep,
                                      RepresentationChanger* changer) {
  DCHECK_EQ(IrOpcode::kLoadBuffer, node->opcode());
  DCHECK_NE(MachineRepresentation::kNone, output_rep);
  MachineType const access_type = BufferAccessOf(node->op()).machine_type();
  if (output_rep != access_type.representation()) {
    Node* const buffer = node->InputAt(0);
    Node* const offset = node->InputAt(1);
    Node* const length = node->InputAt(2);
    Node* const effect = node->InputAt(3);
    Node* const control = node->InputAt(4);
    Node* const index =
        machine()->Is64()
            ? graph()->NewNode(machine()->ChangeUint32ToUint64(), offset)
            : offset;

    Node* check = graph()->NewNode(machine()->Uint32LessThan(), offset, length);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = graph()->NewNode(machine()->Load(access_type), buffer, index,
                                   effect, if_true);
    Type* element_type =
        Type::Intersect(NodeProperties::GetType(node), Type::Number(), zone());
    Node* vtrue = changer->GetRepresentationFor(
        etrue, access_type.representation(), element_type, output_rep,
        Truncation::None());

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse;
    if (output_rep == MachineRepresentation::kTagged) {
      vfalse = jsgraph()->UndefinedConstant();
    } else if (output_rep == MachineRepresentation::kFloat64) {
      vfalse =
          jsgraph()->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    } else if (output_rep == MachineRepresentation::kFloat32) {
      vfalse =
          jsgraph()->Float32Constant(std::numeric_limits<float>::quiet_NaN());
    } else {
      vfalse = jsgraph()->Int32Constant(0);
    }

    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);

    // Replace effect uses of {node} with the {ephi}.
    NodeProperties::ReplaceUses(node, node, ephi);

    // Turn the {node} into a Phi.
    node->ReplaceInput(0, vtrue);
    node->ReplaceInput(1, vfalse);
    node->ReplaceInput(2, merge);
    node->TrimInputCount(3);
    NodeProperties::ChangeOp(node, common()->Phi(output_rep, 2));
  } else {
    NodeProperties::ChangeOp(node, machine()->CheckedLoad(access_type));
  }
}


void SimplifiedLowering::DoStoreBuffer(Node* node) {
  DCHECK_EQ(IrOpcode::kStoreBuffer, node->opcode());
  MachineRepresentation const rep =
      BufferAccessOf(node->op()).machine_type().representation();
  NodeProperties::ChangeOp(node, machine()->CheckedStore(rep));
}

Node* SimplifiedLowering::Float64Ceil(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // Use fast hardware instruction if available.
  if (machine()->Float64RoundUp().IsSupported()) {
    return graph()->NewNode(machine()->Float64RoundUp().op(), input);
  }

  // General case for ceil.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if temp1 < input then
  //         temp1 + 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         let temp3 = (if temp1 < temp2 then temp2 - 1 else temp2) in
  //         -0 - temp3
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), temp1, input),
          graph()->NewNode(machine()->Float64Add(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        Node* temp3 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp1, temp2),
            graph()->NewNode(machine()->Float64Sub(), temp2, one), temp2);
        vfalse2 = graph()->NewNode(machine()->Float64Sub(), minus_zero, temp3);
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  return graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                          vtrue0, vfalse0, merge0);
}

Node* SimplifiedLowering::Float64Floor(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_one = jsgraph()->Float64Constant(-1.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // Use fast hardware instruction if available.
  if (machine()->Float64RoundDown().IsSupported()) {
    return graph()->NewNode(machine()->Float64RoundDown().op(), input);
  }

  // General case for floor.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if input < temp1 then
  //         temp1 - 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         if temp2 < temp1 then
  //           -1 - temp2
  //         else
  //           -0 - temp2
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, temp1),
          graph()->NewNode(machine()->Float64Sub(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        vfalse2 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp2, temp1),
            graph()->NewNode(machine()->Float64Sub(), minus_one, temp2),
            graph()->NewNode(machine()->Float64Sub(), minus_zero, temp2));
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  return graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                          vtrue0, vfalse0, merge0);
}

Node* SimplifiedLowering::Float64Round(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const one_half = jsgraph()->Float64Constant(0.5);
  Node* const input = node->InputAt(0);

  // Round up towards Infinity, and adjust if the difference exceeds 0.5.
  Node* result = Float64Ceil(node);
  return graph()->NewNode(
      common()->Select(MachineRepresentation::kFloat64),
      graph()->NewNode(
          machine()->Float64LessThanOrEqual(),
          graph()->NewNode(machine()->Float64Sub(), result, one_half), input),
      result, graph()->NewNode(machine()->Float64Sub(), result, one));
}

Node* SimplifiedLowering::Float64Trunc(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // Use fast hardware instruction if available.
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return graph()->NewNode(machine()->Float64RoundTruncate().op(), input);
  }

  // General case for trunc.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if input < temp1 then
  //         temp1 - 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         let temp3 = (if temp1 < temp2 then temp2 - 1 else temp2) in
  //         -0 - temp3
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, temp1),
          graph()->NewNode(machine()->Float64Sub(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        Node* temp3 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp1, temp2),
            graph()->NewNode(machine()->Float64Sub(), temp2, one), temp2);
        vfalse2 = graph()->NewNode(machine()->Float64Sub(), minus_zero, temp3);
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  return graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                          vtrue0, vfalse0, merge0);
}

Node* SimplifiedLowering::Int32Div(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1)) {
    return graph()->NewNode(machine()->Int32Sub(), zero, lhs);
  } else if (m.right().Is(0)) {
    return rhs;
  } else if (machine()->Int32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Div(), lhs, rhs, graph()->start());
  }

  // General case for signed integer division.
  //
  //    if 0 < rhs then
  //      lhs / rhs
  //    else
  //      if rhs < -1 then
  //        lhs / rhs
  //      else if rhs == 0 then
  //        0
  //      else
  //        0 - lhs
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true0);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(), check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = zero;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Int32Sub(), zero, lhs);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}


Node* SimplifiedLowering::Int32Mod(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1) || m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Mod(), lhs, rhs, graph()->start());
  }

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if 0 < rhs then
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       if lhs < 0 then
  //         -(-lhs & msk)
  //       else
  //         lhs & msk
  //   else
  //     if rhs < -1 then
  //       lhs % rhs
  //     else
  //       zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Int32LessThan(), lhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = graph()->NewNode(
          machine()->Int32Sub(), zero,
          graph()->NewNode(machine()->Word32And(),
                           graph()->NewNode(machine()->Int32Sub(), zero, lhs),
                           msk));

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Word32And(), lhs, msk);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = zero;

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}


Node* SimplifiedLowering::Uint32Div(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (machine()->Uint32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Div(), lhs, rhs, graph()->start());
  }

  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  Diamond d(graph(), common(), check, BranchHint::kFalse);
  Node* div = graph()->NewNode(machine()->Uint32Div(), lhs, rhs, d.if_false);
  return d.Phi(MachineRepresentation::kWord32, zero, div);
}


Node* SimplifiedLowering::Uint32Mod(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, graph()->start());
  }

  // General case for unsigned integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if rhs then
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       lhs & msk
  //   else
  //     zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), rhs,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = graph()->NewNode(machine()->Word32And(), lhs, msk);

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0 = zero;

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}


void SimplifiedLowering::DoShift(Node* node, Operator const* op,
                                 Type* rhs_type) {
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  if (!rhs_type->Is(type_cache_.kZeroToThirtyOne)) {
    node->ReplaceInput(1, graph()->NewNode(machine()->Word32And(), rhs,
                                           jsgraph()->Int32Constant(0x1f)));
  }
  NodeProperties::ChangeOp(node, op);
}

Node* SimplifiedLowering::ToNumberCode() {
  if (!to_number_code_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    to_number_code_.set(jsgraph()->HeapConstant(callable.code()));
  }
  return to_number_code_.get();
}

Operator const* SimplifiedLowering::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kNoProperties);
    to_number_operator_.set(common()->Call(desc));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
