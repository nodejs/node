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
#include "src/base/vector.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/reloc-info.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/globals.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/turboshaft/builtin-call-descriptors.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/optimization-phase.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/runtime-call-descriptors.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/logging/runtime-call-stats.h"
#include "src/objects/heap-number.h"
#include "src/objects/oddball.h"
#include "src/objects/tagged.h"
#include "src/objects/turbofan-types.h"

#ifdef V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif

namespace v8::internal {
enum class Builtin : int32_t;
}

namespace v8::internal::compiler::turboshaft {

// GotoIf(cond, dst) and GotoIfNot(cond, dst) are not guaranteed to actually
// generate a Branch with `dst` as one of the destination, because some reducer
// in the stack could realize that `cond` is statically known and optimize away
// the Branch. Thus, GotoIf and GotoIfNot return a {ConditionalGotoStatus},
// which represents whether a GotoIf/GotoIfNot was emitted as a Branch or a Goto
// (and if a Goto, then to what: `dst` or the fallthrough block).
enum ConditionalGotoStatus {
  kGotoDestination = 1,  // The conditional Goto became an unconditional Goto to
                         // the destination.
  kGotoEliminated = 2,   // The conditional GotoIf/GotoIfNot would never be
                         // executed and only the fallthrough path remains.
  kBranch = 3            // The conditional Goto became a branch.

  // Some examples of this:
  //   GotoIf(true, dst)     ===> kGotoDestination
  //   GotoIf(false, dst)    ===> kGotoEliminated
  //   GotoIf(var, dst)      ===> kBranch
  //   GotoIfNot(true, dst)  ===> kGotoEliminated
  //   GotoIfNot(false, dst) ===> kGotoDestination
  //   GotoIfNot(var, dst)   ===> kBranch
};
static_assert((ConditionalGotoStatus::kGotoDestination |
               ConditionalGotoStatus::kGotoEliminated) ==
              ConditionalGotoStatus::kBranch);

class ConditionWithHint final {
 public:
  ConditionWithHint(
      V<Word32> condition,
      BranchHint hint = BranchHint::kNone)  // NOLINT(runtime/explicit)
      : condition_(condition), hint_(hint) {}

  template <typename T, typename = std::enable_if_t<std::is_same_v<T, OpIndex>>>
  ConditionWithHint(
      T condition,
      BranchHint hint = BranchHint::kNone)  // NOLINT(runtime/explicit)
      : ConditionWithHint(V<Word32>{condition}, hint) {}

  V<Word32> condition() const { return condition_; }
  BranchHint hint() const { return hint_; }

 private:
  V<Word32> condition_;
  BranchHint hint_;
};

namespace detail {
template <typename T, typename = void>
struct has_constexpr_type : std::false_type {};

template <typename T>
struct has_constexpr_type<T, std::void_t<typename v_traits<T>::constexpr_type>>
    : std::true_type {};

template <typename T, typename...>
struct make_const_or_v {
  using type = V<T>;
};

template <typename T>
struct make_const_or_v<
    T, typename std::enable_if_t<has_constexpr_type<T>::value>> {
  using type = ConstOrV<T>;
};

template <typename T>
struct make_const_or_v<
    T, typename std::enable_if_t<!has_constexpr_type<T>::value>> {
  using type = V<T>;
};

template <typename T>
using make_const_or_v_t = typename make_const_or_v<T, void>::type;

template <typename A, typename ConstOrValues>
auto ResolveAll(A& assembler, const ConstOrValues& const_or_values) {
  return std::apply(
      [&](auto&... args) { return std::tuple{assembler.resolve(args)...}; },
      const_or_values);
}

inline bool SuppressUnusedWarning(bool b) { return b; }
}  // namespace detail

template <bool loop, typename... Ts>
class LabelBase {
 protected:
  static constexpr size_t size = sizeof...(Ts);

 public:
  static constexpr bool is_loop = loop;
  using values_t = std::tuple<V<Ts>...>;
  using const_or_values_t = std::tuple<detail::make_const_or_v_t<Ts>...>;
  using recorded_values_t = std::tuple<base::SmallVector<V<Ts>, 2>...>;

  Block* block() { return data_.block; }

  template <typename A>
  void Goto(A& assembler, const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    Block* current_block = assembler.current_block();
    DCHECK_NOT_NULL(current_block);
    assembler.Goto(data_.block);
    RecordValues(current_block, data_, values);
  }

  template <typename A>
  void GotoIf(A& assembler, OpIndex condition, BranchHint hint,
              const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    Block* current_block = assembler.current_block();
    DCHECK_NOT_NULL(current_block);
    if (assembler.GotoIf(condition, data_.block, hint) &
        ConditionalGotoStatus::kGotoDestination) {
      RecordValues(current_block, data_, values);
    }
  }

  template <typename A>
  void GotoIfNot(A& assembler, OpIndex condition, BranchHint hint,
                 const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    Block* current_block = assembler.current_block();
    DCHECK_NOT_NULL(current_block);
    if (assembler.GotoIfNot(condition, data_.block, hint) &
        ConditionalGotoStatus::kGotoDestination) {
      RecordValues(current_block, data_, values);
    }
  }

  template <typename A>
  base::prepend_tuple_type<bool, values_t> Bind(A& assembler) {
    DCHECK(!data_.block->IsBound());
    if (!assembler.Bind(data_.block)) {
      return std::tuple_cat(std::tuple{false}, values_t{});
    }
    DCHECK_EQ(data_.block, assembler.current_block());
    return std::tuple_cat(std::tuple{true}, MaterializePhis(assembler));
  }

 protected:
  struct BlockData {
    Block* block;
    base::SmallVector<Block*, 4> predecessors;
    recorded_values_t recorded_values;

    explicit BlockData(Block* block) : block(block) {}
  };

  explicit LabelBase(Block* block) : data_(block) {
    DCHECK_NOT_NULL(data_.block);
  }

  static void RecordValues(Block* source, BlockData& data,
                           const values_t& values) {
    DCHECK_NOT_NULL(source);
    if (data.block->IsBound()) {
      // Cannot `Goto` to a bound block. If you are trying to construct a
      // loop, use a `LoopLabel` instead!
      UNREACHABLE();
    }
    RecordValuesImpl(data, source, values, std::make_index_sequence<size>());
  }

  template <size_t... indices>
  static void RecordValuesImpl(BlockData& data, Block* source,
                               const values_t& values,
                               std::index_sequence<indices...>) {
#ifdef DEBUG
    std::initializer_list<size_t> sizes{
        std::get<indices>(data.recorded_values).size()...};
    // There a -1 on the PredecessorCounts below, because we've emitted the
    // Goto/Branch before calling RecordValues (which we do because the
    // condition of the Goto might have been constant-folded, resulting in the
    // destination not actually being reachable).
    DCHECK(base::all_equal(
        sizes, static_cast<size_t>(data.block->PredecessorCount() - 1)));
    DCHECK_EQ(data.block->PredecessorCount() - 1, data.predecessors.size());
#endif
    (std::get<indices>(data.recorded_values)
         .push_back(std::get<indices>(values)),
     ...);
    data.predecessors.push_back(source);
  }

  template <typename A>
  values_t MaterializePhis(A& assembler) {
    return MaterializePhisImpl(assembler, data_,
                               std::make_index_sequence<size>());
  }

  template <typename A, size_t... indices>
  static values_t MaterializePhisImpl(A& assembler, BlockData& data,
                                      std::index_sequence<indices...>) {
    size_t predecessor_count = data.block->PredecessorCount();
    DCHECK_EQ(data.predecessors.size(), predecessor_count);
    // If this label has no values, we don't need any Phis.
    if constexpr (size == 0) return values_t{};

    // If this block does not have any predecessors, we shouldn't call this.
    DCHECK_LT(0, predecessor_count);
    // With 1 predecessor, we don't need any Phis.
    if (predecessor_count == 1) {
      return values_t{std::get<indices>(data.recorded_values)[0]...};
    }
    DCHECK_LT(1, predecessor_count);

    // Construct Phis.
    return values_t{assembler.Phi(
        base::VectorOf(std::get<indices>(data.recorded_values)))...};
  }

  BlockData data_;
};

template <typename... Ts>
class Label : public LabelBase<false, Ts...> {
  using super = LabelBase<false, Ts...>;

 public:
  template <typename Reducer>
  explicit Label(Reducer* reducer) : super(reducer->Asm().NewBlock()) {}
};

template <typename... Ts>
class LoopLabel : public LabelBase<true, Ts...> {
  using super = LabelBase<true, Ts...>;
  using BlockData = typename super::BlockData;

 public:
  using values_t = typename super::values_t;
  template <typename Reducer>
  explicit LoopLabel(Reducer* reducer)
      : super(reducer->Asm().NewBlock()),
        loop_header_data_{reducer->Asm().NewLoopHeader()} {}

  Block* loop_header() const { return loop_header_data_.block; }

  template <typename A>
  void Goto(A& assembler, const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    if (!loop_header_data_.block->IsBound()) {
      // If the loop header is not bound yet, we have the forward edge to the
      // loop.
      DCHECK_EQ(0, loop_header_data_.block->PredecessorCount());
      Block* current_block = assembler.current_block();
      DCHECK_NOT_NULL(current_block);
      assembler.Goto(loop_header_data_.block);
      super::RecordValues(current_block, loop_header_data_, values);
    } else {
      // We have a jump back to the loop header and wire it to the single
      // backedge block.
      this->super::Goto(assembler, values);
    }
  }

  template <typename A>
  void GotoIf(A& assembler, OpIndex condition, BranchHint hint,
              const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    if (!loop_header_data_.block->IsBound()) {
      // If the loop header is not bound yet, we have the forward edge to the
      // loop.
      DCHECK_EQ(0, loop_header_data_.block->PredecessorCount());
      Block* current_block = assembler.current_block();
      DCHECK_NOT_NULL(current_block);
      if (assembler.GotoIf(condition, loop_header_data_.block, hint) &
          ConditionalGotoStatus::kGotoDestination) {
        super::RecordValues(current_block, loop_header_data_, values);
      }
    } else {
      // We have a jump back to the loop header and wire it to the single
      // backedge block.
      this->super::GotoIf(assembler, condition, hint, values);
    }
  }

  template <typename A>
  void GotoIfNot(A& assembler, OpIndex condition, BranchHint hint,
                 const values_t& values) {
    if (assembler.generating_unreachable_operations()) return;
    if (!loop_header_data_.block->IsBound()) {
      // If the loop header is not bound yet, we have the forward edge to the
      // loop.
      DCHECK_EQ(0, loop_header_data_.block->PredecessorCount());
      Block* current_block = assembler.current_block();
      DCHECK_NOT_NULL(current_block);
      if (assembler.GotoIf(condition, loop_header_data_.block, hint) &
          ConditionalGotoStatus::kGotoDestination) {
        super::RecordValues(current_block, loop_header_data_, values);
      }
    } else {
      // We have a jump back to the loop header and wire it to the single
      // backedge block.
      this->super::GotoIfNot(assembler, condition, hint, values);
    }
  }

  template <typename A>
  base::prepend_tuple_type<bool, values_t> Bind(A& assembler) {
    // LoopLabels must not be bound  using `Bind`, but with `Loop`.
    UNREACHABLE();
  }

  template <typename A>
  base::prepend_tuple_type<bool, values_t> BindLoop(A& assembler) {
    DCHECK(!loop_header_data_.block->IsBound());
    if (!assembler.Bind(loop_header_data_.block)) {
      return std::tuple_cat(std::tuple{false}, values_t{});
    }
    DCHECK_EQ(loop_header_data_.block, assembler.current_block());
    values_t pending_loop_phis =
        MaterializeLoopPhis(assembler, loop_header_data_);
    pending_loop_phis_ = pending_loop_phis;
    return std::tuple_cat(std::tuple{true}, pending_loop_phis);
  }

  template <typename A>
  void EndLoop(A& assembler) {
    // First, we need to bind the backedge block.
    auto bind_result = this->super::Bind(assembler);
    // `Bind` returns a tuple with a `bool` as first entry that indicates
    // whether the block was bound. The rest of the tuple contains the phi
    // values. Check if this block was bound (aka is reachable).
    if (std::get<0>(bind_result)) {
      // The block is bound.
      DCHECK_EQ(assembler.current_block(), this->super::block());
      // Now we build a jump from this block to the loop header.
      // Remove the "bound"-flag from the beginning of the tuple.
      auto values = base::tuple_drop<1>(bind_result);
      assembler.Goto(loop_header_data_.block);
      // Finalize Phis in the loop header.
      FixLoopPhis(assembler, values);
    }
    assembler.FinalizeLoop(loop_header_data_.block);
  }

