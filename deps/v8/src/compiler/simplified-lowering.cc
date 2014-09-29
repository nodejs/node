// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include <deque>
#include <queue>

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
    RepTypeUnion use : 14;     // Union of all usages for the node.
    bool queued : 1;           // Bookkeeping for the traversal.
    bool visited : 1;          // Bookkeeping for the traversal.
    RepTypeUnion output : 14;  // Output type of the node.
  };

  RepresentationSelector(JSGraph* jsgraph, Zone* zone,
                         RepresentationChanger* changer)
      : jsgraph_(jsgraph),
        count_(jsgraph->graph()->NodeCount()),
        info_(zone->NewArray<NodeInfo>(count_)),
        nodes_(NodeVector::allocator_type(zone)),
        replacements_(NodeVector::allocator_type(zone)),
        contains_js_nodes_(false),
        phase_(PROPAGATE),
        changer_(changer),
        queue_(std::deque<Node*, NodePtrZoneAllocator>(
            NodePtrZoneAllocator(zone))) {
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
  void Enqueue(Node* node, RepTypeUnion use = 0) {
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

  void Enqueue(Node* node, RepType use) {
    Enqueue(node, static_cast<RepTypeUnion>(use));
  }

  void SetOutput(Node* node, RepTypeUnion output) {
    // Every node should have at most one output representation. Note that
    // phis can have 0, if they have not been used in a representation-inducing
    // instruction.
    DCHECK((output & rMask) == 0 || IsPowerOf2(output & rMask));
    GetInfo(node)->output = output;
  }

  bool BothInputsAre(Node* node, Type* type) {
    DCHECK_EQ(2, node->InputCount());
    return NodeProperties::GetBounds(node->InputAt(0)).upper->Is(type) &&
           NodeProperties::GetBounds(node->InputAt(1)).upper->Is(type);
  }

  void ProcessInput(Node* node, int index, RepTypeUnion use) {
    Node* input = node->InputAt(index);
    if (phase_ == PROPAGATE) {
      // In the propagate phase, propagate the usage information backward.
      Enqueue(input, use);
    } else {
      // In the change phase, insert a change before the use if necessary.
      if ((use & rMask) == 0) return;  // No input requirement on the use.
      RepTypeUnion output = GetInfo(input)->output;
      if ((output & rMask & use) == 0) {
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

  static const RepTypeUnion kFloat64 = rFloat64 | tNumber;
  static const RepTypeUnion kInt32 = rWord32 | tInt32;
  static const RepTypeUnion kUint32 = rWord32 | tUint32;
  static const RepTypeUnion kInt64 = rWord64 | tInt64;
  static const RepTypeUnion kUint64 = rWord64 | tUint64;
  static const RepTypeUnion kAnyTagged = rTagged | tAny;

  // The default, most general visitation case. For {node}, process all value,
  // context, effect, and control inputs, assuming that value inputs should have
  // {rTagged} representation and can observe all output values {tAny}.
  void VisitInputs(Node* node) {
    InputIter i = node->inputs().begin();
    for (int j = OperatorProperties::GetValueInputCount(node->op()); j > 0;
         ++i, j--) {
      ProcessInput(node, i.index(), kAnyTagged);  // Value inputs
    }
    for (int j = OperatorProperties::GetContextInputCount(node->op()); j > 0;
         ++i, j--) {
      ProcessInput(node, i.index(), kAnyTagged);  // Context inputs
    }
    for (int j = OperatorProperties::GetEffectInputCount(node->op()); j > 0;
         ++i, j--) {
      Enqueue(*i);  // Effect inputs: just visit
    }
    for (int j = OperatorProperties::GetControlInputCount(node->op()); j > 0;
         ++i, j--) {
      Enqueue(*i);  // Control inputs: just visit
    }
    SetOutput(node, kAnyTagged);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, RepTypeUnion input_use, RepTypeUnion output) {
    DCHECK_EQ(2, node->InputCount());
    ProcessInput(node, 0, input_use);
    ProcessInput(node, 1, input_use);
    SetOutput(node, output);
  }

  // Helper for unops of the I -> O variety.
  void VisitUnop(Node* node, RepTypeUnion input_use, RepTypeUnion output) {
    DCHECK_EQ(1, node->InputCount());
    ProcessInput(node, 0, input_use);
    SetOutput(node, output);
  }

  // Helper for leaf nodes.
  void VisitLeaf(Node* node, RepTypeUnion output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput(node, output);
  }

  // Helpers for specific types of binops.
  void VisitFloat64Binop(Node* node) { VisitBinop(node, kFloat64, kFloat64); }
  void VisitInt32Binop(Node* node) { VisitBinop(node, kInt32, kInt32); }
  void VisitUint32Binop(Node* node) { VisitBinop(node, kUint32, kUint32); }
  void VisitInt64Binop(Node* node) { VisitBinop(node, kInt64, kInt64); }
  void VisitUint64Binop(Node* node) { VisitBinop(node, kUint64, kUint64); }
  void VisitFloat64Cmp(Node* node) { VisitBinop(node, kFloat64, rBit); }
  void VisitInt32Cmp(Node* node) { VisitBinop(node, kInt32, rBit); }
  void VisitUint32Cmp(Node* node) { VisitBinop(node, kUint32, rBit); }
  void VisitInt64Cmp(Node* node) { VisitBinop(node, kInt64, rBit); }
  void VisitUint64Cmp(Node* node) { VisitBinop(node, kUint64, rBit); }

  // Helper for handling phis.
  void VisitPhi(Node* node, RepTypeUnion use) {
    // First, propagate the usage information to inputs of the phi.
    int values = OperatorProperties::GetValueInputCount(node->op());
    Node::Inputs inputs = node->inputs();
    for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
         ++iter, --values) {
      // Propagate {use} of the phi to value inputs, and 0 to control.
      // TODO(titzer): it'd be nice to have distinguished edge kinds here.
      ProcessInput(node, iter.index(), values > 0 ? use : 0);
    }
    // Phis adapt to whatever output representation their uses demand,
    // pushing representation changes to their inputs.
    RepTypeUnion use_rep = GetUseInfo(node) & rMask;
    RepTypeUnion use_type = GetUseInfo(node) & tMask;
    RepTypeUnion rep = 0;
    if (use_rep & rTagged) {
      rep = rTagged;  // Tagged overrides everything.
    } else if (use_rep & rFloat64) {
      rep = rFloat64;
    } else if (use_rep & rWord64) {
      rep = rWord64;
    } else if (use_rep & rWord32) {
      rep = rWord32;
    } else if (use_rep & rBit) {
      rep = rBit;
    } else {
      // There was no representation associated with any of the uses.
      // TODO(titzer): Select the best rep using phi's type, not the usage type?
      if (use_type & tAny) {
        rep = rTagged;
      } else if (use_type & tNumber) {
        rep = rFloat64;
      } else if (use_type & tInt64 || use_type & tUint64) {
        rep = rWord64;
      } else if (use_type & tInt32 || use_type & tUint32) {
        rep = rWord32;
      } else if (use_type & tBool) {
        rep = rBit;
      } else {
        UNREACHABLE();  // should have at least a usage type!
      }
    }
    // Preserve the usage type, but set the representation.
    Type* upper = NodeProperties::GetBounds(node).upper;
    SetOutput(node, rep | changer_->TypeFromUpperBound(upper));
  }

  Operator* Int32Op(Node* node) {
    return changer_->Int32OperatorFor(node->opcode());
  }

  Operator* Uint32Op(Node* node) {
    return changer_->Uint32OperatorFor(node->opcode());
  }

  Operator* Float64Op(Node* node) {
    return changer_->Float64OperatorFor(node->opcode());
  }

  // Dispatching routine for visiting the node {node} with the usage {use}.
  // Depending on the operator, propagate new usage info to the inputs.
  void VisitNode(Node* node, RepTypeUnion use, SimplifiedLowering* lowering) {
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
        SetOutput(node, rTagged | changer_->TypeFromUpperBound(upper));
        return;
      }
      case IrOpcode::kInt32Constant:
        return VisitLeaf(node, rWord32);
      case IrOpcode::kInt64Constant:
        return VisitLeaf(node, rWord64);
      case IrOpcode::kFloat64Constant:
        return VisitLeaf(node, rFloat64);
      case IrOpcode::kExternalConstant:
        return VisitLeaf(node, rPtr);
      case IrOpcode::kNumberConstant:
        return VisitLeaf(node, rTagged);
      case IrOpcode::kHeapConstant:
        return VisitLeaf(node, rTagged);

      case IrOpcode::kEnd:
      case IrOpcode::kIfTrue:
      case IrOpcode::kIfFalse:
      case IrOpcode::kReturn:
      case IrOpcode::kMerge:
      case IrOpcode::kThrow:
        return VisitInputs(node);  // default visit for all node inputs.

      case IrOpcode::kBranch:
        ProcessInput(node, 0, rBit);
        Enqueue(NodeProperties::GetControlInput(node, 0));
        break;
      case IrOpcode::kPhi:
        return VisitPhi(node, use);

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
        return SetOutput(node, rTagged);

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower()) {
          RepTypeUnion input = GetInfo(node->InputAt(0))->output;
          if (input & rBit) {
            // BooleanNot(x: rBit) => WordEqual(x, #0)
            node->set_op(lowering->machine()->WordEqual());
            node->AppendInput(jsgraph_->zone(), jsgraph_->Int32Constant(0));
          } else {
            // BooleanNot(x: rTagged) => WordEqual(x, #false)
            node->set_op(lowering->machine()->WordEqual());
            node->AppendInput(jsgraph_->zone(), jsgraph_->FalseConstant());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, tBool);
          SetOutput(node, rBit);
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
            (use & (tUint32 | tNumber | tAny)) == 0) {
          // => signed Int32Add/Sub
          VisitInt32Binop(node);
          if (lower()) node->set_op(Int32Op(node));
        } else if (BothInputsAre(node, Type::Unsigned32()) &&
                   (use & (tInt32 | tNumber | tAny)) == 0) {
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
        RepTypeUnion use_rep = use & rMask;
        if (lower()) {
          RepTypeUnion in = GetInfo(node->InputAt(0))->output;
          if ((in & tMask) == tInt32 || (in & rMask) == rWord32) {
            // If the input has type int32, or is already a word32, just change
            // representation if necessary.
            VisitUnop(node, tInt32 | use_rep, tInt32 | use_rep);
            DeferReplacement(node, node->InputAt(0));
          } else {
            // Require the input in float64 format and perform truncation.
            // TODO(turbofan): could also avoid the truncation with a tag check.
            VisitUnop(node, tInt32 | rFloat64, tInt32 | rWord32);
            // TODO(titzer): should be a truncation.
            node->set_op(lowering->machine()->ChangeFloat64ToInt32());
          }
        } else {
          // Propagate a type to the input, but pass through representation.
          VisitUnop(node, tInt32, tInt32 | use_rep);
        }
        break;
      }
      case IrOpcode::kNumberToUint32: {
        RepTypeUnion use_rep = use & rMask;
        if (lower()) {
          RepTypeUnion in = GetInfo(node->InputAt(0))->output;
          if ((in & tMask) == tUint32 || (in & rMask) == rWord32) {
            // The input has type int32, just change representation.
            VisitUnop(node, tUint32 | use_rep, tUint32 | use_rep);
            DeferReplacement(node, node->InputAt(0));
          } else {
            // Require the input in float64 format to perform truncation.
            // TODO(turbofan): could also avoid the truncation with a tag check.
            VisitUnop(node, tUint32 | rFloat64, tUint32 | rWord32);
            // TODO(titzer): should be a truncation.
            node->set_op(lowering->machine()->ChangeFloat64ToUint32());
          }
        } else {
          // Propagate a type to the input, but pass through representation.
          VisitUnop(node, tUint32, tUint32 | use_rep);
        }
        break;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop(node, kAnyTagged, rBit);
        if (lower()) node->set_op(lowering->machine()->WordEqual());
        break;
      }
      case IrOpcode::kStringEqual: {
        VisitBinop(node, kAnyTagged, rBit);
        // TODO(titzer): lower StringEqual to stub/runtime call.
        break;
      }
      case IrOpcode::kStringLessThan: {
        VisitBinop(node, kAnyTagged, rBit);
        // TODO(titzer): lower StringLessThan to stub/runtime call.
        break;
      }
      case IrOpcode::kStringLessThanOrEqual: {
        VisitBinop(node, kAnyTagged, rBit);
        // TODO(titzer): lower StringLessThanOrEqual to stub/runtime call.
        break;
      }
      case IrOpcode::kStringAdd: {
        VisitBinop(node, kAnyTagged, kAnyTagged);
        // TODO(titzer): lower StringAdd to stub/runtime call.
        break;
      }
      case IrOpcode::kLoadField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        SetOutput(node, changer_->TypeForField(access));
        if (lower()) lowering->DoLoadField(node);
        break;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, changer_->TypeForField(access));
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreField(node);
        break;
      }
      case IrOpcode::kLoadElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, kInt32);  // element index
        SetOutput(node, changer_->TypeForElement(access));
        if (lower()) lowering->DoLoadElement(node);
        break;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        ProcessInput(node, 0, changer_->TypeForBasePointer(access));
        ProcessInput(node, 1, kInt32);  // element index
        ProcessInput(node, 2, changer_->TypeForElement(access));
        SetOutput(node, 0);
        if (lower()) lowering->DoStoreElement(node);
        break;
      }

      //------------------------------------------------------------------
      // Machine-level operators.
      //------------------------------------------------------------------
      case IrOpcode::kLoad: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        RepType tBase = rTagged;
        MachineType rep = OpParameter<MachineType>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kInt32);  // index
        SetOutput(node, changer_->TypeForMachineType(rep));
        break;
      }
      case IrOpcode::kStore: {
        // TODO(titzer): machine loads/stores need to know BaseTaggedness!?
        RepType tBase = rTagged;
        StoreRepresentation rep = OpParameter<StoreRepresentation>(node);
        ProcessInput(node, 0, tBase);   // pointer or object
        ProcessInput(node, 1, kInt32);  // index
        ProcessInput(node, 2, changer_->TypeForMachineType(rep.rep));
        SetOutput(node, 0);
        break;
      }
      case IrOpcode::kWord32Shr:
        // We output unsigned int32 for shift right because JavaScript.
        return VisitBinop(node, rWord32, rWord32 | tUint32);
      case IrOpcode::kWord32And:
      case IrOpcode::kWord32Or:
      case IrOpcode::kWord32Xor:
      case IrOpcode::kWord32Shl:
      case IrOpcode::kWord32Sar:
        // We use signed int32 as the output type for these word32 operations,
        // though the machine bits are the same for either signed or unsigned,
        // because JavaScript considers the result from these operations signed.
        return VisitBinop(node, rWord32, rWord32 | tInt32);
      case IrOpcode::kWord32Equal:
        return VisitBinop(node, rWord32, rBit);

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
        return VisitBinop(node, rWord64, rWord64);
      case IrOpcode::kWord64Equal:
        return VisitBinop(node, rWord64, rBit);

      case IrOpcode::kConvertInt32ToInt64:
        return VisitUnop(node, tInt32 | rWord32, tInt32 | rWord64);
      case IrOpcode::kConvertInt64ToInt32:
        return VisitUnop(node, tInt64 | rWord64, tInt32 | rWord32);

      case IrOpcode::kChangeInt32ToFloat64:
        return VisitUnop(node, tInt32 | rWord32, tInt32 | rFloat64);
      case IrOpcode::kChangeUint32ToFloat64:
        return VisitUnop(node, tUint32 | rWord32, tUint32 | rFloat64);
      case IrOpcode::kChangeFloat64ToInt32:
        return VisitUnop(node, tInt32 | rFloat64, tInt32 | rWord32);
      case IrOpcode::kChangeFloat64ToUint32:
        return VisitUnop(node, tUint32 | rFloat64, tUint32 | rWord32);

      case IrOpcode::kFloat64Add:
      case IrOpcode::kFloat64Sub:
      case IrOpcode::kFloat64Mul:
      case IrOpcode::kFloat64Div:
      case IrOpcode::kFloat64Mod:
        return VisitFloat64Binop(node);
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

  void PrintInfo(RepTypeUnion info) {
    if (FLAG_trace_representation) {
      char buf[REP_TYPE_STRLEN];
      RenderRepTypeUnion(buf, info);
      TRACE(("%s", buf));
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

  std::queue<Node*, std::deque<Node*, NodePtrZoneAllocator> > queue_;

  NodeInfo* GetInfo(Node* node) {
    DCHECK(node->id() >= 0);
    DCHECK(node->id() < count_);
    return &info_[node->id()];
  }

  RepTypeUnion GetUseInfo(Node* node) { return GetInfo(node)->use; }
};


Node* SimplifiedLowering::IsTagged(Node* node) {
  // TODO(titzer): factor this out to a TaggingScheme abstraction.
  STATIC_ASSERT(kSmiTagMask == 1);  // Only works if tag is the low bit.
  return graph()->NewNode(machine()->WordAnd(), node,
                          jsgraph()->Int32Constant(kSmiTagMask));
}


void SimplifiedLowering::LowerAllNodes() {
  SimplifiedOperatorBuilder simplified(graph()->zone());
  RepresentationChanger changer(jsgraph(), &simplified, machine(),
                                graph()->zone()->isolate());
  RepresentationSelector selector(jsgraph(), zone(), &changer);
  selector.Run(this);

  LoweringBuilder::LowerAllNodes();
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


static void UpdateControlSuccessors(Node* before, Node* node) {
  DCHECK(IrOpcode::IsControlOpcode(before->opcode()));
  UseIter iter = before->uses().begin();
  while (iter != before->uses().end()) {
    if (IrOpcode::IsControlOpcode((*iter)->opcode()) &&
        NodeProperties::IsControlEdge(iter.edge())) {
      iter = iter.UpdateToAndIncrement(node);
      continue;
    }
    ++iter;
  }
}


void SimplifiedLowering::DoChangeTaggedToUI32(Node* node, Node* effect,
                                              Node* control, bool is_signed) {
  // if (IsTagged(val))
  // ConvertFloat64To(Int32|Uint32)(Load[kMachineFloat64](input, #value_offset))
  // else Untag(val)
  Node* val = node->InputAt(0);
  Node* branch = graph()->NewNode(common()->Branch(), IsTagged(val), control);

  // true branch.
  Node* tbranch = graph()->NewNode(common()->IfTrue(), branch);
  Node* loaded = graph()->NewNode(
      machine()->Load(kMachineFloat64), val,
      OffsetMinusTagConstant(HeapNumber::kValueOffset), effect);
  Operator* op = is_signed ? machine()->ChangeFloat64ToInt32()
                           : machine()->ChangeFloat64ToUint32();
  Node* converted = graph()->NewNode(op, loaded);

  // false branch.
  Node* fbranch = graph()->NewNode(common()->IfFalse(), branch);
  Node* untagged = Untag(val);

  // merge.
  Node* merge = graph()->NewNode(common()->Merge(2), tbranch, fbranch);
  Node* phi = graph()->NewNode(common()->Phi(2), converted, untagged, merge);
  UpdateControlSuccessors(control, merge);
  branch->ReplaceInput(1, control);
  node->ReplaceUses(phi);
}


void SimplifiedLowering::DoChangeTaggedToFloat64(Node* node, Node* effect,
                                                 Node* control) {
  // if (IsTagged(input)) Load[kMachineFloat64](input, #value_offset)
  // else ConvertFloat64(Untag(input))
  Node* val = node->InputAt(0);
  Node* branch = graph()->NewNode(common()->Branch(), IsTagged(val), control);

  // true branch.
  Node* tbranch = graph()->NewNode(common()->IfTrue(), branch);
  Node* loaded = graph()->NewNode(
      machine()->Load(kMachineFloat64), val,
      OffsetMinusTagConstant(HeapNumber::kValueOffset), effect);

  // false branch.
  Node* fbranch = graph()->NewNode(common()->IfFalse(), branch);
  Node* untagged = Untag(val);
  Node* converted =
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), untagged);

  // merge.
  Node* merge = graph()->NewNode(common()->Merge(2), tbranch, fbranch);
  Node* phi = graph()->NewNode(common()->Phi(2), loaded, converted, merge);
  UpdateControlSuccessors(control, merge);
  branch->ReplaceInput(1, control);
  node->ReplaceUses(phi);
}


