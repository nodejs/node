// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
#define V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_

#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/small-vector.h"
#include "src/base/template-utils.h"
#include "src/codegen/callable.h"
#include "src/codegen/reloc-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operation-matching.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/optimization-phase.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/snapshot-table.h"

namespace v8::internal {
enum class Builtin : int32_t;
}

namespace v8::internal::compiler::turboshaft {

namespace detail {
template <typename A, typename Vars, typename Vals, size_t... indices>
void SetVariablesHelper(A& assembler, Vars& vars, const Vals& vals,
                        std::index_sequence<indices...>) {
  static_assert(std::tuple_size_v<Vars> == std::tuple_size_v<Vals>);

  (void)std::initializer_list<int>{
      (assembler.Set(std::get<indices>(vars),
                     assembler.resolve(std::get<indices>(vals))),
       0)...};
}

template <typename A, typename Vars, typename Vals, size_t... indices>
void GetVariablesHelper(A& assembler, Vars& vars, Vals& vals,
                        std::index_sequence<indices...>) {
  static_assert(std::tuple_size_v<Vars> == std::tuple_size_v<Vals>);

  (void)std::initializer_list<int>{
      ((vals = assembler.Get(std::get<indices>(vars))), 0)...};
}

template <typename Rep, typename = void>
struct has_constexpr_type : std::false_type {};

template <typename Rep>
struct has_constexpr_type<Rep, std::void_t<typename Rep::constexpr_type>>
    : std::true_type {};

template <typename Rep, typename...>
struct make_const_or_v {
  using type = V<Rep>;
};

template <typename Rep>
struct make_const_or_v<
    Rep, typename std::enable_if_t<has_constexpr_type<Rep>::value>> {
  using type = ConstOrV<Rep>;
};

template <typename Rep>
struct make_const_or_v<
    Rep, typename std::enable_if_t<!has_constexpr_type<Rep>::value>> {
  using type = V<Rep>;
};

template <typename Rep>
using make_const_or_v_t = typename make_const_or_v<Rep, void>::type;
}  // namespace detail

template <typename... Reps>
class Label {
  using variables_t = std::tuple<std::conditional_t<true, Variable, Reps>...>;
  using values_t = std::tuple<V<Reps>...>;
  static constexpr size_t size = sizeof...(Reps);

 public:
  using const_or_values_t = std::tuple<detail::make_const_or_v_t<Reps>...>;

  template <typename Reducer>
  explicit Label(Reducer* reducer)
      : Label(reducer->Asm().NewBlock(),
              std::tuple{NewFreshVariable<Reps>(reducer->Asm())...}) {}

  Block* block() { return block_; }

  template <typename A>
  void Set(A& assembler, const const_or_values_t& values) {
    detail::SetVariablesHelper(assembler, vars_, values,
                               std::make_index_sequence<size>());
  }

  template <size_t I, typename A>
  OpIndex Get(A& assembler) {
    return assembler.Get(std::get<I>(vars_));
  }

  template <typename A>
  auto GetAll(A& assembler) {
    values_t values;
    detail::GetVariablesHelper(assembler, vars_, values,
                               std::make_index_sequence<size>());
    // C++17 does not allow structured binding of empty tuples, so we pass
    // a dummy nullptr value here in order to prevent empty bindings.
    return std::tuple_cat(std::tuple<std::nullptr_t>{}, values);
  }

 private:
  Label(Block* block, variables_t vars)
      : block_(block), vars_(std::move(vars)) {}

  template <typename T, typename A, RegisterRepresentation::Enum rep = T::Rep>
  static Variable NewFreshVariable(A& assembler) {
    return assembler.NewFreshVariable(RegisterRepresentation{rep});
  }
  template <typename T, typename A>
  static Variable NewFreshVaribale(A& assembler) {
    return assembler.NewFreshVariable();
  }

  Block* block_;
  variables_t vars_;
};

Handle<Code> BuiltinCodeHandle(Builtin builtin, Isolate* isolate);

// Forward declarations
template <class Assembler>
class GraphVisitor;
using Variable =
    SnapshotTable<OpIndex, base::Optional<RegisterRepresentation>>::Key;

template <class Assembler, template <class> class... Reducers>
class ReducerStack {};

template <class Assembler, template <class> class FirstReducer,
          template <class> class... Reducers>
class ReducerStack<Assembler, FirstReducer, Reducers...>
    : public FirstReducer<ReducerStack<Assembler, Reducers...>> {
 public:
  using FirstReducer<ReducerStack<Assembler, Reducers...>>::FirstReducer;
};

template <class Reducers>
class ReducerStack<Assembler<Reducers>> {
 public:
  using AssemblerType = Assembler<Reducers>;
  using ReducerList = Reducers;
  Assembler<ReducerList>& Asm() {
    return *static_cast<Assembler<ReducerList>*>(this);
  }
};

template <class Reducers>
struct reducer_stack_type {};
template <template <class> class... Reducers>
struct reducer_stack_type<reducer_list<Reducers...>> {
  using type = ReducerStack<Assembler<reducer_list<Reducers...>>, Reducers...,
                            v8::internal::compiler::turboshaft::ReducerBase>;
};

template <typename Next>
class ReducerBase;

#define TURBOSHAFT_REDUCER_BOILERPLATE()                               \
  Assembler<typename Next::ReducerList>& Asm() {                       \
    return *static_cast<Assembler<typename Next::ReducerList>*>(this); \
  }

// LABEL_BLOCK is used in Reducers to have a single call forwarding to the next
// reducer without change. A typical use would be:
//
//     OpIndex ReduceFoo(OpIndex arg) {
//       LABEL_BLOCK(no_change) return Next::ReduceFoo(arg);
//       ...
//       if (...) goto no_change;
//       ...
//       if (...) goto no_change;
//       ...
//     }
#define LABEL_BLOCK(label)     \
  for (; false; UNREACHABLE()) \
  label:

// This empty base-class is used to provide default-implementations of plain
// methods emitting operations.
template <class Next>
class ReducerBaseForwarder : public Next {
 public:
#define EMIT_OP(Name)                                                    \
  OpIndex ReduceInputGraph##Name(OpIndex ig_index, const Name##Op& op) { \
    return this->Asm().AssembleOutputGraph##Name(op);                    \
  }                                                                      \
  template <class... Args>                                               \
  OpIndex Reduce##Name(Args... args) {                                   \
    return this->Asm().template Emit<Name##Op>(args...);                 \
  }
  TURBOSHAFT_OPERATION_LIST(EMIT_OP)
#undef EMIT_OP
};

// ReducerBase provides default implementations of Branch-related Operations
// (Goto, Branch, Switch, CallAndCatchException), and takes care of updating
// Block predecessors (and calls the Assembler to maintain split-edge form).
// ReducerBase is always added by Assembler at the bottom of the reducer stack.
template <class Next>
class ReducerBase : public ReducerBaseForwarder<Next> {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  using Base = ReducerBaseForwarder<Next>;
  using ArgT = std::tuple<>;

  template <class... Args>
  explicit ReducerBase(const std::tuple<Args...>&) {}

  void Bind(Block*, const Block*) {}

  void Analyze() {}

#ifdef DEBUG
  void Verify(OpIndex old_index, OpIndex new_index) {}
#endif  // DEBUG

  void RemoveLast(OpIndex index_of_last_operation) {
    Asm().output_graph().RemoveLast();
  }

  // Get, GetPredecessorValue, Set and NewFreshVariable should be overwritten by
  // the VariableReducer. If the reducer stack has no VariableReducer, then
  // those methods should not be called.
  OpIndex Get(Variable) { UNREACHABLE(); }
  OpIndex GetPredecessorValue(Variable, int) { UNREACHABLE(); }
  void Set(Variable, OpIndex) { UNREACHABLE(); }
  Variable NewFreshVariable(base::Optional<RegisterRepresentation>) {
    UNREACHABLE();
  }

  OpIndex ReducePhi(base::Vector<const OpIndex> inputs,
                    RegisterRepresentation rep) {
    DCHECK(Asm().current_block()->IsMerge() &&
           inputs.size() == Asm().current_block()->Predecessors().size());
    return Base::ReducePhi(inputs, rep);
  }

  template <class... Args>
  OpIndex ReducePendingLoopPhi(Args... args) {
    DCHECK(Asm().current_block()->IsLoop());
    return Base::ReducePendingLoopPhi(args...);
  }

  OpIndex ReduceGoto(Block* destination) {
    // Calling Base::Goto will call Emit<Goto>, which will call FinalizeBlock,
    // which will reset {current_block_}. We thus save {current_block_} before
    // calling Base::Goto, as we'll need it for AddPredecessor. Note also that
    // AddPredecessor might introduce some new blocks/operations if it needs to
    // split an edge, which means that it has to run after Base::Goto
    // (otherwise, the current Goto could be inserted in the wrong block).
    Block* saved_current_block = Asm().current_block();
    OpIndex new_opindex = Base::ReduceGoto(destination);
    Asm().AddPredecessor(saved_current_block, destination, false);
    return new_opindex;
  }

