// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bit-vector.h"
#include "src/compiler/instruction.h"
#include "src/compiler/register-allocator-verifier.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

size_t OperandCount(const Instruction* instr) {
  return instr->InputCount() + instr->OutputCount() + instr->TempCount();
}


void VerifyEmptyGaps(const Instruction* instr) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    CHECK(instr->GetParallelMove(inner_pos) == nullptr);
  }
}


void VerifyAllocatedGaps(const Instruction* instr) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    auto moves = instr->GetParallelMove(inner_pos);
    if (moves == nullptr) continue;
    for (auto move : *moves) {
      if (move->IsRedundant()) continue;
      CHECK(move->source().IsAllocated() || move->source().IsConstant());
      CHECK(move->destination().IsAllocated());
    }
  }
}

}  // namespace


void RegisterAllocatorVerifier::VerifyInput(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  if (constraint.type_ != kImmediate && constraint.type_ != kExplicit) {
    CHECK_NE(InstructionOperand::kInvalidVirtualRegister,
             constraint.virtual_register_);
  }
}


void RegisterAllocatorVerifier::VerifyTemp(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(kExplicit, constraint.type_);
  CHECK_NE(kConstant, constraint.type_);
}


void RegisterAllocatorVerifier::VerifyOutput(
    const OperandConstraint& constraint) {
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(kExplicit, constraint.type_);
  CHECK_NE(InstructionOperand::kInvalidVirtualRegister,
           constraint.virtual_register_);
}


RegisterAllocatorVerifier::RegisterAllocatorVerifier(
    Zone* zone, const RegisterConfiguration* config,
    const InstructionSequence* sequence)
    : zone_(zone), config_(config), sequence_(sequence), constraints_(zone) {
  constraints_.reserve(sequence->instructions().size());
  // TODO(dcarney): model unique constraints.
  // Construct OperandConstraints for all InstructionOperands, eliminating
  // kSameAsFirst along the way.
  for (const auto* instr : sequence->instructions()) {
    // All gaps should be totally unallocated at this point.
    VerifyEmptyGaps(instr);
    const size_t operand_count = OperandCount(instr);
    auto* op_constraints = zone->NewArray<OperandConstraint>(operand_count);
    size_t count = 0;
    for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
      BuildConstraint(instr->InputAt(i), &op_constraints[count]);
      VerifyInput(op_constraints[count]);
    }
    for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
      BuildConstraint(instr->TempAt(i), &op_constraints[count]);
      VerifyTemp(op_constraints[count]);
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
      BuildConstraint(instr->OutputAt(i), &op_constraints[count]);
      if (op_constraints[count].type_ == kSameAsFirst) {
        CHECK(instr->InputCount() > 0);
        op_constraints[count].type_ = op_constraints[0].type_;
        op_constraints[count].value_ = op_constraints[0].value_;
      }
      VerifyOutput(op_constraints[count]);
    }
    InstructionConstraint instr_constraint = {instr, operand_count,
                                              op_constraints};
    constraints()->push_back(instr_constraint);
  }
}


void RegisterAllocatorVerifier::VerifyAssignment() {
  CHECK(sequence()->instructions().size() == constraints()->size());
  auto instr_it = sequence()->begin();
  for (const auto& instr_constraint : *constraints()) {
    const auto* instr = instr_constraint.instruction_;
    // All gaps should be totally allocated at this point.
    VerifyAllocatedGaps(instr);
    const size_t operand_count = instr_constraint.operand_constaints_size_;
    const auto* op_constraints = instr_constraint.operand_constraints_;
    CHECK_EQ(instr, *instr_it);
    CHECK(operand_count == OperandCount(instr));
    size_t count = 0;
    for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
      CheckConstraint(instr->InputAt(i), &op_constraints[count]);
    }
    for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
      CheckConstraint(instr->TempAt(i), &op_constraints[count]);
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
      CheckConstraint(instr->OutputAt(i), &op_constraints[count]);
    }
    ++instr_it;
  }
}


