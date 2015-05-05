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
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/source-position.h"
#include "src/objects.h"

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


class RepresentationSelector {
 public:
  // Information for each node tracked during the fixpoint.
  struct NodeInfo {
    MachineTypeUnion use : 15;     // Union of all usages for the node.
    bool queued : 1;           // Bookkeeping for the traversal.
    bool visited : 1;          // Bookkeeping for the traversal.
    MachineTypeUnion output : 15;  // Output type of the node.
  };

  RepresentationSelector(JSGraph* jsgraph, Zone* zone,
                         RepresentationChanger* changer,
                         SourcePositionTable* source_positions)
      : jsgraph_(jsgraph),
        count_(jsgraph->graph()->NodeCount()),
        info_(zone->NewArray<NodeInfo>(count_)),
        nodes_(zone),
        replacements_(zone),
        phase_(PROPAGATE),
        changer_(changer),
        queue_(zone),
        source_positions_(source_positions) {
    memset(info_, 0, sizeof(NodeInfo) * count_);

    safe_int_additive_range_ =
        Type::Range(-std::pow(2.0, 52.0), std::pow(2.0, 52.0), zone);
  }

  void Run(SimplifiedLowering* lowering) {
    // Run propagation phase to a fixpoint.
    TRACE("--{Propagation phase}--\n");
    phase_ = PROPAGATE;
    Enqueue(jsgraph_->graph()->end());
    // Process nodes from the queue until it is empty.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      NodeInfo* info = GetInfo(node);
      queue_.pop();
      info->queued = false;
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      VisitNode(node, info->use, NULL);
      TRACE("  ==> output ");
      PrintInfo(info->output);
      TRACE("\n");
    }