  OpIndex ReduceBranch(OpIndex condition, Block* if_true, Block* if_false,
                       BranchHint hint) {
    // There should never be a good reason to generate a Branch where both the
    // {if_true} and {if_false} are the same Block. If we ever decide to lift
    // this condition, then AddPredecessor and SplitEdge should be updated
    // accordingly.
    DCHECK_NE(if_true, if_false);
    Block* saved_current_block = Asm().current_block();
    OpIndex new_opindex =
        Base::ReduceBranch(condition, if_true, if_false, hint);
    Asm().AddPredecessor(saved_current_block, if_true, true);
    Asm().AddPredecessor(saved_current_block, if_false, true);
    return new_opindex;
  }

  OpIndex ReduceCallAndCatchException(OpIndex callee, OpIndex frame_state,
                                      base::Vector<const OpIndex> arguments,
                                      Block* if_success, Block* if_exception,
                                      const TSCallDescriptor* descriptor) {
    // {if_success} and {if_exception} should never be the same.  If we ever
    // decide to lift this condition, then AddPredecessor and SplitEdge should
    // be updated accordingly.
    DCHECK_NE(if_success, if_exception);
    Block* saved_current_block = Asm().current_block();
    OpIndex new_opindex = Base::ReduceCallAndCatchException(
        callee, frame_state, arguments, if_success, if_exception, descriptor);
    Asm().AddPredecessor(saved_current_block, if_success, true);
    Asm().AddPredecessor(saved_current_block, if_exception, true);
    return new_opindex;
  }

  OpIndex ReduceSwitch(OpIndex input, base::Vector<const SwitchOp::Case> cases,
                       Block* default_case, BranchHint default_hint) {
#ifdef DEBUG
    // Making sure that all cases and {default_case} are different. If we ever
    // decide to lift this condition, then AddPredecessor and SplitEdge should
    // be updated accordingly.
    std::unordered_set<Block*> seen;
    seen.insert(default_case);
    for (auto switch_case : cases) {
      DCHECK_EQ(seen.count(switch_case.destination), 0);
      seen.insert(switch_case.destination);
    }
#endif
    Block* saved_current_block = Asm().current_block();
    OpIndex new_opindex =
        Base::ReduceSwitch(input, cases, default_case, default_hint);
    for (SwitchOp::Case c : cases) {
      Asm().AddPredecessor(saved_current_block, c.destination, true);
    }
    Asm().AddPredecessor(saved_current_block, default_case, true);
    return new_opindex;
  }
};

template <class Assembler>
class AssemblerOpInterface {
 public:
// Methods to be used by the reducers to reducer operations with the whole
// reducer stack.
#define DECL_MULTI_REP_BINOP(name, operation, rep_type, kind)            \
  OpIndex name(OpIndex left, OpIndex right, rep_type rep) {              \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {      \
      return OpIndex::Invalid();                                         \
    }                                                                    \
    return stack().Reduce##operation(left, right,                        \
                                     operation##Op::Kind::k##kind, rep); \
  }
#define DECL_SINGLE_REP_BINOP(name, operation, kind, rep)                \
  OpIndex name(OpIndex left, OpIndex right) {                            \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {      \
      return OpIndex::Invalid();                                         \
    }                                                                    \
    return stack().Reduce##operation(left, right,                        \
                                     operation##Op::Kind::k##kind, rep); \
  }
#define DECL_SINGLE_REP_BINOP_V(name, operation, kind, tag)                   \
  V<tag> name(ConstOrV<tag> left, ConstOrV<tag> right) {                      \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {           \
      return OpIndex::Invalid();                                              \
    }                                                                         \
    return stack().Reduce##operation(resolve(left), resolve(right),           \
                                     operation##Op::Kind::k##kind, tag::Rep); \
  }
#define DECL_SINGLE_REP_BINOP_NO_KIND(name, operation, rep)         \
  OpIndex name(OpIndex left, OpIndex right) {                       \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().Reduce##operation(left, right, rep);             \
  }
  DECL_MULTI_REP_BINOP(WordAdd, WordBinop, WordRepresentation, Add)
  DECL_SINGLE_REP_BINOP_V(Word32Add, WordBinop, Add, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64Add, WordBinop, Add, Word64)
  DECL_SINGLE_REP_BINOP(PointerAdd, WordBinop, Add,
                        WordRepresentation::PointerSized())

  DECL_MULTI_REP_BINOP(WordMul, WordBinop, WordRepresentation, Mul)
  DECL_SINGLE_REP_BINOP_V(Word32Mul, WordBinop, Mul, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64Mul, WordBinop, Mul, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseAnd, WordBinop, WordRepresentation,
                       BitwiseAnd)
  DECL_SINGLE_REP_BINOP_V(Word32BitwiseAnd, WordBinop, BitwiseAnd, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64BitwiseAnd, WordBinop, BitwiseAnd, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseOr, WordBinop, WordRepresentation, BitwiseOr)
  DECL_SINGLE_REP_BINOP_V(Word32BitwiseOr, WordBinop, BitwiseOr, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64BitwiseOr, WordBinop, BitwiseOr, Word64)

  DECL_MULTI_REP_BINOP(WordBitwiseXor, WordBinop, WordRepresentation,
                       BitwiseXor)
  DECL_SINGLE_REP_BINOP_V(Word32BitwiseXor, WordBinop, BitwiseXor, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64BitwiseXor, WordBinop, BitwiseXor, Word64)

  DECL_MULTI_REP_BINOP(WordSub, WordBinop, WordRepresentation, Sub)
  DECL_SINGLE_REP_BINOP_V(Word32Sub, WordBinop, Sub, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64Sub, WordBinop, Sub, Word64)
  DECL_SINGLE_REP_BINOP(PointerSub, WordBinop, Sub,
                        WordRepresentation::PointerSized())

  DECL_MULTI_REP_BINOP(IntDiv, WordBinop, WordRepresentation, SignedDiv)
  DECL_SINGLE_REP_BINOP_V(Int32Div, WordBinop, SignedDiv, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64Div, WordBinop, SignedDiv, Word64)
  DECL_MULTI_REP_BINOP(UintDiv, WordBinop, WordRepresentation, UnsignedDiv)
  DECL_SINGLE_REP_BINOP_V(Uint32Div, WordBinop, UnsignedDiv, Word32)
  DECL_SINGLE_REP_BINOP_V(Uint64Div, WordBinop, UnsignedDiv, Word64)
  DECL_MULTI_REP_BINOP(IntMod, WordBinop, WordRepresentation, SignedMod)
  DECL_SINGLE_REP_BINOP_V(Int32Mod, WordBinop, SignedMod, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64Mod, WordBinop, SignedMod, Word64)
  DECL_MULTI_REP_BINOP(UintMod, WordBinop, WordRepresentation, UnsignedMod)
  DECL_SINGLE_REP_BINOP_V(Uint32Mod, WordBinop, UnsignedMod, Word32)
  DECL_SINGLE_REP_BINOP_V(Uint64Mod, WordBinop, UnsignedMod, Word64)
  DECL_MULTI_REP_BINOP(IntMulOverflownBits, WordBinop, WordRepresentation,
                       SignedMulOverflownBits)
  DECL_SINGLE_REP_BINOP_V(Int32MulOverflownBits, WordBinop,
                          SignedMulOverflownBits, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64MulOverflownBits, WordBinop,
                          SignedMulOverflownBits, Word64)
  DECL_MULTI_REP_BINOP(UintMulOverflownBits, WordBinop, WordRepresentation,
                       UnsignedMulOverflownBits)
  DECL_SINGLE_REP_BINOP_V(Uint32MulOverflownBits, WordBinop,
                          UnsignedMulOverflownBits, Word32)
  DECL_SINGLE_REP_BINOP_V(Uint64MulOverflownBits, WordBinop,
                          UnsignedMulOverflownBits, Word64)

  DECL_MULTI_REP_BINOP(IntAddCheckOverflow, OverflowCheckedBinop,
                       WordRepresentation, SignedAdd)
  DECL_SINGLE_REP_BINOP_V(Int32AddCheckOverflow, OverflowCheckedBinop,
                          SignedAdd, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64AddCheckOverflow, OverflowCheckedBinop,
                          SignedAdd, Word64)
  DECL_MULTI_REP_BINOP(IntSubCheckOverflow, OverflowCheckedBinop,
                       WordRepresentation, SignedSub)
  DECL_SINGLE_REP_BINOP_V(Int32SubCheckOverflow, OverflowCheckedBinop,
                          SignedSub, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64SubCheckOverflow, OverflowCheckedBinop,
                          SignedSub, Word64)
  DECL_MULTI_REP_BINOP(IntMulCheckOverflow, OverflowCheckedBinop,
                       WordRepresentation, SignedMul)
  DECL_SINGLE_REP_BINOP_V(Int32MulCheckOverflow, OverflowCheckedBinop,
                          SignedMul, Word32)
  DECL_SINGLE_REP_BINOP_V(Int64MulCheckOverflow, OverflowCheckedBinop,
                          SignedMul, Word64)