 private:
  template <typename A>
  static values_t MaterializeLoopPhis(A& assembler, BlockData& data) {
    return MaterializeLoopPhisImpl(assembler, data,
                                   std::make_index_sequence<super::size>());
  }

  template <typename A, size_t... indices>
  static values_t MaterializeLoopPhisImpl(A& assembler, BlockData& data,
                                          std::index_sequence<indices...>) {
    size_t predecessor_count = data.block->PredecessorCount();
    USE(predecessor_count);
    DCHECK_EQ(data.predecessors.size(), predecessor_count);
    // If this label has no values, we don't need any Phis.
    if constexpr (super::size == 0) return typename super::values_t{};

    DCHECK_EQ(predecessor_count, 1);
    auto phis = typename super::values_t{assembler.PendingLoopPhi(
        std::get<indices>(data.recorded_values)[0])...};
    return phis;
  }

  template <typename A>
  void FixLoopPhis(A& assembler, const typename super::values_t& values) {
    DCHECK(loop_header_data_.block->IsBound());
    DCHECK(loop_header_data_.block->IsLoop());
    DCHECK_LE(1, loop_header_data_.predecessors.size());
    DCHECK_LE(loop_header_data_.predecessors.size(), 2);
    FixLoopPhi<0>(assembler, values);
  }

  template <size_t I, typename A>
  void FixLoopPhi(A& assembler, const typename super::values_t& values) {
    if constexpr (I < std::tuple_size_v<typename super::values_t>) {
      OpIndex phi_index = std::get<I>(*pending_loop_phis_);
      PendingLoopPhiOp& pending_loop_phi =
          assembler.output_graph()
              .Get(phi_index)
              .template Cast<PendingLoopPhiOp>();
      DCHECK_EQ(pending_loop_phi.first(),
                std::get<I>(loop_header_data_.recorded_values)[0]);
      assembler.output_graph().template Replace<PhiOp>(
          phi_index,
          base::VectorOf<OpIndex>(
              {pending_loop_phi.first(), std::get<I>(values)}),
          pending_loop_phi.rep);
      FixLoopPhi<I + 1>(assembler, values);
    }
  }

  BlockData loop_header_data_;
  base::Optional<values_t> pending_loop_phis_;
};

Handle<Code> BuiltinCodeHandle(Builtin builtin, Isolate* isolate);

template <typename Assembler>
class AssemblerOpInterface;

template <typename T>
class Uninitialized {
  static_assert(std::is_base_of_v<HeapObject, T>);

 public:
  explicit Uninitialized(V<T> object) : object_(object) {}

 private:
  template <typename Assembler>
  friend class AssemblerOpInterface;

  V<T> object() const {
    DCHECK(object_.has_value());
    return *object_;
  }

  V<T> ReleaseObject() {
    DCHECK(object_.has_value());
    auto temp = *object_;
    object_.reset();
    return temp;
  }

  base::Optional<V<T>> object_;
};

// Forward declarations
template <class Assembler>
class GraphVisitor;

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

#define TURBOSHAFT_REDUCER_BOILERPLATE()                \
  using ReducerList = typename Next::ReducerList;       \
  Assembler<ReducerList>& Asm() {                       \
    return *static_cast<Assembler<ReducerList>*>(this); \
  }                                                     \
  template <class T>                                    \
  using ScopedVar = turboshaft::ScopedVariable<T, Assembler<ReducerList>>;

template <class T, class Assembler>
class ScopedVariable : Variable {
 public:
  explicit ScopedVariable(Assembler& assembler)
      : Variable(assembler.NewVariable(
            static_cast<const RegisterRepresentation&>(V<T>::rep))),
        assembler_(assembler) {}
  ScopedVariable(Assembler& assembler, V<T> initial_value)
      : ScopedVariable(assembler) {
    assembler.SetVariable(*this, initial_value);
  }

  void operator=(V<T> new_value) { assembler_.SetVariable(*this, new_value); }
  V<T> operator*() const { return assembler_.GetVariable(*this); }
  ScopedVariable(const ScopedVariable&) = delete;
  ScopedVariable(ScopedVariable&&) = delete;
  ScopedVariable& operator=(const ScopedVariable) = delete;
  ScopedVariable& operator=(ScopedVariable&&) = delete;
  ~ScopedVariable() {
    // Explicitly mark the variable as invalid to avoid the creation of
    // unnecessary loop phis.
    assembler_.SetVariable(*this, OpIndex::Invalid());
  }

 private:
  Assembler& assembler_;
};

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
    return MayThrow(Opcode::k##Name)                                     \
               ? OpIndex::Invalid()                                      \
               : this->Asm().AssembleOutputGraph##Name(op);              \
  }                                                                      \
  template <class... Args>                                               \
  OpIndex Reduce##Name(Args... args) {                                   \
    return this->Asm().template Emit<Name##Op>(args...);                 \
  }
  TURBOSHAFT_OPERATION_LIST(EMIT_OP)
#undef EMIT_OP
};

// ReducerBase provides default implementations of Branch-related Operations
// (Goto, Branch, Switch, CheckException), and takes care of updating
// Block predecessors (and calls the Assembler to maintain split-edge form).
// ReducerBase is always added by Assembler at the bottom of the reducer stack.
template <class Next>
class ReducerBase : public ReducerBaseForwarder<Next> {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  using Base = ReducerBaseForwarder<Next>;

  void Bind(Block* block) {}

  void Analyze() {}

#ifdef DEBUG
  void Verify(OpIndex old_index, OpIndex new_index) {}
#endif  // DEBUG

  void RemoveLast(OpIndex index_of_last_operation) {
    Asm().output_graph().RemoveLast();
  }

  void FixLoopPhi(const PhiOp& input_phi, OpIndex output_index,
                  Block* output_graph_loop) {
    if (!Asm()
             .output_graph()
             .Get(output_index)
             .template Is<PendingLoopPhiOp>()) {
      return;
    }
#ifdef DEBUG
    DCHECK(output_graph_loop->Contains(output_index));
    auto& pending_phi = Asm()
                            .output_graph()
                            .Get(output_index)
                            .template Cast<PendingLoopPhiOp>();
    DCHECK_EQ(pending_phi.rep, input_phi.rep);
    DCHECK_EQ(pending_phi.first(), Asm().MapToNewGraph(input_phi.input(0)));
#endif
    Asm().output_graph().template Replace<PhiOp>(
        output_index,
        base::VectorOf({Asm().MapToNewGraph(input_phi.input(0)),
                        Asm().MapToNewGraph(input_phi.input(1))}),
        input_phi.rep);
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

  OpIndex ReduceCatchBlockBegin() {
    Block* current_block = Asm().current_block();
    if (current_block->IsBranchTarget()) {
      DCHECK_EQ(current_block->PredecessorCount(), 1);
      DCHECK_EQ(current_block->LastPredecessor()
                    ->LastOperation(Asm().output_graph())
                    .template Cast<CheckExceptionOp>()
                    .catch_block,
                current_block);
      return Base::ReduceCatchBlockBegin();
    }
    // We are trying to emit a CatchBlockBegin into a block that used to be the
    // catch_block successor but got edge-splitted into a merge. Therefore, we
    // need to emit a phi now and can rely on the predecessors all having a
    // ReduceCatchBlockBegin and nothing else.
    DCHECK(current_block->IsMerge());
    base::SmallVector<OpIndex, 8> phi_inputs;
    for (Block* predecessor : current_block->Predecessors()) {
      OpIndex catch_begin = predecessor->begin();
      DCHECK(Asm().Get(catch_begin).template Is<CatchBlockBeginOp>());
      phi_inputs.push_back(catch_begin);
    }
    return Asm().Phi(base::VectorOf(phi_inputs),
                     RegisterRepresentation::Tagged());
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

  OpIndex ReduceCall(OpIndex callee, OpIndex frame_state,
                     base::Vector<const OpIndex> arguments,
                     const TSCallDescriptor* descriptor, OpEffects effects) {
    OpIndex raw_call =
        Base::ReduceCall(callee, frame_state, arguments, descriptor, effects);
    bool has_catch_block = false;
    if (descriptor->can_throw == CanThrow::kYes) {
      has_catch_block = CatchIfInCatchScope(raw_call);
    }
    return ReduceDidntThrow(raw_call, has_catch_block, &descriptor->out_reps);
  }

 private:
  // These reduce functions are private, as they should only be emitted
  // automatically by `CatchIfInCatchScope` and `DoNotCatch` defined below and
  // never explicitly.
  using Base::ReduceDidntThrow;
  OpIndex ReduceCheckException(OpIndex throwing_operation, Block* successor,
                               Block* catch_block) {
    // {successor} and {catch_block} should never be the same.  AddPredecessor
    // and SplitEdge rely on this.
    DCHECK_NE(successor, catch_block);
    Block* saved_current_block = Asm().current_block();
    OpIndex new_opindex =
        Base::ReduceCheckException(throwing_operation, successor, catch_block);
    Asm().AddPredecessor(saved_current_block, successor, true);
    Asm().AddPredecessor(saved_current_block, catch_block, true);
    return new_opindex;
  }

  bool CatchIfInCatchScope(OpIndex throwing_operation) {
    if (Asm().current_catch_block()) {
      Block* successor = Asm().NewBlock();
      ReduceCheckException(throwing_operation, successor,
                           Asm().current_catch_block());
      Asm().BindReachable(successor);
      return true;
    }
    return false;
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
#define DECL_SINGLE_REP_BINOP_V(name, operation, kind, tag)         \
  V<tag> name(ConstOrV<tag> left, ConstOrV<tag> right) {            \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().Reduce##operation(resolve(left), resolve(right), \
                                     operation##Op::Kind::k##kind,  \
                                     V<tag>::rep);                  \
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
  DECL_SINGLE_REP_BINOP_V(WordPtrAdd, WordBinop, Add, WordPtr)
  DECL_SINGLE_REP_BINOP(PointerAdd, WordBinop, Add,
                        WordRepresentation::PointerSized())

  DECL_MULTI_REP_BINOP(WordMul, WordBinop, WordRepresentation, Mul)
  DECL_SINGLE_REP_BINOP_V(Word32Mul, WordBinop, Mul, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64Mul, WordBinop, Mul, Word64)
  DECL_SINGLE_REP_BINOP_V(WordPtrMul, WordBinop, Mul, WordPtr)

  DECL_MULTI_REP_BINOP(WordBitwiseAnd, WordBinop, WordRepresentation,
                       BitwiseAnd)
  DECL_SINGLE_REP_BINOP_V(Word32BitwiseAnd, WordBinop, BitwiseAnd, Word32)
  DECL_SINGLE_REP_BINOP_V(Word64BitwiseAnd, WordBinop, BitwiseAnd, Word64)
  DECL_SINGLE_REP_BINOP_V(WordPtrBitwiseAnd, WordBinop, BitwiseAnd, WordPtr)

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
  DECL_SINGLE_REP_BINOP_V(WordPtrSub, WordBinop, Sub, WordPtr)
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

  OpIndex OverflowCheckedBinop(OpIndex left, OpIndex right,
                               OverflowCheckedBinopOp::Kind kind,
                               WordRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceOverflowCheckedBinop(left, right, kind, rep);
  }
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

#define DECL_SINGLE_REP_SHIFT_V(name, kind, tag)                     \
  V<tag> name(ConstOrV<tag> left, ConstOrV<Word32> right) {          \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {  \
      return OpIndex::Invalid();                                     \
    }                                                                \
    return stack().ReduceShift(resolve(left), resolve(right),        \
                               ShiftOp::Kind::k##kind, V<tag>::rep); \
  }

  DECL_MULTI_REP_BINOP(ShiftRightArithmeticShiftOutZeros, Shift,
                       WordRepresentation, ShiftRightArithmeticShiftOutZeros)
  DECL_SINGLE_REP_SHIFT_V(Word32ShiftRightArithmeticShiftOutZeros,
                          ShiftRightArithmeticShiftOutZeros, Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64ShiftRightArithmeticShiftOutZeros,
                          ShiftRightArithmeticShiftOutZeros, Word64)
  DECL_SINGLE_REP_SHIFT_V(WordPtrShiftRightArithmeticShiftOutZeros,
                          ShiftRightArithmeticShiftOutZeros, WordPtr)
  DECL_MULTI_REP_BINOP(ShiftRightArithmetic, Shift, WordRepresentation,
                       ShiftRightArithmetic)
  DECL_SINGLE_REP_SHIFT_V(Word32ShiftRightArithmetic, ShiftRightArithmetic,
                          Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64ShiftRightArithmetic, ShiftRightArithmetic,
                          Word64)
  DECL_SINGLE_REP_SHIFT_V(WordPtrShiftRightArithmetic, ShiftRightArithmetic,
                          WordPtr)
  DECL_MULTI_REP_BINOP(ShiftRightLogical, Shift, WordRepresentation,
                       ShiftRightLogical)
  DECL_SINGLE_REP_SHIFT_V(Word32ShiftRightLogical, ShiftRightLogical, Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64ShiftRightLogical, ShiftRightLogical, Word64)
  DECL_SINGLE_REP_BINOP_V(WordPtrShiftRightLogical, Shift, ShiftRightLogical,
                          WordPtr)
  DECL_MULTI_REP_BINOP(ShiftLeft, Shift, WordRepresentation, ShiftLeft)
  DECL_SINGLE_REP_SHIFT_V(Word32ShiftLeft, ShiftLeft, Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64ShiftLeft, ShiftLeft, Word64)
  DECL_SINGLE_REP_SHIFT_V(WordPtrShiftLeft, ShiftLeft, WordPtr)
  DECL_MULTI_REP_BINOP(RotateRight, Shift, WordRepresentation, RotateRight)
  DECL_SINGLE_REP_SHIFT_V(Word32RotateRight, RotateRight, Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64RotateRight, RotateRight, Word64)
  DECL_MULTI_REP_BINOP(RotateLeft, Shift, WordRepresentation, RotateLeft)
  DECL_SINGLE_REP_SHIFT_V(Word32RotateLeft, RotateLeft, Word32)
  DECL_SINGLE_REP_SHIFT_V(Word64RotateLeft, RotateLeft, Word64)

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

#define DECL_SINGLE_REP_EQUAL_V(name, operation, tag)               \
  V<Word32> name(ConstOrV<tag> left, ConstOrV<tag> right) {         \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().Reduce##operation(resolve(left), resolve(right), \
                                     V<tag>::rep);                  \
  }
  DECL_SINGLE_REP_EQUAL_V(Word32Equal, Equal, Word32)
  DECL_SINGLE_REP_EQUAL_V(Word64Equal, Equal, Word64)
  DECL_SINGLE_REP_EQUAL_V(WordPtrEqual, Equal, WordPtr)
  DECL_SINGLE_REP_EQUAL_V(Float32Equal, Equal, Float32)
  DECL_SINGLE_REP_EQUAL_V(Float64Equal, Equal, Float64)
#undef DECL_SINGLE_REP_EQUAL_V

  DECL_SINGLE_REP_BINOP_NO_KIND(TaggedEqual, Equal,
                                RegisterRepresentation::Tagged())

#define DECL_SINGLE_REP_COMPARISON_V(name, operation, kind, tag)    \
  V<Word32> name(ConstOrV<tag> left, ConstOrV<tag> right) {         \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().Reduce##operation(resolve(left), resolve(right), \
                                     operation##Op::Kind::k##kind,  \
                                     V<tag>::rep);                  \
  }

  DECL_MULTI_REP_BINOP(IntLessThan, Comparison, RegisterRepresentation,
                       SignedLessThan)
  DECL_SINGLE_REP_COMPARISON_V(Int32LessThan, Comparison, SignedLessThan,
                               Word32)
  DECL_SINGLE_REP_COMPARISON_V(Int64LessThan, Comparison, SignedLessThan,
                               Word64)
  DECL_SINGLE_REP_COMPARISON_V(IntPtrLessThan, Comparison, SignedLessThan,
                               WordPtr)

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
#define DECL_SINGLE_REP_UNARY_V(name, operation, kind, tag)         \
  V<tag> name(ConstOrV<tag> input) {                                \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) { \
      return OpIndex::Invalid();                                    \
    }                                                               \
    return stack().Reduce##operation(                               \
        resolve(input), operation##Op::Kind::k##kind, V<tag>::rep); \
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

  V<Float64> BitcastWord32PairToFloat64(ConstOrV<Word32> high_word32,
                                        ConstOrV<Word32> low_word32) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceBitcastWord32PairToFloat64(resolve(high_word32),
                                                    resolve(low_word32));
  }

  OpIndex TaggedBitcast(OpIndex input, RegisterRepresentation from,
                        RegisterRepresentation to) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTaggedBitcast(input, from, to);
  }
  V<WordPtr> BitcastTaggedToWord(V<Object> tagged) {
    return TaggedBitcast(tagged, RegisterRepresentation::Tagged(),
                         RegisterRepresentation::PointerSized());
  }
  V<Object> BitcastWordPtrToTagged(V<WordPtr> word) {
    return TaggedBitcast(word, RegisterRepresentation::PointerSized(),
                         RegisterRepresentation::Tagged());
  }
  V<Object> BitcastWord32ToTagged(V<Word32> word) {
    return TaggedBitcast(word, RegisterRepresentation::Word32(),
                         RegisterRepresentation::Tagged());
  }