void RegisterAllocatorVerifier::BuildConstraint(const InstructionOperand* op,
                                                OperandConstraint* constraint) {
  constraint->value_ = kMinInt;
  constraint->virtual_register_ = InstructionOperand::kInvalidVirtualRegister;
  if (op->IsConstant()) {
    constraint->type_ = kConstant;
    constraint->value_ = ConstantOperand::cast(op)->virtual_register();
    constraint->virtual_register_ = constraint->value_;
  } else if (op->IsExplicit()) {
    constraint->type_ = kExplicit;
  } else if (op->IsImmediate()) {
    auto imm = ImmediateOperand::cast(op);
    int value = imm->type() == ImmediateOperand::INLINE ? imm->inline_value()
                                                        : imm->indexed_value();
    constraint->type_ = kImmediate;
    constraint->value_ = value;
  } else {
    CHECK(op->IsUnallocated());
    const auto* unallocated = UnallocatedOperand::cast(op);
    int vreg = unallocated->virtual_register();
    constraint->virtual_register_ = vreg;
    if (unallocated->basic_policy() == UnallocatedOperand::FIXED_SLOT) {
      constraint->type_ = sequence()->IsFloat(vreg) ? kDoubleSlot : kSlot;
      constraint->value_ = unallocated->fixed_slot_index();
    } else {
      switch (unallocated->extended_policy()) {
        case UnallocatedOperand::ANY:
        case UnallocatedOperand::NONE:
          if (sequence()->IsFloat(vreg)) {
            constraint->type_ = kNoneDouble;
          } else {
            constraint->type_ = kNone;
          }
          break;
        case UnallocatedOperand::FIXED_REGISTER:
          if (unallocated->HasSecondaryStorage()) {
            constraint->type_ = kRegisterAndSlot;
            constraint->spilled_slot_ = unallocated->GetSecondaryStorage();
          } else {
            constraint->type_ = kFixedRegister;
          }
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::FIXED_DOUBLE_REGISTER:
          constraint->type_ = kFixedDoubleRegister;
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::MUST_HAVE_REGISTER:
          if (sequence()->IsFloat(vreg)) {
            constraint->type_ = kDoubleRegister;
          } else {
            constraint->type_ = kRegister;
          }
          break;
        case UnallocatedOperand::MUST_HAVE_SLOT:
          constraint->type_ = sequence()->IsFloat(vreg) ? kDoubleSlot : kSlot;
          break;
        case UnallocatedOperand::SAME_AS_FIRST_INPUT:
          constraint->type_ = kSameAsFirst;
          break;
      }
    }
  }
}


void RegisterAllocatorVerifier::CheckConstraint(
    const InstructionOperand* op, const OperandConstraint* constraint) {
  switch (constraint->type_) {
    case kConstant:
      CHECK(op->IsConstant());
      CHECK_EQ(ConstantOperand::cast(op)->virtual_register(),
               constraint->value_);
      return;
    case kImmediate: {
      CHECK(op->IsImmediate());
      auto imm = ImmediateOperand::cast(op);
      int value = imm->type() == ImmediateOperand::INLINE
                      ? imm->inline_value()
                      : imm->indexed_value();
      CHECK_EQ(value, constraint->value_);
      return;
    }
    case kRegister:
      CHECK(op->IsRegister());
      return;
    case kDoubleRegister:
      CHECK(op->IsDoubleRegister());
      return;
    case kExplicit:
      CHECK(op->IsExplicit());
      return;
    case kFixedRegister:
    case kRegisterAndSlot:
      CHECK(op->IsRegister());
      CHECK_EQ(LocationOperand::cast(op)->GetRegister().code(),
               constraint->value_);
      return;
    case kFixedDoubleRegister:
      CHECK(op->IsDoubleRegister());
      CHECK_EQ(LocationOperand::cast(op)->GetDoubleRegister().code(),
               constraint->value_);
      return;
    case kFixedSlot:
      CHECK(op->IsStackSlot());
      CHECK_EQ(LocationOperand::cast(op)->index(), constraint->value_);
      return;
    case kSlot:
      CHECK(op->IsStackSlot());
      return;
    case kDoubleSlot:
      CHECK(op->IsDoubleStackSlot());
      return;
    case kNone:
      CHECK(op->IsRegister() || op->IsStackSlot());
      return;
    case kNoneDouble:
      CHECK(op->IsDoubleRegister() || op->IsDoubleStackSlot());
      return;
    case kSameAsFirst:
      CHECK(false);
      return;
  }
}

