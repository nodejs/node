// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_TYPESWITCH_H_
#define V8_COMPILER_TURBOSHAFT_TYPESWITCH_H_

#include "src/compiler/turboshaft/assembler.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// This file provides the implementation for the TYPESWITCH construct, which
// allows dispatching over different object types
//
// Example:
//
//   V<Object> obj = ...
//   TYPESWITCH(obj) {
//     CASE(V<Number>, num): { ... use num ... }
//     CASE(V<BigInt>, bi):  { ... use bi  ... }
//     CASE(V<String>, str): { ... use str ... }
//     CASE(V<Union<Undefined, Symbol>>, oth): {
//       ... use oth ...
//     }
//   }
//
// The behavior closely follows the semantics of Torque's typeswitch. This means
// that all cases are tested in sequence of definition, such that in this
// particular example, code is generated that first checks if obj is a Number
// (see below for some details on this works exactly) and executes that case's
// code if successful, otherwise proceeds with checking for BigInt. Note that
// CASEs are terminated implicitly, so there is neither a fallthrough, nor a
// need for some sort of break, but when control flow reaches the end of a CASE
// block, execution continues after the TYPESWITCH.
//
// The sequential order of cases also means that later cases might be hidden by
// previous cases. For example adding a CASE(V<Smi>, smi) at the end of the
// TYPESWITCH above, that case would be unreachable because every Smi would be
// matched by the Number-case already. CASEs that are entirely unreachable will
// report an error as this is never intended. However, some cases can be
// unreachable for specific object types, e.g. when adding a CASE(V<Object>, o)
// at the very end. While the above handles types would not reach this case,
// this is exaplicitly allowed as a "all others"-case.
//
// There is no default case and exhaustiveness is not enforced, so if you need
// this, a CASE(V<Object>, other) is the way to achieve this.
//
// Now for the actual matching: As checking for different object types is most
// efficient using different means (e.g. checking the map, comparing instance
// type ranges, comparing for root constants, ...), the logic to check for a
// specific type needs to be provided explicitly, by specializing the
// MatchCase<> template defined below. The specialization needs to provide an
// implementation of this function
//
//   template <typename AssemblerT>
//   static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
//     ...
//   }
//
// where Turboshaft graphs can be defined in the usual way using the
// assembler's functions (the Asm argument allows the use of the usual __) and
// you need to emit the code to check for the respective type and jump to the
// target block, if the object is of that type. The MatchState arguments
// provides accessed to (cached) data, e.g. value, map or instance_type. Notice,
// that the value() is already known to be a HeapObject, as the Smi case is
// handled separately. MatchState is also a means to communicate information
// between cases, when this is beneficial to emit better code (e.g. if specific
// instance types are already checked), but this is currently not used.
//
// It is possible to define cases for Union types and for a Union<T1, T2>, the
// TYPESWITCH logic will automatically invoke MatchCase<T1> and MatchCase<T2> in
// order to check for the respective types. If there is a more efficient way to
// check for the combined type, consider providing a specialization for
// MatchCase<Union<T1, T2>>.

namespace detail {

// This wrapper allows the use of __ by providing a call operator.
template <typename AssemblerT>
struct AssemblerWrapper {
  using assembler_t = AssemblerT;

  AssemblerWrapper(assembler_t& assembler, Factory* factory)
      : assembler_(assembler), factory_(factory) {}

  inline assembler_t& operator()() { return assembler_; }

  inline Factory* factory() { return factory_; }

  assembler_t assembler_;
  Factory* factory_;
};

}  // namespace detail

// MatchState provides caching for relevant information for MatchCase
// implementations and can be extended to communicate information between cases
// if that turns out to be useful.
class MatchState {
 public:
  MatchState() : value_() {}

  template <typename T>
    requires(std::is_base_of_v<HeapObject, typename T::type>)
  static MatchState FromValue(T value) {
    return MatchState{value};
  }

  template <typename... Ts>
    requires(std::is_base_of_v<HeapObject, Ts> && ...)
  static MatchState FromValue(V<Union<Ts...>> value) {
    return MatchState{V<HeapObject>::Cast(value)};
  }

  V<HeapObject> value() { return value_; }

  template <typename AssemblerT>
  V<Map> map(detail::AssemblerWrapper<AssemblerT>& Asm) {
    if (!map_.has_value()) {
      map_ = __ LoadMapField(value_);
    }
    return map_.value();
  }

  template <typename AssemblerT>
  V<Word32> instance_type(detail::AssemblerWrapper<AssemblerT>& Asm) {
    if (!instance_type_.has_value()) {
      instance_type_ = __ LoadInstanceTypeField(map(Asm));
    }
    return instance_type_.value();
  }

 private:
  explicit MatchState(V<HeapObject> value) : value_(value) {}

  V<HeapObject> value_;
  OptionalV<Map> map_;
  OptionalV<Word32> instance_type_;
};

template <typename T>
struct MatchCase;