  V<Word32> ObjectIs(V<Object> input, ObjectIsOp::Kind kind,
                     ObjectIsOp::InputAssumptions input_assumptions) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceObjectIs(input, kind, input_assumptions);
  }
  V<Word32> ObjectIsSmi(V<Object> object) {
    return ObjectIs(object, ObjectIsOp::Kind::kSmi,
                    ObjectIsOp::InputAssumptions::kNone);
  }

  V<Word32> FloatIs(OpIndex input, NumericKind kind,
                    FloatRepresentation input_rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFloatIs(input, kind, input_rep);
  }
  V<Word32> Float64IsNaN(V<Float64> input) {
    return FloatIs(input, NumericKind::kNaN, FloatRepresentation::Float64());
  }

  OpIndex ObjectIsNumericValue(OpIndex input, NumericKind kind,
                               FloatRepresentation input_rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceObjectIsNumericValue(input, kind, input_rep);
  }

  V<Object> Convert(V<Object> input, ConvertOp::Kind from, ConvertOp::Kind to) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvert(input, from, to);
  }
  V<Number> ConvertPlainPrimitiveToNumber(V<PlainPrimitive> input) {
    return V<Number>::Cast(Convert(input, ConvertOp::Kind::kPlainPrimitive,
                                   ConvertOp::Kind::kNumber));
  }
  V<Boolean> ConvertToBoolean(V<Object> input) {
    return V<Boolean>::Cast(
        Convert(input, ConvertOp::Kind::kObject, ConvertOp::Kind::kBoolean));
  }
  V<String> ConvertNumberToString(V<Number> input) {
    return V<String>::Cast(
        Convert(input, ConvertOp::Kind::kNumber, ConvertOp::Kind::kString));
  }
  V<Number> ConvertStringToNumber(V<String> input) {
    return V<Number>::Cast(
        Convert(input, ConvertOp::Kind::kString, ConvertOp::Kind::kNumber));
  }

  V<Object> ConvertUntaggedToJSPrimitive(
      OpIndex input, ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind kind,
      RegisterRepresentation input_rep,
      ConvertUntaggedToJSPrimitiveOp::InputInterpretation input_interpretation,
      CheckForMinusZeroMode minus_zero_mode) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertUntaggedToJSPrimitive(
        input, kind, input_rep, input_interpretation, minus_zero_mode);
  }
#define CONVERT_PRIMITIVE_TO_OBJECT(name, kind, input_rep,               \
                                    input_interpretation)                \
  V<kind> name(V<input_rep> input) {                                     \
    return V<kind>::Cast(ConvertUntaggedToJSPrimitive(                   \
        input, ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::k##kind, \
        RegisterRepresentation::input_rep(),                             \
        ConvertUntaggedToJSPrimitiveOp::InputInterpretation::            \
            k##input_interpretation,                                     \
        CheckForMinusZeroMode::kDontCheckForMinusZero));                 \
  }
  CONVERT_PRIMITIVE_TO_OBJECT(ConvertInt32ToNumber, Number, Word32, Signed)
  CONVERT_PRIMITIVE_TO_OBJECT(ConvertUint32ToNumber, Number, Word32, Unsigned)
  CONVERT_PRIMITIVE_TO_OBJECT(ConvertWord32ToBoolean, Boolean, Word32, Signed)