    // Run lowering and change insertion phase.
    TRACE("--{Simplified lowering phase}--\n");
    phase_ = LOWER;
    // Process nodes from the collected {nodes_} vector.
    for (NodeVector::iterator i = nodes_.begin(); i != nodes_.end(); ++i) {
      Node* node = *i;
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      // Reuse {VisitNode()} so the representation rules are in one place.
      if (FLAG_turbo_source_positions) {
        SourcePositionTable::Scope scope(
            source_positions_, source_positions_->GetSourcePosition(node));
        VisitNode(node, GetUseInfo(node), lowering);
      } else {
        VisitNode(node, GetUseInfo(node), lowering);
      }
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

  // Enqueue {node} if the {use} contains new information for that node.
  // Add {node} to {nodes_} if this is the first time it's been visited.
  void Enqueue(Node* node, MachineTypeUnion use = 0) {
    if (phase_ != PROPAGATE) return;
    NodeInfo* info = GetInfo(node);
    if (!info->visited) {
      // First visit of this node.
      info->visited = true;
      info->queued = true;
      nodes_.push_back(node);
      queue_.push(node);
      TRACE("  initial: ");
      info->use |= use;
      PrintUseInfo(node);
      return;
    }
    TRACE("   queue?: ");
    PrintUseInfo(node);
    if ((info->use & use) != use) {
      // New usage information for the node is available.
      if (!info->queued) {
        queue_.push(node);
        info->queued = true;
        TRACE("   added: ");
      } else {
        TRACE(" inqueue: ");
      }
      info->use |= use;
      PrintUseInfo(node);
    }
  }

  bool lower() { return phase_ == LOWER; }

  void Enqueue(Node* node, MachineType use) {
    Enqueue(node, static_cast<MachineTypeUnion>(use));
  }

  void SetOutput(Node* node, MachineTypeUnion output) {
    // Every node should have at most one output representation. Note that
    // phis can have 0, if they have not been used in a representation-inducing
    // instruction.
    DCHECK((output & kRepMask) == 0 ||
           base::bits::IsPowerOfTwo32(output & kRepMask));
    GetInfo(node)->output = output;
  }

  bool BothInputsAre(Node* node, Type* type) {
    DCHECK_EQ(2, node->InputCount());
    return NodeProperties::GetBounds(node->InputAt(0)).upper->Is(type) &&
           NodeProperties::GetBounds(node->InputAt(1)).upper->Is(type);
  }

  void ProcessTruncateWord32Input(Node* node, int index, MachineTypeUnion use) {
    Node* input = node->InputAt(index);
    if (phase_ == PROPAGATE) {
      // In the propagate phase, propagate the usage information backward.
      Enqueue(input, use);
    } else {
      // In the change phase, insert a change before the use if necessary.
      MachineTypeUnion output = GetInfo(input)->output;
      if ((output & (kRepBit | kRepWord8 | kRepWord16 | kRepWord32)) == 0) {
        // Output representation doesn't match usage.
        TRACE("  truncate-to-int32: #%d:%s(@%d #%d:%s) ", node->id(),
              node->op()->mnemonic(), index, input->id(),
              input->op()->mnemonic());
        TRACE(" from ");
        PrintInfo(output);
        TRACE(" to ");
        PrintInfo(use);
        TRACE("\n");
        Node* n = changer_->GetTruncatedWord32For(input, output);
        node->ReplaceInput(index, n);
      }
    }
  }

  void ProcessInput(Node* node, int index, MachineTypeUnion use) {
    Node* input = node->InputAt(index);
    if (phase_ == PROPAGATE) {
      // In the propagate phase, propagate the usage information backward.
      Enqueue(input, use);
    } else {
      // In the change phase, insert a change before the use if necessary.
      if ((use & kRepMask) == 0) return;  // No input requirement on the use.
      MachineTypeUnion output = GetInfo(input)->output;
      if ((output & kRepMask & use) == 0) {
        // Output representation doesn't match usage.
        TRACE("  change: #%d:%s(@%d #%d:%s) ", node->id(),
              node->op()->mnemonic(), index, input->id(),
              input->op()->mnemonic());
        TRACE(" from ");
        PrintInfo(output);
        TRACE(" to ");
        PrintInfo(use);
        TRACE("\n");
        Node* n = changer_->GetRepresentationFor(input, output, use);
        node->ReplaceInput(index, n);
      }
    }
  }

  void ProcessRemainingInputs(Node* node, int index) {
    DCHECK_GE(index, NodeProperties::PastValueIndex(node));
    DCHECK_GE(index, NodeProperties::PastContextIndex(node));
    for (int i = std::max(index, NodeProperties::FirstEffectIndex(node));
         i < NodeProperties::PastEffectIndex(node); ++i) {
      Enqueue(node->InputAt(i));  // Effect inputs: just visit
    }
    for (int i = std::max(index, NodeProperties::FirstControlIndex(node));
         i < NodeProperties::PastControlIndex(node); ++i) {
      Enqueue(node->InputAt(i));  // Control inputs: just visit
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
      ProcessInput(node, i, kMachAnyTagged);
    }
    // Only enqueue other inputs (framestates, effects, control).
    for (int i = tagged_count; i < node->InputCount(); i++) {
      Enqueue(node->InputAt(i));
    }
    // Assume the output is tagged.
    SetOutput(node, kMachAnyTagged);
  }

  // Helper for binops of the R x L -> O variety.
  void VisitBinop(Node* node, MachineTypeUnion left_use,
                  MachineTypeUnion right_use, MachineTypeUnion output) {
    DCHECK_EQ(2, node->InputCount());
    ProcessInput(node, 0, left_use);
    ProcessInput(node, 1, right_use);
    SetOutput(node, output);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, MachineTypeUnion input_use,
                  MachineTypeUnion output) {
    VisitBinop(node, input_use, input_use, output);
  }

  // Helper for unops of the I -> O variety.
  void VisitUnop(Node* node, MachineTypeUnion input_use,
                 MachineTypeUnion output) {
    DCHECK_EQ(1, node->InputCount());
    ProcessInput(node, 0, input_use);
    SetOutput(node, output);
  }

  // Helper for leaf nodes.
  void VisitLeaf(Node* node, MachineTypeUnion output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput(node, output);
  }

  // Helpers for specific types of binops.
  void VisitFloat64Binop(Node* node) {
    VisitBinop(node, kMachFloat64, kMachFloat64);
  }
  void VisitInt32Binop(Node* node) { VisitBinop(node, kMachInt32, kMachInt32); }
  void VisitUint32Binop(Node* node) {
    VisitBinop(node, kMachUint32, kMachUint32);
  }
  void VisitInt64Binop(Node* node) { VisitBinop(node, kMachInt64, kMachInt64); }
  void VisitUint64Binop(Node* node) {
    VisitBinop(node, kMachUint64, kMachUint64);
  }
  void VisitFloat64Cmp(Node* node) { VisitBinop(node, kMachFloat64, kRepBit); }
  void VisitInt32Cmp(Node* node) { VisitBinop(node, kMachInt32, kRepBit); }
  void VisitUint32Cmp(Node* node) { VisitBinop(node, kMachUint32, kRepBit); }
  void VisitInt64Cmp(Node* node) { VisitBinop(node, kMachInt64, kRepBit); }
  void VisitUint64Cmp(Node* node) { VisitBinop(node, kMachUint64, kRepBit); }

  // Infer representation for phi-like nodes.
  MachineType GetRepresentationForPhi(Node* node, MachineTypeUnion use) {
    // Phis adapt to the output representation their uses demand.
    Type* upper = NodeProperties::GetBounds(node).upper;
    if ((use & kRepMask) == kRepTagged) {
      // only tagged uses.
      return kRepTagged;
    } else if (upper->Is(Type::Integral32())) {
      // Integer within [-2^31, 2^32[ range.
      if ((use & kRepMask) == kRepFloat64) {
        // only float64 uses.
        return kRepFloat64;
      } else if (upper->Is(Type::Signed32()) || upper->Is(Type::Unsigned32())) {
        // multiple uses, but we are within 32 bits range => pick kRepWord32.
        return kRepWord32;
      } else if (((use & kRepMask) == kRepWord32 &&
                  !CanObserveNonWord32(use)) ||
                 (use & kTypeMask) == kTypeInt32 ||
                 (use & kTypeMask) == kTypeUint32) {
        // We only use 32 bits or we use the result consistently.
        return kRepWord32;
      } else {
        return kRepFloat64;
      }
    } else if (upper->Is(Type::Boolean())) {
      // multiple uses => pick kRepBit.
      return kRepBit;
    } else if (upper->Is(Type::Number())) {
      // multiple uses => pick kRepFloat64.
      return kRepFloat64;
    }
    return kRepTagged;
  }

  // Helper for handling selects.
  void VisitSelect(Node* node, MachineTypeUnion use,
                   SimplifiedLowering* lowering) {
    ProcessInput(node, 0, kRepBit);
    MachineType output = GetRepresentationForPhi(node, use);

    Type* upper = NodeProperties::GetBounds(node).upper;
    MachineType output_type =
        static_cast<MachineType>(changer_->TypeFromUpperBound(upper) | output);
    SetOutput(node, output_type);

    if (lower()) {
      // Update the select operator.
      SelectParameters p = SelectParametersOf(node->op());
      MachineType type = static_cast<MachineType>(output_type);
      if (type != p.type()) {
        node->set_op(lowering->common()->Select(type, p.hint()));
      }

      // Convert inputs to the output representation of this select.
      ProcessInput(node, 1, output_type);
      ProcessInput(node, 2, output_type);
    } else {
      // Propagate {use} of the select to value inputs.
      MachineType use_type =
          static_cast<MachineType>((use & kTypeMask) | output);
      ProcessInput(node, 1, use_type);
      ProcessInput(node, 2, use_type);
    }
  }

  // Helper for handling phis.
  void VisitPhi(Node* node, MachineTypeUnion use,
                SimplifiedLowering* lowering) {
    MachineType output = GetRepresentationForPhi(node, use);

    Type* upper = NodeProperties::GetBounds(node).upper;
    MachineType output_type =
        static_cast<MachineType>(changer_->TypeFromUpperBound(upper) | output);
    SetOutput(node, output_type);

    int values = node->op()->ValueInputCount();

    if (lower()) {
      // Update the phi operator.
      MachineType type = static_cast<MachineType>(output_type);
      if (type != OpParameter<MachineType>(node)) {
        node->set_op(lowering->common()->Phi(type, values));
      }

      // Convert inputs to the output representation of this phi.
      for (int i = 0; i < node->InputCount(); i++) {
        ProcessInput(node, i, i < values ? output_type : 0);
      }
    } else {
      // Propagate {use} of the phi to value inputs, and 0 to control.
      MachineType use_type =
          static_cast<MachineType>((use & kTypeMask) | output);
      for (int i = 0; i < node->InputCount(); i++) {
        ProcessInput(node, i, i < values ? use_type : 0);
      }
    }
  }

  void VisitStateValues(Node* node) {
    if (phase_ == PROPAGATE) {
      for (int i = 0; i < node->InputCount(); i++) {
        Enqueue(node->InputAt(i), kTypeAny);
      }
    } else {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          new (zone->New(sizeof(ZoneVector<MachineType>)))
              ZoneVector<MachineType>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        MachineTypeUnion input_type = GetInfo(node->InputAt(i))->output;
        (*types)[i] = static_cast<MachineType>(input_type);
      }
      node->set_op(jsgraph_->common()->TypedStateValues(types));
    }
    SetOutput(node, kMachAnyTagged);
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

  bool CanLowerToInt32Binop(Node* node, MachineTypeUnion use) {
    return BothInputsAre(node, Type::Signed32()) && !CanObserveNonInt32(use);
  }

  bool CanLowerToInt32AdditiveBinop(Node* node, MachineTypeUnion use) {
    return BothInputsAre(node, safe_int_additive_range_) &&
           !CanObserveNonInt32(use);
  }

  bool CanLowerToUint32Binop(Node* node, MachineTypeUnion use) {
    return BothInputsAre(node, Type::Unsigned32()) && !CanObserveNonUint32(use);
  }

  bool CanLowerToUint32AdditiveBinop(Node* node, MachineTypeUnion use) {
    return BothInputsAre(node, safe_int_additive_range_) &&
           !CanObserveNonUint32(use);
  }

  bool CanObserveNonWord32(MachineTypeUnion use) {
    return (use & ~(kTypeUint32 | kTypeInt32)) != 0;
  }

  bool CanObserveNonInt32(MachineTypeUnion use) {
    return (use & (kTypeUint32 | kTypeNumber | kTypeAny)) != 0;
  }

  bool CanObserveMinusZero(MachineTypeUnion use) {
    // TODO(turbofan): technically Uint32 cannot observe minus zero either.
    return (use & (kTypeUint32 | kTypeNumber | kTypeAny)) != 0;
  }

  bool CanObserveNaN(MachineTypeUnion use) {
    return (use & (kTypeNumber | kTypeAny)) != 0;
  }

  bool CanObserveNonUint32(MachineTypeUnion use) {
    return (use & (kTypeInt32 | kTypeNumber | kTypeAny)) != 0;
  }

  // Dispatching routine for visiting the node {node} with the usage {use}.
  // Depending on the operator, propagate new usage info to the inputs.
  void VisitNode(Node* node, MachineTypeUnion use,
                 SimplifiedLowering* lowering) {
    switch (node->opcode()) {
      //------------------------------------------------------------------
      // Common operators.
      //------------------------------------------------------------------
      case IrOpcode::kStart:
      case IrOpcode::kDead:
        return VisitLeaf(node, 0);
      case IrOpcode::kParameter: {
        // TODO(titzer): use representation from linkage.
        Type* upper = NodeProperties::GetBounds(node).upper;
        ProcessInput(node, 0, 0);
        SetOutput(node, kRepTagged | changer_->TypeFromUpperBound(upper));
        return;
      }
      case IrOpcode::kAlways:
        return VisitLeaf(node, kRepBit);
      case IrOpcode::kInt32Constant:
        return VisitLeaf(node, kRepWord32);
      case IrOpcode::kInt64Constant:
        return VisitLeaf(node, kRepWord64);
      case IrOpcode::kFloat64Constant:
        return VisitLeaf(node, kRepFloat64);
      case IrOpcode::kExternalConstant:
        return VisitLeaf(node, kMachPtr);
      case IrOpcode::kNumberConstant:
        return VisitLeaf(node, kRepTagged);
      case IrOpcode::kHeapConstant:
        return VisitLeaf(node, kRepTagged);

      case IrOpcode::kBranch:
        ProcessInput(node, 0, kRepBit);
        Enqueue(NodeProperties::GetControlInput(node, 0));
        break;
      case IrOpcode::kSwitch:
        ProcessInput(node, 0, kRepWord32);
        Enqueue(NodeProperties::GetControlInput(node, 0));
        break;
      case IrOpcode::kSelect:
        return VisitSelect(node, use, lowering);
      case IrOpcode::kPhi:
        return VisitPhi(node, use, lowering);

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
        return SetOutput(node, kRepTagged);

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower()) {
          MachineTypeUnion input = GetInfo(node->InputAt(0))->output;
          if (input & kRepBit) {
            // BooleanNot(x: kRepBit) => Word32Equal(x, #0)
            node->set_op(lowering->machine()->Word32Equal());
            node->AppendInput(jsgraph_->zone(), jsgraph_->Int32Constant(0));
          } else {
            // BooleanNot(x: kRepTagged) => WordEqual(x, #false)
            node->set_op(lowering->machine()->WordEqual());
            node->AppendInput(jsgraph_->zone(), jsgraph_->FalseConstant());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, kTypeBool);
          SetOutput(node, kRepBit);
        }
        break;
      }
      case IrOpcode::kBooleanToNumber: {
        if (lower()) {
          MachineTypeUnion input = GetInfo(node->InputAt(0))->output;
          if (input & kRepBit) {
            // BooleanToNumber(x: kRepBit) => x
            DeferReplacement(node, node->InputAt(0));
          } else {
            // BooleanToNumber(x: kRepTagged) => WordEqual(x, #true)
            node->set_op(lowering->machine()->WordEqual());
            node->AppendInput(jsgraph_->zone(), jsgraph_->TrueConstant());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, kTypeBool);
          SetOutput(node, kMachInt32);
        }
        break;
      }
      case IrOpcode::kNumberEqual:
      case IrOpcode::kNumberLessThan:
      case IrOpcode::kNumberLessThanOrEqual: {
        // Number comparisons reduce to integer comparisons for integer inputs.
        if (BothInputsAre(node, Type::Signed32())) {
          // => signed Int32Cmp
          VisitInt32Cmp(node);
          if (lower()) node->set_op(Int32Op(node));
        } else if (BothInputsAre(node, Type::Unsigned32())) {
          // => unsigned Int32Cmp
          VisitUint32Cmp(node);
          if (lower()) node->set_op(Uint32Op(node));
        } else {
          // => Float64Cmp
          VisitFloat64Cmp(node);
          if (lower()) node->set_op(Float64Op(node));
        }
        break;
      }
      case IrOpcode::kNumberAdd:
      case IrOpcode::kNumberSubtract: {
        // Add and subtract reduce to Int32Add/Sub if the inputs
        // are already integers and all uses are truncating.
        if (CanLowerToInt32Binop(node, use)) {
          // => signed Int32Add/Sub
          VisitInt32Binop(node);
          if (lower()) node->set_op(Int32Op(node));
        } else if (CanLowerToInt32AdditiveBinop(node, use)) {
          // => signed Int32Add/Sub, truncating inputs
          ProcessTruncateWord32Input(node, 0, kTypeInt32);
          ProcessTruncateWord32Input(node, 1, kTypeInt32);
          SetOutput(node, kMachInt32);
          if (lower()) node->set_op(Int32Op(node));
        } else if (CanLowerToUint32Binop(node, use)) {
          // => unsigned Int32Add/Sub
          VisitUint32Binop(node);
          if (lower()) node->set_op(Uint32Op(node));
        } else if (CanLowerToUint32AdditiveBinop(node, use)) {
          // => signed Int32Add/Sub, truncating inputs
          ProcessTruncateWord32Input(node, 0, kTypeUint32);
          ProcessTruncateWord32Input(node, 1, kTypeUint32);
          SetOutput(node, kMachUint32);
          if (lower()) node->set_op(Uint32Op(node));
        } else {
          // => Float64Add/Sub
          VisitFloat64Binop(node);
          if (lower()) node->set_op(Float64Op(node));
        }
        break;
      }
      case IrOpcode::kNumberMultiply: {
        NumberMatcher right(node->InputAt(1));
        if (right.IsInRange(-1048576, 1048576)) {  // must fit double mantissa.
          if (CanLowerToInt32Binop(node, use)) {
            // => signed Int32Mul
            VisitInt32Binop(node);
            if (lower()) node->set_op(Int32Op(node));
            break;
          }
        }
        // => Float64Mul
        VisitFloat64Binop(node);
        if (lower()) node->set_op(Float64Op(node));
        break;
      }
      case IrOpcode::kNumberDivide: {
        if (CanLowerToInt32Binop(node, use)) {
          // => signed Int32Div
          VisitInt32Binop(node);
          if (lower()) DeferReplacement(node, lowering->Int32Div(node));
          break;
        }
        if (BothInputsAre(node, Type::Unsigned32()) && !CanObserveNaN(use)) {
          // => unsigned Uint32Div
          VisitUint32Binop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Div(node));
          break;
        }
        // => Float64Div
        VisitFloat64Binop(node);
        if (lower()) node->set_op(Float64Op(node));
        break;
      }
      case IrOpcode::kNumberModulus: {
        if (CanLowerToInt32Binop(node, use)) {
          // => signed Int32Mod
          VisitInt32Binop(node);
          if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
          break;
        }
        if (BothInputsAre(node, Type::Unsigned32()) && !CanObserveNaN(use)) {
          // => unsigned Uint32Mod
          VisitUint32Binop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
          break;
        }
        // => Float64Mod
        VisitFloat64Binop(node);
        if (lower()) node->set_op(Float64Op(node));
        break;
      }
      case IrOpcode::kNumberToInt32: {
        MachineTypeUnion use_rep = use & kRepMask;
        Node* input = node->InputAt(0);
        Type* in_upper = NodeProperties::GetBounds(input).upper;
        MachineTypeUnion in = GetInfo(input)->output;
        if (in_upper->Is(Type::Signed32())) {
          // If the input has type int32, pass through representation.
          VisitUnop(node, kTypeInt32 | use_rep, kTypeInt32 | use_rep);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if ((in & kTypeMask) == kTypeUint32 ||
                   in_upper->Is(Type::Unsigned32())) {
          // Just change representation if necessary.
          VisitUnop(node, kTypeUint32 | kRepWord32, kTypeInt32 | kRepWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if ((in & kTypeMask) == kTypeInt32 ||
                   (in & kRepMask) == kRepWord32) {
          // Just change representation if necessary.
          VisitUnop(node, kTypeInt32 | kRepWord32, kTypeInt32 | kRepWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          // Require the input in float64 format and perform truncation.
          // TODO(turbofan): avoid a truncation with a smi check.
          VisitUnop(node, kTypeInt32 | kRepFloat64, kTypeInt32 | kRepWord32);
          if (lower())
            node->set_op(lowering->machine()->TruncateFloat64ToInt32());
        }
        break;
      }
      case IrOpcode::kNumberToUint32: {
        MachineTypeUnion use_rep = use & kRepMask;
        Node* input = node->InputAt(0);
        Type* in_upper = NodeProperties::GetBounds(input).upper;
        MachineTypeUnion in = GetInfo(input)->output;
        if (in_upper->Is(Type::Unsigned32())) {
          // If the input has type uint32, pass through representation.
          VisitUnop(node, kTypeUint32 | use_rep, kTypeUint32 | use_rep);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if ((in & kTypeMask) == kTypeInt32 ||
                   in_upper->Is(Type::Signed32())) {
          // Just change representation if necessary.
          VisitUnop(node, kTypeInt32 | kRepWord32, kTypeUint32 | kRepWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if ((in & kTypeMask) == kTypeUint32 ||
                   (in & kRepMask) == kRepWord32) {
          // Just change representation if necessary.
          VisitUnop(node, kTypeUint32 | kRepWord32, kTypeUint32 | kRepWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          // Require the input in float64 format and perform truncation.
          // TODO(turbofan): avoid a truncation with a smi check.
          VisitUnop(node, kTypeUint32 | kRepFloat64, kTypeUint32 | kRepWord32);
          if (lower())
            node->set_op(lowering->machine()->TruncateFloat64ToInt32());
        }
        break;
      }
      case IrOpcode::kPlainPrimitiveToNumber: {
        VisitUnop(node, kMachAnyTagged, kTypeNumber | kRepTagged);
        if (lower()) {
          // PlainPrimitiveToNumber(x) => Call(ToNumberStub, x, no-context)
          Operator::Properties properties = node->op()->properties();
          Callable callable = CodeFactory::ToNumber(jsgraph_->isolate());
          CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
          CallDescriptor* desc = Linkage::GetStubCallDescriptor(
              jsgraph_->isolate(), jsgraph_->zone(), callable.descriptor(), 0,
              flags, properties);
          node->set_op(jsgraph_->common()->Call(desc));
          node->InsertInput(jsgraph_->zone(), 0,
                            jsgraph_->HeapConstant(callable.code()));
          node->AppendInput(jsgraph_->zone(), jsgraph_->NoContextConstant());
        }
        break;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop(node, kMachAnyTagged, kRepBit);
        if (lower()) node->set_op(lowering->machine()->WordEqual());
        break;
      }
      case IrOpcode::kStringEqual: {
        VisitBinop(node, kMachAnyTagged, kRepBit);
        if (lower()) lowering->DoStringEqual(node);
        break;
      }
      case IrOpcode::kStringLessThan: {
        VisitBinop(node, kMachAnyTagged, kRepBit);
        if (lower()) lowering->DoStringLessThan(node);
        break;
      }
      case IrOpcode::kStringLessThanOrEqual: {
        VisitBinop(node, kMachAnyTagged, kRepBit);
        if (lower()) lowering->DoStringLessThanOrEqual(node);
        break;
      }
      case IrOpcode::kStringAdd: {
        VisitBinop(node, kMachAnyTagged, kMachAnyTagged);
        if (lower()) lowering->DoStringAdd(node);
        break;
      }
      case IrOpcode::kLoadField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessRemainingInputs(node, 1);
        SetOutput(node, access.machine_type);
        if (lower()) lowering->DoLoadField(node);
        break;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, access.machine_type);
        ProcessRemainingInputs(node, 2);
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreField(node);
        break;
      }
      case IrOpcode::kLoadBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, kMachPtr);    // buffer
        ProcessInput(node, 1, kMachInt32);  // offset
        ProcessInput(node, 2, kMachInt32);  // length
        ProcessRemainingInputs(node, 3);
        // Tagged overrides everything if we have to do a typed array bounds
        // check, because we may need to return undefined then.
        MachineType output_type;
        if (use & kRepTagged) {
          output_type = kMachAnyTagged;
        } else if (use & kRepFloat64) {
          if (access.machine_type() & kRepFloat32) {
            output_type = access.machine_type();
          } else {
            output_type = kMachFloat64;
          }
        } else if (use & kRepFloat32) {
          output_type = kMachFloat32;
        } else {
          output_type = access.machine_type();
        }
        SetOutput(node, output_type);
        if (lower()) lowering->DoLoadBuffer(node, output_type, changer_);
        break;
      }
      case IrOpcode::kStoreBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, kMachPtr);               // buffer
        ProcessInput(node, 1, kMachInt32);             // offset
        ProcessInput(node, 2, kMachInt32);             // length
        ProcessInput(node, 3, access.machine_type());  // value
        ProcessRemainingInputs(node, 4);
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreBuffer(node);
        break;
      }
      case IrOpcode::kLoadElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));  // base
        ProcessInput(node, 1, kMachInt32);                            // index
        ProcessRemainingInputs(node, 2);
        SetOutput(node, access.machine_type);
        if (lower()) lowering->DoLoadElement(node);
        break;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));  // base
        ProcessInput(node, 1, kMachInt32);                            // index
        ProcessInput(node, 2, access.machine_type);                   // value
        ProcessRemainingInputs(node, 3);
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreElement(node);
        break;
      }
      case IrOpcode::kObjectIsSmi: {
        ProcessInput(node, 0, kMachAnyTagged);
        SetOutput(node, kRepBit | kTypeBool);
        if (lower()) {
          Node* is_tagged = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->WordAnd(), node->InputAt(0),
              jsgraph_->IntPtrConstant(kSmiTagMask));
          Node* is_smi = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->WordEqual(), is_tagged,
              jsgraph_->IntPtrConstant(kSmiTag));
          DeferReplacement(node, is_smi);
        }
        break;
      }
      case IrOpcode::kObjectIsNonNegativeSmi: {
        ProcessInput(node, 0, kMachAnyTagged);
        SetOutput(node, kRepBit | kTypeBool);
        if (lower()) {
          Node* is_tagged = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->WordAnd(), node->InputAt(0),
              jsgraph_->IntPtrConstant(kSmiTagMask));
          Node* is_smi = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->WordEqual(), is_tagged,
              jsgraph_->IntPtrConstant(kSmiTag));
          Node* is_non_neg = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->IntLessThanOrEqual(),
              jsgraph_->IntPtrConstant(0), node->InputAt(0));
          Node* is_non_neg_smi = jsgraph_->graph()->NewNode(
              jsgraph_->machine()->Word32And(), is_smi, is_non_neg);
          DeferReplacement(node, is_non_neg_smi);
        }
        break;
      }

      //------------------------------------------------------------------
      // Machine-level operators.
      //------------------------------------------------------------------
      case IrOpcode::kLoad: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        MachineTypeUnion tBase = kRepTagged | kMachPtr;
        LoadRepresentation rep = OpParameter<LoadRepresentation>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kMachIntPtr);  // index
        ProcessRemainingInputs(node, 2);
        SetOutput(node, rep);
        break;
      }
      case IrOpcode::kStore: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        MachineTypeUnion tBase = kRepTagged | kMachPtr;
        StoreRepresentation rep = OpParameter<StoreRepresentation>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kMachIntPtr);  // index
        ProcessInput(node, 2, rep.machine_type());
        ProcessRemainingInputs(node, 3);
        SetOutput(node, 0);
        break;
      }
      case IrOpcode::kWord32Shr:
        // We output unsigned int32 for shift right because JavaScript.
        return VisitBinop(node, kMachUint32, kMachUint32);
      case IrOpcode::kWord32And:
      case IrOpcode::kWord32Or:
      case IrOpcode::kWord32Xor:
      case IrOpcode::kWord32Shl:
      case IrOpcode::kWord32Sar:
        // We use signed int32 as the output type for these word32 operations,
        // though the machine bits are the same for either signed or unsigned,
        // because JavaScript considers the result from these operations signed.
        return VisitBinop(node, kRepWord32, kRepWord32 | kTypeInt32);
      case IrOpcode::kWord32Equal:
        return VisitBinop(node, kRepWord32, kRepBit);

      case IrOpcode::kWord32Clz:
        return VisitUnop(node, kMachUint32, kMachUint32);

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
        return VisitBinop(node, kRepWord64, kRepWord64);
      case IrOpcode::kWord64Equal:
        return VisitBinop(node, kRepWord64, kRepBit);

      case IrOpcode::kChangeInt32ToInt64:
        return VisitUnop(node, kTypeInt32 | kRepWord32,
                         kTypeInt32 | kRepWord64);
      case IrOpcode::kChangeUint32ToUint64:
        return VisitUnop(node, kTypeUint32 | kRepWord32,
                         kTypeUint32 | kRepWord64);
      case IrOpcode::kTruncateFloat64ToFloat32:
        return VisitUnop(node, kTypeNumber | kRepFloat64,
                         kTypeNumber | kRepFloat32);
      case IrOpcode::kTruncateInt64ToInt32:
        // TODO(titzer): Is kTypeInt32 correct here?
        return VisitUnop(node, kTypeInt32 | kRepWord64,
                         kTypeInt32 | kRepWord32);

      case IrOpcode::kChangeFloat32ToFloat64:
        return VisitUnop(node, kTypeNumber | kRepFloat32,
                         kTypeNumber | kRepFloat64);
      case IrOpcode::kChangeInt32ToFloat64:
        return VisitUnop(node, kTypeInt32 | kRepWord32,
                         kTypeInt32 | kRepFloat64);
      case IrOpcode::kChangeUint32ToFloat64:
        return VisitUnop(node, kTypeUint32 | kRepWord32,
                         kTypeUint32 | kRepFloat64);
      case IrOpcode::kChangeFloat64ToInt32:
        return VisitUnop(node, kTypeInt32 | kRepFloat64,
                         kTypeInt32 | kRepWord32);
      case IrOpcode::kChangeFloat64ToUint32:
        return VisitUnop(node, kTypeUint32 | kRepFloat64,
                         kTypeUint32 | kRepWord32);

      case IrOpcode::kFloat64Add:
      case IrOpcode::kFloat64Sub:
      case IrOpcode::kFloat64Mul:
      case IrOpcode::kFloat64Div:
      case IrOpcode::kFloat64Mod:
      case IrOpcode::kFloat64Min:
        return VisitFloat64Binop(node);
      case IrOpcode::kFloat64Sqrt:
      case IrOpcode::kFloat64RoundDown:
      case IrOpcode::kFloat64RoundTruncate:
      case IrOpcode::kFloat64RoundTiesAway:
        return VisitUnop(node, kMachFloat64, kMachFloat64);
      case IrOpcode::kFloat64Equal:
      case IrOpcode::kFloat64LessThan:
      case IrOpcode::kFloat64LessThanOrEqual:
        return VisitFloat64Cmp(node);
      case IrOpcode::kFloat64ExtractLowWord32:
      case IrOpcode::kFloat64ExtractHighWord32:
        return VisitUnop(node, kMachFloat64, kMachInt32);
      case IrOpcode::kFloat64InsertLowWord32:
      case IrOpcode::kFloat64InsertHighWord32:
        return VisitBinop(node, kMachFloat64, kMachInt32, kMachFloat64);
      case IrOpcode::kLoadStackPointer:
        return VisitLeaf(node, kMachPtr);
      case IrOpcode::kStateValues:
        VisitStateValues(node);
        break;
      default:
        VisitInputs(node);
        break;
    }
  }

  void DeferReplacement(Node* node, Node* replacement) {
    TRACE("defer replacement #%d:%s with #%d:%s\n", node->id(),
          node->op()->mnemonic(), replacement->id(),
          replacement->op()->mnemonic());

    if (replacement->id() < count_ &&
        GetInfo(replacement)->output == GetInfo(node)->output) {
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

  void PrintUseInfo(Node* node) {
    TRACE("#%d:%-20s ", node->id(), node->op()->mnemonic());
    PrintInfo(GetUseInfo(node));
    TRACE("\n");
  }

  void PrintInfo(MachineTypeUnion info) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << static_cast<MachineType>(info);
    }
  }

 private:
  JSGraph* jsgraph_;
  int count_;                       // number of nodes in the graph
  NodeInfo* info_;                  // node id -> usage information
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
  Type* safe_int_additive_range_;

  NodeInfo* GetInfo(Node* node) {
    DCHECK(node->id() >= 0);
    DCHECK(node->id() < count_);
    return &info_[node->id()];
  }

  MachineTypeUnion GetUseInfo(Node* node) { return GetInfo(node)->use; }
};


