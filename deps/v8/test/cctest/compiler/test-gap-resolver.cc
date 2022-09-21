// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/gap-resolver.h"

#include "src/base/utils/random-number-generator.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

const auto GetRegConfig = RegisterConfiguration::Default;

// Fragments the given FP operand into an equivalent set of FP operands to
// simplify ParallelMove equivalence testing.
void GetCanonicalOperands(const InstructionOperand& op,
                          std::vector<InstructionOperand>* fragments) {
  CHECK_EQ(kFPAliasing, AliasingKind::kCombine);
  CHECK(op.IsFPLocationOperand());
  const LocationOperand& loc = LocationOperand::cast(op);
  MachineRepresentation rep = loc.representation();
  int base = -1;
  int aliases = GetRegConfig()->GetAliases(
      rep, 0, MachineRepresentation::kFloat32, &base);
  CHECK_LT(0, aliases);
  CHECK_GE(4, aliases);
  int index = -1;
  int step = 1;
  if (op.IsFPRegister()) {
    index = loc.register_code() * aliases;
  } else {
    index = loc.index();
    step = -1;
  }
  for (int i = 0; i < aliases; i++) {
    fragments->push_back(AllocatedOperand(loc.location_kind(),
                                          MachineRepresentation::kFloat32,
                                          index + i * step));
  }
}

// The state of our move interpreter is the mapping of operands to values. Note
// that the actual values don't really matter, all we care about is equality.
class InterpreterState {
 public:
  void ExecuteInParallel(const ParallelMove* moves) {
    InterpreterState copy(*this);
    for (const auto m : *moves) {
      CHECK(!m->IsRedundant());
      const InstructionOperand& src = m->source();
      const InstructionOperand& dst = m->destination();
      if (kFPAliasing == AliasingKind::kCombine && src.IsFPLocationOperand() &&
          dst.IsFPLocationOperand()) {
        // Canonicalize FP location-location moves by fragmenting them into
        // an equivalent sequence of float32 moves, to simplify state
        // equivalence testing.
        std::vector<InstructionOperand> src_fragments;
        GetCanonicalOperands(src, &src_fragments);
        CHECK(!src_fragments.empty());
        std::vector<InstructionOperand> dst_fragments;
        GetCanonicalOperands(dst, &dst_fragments);
        CHECK_EQ(src_fragments.size(), dst_fragments.size());

        for (size_t i = 0; i < src_fragments.size(); ++i) {
          write(dst_fragments[i], copy.read(src_fragments[i]));
        }
        continue;
      }
      // All other moves.
      write(dst, copy.read(src));
    }
  }

  void MoveToTempLocation(InstructionOperand& source) {
    scratch_ = KeyFor(source);
  }

  void MoveFromTempLocation(InstructionOperand& dst) {
    AllocatedOperand src(scratch_.kind, scratch_.rep, scratch_.index);
    if (kFPAliasing == AliasingKind::kCombine && src.IsFPLocationOperand() &&
        dst.IsFPLocationOperand()) {
      // Canonicalize FP location-location moves by fragmenting them into
      // an equivalent sequence of float32 moves, to simplify state
      // equivalence testing.
      std::vector<InstructionOperand> src_fragments;
      GetCanonicalOperands(src, &src_fragments);
      CHECK(!src_fragments.empty());
      std::vector<InstructionOperand> dst_fragments;
      GetCanonicalOperands(dst, &dst_fragments);
      CHECK_EQ(src_fragments.size(), dst_fragments.size());

      for (size_t i = 0; i < src_fragments.size(); ++i) {
        write(dst_fragments[i], KeyFor(src_fragments[i]));
      }
      return;
    }
    write(dst, scratch_);
  }

  bool operator==(const InterpreterState& other) const {
    return values_ == other.values_;
  }

 private:
  // struct for mapping operands to a unique value, that makes it easier to
  // detect illegal parallel moves, and to evaluate moves for equivalence. This
  // is a one way transformation. All general register and slot operands are
  // mapped to the default representation. FP registers and slots are mapped to
  // float64 except on architectures with non-simple FP register aliasing, where
  // the actual representation is used.
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
        return this->rep < other.rep;
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

