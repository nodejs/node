// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include <limits>

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/source-position.h"
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
  static UseInfo Float32() {
    return UseInfo(MachineRepresentation::kFloat32, Truncation::Float32());
  }
  static UseInfo Float64() {
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

  // Truncation to a representation that is smaller than the preferred
  // one.
  static UseInfo Float64TruncatingToWord32() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Word32());
  }
  static UseInfo Word64TruncatingToWord32() {
    return UseInfo(MachineRepresentation::kWord64, Truncation::Word32());
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
    return UseInfo::Float64();
    case MachineRepresentation::kFloat32:
      return UseInfo::Float32();
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
  class NodeOutputInfo {
   public:
    NodeOutputInfo(MachineRepresentation representation, Type* type)
        : type_(type), representation_(representation) {}
    NodeOutputInfo()
        : type_(Type::None()), representation_(MachineRepresentation::kNone) {}

    MachineRepresentation representation() const { return representation_; }
    Type* type() const { return type_; }

    static NodeOutputInfo None() {
      return NodeOutputInfo(MachineRepresentation::kNone, Type::None());
    }

    static NodeOutputInfo Float32() {
      return NodeOutputInfo(MachineRepresentation::kFloat32, Type::Number());
    }

    static NodeOutputInfo Float64() {
      return NodeOutputInfo(MachineRepresentation::kFloat64, Type::Number());
    }

    static NodeOutputInfo NumberTruncatedToWord32() {
      return NodeOutputInfo(MachineRepresentation::kWord32, Type::Number());
    }

    static NodeOutputInfo Int32() {
      return NodeOutputInfo(MachineRepresentation::kWord32, Type::Signed32());
    }

    static NodeOutputInfo Uint32() {
      return NodeOutputInfo(MachineRepresentation::kWord32, Type::Unsigned32());
    }

    static NodeOutputInfo Bool() {
      return NodeOutputInfo(MachineRepresentation::kBit, Type::Boolean());
    }

    static NodeOutputInfo Int64() {
      // TODO(jarin) Fix once we have a real int64 type.
      return NodeOutputInfo(MachineRepresentation::kWord64, Type::Internal());
    }

    static NodeOutputInfo Uint64() {
      // TODO(jarin) Fix once we have a real uint64 type.
      return NodeOutputInfo(MachineRepresentation::kWord64, Type::Internal());
    }

    static NodeOutputInfo AnyTagged() {
      return NodeOutputInfo(MachineRepresentation::kTagged, Type::Any());
    }

    static NodeOutputInfo NumberTagged() {
      return NodeOutputInfo(MachineRepresentation::kTagged, Type::Number());
    }

    static NodeOutputInfo Pointer() {
      return NodeOutputInfo(MachineType::PointerRepresentation(), Type::Any());
    }

   private:
    Type* type_;
    MachineRepresentation representation_;
  };

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
    void set_output_type(NodeOutputInfo output) { output_ = output; }

    Type* output_type() const { return output_.type(); }
    MachineRepresentation representation() const {
      return output_.representation();
    }

   private:
    bool queued_ = false;                  // Bookkeeping for the traversal.
    bool visited_ = false;                 // Bookkeeping for the traversal.
    NodeOutputInfo output_;                // Output type and representation.
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

  void SetOutputFromMachineType(Node* node, MachineType machine_type) {
    Type* type = Type::None();
    switch (machine_type.semantic()) {
      case MachineSemantic::kNone:
        type = Type::None();
        break;
      case MachineSemantic::kBool:
        type = Type::Boolean();
        break;
      case MachineSemantic::kInt32:
        type = Type::Signed32();
        break;
      case MachineSemantic::kUint32:
        type = Type::Unsigned32();
        break;
      case MachineSemantic::kInt64:
        // TODO(jarin) Fix once we have proper int64.
        type = Type::Internal();
        break;
      case MachineSemantic::kUint64:
        // TODO(jarin) Fix once we have proper uint64.
        type = Type::Internal();
        break;
      case MachineSemantic::kNumber:
        type = Type::Number();
        break;
      case MachineSemantic::kAny:
        type = Type::Any();
        break;
    }
    return SetOutput(node, NodeOutputInfo(machine_type.representation(), type));
  }

  void SetOutput(Node* node, NodeOutputInfo output_info) {
    // Every node should have at most one output representation. Note that
    // phis can have 0, if they have not been used in a representation-inducing
    // instruction.
    Type* output_type = output_info.type();
    if (NodeProperties::IsTyped(node)) {
      output_type = Type::Intersect(NodeProperties::GetType(node),
                                    output_info.type(), jsgraph_->zone());
    }
    NodeInfo* info = GetInfo(node);
    DCHECK(info->output_type()->Is(output_type));
    DCHECK(MachineRepresentationIsSubtype(info->representation(),
                                          output_info.representation()));
    if (!output_type->Is(info->output_type()) ||
        output_info.representation() != info->representation()) {
      EnqueueUses(node);
    }
    info->set_output_type(
        NodeOutputInfo(output_info.representation(), output_type));
  }

  bool BothInputsAreSigned32(Node* node) {
    DCHECK_EQ(2, node->InputCount());
    return GetInfo(node->InputAt(0))->output_type()->Is(Type::Signed32()) &&
           GetInfo(node->InputAt(1))->output_type()->Is(Type::Signed32());
  }

  bool BothInputsAreUnsigned32(Node* node) {
    DCHECK_EQ(2, node->InputCount());
    return GetInfo(node->InputAt(0))->output_type()->Is(Type::Unsigned32()) &&
           GetInfo(node->InputAt(1))->output_type()->Is(Type::Unsigned32());
  }

  bool BothInputsAre(Node* node, Type* type) {
    DCHECK_EQ(2, node->InputCount());
    return GetInfo(node->InputAt(0))->output_type()->Is(type) &&
           GetInfo(node->InputAt(1))->output_type()->Is(type);
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
          input, input_info->representation(), input_info->output_type(),
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
                  NodeOutputInfo output) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    ProcessInput(node, 0, left_use);
    ProcessInput(node, 1, right_use);
    for (int i = 2; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
    SetOutput(node, output);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, UseInfo input_use, NodeOutputInfo output) {
    VisitBinop(node, input_use, input_use, output);
  }

  // Helper for unops of the I -> O variety.
  void VisitUnop(Node* node, UseInfo input_use, NodeOutputInfo output) {
    DCHECK_EQ(1, node->InputCount());
    ProcessInput(node, 0, input_use);
    SetOutput(node, output);
  }

  // Helper for leaf nodes.
  void VisitLeaf(Node* node, NodeOutputInfo output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput(node, output);
  }

  // Helpers for specific types of binops.
  void VisitFloat64Binop(Node* node) {
    VisitBinop(node, UseInfo::Float64(), NodeOutputInfo::Float64());
  }
  void VisitInt32Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Int32());
  }
  void VisitWord32TruncatingBinop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(),
               NodeOutputInfo::NumberTruncatedToWord32());
  }
  void VisitUint32Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Uint32());
  }
  void VisitInt64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), NodeOutputInfo::Int64());
  }
  void VisitUint64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), NodeOutputInfo::Uint64());
  }
  void VisitFloat64Cmp(Node* node) {
    VisitBinop(node, UseInfo::Float64(), NodeOutputInfo::Bool());
  }
  void VisitInt32Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Bool());
  }
  void VisitUint32Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Bool());
  }
  void VisitInt64Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), NodeOutputInfo::Bool());
  }
  void VisitUint64Cmp(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord64(), NodeOutputInfo::Bool());
  }

  // Infer representation for phi-like nodes.
  NodeOutputInfo GetOutputInfoForPhi(Node* node, Truncation use) {
    // Compute the type.
    Type* type = GetInfo(node->InputAt(0))->output_type();
    for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
      type = Type::Union(type, GetInfo(node->InputAt(i))->output_type(),
                         jsgraph_->zone());
    }

    // Compute the representation.
    MachineRepresentation rep = MachineRepresentation::kTagged;
    if (type->Is(Type::None())) {
      rep = MachineRepresentation::kNone;
    } else if (type->Is(Type::Signed32()) || type->Is(Type::Unsigned32())) {
      rep = MachineRepresentation::kWord32;
    } else if (use.TruncatesToWord32()) {
      rep = MachineRepresentation::kWord32;
    } else if (type->Is(Type::Boolean())) {
      rep = MachineRepresentation::kBit;
    } else if (type->Is(Type::Number())) {
      rep = MachineRepresentation::kFloat64;
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
      rep = is_word64 ? MachineRepresentation::kWord64
                      : MachineRepresentation::kTagged;
    }
    return NodeOutputInfo(rep, type);
  }

  // Helper for handling selects.
  void VisitSelect(Node* node, Truncation truncation,
                   SimplifiedLowering* lowering) {
    ProcessInput(node, 0, UseInfo::Bool());

    NodeOutputInfo output = GetOutputInfoForPhi(node, truncation);
    SetOutput(node, output);

    if (lower()) {
      // Update the select operator.
      SelectParameters p = SelectParametersOf(node->op());
      if (output.representation() != p.representation()) {
        NodeProperties::ChangeOp(node, lowering->common()->Select(
                                           output.representation(), p.hint()));
      }
    }
    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output.representation(), truncation);
    ProcessInput(node, 1, input_use);
    ProcessInput(node, 2, input_use);
  }

  // Helper for handling phis.
  void VisitPhi(Node* node, Truncation truncation,
                SimplifiedLowering* lowering) {
    NodeOutputInfo output = GetOutputInfoForPhi(node, truncation);
    SetOutput(node, output);

    int values = node->op()->ValueInputCount();
    if (lower()) {
      // Update the phi operator.
      if (output.representation() != PhiRepresentationOf(node->op())) {
        NodeProperties::ChangeOp(
            node, lowering->common()->Phi(output.representation(), values));
      }
    }

    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output.representation(), truncation);
    for (int i = 0; i < node->InputCount(); i++) {
      ProcessInput(node, i, i < values ? input_use : UseInfo::None());
    }
  }

  void VisitCall(Node* node, SimplifiedLowering* lowering) {
    const CallDescriptor* desc = OpParameter<const CallDescriptor*>(node->op());
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
      SetOutputFromMachineType(node, desc->GetMachineSignature()->GetReturn());
    } else {
      SetOutput(node, NodeOutputInfo::AnyTagged());
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
        NodeInfo* input_info = GetInfo(node->InputAt(i));
        MachineType machine_type(
            input_info->representation(),
            DeoptValueSemanticOf(input_info->output_type()));
        DCHECK(machine_type.representation() !=
                   MachineRepresentation::kWord32 ||
               machine_type.semantic() == MachineSemantic::kInt32 ||
               machine_type.semantic() == MachineSemantic::kUint32);
        (*types)[i] = machine_type;
      }
      NodeProperties::ChangeOp(node,
                               jsgraph_->common()->TypedStateValues(types));
    }
    SetOutput(node, NodeOutputInfo::AnyTagged());
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
        return VisitLeaf(node, NodeOutputInfo::None());
      case IrOpcode::kParameter: {
        // TODO(titzer): use representation from linkage.
        Type* type = NodeProperties::GetType(node);
        ProcessInput(node, 0, UseInfo::None());
        SetOutput(node, NodeOutputInfo(MachineRepresentation::kTagged, type));
        return;
      }
      case IrOpcode::kInt32Constant:
        return VisitLeaf(node, NodeOutputInfo::Int32());
      case IrOpcode::kInt64Constant:
        return VisitLeaf(node, NodeOutputInfo::Int64());
      case IrOpcode::kFloat32Constant:
        return VisitLeaf(node, NodeOutputInfo::Float32());
      case IrOpcode::kFloat64Constant:
        return VisitLeaf(node, NodeOutputInfo::Float64());
      case IrOpcode::kExternalConstant:
        return VisitLeaf(node, NodeOutputInfo::Pointer());
      case IrOpcode::kNumberConstant:
        return VisitLeaf(node, NodeOutputInfo::NumberTagged());
      case IrOpcode::kHeapConstant:
        return VisitLeaf(node, NodeOutputInfo::AnyTagged());

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
// For now, we assume that all JS operators were too complex to lower
// to Simplified and that they will always require tagged value inputs
// and produce tagged value outputs.
// TODO(turbofan): it might be possible to lower some JSOperators here,
// but that responsibility really lies in the typed lowering phase.
#define DEFINE_JS_CASE(x) case IrOpcode::k##x:
        JS_OP_LIST(DEFINE_JS_CASE)