#undef CONVERT_PRIMITIVE_TO_OBJECT
  V<Number> ConvertFloat64ToNumber(V<Float64> input,
                                   CheckForMinusZeroMode minus_zero_mode) {
    return V<Number>::Cast(ConvertUntaggedToJSPrimitive(
        input, ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kNumber,
        RegisterRepresentation::Float64(),
        ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned,
        minus_zero_mode));
  }

  OpIndex ConvertUntaggedToJSPrimitiveOrDeopt(
      OpIndex input, OpIndex frame_state,
      ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind kind,
      RegisterRepresentation input_rep,
      ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation
          input_interpretation,
      const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertUntaggedToJSPrimitiveOrDeopt(
        input, frame_state, kind, input_rep, input_interpretation, feedback);
  }

  OpIndex ConvertJSPrimitiveToUntagged(
      V<Object> object, ConvertJSPrimitiveToUntaggedOp::UntaggedKind kind,
      ConvertJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertJSPrimitiveToUntagged(object, kind,
                                                      input_assumptions);
  }

  OpIndex ConvertJSPrimitiveToUntaggedOrDeopt(
      V<Object> object, OpIndex frame_state,
      ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind from_kind,
      ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind to_kind,
      CheckForMinusZeroMode minus_zero_mode, const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertJSPrimitiveToUntaggedOrDeopt(
        object, frame_state, from_kind, to_kind, minus_zero_mode, feedback);
  }

  OpIndex TruncateJSPrimitiveToUntagged(
      V<Object> object, TruncateJSPrimitiveToUntaggedOp::UntaggedKind kind,
      TruncateJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTruncateJSPrimitiveToUntagged(object, kind,
                                                       input_assumptions);
  }

  OpIndex TruncateJSPrimitiveToUntaggedOrDeopt(
      V<Object> object, OpIndex frame_state,
      TruncateJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind kind,
      TruncateJSPrimitiveToUntaggedOrDeoptOp::InputRequirement
          input_requirement,
      const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceTruncateJSPrimitiveToUntaggedOrDeopt(
        object, frame_state, kind, input_requirement, feedback);
  }

  V<Object> ConvertJSPrimitiveToObject(V<Object> value, V<Object> global_proxy,
                                       ConvertReceiverMode mode) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConvertJSPrimitiveToObject(value, global_proxy, mode);
  }

  V<Word32> Word32Constant(uint32_t value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kWord32, uint64_t{value});
  }
  V<Word32> Word32Constant(int32_t value) {
    return Word32Constant(static_cast<uint32_t>(value));
  }
  V<Word64> Word64Constant(uint64_t value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kWord64, value);
  }
  V<Word64> Word64Constant(int64_t value) {
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
  V<WordPtr> IntPtrConstant(intptr_t value) {
    return UintPtrConstant(static_cast<uintptr_t>(value));
  }
  V<WordPtr> UintPtrConstant(uintptr_t value) {
    return WordConstant(static_cast<uint64_t>(value),
                        WordRepresentation::PointerSized());
  }
  V<Object> SmiConstant(i::Tagged<Smi> value) {
    return V<Smi>::Cast(UintPtrConstant(value.ptr()));
  }
  V<Float32> Float32Constant(float value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kFloat32, value);
  }
  V<Float64> Float64Constant(double value) {
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
  template <typename T,
            typename = std::enable_if_t<std::is_base_of_v<HeapObject, T>>>
  V<T> HeapConstant(Handle<T> value) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceConstant(ConstantOp::Kind::kHeapObject,
                                  ConstantOp::Storage{value});
  }
  V<Code> BuiltinCode(Builtin builtin, Isolate* isolate) {
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
  V<WordPtr> RelocatableConstant(int64_t value, RelocInfo::Mode mode) {
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
  V<Context> NoContextConstant() {
    return V<Context>::Cast(TagSmi(Context::kNoContext));
  }
  // TODO(nicohartmann@): Might want to get rid of the isolate when supporting
  // Wasm.
  V<Tagged> CEntryStubConstant(Isolate* isolate, int result_size,
                               ArgvMode argv_mode = ArgvMode::kStack,
                               bool builtin_exit_frame = false) {
    if (argv_mode != ArgvMode::kStack) {
      return HeapConstant(CodeFactory::CEntry(isolate, result_size, argv_mode,
                                              builtin_exit_frame));
    }

    DCHECK(result_size >= 1 && result_size <= 3);
    DCHECK_IMPLIES(builtin_exit_frame, result_size == 1);
    const int index = builtin_exit_frame ? 0 : result_size;
    if (cached_centry_stub_constants_[index].is_null()) {
      cached_centry_stub_constants_[index] = CodeFactory::CEntry(
          isolate, result_size, argv_mode, builtin_exit_frame);
    }
    return HeapConstant(cached_centry_stub_constants_[index].ToHandleChecked());
  }

#define DECL_CHANGE_V(name, kind, assumption, from, to)               \
  V<to> name(ConstOrV<from> input) {                                  \
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {   \
      return OpIndex::Invalid();                                      \
    }                                                                 \
    return stack().ReduceChange(resolve(input), ChangeOp::Kind::kind, \
                                ChangeOp::Assumption::assumption,     \
                                V<from>::rep, V<to>::rep);            \
  }
#define DECL_TRY_CHANGE_V(name, kind, from, to)                     \
  V<to> name(V<from> input) {                                       \
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
  DECL_CHANGE_V(TruncateWord64ToWord32, kTruncate, kNoAssumption, Word64,
                Word32)
  OpIndex ZeroExtendWord32ToRep(V<Word32> value, WordRepresentation rep) {
    if (rep == WordRepresentation::Word32()) return value;
    DCHECK_EQ(rep, WordRepresentation::Word64());
    return ChangeUint32ToUint64(value);
  }
  V<Word32> TruncateWordPtrToWord32(ConstOrV<WordPtr> input) {
    if constexpr (Is64()) {
      return TruncateWord64ToWord32(input);
    } else {
      DCHECK_EQ(WordPtr::bits, Word32::bits);
      return V<Word32>::Cast(resolve(input));
    }
  }
  V<WordPtr> ChangeInt32ToIntPtr(V<Word32> input) {
    if constexpr (Is64()) {
      return ChangeInt32ToInt64(input);
    } else {
      DCHECK_EQ(WordPtr::bits, Word32::bits);
      return V<WordPtr>::Cast(input);
    }
  }
  V<WordPtr> ChangeUint32ToUintPtr(V<Word32> input) {
    if constexpr (Is64()) {
      return ChangeUint32ToUint64(input);
    } else {
      DCHECK_EQ(WordPtr::bits, Word32::bits);
      return V<WordPtr>::Cast(input);
    }
  }

  V<Word64> ChangeIntPtrToInt64(V<WordPtr> input) {
    if constexpr (Is64()) {
      DCHECK_EQ(WordPtr::bits, Word64::bits);
      return V<Word64>::Cast(input);
    } else {
      return ChangeInt32ToInt64(input);
    }
  }

  V<Word64> ChangeUintPtrToUint64(V<WordPtr> input) {
    if constexpr (Is64()) {
      DCHECK_EQ(WordPtr::bits, Word64::bits);
      return V<Word64>::Cast(input);
    } else {
      return ChangeUint32ToUint64(input);
    }
  }

  V<Word32> IsSmi(V<Tagged> object) {
    if constexpr (COMPRESS_POINTERS_BOOL) {
      return Word32Equal(Word32BitwiseAnd(V<Word32>::Cast(object), kSmiTagMask),
                         kSmiTag);
    } else {
      return WordPtrEqual(
          WordPtrBitwiseAnd(V<WordPtr>::Cast(object), kSmiTagMask), kSmiTag);
    }
  }

#define DECL_SIGNED_FLOAT_TRUNCATE(FloatBits, ResultBits)                    \
  DECL_CHANGE_V(                                                             \
      TruncateFloat##FloatBits##ToInt##ResultBits##OverflowUndefined,        \
      kSignedFloatTruncateOverflowToMin, kNoOverflow, Float##FloatBits,      \
      Word##ResultBits)                                                      \
  DECL_TRY_CHANGE_V(TryTruncateFloat##FloatBits##ToInt##ResultBits,          \
                    kSignedFloatTruncateOverflowUndefined, Float##FloatBits, \
                    Word##ResultBits)

  DECL_SIGNED_FLOAT_TRUNCATE(64, 64)
  DECL_SIGNED_FLOAT_TRUNCATE(64, 32)
  DECL_SIGNED_FLOAT_TRUNCATE(32, 64)
  DECL_SIGNED_FLOAT_TRUNCATE(32, 32)
#undef DECL_SIGNED_FLOAT_TRUNCATE
  DECL_CHANGE_V(TruncateFloat64ToInt64OverflowToMin,
                kSignedFloatTruncateOverflowToMin, kNoAssumption, Float64,
                Word64)
  DECL_CHANGE_V(TruncateFloat32ToInt32OverflowToMin,
                kSignedFloatTruncateOverflowToMin, kNoAssumption, Float32,
                Word32)

#define DECL_UNSIGNED_FLOAT_TRUNCATE(FloatBits, ResultBits)                    \
  DECL_CHANGE_V(                                                               \
      TruncateFloat##FloatBits##ToUint##ResultBits##OverflowUndefined,         \
      kUnsignedFloatTruncateOverflowToMin, kNoOverflow, Float##FloatBits,      \
      Word##ResultBits)                                                        \
  DECL_CHANGE_V(TruncateFloat##FloatBits##ToUint##ResultBits##OverflowToMin,   \
                kUnsignedFloatTruncateOverflowToMin, kNoAssumption,            \
                Float##FloatBits, Word##ResultBits)                            \
  DECL_TRY_CHANGE_V(TryTruncateFloat##FloatBits##ToUint##ResultBits,           \
                    kUnsignedFloatTruncateOverflowUndefined, Float##FloatBits, \
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
#undef DECL_CHANGE_V
#undef DECL_TRY_CHANGE_V

  OpIndex ChangeOrDeopt(OpIndex input, OpIndex frame_state,
                        ChangeOrDeoptOp::Kind kind,
                        CheckForMinusZeroMode minus_zero_mode,
                        const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceChangeOrDeopt(input, frame_state, kind,
                                       minus_zero_mode, feedback);
  }

  V<Word32> ChangeFloat64ToInt32OrDeopt(V<Float64> input, OpIndex frame_state,
                                        CheckForMinusZeroMode minus_zero_mode,
                                        const FeedbackSource& feedback) {
    return ChangeOrDeopt(input, frame_state,
                         ChangeOrDeoptOp::Kind::kFloat64ToInt32,
                         minus_zero_mode, feedback);
  }
  V<Word64> ChangeFloat64ToInt64OrDeopt(V<Float64> input, OpIndex frame_state,
                                        CheckForMinusZeroMode minus_zero_mode,
                                        const FeedbackSource& feedback) {
    return ChangeOrDeopt(input, frame_state,
                         ChangeOrDeoptOp::Kind::kFloat64ToInt64,
                         minus_zero_mode, feedback);
  }

  V<Smi> TagSmi(ConstOrV<Word32> input) {
    constexpr int kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
    // Do shift on 32bit values if Smis are stored in the lower word.
    if constexpr (Is64() && SmiValuesAre31Bits()) {
      V<Word32> shifted = Word32ShiftLeft(resolve(input), kSmiShiftBits);
      // In pointer compression, we smi-corrupt. Then, the upper bits are not
      // important.
      return V<Smi>::Cast(
          COMPRESS_POINTERS_BOOL
              ? BitcastWord32ToTagged(shifted)
              : BitcastWordPtrToTagged(ChangeInt32ToIntPtr(shifted)));
    } else {
      return V<Smi>::Cast(BitcastWordPtrToTagged(WordPtrShiftLeft(
          ChangeInt32ToIntPtr(resolve(input)), kSmiShiftBits)));
    }
  }

  V<Word32> UntagSmi(V<Tagged> input) {
    constexpr int kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
    if constexpr (Is64() && SmiValuesAre31Bits()) {
      return Word32ShiftRightArithmeticShiftOutZeros(
          TruncateWordPtrToWord32(BitcastTaggedToWord(input)), kSmiShiftBits);
    }
    return TruncateWordPtrToWord32(WordPtrShiftRightArithmeticShiftOutZeros(
        BitcastTaggedToWord(input), kSmiShiftBits));
  }

  OpIndex AtomicRMW(V<WordPtr> base, V<WordPtr> index, OpIndex value,
                    AtomicRMWOp::BinOp bin_op,
                    RegisterRepresentation result_rep,
                    MemoryRepresentation input_rep,
                    MemoryAccessKind memory_access_kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceAtomicRMW(base, index, value, OpIndex::Invalid(),
                                   bin_op, result_rep, input_rep,
                                   memory_access_kind);
  }

  OpIndex AtomicCompareExchange(V<WordPtr> base, V<WordPtr> index,
                                OpIndex expected, OpIndex new_value,
                                RegisterRepresentation result_rep,
                                MemoryRepresentation input_rep,
                                MemoryAccessKind memory_access_kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceAtomicRMW(base, index, new_value, expected,
                                   AtomicRMWOp::BinOp::kCompareExchange,
                                   result_rep, input_rep, memory_access_kind);
  }

  OpIndex AtomicWord32Pair(V<WordPtr> base, V<WordPtr> index,
                           V<Word32> value_low, V<Word32> value_high,
                           V<Word32> expected_low, V<Word32> expected_high,
                           AtomicWord32PairOp::OpKind op_kind, int32_t offset) {
    return stack().ReduceAtomicWord32Pair(base, index, value_low, value_high,
                                          expected_low, expected_high, op_kind,
                                          offset);
  }

  OpIndex Load(OpIndex base, OpIndex index, LoadOp::Kind kind,
               MemoryRepresentation loaded_rep,
               RegisterRepresentation result_rep, int32_t offset = 0,
               uint8_t element_size_log2 = 0) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoad(base, index, kind, loaded_rep, result_rep, offset,
                              element_size_log2);
  }

  OpIndex Load(OpIndex base, OpIndex index, LoadOp::Kind kind,
               MemoryRepresentation loaded_rep, int32_t offset = 0,
               uint8_t element_size_log2 = 0) {
    return Load(base, index, kind, loaded_rep,
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
             int32_t offset = 0, uint8_t element_size_log2 = 0,
             bool maybe_initializing_or_transitioning = false) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStore(base, index, value, kind, stored_rep, write_barrier,
                        offset, element_size_log2,
                        maybe_initializing_or_transitioning);
  }
  void Store(OpIndex base, OpIndex value, StoreOp::Kind kind,
             MemoryRepresentation stored_rep, WriteBarrierKind write_barrier,
             int32_t offset = 0,
             bool maybe_initializing_or_transitioning = false) {
    Store(base, OpIndex::Invalid(), value, kind, stored_rep, write_barrier,
          offset, 0, maybe_initializing_or_transitioning);
  }

  template <typename T>
  void Initialize(Uninitialized<T>& object, OpIndex value,
                  MemoryRepresentation stored_rep,
                  WriteBarrierKind write_barrier, int32_t offset = 0) {
    return Store(object.object(), value,
                 StoreOp::Kind::Aligned(BaseTaggedness::kTaggedBase),
                 stored_rep, write_barrier, offset, true);
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

  template <typename Rep = Any, typename Base>
  V<Rep> LoadField(V<Base> object, const FieldAccess& access) {
    if constexpr (std::is_base_of_v<Object, Base>) {
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kTaggedBase);
    } else {
      static_assert(std::is_same_v<Base, WordPtr>);
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kUntaggedBase);
    }
    MachineType machine_type = access.machine_type;
    if (machine_type.IsMapWord()) {
      machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
      UNIMPLEMENTED();
#endif
    }
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(machine_type);
#ifdef V8_ENABLE_SANDBOX
    bool is_sandboxed_external =
        access.type.Is(compiler::Type::ExternalPointer());
    if (is_sandboxed_external) {
      // Fields for sandboxed external pointer contain a 32-bit handle, not a
      // 64-bit raw pointer.
      rep = MemoryRepresentation::Uint32();
    }
#endif  // V8_ENABLE_SANDBOX
    V<Rep> value = Load(object, LoadOp::Kind::Aligned(access.base_is_tagged),
                        rep, access.offset);
#ifdef V8_ENABLE_SANDBOX
    if (is_sandboxed_external) {
      value = DecodeExternalPointer(value, access.external_pointer_tag);
    }
    if (access.is_bounded_size_access) {
      DCHECK(!is_sandboxed_external);
      value = ShiftRightLogical(value, kBoundedSizeShift,
                                WordRepresentation::PointerSized());
    }
#endif  // V8_ENABLE_SANDBOX
    return value;
  }

  // Helpers to read the most common fields.
  // TODO(nicohartmann@): Strengthen this to `V<HeapObject>`.
  V<Map> LoadMapField(V<Object> object) {
    return LoadField<Map>(object, AccessBuilder::ForMap());
  }
  V<Word32> LoadInstanceTypeField(V<Map> map) {
    return LoadField<Word32>(map, AccessBuilder::ForMapInstanceType());
  }

  template <typename Base>
  void StoreField(V<Base> object, const FieldAccess& access, V<Any> value) {
    StoreFieldImpl(object, access, value,
                   access.maybe_initializing_or_transitioning_store);
  }

  template <typename T>
  void InitializeField(Uninitialized<T>& object, const FieldAccess& access,
                       V<Any> value) {
    StoreFieldImpl(object.object(), access, value, true);
  }

  template <typename Base>
  void StoreFieldImpl(V<Base> object, const FieldAccess& access, V<Any> value,
                      bool maybe_initializing_or_transitioning) {
    if constexpr (std::is_base_of_v<Object, Base>) {
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kTaggedBase);
    } else {
      static_assert(std::is_same_v<Base, WordPtr>);
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kUntaggedBase);
    }
    // External pointer must never be stored by optimized code.
    DCHECK(!access.type.Is(compiler::Type::ExternalPointer()) ||
           !V8_ENABLE_SANDBOX_BOOL);
    // SandboxedPointers are not currently stored by optimized code.
    DCHECK(!access.type.Is(compiler::Type::SandboxedPointer()));

#ifdef V8_ENABLE_SANDBOX
    if (access.is_bounded_size_access) {
      value = ShiftLeft(value, kBoundedSizeShift,
                        WordRepresentation::PointerSized());
    }
#endif  // V8_ENABLE_SANDBOX

    StoreOp::Kind kind = StoreOp::Kind::Aligned(access.base_is_tagged);
    MachineType machine_type = access.machine_type;
    if (machine_type.IsMapWord()) {
      machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
      UNIMPLEMENTED();
#endif
    }
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(machine_type);
    Store(object, value, kind, rep, access.write_barrier_kind, access.offset,
          maybe_initializing_or_transitioning);
  }

  template <typename T = Any, typename Base>
  V<T> LoadArrayBufferElement(V<Base> object, const ElementAccess& access,
                              V<WordPtr> index) {
    return LoadElement<T>(object, access, index, true);
  }
  template <typename T = Any, typename Base>
  V<T> LoadNonArrayBufferElement(V<Base> object, const ElementAccess& access,
                                 V<WordPtr> index) {
    return LoadElement<T>(object, access, index, false);
  }

  template <typename Base>
  void StoreArrayBufferElement(V<Base> object, const ElementAccess& access,
                               V<WordPtr> index, V<Any> value) {
    return StoreElement(object, access, index, value, true);
  }
  template <typename Base>
  void StoreNonArrayBufferElement(V<Base> object, const ElementAccess& access,
                                  V<WordPtr> index, V<Any> value) {
    return StoreElement(object, access, index, value, false);
  }

  template <typename T = HeapObject>
  Uninitialized<T> Allocate(ConstOrV<WordPtr> size, AllocationType type) {
    static_assert(std::is_base_of_v<HeapObject, T>);
    DCHECK(!in_object_initialization_);
    in_object_initialization_ = true;
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return Uninitialized<T>(OpIndex::Invalid());
    }
    return Uninitialized<T>{stack().ReduceAllocate(resolve(size), type)};
  }

  template <typename T>
  V<T> FinishInitialization(Uninitialized<T>&& uninitialized) {
    DCHECK(in_object_initialization_);
    in_object_initialization_ = false;
    return uninitialized.ReleaseObject();
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

  V<WordPtr> StackSlot(int size, int alignment) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStackSlot(size, alignment);
  }

  OpIndex LoadRootRegister() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadRootRegister();
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
  void Branch(ConditionWithHint condition, Block* if_true, Block* if_false) {
    return Branch(condition.condition(), if_true, if_false, condition.hint());
  }
  OpIndex Select(OpIndex cond, OpIndex vtrue, OpIndex vfalse,
                 RegisterRepresentation rep, BranchHint hint,
                 SelectOp::Implementation implem) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSelect(cond, vtrue, vfalse, rep, hint, implem);
  }
  template <typename T, typename U>
  V<std::common_type_t<T, U>> Conditional(V<Word32> cond, V<T> vtrue,
                                          V<U> vfalse,
                                          BranchHint hint = BranchHint::kNone) {
    return Select(cond, vtrue, vfalse, V<std::common_type_t<T, U>>::rep, hint,
                  SelectOp::Implementation::kBranch);
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
  void Return(OpIndex pop_count, base::Vector<const OpIndex> return_values) {
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
               const TSCallDescriptor* descriptor,
               OpEffects effects = OpEffects().CanCallAnything()) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCall(callee, frame_state, arguments, descriptor,
                              effects);
  }
  OpIndex Call(OpIndex callee, std::initializer_list<OpIndex> arguments,
               const TSCallDescriptor* descriptor,
               OpEffects effects = OpEffects().CanCallAnything()) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return Call(callee, OpIndex::Invalid(), base::VectorOf(arguments),
                descriptor, effects);
  }

  template <typename Descriptor>
  std::enable_if_t<Descriptor::NeedsFrameState && Descriptor::NeedsContext,
                   typename Descriptor::result_t>
  CallBuiltin(Isolate* isolate, OpIndex frame_state, OpIndex context,
              const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    DCHECK(frame_state.valid());
    DCHECK(context.valid());
    return CallBuiltinImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(isolate, stack().output_graph().graph_zone()),
        Descriptor::Effects, frame_state, context, args);
  }
  template <typename Descriptor>
  std::enable_if_t<!Descriptor::NeedsFrameState && Descriptor::NeedsContext,
                   typename Descriptor::result_t>
  CallBuiltin(Isolate* isolate, OpIndex context,
              const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    DCHECK(context.valid());
    return CallBuiltinImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(isolate, stack().output_graph().graph_zone()),
        Descriptor::Effects, {}, context, args);
  }
  template <typename Descriptor>
  std::enable_if_t<Descriptor::NeedsFrameState && !Descriptor::NeedsContext,
                   typename Descriptor::result_t>
  CallBuiltin(Isolate* isolate, OpIndex frame_state,
              const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    DCHECK(frame_state.valid());
    return CallBuiltinImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(isolate, stack().output_graph().graph_zone()),
        Descriptor::Effects, frame_state, {}, args);
  }
  template <typename Descriptor>
  std::enable_if_t<!Descriptor::NeedsFrameState && !Descriptor::NeedsContext,
                   typename Descriptor::result_t>
  CallBuiltin(Isolate* isolate, const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return CallBuiltinImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(isolate, stack().output_graph().graph_zone()),
        Descriptor::Effects, {}, {}, args);
  }

  template <typename Ret, typename Args>
  Ret CallBuiltinImpl(Isolate* isolate, Builtin function,
                      const TSCallDescriptor* desc, OpEffects effects,
                      OpIndex frame_state, V<Context> context,
                      const Args& args) {
    Callable callable = Builtins::CallableFor(isolate, function);
    // Convert arguments from `args` tuple into a `SmallVector<OpIndex>`.
    auto inputs = std::apply(
        [](auto&&... as) {
          return base::SmallVector<OpIndex, std::tuple_size_v<Args> + 1>{
              std::forward<decltype(as)>(as)...};
        },
        args);
    if (context.valid()) inputs.push_back(context);

    if constexpr (std::is_same_v<Ret, void>) {
      Call(HeapConstant(callable.code()), frame_state, base::VectorOf(inputs),
           desc, effects);
    } else {
      return Call(HeapConstant(callable.code()), frame_state,
                  base::VectorOf(inputs), desc, effects);
    }
  }

  void CallBuiltin_CheckTurbofanType(Isolate* isolate, V<Context> context,
                                     V<Object> object,
                                     V<TurbofanType> allocated_type,
                                     V<Smi> node_id) {
    CallBuiltin<typename BuiltinCallDescriptor::CheckTurbofanType>(
        isolate, context, {object, allocated_type, node_id});
  }
  V<Object> CallBuiltin_CopyFastSmiOrObjectElements(Isolate* isolate,
                                                    V<Object> object) {
    return CallBuiltin<
        typename BuiltinCallDescriptor::CopyFastSmiOrObjectElements>(isolate,
                                                                     {object});
  }
  void CallBuiltin_DebugPrintFloat64(Isolate* isolate, V<Context> context,
                                     V<Float64> value) {
    CallBuiltin<typename BuiltinCallDescriptor::DebugPrintFloat64>(
        isolate, context, {value});
  }
  void CallBuiltin_DebugPrintWordPtr(Isolate* isolate, V<Context> context,
                                     V<WordPtr> value) {
    CallBuiltin<typename BuiltinCallDescriptor::DebugPrintWordPtr>(
        isolate, context, {value});
  }
  V<Smi> CallBuiltin_FindOrderedHashMapEntry(Isolate* isolate,
                                             V<Context> context,
                                             V<Object> table, V<Smi> key) {
    return CallBuiltin<typename BuiltinCallDescriptor::FindOrderedHashMapEntry>(
        isolate, context, {table, key});
  }
  V<Smi> CallBuiltin_FindOrderedHashSetEntry(Isolate* isolate,
                                             V<Context> context, V<Object> set,
                                             V<Smi> key) {
    return CallBuiltin<typename BuiltinCallDescriptor::FindOrderedHashSetEntry>(
        isolate, context, {set, key});
  }
  V<Object> CallBuiltin_GrowFastDoubleElements(Isolate* isolate,
                                               V<Object> object, V<Smi> size) {
    return CallBuiltin<typename BuiltinCallDescriptor::GrowFastDoubleElements>(
        isolate, {object, size});
  }
  V<Object> CallBuiltin_GrowFastSmiOrObjectElements(Isolate* isolate,
                                                    V<Object> object,
                                                    V<Smi> size) {
    return CallBuiltin<
        typename BuiltinCallDescriptor::GrowFastSmiOrObjectElements>(
        isolate, {object, size});
  }
  V<FixedArray> CallBuiltin_NewSloppyArgumentsElements(
      Isolate* isolate, V<WordPtr> frame, V<WordPtr> formal_parameter_count,
      V<Smi> arguments_count) {
    return CallBuiltin<
        typename BuiltinCallDescriptor::NewSloppyArgumentsElements>(
        isolate, {frame, formal_parameter_count, arguments_count});
  }
  V<FixedArray> CallBuiltin_NewStrictArgumentsElements(
      Isolate* isolate, V<WordPtr> frame, V<WordPtr> formal_parameter_count,
      V<Smi> arguments_count) {
    return CallBuiltin<
        typename BuiltinCallDescriptor::NewStrictArgumentsElements>(
        isolate, {frame, formal_parameter_count, arguments_count});
  }
  V<FixedArray> CallBuiltin_NewRestArgumentsElements(
      Isolate* isolate, V<WordPtr> frame, V<WordPtr> formal_parameter_count,
      V<Smi> arguments_count) {
    return CallBuiltin<
        typename BuiltinCallDescriptor::NewRestArgumentsElements>(
        isolate, {frame, formal_parameter_count, arguments_count});
  }
  V<String> CallBuiltin_NumberToString(Isolate* isolate, V<Number> input) {
    return CallBuiltin<typename BuiltinCallDescriptor::NumberToString>(isolate,
                                                                       {input});
  }
  V<Number> CallBuiltin_PlainPrimitiveToNumber(Isolate* isolate,
                                               V<PlainPrimitive> input) {
    return CallBuiltin<typename BuiltinCallDescriptor::PlainPrimitiveToNumber>(
        isolate, {input});
  }
  V<Boolean> CallBuiltin_SameValue(Isolate* isolate, V<Object> left,
                                   V<Object> right) {
    return CallBuiltin<typename BuiltinCallDescriptor::SameValue>(
        isolate, {left, right});
  }
  V<Boolean> CallBuiltin_SameValueNumbersOnly(Isolate* isolate, V<Object> left,
                                              V<Object> right) {
    return CallBuiltin<typename BuiltinCallDescriptor::SameValueNumbersOnly>(
        isolate, {left, right});
  }
  V<String> CallBuiltin_StringAdd_CheckNone(Isolate* isolate,
                                            V<Context> context, V<String> left,
                                            V<String> right) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringAdd_CheckNone>(
        isolate, context, {left, right});
  }
  V<Boolean> CallBuiltin_StringEqual(Isolate* isolate, V<String> left,
                                     V<String> right, V<WordPtr> length) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringEqual>(
        isolate, {left, right, length});
  }
  V<Boolean> CallBuiltin_StringLessThan(Isolate* isolate, V<String> left,
                                        V<String> right) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringLessThan>(
        isolate, {left, right});
  }
  V<Boolean> CallBuiltin_StringLessThanOrEqual(Isolate* isolate, V<String> left,
                                               V<String> right) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringLessThanOrEqual>(
        isolate, {left, right});
  }
  V<Smi> CallBuiltin_StringIndexOf(Isolate* isolate, V<String> string,
                                   V<String> search, V<Smi> position) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringIndexOf>(
        isolate, {string, search, position});
  }
  V<String> CallBuiltin_StringFromCodePointAt(Isolate* isolate,
                                              V<String> string,
                                              V<WordPtr> index) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringFromCodePointAt>(
        isolate, {string, index});
  }