  DECL_MULTI_REP_BINOP(FloatAdd, FloatBinop, FloatRepresentation, Add)
  DECL_SINGLE_REP_BINOP_V(Float32Add, FloatBinop, Add, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Add, FloatBinop, Add, Float64)
  DECL_MULTI_REP_BINOP(FloatMul, FloatBinop, FloatRepresentation, Mul)
  DECL_SINGLE_REP_BINOP_V(Float32Mul, FloatBinop, Mul, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Mul, FloatBinop, Mul, Float64)
  DECL_MULTI_REP_BINOP(FloatSub, FloatBinop, FloatRepresentation, Sub)
  DECL_SINGLE_REP_BINOP_V(Float32Sub, FloatBinop, Sub, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Sub, FloatBinop, Sub, Float64)
  DECL_MULTI_REP_BINOP(FloatDiv, FloatBinop, FloatRepresentation, Div)
  DECL_SINGLE_REP_BINOP_V(Float32Div, FloatBinop, Div, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Div, FloatBinop, Div, Float64)
  DECL_MULTI_REP_BINOP(FloatMin, FloatBinop, FloatRepresentation, Min)
  DECL_SINGLE_REP_BINOP_V(Float32Min, FloatBinop, Min, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Min, FloatBinop, Min, Float64)
  DECL_MULTI_REP_BINOP(FloatMax, FloatBinop, FloatRepresentation, Max)
  DECL_SINGLE_REP_BINOP_V(Float32Max, FloatBinop, Max, Float32)
  DECL_SINGLE_REP_BINOP_V(Float64Max, FloatBinop, Max, Float64)
  DECL_SINGLE_REP_BINOP_V(Float64Mod, FloatBinop, Mod, Float64)
  DECL_SINGLE_REP_BINOP_V(Float64Power, FloatBinop, Power, Float64)
  DECL_SINGLE_REP_BINOP_V(Float64Atan2, FloatBinop, Atan2, Float64)

  OpIndex Shift(OpIndex left, OpIndex right, ShiftOp::Kind kind,
                WordRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceShift(left, right, kind, rep);
  }

  DECL_MULTI_REP_BINOP(ShiftRightArithmeticShiftOutZeros, Shift,
                       WordRepresentation, ShiftRightArithmeticShiftOutZeros)
  DECL_SINGLE_REP_BINOP_V(Word32ShiftRightArithmeticShiftOutZeros, Shift,
                          ShiftRightArithmeticShiftOutZeros, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64ShiftRightArithmeticShiftOutZeros, Shift,
                          ShiftRightArithmeticShiftOutZeros, Word64)
  DECL_MULTI_REP_BINOP(ShiftRightArithmetic, Shift, WordRepresentation,
                       ShiftRightArithmetic)
  DECL_SINGLE_REP_BINOP_V(Word32ShiftRightArithmetic, Shift,
                          ShiftRightArithmetic, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64ShiftRightArithmetic, Shift,
                          ShiftRightArithmetic, Word64)
  DECL_MULTI_REP_BINOP(ShiftRightLogical, Shift, WordRepresentation,
                       ShiftRightLogical)
  DECL_SINGLE_REP_BINOP_V(Word32ShiftRightLogical, Shift, ShiftRightLogical,
                          Word32)
  DECL_SINGLE_REP_BINOP_V(Word64ShiftRightLogical, Shift, ShiftRightLogical,
                          Word64)
  DECL_MULTI_REP_BINOP(ShiftLeft, Shift, WordRepresentation, ShiftLeft)
  DECL_SINGLE_REP_BINOP_V(Word32ShiftLeft, Shift, ShiftLeft, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64ShiftLeft, Shift, ShiftLeft, Word64)
  DECL_SINGLE_REP_BINOP_V(WordPtrShiftLeft, Shift, ShiftLeft, WordPtr)
  DECL_MULTI_REP_BINOP(RotateRight, Shift, WordRepresentation, RotateRight)
  DECL_SINGLE_REP_BINOP_V(Word32RotateRight, Shift, RotateRight, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64RotateRight, Shift, RotateRight, Word64)
  DECL_MULTI_REP_BINOP(RotateLeft, Shift, WordRepresentation, RotateLeft)
  DECL_SINGLE_REP_BINOP_V(Word32RotateLeft, Shift, RotateLeft, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64RotateLeft, Shift, RotateLeft, Word64)

  OpIndex ShiftRightLogical(OpIndex left, uint32_t right,
                            WordRepresentation rep) {
    DCHECK_GE(right, 0);
    DCHECK_LT(right, rep.bit_width());
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return ShiftRightLogical(left, this->Word32Constant(right), rep);
  }
  OpIndex ShiftRightArithmetic(OpIndex left, uint32_t right,
                               WordRepresentation rep) {
    DCHECK_GE(right, 0);
    DCHECK_LT(right, rep.bit_width());
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return ShiftRightArithmetic(left, this->Word32Constant(right), rep);
  }
  OpIndex ShiftLeft(OpIndex left, uint32_t right, WordRepresentation rep) {
    DCHECK_LT(right, rep.bit_width());
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return ShiftLeft(left, this->Word32Constant(right), rep);
  }

  OpIndex Equal(OpIndex left, OpIndex right, RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceEqual(left, right, rep);
  }

#define DECL_SINGLE_REP_EQUAL_V(name, operation, tag)                          \
  V<Word32> name(ConstOrV<tag> left, ConstOrV<tag> right) {                    \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {            \
      return OpIndex::Invalid();                                               \
    }                                                                          \
    return stack().Reduce##operation(resolve(left), resolve(right), tag::Rep); \
  }
  DECL_SINGLE_REP_EQUAL_V(Word32Equal, Equal, Word32)
  DECL_SINGLE_REP_EQUAL_V(Word64Equal, Equal, Word64)
  DECL_SINGLE_REP_EQUAL_V(Float32Equal, Equal, Float32)
  DECL_SINGLE_REP_EQUAL_V(Float64Equal, Equal, Float64)
#undef DECL_SINGLE_REP_EQUAL_V

  DECL_SINGLE_REP_BINOP_NO_KIND(TaggedEqual, Equal,
                                RegisterRepresentation::Tagged())

#define DECL_SINGLE_REP_COMPARISON_V(name, operation, kind, tag)              \
  V<Word32> name(ConstOrV<tag> left, ConstOrV<tag> right) {                   \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {           \
      return OpIndex::Invalid();                                              \
    }                                                                         \
    return stack().Reduce##operation(resolve(left), resolve(right),           \
                                     operation##Op::Kind::k##kind, tag::Rep); \
  }

  DECL_MULTI_REP_BINOP(IntLessThan, Comparison, RegisterRepresentation,
                       SignedLessThan)
  DECL_SINGLE_REP_COMPARISON_V(Int32LessThan, Comparison, SignedLessThan,
                               Word32)
  DECL_SINGLE_REP_COMPARISON_V(Int64LessThan, Comparison, SignedLessThan,
                               Word64)

  DECL_MULTI_REP_BINOP(UintLessThan, Comparison, RegisterRepresentation,
                       UnsignedLessThan)
  DECL_SINGLE_REP_COMPARISON_V(Uint32LessThan, Comparison, UnsignedLessThan,
                               Word32)
  DECL_SINGLE_REP_COMPARISON_V(Uint64LessThan, Comparison, UnsignedLessThan,
                               Word64)
  DECL_SINGLE_REP_BINOP(UintPtrLessThan, Comparison, UnsignedLessThan,
                        WordRepresentation::PointerSized())
  DECL_MULTI_REP_BINOP(FloatLessThan, Comparison, RegisterRepresentation,
                       SignedLessThan)
  DECL_SINGLE_REP_COMPARISON_V(Float32LessThan, Comparison, SignedLessThan,
                               Float32)
  DECL_SINGLE_REP_COMPARISON_V(Float64LessThan, Comparison, SignedLessThan,
                               Float64)