namespace detail {
template <typename T, typename = void>
struct TestIsSmiCase : std::false_type {};

template <typename T>
struct TestIsSmiCase<T, std::void_t<decltype(MatchCase<T>::kIsSmiCase)>>
    : std::true_type {};
}  // namespace detail

template <>
struct MatchCase<Smi> {
  static constexpr bool kIsSmiCase = true;

  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* block) {
    // Nothing to do here, as Smi has special handling in TypeswitchBuilder.
  }
};

template <>
struct MatchCase<HeapNumber> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ GotoIf(__ TaggedEqual(state.map(Asm),
                             __ HeapConstant(Asm.factory()->heap_number_map())),
              target);
  }
};

template <>
struct MatchCase<String> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
#if V8_STATIC_ROOTS_BOOL
    V<Word32> is_string = __ IsStringMap(state.map(Asm));
#else
    V<Word32> instance_type = state.instance_type(Asm);
    V<Word32> is_string =
        __ Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE);
#endif
    __ GotoIf(is_string, target);
  }
};

template <>
struct MatchCase<Oddball> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ GotoIf(__ Word32Equal(state.instance_type(Asm), ODDBALL_TYPE), target);
  }
};

template <>
struct MatchCase<JSReceiver> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    V<Word32> i = state.instance_type(Asm);
    V<Word32> cond =
        __ Word32BitwiseAnd(__ Uint32LessThanOrEqual(FIRST_JS_RECEIVER_TYPE, i),
                            __ Uint32LessThanOrEqual(i, LAST_JS_RECEIVER_TYPE));
    __ GotoIf(cond, target);
  }
};

template <>
struct MatchCase<Symbol> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
#if V8_STATIC_ROOTS_BOOL
    V<Word32> is_symbol = __ IsSymbolMap(state.map(Asm));
#else
    V<Word32> instance_type = state.instance_type(Asm);
    V<Word32> is_symbol = __ Word32Equal(instance_type, SYMBOL_TYPE);

#endif
    __ GotoIf(is_symbol, target);
  }
};

template <>
struct MatchCase<Object> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ Goto(target);
  }
};

template <>
struct MatchCase<Undefined> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ GotoIf(__ TaggedEqual(state.value(),
                             __ HeapConstant(Asm.factory()->undefined_value())),
              target);
  }
};

template <>
struct MatchCase<Boolean> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
#if V8_STATIC_ROOTS_BOOL
    V<Word32> map_int32 = __ TruncateWordPtrToWord32(
        __ BitcastHeapObjectToWordPtr(state.map(Asm)));
    __ GotoIf(__ Word32Equal(map_int32, StaticReadOnlyRoot::kBooleanMap),
              target);
#else
    __ GotoIf(__ TaggedEqual(state.map(Asm),
                             __ HeapConstant(Asm.factory()->boolean_map())),
              target);
#endif
  }
};

template <>
struct MatchCase<Null> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
#if V8_STATIC_ROOTS_BOOL
    V<Word32> value_int32 = __ TruncateWordPtrToWord32(
        __ BitcastHeapObjectToWordPtr(state.value()));
    __ GotoIf(__ Word32Equal(value_int32, StaticReadOnlyRoot::kNullValue),
              target);
#else
    __ GotoIf(__ TaggedEqual(state.value(),
                             __ HeapConstant(Asm.factory()->null_value())),
              target);
#endif
  }
};

template <>
struct MatchCase<BigInt> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ GotoIf(__ TaggedEqual(state.map(Asm),
                             __ HeapConstant(Asm.factory()->bigint_map())),
              target);
  }
};

template <>
struct MatchCase<JSCallable> {
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    __ GotoIf(__ template IsSetWord32<Map::Bits1::IsCallableBit>(__ LoadField(
                  state.map(Asm), AccessBuilderTS::ForMapBitField())),
              target);
  }
};

template <>
struct MatchCase<JSAny> {
  static constexpr bool kIsSmiCase = true;
  static_assert(
      std::is_same_v<JSAny, Union<Smi, HeapNumber, BigInt, String, Symbol,
                                  Boolean, Null, Undefined, JSReceiver>>);
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    // TODO(nicohartmann): Look into some ways to be a bit smarter about that.
    MatchCase<Smi>::Match(Asm, state, target);
    MatchCase<HeapNumber>::Match(Asm, state, target);
    MatchCase<BigInt>::Match(Asm, state, target);
    MatchCase<String>::Match(Asm, state, target);
    MatchCase<Symbol>::Match(Asm, state, target);
    MatchCase<Oddball>::Match(Asm, state, target);
    MatchCase<JSReceiver>::Match(Asm, state, target);
  }
};

template <typename... Ts>
struct MatchCase<Union<Ts...>> {
  static constexpr bool kIsSmiCase = (detail::TestIsSmiCase<Ts>::value || ...);
  template <typename AssemblerT>
  static void Match(AssemblerT& Asm, MatchState& state, Block* target) {
    std::initializer_list<int> _{
        (MatchCase<Ts>::Match(Asm, state, target), 0)...};
  }
};