#ifdef V8_INTL_SUPPORT
  V<String> CallBuiltin_StringToLowerCaseIntl(Isolate* isolate,
                                              V<Context> context,
                                              V<String> string) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringToLowerCaseIntl>(
        isolate, context, {string});
  }
#endif  // V8_INTL_SUPPORT
  V<Number> CallBuiltin_StringToNumber(Isolate* isolate, V<String> input) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringToNumber>(isolate,
                                                                       {input});
  }
  V<String> CallBuiltin_StringSubstring(Isolate* isolate, V<String> string,
                                        V<WordPtr> start, V<WordPtr> end) {
    return CallBuiltin<typename BuiltinCallDescriptor::StringSubstring>(
        isolate, {string, start, end});
  }
  V<Boolean> CallBuiltin_ToBoolean(Isolate* isolate, V<Object> object) {
    return CallBuiltin<typename BuiltinCallDescriptor::ToBoolean>(isolate,
                                                                  {object});
  }
  V<Object> CallBuiltin_ToObject(Isolate* isolate, V<Context> context,
                                 V<Object> object) {
    return CallBuiltin<typename BuiltinCallDescriptor::ToObject>(
        isolate, context, {object});
  }
  V<String> CallBuiltin_Typeof(Isolate* isolate, V<Object> object) {
    return CallBuiltin<typename BuiltinCallDescriptor::Typeof>(isolate,
                                                               {object});
  }

  template <typename Descriptor>
  std::enable_if_t<Descriptor::NeedsFrameState, typename Descriptor::result_t>
  CallRuntime(Isolate* isolate, OpIndex frame_state, OpIndex context,
              const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    DCHECK(frame_state.valid());
    DCHECK(context.valid());
    return CallRuntimeImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(stack().output_graph().graph_zone()), frame_state,
        context, args);
  }
  template <typename Descriptor>
  std::enable_if_t<!Descriptor::NeedsFrameState, typename Descriptor::result_t>
  CallRuntime(Isolate* isolate, OpIndex context,
              const typename Descriptor::arguments_t& args) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    DCHECK(context.valid());
    return CallRuntimeImpl<typename Descriptor::result_t>(
        isolate, Descriptor::Function,
        Descriptor::Create(stack().output_graph().graph_zone()), {}, context,
        args);
  }

  template <typename Ret, typename Args>
  Ret CallRuntimeImpl(Isolate* isolate, Runtime::FunctionId function,
                      const TSCallDescriptor* desc, OpIndex frame_state,
                      OpIndex context, const Args& args) {
    const int result_size = Runtime::FunctionForId(function)->result_size;
    constexpr size_t kMaxNumArgs = 6;
    const size_t argc = std::tuple_size_v<Args>;
    static_assert(kMaxNumArgs >= argc);
    // Convert arguments from `args` tuple into a `SmallVector<OpIndex>`.
    using vector_t = base::SmallVector<OpIndex, argc + 4>;
    auto inputs = std::apply(
        [](auto&&... as) {
          return vector_t{std::forward<decltype(as)>(as)...};
        },
        args);
    DCHECK(context.valid());
    inputs.push_back(ExternalConstant(ExternalReference::Create(function)));
    inputs.push_back(Word32Constant(static_cast<int>(argc)));
    inputs.push_back(context);

    if constexpr (std::is_same_v<Ret, void>) {
      Call(CEntryStubConstant(isolate, result_size), frame_state,
           base::VectorOf(inputs), desc);
    } else {
      return Call(CEntryStubConstant(isolate, result_size), frame_state,
                  base::VectorOf(inputs), desc);
    }
  }

  void CallRuntime_Abort(Isolate* isolate, V<Context> context, V<Smi> reason) {
    CallRuntime<typename RuntimeCallDescriptor::Abort>(isolate, context,
                                                       {reason});
  }
  V<Number> CallRuntime_DateCurrentTime(Isolate* isolate, V<Context> context) {
    return CallRuntime<typename RuntimeCallDescriptor::DateCurrentTime>(
        isolate, context, {});
  }
  V<Tagged> CallRuntime_StringCharCodeAt(Isolate* isolate, V<Context> context,
                                         V<String> string, V<Number> index) {
    return CallRuntime<typename RuntimeCallDescriptor::StringCharCodeAt>(
        isolate, context, {string, index});
  }