  DECL_MULTI_REP_BINOP(IntLessThanOrEqual, Comparison, RegisterRepresentation,
                       SignedLessThanOrEqual)
  DECL_SINGLE_REP_COMPARISON_V(Int32LessThanOrEqual, Comparison,
                               SignedLessThanOrEqual, Word32)
  DECL_SINGLE_REP_COMPARISON_V(Int64LessThanOrEqual, Comparison,
                               SignedLessThanOrEqual, Word64)
  DECL_MULTI_REP_BINOP(UintLessThanOrEqual, Comparison, RegisterRepresentation,
                       UnsignedLessThanOrEqual)
  DECL_SINGLE_REP_COMPARISON_V(Uint32LessThanOrEqual, Comparison,
                               UnsignedLessThanOrEqual, Word32)
  DECL_SINGLE_REP_COMPARISON_V(Uint64LessThanOrEqual, Comparison,
                               UnsignedLessThanOrEqual, Word64)
  DECL_SINGLE_REP_BINOP(UintPtrLessThanOrEqual, Comparison,
                        UnsignedLessThanOrEqual,
                        WordRepresentation::PointerSized())
  DECL_MULTI_REP_BINOP(FloatLessThanOrEqual, Comparison, RegisterRepresentation,
                       SignedLessThanOrEqual)
  DECL_SINGLE_REP_COMPARISON_V(Float32LessThanOrEqual, Comparison,
                               SignedLessThanOrEqual, Float32)
  DECL_SINGLE_REP_COMPARISON_V(Float64LessThanOrEqual, Comparison,
                               SignedLessThanOrEqual, Float64)
#undef DECL_SINGLE_REP_COMPARISON_V

  OpIndex Comparison(OpIndex left, OpIndex right, ComparisonOp::Kind kind,
                     RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceComparison(left, right, kind, rep);
  }

#undef DECL_SINGLE_REP_BINOP
#undef DECL_SINGLE_REP_BINOP_V
#undef DECL_MULTI_REP_BINOP
#undef DECL_SINGLE_REP_BINOP_NO_KIND

#define DECL_MULTI_REP_UNARY(name, operation, rep_type, kind)             \
  OpIndex name(OpIndex input, rep_type rep) {                             \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {       \
      return OpIndex::Invalid();                                          \
    }                                                                     \
    return stack().Reduce##operation(input, operation##Op::Kind::k##kind, \
                                     rep);                                \
  }
#define DECL_SINGLE_REP_UNARY_V(name, operation, kind, tag)                   \
  V<tag> name(ConstOrV<tag> input) {                                          \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {           \
      return OpIndex::Invalid();                                              \
    }                                                                         \
    return stack().Reduce##operation(resolve(input),                          \
                                     operation##Op::Kind::k##kind, tag::Rep); \
  }

  DECL_MULTI_REP_UNARY(FloatAbs, FloatUnary, FloatRepresentation, Abs)
  DECL_SINGLE_REP_UNARY_V(Float32Abs, FloatUnary, Abs, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64Abs, FloatUnary, Abs, Float64)
  DECL_MULTI_REP_UNARY(FloatNegate, FloatUnary, FloatRepresentation, Negate)
  DECL_SINGLE_REP_UNARY_V(Float32Negate, FloatUnary, Negate, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64Negate, FloatUnary, Negate, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64SilenceNaN, FloatUnary, SilenceNaN, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundDown, FloatUnary, FloatRepresentation,
                       RoundDown)
  DECL_SINGLE_REP_UNARY_V(Float32RoundDown, FloatUnary, RoundDown, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64RoundDown, FloatUnary, RoundDown, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundUp, FloatUnary, FloatRepresentation, RoundUp)
  DECL_SINGLE_REP_UNARY_V(Float32RoundUp, FloatUnary, RoundUp, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64RoundUp, FloatUnary, RoundUp, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundToZero, FloatUnary, FloatRepresentation,
                       RoundToZero)
  DECL_SINGLE_REP_UNARY_V(Float32RoundToZero, FloatUnary, RoundToZero, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64RoundToZero, FloatUnary, RoundToZero, Float64)
  DECL_MULTI_REP_UNARY(FloatRoundTiesEven, FloatUnary, FloatRepresentation,
                       RoundTiesEven)
  DECL_SINGLE_REP_UNARY_V(Float32RoundTiesEven, FloatUnary, RoundTiesEven,
                          Float32)
  DECL_SINGLE_REP_UNARY_V(Float64RoundTiesEven, FloatUnary, RoundTiesEven,
                          Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Log, FloatUnary, Log, Float64)
  DECL_MULTI_REP_UNARY(FloatSqrt, FloatUnary, FloatRepresentation, Sqrt)
  DECL_SINGLE_REP_UNARY_V(Float32Sqrt, FloatUnary, Sqrt, Float32)
  DECL_SINGLE_REP_UNARY_V(Float64Sqrt, FloatUnary, Sqrt, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Exp, FloatUnary, Exp, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Expm1, FloatUnary, Expm1, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Sin, FloatUnary, Sin, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Cos, FloatUnary, Cos, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Sinh, FloatUnary, Sinh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Cosh, FloatUnary, Cosh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Asin, FloatUnary, Asin, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Acos, FloatUnary, Acos, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Asinh, FloatUnary, Asinh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Acosh, FloatUnary, Acosh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Tan, FloatUnary, Tan, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Tanh, FloatUnary, Tanh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Log2, FloatUnary, Log2, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Log10, FloatUnary, Log10, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Log1p, FloatUnary, Log1p, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Atan, FloatUnary, Atan, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Atanh, FloatUnary, Atanh, Float64)
  DECL_SINGLE_REP_UNARY_V(Float64Cbrt, FloatUnary, Cbrt, Float64)

  DECL_MULTI_REP_UNARY(WordReverseBytes, WordUnary, WordRepresentation,
                       ReverseBytes)
  DECL_SINGLE_REP_UNARY_V(Word32ReverseBytes, WordUnary, ReverseBytes, Word32)
  DECL_SINGLE_REP_UNARY_V(Word64ReverseBytes, WordUnary, ReverseBytes, Word64)
  DECL_MULTI_REP_UNARY(WordCountLeadingZeros, WordUnary, WordRepresentation,
                       CountLeadingZeros)
  DECL_SINGLE_REP_UNARY_V(Word32CountLeadingZeros, WordUnary, CountLeadingZeros,
                          Word32)
  DECL_SINGLE_REP_UNARY_V(Word64CountLeadingZeros, WordUnary, CountLeadingZeros,
                          Word64)
  DECL_MULTI_REP_UNARY(WordCountTrailingZeros, WordUnary, WordRepresentation,
                       CountTrailingZeros)
  DECL_SINGLE_REP_UNARY_V(Word32CountTrailingZeros, WordUnary,
                          CountTrailingZeros, Word32)
  DECL_SINGLE_REP_UNARY_V(Word64CountTrailingZeros, WordUnary,
                          CountTrailingZeros, Word64)
  DECL_MULTI_REP_UNARY(WordPopCount, WordUnary, WordRepresentation, PopCount)
  DECL_SINGLE_REP_UNARY_V(Word32PopCount, WordUnary, PopCount, Word32)
  DECL_SINGLE_REP_UNARY_V(Word64PopCount, WordUnary, PopCount, Word64)
  DECL_MULTI_REP_UNARY(WordSignExtend8, WordUnary, WordRepresentation,
                       SignExtend8)
  DECL_SINGLE_REP_UNARY_V(Word32SignExtend8, WordUnary, SignExtend8, Word32)
  DECL_SINGLE_REP_UNARY_V(Word64SignExtend8, WordUnary, SignExtend8, Word64)
  DECL_MULTI_REP_UNARY(WordSignExtend16, WordUnary, WordRepresentation,
                       SignExtend16)
  DECL_SINGLE_REP_UNARY_V(Word32SignExtend16, WordUnary, SignExtend16, Word32)
  DECL_SINGLE_REP_UNARY_V(Word64SignExtend16, WordUnary, SignExtend16, Word64)
