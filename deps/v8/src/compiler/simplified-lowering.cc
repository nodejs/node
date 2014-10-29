// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace compiler {

// Macro for outputting trace information from representation inference.
#define TRACE(x) \
  if (FLAG_trace_representation) PrintF x

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
                         RepresentationChanger* changer)
      : jsgraph_(jsgraph),
        count_(jsgraph->graph()->NodeCount()),
        info_(zone->NewArray<NodeInfo>(count_)),
        nodes_(zone),
        replacements_(zone),
        contains_js_nodes_(false),
        phase_(PROPAGATE),
        changer_(changer),
        queue_(zone) {
    memset(info_, 0, sizeof(NodeInfo) * count_);
  }

  void Run(SimplifiedLowering* lowering) {
    // Run propagation phase to a fixpoint.
    TRACE(("--{Propagation phase}--\n"));
    phase_ = PROPAGATE;
    Enqueue(jsgraph_->graph()->end());
    // Process nodes from the queue until it is empty.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      NodeInfo* info = GetInfo(node);
      queue_.pop();
      info->queued = false;
      TRACE((" visit #%d: %s\n", node->id(), node->op()->mnemonic()));
      VisitNode(node, info->use, NULL);
      TRACE(("  ==> output "));
      PrintInfo(info->output);
      TRACE(("\n"));
    }

    // Run lowering and change insertion phase.
    TRACE(("--{Simplified lowering phase}--\n"));
    phase_ = LOWER;
    // Process nodes from the collected {nodes_} vector.
    for (NodeVector::iterator i = nodes_.begin(); i != nodes_.end(); ++i) {
      Node* node = *i;
      TRACE((" visit #%d: %s\n", node->id(), node->op()->mnemonic()));
      // Reuse {VisitNode()} so the representation rules are in one place.
      VisitNode(node, GetUseInfo(node), lowering);
    }

    // Perform the final replacements.
    for (NodeVector::iterator i = replacements_.begin();
         i != replacements_.end(); ++i) {
      Node* node = *i;
      Node* replacement = *(++i);
      node->ReplaceUses(replacement);
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
      TRACE(("  initial: "));
      info->use |= use;
      PrintUseInfo(node);
      return;
    }
    TRACE(("   queue?: "));
    PrintUseInfo(node);
    if ((info->use & use) != use) {
      // New usage information for the node is available.
      if (!info->queued) {
        queue_.push(node);
        info->queued = true;
        TRACE(("   added: "));
      } else {
        TRACE((" inqueue: "));
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
        TRACE(("  change: #%d:%s(@%d #%d:%s) ", node->id(),
               node->op()->mnemonic(), index, input->id(),
               input->op()->mnemonic()));
        TRACE((" from "));
        PrintInfo(output);
        TRACE((" to "));
        PrintInfo(use);
        TRACE(("\n"));
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
  // context, effect, and control inputs, assuming that value inputs should have
  // {kRepTagged} representation and can observe all output values {kTypeAny}.
  void VisitInputs(Node* node) {
    InputIter i = node->inputs().begin();
    for (int j = OperatorProperties::GetValueInputCount(node->op()); j > 0;
         ++i, j--) {
      ProcessInput(node, i.index(), kMachAnyTagged);  // Value inputs
    }
    for (int j = OperatorProperties::GetContextInputCount(node->op()); j > 0;
         ++i, j--) {
      ProcessInput(node, i.index(), kMachAnyTagged);  // Context inputs
    }
    for (int j = OperatorProperties::GetEffectInputCount(node->op()); j > 0;
         ++i, j--) {
      Enqueue(*i);  // Effect inputs: just visit
    }
    for (int j = OperatorProperties::GetControlInputCount(node->op()); j > 0;
         ++i, j--) {
      Enqueue(*i);  // Control inputs: just visit
    }
    SetOutput(node, kMachAnyTagged);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, MachineTypeUnion input_use,
                  MachineTypeUnion output) {
    DCHECK_EQ(2, node->InputCount());
    ProcessInput(node, 0, input_use);
    ProcessInput(node, 1, input_use);
    SetOutput(node, output);
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

  // Helper for handling phis.
  void VisitPhi(Node* node, MachineTypeUnion use,
                SimplifiedLowering* lowering) {
    // First, propagate the usage information to inputs of the phi.
    if (!lower()) {
      int values = OperatorProperties::GetValueInputCount(node->op());
      // Propagate {use} of the phi to value inputs, and 0 to control.
      Node::Inputs inputs = node->inputs();
      for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
           ++iter, --values) {
        // TODO(titzer): it'd be nice to have distinguished edge kinds here.
        ProcessInput(node, iter.index(), values > 0 ? use : 0);
      }
    }
    // Phis adapt to whatever output representation their uses demand,
    // pushing representation changes to their inputs.
    MachineTypeUnion use_rep = GetUseInfo(node) & kRepMask;
    MachineTypeUnion use_type = GetUseInfo(node) & kTypeMask;
    MachineTypeUnion rep = 0;
    if (use_rep & kRepTagged) {
      rep = kRepTagged;  // Tagged overrides everything.
    } else if (use_rep & kRepFloat32) {
      rep = kRepFloat32;
    } else if (use_rep & kRepFloat64) {
      rep = kRepFloat64;
    } else if (use_rep & kRepWord64) {
      rep = kRepWord64;
    } else if (use_rep & kRepWord32) {
      rep = kRepWord32;
    } else if (use_rep & kRepBit) {
      rep = kRepBit;
    } else {
      // There was no representation associated with any of the uses.
      // TODO(titzer): Select the best rep using phi's type, not the usage type?
      if (use_type & kTypeAny) {
        rep = kRepTagged;
      } else if (use_type & kTypeNumber) {
        rep = kRepFloat64;
      } else if (use_type & kTypeInt64 || use_type & kTypeUint64) {
        rep = kRepWord64;
      } else if (use_type & kTypeInt32 || use_type & kTypeUint32) {
        rep = kRepWord32;
      } else if (use_type & kTypeBool) {
        rep = kRepBit;
      } else {
        UNREACHABLE();  // should have at least a usage type!
      }
    }
    // Preserve the usage type, but set the representation.
    Type* upper = NodeProperties::GetBounds(node).upper;
    MachineTypeUnion output_type = rep | changer_->TypeFromUpperBound(upper);
    SetOutput(node, output_type);

    if (lower()) {
      int values = OperatorProperties::GetValueInputCount(node->op());

      // Update the phi operator.
      MachineType type = static_cast<MachineType>(output_type);
      if (type != OpParameter<MachineType>(node)) {
        node->set_op(lowering->common()->Phi(type, values));
      }

      // Convert inputs to the output representation of this phi.
      Node::Inputs inputs = node->inputs();
      for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
           ++iter, --values) {
        // TODO(titzer): it'd be nice to have distinguished edge kinds here.
        ProcessInput(node, iter.index(), values > 0 ? output_type : 0);
      }
    }
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

      case IrOpcode::kEnd:
      case IrOpcode::kIfTrue:
      case IrOpcode::kIfFalse:
      case IrOpcode::kReturn:
      case IrOpcode::kMerge:
      case IrOpcode::kThrow:
        return VisitInputs(node);  // default visit for all node inputs.

      case IrOpcode::kBranch:
        ProcessInput(node, 0, kRepBit);
        Enqueue(NodeProperties::GetControlInput(node, 0));
        break;
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
        contains_js_nodes_ = true;
        VisitInputs(node);
        return SetOutput(node, kRepTagged);

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower()) {
          MachineTypeUnion input = GetInfo(node->InputAt(0))->output;
          if (input & kRepBit) {
            // BooleanNot(x: kRepBit) => WordEqual(x, #0)
            node->set_op(lowering->machine()->WordEqual());
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
        if (BothInputsAre(node, Type::Signed32()) &&
            (use & (kTypeUint32 | kTypeNumber | kTypeAny)) == 0) {
          // => signed Int32Add/Sub
          VisitInt32Binop(node);
          if (lower()) node->set_op(Int32Op(node));
        } else if (BothInputsAre(node, Type::Unsigned32()) &&
                   (use & (kTypeInt32 | kTypeNumber | kTypeAny)) == 0) {
          // => unsigned Int32Add/Sub
          VisitUint32Binop(node);
          if (lower()) node->set_op(Uint32Op(node));
        } else {
          // => Float64Add/Sub
          VisitFloat64Binop(node);
          if (lower()) node->set_op(Float64Op(node));
        }
        break;
      }
      case IrOpcode::kNumberMultiply:
      case IrOpcode::kNumberDivide:
      case IrOpcode::kNumberModulus: {
        // Float64Mul/Div/Mod
        VisitFloat64Binop(node);
        if (lower()) node->set_op(Float64Op(node));
        break;
      }
      case IrOpcode::kNumberToInt32: {
        MachineTypeUnion use_rep = use & kRepMask;
        if (lower()) {
          MachineTypeUnion in = GetInfo(node->InputAt(0))->output;
          if ((in & kTypeMask) == kTypeInt32 || (in & kRepMask) == kRepWord32) {
            // If the input has type int32, or is already a word32, just change
            // representation if necessary.
            VisitUnop(node, kTypeInt32 | use_rep, kTypeInt32 | use_rep);
            DeferReplacement(node, node->InputAt(0));
          } else {
            // Require the input in float64 format and perform truncation.
            // TODO(turbofan): avoid a truncation with a smi check.
            VisitUnop(node, kTypeInt32 | kRepFloat64, kTypeInt32 | kRepWord32);
            node->set_op(lowering->machine()->TruncateFloat64ToInt32());
          }
        } else {
          // Propagate a type to the input, but pass through representation.
          VisitUnop(node, kTypeInt32, kTypeInt32 | use_rep);
        }
        break;
      }
      case IrOpcode::kNumberToUint32: {
        MachineTypeUnion use_rep = use & kRepMask;
        if (lower()) {
          MachineTypeUnion in = GetInfo(node->InputAt(0))->output;
          if ((in & kTypeMask) == kTypeUint32 ||
              (in & kRepMask) == kRepWord32) {
            // The input has type int32, just change representation.
            VisitUnop(node, kTypeUint32 | use_rep, kTypeUint32 | use_rep);
            DeferReplacement(node, node->InputAt(0));
          } else {
            // Require the input in float64 format to perform truncation.
            // TODO(turbofan): avoid the truncation with a smi check.
            VisitUnop(node, kTypeUint32 | kRepFloat64,
                      kTypeUint32 | kRepWord32);
            node->set_op(lowering->machine()->TruncateFloat64ToInt32());
          }
        } else {
          // Propagate a type to the input, but pass through representation.
          VisitUnop(node, kTypeUint32, kTypeUint32 | use_rep);
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
      case IrOpcode::kLoadElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, kMachInt32);  // element index
        ProcessInput(node, 2, kMachInt32);  // length
        ProcessRemainingInputs(node, 3);
        SetOutput(node, access.machine_type);
        if (lower()) lowering->DoLoadElement(node);
        break;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, kMachInt32);  // element index
        ProcessInput(node, 2, kMachInt32);  // length
        ProcessInput(node, 3, access.machine_type);
        ProcessRemainingInputs(node, 4);
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreElement(node);
        break;
      }

      //------------------------------------------------------------------
      // Machine-level operators.
      //------------------------------------------------------------------
      case IrOpcode::kLoad: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        MachineType tBase = kRepTagged;
        LoadRepresentation rep = OpParameter<LoadRepresentation>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kMachInt32);  // index
        ProcessRemainingInputs(node, 2);
        SetOutput(node, rep);
        break;
      }
      case IrOpcode::kStore: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        MachineType tBase = kRepTagged;
        StoreRepresentation rep = OpParameter<StoreRepresentation>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kMachInt32);  // index
        ProcessInput(node, 2, rep.machine_type());
        ProcessRemainingInputs(node, 3);
        SetOutput(node, 0);
        break;
      }
      case IrOpcode::kWord32Shr:
        // We output unsigned int32 for shift right because JavaScript.
        return VisitBinop(node, kRepWord32, kRepWord32 | kTypeUint32);
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

      case IrOpcode::kInt32Add:
      case IrOpcode::kInt32Sub:
      case IrOpcode::kInt32Mul:
      case IrOpcode::kInt32Div:
      case IrOpcode::kInt32Mod:
        return VisitInt32Binop(node);
      case IrOpcode::kInt32UDiv:
      case IrOpcode::kInt32UMod:
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

      case IrOpcode::kInt64UDiv:
      case IrOpcode::kInt64UMod:
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
        return VisitFloat64Binop(node);
      case IrOpcode::kFloat64Sqrt:
        return VisitUnop(node, kMachFloat64, kMachFloat64);
      case IrOpcode::kFloat64Equal:
      case IrOpcode::kFloat64LessThan:
      case IrOpcode::kFloat64LessThanOrEqual:
        return VisitFloat64Cmp(node);
      default:
        VisitInputs(node);
        break;
    }
  }

  void DeferReplacement(Node* node, Node* replacement) {
    if (replacement->id() < count_) {
      // Replace with a previously existing node eagerly.
      node->ReplaceUses(replacement);
    } else {
      // Otherwise, we are replacing a node with a representation change.
      // Such a substitution must be done after all lowering is done, because
      // new nodes do not have {NodeInfo} entries, and that would confuse
      // the representation change insertion for uses of it.
      replacements_.push_back(node);
      replacements_.push_back(replacement);
    }
    // TODO(titzer) node->RemoveAllInputs();  // Node is now dead.
  }

  void PrintUseInfo(Node* node) {
    TRACE(("#%d:%-20s ", node->id(), node->op()->mnemonic()));
    PrintInfo(GetUseInfo(node));
    TRACE(("\n"));
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
  bool contains_js_nodes_;          // {true} if a JS operator was seen
  Phase phase_;                     // current phase of algorithm
  RepresentationChanger* changer_;  // for inserting representation changes
  ZoneQueue<Node*> queue_;          // queue for traversing the graph

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
  RepresentationChanger changer(jsgraph(), &simplified,
                                graph()->zone()->isolate());
  RepresentationSelector selector(jsgraph(), zone(), &changer);
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


static WriteBarrierKind ComputeWriteBarrierKind(BaseTaggedness base_is_tagged,
                                                MachineType representation,
                                                Type* type) {
  // TODO(turbofan): skip write barriers for Smis, etc.
  if (base_is_tagged == kTaggedBase &&
      RepresentationOf(representation) == kRepTagged) {
    // Write barriers are only for writes into heap objects (i.e. tagged base).
    return kFullWriteBarrier;
  }
  return kNoWriteBarrier;
}


void SimplifiedLowering::DoLoadField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  node->set_op(machine()->Load(access.machine_type));
  Node* offset = jsgraph()->Int32Constant(access.offset - access.tag());
  node->InsertInput(zone(), 1, offset);
}


