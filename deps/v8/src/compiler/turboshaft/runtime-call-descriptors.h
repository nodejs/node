// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_
#define V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_

#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

struct RuntimeCallDescriptor {
 private:
  template <typename Derived>
  struct Descriptor {
    static const TSCallDescriptor* Create(Zone* zone) {
      auto descriptor = Linkage::GetRuntimeCallDescriptor(
          zone, Derived::Function,
          std::tuple_size_v<typename Derived::arguments_t>, Derived::Properties,
          Derived::NeedsFrameState ? CallDescriptor::kNeedsFrameState
                                   : CallDescriptor::kNoFlags);
#ifdef DEBUG
      Derived::Verify(descriptor);
#endif  // DEBUG
      return TSCallDescriptor::Create(descriptor, zone);
    }

#ifdef DEBUG
    static void Verify(const CallDescriptor* desc) {
      using result_t = typename Derived::result_t;
      using arguments_t = typename Derived::arguments_t;
      if constexpr (std::is_same_v<result_t, void>) {
        DCHECK_EQ(desc->ReturnCount(), 0);
      } else {
        DCHECK_EQ(desc->ReturnCount(), 1);
        DCHECK(result_t::allows_representation(
            RegisterRepresentation::FromMachineRepresentation(
                desc->GetReturnType(0).representation())));
      }
      DCHECK_EQ(desc->NeedsFrameState(), Derived::NeedsFrameState);
      DCHECK_EQ(desc->properties(), Derived::Properties);
      constexpr int additional_stub_arguments =
          3;  // function id, argument count, context (or NoContextConstant)
      DCHECK_EQ(desc->ParameterCount(),
                std::tuple_size_v<arguments_t> + additional_stub_arguments);
      DCHECK(VerifyArguments<arguments_t>(desc));
    }

    template <typename Arguments>
    static bool VerifyArguments(const CallDescriptor* desc) {
      return VerifyArgumentsImpl<Arguments>(
          desc, std::make_index_sequence<std::tuple_size_v<Arguments>>());
    }

   private:
    template <typename Arguments, size_t... Indices>
    static bool VerifyArgumentsImpl(const CallDescriptor* desc,
                                    std::index_sequence<Indices...>) {
      return (std::tuple_element_t<Indices, Arguments>::allows_representation(
                  RegisterRepresentation::FromMachineRepresentation(
                      desc->GetParameterType(Indices).representation())) &&
              ...);
    }
#endif  // DEBUG
  };

  using Boolean = Oddball;

 public:
  struct StringCharCodeAt : public Descriptor<StringCharCodeAt> {
    static constexpr auto Function = Runtime::kStringCharCodeAt;
    using arguments_t = std::tuple<V<String>, V<Number>>;
    using result_t = V<Smi>;

    static constexpr bool NeedsFrameState = false;
    static constexpr Operator::Properties Properties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

#ifdef V8_INTL_SUPPORT
  struct StringToUpperCaseIntl : public Descriptor<StringToUpperCaseIntl> {
    static constexpr auto Function = Runtime::kStringToUpperCaseIntl;
    using arguments_t = std::tuple<V<String>>;
    using result_t = V<String>;

    static constexpr bool NeedsFrameState = false;
    static constexpr Operator::Properties Properties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };
#endif  // V8_INTL_SUPPORT

  struct TerminateExecution : public Descriptor<TerminateExecution> {
    static constexpr auto Function = Runtime::kTerminateExecution;
    using arguments_t = std::tuple<>;
    using result_t = V<Object>;

    static constexpr bool NeedsFrameState = true;
    static constexpr Operator::Properties Properties = Operator::kNoDeopt;
  };
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_