void SimplifiedLowering::DoChangeUI32ToTagged(Node* node, Node* effect,
                                              Node* control, bool is_signed) {
  Node* val = node->InputAt(0);
  Node* is_smi = NULL;
  if (is_signed) {
    if (SmiValuesAre32Bits()) {
      // All int32s fit in this case.
      DCHECK(kPointerSize == 8);
      return node->ReplaceUses(SmiTag(val));
    } else {
      // TODO(turbofan): use an Int32AddWithOverflow to tag and check here.
      Node* lt = graph()->NewNode(machine()->Int32LessThanOrEqual(), val,
                                  jsgraph()->Int32Constant(Smi::kMaxValue));
      Node* gt =
          graph()->NewNode(machine()->Int32LessThanOrEqual(),
                           jsgraph()->Int32Constant(Smi::kMinValue), val);
      is_smi = graph()->NewNode(machine()->Word32And(), lt, gt);
    }
  } else {
    // Check if Uint32 value is in the smi range.
    is_smi = graph()->NewNode(machine()->Uint32LessThanOrEqual(), val,
                              jsgraph()->Int32Constant(Smi::kMaxValue));
  }

  // TODO(turbofan): fold smi test branch eagerly.
  // if (IsSmi(input)) SmiTag(input);
  // else InlineAllocAndInitHeapNumber(ConvertToFloat64(input)))
  Node* branch = graph()->NewNode(common()->Branch(), is_smi, control);

  // true branch.
  Node* tbranch = graph()->NewNode(common()->IfTrue(), branch);
  Node* smi_tagged = SmiTag(val);

  // false branch.
  Node* fbranch = graph()->NewNode(common()->IfFalse(), branch);
  Node* heap_num = jsgraph()->Constant(0.0);  // TODO(titzer): alloc and init

  // merge.
  Node* merge = graph()->NewNode(common()->Merge(2), tbranch, fbranch);
  Node* phi = graph()->NewNode(common()->Phi(2), smi_tagged, heap_num, merge);
  UpdateControlSuccessors(control, merge);
  branch->ReplaceInput(1, control);
  node->ReplaceUses(phi);
}