Node* SimplifiedLowering::IsTagged(Node* node) {
  // TODO(titzer): factor this out to a TaggingScheme abstraction.
  STATIC_ASSERT(kSmiTagMask == 1);  // Only works if tag is the low bit.
  return graph()->NewNode(machine()->WordAnd(), node,
                          jsgraph()->Int32Constant(kSmiTagMask));
}


void SimplifiedLowering::LowerAllNodes() {
  SimplifiedOperatorBuilder simplified(graph()->zone());
  RepresentationChanger changer(jsgraph(), &simplified, jsgraph()->isolate());
  RepresentationSelector selector(jsgraph(), zone_, &changer,
                                  source_positions_);
  selector.Run(this);
}


Node* SimplifiedLowering::Untag(Node* node) {
  // TODO(titzer): factor this out to a TaggingScheme abstraction.
  Node* shift_amount = jsgraph()->Int32Constant(kSmiTagSize + kSmiShiftSize);
  return graph()->NewNode(machine()->WordSar(), node, shift_amount);
}


Node* SimplifiedLowering::SmiTag(Node* node) {
  // TODO(titzer): factor this out to a TaggingScheme abstraction.
  Node* shift_amount = jsgraph()->Int32Constant(kSmiTagSize + kSmiShiftSize);
  return graph()->NewNode(machine()->WordShl(), node, shift_amount);
}


