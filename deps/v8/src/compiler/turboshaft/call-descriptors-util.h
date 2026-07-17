// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_CALL_DESCRIPTORS_UTIL_H_
#define V8_COMPILER_TURBOSHAFT_CALL_DESCRIPTORS_UTIL_H_

#include "src/compiler/turboshaft/operations.h"

#define DEFINE_TURBOSHAFT_CALL_DESCRIPTOR_ARG(type, name)                      \
  type name;                                                                   \
  static constexpr const size_t name##_index =                                 \
      decltype(index_counter(detail::IndexTag<kMaxArgumentCount>{}))::value;   \
  static constexpr detail::IndexTag<name##_index + 1> index_counter(           \
      detail::IndexTag<name##_index + 1>);                                     \
  static_assert(name##_index < kMaxArgumentCount);                             \
  static constexpr base::tmp::append_t<                                        \
      decltype(make_args_type_list_n(detail::IndexTag<name##_index>{})), type> \
      make_args_type_list_n(detail::IndexTag<name##_index + 1>);               \
  template <size_t I>                                                          \
    requires(I == name##_index + 1)                                            \
  void CollectArguments(arguments_vector_t&, bool, detail::IndexTag<I>)        \
      const {}                                                                 \
  template <typename T>                                                        \
    requires(!is_optional_index_v<T>)                                          \
  void CollectArgumentsImpl(const T& arg, arguments_vector_t& args, bool,      \
                            detail::IndexTag<name##_index>) const {            \
    args.push_back(arg);                                                       \
    CollectArguments(args, false, detail::IndexTag<name##_index + 1>{});       \
  }                                                                            \
  template <typename T>                                                        \
    requires(is_optional_index_v<T>)                                           \
  void CollectArgumentsImpl(const T& arg, arguments_vector_t& args,            \
                            bool must_be_nullopt,                              \
                            detail::IndexTag<name##_index>) const {            \
    CHECK_WITH_MSG(!must_be_nullopt || !arg.has_value(),                       \
                   "After the first omitted optional argument, all remaining " \
                   "arguments must be omitted");                               \
    if (arg.has_value()) {                                                     \
      args.push_back(arg.value());                                             \
      CollectArguments(args, false, detail::IndexTag<name##_index + 1>{});     \
    } else {                                                                   \
      CollectArguments(args, true, detail::IndexTag<name##_index + 1>{});      \
    }                                                                          \
  }                                                                            \
  void CollectArguments(arguments_vector_t& args, bool must_be_nullopt,        \
                        detail::IndexTag<name##_index> tag) const {            \
    CollectArgumentsImpl(name, args, must_be_nullopt, tag);                    \
  }

namespace v8::internal::compiler::turboshaft {

namespace detail {
template <size_t I>
struct IndexTag : public IndexTag<I - 1> {
  static constexpr size_t value = I;
};
template <>
struct IndexTag<0> {
  static constexpr size_t value = 0;
};
}  // namespace detail

struct CallDescriptorBuilder {
  // The maximum number of arguments is chosen arbitrarily and can be increased
  // if necessary.
  static constexpr std::size_t kMaxArgumentCount = 8;
  using arguments_vector_t = base::SmallVector<OpIndex, kMaxArgumentCount>;
  // TODO(abmusse): use ArgumentsBase (until we can use GCC 13 or better)
  struct ArgumentsBase {
    static constexpr inline detail::IndexTag<0> index_counter(
        detail::IndexTag<0>);
    static constexpr base::tmp::list<> make_args_type_list_n(
        detail::IndexTag<0>);
  };
  static constexpr OpEffects base_effects = OpEffects().CanDependOnChecks();
  template <typename A>
  static arguments_vector_t ArgumentsToVector(const A& args) {
    arguments_vector_t result;
    args.CollectArguments(result, false, detail::IndexTag<0>{});
    return result;
  }

  template <typename A>
  static constexpr size_t GetArgumentCount() {
    return decltype(A::index_counter(
        detail::IndexTag<kMaxArgumentCount>{}))::value;
  }

  class NoArguments {
   public:
    static constexpr base::tmp::list<> make_args_type_list_n(
        detail::IndexTag<0>);
    static constexpr inline detail::IndexTag<0> index_counter(
        detail::IndexTag<0>);
    void CollectArguments(arguments_vector_t&, bool,
                          detail::IndexTag<0>) const {}
  };

#ifdef DEBUG
  template <typename T, size_t I>
  struct VerifyArgument {
    void operator()(const CallDescriptor* desc,
                    size_t actual_argument_count) const {
      if (I >= actual_argument_count) return;
      DCHECK(AllowsRepresentation<T>(
          RegisterRepresentation::FromMachineRepresentation(
              desc->GetParameterType(I).representation())));
    }
  };
  template <typename T, size_t I>
  struct VerifyReturn {
    void operator()(const CallDescriptor* desc) const {
      DCHECK(AllowsRepresentation<T>(
          RegisterRepresentation::FromMachineRepresentation(
              desc->GetReturnType(I).representation())));
    }
  };

  template <typename T>
  static bool AllowsRepresentation(RegisterRepresentation rep) {
    if constexpr (std::is_same_v<T, OpIndex> ||
                  std::is_same_v<T, OptionalOpIndex>) {
      return true;
    } else {
      // T is V<...> or OptionalV<...>
      return T::allows_representation(rep);
    }
  }
#endif  // DEBUG
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_CALL_DESCRIPTORS_UTIL_H_
