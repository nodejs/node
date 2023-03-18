// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-phi-representation-selector.h"

#include "src/handles/handles-inl.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

constexpr int representation_mask_for(ValueRepresentation r) {
  if (r == ValueRepresentation::kTagged) return 0;
  return 1 << static_cast<int>(r);
}

}  // namespace

void MaglevPhiRepresentationSelector::Process(Phi* node,
                                              const ProcessingState&) {
  DCHECK_EQ(node->value_representation(), ValueRepresentation::kTagged);

  // We negate the Uint32 from {representation_mask} from the start in order to
  // disable Uint32 untagged Phis for now, because they don't seem very useful,
  // and this simplifies things a little bit.
  // TODO(dmercadier): evaluate what benefits untagged Uint32 Phis would bring,
  // and consider allowing them.
  unsigned int representation_mask =
      ~representation_mask_for(ValueRepresentation::kUint32);

  for (int i = 0; i < node->input_count(); i++) {
    Node* input = node->input(i).node();
    if (input->Is<SmiConstant>()) {
      // Could be any representation
    } else if (input->properties().is_conversion()) {
      DCHECK_EQ(input->input_count(), 1);
      representation_mask &= representation_mask_for(
          input->input(0).node()->properties().value_representation());
    } else {
      representation_mask = 0;
      break;
    }
  }

  if (base::bits::CountPopulation(representation_mask) == 1) {
    ConvertTaggedPhiTo(
        node, static_cast<ValueRepresentation>(
                  base::bits::CountTrailingZeros(representation_mask)));
  } else {
    // No possible representations, or more than 1 possible representation.
    // We'll typically end up in this case if:
    //   - All of the inputs of the Phi are SmiConstant (which can have any
    //     representation)
    //   - Some inputs are Int32 tagging and other inputs are Float64 tagging.
    // If we had information about uses of the node, then we could chose a
    // representation over the other (eg, Float64 rather than Int32), but
    // since we don't, we just bail out to avoid choosing the wrong
    // representation.
    EnsurePhiInputsTagged(node);
  }
}

void MaglevPhiRepresentationSelector::EnsurePhiInputsTagged(Phi* phi) {
  // Since we are untagging some Phis, it's possible that one of the inputs of
  // {phi} is an untagged Phi. However, if this function is called, then we've
  // decided that {phi} is going to stay tagged, and thus, all of its inputs
  // should be tagged. We'll thus insert tagging operation on the untagged phi
  // inputs of {phi}.

  for (int i = 0; i < phi->input_count(); i++) {
    ValueNode* input = phi->input(i).node();
    if (Phi* phi_input = input->TryCast<Phi>()) {
      phi->set_input(
          i, TagPhi(phi_input, phi->predecessor_at(i), NewNodePosition::kEnd));
    } else {
      // Inputs of Phis that aren't Phi should always be tagged (except for the
      // phis untagged by this class, but {phi} isn't one of them).
      DCHECK(input->is_tagged());
    }
  }
}

void MaglevPhiRepresentationSelector::ConvertTaggedPhiTo(
    Phi* phi, ValueRepresentation repr) {
  phi->change_representation(repr);

  for (int i = 0; i < phi->input_count(); i++) {
    ValueNode* input = phi->input(i).node();
    if (input->Is<SmiConstant>()) {
      switch (repr) {
        case ValueRepresentation::kInt32:
          phi->change_input(i,
                            builder_->GetInt32Constant(
                                input->Cast<SmiConstant>()->value().value()));
          break;
        case ValueRepresentation::kFloat64:
          phi->change_input(i,
                            builder_->GetFloat64Constant(
                                input->Cast<SmiConstant>()->value().value()));
          break;
        case ValueRepresentation::kUint32:
          UNIMPLEMENTED();
        default:
          UNREACHABLE();
      }
    } else if (input->properties().is_conversion()) {
      // Unwrapping the conversion.
      DCHECK_EQ(input->value_representation(), ValueRepresentation::kTagged);
      DCHECK_EQ(input->input(0).node()->value_representation(), repr);
      phi->set_input(i, input->input(0).node());
    } else {
      DCHECK_EQ(input->value_representation(), repr);
    }
  }
}