void SimplifiedLowering::DoChangeFloat64ToTagged(Node* node, Node* effect,
                                                 Node* control) {
  return;  // TODO(titzer): need to call runtime to allocate in one branch
}


void SimplifiedLowering::DoChangeBoolToBit(Node* node, Node* effect,
                                           Node* control) {
  Node* cmp = graph()->NewNode(machine()->WordEqual(), node->InputAt(0),
                               jsgraph()->TrueConstant());
  node->ReplaceUses(cmp);
}


void SimplifiedLowering::DoChangeBitToBool(Node* node, Node* effect,
                                           Node* control) {
  Node* val = node->InputAt(0);
  Node* branch = graph()->NewNode(common()->Branch(), val, control);

  // true branch.
  Node* tbranch = graph()->NewNode(common()->IfTrue(), branch);
  // false branch.
  Node* fbranch = graph()->NewNode(common()->IfFalse(), branch);
  // merge.
  Node* merge = graph()->NewNode(common()->Merge(2), tbranch, fbranch);
  Node* phi = graph()->NewNode(common()->Phi(2), jsgraph()->TrueConstant(),
                               jsgraph()->FalseConstant(), merge);
  UpdateControlSuccessors(control, merge);
  branch->ReplaceInput(1, control);
  node->ReplaceUses(phi);
}