#ifdef V8_INTL_SUPPORT
  V<String> CallRuntime_StringToUpperCaseIntl(Isolate* isolate,
                                              V<Context> context,
                                              V<String> string) {
    return CallRuntime<typename RuntimeCallDescriptor::StringToUpperCaseIntl>(
        isolate, context, {string});
  }
#endif  // V8_INTL_SUPPORT
  V<Tagged> CallRuntime_TerminateExecution(Isolate* isolate,
                                           OpIndex frame_state,
                                           V<Context> context) {
    return CallRuntime<typename RuntimeCallDescriptor::TerminateExecution>(
        isolate, frame_state, context, {});
  }
  V<Object> CallRuntime_TransitionElementsKind(Isolate* isolate,
                                               V<Context> context,
                                               V<HeapObject> object,
                                               V<Map> target_map) {
    return CallRuntime<typename RuntimeCallDescriptor::TransitionElementsKind>(
        isolate, context, {object, target_map});
  }
  V<Object> CallRuntime_TryMigrateInstance(Isolate* isolate, V<Context> context,
                                           V<HeapObject> heap_object) {
    return CallRuntime<typename RuntimeCallDescriptor::TryMigrateInstance>(
        isolate, context, {heap_object});
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
    if (stack().current_block() == nullptr) {
      // The DeoptimizeIf was transformed into an inconditional deopt
      stack().SetGeneratingUnreachableOperations();
    }
  }
  void DeoptimizeIfNot(OpIndex condition, OpIndex frame_state,
                       const DeoptimizeParameters* parameters) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDeoptimizeIf(condition, frame_state, true, parameters);
    if (stack().current_block() == nullptr) {
      // The DeoptimizeIfNot was transformed into an inconditional deopt
      stack().SetGeneratingUnreachableOperations();
    }
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
  void Deoptimize(OpIndex frame_state, DeoptimizeReason reason,
                  const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    Zone* zone = stack().output_graph().graph_zone();
    const DeoptimizeParameters* params =
        zone->New<DeoptimizeParameters>(reason, feedback);
    Deoptimize(frame_state, params);
  }

  void TrapIf(V<Word32> condition, OpIndex frame_state, TrapId trap_id) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTrapIf(condition, frame_state, false, trap_id);
    if (stack().current_block() == nullptr) {
      // The TrapIf was transformed into an inconditional trap
      stack().SetGeneratingUnreachableOperations();
    }
  }
  void TrapIfNot(V<Word32> condition, OpIndex frame_state, TrapId trap_id) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTrapIf(condition, frame_state, true, trap_id);
    if (stack().current_block() == nullptr) {
      // The TrapIfNot was transformed into an inconditional trap
      stack().SetGeneratingUnreachableOperations();
    }
  }

  void StaticAssert(OpIndex condition, const char* source) {
    CHECK(v8_flags.turboshaft_enable_debug_features);
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
  template <typename T>
  V<T> Phi(const base::Vector<V<T>>& inputs) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    std::vector<OpIndex> temp(inputs.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) temp[i] = inputs[i];
    return Phi(base::VectorOf(temp), V<T>::rep);
  }
  OpIndex PendingLoopPhi(OpIndex first, RegisterRepresentation rep) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReducePendingLoopPhi(first, rep);
  }
  template <typename T>
  V<T> PendingLoopPhi(V<T> first) {
    return PendingLoopPhi(first, V<T>::rep);
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
  template <typename T>
  V<T> Projection(OpIndex tuple, uint16_t index) {
    return Projection(tuple, index, V<T>::rep);
  }
  OpIndex CheckTurboshaftTypeOf(OpIndex input, RegisterRepresentation rep,
                                Type expected_type, bool successful) {
    CHECK(v8_flags.turboshaft_enable_debug_features);
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCheckTurboshaftTypeOf(input, rep, expected_type,
                                               successful);
  }

  OpIndex CatchBlockBegin() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCatchBlockBegin();
  }

  // Return `true` if the control flow after the conditional jump is reachable.
  ConditionalGotoStatus GotoIf(OpIndex condition, Block* if_true,
                               BranchHint hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      // What we return here should not matter.
      return ConditionalGotoStatus::kBranch;
    }
    Block* if_false = stack().NewBlock();
    return BranchAndBind(condition, if_true, if_false, hint, if_false);
  }
  ConditionalGotoStatus GotoIf(ConditionWithHint condition, Block* if_true) {
    return GotoIf(condition.condition(), if_true, condition.hint());
  }
  // Return `true` if the control flow after the conditional jump is reachable.
  ConditionalGotoStatus GotoIfNot(OpIndex condition, Block* if_false,
                                  BranchHint hint = BranchHint::kNone) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      // What we return here should not matter.
      return ConditionalGotoStatus::kBranch;
    }
    Block* if_true = stack().NewBlock();
    return BranchAndBind(condition, if_true, if_false, hint, if_true);
  }

  ConditionalGotoStatus GotoIfNot(ConditionWithHint condition,
                                  Block* if_false) {
    return GotoIfNot(condition.condition(), if_false, condition.hint());
  }

  OpIndex CallBuiltin(Builtin builtin, OpIndex frame_state,
                      base::Vector<OpIndex> arguments, CanThrow can_throw,
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
        TSCallDescriptor::Create(call_descriptor, can_throw, graph_zone);

    OpIndex callee = stack().HeapConstant(callable.code());

    return stack().Call(callee, frame_state, arguments, ts_call_descriptor);
  }

  V<Tagged> NewConsString(V<Word32> length, V<Tagged> first, V<Tagged> second) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceNewConsString(length, first, second);
  }
  V<Tagged> NewArray(V<WordPtr> length, NewArrayOp::Kind kind,
                     AllocationType allocation_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceNewArray(length, kind, allocation_type);
  }
  V<Tagged> NewDoubleArray(V<WordPtr> length, AllocationType allocation_type) {
    return NewArray(length, NewArrayOp::Kind::kDouble, allocation_type);
  }

  V<Tagged> DoubleArrayMinMax(V<Tagged> array, DoubleArrayMinMaxOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceDoubleArrayMinMax(array, kind);
  }
  V<Tagged> DoubleArrayMin(V<Tagged> array) {
    return DoubleArrayMinMax(array, DoubleArrayMinMaxOp::Kind::kMin);
  }
  V<Tagged> DoubleArrayMax(V<Tagged> array) {
    return DoubleArrayMinMax(array, DoubleArrayMinMaxOp::Kind::kMax);
  }

  V<Any> LoadFieldByIndex(V<Tagged> object, V<Word32> index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadFieldByIndex(object, index);
  }

  void DebugBreak() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDebugBreak();
  }

  void DebugPrint(OpIndex input, RegisterRepresentation rep) {
    CHECK(v8_flags.turboshaft_enable_debug_features);
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceDebugPrint(input, rep);
  }
  void DebugPrint(V<WordPtr> input) {
    return DebugPrint(input, RegisterRepresentation::PointerSized());
  }
  void DebugPrint(V<Float64> input) {
    return DebugPrint(input, RegisterRepresentation::Float64());
  }

  V<Tagged> BigIntBinop(V<Tagged> left, V<Tagged> right, OpIndex frame_state,
                        BigIntBinopOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceBigIntBinop(left, right, frame_state, kind);
  }
