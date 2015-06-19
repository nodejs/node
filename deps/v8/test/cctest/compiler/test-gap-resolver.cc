// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/gap-resolver.h"

#include "src/base/utils/random-number-generator.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

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
    AllocatedOperand::AllocatedKind kind;
    int index;

    bool operator<(const Key& other) const {
      if (this->is_constant != other.is_constant) {
        return this->is_constant;
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
    AllocatedOperand::AllocatedKind kind;
    int index;
    if (!is_constant) {
      index = AllocatedOperand::cast(op).index();
      kind = AllocatedOperand::cast(op).allocated_kind();
    } else {
      index = ConstantOperand::cast(op).virtual_register();
      kind = AllocatedOperand::REGISTER;
    }
    Key key = {is_constant, kind, index};
    return key;
  }

  static Value ValueFor(const InstructionOperand& op) { return KeyFor(op); }

  static InstructionOperand FromKey(Key key) {
    if (key.is_constant) {
      return ConstantOperand(key.index);
    }
    return AllocatedOperand(
        key.kind, InstructionSequence::DefaultRepresentation(), key.index);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const InterpreterState& is) {
    for (OperandMap::const_iterator it = is.values_.begin();
         it != is.values_.end(); ++it) {
      if (it != is.values_.begin()) os << " ";
      InstructionOperand source = FromKey(it->first);
      InstructionOperand destination = FromKey(it->second);
      MoveOperands mo(source, destination);
      PrintableMoveOperands pmo = {RegisterConfiguration::ArchDefault(), &mo};
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

  virtual void AssembleMove(InstructionOperand* source,
                            InstructionOperand* destination) override {
    ParallelMove* moves = new (zone_) ParallelMove(zone_);
    moves->AddMove(*source, *destination);
    state_.ExecuteInParallel(moves);
  }

  virtual void AssembleSwap(InstructionOperand* source,
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
  MachineType RandomType() {
    int index = rng_->NextInt(3);
    switch (index) {
      case 0:
        return kRepWord32;
      case 1:
        return kRepWord64;
      case 2:
        return kRepTagged;
    }
    UNREACHABLE();
    return kMachNone;
  }

  MachineType RandomDoubleType() {
    int index = rng_->NextInt(2);
    if (index == 0) return kRepFloat64;
    return kRepFloat32;
  }

  InstructionOperand CreateRandomOperand(bool is_source) {
    int index = rng_->NextInt(6);
    // destination can't be Constant.
    switch (rng_->NextInt(is_source ? 5 : 4)) {
      case 0:
        return StackSlotOperand(RandomType(), index);
      case 1:
        return DoubleStackSlotOperand(RandomDoubleType(), index);
      case 2:
        return RegisterOperand(RandomType(), index);
      case 3:
        return DoubleRegisterOperand(RandomDoubleType(), index);
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

      CHECK(mi1.state() == mi2.state());
    }
  }
}