static WriteBarrierKind ComputeWriteBarrierKind(BaseTaggedness base_is_tagged,
                                                MachineType representation,
                                                Type* type) {
  // TODO(turbofan): skip write barriers for Smis, etc.
  if (base_is_tagged == kTaggedBase && representation == kMachineTagged) {
    // Write barriers are only for writes into heap objects (i.e. tagged base).
    return kFullWriteBarrier;
  }
  return kNoWriteBarrier;
}


void SimplifiedLowering::DoLoadField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  node->set_op(machine_.Load(access.representation));
  Node* offset = jsgraph()->Int32Constant(access.offset - access.tag());
  node->InsertInput(zone(), 1, offset);
}


void SimplifiedLowering::DoStoreField(Node* node) {
  const FieldAccess& access = FieldAccessOf(node->op());
  WriteBarrierKind kind = ComputeWriteBarrierKind(
      access.base_is_tagged, access.representation, access.type);
  node->set_op(machine_.Store(access.representation, kind));
  Node* offset = jsgraph()->Int32Constant(access.offset - access.tag());
  node->InsertInput(zone(), 1, offset);
}


Node* SimplifiedLowering::ComputeIndex(const ElementAccess& access,
                                       Node* index) {
  int element_size = 0;
  switch (access.representation) {
    case kMachineTagged:
      element_size = kPointerSize;
      break;
    case kMachineWord8:
      element_size = 1;
      break;
    case kMachineWord16:
      element_size = 2;
      break;
    case kMachineWord32:
      element_size = 4;
      break;
    case kMachineWord64:
    case kMachineFloat64:
      element_size = 8;
      break;
    case kMachineLast:
      UNREACHABLE();
      break;
  }
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
  node->set_op(machine_.Load(access.representation));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
}