#define BIGINT_BINOP(kind)                                \
  V<Tagged> BigInt##kind(V<Tagged> left, V<Tagged> right, \
                         OpIndex frame_state) {           \
    return BigIntBinop(left, right, frame_state,          \
                       BigIntBinopOp::Kind::k##kind);     \
  }
  BIGINT_BINOP(Add)
  BIGINT_BINOP(Sub)
  BIGINT_BINOP(Mul)
  BIGINT_BINOP(Div)
  BIGINT_BINOP(Mod)
  BIGINT_BINOP(BitwiseAnd)
  BIGINT_BINOP(BitwiseOr)
  BIGINT_BINOP(BitwiseXor)
  BIGINT_BINOP(ShiftLeft)
  BIGINT_BINOP(ShiftRightArithmetic)
#undef BIGINT_BINOP

  V<Word32> BigIntEqual(V<Tagged> left, V<Tagged> right) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceBigIntEqual(left, right);
  }

  V<Word32> BigIntComparison(V<Tagged> left, V<Tagged> right,
                             BigIntComparisonOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceBigIntComparison(left, right, kind);
  }
  V<Word32> BigIntLessThan(V<Tagged> left, V<Tagged> right) {
    return BigIntComparison(left, right, BigIntComparisonOp::Kind::kLessThan);
  }
  V<Word32> BigIntLessThanOrEqual(V<Tagged> left, V<Tagged> right) {
    return BigIntComparison(left, right,
                            BigIntComparisonOp::Kind::kLessThanOrEqual);
  }

  V<Tagged> BigIntUnary(V<Tagged> input, BigIntUnaryOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceBigIntUnary(input, kind);
  }
  V<Tagged> BigIntNegate(V<Tagged> input) {
    return BigIntUnary(input, BigIntUnaryOp::Kind::kNegate);
  }

  OpIndex Word32PairBinop(V<Word32> left_low, V<Word32> left_high,
                          V<Word32> right_low, V<Word32> right_high,
                          Word32PairBinopOp::Kind kind) {
    return stack().ReduceWord32PairBinop(left_low, left_high, right_low,
                                         right_high, kind);
  }

  V<Word32> StringAt(V<String> string, V<WordPtr> position,
                     StringAtOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringAt(string, position, kind);
  }
  V<Word32> StringCharCodeAt(V<String> string, V<WordPtr> position) {
    return StringAt(string, position, StringAtOp::Kind::kCharCode);
  }
  V<Word32> StringCodePointAt(V<String> string, V<WordPtr> position) {
    return StringAt(string, position, StringAtOp::Kind::kCodePoint);
  }

#ifdef V8_INTL_SUPPORT
  V<String> StringToCaseIntl(V<String> string, StringToCaseIntlOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringToCaseIntl(string, kind);
  }
  V<String> StringToLowerCaseIntl(V<String> string) {
    return StringToCaseIntl(string, StringToCaseIntlOp::Kind::kLower);
  }
  V<String> StringToUpperCaseIntl(V<String> string) {
    return StringToCaseIntl(string, StringToCaseIntlOp::Kind::kUpper);
  }
#endif  // V8_INTL_SUPPORT

  V<Word32> StringLength(V<String> string) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringLength(string);
  }

  V<Smi> StringIndexOf(V<String> string, V<String> search, V<Smi> position) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringIndexOf(string, search, position);
  }

  V<String> StringFromCodePointAt(V<String> string, V<WordPtr> index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringFromCodePointAt(string, index);
  }

  V<String> StringSubstring(V<String> string, V<Word32> start, V<Word32> end) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringSubstring(string, start, end);
  }

  V<String> StringConcat(V<String> left, V<String> right) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringConcat(left, right);
  }

  V<Boolean> StringEqual(V<String> left, V<String> right) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringEqual(left, right);
  }

  V<Boolean> StringComparison(V<String> left, V<String> right,
                              StringComparisonOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStringComparison(left, right, kind);
  }
  V<Boolean> StringLessThan(V<String> left, V<String> right) {
    return StringComparison(left, right, StringComparisonOp::Kind::kLessThan);
  }
  V<Boolean> StringLessThanOrEqual(V<String> left, V<String> right) {
    return StringComparison(left, right,
                            StringComparisonOp::Kind::kLessThanOrEqual);
  }

  V<Smi> ArgumentsLength() {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceArgumentsLength(ArgumentsLengthOp::Kind::kArguments,
                                         0);
  }
  V<Smi> RestLength(int formal_parameter_count) {
    DCHECK_LE(0, formal_parameter_count);
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceArgumentsLength(ArgumentsLengthOp::Kind::kRest,
                                         formal_parameter_count);
  }

  V<FixedArray> NewArgumentsElements(V<Smi> arguments_count,
                                     CreateArgumentsType type,
                                     int formal_parameter_count) {
    DCHECK_LE(0, formal_parameter_count);
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceNewArgumentsElements(arguments_count, type,
                                              formal_parameter_count);
  }

  OpIndex LoadTypedElement(OpIndex buffer, V<Object> base, V<WordPtr> external,
                           V<WordPtr> index, ExternalArrayType array_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadTypedElement(buffer, base, external, index,
                                          array_type);
  }

  OpIndex LoadDataViewElement(V<Object> object, V<Object> storage,
                              V<WordPtr> index, V<Word32> is_little_endian,
                              ExternalArrayType element_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadDataViewElement(object, storage, index,
                                             is_little_endian, element_type);
  }

  V<Object> LoadStackArgument(V<Object> base, V<WordPtr> index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadStackArgument(base, index);
  }

  void StoreTypedElement(OpIndex buffer, V<Object> base, V<WordPtr> external,
                         V<WordPtr> index, OpIndex value,
                         ExternalArrayType array_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStoreTypedElement(buffer, base, external, index, value,
                                    array_type);
  }

  void StoreDataViewElement(V<Object> object, V<Object> storage,
                            V<WordPtr> index, OpIndex value,
                            V<Word32> is_little_endian,
                            ExternalArrayType element_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStoreDataViewElement(object, storage, index, value,
                                       is_little_endian, element_type);
  }

  void TransitionAndStoreArrayElement(
      V<Object> array, V<WordPtr> index, OpIndex value,
      TransitionAndStoreArrayElementOp::Kind kind, MaybeHandle<Map> fast_map,
      MaybeHandle<Map> double_map) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTransitionAndStoreArrayElement(array, index, value, kind,
                                                 fast_map, double_map);
  }

  void StoreSignedSmallElement(V<Object> array, V<WordPtr> index,
                               V<Word32> value) {
    TransitionAndStoreArrayElement(
        array, index, value,
        TransitionAndStoreArrayElementOp::Kind::kSignedSmallElement, {}, {});
  }

  V<Word32> CompareMaps(V<HeapObject> heap_object,
                        const ZoneRefSet<Map>& maps) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCompareMaps(heap_object, maps);
  }

  void CheckMaps(V<HeapObject> heap_object, OpIndex frame_state,
                 const ZoneRefSet<Map>& maps, CheckMapsFlags flags,
                 const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceCheckMaps(heap_object, frame_state, maps, flags, feedback);
  }

  void AssumeMap(V<HeapObject> heap_object, const ZoneRefSet<Map>& maps) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceAssumeMap(heap_object, maps);
  }

  V<Object> CheckedClosure(V<Object> input, OpIndex frame_state,
                           Handle<FeedbackCell> feedback_cell) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceCheckedClosure(input, frame_state, feedback_cell);
  }

  void CheckEqualsInternalizedString(V<Object> expected, V<Object> value,
                                     OpIndex frame_state) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceCheckEqualsInternalizedString(expected, value, frame_state);
  }

  V<Object> LoadMessage(V<WordPtr> offset) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceLoadMessage(offset);
  }

  void StoreMessage(V<WordPtr> offset, V<Object> object) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStoreMessage(offset, object);
  }

  V<Boolean> SameValue(V<Object> left, V<Object> right,
                       SameValueOp::Mode mode) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSameValue(left, right, mode);
  }

  V<Word32> Float64SameValue(OpIndex left, OpIndex right) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFloat64SameValue(left, right);
  }

  OpIndex FastApiCall(OpIndex data_argument,
                      base::Vector<const OpIndex> arguments,
                      const FastApiCallParameters* parameters) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFastApiCall(data_argument, arguments, parameters);
  }

  void RuntimeAbort(AbortReason reason) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceRuntimeAbort(reason);
  }

  V<Object> EnsureWritableFastElements(V<Object> object, V<Object> elements) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceEnsureWritableFastElements(object, elements);
  }

  V<Object> MaybeGrowFastElements(V<Object> object, V<Object> elements,
                                  V<Word32> index, V<Word32> elements_length,
                                  OpIndex frame_state,
                                  GrowFastElementsMode mode,
                                  const FeedbackSource& feedback) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceMaybeGrowFastElements(
        object, elements, index, elements_length, frame_state, mode, feedback);
  }

  void TransitionElementsKind(V<HeapObject> object,
                              const ElementsTransition& transition) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceTransitionElementsKind(object, transition);
  }

  OpIndex FindOrderedHashEntry(V<Object> data_structure, OpIndex key,
                               FindOrderedHashEntryOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceFindOrderedHashEntry(data_structure, key, kind);
  }
  V<Smi> FindOrderedHashMapEntry(V<Object> table, V<Smi> key) {
    return FindOrderedHashEntry(
        table, key, FindOrderedHashEntryOp::Kind::kFindOrderedHashMapEntry);
  }
  V<Smi> FindOrderedHashSetEntry(V<Object> table, V<Smi> key) {
    return FindOrderedHashEntry(
        table, key, FindOrderedHashEntryOp::Kind::kFindOrderedHashSetEntry);
  }
  V<WordPtr> FindOrderedHashMapEntryForInt32Key(V<Object> table,
                                                V<Word32> key) {
    return FindOrderedHashEntry(
        table, key,
        FindOrderedHashEntryOp::Kind::kFindOrderedHashMapEntryForInt32Key);
  }

#ifdef V8_ENABLE_WEBASSEMBLY
  OpIndex GlobalGet(V<WasmInstanceObject> instance,
                    const wasm::WasmGlobal* global) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceGlobalGet(instance, global);
  }

  OpIndex GlobalSet(V<WasmInstanceObject> instance, OpIndex value,
                    const wasm::WasmGlobal* global) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceGlobalSet(instance, value, global);
  }

  V<HeapObject> Null(wasm::ValueType type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceNull(type);
  }

  V<Word32> IsNull(V<Tagged> input, wasm::ValueType type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceIsNull(input, type);
  }

  V<Tagged> AssertNotNull(V<Tagged> object, wasm::ValueType type,
                          TrapId trap_id) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceAssertNotNull(object, type, trap_id);
  }

  V<Map> RttCanon(V<WasmInstanceObject> instance, uint32_t type_index) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceRttCanon(instance, type_index);
  }

  V<Word32> WasmTypeCheck(V<Tagged> object, V<Map> rtt,
                          WasmTypeCheckConfig config) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceWasmTypeCheck(object, rtt, config);
  }

  V<Tagged> WasmTypeCast(V<Tagged> object, V<Map> rtt,
                         WasmTypeCheckConfig config) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceWasmTypeCast(object, rtt, config);
  }

  OpIndex StructGet(V<HeapObject> object, const wasm::StructType* type,
                    int field_index, bool is_signed, CheckForNull null_check) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceStructGet(object, type, field_index, is_signed,
                                   null_check);
  }

  void StructSet(V<HeapObject> object, OpIndex value,
                 const wasm::StructType* type, int field_index,
                 CheckForNull null_check) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceStructSet(object, value, type, field_index, null_check);
  }

  OpIndex ArrayGet(V<HeapObject> array, V<Word32> index,
                   wasm::ValueType element_type, bool is_signed) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceArrayGet(array, index, element_type, is_signed);
  }

  void ArraySet(V<HeapObject> array, V<Word32> index, OpIndex value,
                wasm::ValueType element_type) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return;
    }
    stack().ReduceArraySet(array, index, value, element_type);
  }

  V<Word32> ArrayLength(V<HeapObject> array, CheckForNull null_check) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceArrayLength(array, null_check);
  }

  V<Simd128> Simd128Constant(const uint8_t value[kSimd128Size]) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Constant(value);
  }

  V<Simd128> Simd128Binop(V<Simd128> left, V<Simd128> right,
                          Simd128BinopOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Binop(left, right, kind);
  }

  V<Simd128> Simd128Unary(V<Simd128> input, Simd128UnaryOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Unary(input, kind);
  }

  V<Simd128> Simd128Shift(V<Simd128> input, V<Word32> shift,
                          Simd128ShiftOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Shift(input, shift, kind);
  }

  V<Word32> Simd128Test(V<Simd128> input, Simd128TestOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Test(input, kind);
  }

  V<Simd128> Simd128Splat(OpIndex input, Simd128SplatOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Splat(input, kind);
  }

  V<Simd128> Simd128Ternary(OpIndex first, OpIndex second, OpIndex third,
                            Simd128TernaryOp::Kind kind) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Ternary(first, second, third, kind);
  }

  OpIndex Simd128ExtractLane(V<Simd128> input, Simd128ExtractLaneOp::Kind kind,
                             uint8_t lane) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128ExtractLane(input, kind, lane);
  }

  V<Simd128> Simd128ReplaceLane(V<Simd128> into, OpIndex new_lane,
                                Simd128ReplaceLaneOp::Kind kind, uint8_t lane) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128ReplaceLane(into, new_lane, kind, lane);
  }

  OpIndex Simd128LaneMemory(V<WordPtr> base, V<WordPtr> index, V<WordPtr> value,
                            Simd128LaneMemoryOp::Mode mode,
                            Simd128LaneMemoryOp::Kind kind,
                            Simd128LaneMemoryOp::LaneKind lane_kind,
                            uint8_t lane, int offset) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128LaneMemory(base, index, value, mode, kind,
                                           lane_kind, lane, offset);
  }

  OpIndex Simd128LoadTransform(
      V<WordPtr> base, V<WordPtr> index,
      Simd128LoadTransformOp::LoadKind load_kind,
      Simd128LoadTransformOp::TransformKind transform_kind, int offset) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128LoadTransform(base, index, load_kind,
                                              transform_kind, offset);
  }

  V<Simd128> Simd128Shuffle(V<Simd128> left, V<Simd128> right,
                            const uint8_t shuffle[kSimd128Size]) {
    if (V8_UNLIKELY(stack().generating_unreachable_operations())) {
      return OpIndex::Invalid();
    }
    return stack().ReduceSimd128Shuffle(left, right, shuffle);
  }