bool MaglevPhiRepresentationSelector::IsUntagging(Opcode op) {
  switch (op) {
    case Opcode::kCheckedSmiUntag:
    case Opcode::kUnsafeSmiUntag:
    case Opcode::kCheckedObjectToIndex:
    case Opcode::kCheckedTruncateNumberToInt32:
    case Opcode::kTruncateNumberToInt32:
    case Opcode::kCheckedFloat64Unbox:
    case Opcode::kUnsafeFloat64Unbox:
      return true;
    default:
      return false;
  }
}

namespace {

base::Optional<Opcode> GetOpcodeForCheckedConversion(ValueRepresentation from,
                                                     ValueRepresentation to) {
  static_assert(static_cast<int>(ValueRepresentation::kTagged) == 0);
  static_assert(static_cast<int>(ValueRepresentation::kInt32) == 1);
  static_assert(static_cast<int>(ValueRepresentation::kUint32) == 2);
  static_assert(static_cast<int>(ValueRepresentation::kFloat64) == 3);

  constexpr int kNumberOfValueRepresentation = 4;

  static base::Optional<Opcode> conversion_table[] = {
      // Int32 ->
      Opcode::kInt32ToNumber,         // -> Tagged
      base::nullopt,                  // -> Int32
      Opcode::kCheckedInt32ToUint32,  // -> Uint32
      Opcode::kChangeInt32ToFloat64,  // -> Float64
      // Uint32 ->
      Opcode::kUint32ToNumber,         // -> Tagged
      Opcode::kCheckedInt32ToUint32,   // -> Int32
      base::nullopt,                   // -> Uint32
      Opcode::kChangeUint32ToFloat64,  // -> Float64
      // Float64 ->
      Opcode::kFloat64Box,              // -> Tagged
      Opcode::kTruncateFloat64ToInt32,  // -> Int32
      // ^ Note that Maglev doesn't have a Tagged HeapNumber->Int32 conversion
      // that checks if converting the Float64 to a Int32 loses precision. We
      // thus use here TruncateFloat64ToInt32 instead of
      // CheckedTruncateFloat64ToInt32.
      base::nullopt,  // -> Uint32
      // ^ The graph builder never inserts Tagged->Uint32 conversions, so we
      // don't have to handle this case.
      base::nullopt  // -> Float64
  };

  return conversion_table[(static_cast<int>(from) - 1) *
                              kNumberOfValueRepresentation +
                          static_cast<int>(to)];
}

}  // namespace

ValueNode* MaglevPhiRepresentationSelector::GetInputReplacement(
    ValueNode* old_conversion) {
  DCHECK_EQ(old_conversion->input_count(), 1);
  DCHECK(old_conversion->input(0).node()->Is<Phi>());

  ValueRepresentation from_repr =
      old_conversion->input(0).node()->value_representation();
  ValueRepresentation to_repr = old_conversion->value_representation();

  // Since initially Phis are tagged, it would make not sense for
  // {old_conversion} to convert a Phi to a Tagged value.
  DCHECK_NE(to_repr, ValueRepresentation::kTagged);
  // The graph builder never inserts Tagged->Uint32 conversions (and thus, we
  // don't handle them in GetOpcodeForCheckedConversion).
  DCHECK_NE(to_repr, ValueRepresentation::kUint32);

  if (from_repr == ValueRepresentation::kTagged) {
    // The Phi hasn't been untagged, so we leave the conversion as it is.
    return old_conversion;
  }

  if (from_repr == to_repr) {
    old_conversion->kill();
    return old_conversion->input(0).node();
  }

  if (old_conversion->Is<UnsafeSmiUntag>()) {
    // UnsafeSmiTag are only inserted when the node is a known Smi. If the
    // current phi has a Float64/Uint32 representation, then we can safely
    // truncate it to Int32, because we know that the Float64/Uint32 fits in a
    // Smi, and therefore in a Int32.
    if (from_repr == ValueRepresentation::kFloat64) {
      old_conversion->OverwriteWith(Opcode::kUnsafeTruncateFloat64ToInt32,
                                    UnsafeTruncateFloat64ToInt32::kProperties);
      return old_conversion;
    } else if (from_repr == ValueRepresentation::kUint32) {
      old_conversion->OverwriteWith(Opcode::kUnsafeTruncateUint32ToInt32,
                                    UnsafeTruncateUint32ToInt32::kProperties);
      return old_conversion;
    } else {
      DCHECK_EQ(from_repr, ValueRepresentation::kInt32);
      old_conversion->kill();
      return old_conversion->input(0).node();
    }
  }

  base::Optional<Opcode> needed_conversion =
      GetOpcodeForCheckedConversion(from_repr, to_repr);

  DCHECK(needed_conversion.has_value());

  if (*needed_conversion != old_conversion->opcode()) {
    old_conversion->OverwriteWith(*needed_conversion);
  }

  return old_conversion;
}