namespace {

typedef RpoNumber Rpo;

static const int kInvalidVreg = InstructionOperand::kInvalidVirtualRegister;

struct PhiData : public ZoneObject {
  PhiData(Rpo definition_rpo, const PhiInstruction* phi, int first_pred_vreg,
          const PhiData* first_pred_phi, Zone* zone)
      : definition_rpo(definition_rpo),
        virtual_register(phi->virtual_register()),
        first_pred_vreg(first_pred_vreg),
        first_pred_phi(first_pred_phi),
        operands(zone) {
    operands.reserve(phi->operands().size());
    operands.insert(operands.begin(), phi->operands().begin(),
                    phi->operands().end());
  }
  const Rpo definition_rpo;
  const int virtual_register;
  const int first_pred_vreg;
  const PhiData* first_pred_phi;
  IntVector operands;
};

class PhiMap : public ZoneMap<int, PhiData*>, public ZoneObject {
 public:
  explicit PhiMap(Zone* zone) : ZoneMap<int, PhiData*>(zone) {}
};

struct OperandLess {
  bool operator()(const InstructionOperand* a,
                  const InstructionOperand* b) const {
    return a->CompareCanonicalized(*b);
  }
};

class OperandMap : public ZoneObject {
 public:
  struct MapValue : public ZoneObject {
    MapValue()
        : incoming(nullptr),
          define_vreg(kInvalidVreg),
          use_vreg(kInvalidVreg),
          succ_vreg(kInvalidVreg) {}
    MapValue* incoming;  // value from first predecessor block.
    int define_vreg;     // valid if this value was defined in this block.
    int use_vreg;        // valid if this value was used in this block.
    int succ_vreg;       // valid if propagated back from successor block.
  };

  class Map
      : public ZoneMap<const InstructionOperand*, MapValue*, OperandLess> {
   public:
    explicit Map(Zone* zone)
        : ZoneMap<const InstructionOperand*, MapValue*, OperandLess>(zone) {}

    // Remove all entries with keys not in other.
    void Intersect(const Map& other) {
      if (this->empty()) return;
      auto it = this->begin();
      OperandLess less;
      for (const auto& o : other) {
        while (less(it->first, o.first)) {
          this->erase(it++);
          if (it == this->end()) return;
        }
        if (it->first->EqualsCanonicalized(*o.first)) {
          ++it;
          if (it == this->end()) return;
        } else {
          CHECK(less(o.first, it->first));
        }
      }
    }
  };

  explicit OperandMap(Zone* zone) : map_(zone) {}

  Map& map() { return map_; }

  void RunParallelMoves(Zone* zone, const ParallelMove* moves) {
    // Compute outgoing mappings.
    Map to_insert(zone);
    for (auto move : *moves) {
      if (move->IsEliminated()) continue;
      auto cur = map().find(&move->source());
      CHECK(cur != map().end());
      auto res =
          to_insert.insert(std::make_pair(&move->destination(), cur->second));
      // Ensure injectivity of moves.
      CHECK(res.second);
    }
    // Drop current mappings.
    for (auto move : *moves) {
      if (move->IsEliminated()) continue;
      auto cur = map().find(&move->destination());
      if (cur != map().end()) map().erase(cur);
    }
    // Insert new values.
    map().insert(to_insert.begin(), to_insert.end());
  }

  void RunGaps(Zone* zone, const Instruction* instr) {
    for (int i = Instruction::FIRST_GAP_POSITION;
         i <= Instruction::LAST_GAP_POSITION; i++) {
      auto inner_pos = static_cast<Instruction::GapPosition>(i);
      auto move = instr->GetParallelMove(inner_pos);
      if (move == nullptr) continue;
      RunParallelMoves(zone, move);
    }
  }

  void Drop(const InstructionOperand* op) {
    auto it = map().find(op);
    if (it != map().end()) map().erase(it);
  }

  void DropRegisters(const RegisterConfiguration* config) {
    // TODO(dcarney): sort map by kind and drop range.
    for (auto it = map().begin(); it != map().end();) {
      auto op = it->first;
      if (op->IsRegister() || op->IsDoubleRegister()) {
        map().erase(it++);
      } else {
        ++it;
      }
    }
  }

  MapValue* Define(Zone* zone, const InstructionOperand* op,
                   int virtual_register) {
    auto value = new (zone) MapValue();
    value->define_vreg = virtual_register;
    auto res = map().insert(std::make_pair(op, value));
    if (!res.second) res.first->second = value;
    return value;
  }

