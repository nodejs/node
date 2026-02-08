// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_
#define V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_

#include "src/compiler/globals.h"
#include "src/compiler/operator.h"
#include "src/compiler/turboshaft/call-descriptors-util.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/runtime/runtime.h"

// Use this macro to define Arguments in runtime function's Descriptor's
// Arguments. For functions without arguments, use `runtime::NoArguments`.
#define ARG(type, name) DEFINE_TURBOSHAFT_CALL_DESCRIPTOR_ARG(type, name)

namespace v8::internal::compiler::turboshaft {

struct runtime : CallDescriptorBuilder {
  // TODO(nicohartmann@): Unfortunately, we cannot define runtime functions with
  // void/never return types properly, but they typically have
  // a JSAny dummy return type. Use Void/Never sentinels to express that in
  // Turboshaft's descriptors. We should find a better way to model this.
  using Void = V<Any>;
  using Never = V<Any>;

  template <typename Derived>
  struct Descriptor {
    static const TSCallDescriptor* Create(size_t actual_argument_count,
                                          Zone* zone,
                                          LazyDeoptOnThrow lazy_deopt_on_throw,
                                          bool caller_can_deopt = true) {
      DCHECK_IMPLIES(lazy_deopt_on_throw == LazyDeoptOnThrow::kYes,
                     Derived::kCanTriggerLazyDeopt);
      auto descriptor = Linkage::GetRuntimeCallDescriptor(
          zone, Derived::kFunction, static_cast<int>(actual_argument_count),
          Derived::kProperties,
          (Derived::kCanTriggerLazyDeopt && caller_can_deopt)
              ? CallDescriptor::kNeedsFrameState
              : CallDescriptor::kNoFlags);
#ifdef DEBUG
      Derived::Verify(descriptor, actual_argument_count, caller_can_deopt);
#endif  // DEBUG
      CanThrow can_throw = (Derived::kProperties & Operator::kNoThrow)
                               ? CanThrow::kNo
                               : CanThrow::kYes;
      return TSCallDescriptor::Create(descriptor, can_throw,
                                      lazy_deopt_on_throw, zone);
    }

#ifdef DEBUG
    static void Verify(const CallDescriptor* desc, size_t actual_argument_count,
                       bool caller_can_deopt) {
      // Verify return types.
      using returns_t = typename Derived::returns_t;
      DCHECK_EQ(desc->ReturnCount(), 1);
      VerifyReturn<returns_t, 0>{}(desc);

      // Verify argument types.
      using arguments_t = decltype(Derived::Arguments::make_args_type_list_n(
          detail::IndexTag<kMaxArgumentCount>{}));
      const size_t arguments_count =
          runtime::GetArgumentCount<typename Derived::Arguments>();
      DCHECK_EQ(base::tmp::length_v<arguments_t>, arguments_count);
      DCHECK_LE(actual_argument_count, arguments_count);
      constexpr int additional_stub_arguments =
          3;  // function id, argument count, context (or NoContextConstant)
      DCHECK_EQ(desc->ParameterCount(),
                actual_argument_count + additional_stub_arguments);
      base::tmp::call_foreach<arguments_t, VerifyArgument>(
          desc, actual_argument_count);

      // Verify properties.
      DCHECK_EQ(desc->NeedsFrameState(),
                (Derived::kCanTriggerLazyDeopt && caller_can_deopt));
      DCHECK_EQ(desc->properties(), Derived::kProperties);
    }
#endif  // DEBUG
  };