#undef DECL_SINGLE_REP_UNARY_V
#undef DECL_MULTI_REP_UNARY

  OpIndex Float64InsertWord32(OpIndex float64, OpIndex word32,
                              Float64InsertWord32Op::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFloat64InsertWord32(float64, word32, kind);
  }

  OpIndex TaggedBitcast(OpIndex input, RegisterRepresentation from,
                        RegisterRepresentation to) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTaggedBitcast(input, from, to);
  }
  OpIndex BitcastTaggedToWord(OpIndex tagged) {
    return TaggedBitcast(tagged, RegisterRepresentation::Tagged(),
                         RegisterRepresentation::PointerSized());
  }
  OpIndex BitcastWordToTagged(OpIndex word) {
    return TaggedBitcast(word, RegisterRepresentation::PointerSized(),
                         RegisterRepresentation::Tagged());
  }

  OpIndex ObjectIs(OpIndex input, ObjectIsOp::Kind kind,
                   ObjectIsOp::InputAssumptions input_assumptions) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceObjectIs(input, kind, input_assumptions);
  }

  OpIndex ConvertToObject(
      OpIndex input, ConvertToObjectOp::Kind kind,
      RegisterRepresentation input_rep,
      ConvertToObjectOp::InputInterpretation input_interpretation,
      CheckForMinusZeroMode minus_zero_mode) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertToObject(input, kind, input_rep,
                                         input_interpretation, minus_zero_mode);
  }

  OpIndex Word32Constant(uint32_t value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kWord32, uint64_t{value});
  }
  OpIndex Word32Constant(int32_t value) {
    return Word32Constant(static_cast<uint32_t>(value));
  }
  OpIndex Word64Constant(uint64_t value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kWord64, value);
  }
  OpIndex Word64Constant(int64_t value) {
    return Word64Constant(static_cast<uint64_t>(value));
  }
  OpIndex WordConstant(uint64_t value, WordRepresentation rep) {
    switch (rep.value()) {
      case WordRepresentation::Word32():
        return Word32Constant(static_cast<uint32_t>(value));
      case WordRepresentation::Word64():
        return Word64Constant(value);
    }
  }
  OpIndex IntPtrConstant(intptr_t value) {
    return UintPtrConstant(static_cast<uintptr_t>(value));
  }
  OpIndex UintPtrConstant(uintptr_t value) {
    return WordConstant(static_cast<uint64_t>(value),
                        WordRepresentation::PointerSized());
  }
  OpIndex Float32Constant(float value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kFloat32, value);
  }
  OpIndex Float64Constant(double value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kFloat64, value);
  }
  OpIndex FloatConstant(double value, FloatRepresentation rep) {
    switch (rep.value()) {
      case FloatRepresentation::Float32():
        return Float32Constant(static_cast<float>(value));
      case FloatRepresentation::Float64():
        return Float64Constant(value);
    }
  }
  OpIndex NumberConstant(double value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kNumber, value);
  }
  OpIndex TaggedIndexConstant(int32_t value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kTaggedIndex,
                                  uint64_t{static_cast<uint32_t>(value)});
  }
  OpIndex HeapConstant(Handle<HeapObject> value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kHeapObject, value);
  }
  OpIndex BuiltinCode(Builtin builtin, Isolate* isolate) {
    return HeapConstant(BuiltinCodeHandle(builtin, isolate));
  }
  OpIndex CompressedHeapConstant(Handle<HeapObject> value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kHeapObject, value);
  }
  OpIndex ExternalConstant(ExternalReference value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kExternal, value);
  }
  OpIndex RelocatableConstant(int64_t value, RelocInfo::Mode mode) {
    DCHECK_EQ(mode, any_of(RelocInfo::WASM_CALL, RelocInfo::WASM_STUB_CALL));
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(
        mode == RelocInfo::WASM_CALL
            ? ConstantOp::Kind::kRelocatableWasmCall
            : ConstantOp::Kind::kRelocatableWasmStubCall,
        static_cast<uint64_t>(value));
  }

#define DECL_CHANGE(name, kind, assumption, from, to)                  \
  OpIndex name(OpIndex input) {                                        \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {    \
      return OpIndex::Invalid();                                       \
    }                                                                  \
    return stack().ReduceChange(                                       \
        input, ChangeOp::Kind::kind, ChangeOp::Assumption::assumption, \
        RegisterRepresentation::from(), RegisterRepresentation::to()); \
  }
#define DECL_CHANGE_V(name, kind, assumption, from, to)                      \
  V<to> name(ConstOrV<from> input) {                                         \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {          \
      return OpIndex::Invalid();                                             \
    }                                                                        \
    return stack().ReduceChange(resolve(input), ChangeOp::Kind::kind,        \
                                ChangeOp::Assumption::assumption, from::Rep, \
                                to::Rep);                                    \
  }
#define DECL_TRY_CHANGE(name, kind, from, to)                       \
  OpIndex name(OpIndex input) {                                     \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().ReduceTryChange(input, TryChangeOp::Kind::kind,  \
                                   FloatRepresentation::from(),     \
                                   WordRepresentation::to());       \
  }

  DECL_CHANGE_V(BitcastWord32ToWord64, kBitcast, kNoAssumption, Word32, Word64)
  DECL_CHANGE_V(BitcastFloat32ToWord32, kBitcast, kNoAssumption, Float32,
                Word32)
  DECL_CHANGE_V(BitcastWord32ToFloat32, kBitcast, kNoAssumption, Word32,
                Float32)
  DECL_CHANGE_V(BitcastFloat64ToWord64, kBitcast, kNoAssumption, Float64,
                Word64)
  DECL_CHANGE_V(BitcastWord64ToFloat64, kBitcast, kNoAssumption, Word64,
                Float64)
  DECL_CHANGE_V(ChangeUint32ToUint64, kZeroExtend, kNoAssumption, Word32,
                Word64)
  DECL_CHANGE_V(ChangeInt32ToInt64, kSignExtend, kNoAssumption, Word32, Word64)
  DECL_CHANGE_V(ChangeInt32ToFloat64, kSignedToFloat, kNoAssumption, Word32,
                Float64)
  DECL_CHANGE_V(ChangeInt64ToFloat64, kSignedToFloat, kNoAssumption, Word64,
                Float64)
  DECL_CHANGE_V(ChangeInt32ToFloat32, kSignedToFloat, kNoAssumption, Word32,
                Float32)
  DECL_CHANGE_V(ChangeInt64ToFloat32, kSignedToFloat, kNoAssumption, Word64,
                Float32)
  DECL_CHANGE_V(ChangeUint32ToFloat32, kUnsignedToFloat, kNoAssumption, Word32,
                Float32)
  DECL_CHANGE_V(ChangeUint64ToFloat32, kUnsignedToFloat, kNoAssumption, Word64,
                Float32)
  DECL_CHANGE_V(ReversibleInt64ToFloat64, kSignedToFloat, kReversible, Word64,
                Float64)
  DECL_CHANGE_V(ChangeUint64ToFloat64, kUnsignedToFloat, kNoAssumption, Word64,
                Float64)
  DECL_CHANGE_V(ReversibleUint64ToFloat64, kUnsignedToFloat, kReversible,
                Word64, Float64)
  DECL_CHANGE_V(ChangeUint32ToFloat64, kUnsignedToFloat, kNoAssumption, Word32,
                Float64)
  DECL_CHANGE_V(ChangeFloat64ToFloat32, kFloatConversion, kNoAssumption,
                Float64, Float32)
  DECL_CHANGE_V(ChangeFloat32ToFloat64, kFloatConversion, kNoAssumption,
                Float32, Float64)
  DECL_CHANGE_V(JSTruncateFloat64ToWord32, kJSFloatTruncate, kNoAssumption,
                Float64, Word32)
  V<WordPtr> ChangeInt32ToIntPtr(V<Word32> input) {
    if constexpr (Is64()) {
      return ChangeInt32ToInt64(input);
    } else {
      DCHECK_EQ(WordPtr::Rep, Word32::Rep);
      return V<WordPtr>::Cast(input);
    }
  }
  V<WordPtr> ChangeUint32ToUintPtr(V<Word32> input) {
    if constexpr (Is64()) {
      return ChangeUint32ToUint64(input);
    } else {
      DCHECK_EQ(WordPtr::Rep, Word32::Rep);
      return V<WordPtr>::Cast(input);
    }
  }

