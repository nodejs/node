// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/gap-resolver.h"

#include "src/base/utils/random-number-generator.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

// The state of our move interpreter is the mapping of operands to values. Note
// that the actual values don't really matter, all we care about is equality.
class InterpreterState {
 public:
  void ExecuteInParallel(const ParallelMove* moves) {
    InterpreterState copy(*this);
    for (const auto m : *moves) {
      if (!m->IsRedundant()) write(m->destination(), copy.read(m->source()));
    }
  }

  bool operator==(const InterpreterState& other) const {
    return values_ == other.values_;
  }

  bool operator!=(const InterpreterState& other) const {
    return values_ != other.values_;
  }

 private:
  struct Key {
    bool is_constant;
    MachineRepresentation rep;
    LocationOperand::LocationKind kind;
    int index;

    bool operator<(const Key& other) const {
      if (this->is_constant != other.is_constant) {
        return this->is_constant;
      }
      if (this->rep != other.rep) {
        return static_cast<int>(this->rep) < static_cast<int>(other.rep);
      }
      if (this->kind != other.kind) {
        return this->kind < other.kind;
      }
      return this->index < other.index;
    }

    bool operator==(const Key& other) const {
      return this->is_constant == other.is_constant && this->rep == other.rep &&
             this->kind == other.kind && this->index == other.index;
    }
  };

  // Internally, the state is a normalized permutation of (kind,index) pairs.
  typedef Key Value;
  typedef std::map<Key, Value> OperandMap;

  Value read(const InstructionOperand& op) const {
    OperandMap::const_iterator it = values_.find(KeyFor(op));
    return (it == values_.end()) ? ValueFor(op) : it->second;
  }

  void write(const InstructionOperand& op, Value v) {
    if (v == ValueFor(op)) {
      values_.erase(KeyFor(op));
    } else {
      values_[KeyFor(op)] = v;
    }
  }

  static Key KeyFor(const InstructionOperand& op) {
    bool is_constant = op.IsConstant();
    MachineRepresentation rep =
        v8::internal::compiler::InstructionSequence::DefaultRepresentation();
    LocationOperand::LocationKind kind;
    int index;
    if (!is_constant) {
      const LocationOperand& loc_op = LocationOperand::cast(op);
      if (loc_op.IsAnyRegister()) {
        if (loc_op.IsFPRegister()) {
          rep = MachineRepresentation::kFloat64;
        }
        index = loc_op.register_code();
      } else {
        index = loc_op.index();
      }
      kind = loc_op.location_kind();
    } else {
      index = ConstantOperand::cast(op).virtual_register();
      kind = LocationOperand::REGISTER;
    }
    Key key = {is_constant, rep, kind, index};
    return key;
  }

  static Value ValueFor(const InstructionOperand& op) { return KeyFor(op); }

  static InstructionOperand FromKey(Key key) {
    if (key.is_constant) {
      return ConstantOperand(key.index);
    }
    return AllocatedOperand(key.kind, key.rep, key.index);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const InterpreterState& is) {
    for (OperandMap::const_iterator it = is.values_.begin();
         it != is.values_.end(); ++it) {
      if (it != is.values_.begin()) os << " ";
      InstructionOperand source = FromKey(it->second);
      InstructionOperand destination = FromKey(it->first);
      MoveOperands mo(source, destination);
      PrintableMoveOperands pmo = {RegisterConfiguration::Turbofan(), &mo};
      os << pmo;
    }
    return os;
  }

  OperandMap values_;
};


// An abstract interpreter for moves, swaps and parallel moves.
class MoveInterpreter : public GapResolver::Assembler {
 public:
  explicit MoveInterpreter(Zone* zone) : zone_(zone) {}

  void AssembleMove(InstructionOperand* source,
                    InstructionOperand* destination) override {
    ParallelMove* moves = new (zone_) ParallelMove(zone_);
    moves->AddMove(*source, *destination);
    state_.ExecuteInParallel(moves);
  }