namespace detail {
template <typename T>
struct TypesOf {
  using type = base::tmp::list<T>;
};

template <typename... Ts>
struct TypesOf<Union<Ts...>> {
  using type = base::tmp::list<Ts...>;
};

template <typename T>
using IsObjectSubtype = is_subtype<T, Object>;
}  // namespace detail

template <typename AssemblerT>
class TypeswitchBuilder {
  enum ControlState : int {
    kUninitialized,
    kCollectCases,
    kEmitBranches,
    kEmitCases,
    kDone,
  };

 public:
  using assembler_t = AssemblerT;

  template <typename T>
  TypeswitchBuilder(assembler_t& assembler, PipelineData* data, V<T> value)
      : assembler_(assembler), data_(data), value_(value) {
    using types = detail::TypesOf<T>::type;
    static_assert(base::tmp::all_of_v<types, detail::IsObjectSubtype>,
                  "Cannot TYPESWITCH over this type");
    state_ = MatchState::FromValue(V<HeapObject>::Cast(value));
    can_be_smi_ = base::tmp::contains_v<types, Smi> ||
                  base::tmp::contains_v<types, Object>;
  }

  // BeginCase is invoked multiple times by the builder for each case (case_ref
  // here is a unique identification for the specific case). The builder's
  // control_state_ describes what needs to be done for this case.
  //

  template <typename T>
  std::pair<bool, T> BeginCase(int case_ref) {
    switch (control_state_) {
      case ControlState::kUninitialized:
      case ControlState::kDone:
        UNREACHABLE();
      case ControlState::kCollectCases: {
        // 1.) ControlState::kCollectCases: We add this case into the internal
        // array of cases and allocate an entry block for that case, which will
        // be used as the jump target and which will contain the CASE's
        // implementation.
        DCHECK(!base::contains(cases_, case_ref));
        cases_[case_ref] = assembler_.NewBlock();
        if (smi_target_ == nullptr &&
            detail::TestIsSmiCase<typename T::type>::value) {
          smi_target_ = cases_[case_ref];
        }
        return {false, T::Invalid()};
      }
      case ControlState::kEmitBranches: {
        // 2.) ControlState::kEmitBranches: Now we need to emit the check for
        // this CASE's type and emit the conditional jump to the CASE's block
        // allocated in 1. We use the respective MatchCase specialization for
        // that.
        detail::AssemblerWrapper<assembler_t> Asm(assembler_,
                                                  data_->isolate()->factory());
        auto it = cases_.find(case_ref);
        DCHECK_NE(it, cases_.end());
        MatchCase<typename T::type>::Match(Asm, state_, it->second);
        return {false, T::Invalid()};
      }
      case ControlState::kEmitCases: {
        // 3.) Now emit all cases in order. We bind the block allocated in 1
        // and we return `true`, such that we will enter the loop guarded by
        // this `BeginCase` where the CASE's handling is emitted.
        auto it = cases_.find(case_ref);
        DCHECK_NE(it, cases_.end());
        const bool bound = assembler_.Bind(it->second);
        if (!bound) FATAL("CASE of TYPESWITCH is unreachable");
        return {true, T::Cast(state_.value())};
      }
    }
    UNREACHABLE();
  }

  // EndCase is invoked for a CASE when we have emitted that case's code and if
  // the case is not terminated unconditionally, we set control flow to continue
  // after the TYPESWITCH.
  void EndCase(int case_ref) {
    DCHECK_NOT_NULL(exit_block_);
    if (!assembler_.generating_unreachable_operations()) {
      assembler_.Goto(exit_block_);
    }
  }

  bool NextState() {
    ++control_state_;
    switch (control_state_) {
      case ControlState::kUninitialized:
        UNREACHABLE();
      case ControlState::kCollectCases:
        return true;
      case ControlState::kEmitBranches: {
        DCHECK_NULL(exit_block_);
        exit_block_ = assembler_.NewBlock();

        // We explicitly handle smi case here if applicable.
        if (can_be_smi_) {
          if (!smi_target_) smi_target_ = exit_block_;
          assembler_.GotoIf(assembler_.IsSmi(value_), smi_target_);
        }
        return true;
      }
      case ControlState::kEmitCases: {
        // Now we are emitting the code for the cases. Before the first case,
        // though, we emit a jump to the exit block, so that we jump to after
        // the TYPESWITCH for all types that are not handled.
        if (!assembler_.generating_unreachable_operations()) {
          assembler_.Goto(exit_block_);
        }
        return true;
      }
      case ControlState::kDone: {
        // We are done emitting cases. Bind the exit block and terminate the
        // loop.
        assembler_.Bind(exit_block_);
        return false;
      }
    }
    UNREACHABLE();
  }

 private:
  assembler_t& assembler_;
  PipelineData* data_;
  OpIndex value_;
  bool can_be_smi_;
  MatchState state_;
  int control_state_ = ControlState::kUninitialized;
  base::SmallMap<std::map<int, Block*>, 8> cases_;
  Block* smi_target_ = nullptr;
  Block* exit_block_ = nullptr;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_TYPESWITCH_H_