Node* SimplifiedLowering::OffsetMinusTagConstant(int32_t offset) {
  return jsgraph()->Int32Constant(offset - kHeapObjectTag);
}


namespace {

WriteBarrierKind ComputeWriteBarrierKind(BaseTaggedness base_is_tagged,
                                         MachineType representation,
                                         Type* type) {
  if (type->Is(Type::TaggedSigned())) {
    // Write barriers are only for writes of heap objects.
    return kNoWriteBarrier;
  }
  if (base_is_tagged == kTaggedBase &&
      RepresentationOf(representation) == kRepTagged) {
    // Write barriers are only for writes into heap objects (i.e. tagged base).
    return kFullWriteBarrier;
  }
  return kNoWriteBarrier;
}

}  // namespace


void SimplifiedLowering::DoLoadField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  node->set_op(machine()->Load(access.machine_type));
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
}


void SimplifiedLowering::DoStoreField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  Type* type = NodeProperties::GetBounds(node->InputAt(1)).upper;
  WriteBarrierKind kind =
      ComputeWriteBarrierKind(access.base_is_tagged, access.machine_type, type);
  node->set_op(
      machine()->Store(StoreRepresentation(access.machine_type, kind)));
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
}


Node* SimplifiedLowering::ComputeIndex(const ElementAccess& access,
                                       Node* const key) {
  Node* index = key;
  const int element_size_shift = ElementSizeLog2Of(access.machine_type);
  if (element_size_shift) {
    index = graph()->NewNode(machine()->Word32Shl(), index,
                             jsgraph()->Int32Constant(element_size_shift));
  }
  const int fixed_offset = access.header_size - access.tag();
  if (fixed_offset) {
    index = graph()->NewNode(machine()->Int32Add(), index,
                             jsgraph()->Int32Constant(fixed_offset));
  }
  if (machine()->Is64()) {
    // TODO(turbofan): This is probably only correct for typed arrays, and only
    // if the typed arrays are at most 2GiB in size, which happens to match
    // exactly our current situation.
    index = graph()->NewNode(machine()->ChangeUint32ToUint64(), index);
  }
  return index;
}