void SimplifiedLowering::DoStoreElement(Node* node) {
  const ElementAccess& access = ElementAccessOf(node->op());
  WriteBarrierKind kind = ComputeWriteBarrierKind(
      access.base_is_tagged, access.representation, access.type);
  node->set_op(machine_.Store(access.representation, kind));
  node->ReplaceInput(1, ComputeIndex(access, node->InputAt(1)));
}


void SimplifiedLowering::Lower(Node* node) {}


void SimplifiedLowering::LowerChange(Node* node, Node* effect, Node* control) {
  switch (node->opcode()) {
    case IrOpcode::kChangeTaggedToInt32:
      DoChangeTaggedToUI32(node, effect, control, true);
      break;
    case IrOpcode::kChangeTaggedToUint32:
      DoChangeTaggedToUI32(node, effect, control, false);
      break;
    case IrOpcode::kChangeTaggedToFloat64:
      DoChangeTaggedToFloat64(node, effect, control);
      break;
    case IrOpcode::kChangeInt32ToTagged:
      DoChangeUI32ToTagged(node, effect, control, true);
      break;
    case IrOpcode::kChangeUint32ToTagged:
      DoChangeUI32ToTagged(node, effect, control, false);
      break;
    case IrOpcode::kChangeFloat64ToTagged:
      DoChangeFloat64ToTagged(node, effect, control);
      break;
    case IrOpcode::kChangeBoolToBit:
      DoChangeBoolToBit(node, effect, control);
      break;
    case IrOpcode::kChangeBitToBool:
      DoChangeBitToBool(node, effect, control);
      break;
    default:
      UNREACHABLE();
      break;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