  void Use(const InstructionOperand* op, int use_vreg, bool initial_pass) {
    auto it = map().find(op);
    CHECK(it != map().end());
    auto v = it->second;
    if (v->define_vreg != kInvalidVreg) {
      CHECK_EQ(v->define_vreg, use_vreg);
    }
    // Already used this vreg in this block.
    if (v->use_vreg != kInvalidVreg) {
      CHECK_EQ(v->use_vreg, use_vreg);
      return;
    }
    if (!initial_pass) {
      // A value may be defined and used in this block or the use must have
      // propagated up.
      if (v->succ_vreg != kInvalidVreg) {
        CHECK_EQ(v->succ_vreg, use_vreg);
      } else {
        CHECK_EQ(v->define_vreg, use_vreg);
      }
      // Mark the use.
      it->second->use_vreg = use_vreg;
      return;
    }
    // Go up block list and ensure the correct definition is reached.
    for (; v != nullptr; v = v->incoming) {
      // Value unused in block.
      if (v->define_vreg == kInvalidVreg && v->use_vreg == kInvalidVreg) {
        continue;
      }
      // Found correct definition or use.
      CHECK(v->define_vreg == use_vreg || v->use_vreg == use_vreg);
      // Mark the use.
      it->second->use_vreg = use_vreg;
      return;
    }
    // Use of a non-phi value without definition.
    CHECK(false);
  }

  void UsePhi(const InstructionOperand* op, const PhiData* phi,
              bool initial_pass) {
    auto it = map().find(op);
    CHECK(it != map().end());
    auto v = it->second;
    int use_vreg = phi->virtual_register;
    // Phis are not defined.
    CHECK_EQ(kInvalidVreg, v->define_vreg);
    // Already used this vreg in this block.
    if (v->use_vreg != kInvalidVreg) {
      CHECK_EQ(v->use_vreg, use_vreg);
      return;
    }
    if (!initial_pass) {
      // A used phi must have propagated its use to a predecessor.
      CHECK_EQ(v->succ_vreg, use_vreg);
      // Mark the use.
      v->use_vreg = use_vreg;
      return;
    }
    // Go up the block list starting at the first predecessor and ensure this
    // phi has a correct use or definition.
    for (v = v->incoming; v != nullptr; v = v->incoming) {
      // Value unused in block.
      if (v->define_vreg == kInvalidVreg && v->use_vreg == kInvalidVreg) {
        continue;
      }
      // Found correct definition or use.
      if (v->define_vreg != kInvalidVreg) {
        CHECK(v->define_vreg == phi->first_pred_vreg);
      } else if (v->use_vreg != phi->first_pred_vreg) {
        // Walk the phi chain, hunting for a matching phi use.
        auto p = phi;
        for (; p != nullptr; p = p->first_pred_phi) {
          if (p->virtual_register == v->use_vreg) break;
        }
        CHECK(p);
      }
      // Mark the use.
      it->second->use_vreg = use_vreg;
      return;
    }
    // Use of a phi value without definition.
    UNREACHABLE();
  }

 private:
  Map map_;
  DISALLOW_COPY_AND_ASSIGN(OperandMap);
};

}  // namespace


class RegisterAllocatorVerifier::BlockMaps {
 public:
  BlockMaps(Zone* zone, const InstructionSequence* sequence)
      : zone_(zone),
        sequence_(sequence),
        phi_map_guard_(sequence->VirtualRegisterCount(), zone),
        phi_map_(zone),
        incoming_maps_(zone),
        outgoing_maps_(zone) {
    InitializePhis();
    InitializeOperandMaps();
  }

  bool IsPhi(int virtual_register) {
    return phi_map_guard_.Contains(virtual_register);
  }

  const PhiData* GetPhi(int virtual_register) {
    auto it = phi_map_.find(virtual_register);
    CHECK(it != phi_map_.end());
    return it->second;
  }

  OperandMap* InitializeIncoming(size_t block_index, bool initial_pass) {
    return initial_pass ? InitializeFromFirstPredecessor(block_index)
                        : InitializeFromIntersection(block_index);
  }