void SimplifiedLowering::DoLoadBuffer(Node* node, MachineType output_type,
                                      RepresentationChanger* changer) {
  DCHECK_EQ(IrOpcode::kLoadBuffer, node->opcode());
  DCHECK_NE(kMachNone, RepresentationOf(output_type));
  MachineType const type = BufferAccessOf(node->op()).machine_type();
  if (output_type != type) {
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
    Node* etrue =
        graph()->NewNode(machine()->Load(type), buffer, index, effect, if_true);
    Node* vtrue = changer->GetRepresentationFor(etrue, type, output_type);

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse;
    if (output_type & kRepTagged) {
      vfalse = jsgraph()->UndefinedConstant();
    } else if (output_type & kRepFloat64) {
      vfalse =
          jsgraph()->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    } else if (output_type & kRepFloat32) {
      vfalse =
          jsgraph()->Float32Constant(std::numeric_limits<float>::quiet_NaN());
    } else {
      vfalse = jsgraph()->Int32Constant(0);
    }

    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);

    // Replace effect uses of {node} with the {ephi}.
    NodeProperties::ReplaceWithValue(node, node, ephi);

    // Turn the {node} into a Phi.
    node->set_op(common()->Phi(output_type, 2));
    node->ReplaceInput(0, vtrue);
    node->ReplaceInput(1, vfalse);
    node->ReplaceInput(2, merge);
    node->TrimInputCount(3);
  } else {
    node->set_op(machine()->CheckedLoad(type));
  }
}