  void AssembleSwap(InstructionOperand* source,
                    InstructionOperand* destination) override {
    ParallelMove* moves = new (zone_) ParallelMove(zone_);
    moves->AddMove(*source, *destination);
    moves->AddMove(*destination, *source);
    state_.ExecuteInParallel(moves);
  }

  void AssembleParallelMove(const ParallelMove* moves) {
    state_.ExecuteInParallel(moves);
  }

  InterpreterState state() const { return state_; }

 private:
  Zone* const zone_;
  InterpreterState state_;
};


class ParallelMoveCreator : public HandleAndZoneScope {
 public:
  ParallelMoveCreator() : rng_(CcTest::random_number_generator()) {}

  ParallelMove* Create(int size) {
    ParallelMove* parallel_move = new (main_zone()) ParallelMove(main_zone());
    std::set<InstructionOperand, CompareOperandModuloType> seen;
    for (int i = 0; i < size; ++i) {
      MachineRepresentation rep = RandomRepresentation();
      MoveOperands mo(CreateRandomOperand(true, rep),
                      CreateRandomOperand(false, rep));
      if (!mo.IsRedundant() && seen.find(mo.destination()) == seen.end()) {
        parallel_move->AddMove(mo.source(), mo.destination());
        seen.insert(mo.destination());
      }
    }
    return parallel_move;
  }

 private:
  MachineRepresentation RandomRepresentation() {
    int index = rng_->NextInt(5);
    switch (index) {
      case 0:
        return MachineRepresentation::kWord32;
      case 1:
        return MachineRepresentation::kWord64;
      case 2:
        return MachineRepresentation::kFloat32;
      case 3:
        return MachineRepresentation::kFloat64;
      case 4:
        return MachineRepresentation::kTagged;
    }
    UNREACHABLE();
    return MachineRepresentation::kNone;
  }

  InstructionOperand CreateRandomOperand(bool is_source,
                                         MachineRepresentation rep) {
    auto conf = RegisterConfiguration::Turbofan();
    auto GetRegisterCode = [&conf](MachineRepresentation rep, int index) {
      switch (rep) {
        case MachineRepresentation::kFloat32:
#if V8_TARGET_ARCH_ARM
          // Only even number float registers are used on Arm.
          // TODO(bbudge) Eliminate this when FP register aliasing works.
          return conf->RegisterConfiguration::GetAllocatableDoubleCode(index) *
                 2;
#endif
        // Fall through on non-Arm targets.
        case MachineRepresentation::kFloat64:
          return conf->RegisterConfiguration::GetAllocatableDoubleCode(index);

        default:
          return conf->RegisterConfiguration::GetAllocatableGeneralCode(index);
      }
      UNREACHABLE();
      return static_cast<int>(Register::kCode_no_reg);
    };
    int index = rng_->NextInt(7);
    // destination can't be Constant.
    switch (rng_->NextInt(is_source ? 5 : 4)) {
      case 0:
        return AllocatedOperand(LocationOperand::STACK_SLOT, rep, index);
      case 1:
        return AllocatedOperand(LocationOperand::REGISTER, rep, index);
      case 2:
        return ExplicitOperand(LocationOperand::REGISTER, rep,
                               GetRegisterCode(rep, 1));
      case 3:
        return ExplicitOperand(LocationOperand::STACK_SLOT, rep,
                               GetRegisterCode(rep, index));
      case 4:
        return ConstantOperand(index);
    }
    UNREACHABLE();
    return InstructionOperand();
  }

 private:
  v8::base::RandomNumberGenerator* rng_;
};


TEST(FuzzResolver) {
  ParallelMoveCreator pmc;
  for (int size = 0; size < 20; ++size) {
    for (int repeat = 0; repeat < 50; ++repeat) {
      ParallelMove* pm = pmc.Create(size);

      // Note: The gap resolver modifies the ParallelMove, so interpret first.
      MoveInterpreter mi1(pmc.main_zone());
      mi1.AssembleParallelMove(pm);

      MoveInterpreter mi2(pmc.main_zone());
      GapResolver resolver(&mi2);
      resolver.Resolve(pm);

      CHECK_EQ(mi1.state(), mi2.state());
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