  void PropagateUsesBackwards() {
    typedef std::set<size_t, std::greater<size_t>, zone_allocator<size_t>>
        BlockIds;
    BlockIds block_ids((BlockIds::key_compare()),
                       zone_allocator<size_t>(zone()));
    // First ensure that incoming contains only keys in all predecessors.
    for (auto block : sequence()->instruction_blocks()) {
      size_t index = block->rpo_number().ToSize();
      block_ids.insert(index);
      auto& succ_map = incoming_maps_[index]->map();
      for (size_t i = 0; i < block->PredecessorCount(); ++i) {
        auto pred_rpo = block->predecessors()[i];
        succ_map.Intersect(outgoing_maps_[pred_rpo.ToSize()]->map());
      }
    }
    // Back propagation fixpoint.
    while (!block_ids.empty()) {
      // Pop highest block_id.
      auto block_id_it = block_ids.begin();
      const size_t succ_index = *block_id_it;
      block_ids.erase(block_id_it);
      // Propagate uses back to their definition blocks using succ_vreg.
      auto block = sequence()->instruction_blocks()[succ_index];
      auto& succ_map = incoming_maps_[succ_index]->map();
      for (size_t i = 0; i < block->PredecessorCount(); ++i) {
        for (auto& succ_val : succ_map) {
          // An incoming map contains no defines.
          CHECK_EQ(kInvalidVreg, succ_val.second->define_vreg);
          // Compute succ_vreg.
          int succ_vreg = succ_val.second->succ_vreg;
          if (succ_vreg == kInvalidVreg) {
            succ_vreg = succ_val.second->use_vreg;
            // Initialize succ_vreg in back propagation chain.
            succ_val.second->succ_vreg = succ_vreg;
          }
          if (succ_vreg == kInvalidVreg) continue;
          // May need to transition phi.
          if (IsPhi(succ_vreg)) {
            auto phi = GetPhi(succ_vreg);
            if (phi->definition_rpo.ToSize() == succ_index) {
              // phi definition block, transition to pred value.
              succ_vreg = phi->operands[i];
            }
          }
          // Push succ_vreg up to all predecessors.
          auto pred_rpo = block->predecessors()[i];
          auto& pred_map = outgoing_maps_[pred_rpo.ToSize()]->map();
          auto& pred_val = *pred_map.find(succ_val.first);
          if (pred_val.second->use_vreg != kInvalidVreg) {
            CHECK_EQ(succ_vreg, pred_val.second->use_vreg);
          }
          if (pred_val.second->define_vreg != kInvalidVreg) {
            CHECK_EQ(succ_vreg, pred_val.second->define_vreg);
          }
          if (pred_val.second->succ_vreg != kInvalidVreg) {
            if (succ_vreg != pred_val.second->succ_vreg) {
              // When a block introduces 2 identical phis A and B, and both are
              // operands to other phis C and D, and we optimized the moves
              // defining A or B such that they now appear in the block defining
              // A and B, the back propagation will get confused when visiting
              // upwards from C and D. The operand in the block defining A and B
              // will be attributed to C (or D, depending which of these is
              // visited first).
              CHECK(IsPhi(pred_val.second->succ_vreg));
              CHECK(IsPhi(succ_vreg));
              const PhiData* current_phi = GetPhi(succ_vreg);
              const PhiData* assigned_phi = GetPhi(pred_val.second->succ_vreg);
              CHECK_EQ(current_phi->operands.size(),
                       assigned_phi->operands.size());
              CHECK_EQ(current_phi->definition_rpo,
                       assigned_phi->definition_rpo);
              for (size_t i = 0; i < current_phi->operands.size(); ++i) {
                CHECK_EQ(current_phi->operands[i], assigned_phi->operands[i]);
              }
            }
          } else {
            pred_val.second->succ_vreg = succ_vreg;
            block_ids.insert(pred_rpo.ToSize());
          }
        }
      }
    }
    // Clear uses and back links for second pass.
    for (auto operand_map : incoming_maps_) {
      for (auto& succ_val : operand_map->map()) {
        succ_val.second->incoming = nullptr;
        succ_val.second->use_vreg = kInvalidVreg;
      }
    }
  }

 private:
  OperandMap* InitializeFromFirstPredecessor(size_t block_index) {
    auto to_init = outgoing_maps_[block_index];
    CHECK(to_init->map().empty());
    auto block = sequence()->instruction_blocks()[block_index];
    if (block->predecessors().empty()) return to_init;
    size_t predecessor_index = block->predecessors()[0].ToSize();
    // Ensure not a backedge.
    CHECK(predecessor_index < block->rpo_number().ToSize());
    auto incoming = outgoing_maps_[predecessor_index];
    // Copy map and replace values.
    to_init->map() = incoming->map();
    for (auto& it : to_init->map()) {
      auto incoming = it.second;
      it.second = new (zone()) OperandMap::MapValue();
      it.second->incoming = incoming;
    }
    // Copy to incoming map for second pass.
    incoming_maps_[block_index]->map() = to_init->map();
    return to_init;
  }