void SimplifiedLowering::DoStoreBuffer(Node* node) {
  DCHECK_EQ(IrOpcode::kStoreBuffer, node->opcode());
  MachineType const type = BufferAccessOf(node->op()).machine_type();
  node->set_op(machine()->CheckedStore(type));
}


void SimplifiedLowering::DoLoadElement(Node* node) {
  const ElementAccess& access = ElementAccessOf(node->op());
  node->set_op(machine()->Load(access.machine_type));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
}


void SimplifiedLowering::DoStoreElement(Node* node) {
  const ElementAccess& access = ElementAccessOf(node->op());
  Type* type = NodeProperties::GetBounds(node->InputAt(2)).upper;
  node->set_op(machine()->Store(
      StoreRepresentation(access.machine_type,
                          ComputeWriteBarrierKind(access.base_is_tagged,
                                                  access.machine_type, type))));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
}


void SimplifiedLowering::DoStringAdd(Node* node) {
  Operator::Properties properties = node->op()->properties();
  Callable callable = CodeFactory::StringAdd(
      jsgraph()->isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      jsgraph()->isolate(), zone(), callable.descriptor(), 0, flags,
      properties);
  node->set_op(common()->Call(desc));
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(callable.code()));
  node->AppendInput(graph()->zone(), jsgraph()->UndefinedConstant());
  node->AppendInput(graph()->zone(), graph()->start());
  node->AppendInput(graph()->zone(), graph()->start());
}