#endif  // V8_ENABLE_WEBASSEMBLY

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

  // These methods are used by the assembler macros (IF, ELSE, ELSE_IF, END_IF).
  template <typename L>
  auto ControlFlowHelper_Bind(L& label)
      -> base::prepend_tuple_type<bool, typename L::values_t> {
    // LoopLabels need to be bound with `LOOP` instead of `BIND`.
    static_assert(!L::is_loop);
    return label.Bind(stack());
  }

  template <typename L>
  auto ControlFlowHelper_BindLoop(L& label)
      -> base::prepend_tuple_type<bool, typename L::values_t> {
    // Only LoopLabels can be bound with `LOOP`. Otherwise use `BIND`.
    static_assert(L::is_loop);
    return label.BindLoop(stack());
  }

  template <typename L>
  void ControlFlowHelper_EndLoop(L& label) {
    static_assert(L::is_loop);
    label.EndLoop(stack());
  }

  template <typename L>
  void ControlFlowHelper_Goto(L& label,
                              const typename L::const_or_values_t& values) {
    auto resolved_values = detail::ResolveAll(stack(), values);
    label.Goto(stack(), resolved_values);
  }

  template <typename L>
  void ControlFlowHelper_GotoIf(ConditionWithHint condition, L& label,
                                const typename L::const_or_values_t& values) {
    auto resolved_values = detail::ResolveAll(stack(), values);
    label.GotoIf(stack(), condition.condition(), condition.hint(),
                 resolved_values);
  }

  template <typename L>
  void ControlFlowHelper_GotoIfNot(
      ConditionWithHint condition, L& label,
      const typename L::const_or_values_t& values) {
    auto resolved_values = detail::ResolveAll(stack(), values);
    label.GotoIfNot(stack(), condition.condition(), condition.hint(),
                    resolved_values);
  }

  bool ControlFlowHelper_If(ConditionWithHint condition, bool negate) {
    Block* then_block = stack().NewBlock();
    Block* else_block = stack().NewBlock();
    Block* end_block = stack().NewBlock();
    if (negate) {
      this->Branch(condition, else_block, then_block);
    } else {
      this->Branch(condition, then_block, else_block);
    }
    if_scope_stack_.emplace_back(else_block, end_block);
    return stack().Bind(then_block);
  }

  template <typename F>
  bool ControlFlowHelper_ElseIf(F&& condition_builder) {
    DCHECK_LT(0, if_scope_stack_.size());
    auto& info = if_scope_stack_.back();
    Block* else_block = info.else_block;
    DCHECK_NOT_NULL(else_block);
    if (!stack().Bind(else_block)) return false;
    Block* then_block = stack().NewBlock();
    info.else_block = stack().NewBlock();
    stack().Branch(ConditionWithHint{condition_builder()}, then_block,
                   info.else_block);
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
  // LoadArrayBufferElement and LoadNonArrayBufferElement should be called
  // instead of LoadElement.
  template <typename T = Any, typename Base>
  V<T> LoadElement(V<Base> object, const ElementAccess& access,
                   V<WordPtr> index, bool is_array_buffer) {
    if constexpr (std::is_base_of_v<Object, Base>) {
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kTaggedBase);
    } else {
      static_assert(std::is_same_v<Base, WordPtr>);
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kUntaggedBase);
    }
    LoadOp::Kind kind = LoadOp::Kind::Aligned(access.base_is_tagged);
    if (is_array_buffer) kind = kind.NotAlwaysCanonicallyAccessed();
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(access.machine_type);
    return Load(object, index, kind, rep, access.header_size,
                rep.SizeInBytesLog2());
  }

  // StoreArrayBufferElement and StoreNonArrayBufferElement should be called
  // instead of StoreElement.
  template <typename Base>
  void StoreElement(V<Base> object, const ElementAccess& access,
                    V<WordPtr> index, V<Any> value, bool is_array_buffer) {
    if constexpr (std::is_base_of_v<Object, Base>) {
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kTaggedBase);
    } else {
      static_assert(std::is_same_v<Base, WordPtr>);
      DCHECK_EQ(access.base_is_tagged, BaseTaggedness::kUntaggedBase);
    }
    LoadOp::Kind kind = LoadOp::Kind::Aligned(access.base_is_tagged);
    if (is_array_buffer) kind = kind.NotAlwaysCanonicallyAccessed();
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(access.machine_type);
    Store(object, index, value, kind, rep, access.write_barrier_kind,
          access.header_size, rep.SizeInBytesLog2());
  }

  // BranchAndBind should be called from GotoIf/GotoIfNot. It will insert a
  // Branch, bind {to_bind} (which should correspond to the implicit new block
  // following the GotoIf/GotoIfNot) and return a ConditionalGotoStatus
  // representing whether the destinations of the Branch are reachable or not.
  ConditionalGotoStatus BranchAndBind(OpIndex condition, Block* if_true,
                                      Block* if_false, BranchHint hint,
                                      Block* to_bind) {
    DCHECK_EQ(to_bind, any_of(if_true, if_false));
    Block* other = to_bind == if_true ? if_false : if_true;
    Block* to_bind_last_pred = to_bind->LastPredecessor();
    Block* other_last_pred = other->LastPredecessor();
    stack().Branch(condition, if_true, if_false, hint);
    bool to_bind_reachable = to_bind_last_pred != to_bind->LastPredecessor();
    bool other_reachable = other_last_pred != other->LastPredecessor();
    ConditionalGotoStatus status = static_cast<ConditionalGotoStatus>(
        static_cast<int>(other_reachable) | ((to_bind_reachable) << 1));
    bool bind_status = stack().Bind(to_bind);
    DCHECK_EQ(bind_status, to_bind_reachable);
    USE(bind_status);
    return status;
  }

  Assembler& stack() { return *static_cast<Assembler*>(this); }
  struct IfScopeInfo {
    Block* else_block;
    Block* end_block;

    IfScopeInfo(Block* else_block, Block* end_block)
        : else_block(else_block), end_block(end_block) {}
  };
  base::SmallVector<IfScopeInfo, 16> if_scope_stack_;
  // [0] contains the stub with exit frame.
  MaybeHandle<Code> cached_centry_stub_constants_[4];
  bool in_object_initialization_ = false;
};

template <class Reducers>
class Assembler : public GraphVisitor<Assembler<Reducers>>,
                  public reducer_stack_type<Reducers>::type,
                  public OperationMatcher,
                  public AssemblerOpInterface<Assembler<Reducers>> {
  using Stack = typename reducer_stack_type<Reducers>::type;

 public:
  class CatchScope;

  explicit Assembler(Graph& input_graph, Graph& output_graph, Zone* phase_zone,
                     compiler::NodeOriginTable* origins)
      : GraphVisitor<Assembler>(input_graph, output_graph, phase_zone, origins),
        Stack(),
        OperationMatcher(output_graph) {
    SupportedOperations::Initialize();
  }

  Block* NewLoopHeader() { return this->output_graph().NewLoopHeader(); }
  Block* NewBlock() { return this->output_graph().NewBlock(); }

  V8_INLINE bool Bind(Block* block) {
    if (!this->output_graph().Add(block)) {
      generating_unreachable_operations_ = true;
      return false;
    }
    DCHECK_NULL(current_block_);
    current_block_ = block;
    generating_unreachable_operations_ = false;
    block->SetOrigin(this->current_input_block());
    Stack::Bind(block);
    return true;
  }

  // TODO(nicohartmann@): Remove this.
  V8_INLINE void BindReachable(Block* block) {
    bool bound = Bind(block);
    DCHECK(bound);
    USE(bound);
  }

  // Every loop should be finalized once, after it is certain that no backedge
  // can be added anymore.
  void FinalizeLoop(Block* loop_header) {
    DCHECK(loop_header->IsLoop());
    if (loop_header->HasExactlyNPredecessors(1)) {
      this->output_graph().TurnLoopIntoMerge(loop_header);
    }
  }

  void SetCurrentOrigin(OpIndex operation_origin) {
    current_operation_origin_ = operation_origin;
  }

  void SetGeneratingUnreachableOperations() {
    DCHECK_NULL(current_block_);
    generating_unreachable_operations_ = true;
  }

  Block* current_block() const { return current_block_; }
  Block* current_catch_block() const { return current_catch_block_; }
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
    DCHECK(ValidInputs(result));
#endif  // DEBUG
    if (op.IsBlockTerminator()) FinalizeBlock();
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
      // We have to split `pred` first to preserve order of predecessors.
      SplitEdge(pred, destination);
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

    // Updating {source}'s last Branch/Switch/CheckException. Note that
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
      case Opcode::kCheckException: {
        CheckExceptionOp& catch_exception_op = op.Cast<CheckExceptionOp>();
        if (catch_exception_op.didnt_throw_block == destination) {
          catch_exception_op.didnt_throw_block = intermediate_block;
          // We assume that CheckException's successor and catch_block
          // can never be the same (there is a DCHECK in
          // CheckExceptionOp::Validate enforcing that).
          DCHECK_NE(catch_exception_op.catch_block, destination);
        } else {
          DCHECK_EQ(catch_exception_op.catch_block, destination);
          catch_exception_op.catch_block = intermediate_block;
          // A catch block always has to start with a `CatchBlockBeginOp`.
          BindReachable(intermediate_block);
          intermediate_block->SetOrigin(source->OriginForBlockEnd());
          this->CatchBlockBegin();
          this->Goto(destination);
          return;
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

    BindReachable(intermediate_block);
    intermediate_block->SetOrigin(source->OriginForBlockEnd());
    // Inserting a Goto in {intermediate_block} to {destination}. This will
    // create the edge from {intermediate_block} to {destination}. Note that
    // this will call AddPredecessor, but we've already removed the eventual
    // edge of {destination} that need splitting, so no risks of inifinite
    // recursion here.
    this->Goto(destination);
  }

  Block* current_block_ = nullptr;
  Block* current_catch_block_ = nullptr;
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

template <class Reducers>
class Assembler<Reducers>::CatchScope {
 public:
  CatchScope(Assembler& assembler, Block* catch_block)
      : assembler_(assembler),
        previous_catch_block_(assembler.current_catch_block_) {
    assembler_.current_catch_block_ = catch_block;
#ifdef DEBUG
    this->catch_block = catch_block;
#endif
  }

  ~CatchScope() {
    DCHECK_EQ(assembler_.current_catch_block_, catch_block);
    assembler_.current_catch_block_ = previous_catch_block_;
  }

  CatchScope& operator=(const CatchScope&) = delete;
  CatchScope(const CatchScope&) = delete;
  CatchScope& operator=(CatchScope&&) = delete;
  CatchScope(CatchScope&&) = delete;

 private:
  Assembler& assembler_;
  Block* previous_catch_block_;
#ifdef DEBUG
  Block* catch_block = nullptr;
#endif

  friend class Assembler;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSEMBLER_H_