#undef DEFINE_JS_CASE
        VisitInputs(node);
        return SetOutput(node, NodeOutputInfo::AnyTagged());

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
          SetOutput(node, NodeOutputInfo::Bool());
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
          SetOutput(node, NodeOutputInfo::Int32());
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
        Type* rhs_type = GetInfo(node->InputAt(1))->output_type();
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), NodeOutputInfo::Int32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shl(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberShiftRight: {
        Type* rhs_type = GetInfo(node->InputAt(1))->output_type();
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), NodeOutputInfo::Int32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Sar(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberShiftRightLogical: {
        Type* rhs_type = GetInfo(node->InputAt(1))->output_type();
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), NodeOutputInfo::Uint32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shr(), rhs_type);
        }
        break;
      }
      case IrOpcode::kNumberToInt32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Int32());
        if (lower()) DeferReplacement(node, node->InputAt(0));
        break;
      }
      case IrOpcode::kNumberToUint32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(), NodeOutputInfo::Uint32());
        if (lower()) DeferReplacement(node, node->InputAt(0));
        break;
      }
      case IrOpcode::kNumberIsHoleNaN: {
        VisitUnop(node, UseInfo::Float64(), NodeOutputInfo::Bool());
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
      case IrOpcode::kPlainPrimitiveToNumber: {
        VisitUnop(node, UseInfo::AnyTagged(), NodeOutputInfo::NumberTagged());
        if (lower()) {
          // PlainPrimitiveToNumber(x) => Call(ToNumberStub, x, no-context)
          Operator::Properties properties = node->op()->properties();
          Callable callable = CodeFactory::ToNumber(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
          NodeProperties::ChangeOp(node, jsgraph_->common()->Call(desc));
        }
        break;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), NodeOutputInfo::Bool());
        if (lower()) {
          NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
        }
        break;
      }
      case IrOpcode::kStringEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), NodeOutputInfo::Bool());
        if (lower()) lowering->DoStringEqual(node);
        break;
      }
      case IrOpcode::kStringLessThan: {
        VisitBinop(node, UseInfo::AnyTagged(), NodeOutputInfo::Bool());
        if (lower()) lowering->DoStringLessThan(node);
        break;
      }
      case IrOpcode::kStringLessThanOrEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), NodeOutputInfo::Bool());
        if (lower()) lowering->DoStringLessThanOrEqual(node);
        break;
      }
      case IrOpcode::kAllocate: {
        ProcessInput(node, 0, UseInfo::AnyTagged());
        ProcessRemainingInputs(node, 1);
        SetOutput(node, NodeOutputInfo::AnyTagged());
        break;
      }
      case IrOpcode::kLoadField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));
        ProcessRemainingInputs(node, 1);
        SetOutputFromMachineType(node, access.machine_type);
        break;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));
        ProcessInput(node, 1, TruncatingUseInfoFromRepresentation(
                                  access.machine_type.representation()));
        ProcessRemainingInputs(node, 2);
        SetOutput(node, NodeOutputInfo::None());
        break;
      }
      case IrOpcode::kLoadBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, UseInfo::PointerInt());        // buffer
        ProcessInput(node, 1, UseInfo::TruncatingWord32());  // offset
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // length
        ProcessRemainingInputs(node, 3);

        NodeOutputInfo output_info;
        if (truncation.TruncatesUndefinedToZeroOrNaN()) {
          if (truncation.TruncatesNaNToZero()) {
            // If undefined is truncated to a non-NaN number, we can use
            // the load's representation.
            output_info = NodeOutputInfo(access.machine_type().representation(),
                                         NodeProperties::GetType(node));
          } else {
            // If undefined is truncated to a number, but the use can
            // observe NaN, we need to output at least the float32
            // representation.
            if (access.machine_type().representation() ==
                MachineRepresentation::kFloat32) {
              output_info =
                  NodeOutputInfo(access.machine_type().representation(),
                                 NodeProperties::GetType(node));
            } else {
              if (access.machine_type().representation() !=
                  MachineRepresentation::kFloat64) {
                // TODO(bmeurer): See comment on abort_compilation_.
                if (lower()) lowering->abort_compilation_ = true;
              }
              output_info = NodeOutputInfo::Float64();
            }
          }
        } else {
          // TODO(bmeurer): See comment on abort_compilation_.
          if (lower()) lowering->abort_compilation_ = true;

          // If undefined is not truncated away, we need to have the tagged
          // representation.
          output_info = NodeOutputInfo::AnyTagged();
        }
        SetOutput(node, output_info);
        if (lower())
          lowering->DoLoadBuffer(node, output_info.representation(), changer_);
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
        SetOutput(node, NodeOutputInfo::None());
        if (lower()) lowering->DoStoreBuffer(node);
        break;
      }
      case IrOpcode::kLoadElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, UseInfoForBasePointer(access));  // base
        ProcessInput(node, 1, UseInfo::TruncatingWord32());    // index
        ProcessRemainingInputs(node, 2);
        SetOutputFromMachineType(node, access.machine_type);
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
        SetOutput(node, NodeOutputInfo::None());
        break;
      }
      case IrOpcode::kObjectIsNumber: {
        ProcessInput(node, 0, UseInfo::AnyTagged());
        SetOutput(node, NodeOutputInfo::Bool());
        break;
      }
      case IrOpcode::kObjectIsReceiver: {
        ProcessInput(node, 0, UseInfo::AnyTagged());
        SetOutput(node, NodeOutputInfo::Bool());
        break;
      }
      case IrOpcode::kObjectIsSmi: {
        ProcessInput(node, 0, UseInfo::AnyTagged());
        SetOutput(node, NodeOutputInfo::Bool());
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
        SetOutputFromMachineType(node, rep);
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
        SetOutput(node, NodeOutputInfo::None());
        break;
      }
      case IrOpcode::kWord32Shr:
        // We output unsigned int32 for shift right because JavaScript.
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          NodeOutputInfo::Uint32());
      case IrOpcode::kWord32And:
      case IrOpcode::kWord32Or:
      case IrOpcode::kWord32Xor:
      case IrOpcode::kWord32Shl:
      case IrOpcode::kWord32Sar:
        // We use signed int32 as the output type for these word32 operations,
        // though the machine bits are the same for either signed or unsigned,
        // because JavaScript considers the result from these operations signed.
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          NodeOutputInfo::Int32());
      case IrOpcode::kWord32Equal:
        return VisitBinop(node, UseInfo::TruncatingWord32(),
                          NodeOutputInfo::Bool());

      case IrOpcode::kWord32Clz:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         NodeOutputInfo::Uint32());

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
                          NodeOutputInfo::Int64());
      case IrOpcode::kWord64Equal:
        return VisitBinop(node, UseInfo::TruncatingWord64(),
                          NodeOutputInfo::Bool());

      case IrOpcode::kChangeInt32ToInt64:
        return VisitUnop(
            node, UseInfo::TruncatingWord32(),
            NodeOutputInfo(MachineRepresentation::kWord64, Type::Signed32()));
      case IrOpcode::kChangeUint32ToUint64:
        return VisitUnop(
            node, UseInfo::TruncatingWord32(),
            NodeOutputInfo(MachineRepresentation::kWord64, Type::Unsigned32()));
      case IrOpcode::kTruncateFloat64ToFloat32:
        return VisitUnop(node, UseInfo::Float64(), NodeOutputInfo::Float32());
      case IrOpcode::kTruncateFloat64ToInt32:
        return VisitUnop(node, UseInfo::Float64(), NodeOutputInfo::Int32());
      case IrOpcode::kTruncateInt64ToInt32:
        // TODO(titzer): Is kTypeInt32 correct here?
        return VisitUnop(node, UseInfo::Word64TruncatingToWord32(),
                         NodeOutputInfo::Int32());

      case IrOpcode::kChangeFloat32ToFloat64:
        return VisitUnop(node, UseInfo::Float32(), NodeOutputInfo::Float64());
      case IrOpcode::kChangeInt32ToFloat64:
        return VisitUnop(
            node, UseInfo::TruncatingWord32(),
            NodeOutputInfo(MachineRepresentation::kFloat64, Type::Signed32()));
      case IrOpcode::kChangeUint32ToFloat64:
        return VisitUnop(node, UseInfo::TruncatingWord32(),
                         NodeOutputInfo(MachineRepresentation::kFloat64,
                                        Type::Unsigned32()));
      case IrOpcode::kChangeFloat64ToInt32:
        return VisitUnop(node, UseInfo::Float64TruncatingToWord32(),
                         NodeOutputInfo::Int32());
      case IrOpcode::kChangeFloat64ToUint32:
        return VisitUnop(node, UseInfo::Float64TruncatingToWord32(),
                         NodeOutputInfo::Uint32());

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
        return VisitUnop(node, UseInfo::Float64(), NodeOutputInfo::Float64());
      case IrOpcode::kFloat64Equal:
      case IrOpcode::kFloat64LessThan:
      case IrOpcode::kFloat64LessThanOrEqual:
        return VisitFloat64Cmp(node);
      case IrOpcode::kFloat64ExtractLowWord32:
      case IrOpcode::kFloat64ExtractHighWord32:
        return VisitUnop(node, UseInfo::Float64(), NodeOutputInfo::Int32());
      case IrOpcode::kFloat64InsertLowWord32:
      case IrOpcode::kFloat64InsertHighWord32:
        return VisitBinop(node, UseInfo::Float64(), UseInfo::TruncatingWord32(),
                          NodeOutputInfo::Float64());
      case IrOpcode::kLoadStackPointer:
      case IrOpcode::kLoadFramePointer:
      case IrOpcode::kLoadParentFramePointer:
        return VisitLeaf(node, NodeOutputInfo::Pointer());
      case IrOpcode::kStateValues:
        VisitStateValues(node);
        break;
      default:
        VisitInputs(node);
        // Assume the output is tagged.
        SetOutput(node, NodeOutputInfo::AnyTagged());
        break;
    }
  }

  void DeferReplacement(Node* node, Node* replacement) {
    TRACE("defer replacement #%d:%s with #%d:%s\n", node->id(),
          node->op()->mnemonic(), replacement->id(),
          replacement->op()->mnemonic());

    if (replacement->id() < count_ &&
        GetInfo(node)->output_type()->Is(GetInfo(replacement)->output_type())) {
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
      os << info->representation() << " (";
      info->output_type()->PrintTo(os, Type::SEMANTIC_DIM);
      os << ")";
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
    Node* vtrue = changer->GetRepresentationFor(
        etrue, access_type.representation(), NodeProperties::GetType(node),
        output_rep, Truncation::None());

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


Node* SimplifiedLowering::StringComparison(Node* node) {
  Operator::Properties properties = node->op()->properties();
  Callable callable = CodeFactory::StringCompare(isolate());
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, flags, properties);
  return graph()->NewNode(
      common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
      NodeProperties::GetValueInput(node, 0),
      NodeProperties::GetValueInput(node, 1), jsgraph()->NoContextConstant(),
      NodeProperties::GetEffectInput(node),
      NodeProperties::GetControlInput(node));
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


namespace {

void ReplaceEffectUses(Node* node, Node* replacement) {
  // Requires distinguishing between value and effect edges.
  DCHECK(replacement->op()->EffectOutputCount() > 0);
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(replacement);
    } else {
      DCHECK(NodeProperties::IsValueEdge(edge));
    }
  }
}

}  // namespace


void SimplifiedLowering::DoStringEqual(Node* node) {
  Node* comparison = StringComparison(node);
  ReplaceEffectUses(node, comparison);
  node->ReplaceInput(0, comparison);
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, machine()->WordEqual());
}


void SimplifiedLowering::DoStringLessThan(Node* node) {
  Node* comparison = StringComparison(node);
  ReplaceEffectUses(node, comparison);
  node->ReplaceInput(0, comparison);
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, machine()->IntLessThan());
}


void SimplifiedLowering::DoStringLessThanOrEqual(Node* node) {
  Node* comparison = StringComparison(node);
  ReplaceEffectUses(node, comparison);
  node->ReplaceInput(0, comparison);
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, machine()->IntLessThanOrEqual());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