void SimplifiedLowering::DoStoreField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  WriteBarrierKind kind = ComputeWriteBarrierKind(
      access.base_is_tagged, access.machine_type, access.type);
  node->set_op(
      machine()->Store(StoreRepresentation(access.machine_type, kind)));
  Node* offset = jsgraph()->Int32Constant(access.offset - access.tag());
  node->InsertInput(zone(), 1, offset);
}


Node* SimplifiedLowering::ComputeIndex(const ElementAccess& access,
                                       Node* index) {
  int element_size = ElementSizeOf(access.machine_type);
  if (element_size != 1) {
    index = graph()->NewNode(machine()->Int32Mul(),
                             jsgraph()->Int32Constant(element_size), index);
  }
  int fixed_offset = access.header_size - access.tag();
  if (fixed_offset == 0) return index;
  return graph()->NewNode(machine()->Int32Add(), index,
                          jsgraph()->Int32Constant(fixed_offset));
}


void SimplifiedLowering::DoLoadElement(Node* node) {
  const ElementAccess& access = ElementAccessOf(node->op());
  node->set_op(machine()->Load(access.machine_type));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
  node->RemoveInput(2);
}


void SimplifiedLowering::DoStoreElement(Node* node) {
  const ElementAccess& access = ElementAccessOf(node->op());
  WriteBarrierKind kind = ComputeWriteBarrierKind(
      access.base_is_tagged, access.machine_type, access.type);
  node->set_op(
      machine()->Store(StoreRepresentation(access.machine_type, kind)));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
  node->RemoveInput(2);
}