  // Internally, the state is a normalized permutation of Value pairs.
  using Value = Key;
  using OperandMap = std::map<Key, Value>;

  Value read(const InstructionOperand& op) const {
    OperandMap::const_iterator it = values_.find(KeyFor(op));
    return (it == values_.end()) ? ValueFor(op) : it->second;
  }

  void write(const InstructionOperand& dst, Value v) {
    if (v == ValueFor(dst)) {
      values_.erase(KeyFor(dst));
    } else {
      values_[KeyFor(dst)] = v;
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
      // Preserve FP representation when FP register aliasing is complex.
      // Otherwise, canonicalize to kFloat64.
      if (IsFloatingPoint(loc_op.representation())) {
        if (kFPAliasing == AliasingKind::kIndependent) {
          rep = IsSimd128(loc_op.representation())
                    ? MachineRepresentation::kSimd128
                    : MachineRepresentation::kFloat64;
        } else if (kFPAliasing == AliasingKind::kOverlap) {
          rep = MachineRepresentation::kFloat64;
        } else {
          rep = loc_op.representation();
        }
      }
      if (loc_op.IsAnyRegister()) {
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
    const char* space = "";
    for (auto& value : is.values_) {
      InstructionOperand source = FromKey(value.second);
      InstructionOperand destination = FromKey(value.first);
      os << space << MoveOperands{source, destination};
      space = " ";
    }
    return os;
  }

  OperandMap values_;
  Key scratch_ = {};
};

// An abstract interpreter for moves, swaps and parallel moves.
class MoveInterpreter : public GapResolver::Assembler {
 public:
  explicit MoveInterpreter(Zone* zone) : zone_(zone) {}

  void MoveToTempLocation(InstructionOperand* source) final {
    state_.MoveToTempLocation(*source);
  }
  void MoveTempLocationTo(InstructionOperand* dest,
                          MachineRepresentation rep) final {
    state_.MoveFromTempLocation(*dest);
  }
  void SetPendingMove(MoveOperands* move) final {}
  void AssembleMove(InstructionOperand* source,
                    InstructionOperand* destination) override {
    ParallelMove* moves = zone_->New<ParallelMove>(zone_);
    moves->AddMove(*source, *destination);
    state_.ExecuteInParallel(moves);
  }
  void AssembleSwap(InstructionOperand* source,
                    InstructionOperand* destination) override {
    ParallelMove* moves = zone_->New<ParallelMove>(zone_);
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

  // Creates a ParallelMove with 'size' random MoveOperands. Note that illegal
  // moves will be rejected, so the actual number of MoveOperands may be less.
  ParallelMove* Create(int size) {
    ParallelMove* parallel_move = main_zone()->New<ParallelMove>(main_zone());
    // Valid ParallelMoves can't have interfering destination ops.
    std::set<InstructionOperand, CompareOperandModuloType> destinations;
    // Valid ParallelMoves can't have interfering source ops of different reps.
    std::map<InstructionOperand, MachineRepresentation,
             CompareOperandModuloType>
        sources;
    for (int i = 0; i < size; ++i) {
      MachineRepresentation rep = RandomRepresentation();
      MoveOperands mo(CreateRandomOperand(true, rep),
                      CreateRandomOperand(false, rep));
      if (mo.IsRedundant()) continue;

      const InstructionOperand& dst = mo.destination();
      bool reject = false;
      // On architectures where FP register aliasing is non-simple, update the
      // destinations set with the float equivalents of the operand and check
      // that all destinations are unique and do not alias each other.
      if (kFPAliasing == AliasingKind::kCombine &&
          mo.destination().IsFPLocationOperand()) {
        std::vector<InstructionOperand> dst_fragments;
        GetCanonicalOperands(dst, &dst_fragments);
        CHECK(!dst_fragments.empty());
        for (size_t j = 0; j < dst_fragments.size(); ++j) {
          if (destinations.find(dst_fragments[j]) == destinations.end()) {
            destinations.insert(dst_fragments[j]);
          } else {
            reject = true;
            break;
          }
        }
        // Update the sources map, and check that no FP source has multiple
        // representations.
        const InstructionOperand& src = mo.source();
        if (src.IsFPRegister()) {
          std::vector<InstructionOperand> src_fragments;
          MachineRepresentation src_rep =
              LocationOperand::cast(src).representation();
          GetCanonicalOperands(src, &src_fragments);
          CHECK(!src_fragments.empty());
          for (size_t j = 0; j < src_fragments.size(); ++j) {
            auto find_it = sources.find(src_fragments[j]);
            if (find_it != sources.end() && find_it->second != src_rep) {
              reject = true;
              break;
            }
            sources.insert(std::make_pair(src_fragments[j], src_rep));
          }
        }
      } else {
        if (destinations.find(dst) == destinations.end()) {
          destinations.insert(dst);
        } else {
          reject = true;
        }
      }

      if (!reject) {
        parallel_move->AddMove(mo.source(), mo.destination());
      }
    }
    return parallel_move;
  }

  // Creates a ParallelMove from a list of operand pairs. Even operands are
  // destinations, odd ones are sources.
  ParallelMove* Create(const std::vector<InstructionOperand>& operand_pairs) {
    ParallelMove* parallel_move = main_zone()->New<ParallelMove>(main_zone());
    for (size_t i = 0; i < operand_pairs.size(); i += 2) {
      const InstructionOperand& dst = operand_pairs[i];
      const InstructionOperand& src = operand_pairs[i + 1];
      parallel_move->AddMove(src, dst);
    }
    return parallel_move;
  }

 private:
  MachineRepresentation RandomRepresentation() {
    int index = rng_->NextInt(6);
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
        return MachineRepresentation::kSimd128;
      case 5:
        return MachineRepresentation::kTagged;
    }
    UNREACHABLE();
  }

  // min(num_alloctable_general_registers for each arch) == 5 from
  // assembler-ia32.h
  const int kMaxIndex = 5;
  const int kMaxIndices = kMaxIndex + 1;

  // Non-FP slots shouldn't overlap FP slots.
  // FP slots with different representations shouldn't overlap.
  int GetValidSlotIndex(MachineRepresentation rep, int index) {
    DCHECK_GE(kMaxIndex, index);
    // The first group of slots are for non-FP values.
    if (!IsFloatingPoint(rep)) return index;
    // The next group are for float values.
    int base = kMaxIndices;
    if (rep == MachineRepresentation::kFloat32) return base + index;
    // Double values.
    base += kMaxIndices;
    if (rep == MachineRepresentation::kFloat64) return base + index * 2;
    // SIMD values
    base += kMaxIndices * 2;
    CHECK_EQ(MachineRepresentation::kSimd128, rep);
    return base + index * 4;
  }

  InstructionOperand CreateRandomOperand(bool is_source,
                                         MachineRepresentation rep) {
    auto conf = RegisterConfiguration::Default();
    auto GetValidRegisterCode = [&conf](MachineRepresentation rep, int index) {
      switch (rep) {
        case MachineRepresentation::kFloat32:
          return conf->RegisterConfiguration::GetAllocatableFloatCode(index);
        case MachineRepresentation::kFloat64:
          return conf->RegisterConfiguration::GetAllocatableDoubleCode(index);
        case MachineRepresentation::kSimd128:
          return conf->RegisterConfiguration::GetAllocatableSimd128Code(index);
        default:
          return conf->RegisterConfiguration::GetAllocatableGeneralCode(index);
      }
      UNREACHABLE();
    };
    int index = rng_->NextInt(kMaxIndex);
    // destination can't be Constant.
    switch (rng_->NextInt(is_source ? 3 : 2)) {
      case 0:
        return AllocatedOperand(LocationOperand::STACK_SLOT, rep,
                                GetValidSlotIndex(rep, index));
      case 1:
        return AllocatedOperand(LocationOperand::REGISTER, rep,
                                GetValidRegisterCode(rep, index));
      case 2:
        return ConstantOperand(index);
    }
    UNREACHABLE();
  }

 private:
  v8::base::RandomNumberGenerator* rng_;
};

void RunTest(ParallelMove* pm, Zone* zone) {
  // Note: The gap resolver modifies the ParallelMove, so interpret first.
  MoveInterpreter mi1(zone);
  mi1.AssembleParallelMove(pm);

  MoveInterpreter mi2(zone);
  GapResolver resolver(&mi2);
  resolver.Resolve(pm);

  CHECK_EQ(mi1.state(), mi2.state());
}

TEST(Aliasing) {
  // On platforms with simple aliasing, these parallel moves are ill-formed.
  if (kFPAliasing != AliasingKind::kCombine) return;

  ParallelMoveCreator pmc;
  Zone* zone = pmc.main_zone();

  auto s0 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat32, 0);
  auto s1 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat32, 1);
  auto s2 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat32, 2);
  auto s3 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat32, 3);
  auto s4 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat32, 4);

  auto d0 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat64, 0);
  auto d1 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kFloat64, 1);
  auto d16 = AllocatedOperand(LocationOperand::REGISTER,
                              MachineRepresentation::kFloat64, 16);

  // Double slots must be odd to match frame allocation.
  auto dSlot = AllocatedOperand(LocationOperand::STACK_SLOT,
                                MachineRepresentation::kFloat64, 3);

  // Cycles involving s- and d-registers.
  {
    std::vector<InstructionOperand> moves = {
        s2, s0,  // s2 <- s0
        d0, d1   // d0 <- d1
    };
    RunTest(pmc.Create(moves), zone);
  }
  {
    std::vector<InstructionOperand> moves = {
        d0, d1,  // d0 <- d1
        s2, s0   // s2 <- s0
    };
    RunTest(pmc.Create(moves), zone);
  }
  {
    std::vector<InstructionOperand> moves = {
        s2, s1,  // s2 <- s1
        d0, d1   // d0 <- d1
    };
    RunTest(pmc.Create(moves), zone);
  }
  {
    std::vector<InstructionOperand> moves = {
        d0, d1,  // d0 <- d1
        s2, s1   // s2 <- s1
    };
    RunTest(pmc.Create(moves), zone);
  }
  // Two cycles involving a single d-register.
  {
    std::vector<InstructionOperand> moves = {
        d0, d1,  // d0 <- d1
        s2, s1,  // s2 <- s1
        s3, s0   // s3 <- s0
    };
    RunTest(pmc.Create(moves), zone);
  }
  // Cycle with a float move that must be deferred until after swaps.
  {
    std::vector<InstructionOperand> moves = {
        d0, d1,  // d0 <- d1
        s2, s0,  // s2 <- s0
        s3, s4   // s3 <- s4  must be deferred
    };
    RunTest(pmc.Create(moves), zone);
  }
  // Cycles involving s-registers and a non-aliased d-register.
  {
    std::vector<InstructionOperand> moves = {
        d16, d0,  // d16 <- d0
        s1,  s2,  // s1 <- s2
        d1,  d16  // d1 <- d16
    };
    RunTest(pmc.Create(moves), zone);
  }
  {
    std::vector<InstructionOperand> moves = {
        s2,  s1,   // s1 <- s2
        d0,  d16,  // d16 <- d0
        d16, d1    // d1 <- d16
    };
    RunTest(pmc.Create(moves), zone);
  }
  {
    std::vector<InstructionOperand> moves = {
        d0,  d16,  // d0 <- d16
        d16, d1,   // s2 <- s0
        s3,  s0    // d0 <- d1
    };
    RunTest(pmc.Create(moves), zone);
  }
  // Cycle involving aliasing registers and a slot.
  {
    std::vector<InstructionOperand> moves = {
        dSlot, d0,     // dSlot <- d0
        d1,    dSlot,  // d1 <- dSlot
        s0,    s3      // s0 <- s3
    };
    RunTest(pmc.Create(moves), zone);
  }
}

TEST(FuzzResolver) {
  ParallelMoveCreator pmc;
  for (int size = 0; size < 80; ++size) {
    for (int repeat = 0; repeat < 50; ++repeat) {
      RunTest(pmc.Create(size), pmc.main_zone());
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