#define DECL_SIGNED_FLOAT_TRUNCATE(FloatBits, ResultBits)                     \
  DECL_CHANGE(TruncateFloat##FloatBits##ToInt##ResultBits##OverflowUndefined, \
              kSignedFloatTruncateOverflowToMin, kNoOverflow,                 \
              Float##FloatBits, Word##ResultBits)                             \
  DECL_CHANGE(TruncateFloat##FloatBits##ToInt##ResultBits##OverflowToMin,     \
              kSignedFloatTruncateOverflowToMin, kNoAssumption,               \
              Float##FloatBits, Word##ResultBits)                             \
  DECL_TRY_CHANGE(TryTruncateFloat##FloatBits##ToInt##ResultBits,             \
                  kSignedFloatTruncateOverflowUndefined, Float##FloatBits,    \
                  Word##ResultBits)

  DECL_SIGNED_FLOAT_TRUNCATE(64, 64)
  DECL_SIGNED_FLOAT_TRUNCATE(64, 32)
  DECL_SIGNED_FLOAT_TRUNCATE(32, 64)
  DECL_SIGNED_FLOAT_TRUNCATE(32, 32)
#undef DECL_SIGNED_FLOAT_TRUNCATE

#define DECL_UNSIGNED_FLOAT_TRUNCATE(FloatBits, ResultBits)                    \
  DECL_CHANGE(TruncateFloat##FloatBits##ToUint##ResultBits##OverflowUndefined, \
              kUnsignedFloatTruncateOverflowToMin, kNoOverflow,                \
              Float##FloatBits, Word##ResultBits)                              \
  DECL_CHANGE(TruncateFloat##FloatBits##ToUint##ResultBits##OverflowToMin,     \
              kUnsignedFloatTruncateOverflowToMin, kNoAssumption,              \
              Float##FloatBits, Word##ResultBits)                              \
  DECL_TRY_CHANGE(TryTruncateFloat##FloatBits##ToUint##ResultBits,             \
                  kUnsignedFloatTruncateOverflowUndefined, Float##FloatBits,   \
                  Word##ResultBits)

  DECL_UNSIGNED_FLOAT_TRUNCATE(64, 64)
  DECL_UNSIGNED_FLOAT_TRUNCATE(64, 32)
  DECL_UNSIGNED_FLOAT_TRUNCATE(32, 64)
  DECL_UNSIGNED_FLOAT_TRUNCATE(32, 32)
#undef DECL_UNSIGNED_FLOAT_TRUNCATE

  DECL_CHANGE_V(ReversibleFloat64ToInt32, kSignedFloatTruncateOverflowToMin,
                kReversible, Float64, Word32)
  DECL_CHANGE_V(ReversibleFloat64ToUint32, kUnsignedFloatTruncateOverflowToMin,
                kReversible, Float64, Word32)
  DECL_CHANGE_V(ReversibleFloat64ToInt64, kSignedFloatTruncateOverflowToMin,
                kReversible, Float64, Word64)
  DECL_CHANGE_V(ReversibleFloat64ToUint64, kUnsignedFloatTruncateOverflowToMin,
                kReversible, Float64, Word64)
  DECL_CHANGE_V(Float64ExtractLowWord32, kExtractLowHalf, kNoAssumption,
                Float64, Word32)
  DECL_CHANGE_V(Float64ExtractHighWord32, kExtractHighHalf, kNoAssumption,
                Float64, Word32)
#undef DECL_CHANGE
#undef DECL_CHANGE_V
#undef DECL_TRY_CHANGE

  OpIndex Tag(OpIndex input, TagKind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTag(input, kind);
  }
  V<Tagged> SmiTag(V<Word32> input) { return Tag(input, TagKind::kSmiTag); }

  OpIndex Untag(OpIndex input, TagKind kind, RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceUntag(input, kind, rep);
  }
  V<Word32> SmiUntag(V<Tagged> input) {
    return Untag(input, TagKind::kSmiTag, RegisterRepresentation::Word32());
  }

  OpIndex Load(OpIndex base, OpIndex index, LoadOp::Kind kind,
               MemoryRepresentation loaded_rep, int32_t offset = 0,
               uint8_t element_size_log2 = 0) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoad(base, index, kind, loaded_rep,
                              loaded_rep.ToRegisterRepresentation(), offset,
                              element_size_log2);
  }
  OpIndex Load(OpIndex base, LoadOp::Kind kind, MemoryRepresentation loaded_rep,
               int32_t offset = 0) {
    return Load(base, OpIndex::Invalid(), kind, loaded_rep, offset);
  }
  OpIndex LoadOffHeap(OpIndex address, MemoryRepresentation rep) {
    return LoadOffHeap(address, 0, rep);
  }
  OpIndex LoadOffHeap(OpIndex address, int32_t offset,
                      MemoryRepresentation rep) {
    return Load(address, LoadOp::Kind::RawAligned(), rep, offset);
  }
  OpIndex LoadOffHeap(OpIndex address, OpIndex index, int32_t offset,
                      MemoryRepresentation rep) {
    return Load(address, index, LoadOp::Kind::RawAligned(), rep, offset,
                rep.SizeInBytesLog2());
  }

  void Store(OpIndex base, OpIndex index, OpIndex value, StoreOp::Kind kind,
             MemoryRepresentation stored_rep, WriteBarrierKind write_barrier,
             int32_t offset = 0, uint8_t element_size_log2 = 0) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStore(base, index, value, kind, stored_rep, write_barrier,
                        offset, element_size_log2);
  }
  void Store(OpIndex base, OpIndex value, StoreOp::Kind kind,
             MemoryRepresentation stored_rep, WriteBarrierKind write_barrier,
             int32_t offset = 0) {
    Store(base, OpIndex::Invalid(), value, kind, stored_rep, write_barrier,
          offset);
  }
  void StoreOffHeap(OpIndex address, OpIndex value, MemoryRepresentation rep,
                    int32_t offset = 0) {
    Store(address, value, StoreOp::Kind::RawAligned(), rep,
          WriteBarrierKind::kNoWriteBarrier, offset);
  }
  void StoreOffHeap(OpIndex address, OpIndex index, OpIndex value,
                    MemoryRepresentation rep, int32_t offset) {
    Store(address, index, value, StoreOp::Kind::RawAligned(), rep,
          WriteBarrierKind::kNoWriteBarrier, offset, rep.SizeInBytesLog2());
  }

  V<Tagged> Allocate(V<WordPtr> size, AllocationType type,
                     AllowLargeObjects allow_large_objects) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceAllocate(size, type, allow_large_objects);
  }

  OpIndex DecodeExternalPointer(OpIndex handle, ExternalPointerTag tag) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceDecodeExternalPointer(handle, tag);
  }

  void Retain(OpIndex value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceRetain(value);
  }

  OpIndex StackPointerGreaterThan(OpIndex limit, StackCheckKind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStackPointerGreaterThan(limit, kind);
  }

  OpIndex StackCheckOffset() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFrameConstant(
        FrameConstantOp::Kind::kStackCheckOffset);
  }
  OpIndex FramePointer() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFrameConstant(FrameConstantOp::Kind::kFramePointer);
  }
  OpIndex ParentFramePointer() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFrameConstant(
        FrameConstantOp::Kind::kParentFramePointer);
  }

  OpIndex StackSlot(int size, int alignment) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStackSlot(size, alignment);
  }

  void Goto(Block* destination) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceGoto(destination);
  }
  void Branch(V<Word32> condition, Block* if_true, Block* if_false,
              BranchHint hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceBranch(condition, if_true, if_false, hint);
  }
  OpIndex Select(OpIndex cond, OpIndex vtrue, OpIndex vfalse,
                 RegisterRepresentation rep, BranchHint hint,
                 SelectOp::Implementation implem) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSelect(cond, vtrue, vfalse, rep, hint, implem);
  }
  void Switch(OpIndex input, base::Vector<const SwitchOp::Case> cases,
              Block* default_case,
              BranchHint default_hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceSwitch(input, cases, default_case, default_hint);
  }
  void Unreachable() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceUnreachable();
  }

  OpIndex Parameter(int index, RegisterRepresentation rep,
                    const char* debug_name = nullptr) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceParameter(index, rep, debug_name);
  }
  OpIndex OsrValue(int index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceOsrValue(index);
  }
  void Return(OpIndex pop_count, base::Vector<OpIndex> return_values) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceReturn(pop_count, return_values);
  }
  void Return(OpIndex result) {
    Return(Word32Constant(0), base::VectorOf({result}));
  }

  OpIndex Call(OpIndex callee, OpIndex frame_state,
               base::Vector<const OpIndex> arguments,
               const TSCallDescriptor* descriptor) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCall(callee, frame_state, arguments, descriptor);
  }
  OpIndex Call(OpIndex callee, std::initializer_list<OpIndex> arguments,
               const TSCallDescriptor* descriptor) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return Call(callee, OpIndex::Invalid(), base::VectorOf(arguments),
                descriptor);
  }
  OpIndex CallAndCatchException(OpIndex callee, OpIndex frame_state,
                                base::Vector<const OpIndex> arguments,
                                Block* if_success, Block* if_exception,
                                const TSCallDescriptor* descriptor) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCallAndCatchException(
        callee, frame_state, arguments, if_success, if_exception, descriptor);
  }
  void TailCall(OpIndex callee, base::Vector<const OpIndex> arguments,
                const TSCallDescriptor* descriptor) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTailCall(callee, arguments, descriptor);
  }

  OpIndex FrameState(base::Vector<const OpIndex> inputs, bool inlined,
                     const FrameStateData* data) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFrameState(inputs, inlined, data);
  }
  void DeoptimizeIf(OpIndex condition, OpIndex frame_state,
                    const DeoptimizeParameters* parameters) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDeoptimizeIf(condition, frame_state, false, parameters);
  }
  void DeoptimizeIfNot(OpIndex condition, OpIndex frame_state,
                       const DeoptimizeParameters* parameters) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDeoptimizeIf(condition, frame_state, true, parameters);
  }
  void DeoptimizeIf(OpIndex condition, OpIndex frame_state,
                    DeoptimizeReason reason, const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    Zone* zone = stack().output_graph().graph_zone();
    const DeoptimizeParameters* params =
        zone->New<DeoptimizeParameters>(reason, feedback);
    DeoptimizeIf(condition, frame_state, params);
  }
  void DeoptimizeIfNot(OpIndex condition, OpIndex frame_state,
                       DeoptimizeReason reason,
                       const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    Zone* zone = stack().output_graph().graph_zone();
    const DeoptimizeParameters* params =
        zone->New<DeoptimizeParameters>(reason, feedback);
    DeoptimizeIfNot(condition, frame_state, params);
  }
  void Deoptimize(OpIndex frame_state, const DeoptimizeParameters* parameters) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDeoptimize(frame_state, parameters);
  }

  void TrapIf(OpIndex condition, TrapId trap_id) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTrapIf(condition, false, trap_id);
  }
  void TrapIfNot(OpIndex condition, TrapId trap_id) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTrapIf(condition, true, trap_id);
  }

  void StaticAssert(OpIndex condition, const char* source) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStaticAssert(condition, source);
  }

  OpIndex Phi(base::Vector<const OpIndex> inputs, RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReducePhi(inputs, rep);
  }
  OpIndex Phi(std::initializer_list<OpIndex> inputs,
              RegisterRepresentation rep) {
    return Phi(base::VectorOf(inputs), rep);
  }
  OpIndex PendingLoopPhi(OpIndex first, RegisterRepresentation rep,
                         OpIndex old_backedge_index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReducePendingLoopPhi(first, rep, old_backedge_index);
  }
  OpIndex PendingLoopPhi(OpIndex first, RegisterRepresentation rep,
                         Node* old_backedge_index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReducePendingLoopPhi(first, rep, old_backedge_index);
  }

  OpIndex Tuple(base::Vector<OpIndex> indices) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTuple(indices);
  }
  OpIndex Tuple(OpIndex a, OpIndex b) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTuple(base::VectorOf({a, b}));
  }
  OpIndex Projection(OpIndex tuple, uint16_t index,
                     RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceProjection(tuple, index, rep);
  }
  OpIndex CheckTurboshaftTypeOf(OpIndex input, RegisterRepresentation rep,
                                Type expected_type, bool successful) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCheckTurboshaftTypeOf(input, rep, expected_type,
                                               successful);
  }

  OpIndex LoadException() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadException();
  }

  // Return `true` if the control flow after the conditional jump is reachable.
  bool GotoIf(OpIndex condition, Block* if_true,
              BranchHint hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return false;
    }
    Block* if_false = stack().NewBlock();
    stack().Branch(condition, if_true, if_false, hint);
    return stack().Bind(if_false);
  }
  // Return `true` if the control flow after the conditional jump is reachable.
  bool GotoIfNot(OpIndex condition, Block* if_false,
                 BranchHint hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return false;
    }
    Block* if_true = stack().NewBlock();
    stack().Branch(condition, if_true, if_false, hint);
    return stack().Bind(if_true);
  }

  OpIndex CallBuiltin(Builtin builtin, OpIndex frame_state,
                      const base::Vector<OpIndex>& arguments,
                      Isolate* isolate) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    Callable const callable = Builtins::CallableFor(isolate, builtin);
    Zone* graph_zone = stack().output_graph().graph_zone();

    const CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
        graph_zone, callable.descriptor(),
        callable.descriptor().GetStackParameterCount(),
        CallDescriptor::kNoFlags, Operator::kNoThrow | Operator::kNoDeopt);
    DCHECK_EQ(call_descriptor->NeedsFrameState(), frame_state.valid());

    const TSCallDescriptor* ts_call_descriptor =
        TSCallDescriptor::Create(call_descriptor, graph_zone);

    OpIndex callee = stack().HeapConstant(callable.code());

    return stack().Call(callee, frame_state, arguments, ts_call_descriptor);
  }

  template <typename Rep>
  V<Rep> resolve(const V<Rep>& v) {
    return v;
  }
  V<Word32> resolve(const ConstOrV<Word32>& v) {
    return v.is_constant() ? Word32Constant(v.constant_value()) : v.value();
  }
  V<Word64> resolve(const ConstOrV<Word64>& v) {
    return v.is_constant() ? Word64Constant(v.constant_value()) : v.value();
  }
  V<Float32> resolve(const ConstOrV<Float32>& v) {
    return v.is_constant() ? Float32Constant(v.constant_value()) : v.value();
  }
  V<Float64> resolve(const ConstOrV<Float64>& v) {
    return v.is_constant() ? Float64Constant(v.constant_value()) : v.value();
  }

  template <typename... Reps>
  Label<Reps...> CreateLabel() {
    return Label<Reps...>::Create(stack());
  }

  // These methods are used by the assembler macros (IF, ELSE, ELSE_IF, END_IF).
  template <typename... Reps>
  void ControlFlowHelper_Goto(
      Label<Reps...>& label,
      const typename Label<Reps...>::const_or_values_t& values) {
    label.Set(stack(), values);
    Goto(label.block());
  }

  template <typename... Reps>
  void ControlFlowHelper_GotoIf(
      V<Word32> condition, Label<Reps...>& label,
      const typename Label<Reps...>::const_or_values_t& values,
      BranchHint hint) {
    label.Set(stack(), values);
    GotoIf(condition, label.block(), hint);
  }

  template <typename... Reps>
  void ControlFlowHelper_GotoIfNot(
      V<Word32> condition, Label<Reps...>& label,
      const typename Label<Reps...>::const_or_values_t& values,
      BranchHint hint) {
    label.Set(stack(), values);
    GotoIfNot(condition, label.block(), hint);
  }

  bool ControlFlowHelper_If(V<Word32> condition, BranchHint hint) {
    Block* then_block = stack().NewBlock();
    Block* else_block = stack().NewBlock();
    Block* end_block = stack().NewBlock();
    this->Branch(condition, then_block, else_block, hint);
    if_scope_stack_.emplace_back(else_block, end_block);
    return stack().Bind(then_block);
  }

  bool ControlFlowHelper_ElseIf(V<Word32> condition) {
    DCHECK_LT(0, if_scope_stack_.size());
    auto& info = if_scope_stack_.back();
    Block* else_block = info.else_block;
    DCHECK_NOT_NULL(else_block);
    if (!stack().Bind(else_block)) return false;
    Block* then_block = stack().NewBlock();
    info.else_block = stack().NewBlock();
    stack().Branch(condition, then_block, info.else_block);
    return stack().Bind(then_block);
  }

  bool ControlFlowHelper_Else() {
    DCHECK_LT(0, if_scope_stack_.size());
    auto& info = if_scope_stack_.back();
    Block* else_block = info.else_block;
    DCHECK_NOT_NULL(else_block);
    info.else_block = nullptr;
    return stack().Bind(else_block);
  }

  void ControlFlowHelper_EndIf() {
    DCHECK_LT(0, if_scope_stack_.size());
    auto& info = if_scope_stack_.back();
    // Do we still have to place an else block (aka we had if's without else).
    if (info.else_block) {
      if (stack().Bind(info.else_block)) {
        stack().Goto(info.end_block);
      }
    }
    stack().Bind(info.end_block);
    if_scope_stack_.pop_back();
  }

  void ControlFlowHelper_GotoEnd() {
    DCHECK_LT(0, if_scope_stack_.size());
    auto& info = if_scope_stack_.back();

    if (!stack().current_block()) {
      // We had an unconditional goto inside the block, so we don't need to add
      // a jump to the end block.
      return;
    }
    // Generate a jump to the end block.
    stack().Goto(info.end_block);
  }

 private:
  Assembler& stack() { return *static_cast<Assembler*>(this); }
  struct IfScopeInfo {
    Block* else_block;
    Block* end_block;

    IfScopeInfo(Block* else_block, Block* end_block)
        : else_block(else_block), end_block(end_block) {}
  };
  base::SmallVector<IfScopeInfo, 16> if_scope_stack_;
};