  OperandMap* InitializeFromIntersection(size_t block_index) {
    return incoming_maps_[block_index];
  }

  void InitializeOperandMaps() {
    size_t block_count = sequence()->instruction_blocks().size();
    incoming_maps_.reserve(block_count);
    outgoing_maps_.reserve(block_count);
    for (size_t i = 0; i < block_count; ++i) {
      incoming_maps_.push_back(new (zone()) OperandMap(zone()));
      outgoing_maps_.push_back(new (zone()) OperandMap(zone()));
    }
  }

  void InitializePhis() {
    const size_t block_count = sequence()->instruction_blocks().size();
    for (size_t block_index = 0; block_index < block_count; ++block_index) {
      const auto block = sequence()->instruction_blocks()[block_index];
      for (auto phi : block->phis()) {
        int first_pred_vreg = phi->operands()[0];
        const PhiData* first_pred_phi = nullptr;
        if (IsPhi(first_pred_vreg)) {
          first_pred_phi = GetPhi(first_pred_vreg);
          first_pred_vreg = first_pred_phi->first_pred_vreg;
        }
        CHECK(!IsPhi(first_pred_vreg));
        auto phi_data = new (zone()) PhiData(
            block->rpo_number(), phi, first_pred_vreg, first_pred_phi, zone());
        auto res =
            phi_map_.insert(std::make_pair(phi->virtual_register(), phi_data));
        CHECK(res.second);
        phi_map_guard_.Add(phi->virtual_register());
      }
    }
  }

  typedef ZoneVector<OperandMap*> OperandMaps;
  typedef ZoneVector<PhiData*> PhiVector;

  Zone* zone() const { return zone_; }
  const InstructionSequence* sequence() const { return sequence_; }

  Zone* const zone_;
  const InstructionSequence* const sequence_;
  BitVector phi_map_guard_;
  PhiMap phi_map_;
  OperandMaps incoming_maps_;
  OperandMaps outgoing_maps_;
};


void RegisterAllocatorVerifier::VerifyGapMoves() {
  BlockMaps block_maps(zone(), sequence());
  VerifyGapMoves(&block_maps, true);
  block_maps.PropagateUsesBackwards();
  VerifyGapMoves(&block_maps, false);
}


// Compute and verify outgoing values for every block.
void RegisterAllocatorVerifier::VerifyGapMoves(BlockMaps* block_maps,
                                               bool initial_pass) {
  const size_t block_count = sequence()->instruction_blocks().size();
  for (size_t block_index = 0; block_index < block_count; ++block_index) {
    auto current = block_maps->InitializeIncoming(block_index, initial_pass);
    const auto block = sequence()->instruction_blocks()[block_index];
    for (int instr_index = block->code_start(); instr_index < block->code_end();
         ++instr_index) {
      const auto& instr_constraint = constraints_[instr_index];
      const auto instr = instr_constraint.instruction_;
      current->RunGaps(zone(), instr);
      const auto op_constraints = instr_constraint.operand_constraints_;
      size_t count = 0;
      for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
        if (op_constraints[count].type_ == kImmediate ||
            op_constraints[count].type_ == kExplicit) {
          continue;
        }
        int virtual_register = op_constraints[count].virtual_register_;
        auto op = instr->InputAt(i);
        if (!block_maps->IsPhi(virtual_register)) {
          current->Use(op, virtual_register, initial_pass);
        } else {
          auto phi = block_maps->GetPhi(virtual_register);
          current->UsePhi(op, phi, initial_pass);
        }
      }
      for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
        current->Drop(instr->TempAt(i));
      }
      if (instr->IsCall()) {
        current->DropRegisters(config());
      }
      for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
        int virtual_register = op_constraints[count].virtual_register_;
        OperandMap::MapValue* value =
            current->Define(zone(), instr->OutputAt(i), virtual_register);
        if (op_constraints[count].type_ == kRegisterAndSlot) {
          const AllocatedOperand* reg_op =
              AllocatedOperand::cast(instr->OutputAt(i));
          MachineRepresentation rep = reg_op->representation();
          const AllocatedOperand* stack_op = AllocatedOperand::New(
              zone(), LocationOperand::LocationKind::STACK_SLOT, rep,
              op_constraints[i].spilled_slot_);
          auto insert_result =
              current->map().insert(std::make_pair(stack_op, value));
          DCHECK(insert_result.second);
          USE(insert_result);
        }
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