void SimplifiedLowering::DoStringAdd(Node* node) {
  Callable callable = CodeFactory::StringAdd(
      zone()->isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc =
      Linkage::GetStubCallDescriptor(callable.descriptor(), 0, flags, zone());
  node->set_op(common()->Call(desc));
  node->InsertInput(zone(), 0, jsgraph()->HeapConstant(callable.code()));
  node->AppendInput(zone(), jsgraph()->UndefinedConstant());
  node->AppendInput(zone(), graph()->start());
  node->AppendInput(zone(), graph()->start());
}


Node* SimplifiedLowering::StringComparison(Node* node, bool requires_ordering) {
  CEntryStub stub(zone()->isolate(), 1);
  Runtime::FunctionId f =
      requires_ordering ? Runtime::kStringCompare : Runtime::kStringEquals;
  ExternalReference ref(f, zone()->isolate());
  Operator::Properties props = node->op()->properties();
  // TODO(mstarzinger): We should call StringCompareStub here instead, once an
  // interface descriptor is available for it.
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(f, 2, props, zone());
  return graph()->NewNode(common()->Call(desc),
                          jsgraph()->HeapConstant(stub.GetCode()),
                          NodeProperties::GetValueInput(node, 0),
                          NodeProperties::GetValueInput(node, 1),
                          jsgraph()->ExternalConstant(ref),
                          jsgraph()->Int32Constant(2),
                          jsgraph()->UndefinedConstant());
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