template <class Reducers>
class Assembler : public GraphVisitor<Assembler<Reducers>>,
                  public reducer_stack_type<Reducers>::type,
                  public OperationMatching<Assembler<Reducers>>,
                  public AssemblerOpInterface<Assembler<Reducers>> {
  using Stack = typename reducer_stack_type<Reducers>::type;

 public:
  template <class... ReducerArgs>
  explicit Assembler(Graph& input_graph, Graph& output_graph, Zone* phase_zone,
                     compiler::NodeOriginTable* origins,
                     const typename Stack::ArgT& reducer_args)
      : GraphVisitor<Assembler>(input_graph, output_graph, phase_zone, origins),
        Stack(reducer_args) {
    SupportedOperations::Initialize();
  }

  Block* NewLoopHeader() { return this->output_graph().NewLoopHeader(); }
  Block* NewBlock() { return this->output_graph().NewBlock(); }

  using OperationMatching<Assembler<Reducers>>::Get;
  using Stack::Get;

  V8_INLINE bool Bind(Block* block, const Block* origin = nullptr) {
    if (!this->output_graph().Add(block)) {
      generating_unreachable_operations_ = true;
      return false;
    }
    DCHECK_NULL(current_block_);
    current_block_ = block;
    generating_unreachable_operations_ = false;
    if (origin == nullptr) origin = this->current_input_block();
    if (origin != nullptr) block->SetOrigin(origin);
    Stack::Bind(block, origin);
    return true;
  }

  // TODO(nicohartmann@): Remove this.
  V8_INLINE void BindReachable(Block* block, const Block* origin = nullptr) {
    bool bound = Bind(block, origin);
    DCHECK(bound);
    USE(bound);
  }

  void SetCurrentOrigin(OpIndex operation_origin) {
    current_operation_origin_ = operation_origin;
  }

  Block* current_block() const { return current_block_; }
  bool generating_unreachable_operations() const {
    DCHECK_IMPLIES(generating_unreachable_operations_,
                   current_block_ == nullptr);
    return generating_unreachable_operations_;
  }
  OpIndex current_operation_origin() const { return current_operation_origin_; }

  // ReduceProjection eliminates projections to tuples and returns instead the
  // corresponding tuple input. We do this at the top of the stack to avoid
  // passing this Projection around needlessly. This is in particular important
  // to ValueNumberingReducer, which assumes that it's at the bottom of the
  // stack, and that the BaseReducer will actually emit an Operation. If we put
  // this projection-to-tuple-simplification in the BaseReducer, then this
  // assumption of the ValueNumberingReducer will break.
  OpIndex ReduceProjection(OpIndex tuple, uint16_t index,
                           RegisterRepresentation rep) {
    if (auto* tuple_op = this->template TryCast<TupleOp>(tuple)) {
      return tuple_op->input(index);
    }
    return Stack::ReduceProjection(tuple, index, rep);
  }

  template <class Op, class... Args>
  OpIndex Emit(Args... args) {
    static_assert((std::is_base_of<Operation, Op>::value));
    static_assert(!(std::is_same<Op, Operation>::value));
    DCHECK_NOT_NULL(current_block_);
    OpIndex result = this->output_graph().next_operation_index();
    Op& op = this->output_graph().template Add<Op>(args...);
    this->output_graph().operation_origins()[result] =
        current_operation_origin_;
#ifdef DEBUG
    op_to_block_[result] = current_block_;
    ValidInputs(result);
#endif  // DEBUG
    if (op.Properties().is_block_terminator) FinalizeBlock();
    return result;
  }

  // Adds {source} to the predecessors of {destination}.
  void AddPredecessor(Block* source, Block* destination, bool branch) {
    DCHECK_IMPLIES(branch, source->EndsWithBranchingOp(this->output_graph()));
    if (destination->LastPredecessor() == nullptr) {
      // {destination} has currently no predecessors.
      DCHECK(destination->IsLoopOrMerge());
      if (branch && destination->IsLoop()) {
        // We always split Branch edges that go to loop headers.
        SplitEdge(source, destination);
      } else {
        destination->AddPredecessor(source);
        if (branch) {
          DCHECK(!destination->IsLoop());
          destination->SetKind(Block::Kind::kBranchTarget);
        }
      }
      return;
    } else if (destination->IsBranchTarget()) {
      // {destination} used to be a BranchTarget, but branch targets can only
      // have one predecessor. We'll thus split its (single) incoming edge, and
      // change its type to kMerge.
      DCHECK_EQ(destination->PredecessorCount(), 1);
      Block* pred = destination->LastPredecessor();
      destination->ResetLastPredecessor();
      destination->SetKind(Block::Kind::kMerge);
      // It is important to add `source` first, because it was bound first.
      if (branch) {
        // A branch always goes to a BranchTarget. We thus split the edge: we'll
        // insert a new Block, to which {source} will branch, and which will
        // "Goto" to {destination}.
        SplitEdge(source, destination);
      } else {
        // {destination} is a Merge, and {source} just does a Goto; nothing
        // special to do.
        destination->AddPredecessor(source);
      }
      SplitEdge(pred, destination);
      return;
    }

    DCHECK(destination->IsLoopOrMerge());

    if (branch) {
      // A branch always goes to a BranchTarget. We thus split the edge: we'll
      // insert a new Block, to which {source} will branch, and which will
      // "Goto" to {destination}.
      SplitEdge(source, destination);
    } else {
      // {destination} is a Merge, and {source} just does a Goto; nothing
      // special to do.
      destination->AddPredecessor(source);
    }
  }

 private:
  void FinalizeBlock() {
    this->output_graph().Finalize(current_block_);
    current_block_ = nullptr;
  }

  // Insert a new Block between {source} and {destination}, in order to maintain
  // the split-edge form.
  void SplitEdge(Block* source, Block* destination) {
    DCHECK(source->EndsWithBranchingOp(this->output_graph()));
    // Creating the new intermediate block
    Block* intermediate_block = NewBlock();
    intermediate_block->SetKind(Block::Kind::kBranchTarget);
    // Updating "predecessor" edge of {intermediate_block}. This needs to be
    // done before calling Bind, because otherwise Bind will think that this
    // block is not reachable.
    intermediate_block->AddPredecessor(source);

    // Updating {source}'s last Branch/Switch/CallAndCatchException. Note that
    // this must be done before Binding {intermediate_block}, otherwise,
    // Reducer::Bind methods will see an invalid block being bound (because its
    // predecessor would be a branch, but none of its targets would be the block
    // being bound).
    Operation& op = this->output_graph().Get(
        this->output_graph().PreviousIndex(source->end()));
    switch (op.opcode) {
      case Opcode::kBranch: {
        BranchOp& branch = op.Cast<BranchOp>();
        if (branch.if_true == destination) {
          branch.if_true = intermediate_block;
          // We enforce that Branches if_false and if_true can never be the same
          // (there is a DCHECK in Assembler::Branch enforcing that).
          DCHECK_NE(branch.if_false, destination);
        } else {
          DCHECK_EQ(branch.if_false, destination);
          branch.if_false = intermediate_block;
        }
        break;
      }
      case Opcode::kCallAndCatchException: {
        CallAndCatchExceptionOp& catch_exception =
            op.Cast<CallAndCatchExceptionOp>();
        if (catch_exception.if_success == destination) {
          catch_exception.if_success = intermediate_block;
          // We enforce that CallAndCatchException's if_success and if_exception
          // can never be the same (there is a DCHECK in
          // Assembler::CallAndCatchException enforcing that).
          DCHECK_NE(catch_exception.if_exception, destination);
        } else {
          DCHECK_EQ(catch_exception.if_exception, destination);
          catch_exception.if_exception = intermediate_block;
        }
        break;
      }
      case Opcode::kSwitch: {
        SwitchOp& switch_op = op.Cast<SwitchOp>();
        bool found = false;
        for (auto case_block : switch_op.cases) {
          if (case_block.destination == destination) {
            case_block.destination = intermediate_block;
            DCHECK(!found);
            found = true;
#ifndef DEBUG
            break;
#endif
          }
        }
        DCHECK_IMPLIES(found, switch_op.default_case != destination);
        if (!found) {
          DCHECK_EQ(switch_op.default_case, destination);
          switch_op.default_case = intermediate_block;
        }
        break;
      }

      default:
        UNREACHABLE();
    }

    BindReachable(intermediate_block, source->Origin());
    // Inserting a Goto in {intermediate_block} to {destination}. This will
    // create the edge from {intermediate_block} to {destination}. Note that
    // this will call AddPredecessor, but we've already removed the eventual
    // edge of {destination} that need splitting, so no risks of inifinite
    // recursion here.
    this->Goto(destination);
  }

  Block* current_block_ = nullptr;
  bool generating_unreachable_operations_ = false;
  // TODO(dmercadier,tebbi): remove {current_operation_origin_} and pass instead
  // additional parameters to ReduceXXX methods.
  OpIndex current_operation_origin_ = OpIndex::Invalid();
#ifdef DEBUG
  GrowingSidetable<Block*> op_to_block_{this->phase_zone()};

  bool ValidInputs(OpIndex op_idx) {
    const Operation& op = this->output_graph().Get(op_idx);
    if (auto* phi = op.TryCast<PhiOp>()) {
      auto pred_blocks = current_block_->Predecessors();
      for (size_t i = 0; i < phi->input_count; ++i) {
        Block* input_block = op_to_block_[phi->input(i)];
        Block* pred_block = pred_blocks[i];
        if (input_block->GetCommonDominator(pred_block) != input_block) {
          std::cerr << "Input #" << phi->input(i).id()
                    << " does not dominate predecessor B"
                    << pred_block->index().id() << ".\n";
          std::cerr << op_idx.id() << ": " << op << "\n";
          return false;
        }
      }
    } else {
      for (OpIndex input : op.inputs()) {
        Block* input_block = op_to_block_[input];
        if (input_block->GetCommonDominator(current_block_) != input_block) {
          std::cerr << "Input #" << input.id()
                    << " does not dominate its use.\n";
          std::cerr << op_idx.id() << ": " << op << "\n";
          return false;
        }
      }
    }
    return true;
  }
#endif  // DEBUG
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