  struct Abort : public Descriptor<Abort> {
    static constexpr auto kFunction = Runtime::kAbort;
    struct Arguments : ArgumentsBase {
      ARG(V<Smi>, messageOrMessageId)
    };
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct BigIntUnaryOp : public Descriptor<BigIntUnaryOp> {
    static constexpr auto kFunction = Runtime::kBigIntUnaryOp;
    struct Arguments : ArgumentsBase {
      ARG(V<BigInt>, x)
      ARG(V<Smi>, opcode)
    };
    using returns_t = V<BigInt>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct DateCurrentTime : public Descriptor<DateCurrentTime> {
    static constexpr auto kFunction = Runtime::kDateCurrentTime;
    using Arguments = NoArguments;
    using returns_t = V<Number>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct NumberToStringSlow : public Descriptor<NumberToStringSlow> {
    static constexpr auto kFunction = Runtime::kNumberToStringSlow;
    struct Arguments : ArgumentsBase {
      ARG(V<Number>, input)
    };
    using returns_t = V<String>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct StackGuard : public Descriptor<StackGuard> {
    static constexpr auto kFunction = Runtime::kStackGuard;
    using Arguments = NoArguments;
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    // TODO(nicohartmann@): Verify this.
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct StackGuardWithGap : public Descriptor<StackGuardWithGap> {
    static constexpr auto kFunction = Runtime::kStackGuardWithGap;
    struct Arguments : ArgumentsBase {
      ARG(V<Smi>, gap)
    };
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    // TODO(nicohartmann@): Verify this.
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct HandleNoHeapWritesInterrupts
      : public Descriptor<HandleNoHeapWritesInterrupts> {
    static constexpr auto kFunction = Runtime::kHandleNoHeapWritesInterrupts;
    using Arguments = NoArguments;
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoWrite;
  };

  struct PropagateException : public Descriptor<PropagateException> {
    static constexpr auto kFunction = Runtime::kPropagateException;
    using Arguments = NoArguments;
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ReThrow : public Descriptor<ReThrow> {
    static constexpr auto kFunction = Runtime::kReThrow;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, exception)
    };
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kNoThrow;
  };

  struct StringCharCodeAt : public Descriptor<StringCharCodeAt> {
    static constexpr auto kFunction = Runtime::kStringCharCodeAt;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, string)
      ARG(V<Number>, index)
    };
    using returns_t = V<Smi>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

#ifdef V8_INTL_SUPPORT
  struct StringToUpperCaseIntl : public Descriptor<StringToUpperCaseIntl> {
    static constexpr auto kFunction = Runtime::kStringToUpperCaseIntl;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, string)
    };
    using returns_t = V<String>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties =
        Operator::kFoldable | Operator::kIdempotent;
  };
#endif  // V8_INTL_SUPPORT

  struct SymbolDescriptiveString : public Descriptor<SymbolDescriptiveString> {
    static constexpr auto kFunction = Runtime::kSymbolDescriptiveString;
    struct Arguments : ArgumentsBase {
      ARG(V<Symbol>, symbol)
    };
    using returns_t = V<String>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoDeopt;
  };

  struct TerminateExecution : public Descriptor<TerminateExecution> {
    static constexpr auto kFunction = Runtime::kTerminateExecution;
    using Arguments = NoArguments;
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoDeopt;
  };

  struct TransitionElementsKind : public Descriptor<TransitionElementsKind> {
    static constexpr auto kFunction = Runtime::kTransitionElementsKind;
    struct Arguments : ArgumentsBase {
      ARG(V<HeapObject>, object)
      ARG(V<Map>, target_map)
    };
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct TryMigrateInstance : public Descriptor<TryMigrateInstance> {
    static constexpr auto kFunction = Runtime::kTryMigrateInstance;
    struct Arguments : ArgumentsBase {
      ARG(V<HeapObject>, heap_object)
    };
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct TryMigrateInstanceAndMarkMapAsMigrationTarget
      : public Descriptor<TryMigrateInstanceAndMarkMapAsMigrationTarget> {
    static constexpr auto kFunction =
        Runtime::kTryMigrateInstanceAndMarkMapAsMigrationTarget;
    struct Arguments : ArgumentsBase {
      ARG(V<HeapObject>, heap_object)
    };
    using returns_t = V<Object>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct ThrowAccessedUninitializedVariable
      : public Descriptor<ThrowAccessedUninitializedVariable> {
    static constexpr auto kFunction =
        Runtime::kThrowAccessedUninitializedVariable;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, object)
    };
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowConstructorReturnedNonObject
      : public Descriptor<ThrowConstructorReturnedNonObject> {
    static constexpr auto kFunction =
        Runtime::kThrowConstructorReturnedNonObject;
    using Arguments = NoArguments;
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowNotSuperConstructor
      : public Descriptor<ThrowNotSuperConstructor> {
    static constexpr auto kFunction = Runtime::kThrowNotSuperConstructor;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, constructor)
      ARG(V<Object>, function)
    };
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowSuperAlreadyCalledError
      : public Descriptor<ThrowSuperAlreadyCalledError> {
    static constexpr auto kFunction = Runtime::kThrowSuperAlreadyCalledError;
    using Arguments = NoArguments;
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowSuperNotCalled : public Descriptor<ThrowSuperNotCalled> {
    static constexpr auto kFunction = Runtime::kThrowSuperNotCalled;
    using Arguments = NoArguments;
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowCalledNonCallable : public Descriptor<ThrowCalledNonCallable> {
    static constexpr auto kFunction = Runtime::kThrowCalledNonCallable;
    using arguments_t = std::tuple<V<Object>>;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, value)
    };
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowInvalidStringLength
      : public Descriptor<ThrowInvalidStringLength> {
    static constexpr auto kFunction = Runtime::kThrowInvalidStringLength;
    using Arguments = NoArguments;
    // Doesn't actually return something, but the actual runtime call descriptor
    // (returned by Linkage::GetRuntimeCallDescriptor) returns 1 instead of 0.
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowRangeError : public Descriptor<ThrowRangeError> {
    static constexpr auto kFunction = Runtime::kThrowRangeError;
    struct Arguments : ArgumentsBase {
      ARG(V<Smi>, template_index)
    };
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ThrowTypeError : public Descriptor<ThrowTypeError> {
    static constexpr auto kFunction = Runtime::kThrowTypeError;
    struct Arguments : ArgumentsBase {
      ARG(V<Smi>, template_index)
      ARG(OptionalV<Object>, arg0)
      ARG(OptionalV<Object>, arg1)
      ARG(OptionalV<Object>, arg2)
    };
    using returns_t = Never;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct NewClosure : public Descriptor<NewClosure> {
    static constexpr auto kFunction = Runtime::kNewClosure;
    struct Arguments : ArgumentsBase {
      ARG(V<SharedFunctionInfo>, shared_function_info)
      ARG(V<FeedbackCell>, feedback_cell)
    };
    using returns_t = V<JSFunction>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kNoThrow;
  };

  struct NewClosure_Tenured : public Descriptor<NewClosure_Tenured> {
    static constexpr auto kFunction = Runtime::kNewClosure_Tenured;
    struct Arguments : ArgumentsBase {
      ARG(V<SharedFunctionInfo>, shared_function_info)
      ARG(V<FeedbackCell>, feedback_cell)
    };
    using returns_t = V<JSFunction>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kNoThrow;
  };

  struct HasInPrototypeChain : public Descriptor<HasInPrototypeChain> {
    static constexpr auto kFunction = Runtime::kHasInPrototypeChain;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, object)
      ARG(V<HeapObject>, prototype)
    };
    using returns_t = V<Boolean>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct ToString : public Descriptor<ToString> {
    static constexpr auto kFunction = Runtime::kToString;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, input)
    };
    using returns_t = V<String>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
  };

  struct MajorGCForCompilerTesting
      : public Descriptor<MajorGCForCompilerTesting> {
    static constexpr auto kFunction = Runtime::kMajorGCForCompilerTesting;
    using Arguments = NoArguments;
    using returns_t = V<Object>;

    // A GC never triggers a lazy deoptimization for the topmost optimized
    // frame.
    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr Operator::Properties kProperties = Operator::kFoldable;
  };
};

}  // namespace v8::internal::compiler::turboshaft

#undef ARG

#endif  // V8_COMPILER_TURBOSHAFT_RUNTIME_CALL_DESCRIPTORS_H_