Node* SimplifiedLowering::StringComparison(Node* node, bool requires_ordering) {
  CEntryStub stub(jsgraph()->isolate(), 1);
  Runtime::FunctionId f =
      requires_ordering ? Runtime::kStringCompareRT : Runtime::kStringEquals;
  ExternalReference ref(f, jsgraph()->isolate());
  Operator::Properties props = node->op()->properties();
  // TODO(mstarzinger): We should call StringCompareStub here instead, once an
  // interface descriptor is available for it.
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(zone(), f, 2, props);
  return graph()->NewNode(common()->Call(desc),
                          jsgraph()->HeapConstant(stub.GetCode()),
                          NodeProperties::GetValueInput(node, 0),
                          NodeProperties::GetValueInput(node, 1),
                          jsgraph()->ExternalConstant(ref),
                          jsgraph()->Int32Constant(2),
                          jsgraph()->UndefinedConstant());
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
  const Operator* const phi_op = common()->Phi(kMachInt32, 2);

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
  const Operator* const phi_op = common()->Phi(kMachInt32, 2);

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
  return d.Phi(kMachUint32, zero, div);
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
  const Operator* const phi_op = common()->Phi(kMachInt32, 2);

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


void SimplifiedLowering::DoStringEqual(Node* node) {
  node->set_op(machine()->WordEqual());
  node->ReplaceInput(0, StringComparison(node, false));
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
}


void SimplifiedLowering::DoStringLessThan(Node* node) {
  node->set_op(machine()->IntLessThan());
  node->ReplaceInput(0, StringComparison(node, true));
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
}


void SimplifiedLowering::DoStringLessThanOrEqual(Node* node) {
  node->set_op(machine()->IntLessThanOrEqual());
  node->ReplaceInput(0, StringComparison(node, true));
  node->ReplaceInput(1, jsgraph()->SmiConstant(EQUAL));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
