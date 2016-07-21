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
    bool is_float;
    LocationOperand::LocationKind kind;
    int index;

    bool operator<(const Key& other) const {
      if (this->is_constant != other.is_constant) {
        return this->is_constant;
      }
      if (this->is_float != other.is_float) {
        return this->is_float;
      }
      if (this->kind != other.kind) {
        return this->kind < other.kind;
      }
      return this->index < other.index;
    }

    bool operator==(const Key& other) const {
      return this->is_constant == other.is_constant &&
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
    bool is_float = false;
    LocationOperand::LocationKind kind;
    int index;
    if (!is_constant) {
      if (op.IsRegister()) {
        index = LocationOperand::cast(op).GetRegister().code();
      } else if (op.IsFPRegister()) {
        index = LocationOperand::cast(op).GetDoubleRegister().code();
      } else {
        index = LocationOperand::cast(op).index();
      }
      is_float = IsFloatingPoint(LocationOperand::cast(op).representation());
      kind = LocationOperand::cast(op).location_kind();
    } else {
      index = ConstantOperand::cast(op).virtual_register();
      kind = LocationOperand::REGISTER;
    }
    Key key = {is_constant, is_float, kind, index};
    return key;
  }

  static Value ValueFor(const InstructionOperand& op) { return KeyFor(op); }

  static InstructionOperand FromKey(Key key) {
    if (key.is_constant) {
      return ConstantOperand(key.index);
    }
    return AllocatedOperand(
        key.kind,
        v8::internal::compiler::InstructionSequence::DefaultRepresentation(),
        key.index);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const InterpreterState& is) {
    for (OperandMap::const_iterator it = is.values_.begin();
         it != is.values_.end(); ++it) {
      if (it != is.values_.begin()) os << " ";
      InstructionOperand source = FromKey(it->first);
      InstructionOperand destination = FromKey(it->second);
      MoveOperands mo(source, destination);
      PrintableMoveOperands pmo = {
          RegisterConfiguration::ArchDefault(RegisterConfiguration::TURBOFAN),
          &mo};
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
      MoveOperands mo(CreateRandomOperand(true), CreateRandomOperand(false));
      if (!mo.IsRedundant() && seen.find(mo.destination()) == seen.end()) {
        parallel_move->AddMove(mo.source(), mo.destination());
        seen.insert(mo.destination());
      }
    }
    return parallel_move;
  }

 private:
  MachineRepresentation RandomRepresentation() {
    int index = rng_->NextInt(3);
    switch (index) {
      case 0:
        return MachineRepresentation::kWord32;
      case 1:
        return MachineRepresentation::kWord64;
      case 2:
        return MachineRepresentation::kTagged;
    }
    UNREACHABLE();
    return MachineRepresentation::kNone;
  }

  MachineRepresentation RandomDoubleRepresentation() {
    int index = rng_->NextInt(2);
    if (index == 0) return MachineRepresentation::kFloat64;
    return MachineRepresentation::kFloat32;
  }

  InstructionOperand CreateRandomOperand(bool is_source) {
    int index = rng_->NextInt(7);
    // destination can't be Constant.
    switch (rng_->NextInt(is_source ? 7 : 6)) {
      case 0:
        return AllocatedOperand(LocationOperand::STACK_SLOT,
                                RandomRepresentation(), index);
      case 1:
        return AllocatedOperand(LocationOperand::STACK_SLOT,
                                RandomDoubleRepresentation(), index);
      case 2:
        return AllocatedOperand(LocationOperand::REGISTER,
                                RandomRepresentation(), index);
      case 3:
        return AllocatedOperand(LocationOperand::REGISTER,
                                RandomDoubleRepresentation(), index);
      case 4:
        return ExplicitOperand(
            LocationOperand::REGISTER, RandomRepresentation(),
            RegisterConfiguration::ArchDefault(RegisterConfiguration::TURBOFAN)
                ->GetAllocatableGeneralCode(1));
      case 5:
        return ExplicitOperand(
            LocationOperand::STACK_SLOT, RandomRepresentation(),
            RegisterConfiguration::ArchDefault(RegisterConfiguration::TURBOFAN)
                ->GetAllocatableGeneralCode(index));
      case 6:
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

      CHECK(mi1.state() == mi2.state());
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