ValueNode* MaglevPhiRepresentationSelector::TagPhi(Phi* phi, BasicBlock* block,
                                                   NewNodePosition pos) {
  switch (phi->value_representation()) {
    case ValueRepresentation::kFloat64:
      return AddNode(NodeBase::New<Float64Box>(builder_->zone(), {phi}), block,
                     pos);
    case ValueRepresentation::kInt32:
      return AddNode(NodeBase::New<Int32ToNumber>(builder_->zone(), {phi}),
                     block, pos);
    case ValueRepresentation::kUint32:
      return AddNode(NodeBase::New<Uint32ToNumber>(builder_->zone(), {phi}),
                     block, pos);
    case ValueRepresentation::kTagged:
      return phi;
    case ValueRepresentation::kWord64:
      UNREACHABLE();
  }
}

void MaglevPhiRepresentationSelector::FixLoopPhisBackedge(BasicBlock* block) {
  // TODO(dmercadier): it would be interesting to compute a fix point for loop
  // phis, or at least to go over the loop header twice.
  if (!block->has_phi()) return;
  for (Phi* phi : *block->phis()) {
    if (phi->value_representation() == ValueRepresentation::kTagged) {
      int last_input_idx = phi->input_count() - 1;
      if (phi->input(last_input_idx).node()->value_representation() !=
          ValueRepresentation::kTagged) {
        // Since all Phi inputs are initially tagged, the fact that the backedge
        // is not tagged means that it's a Phi that we recently untagged.
        DCHECK(phi->input(last_input_idx).node()->Is<Phi>());
        phi->set_input(last_input_idx,
                       TagPhi(phi->input(last_input_idx).node()->Cast<Phi>(),
                              current_block_, NewNodePosition::kEnd));
      }
    }
  }
}

ValueNode* MaglevPhiRepresentationSelector::AddNode(ValueNode* node,
                                                    BasicBlock* block,
                                                    NewNodePosition pos) {
  if (block == current_block_) {
    // When adding an Node in the current block, we delay until we've finished
    // processing the current block, to avoid mutating the list of nodes while
    // we're iterating it.
    if (pos == NewNodePosition::kStart) {
      new_nodes_current_block_start_.push_back(node);
    } else {
      new_nodes_current_block_end_.push_back(node);
    }
  } else {
    // However, when adding a node in a predecessor, the node won't be used
    // until the current block, and it might be using nodes computed in the
    // predecessor, so we add it at the end of the predecessor.
    DCHECK_EQ(pos, NewNodePosition::kEnd);
    block->nodes().Add(node);
  }
  if (builder_->has_graph_labeller()) {
    builder_->graph_labeller()->RegisterNode(node);
  }
  return node;
}

void MaglevPhiRepresentationSelector::MergeNewNodesInBlock(BasicBlock* block) {
  if (block != nullptr && !new_nodes_current_block_start_.empty()) {
    for (Node* node : new_nodes_current_block_start_) {
      block->nodes().AddFront(node);
    }
  }
  new_nodes_current_block_start_.clear();

  if (block != nullptr && !new_nodes_current_block_end_.empty()) {
    for (Node* node : new_nodes_current_block_end_) {
      block->nodes().Add(node);
    }
  }
  new_nodes_current_block_end_.clear();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
