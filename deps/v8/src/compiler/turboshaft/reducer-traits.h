// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_REDUCER_TRAITS_H_
#define V8_COMPILER_TURBOSHAFT_REDUCER_TRAITS_H_

#include <limits>
#include <type_traits>

#include "src/base/template-meta-programming/common.h"
#include "src/base/template-meta-programming/list.h"

namespace v8::internal::compiler::turboshaft {

template <template <typename> typename... Ts>
using reducer_list = base::tmp::list1<Ts...>;

// Get the length of a reducer_list<> {RL}.
template <typename RL>
struct reducer_list_length : base::tmp::length1<RL> {};
template <typename RL>
constexpr size_t reducer_list_length_v = reducer_list_length<RL>::value;

// Checks if a reducer_list<> {RL} contains reducer {R}.
template <typename RL, template <typename> typename R>
struct reducer_list_contains : base::tmp::contains1<RL, R> {};
template <typename RL, template <typename> typename R>
constexpr bool reducer_list_contains_v = reducer_list_contains<RL, R>::value;

// Checks if a reducer_list<> {RL} starts with reducer {R}.
template <typename RL, template <typename> typename R>
struct reducer_list_starts_with {
  static constexpr bool value = base::tmp::index_of1<RL, R>::value == 0;
};
template <typename RL, template <typename> typename R>
constexpr bool reducer_list_starts_with_v =
    reducer_list_starts_with<RL, R>::value;

// Get the index of {R} in the reducer_list<> {RL} or {Otherwise} if it is not
// in the list.
template <typename RL, template <typename> typename R,
          size_t Otherwise = std::numeric_limits<size_t>::max()>
struct reducer_list_index_of : public base::tmp::index_of1<RL, R, Otherwise> {};
template <typename RL, template <typename> typename R,
          size_t Otherwise = std::numeric_limits<size_t>::max()>
constexpr size_t reducer_list_index_of_v =
    reducer_list_index_of<RL, R, Otherwise>::value;

// Inserts reducers {Rs...} into reducer_list<> {RL} at index {I}. If I >=
// length of {RL}, then {Rs...} are appended.
template <typename RL, size_t I, template <typename> typename... Rs>
struct reducer_list_insert_at : base::tmp::insert_at1<RL, I, Rs...> {};
template <typename RL, size_t I, template <typename> typename... Rs>
using reducer_list_insert_at_t = reducer_list_insert_at<RL, I, Rs...>::type;

// Append reducers {Rs...} at the end of reducer_list<> {RL}.
template <typename RL, template <typename> typename... Rs>
struct reducer_list_append : base::tmp::append1<RL, Rs...> {};
template <typename RL, template <typename> typename... Rs>
using reducer_list_append_t = reducer_list_append<RL, Rs...>::type;

// Turns a reducer_list<> into the instantiated class for the stack.
template <typename RL, typename Bottom>
struct reducer_list_to_stack
    : base::tmp::fold_right1<base::tmp::instantiate, RL, Bottom> {};
template <typename RL, typename Bottom>
using reducer_list_to_stack_t = reducer_list_to_stack<RL, Bottom>::type;

// Check if in the {Next} ReducerStack, any of {Reducer} comes next.
template <typename Next, template <typename> typename... Reducer>
struct next_reducer_is {
  static constexpr bool value =
      (base::tmp::is_instantiation_of<Next, Reducer>::value || ...);
};
template <typename Next, template <typename> typename... Reducer>
constexpr bool next_reducer_is_v = next_reducer_is<Next, Reducer...>::value;

// Check if the {Next} ReducerStack contains {Reducer}.
template <typename Next, template <typename> typename Reducer>
struct next_contains_reducer : public std::bool_constant<false> {};
template <template <typename> typename R, typename T,
          template <typename> typename Reducer>
struct next_contains_reducer<R<T>, Reducer> {
  static constexpr bool value = base::tmp::equals1<R, Reducer>::value ||
                                next_contains_reducer<T, Reducer>::value;
};
template <typename Next, template <typename> typename Reducer>
constexpr bool next_contains_reducer_v =
    next_contains_reducer<Next, Reducer>::value;

template <typename Next>
class EmitProjectionReducer;
template <typename Next>
class AssemblerOpInterface;
template <typename Next>
class ReducerBase;
template <typename Next>
class GraphEmitter;

// TODO(dmercadier): EmitProjectionReducer is not always the bottom of the stack
// because it could be succeeded by a ValueNumberingReducer. We should take this
// into account in next_is_bottom_of_assembler_stack.
// TODO(nicohartmann): Currently we insert AssemblerOpInterface, ReducerBase and
// GraphEmitter as the bottom of the reducer stack (see BuildReducerStack in
// assembler.h). We should solve this differently or get rid of this predicate
// entirely.
template <typename Next>
struct next_is_bottom_of_assembler_stack
    : public next_reducer_is<Next, EmitProjectionReducer, AssemblerOpInterface,
                             ReducerBase, GraphEmitter> {};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_REDUCER_TRAITS_H_
