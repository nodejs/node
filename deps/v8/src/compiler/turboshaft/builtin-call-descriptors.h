// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_
#define V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_

#include "src/builtins/builtins.h"
#include "src/codegen/callable.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/frame.h"
#include "src/compiler/globals.h"
#include "src/compiler/turbofan-types.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/objects/js-function.h"
#include "src/objects/scope-info.h"
#include "src/objects/turbofan-types.h"

// Use this macro to define Arguments in builtins' Descriptor's Arguments.
#define ARG(type, name)                                                        \
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
  void CollectArguments(arguments_vector_t& args, detail::IndexTag<I>) const { \
  }                                                                            \
  void CollectArguments(arguments_vector_t& args,                              \
                        detail::IndexTag<name##_index>) const {                \
    args.push_back(name);                                                      \
    CollectArguments(args, detail::IndexTag<name##_index + 1>{});              \
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

// TODO(nicohartmann): Consider a different name, but currently everything that
// is not lengthy is already used in so many other places that constant name
// collisions are unavoidable.
struct builtin {
  // TODO(nicohartmann@): Unfortunately, we cannot define builtins with
  // void/never return types properly (e.g. in Torque), but they typically have
  // a JSAny dummy return type. Use Void/Never sentinels to express that in
  // Turboshaft's descriptors. We should find a better way to model this.
  using Void = std::tuple<OpIndex>;
  using Never = std::tuple<OpIndex>;
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
    args.CollectArguments(result, detail::IndexTag<0>{});
    return result;
  }

  template <typename A>
  static constexpr size_t GetArgumentCount() {
    return decltype(A::index_counter(
        detail::IndexTag<kMaxArgumentCount>{}))::value;
  }

  template <typename Derived>
  struct Descriptor {
    static const TSCallDescriptor* Create(
        StubCallMode call_mode, Zone* zone,
        LazyDeoptOnThrow lazy_deopt_on_throw = LazyDeoptOnThrow::kNo,
        bool caller_can_deopt = true) {
      CallInterfaceDescriptor interface_descriptor =
          Builtins::CallInterfaceDescriptorFor(Derived::kFunction);
      auto descriptor = Linkage::GetStubCallDescriptor(
          zone, interface_descriptor,
          interface_descriptor.GetStackParameterCount(),
          (Derived::kCanTriggerLazyDeopt && caller_can_deopt)
              ? CallDescriptor::kNeedsFrameState
              : CallDescriptor::kNoFlags,
          Derived::kProperties, call_mode);
#ifdef DEBUG
      Derived::Verify(descriptor, caller_can_deopt);
#endif  // DEBUG
      bool can_throw = !(Derived::kProperties & Operator::kNoThrow);
      return TSCallDescriptor::Create(
          descriptor, can_throw ? CanThrow::kYes : CanThrow::kNo,
          lazy_deopt_on_throw, zone);
    }

#ifdef DEBUG
    static void Verify(const CallDescriptor* desc, bool caller_can_deopt) {
      // Verify return types.
      using returns_t = base::tmp::from_tuple_t<typename Derived::returns_t>;
      DCHECK_EQ(desc->ReturnCount(), base::tmp::length_v<returns_t>);
      base::tmp::call_foreach<returns_t, VerifyReturn>(desc);

      // Verify argument types.
      using arguments_t = decltype(Derived::Arguments::make_args_type_list_n(
          detail::IndexTag<kMaxArgumentCount>{}));
      const size_t arguments_count =
          builtin::GetArgumentCount<typename Derived::Arguments>();
      DCHECK_EQ(base::tmp::length_v<arguments_t>, arguments_count);
      DCHECK_EQ(desc->ParameterCount(),
                arguments_count + (Derived::kNeedsContext ? 1 : 0));
      base::tmp::call_foreach<arguments_t, VerifyArgument>(desc);

      // Verify properties.
      DCHECK_EQ(desc->NeedsFrameState(),
                (Derived::kCanTriggerLazyDeopt && caller_can_deopt));
      DCHECK_EQ(desc->properties(), Derived::kProperties);
      // TODO(nicohartmann): Unfortunately, the condition is not that simple, we
      // should find a more appropriate way to detect this. Ideally we would use
      // the builtin effect analysis.
      // DCHECK_IMPLIES(Derived::kEffects.can_allocate,
      // Derived::kCanTriggerLazyDeopt);
    }

   private:
    template <typename T, size_t I>
    struct VerifyArgument {
      void operator()(const CallDescriptor* desc) const {
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
      if constexpr (std::is_same_v<T, OpIndex>) {
        return true;
      } else {
        // T is V<...>
        return T::allows_representation(rep);
      }
    }
#endif  // DEBUG
  };

  struct BigIntAdd : public Descriptor<BigIntAdd> {
    static constexpr auto kFunction = Builtin::kBigIntAdd;
    struct Arguments : ArgumentsBase {
      ARG(V<Numeric>, left)
      ARG(V<Numeric>, right)
    };
    using returns_t = std::tuple<V<BigInt>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct CheckTurbofanType : public Descriptor<CheckTurbofanType> {
    static constexpr auto kFunction = Builtin::kCheckTurbofanType;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, value)
      ARG(V<TurbofanType>, expected_type)
      ARG(V<Smi>, node_id)
    };
    using returns_t = std::tuple<V<Object>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoThrow | Operator::kNoDeopt;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().RequiredWhenUnused();
  };

#define DECL_GENERIC_BINOP(Name)                                          \
  struct Name : public Descriptor<Name> {                                 \
    static constexpr auto kFunction = Builtin::k##Name;                   \
    struct Arguments : ArgumentsBase {                                    \
      ARG(V<Object>, left)                                                \
      ARG(V<Object>, right)                                               \
    };                                                                    \
    using returns_t = std::tuple<V<Object>>;                              \
                                                                          \
    static constexpr bool kCanTriggerLazyDeopt = true;                    \
    static constexpr bool kNeedsContext = true;                           \
    static constexpr Operator::Properties kProperties =                   \
        Operator::kNoProperties;                                          \
    static constexpr OpEffects kEffects = base_effects.CanCallAnything(); \
  };
  GENERIC_BINOP_LIST(DECL_GENERIC_BINOP)
#undef DECL_GENERIC_BINOP

#define DECL_GENERIC_UNOP(Name)                                           \
  struct Name : public Descriptor<Name> {                                 \
    static constexpr auto kFunction = Builtin::k##Name;                   \
    struct Arguments : ArgumentsBase {                                    \
      ARG(V<Object>, input)                                               \
    };                                                                    \
    using returns_t = std::tuple<V<Object>>;                              \
                                                                          \
    static constexpr bool kCanTriggerLazyDeopt = true;                    \
    static constexpr bool kNeedsContext = true;                           \
    static constexpr Operator::Properties kProperties =                   \
        Operator::kNoProperties;                                          \
    static constexpr OpEffects kEffects = base_effects.CanCallAnything(); \
  };
  GENERIC_UNOP_LIST(DECL_GENERIC_UNOP)
#undef DECL_GENERIC_UNOP

  struct DetachContextCell : public Descriptor<DetachContextCell> {
    static constexpr auto kFunction = Builtin::kDetachContextCell;
    struct Arguments : ArgumentsBase {
      ARG(V<Context>, the_context)
      ARG(V<Object>, new_value)
      ARG(V<WordPtr>, i)
    };
    using returns_t = std::tuple<V<Object>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteHeapMemory().CanReadMemory();
  };

  struct ToNumber : public Descriptor<ToNumber> {
    static constexpr auto kFunction = Builtin::kToNumber;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, input)
    };
    using returns_t = std::tuple<V<Number>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct NonNumberToNumber : public Descriptor<NonNumberToNumber> {
    static constexpr auto kFunction = Builtin::kNonNumberToNumber;
    struct Arguments : ArgumentsBase {
      ARG(V<JSAnyNotNumber>, input)
    };
    using returns_t = std::tuple<V<Number>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct ToNumeric : public Descriptor<ToNumeric> {
    static constexpr auto kFunction = Builtin::kToNumeric;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, input)
    };
    using returns_t = std::tuple<V<Numeric>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct NonNumberToNumeric : public Descriptor<NonNumberToNumeric> {
    static constexpr auto kFunction = Builtin::kNonNumberToNumeric;
    struct Arguments : ArgumentsBase {
      ARG(V<JSAnyNotNumber>, input)
    };
    using returns_t = std::tuple<V<Numeric>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct CopyFastSmiOrObjectElements
      : public Descriptor<CopyFastSmiOrObjectElements> {
    static constexpr auto kFunction = Builtin::kCopyFastSmiOrObjectElements;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, object)
    };
    using returns_t = std::tuple<V<Object>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanReadMemory().CanAllocate();
  };

  template <Builtin B, typename Input>
  struct DebugPrint : public Descriptor<DebugPrint<B, Input>> {
    static constexpr auto kFunction = B;
    using StringOrSmi = Union<String, Smi>;
    // We use smi:0 for an empty label.
    struct Arguments : ArgumentsBase {
      ARG(V<StringOrSmi>, label_or_0)
      ARG(V<Input>, value)
    };
    using returns_t = std::tuple<V<Object>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoThrow | Operator::kNoDeopt;
    static constexpr OpEffects kEffects =
        base_effects.RequiredWhenUnused().CanAllocate();
  };
  using DebugPrintObject = DebugPrint<Builtin::kDebugPrintObject, Object>;
  using DebugPrintWord32 = DebugPrint<Builtin::kDebugPrintWord32, Word32>;
  using DebugPrintWord64 = DebugPrint<Builtin::kDebugPrintWord64, Word64>;
  using DebugPrintFloat32 = DebugPrint<Builtin::kDebugPrintFloat32, Float32>;
  using DebugPrintFloat64 = DebugPrint<Builtin::kDebugPrintFloat64, Float64>;

  template <Builtin B>
  struct FindOrderedHashEntry : public Descriptor<FindOrderedHashEntry<B>> {
    static constexpr auto kFunction = B;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, table)
      ARG(V<Smi>, key)
    };
    using returns_t = std::tuple<V<Smi>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.AssumesConsistentHeap().CanReadMemory().CanAllocate();
  };
  using FindOrderedHashMapEntry =
      FindOrderedHashEntry<Builtin::kFindOrderedHashMapEntry>;
  using FindOrderedHashSetEntry =
      FindOrderedHashEntry<Builtin::kFindOrderedHashSetEntry>;

  template <Builtin B>
  struct GrowFastElements : public Descriptor<GrowFastElements<B>> {
    static constexpr auto kFunction = B;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, object)
      ARG(V<Smi>, size)
    };
    using returns_t = std::tuple<V<Object>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanReadMemory().CanAllocate();
  };
  using GrowFastDoubleElements =
      GrowFastElements<Builtin::kGrowFastDoubleElements>;
  using GrowFastSmiOrObjectElements =
      GrowFastElements<Builtin::kGrowFastSmiOrObjectElements>;

  template <Builtin B>
  struct NewArgumentsElements : public Descriptor<NewArgumentsElements<B>> {
    static constexpr auto kFunction = B;
    struct Arguments : ArgumentsBase {
      // TODO(nicohartmann@): First argument should be replaced by a proper
      // RawPtr.
      ARG(V<WordPtr>, frame)
      ARG(V<WordPtr>, formal_parameter_count)
      ARG(V<Smi>, arguments_count)
    };
    using returns_t = std::tuple<V<FixedArray>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanAllocate();
  };
  using NewSloppyArgumentsElements =
      NewArgumentsElements<Builtin::kNewSloppyArgumentsElements>;
  using NewStrictArgumentsElements =
      NewArgumentsElements<Builtin::kNewStrictArgumentsElements>;
  using NewRestArgumentsElements =
      NewArgumentsElements<Builtin::kNewRestArgumentsElements>;

  struct NumberToString : public Descriptor<NumberToString> {
    static constexpr auto kFunction = Builtin::kNumberToString;
    struct Arguments : ArgumentsBase {
      ARG(V<Number>, input)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct ToString : public Descriptor<ToString> {
    static constexpr auto kFunction = Builtin::kToString;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, o)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct PlainPrimitiveToNumber : public Descriptor<PlainPrimitiveToNumber> {
    static constexpr auto kFunction = Builtin::kPlainPrimitiveToNumber;
    struct Arguments : ArgumentsBase {
      ARG(V<PlainPrimitive>, input)
    };
    using returns_t = std::tuple<V<Number>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct SameValue : public Descriptor<SameValue> {
    static constexpr auto kFunction = Builtin::kSameValue;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, left)
      ARG(V<Object>, right)
    };
    using returns_t = std::tuple<V<Boolean>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocate();
  };

  struct SameValueNumbersOnly : public Descriptor<SameValueNumbersOnly> {
    static constexpr auto kFunction = Builtin::kSameValueNumbersOnly;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, left)
      ARG(V<Object>, right)
    };
    using returns_t = std::tuple<V<Boolean>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory();
  };

  struct StringAdd_CheckNone : public Descriptor<StringAdd_CheckNone> {
    static constexpr auto kFunction = Builtin::kStringAdd_CheckNone;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, left)
      ARG(V<String>, right)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoWrite;
    // This will only write in a fresh object, so the writes are not visible
    // from Turboshaft, and CanAllocate is enough.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct StringEqual : public Descriptor<StringEqual> {
    static constexpr auto kFunction = Builtin::kStringEqual;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, left)
      ARG(V<String>, right)
      ARG(V<WordPtr>, length)
    };
    using returns_t = std::tuple<V<Boolean>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // If the strings aren't flat, StringEqual could flatten them, which will
    // allocate new strings.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct StringFromCodePointAt : public Descriptor<StringFromCodePointAt> {
    static constexpr auto kFunction = Builtin::kStringFromCodePointAt;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, receiver)
      ARG(V<WordPtr>, position)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct StringIndexOf : public Descriptor<StringIndexOf> {
    static constexpr auto kFunction = Builtin::kStringIndexOf;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, s)
      ARG(V<String>, search_string)
      ARG(V<Smi>, start)
    };
    using returns_t = std::tuple<V<Smi>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // StringIndexOf does a ToString on the receiver, which can allocate a new
    // string.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct StringCompare : public Descriptor<StringCompare> {
    static constexpr auto kFunction = Builtin::kStringCompare;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, left)
      ARG(V<String>, right)
    };
    using returns_t = std::tuple<V<Smi>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  template <Builtin B>
  struct StringComparison : public Descriptor<StringComparison<B>> {
    static constexpr auto kFunction = B;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, left)
      ARG(V<String>, right)
    };
    using returns_t = std::tuple<V<Boolean>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };
  using StringLessThan = StringComparison<Builtin::kStringLessThan>;
  using StringLessThanOrEqual =
      StringComparison<Builtin::kStringLessThanOrEqual>;

  struct StringSubstring : public Descriptor<StringSubstring> {
    static constexpr auto kFunction = Builtin::kStringSubstring;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, string)
      ARG(V<WordPtr>, from)
      ARG(V<WordPtr>, to)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

#ifdef V8_INTL_SUPPORT
  struct StringToLowerCaseIntl : public Descriptor<StringToLowerCaseIntl> {
    static constexpr auto kFunction = Builtin::kStringToLowerCaseIntl;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, string)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };
#endif  // V8_INTL_SUPPORT

  struct StringToNumber : public Descriptor<StringToNumber> {
    static constexpr auto kFunction = Builtin::kStringToNumber;
    struct Arguments : ArgumentsBase {
      ARG(V<String>, input)
    };
    using returns_t = std::tuple<V<Number>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct ToBoolean : public Descriptor<ToBoolean> {
    static constexpr auto kFunction = Builtin::kToBoolean;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, input)
    };
    using returns_t = std::tuple<V<Boolean>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory();
  };

  struct ToObject : public Descriptor<ToObject> {
    static constexpr auto kFunction = Builtin::kToObject;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, input)
    };
    using returns_t = std::tuple<V<JSReceiver>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocate();
  };

  template <Builtin B>
  struct CreateFunctionContext : public Descriptor<CreateFunctionContext<B>> {
    static constexpr auto kFunction = B;
    struct Arguments : ArgumentsBase {
      ARG(V<ScopeInfo>, scope_info)
      ARG(V<Word32>, slots)
    };
    using returns_t = std::tuple<V<Context>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocate();
  };

  using FastNewFunctionContextFunction =
      CreateFunctionContext<Builtin::kFastNewFunctionContextFunction>;
  using FastNewFunctionContextEval =
      CreateFunctionContext<Builtin::kFastNewFunctionContextEval>;

  struct FastNewClosure : public Descriptor<FastNewClosure> {
    static constexpr auto kFunction = Builtin::kFastNewClosure;
    struct Arguments : ArgumentsBase {
      ARG(V<SharedFunctionInfo>, shared_function_info)
      ARG(V<FeedbackCell>, feedback_cell)
    };
    using returns_t = std::tuple<V<JSFunction>>;

    static constexpr bool kCanTriggerLazyDeopt = true;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kEliminatable | Operator::kNoThrow;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteMemory().CanAllocate();
  };

  struct Typeof : public Descriptor<Typeof> {
    static constexpr auto kFunction = Builtin::kTypeof;
    struct Arguments : ArgumentsBase {
      ARG(V<Object>, object)
    };
    using returns_t = std::tuple<V<String>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory();
  };

  struct CheckTurboshaftWord32Type
      : public Descriptor<CheckTurboshaftWord32Type> {
    static constexpr auto kFunction = Builtin::kCheckTurboshaftWord32Type;
    struct Arguments : ArgumentsBase {
      ARG(V<Word32>, value)
      ARG(V<TurboshaftWord32Type>, expected_type)
      ARG(V<Smi>, node_id)
    };
    using returns_t = std::tuple<V<Oddball>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct CheckTurboshaftWord64Type
      : public Descriptor<CheckTurboshaftWord64Type> {
    static constexpr auto kFunction = Builtin::kCheckTurboshaftWord64Type;
    struct Arguments : ArgumentsBase {
      ARG(V<Word32>, value_high)
      ARG(V<Word32>, value_low)
      ARG(V<TurboshaftWord64Type>, expected_type)
      ARG(V<Smi>, node_id)
    };
    using returns_t = std::tuple<V<Oddball>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct CheckTurboshaftFloat32Type
      : public Descriptor<CheckTurboshaftFloat32Type> {
    static constexpr auto kFunction = Builtin::kCheckTurboshaftFloat32Type;
    struct Arguments : ArgumentsBase {
      ARG(V<Float32>, value)
      ARG(V<TurboshaftFloat64Type>, expected_type)
      ARG(V<Smi>, node_id)
    };
    using returns_t = std::tuple<V<Oddball>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };

  struct CheckTurboshaftFloat64Type
      : public Descriptor<CheckTurboshaftFloat64Type> {
    static constexpr auto kFunction = Builtin::kCheckTurboshaftFloat64Type;
    struct Arguments : ArgumentsBase {
      ARG(V<Float64>, value)
      ARG(V<TurboshaftFloat64Type>, expected_type)
      ARG(V<Smi>, node_id)
    };
    using returns_t = std::tuple<V<Oddball>>;

    static constexpr bool kCanTriggerLazyDeopt = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
  };
};

// TODO(nicohartmann): These call descriptors are deprecated and shall be
// replaced with the new version above.
namespace deprecated {
struct BuiltinCallDescriptor {
 private:
  template <typename Derived>
  struct Descriptor {
    static const TSCallDescriptor* Create(
        StubCallMode call_mode, Zone* zone,
        LazyDeoptOnThrow lazy_deopt_on_throw = LazyDeoptOnThrow::kNo,
        bool compiling_builtins = false) {
      CallInterfaceDescriptor interface_descriptor =
          Builtins::CallInterfaceDescriptorFor(Derived::kFunction);
      auto descriptor = Linkage::GetStubCallDescriptor(
          zone, interface_descriptor,
          interface_descriptor.GetStackParameterCount(),
          (Derived::kNeedsFrameState && !compiling_builtins)
              ? CallDescriptor::kNeedsFrameState
              : CallDescriptor::kNoFlags,
          Derived::kProperties, call_mode);
#ifdef DEBUG
      Derived::Verify(descriptor, compiling_builtins);
#endif  // DEBUG
      bool can_throw = !(Derived::kProperties & Operator::kNoThrow);
      return TSCallDescriptor::Create(
          descriptor, can_throw ? CanThrow::kYes : CanThrow::kNo,
          lazy_deopt_on_throw, zone);
    }

#ifdef DEBUG
    static void Verify(const CallDescriptor* desc, bool compiling_builtins) {
      using results_t = typename Derived::results_t;
      using arguments_t = typename Derived::arguments_t;
      DCHECK_EQ(desc->ReturnCount(), std::tuple_size_v<results_t>);
      if constexpr (std::tuple_size_v<results_t> >= 1) {
        using result0_t = std::tuple_element_t<0, results_t>;
        DCHECK(AllowsRepresentation<result0_t>(
            RegisterRepresentation::FromMachineRepresentation(
                desc->GetReturnType(0).representation())));
      }
      if constexpr (std::tuple_size_v<results_t> >= 2) {
        using result1_t = std::tuple_element_t<1, results_t>;
        DCHECK(AllowsRepresentation<result1_t>(
            RegisterRepresentation::FromMachineRepresentation(
                desc->GetReturnType(1).representation())));
      }
      DCHECK_EQ(desc->NeedsFrameState(),
                (Derived::kNeedsFrameState && !compiling_builtins));
      DCHECK_EQ(desc->properties(), Derived::kProperties);
      DCHECK_EQ(desc->ParameterCount(), std::tuple_size_v<arguments_t> +
                                            (Derived::kNeedsContext ? 1 : 0));
      DCHECK(VerifyArguments<arguments_t>(desc));
    }

    template <typename Arguments>
    static bool VerifyArguments(const CallDescriptor* desc) {
      return VerifyArgumentsImpl<Arguments>(
          desc, std::make_index_sequence<std::tuple_size_v<Arguments>>());
    }

   private:
    template <typename T>
    static bool AllowsRepresentation(RegisterRepresentation rep) {
      if constexpr (std::is_same_v<T, OpIndex>) {
        return true;
      } else {
        // T is V<...>
        return T::allows_representation(rep);
      }
    }
    template <typename Arguments, size_t... Indices>
    static bool VerifyArgumentsImpl(const CallDescriptor* desc,
                                    std::index_sequence<Indices...>) {
      return (AllowsRepresentation<std::tuple_element_t<Indices, Arguments>>(
                  RegisterRepresentation::FromMachineRepresentation(
                      desc->GetParameterType(Indices).representation())) &&
              ...);
    }
#endif  // DEBUG
  };

  static constexpr OpEffects base_effects = OpEffects().CanDependOnChecks();
  // TODO(nicohartmann@): Unfortunately, we cannot define builtins with
  // void/never return types properly (e.g. in Torque), but they typically have
  // a JSAny dummy return type. Use Void/Never sentinels to express that in
  // Turboshaft's descriptors. We should find a better way to model this.
  using Void = std::tuple<OpIndex>;
  using Never = std::tuple<OpIndex>;

 public:
#if V8_ENABLE_WEBASSEMBLY
  struct WasmStringAdd_CheckNone : public Descriptor<WasmStringAdd_CheckNone> {
    static constexpr auto kFunction = Builtin::kWasmStringAdd_CheckNone;
    using arguments_t = std::tuple<V<String>, V<String>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoWrite;
    // This will only write in a fresh object, so the writes are not visible
    // from Turboshaft, and CanAllocate is enough.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmJSStringEqual : public Descriptor<WasmJSStringEqual> {
    static constexpr auto kFunction = Builtin::kWasmJSStringEqual;
    using arguments_t = std::tuple<V<String>, V<String>, V<WordPtr>>;
    using results_t = std::tuple<V<Boolean>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // If the strings aren't flat, StringEqual could flatten them, which will
    // allocate new strings.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringCompare : public Descriptor<WasmStringCompare> {
    static constexpr auto kFunction = Builtin::kWasmStringCompare;
    using arguments_t = std::tuple<V<String>, V<String>>;
    using results_t = std::tuple<V<Smi>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringIndexOf : public Descriptor<WasmStringIndexOf> {
    static constexpr auto kFunction = Builtin::kWasmStringIndexOf;
    using arguments_t = std::tuple<V<String>, V<String>, V<Smi>>;
    using results_t = std::tuple<V<Smi>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // StringIndexOf does a ToString on the receiver, which can allocate a new
    // string.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

#ifdef V8_INTL_SUPPORT
  struct WasmStringToLowerCaseIntl
      : public Descriptor<WasmStringToLowerCaseIntl> {
    static constexpr auto kFunction = Builtin::kWasmStringToLowerCaseIntl;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };
#endif

  struct WasmStringAsWtf8 : public Descriptor<WasmStringAsWtf8> {
    static constexpr auto kFunction = Builtin::kWasmStringAsWtf8;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<ByteArray>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringAsWtf16 : public Descriptor<WasmStringAsWtf16> {
    static constexpr auto kFunction = Builtin::kWasmStringAsWtf16;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmInt32ToHeapNumber : public Descriptor<WasmInt32ToHeapNumber> {
    static constexpr auto kFunction = Builtin::kWasmInt32ToHeapNumber;
    using arguments_t = std::tuple<V<Word32>>;
    using results_t = std::tuple<V<HeapNumber>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kPure;
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity();
  };

  struct WasmInt32ToSharedHeapNumber
      : public Descriptor<WasmInt32ToSharedHeapNumber> {
    static constexpr auto kFunction = Builtin::kWasmInt32ToSharedHeapNumber;
    using arguments_t = std::tuple<V<Word32>>;
    using results_t = std::tuple<V<HeapNumber>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kPure;
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity();
  };

  struct WasmRefFunc : public Descriptor<WasmRefFunc> {
    static constexpr auto kFunction = Builtin::kWasmRefFunc;
    using arguments_t = std::tuple<V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<WasmFuncRef>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoThrow;
    // TODO(nicohartmann@): Use more precise effects.
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct WasmAllocateDescriptorStruct
      : public Descriptor<WasmAllocateDescriptorStruct> {
    static constexpr auto kFunction = Builtin::kWasmAllocateDescriptorStruct;
    using arguments_t = std::tuple<V<Map>, V<Word32>, V<Object>>;
    using results_t = std::tuple<V<WasmStruct>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanAllocate();
  };

  struct WasmGetOwnProperty : public Descriptor<WasmGetOwnProperty> {
    static constexpr auto kFunction = Builtin::kWasmGetOwnProperty;
    using arguments_t = std::tuple<V<Object>, V<Symbol>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoThrow;
    // Calls {GetPropertyWithReceiver}, which has paths that can allocate,
    // but from this caller we won't reach them. Nevertheless, to please the
    // verifier we currently have no other choice than setting the CanAllocate
    // effect here.
    // TODO(dmercadier): Support overriding the automatic can-allocate
    // inference.
    static constexpr OpEffects kEffects =
        base_effects.CanReadHeapMemory().CanAllocate();
  };

  struct WasmRethrow : public Descriptor<WasmRethrow> {
    static constexpr auto kFunction = Builtin::kWasmRethrow;
    using arguments_t = std::tuple<V<Object>>;
    using results_t = std::tuple<OpIndex>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanChangeControlFlow();
  };

  struct WasmThrowRef : public Descriptor<WasmThrowRef> {
    static constexpr auto kFunction = Builtin::kWasmThrowRef;
    using arguments_t = std::tuple<V<Object>>;
    using results_t = std::tuple<OpIndex>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct WasmMemoryGrow : public Descriptor<WasmMemoryGrow> {
    static constexpr auto kFunction = Builtin::kWasmMemoryGrow;
    using arguments_t = std::tuple<V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteMemory().CanAllocate();
  };

  struct WasmStringFromCodePoint : public Descriptor<WasmStringFromCodePoint> {
    static constexpr auto kFunction = Builtin::kWasmStringFromCodePoint;
    using arguments_t = std::tuple<V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoWrite;
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity().CanThrowOrTrap();
  };

  struct WasmStringNewWtf8Array : public Descriptor<WasmStringNewWtf8Array> {
    static constexpr auto kFunction = Builtin::kWasmStringNewWtf8Array;
    using arguments_t = std::tuple<V<Word32>, V<Word32>, V<WasmArray>, V<Smi>>;
    using results_t = std::tuple<V<WasmStringRefNullable>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadHeapMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringNewWtf16Array : public Descriptor<WasmStringNewWtf16Array> {
    static constexpr auto kFunction = Builtin::kWasmStringNewWtf16Array;
    using arguments_t = std::tuple<V<WasmArray>, V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadHeapMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringViewWtf8Slice : public Descriptor<WasmStringViewWtf8Slice> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf8Slice;
    using arguments_t = std::tuple<V<ByteArray>, V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringViewWtf16Slice
      : public Descriptor<WasmStringViewWtf16Slice> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf16Slice;
    using arguments_t = std::tuple<V<String>, V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringEncodeWtf8Array
      : public Descriptor<WasmStringEncodeWtf8Array> {
    static constexpr auto kFunction = Builtin::kWasmStringEncodeWtf8Array;
    using arguments_t = std::tuple<V<String>, V<WasmArray>, V<Word32>, V<Smi>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteHeapMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringToUtf8Array : public Descriptor<WasmStringToUtf8Array> {
    static constexpr auto kFunction = Builtin::kWasmStringToUtf8Array;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<WasmArray>>;
    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocate();
  };

  struct WasmStringEncodeWtf16Array
      : public Descriptor<WasmStringEncodeWtf16Array> {
    static constexpr auto kFunction = Builtin::kWasmStringEncodeWtf16Array;
    using arguments_t = std::tuple<V<String>, V<WasmArray>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteHeapMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmFloat64ToString : public Descriptor<WasmFloat64ToString> {
    static constexpr auto kFunction = Builtin::kWasmFloat64ToString;
    using arguments_t = std::tuple<V<Float64>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity();
  };

  struct WasmIntToString : public Descriptor<WasmIntToString> {
    static constexpr auto kFunction = Builtin::kWasmIntToString;
    using arguments_t = std::tuple<V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoDeopt;
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity();
  };

  struct WasmStringToDouble : public Descriptor<WasmStringToDouble> {
    static constexpr auto kFunction = Builtin::kWasmStringToDouble;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<Float64>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmAllocateFixedArray : public Descriptor<WasmAllocateFixedArray> {
    static constexpr auto kFunction = Builtin::kWasmAllocateFixedArray;
    using arguments_t = std::tuple<V<WordPtr>>;
    using results_t = std::tuple<V<FixedArray>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanAllocate();
  };

  struct WasmThrow : public Descriptor<WasmThrow> {
    static constexpr auto kFunction = Builtin::kWasmThrow;
    using arguments_t = std::tuple<V<Object>, V<FixedArray>>;
    using results_t = std::tuple<OpIndex>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadHeapMemory().CanThrowOrTrap();
  };

  struct WasmI32AtomicWait : public Descriptor<WasmI32AtomicWait> {
    static constexpr auto kFunction = Builtin::kWasmI32AtomicWait;
    using arguments_t = std::tuple<V<Word32>, V<WordPtr>, V<Word32>, V<BigInt>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct WasmI64AtomicWait : public Descriptor<WasmI64AtomicWait> {
    static constexpr auto kFunction = Builtin::kWasmI64AtomicWait;
    using arguments_t = std::tuple<V<Word32>, V<WordPtr>, V<BigInt>, V<BigInt>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct WasmFunctionTableGet : public Descriptor<WasmFunctionTableGet> {
    static constexpr auto kFunction = Builtin::kWasmFunctionTableGet;
    using arguments_t = std::tuple<V<WordPtr>, V<WordPtr>, V<Word32>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmTableSetFuncRef : public Descriptor<WasmTableSetFuncRef> {
    static constexpr auto kFunction = Builtin::kWasmTableSetFuncRef;
    using arguments_t =
        std::tuple<V<WordPtr>, V<Word32>, V<WordPtr>, V<WasmFuncRef>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmTableSet : public Descriptor<WasmTableSet> {
    static constexpr auto kFunction = Builtin::kWasmTableSet;
    using arguments_t =
        std::tuple<V<WordPtr>, V<Word32>, V<WordPtr>, V<Object>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmTableInit : public Descriptor<WasmTableInit> {
    static constexpr auto kFunction = Builtin::kWasmTableInit;
    using arguments_t =
        std::tuple<V<WordPtr>, V<Word32>, V<Word32>, V<Smi>, V<Smi>, V<Smi>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmTableCopy : public Descriptor<WasmTableCopy> {
    static constexpr auto kFunction = Builtin::kWasmTableCopy;
    using arguments_t =
        std::tuple<V<WordPtr>, V<WordPtr>, V<WordPtr>, V<Smi>, V<Smi>, V<Smi>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmTableGrow : public Descriptor<WasmTableGrow> {
    static constexpr auto kFunction = Builtin::kWasmTableGrow;
    using arguments_t = std::tuple<V<Smi>, V<WordPtr>, V<Word32>, V<Object>>;
    using results_t = std::tuple<V<Smi>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteMemory().CanAllocate();
  };

  struct WasmTableFill : public Descriptor<WasmTableFill> {
    static constexpr auto kFunction = Builtin::kWasmTableFill;
    using arguments_t =
        std::tuple<V<WordPtr>, V<WordPtr>, V<Word32>, V<Smi>, V<Object>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteMemory().CanThrowOrTrap();
  };

  struct WasmArrayNewSegment : public Descriptor<WasmArrayNewSegment> {
    static constexpr auto kFunction = Builtin::kWasmArrayNewSegment;
    using arguments_t =
        std::tuple<V<Word32>, V<Word32>, V<Word32>, V<Smi>, V<Smi>, V<Map>>;
    using results_t = std::tuple<V<WasmArray>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadHeapMemory().CanAllocate().CanThrowOrTrap();
  };

  struct WasmArrayInitSegment : public Descriptor<WasmArrayInitSegment> {
    static constexpr auto kFunction = Builtin::kWasmArrayInitSegment;
    using arguments_t = std::tuple<V<Word32>, V<Word32>, V<Word32>, V<Smi>,
                                   V<Smi>, V<Smi>, V<HeapObject>>;
    using results_t = std::tuple<V<Object>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanWriteHeapMemory().CanReadHeapMemory().CanThrowOrTrap();
  };

  struct WasmStringNewWtf8 : public Descriptor<WasmStringNewWtf8> {
    static constexpr auto kFunction = Builtin::kWasmStringNewWtf8;
    using arguments_t = std::tuple<V<WordPtr>, V<Word32>, V<Word32>, V<Smi>>;
    using results_t = std::tuple<V<WasmStringRefNullable>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanLeaveCurrentFunction();
  };

  struct WasmStringNewWtf16 : public Descriptor<WasmStringNewWtf16> {
    static constexpr auto kFunction = Builtin::kWasmStringNewWtf16;
    using arguments_t = std::tuple<V<Word32>, V<WordPtr>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadHeapMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanLeaveCurrentFunction();
  };

  struct WasmStringFromDataSegment
      : public Descriptor<WasmStringFromDataSegment> {
    static constexpr auto kFunction = Builtin::kWasmStringFromDataSegment;
    using arguments_t =
        std::tuple<V<Word32>, V<Word32>, V<Word32>, V<Smi>, V<Smi>, V<Smi>>;
    using results_t = std::tuple<V<WasmStringRefNullable>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoDeopt;
    // No "CanReadMemory" because data segments are immutable.
    static constexpr OpEffects kEffects =
        base_effects.CanAllocateWithoutIdentity().RequiredWhenUnused();
  };

  struct WasmStringConst : public Descriptor<WasmStringConst> {
    static constexpr auto kFunction = Builtin::kWasmStringConst;
    using arguments_t = std::tuple<V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects =
        base_effects.CanReadHeapMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringMeasureUtf8 : public Descriptor<WasmStringMeasureUtf8> {
    static constexpr auto kFunction = Builtin::kWasmStringMeasureUtf8;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringMeasureWtf8 : public Descriptor<WasmStringMeasureWtf8> {
    static constexpr auto kFunction = Builtin::kWasmStringMeasureWtf8;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringEncodeWtf8 : public Descriptor<WasmStringEncodeWtf8> {
    static constexpr auto kFunction = Builtin::kWasmStringEncodeWtf8;
    using arguments_t = std::tuple<V<WordPtr>, V<Word32>, V<Word32>, V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringEncodeWtf16 : public Descriptor<WasmStringEncodeWtf16> {
    static constexpr auto kFunction = Builtin::kWasmStringEncodeWtf16;
    using arguments_t = std::tuple<V<String>, V<WordPtr>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringEqual : public Descriptor<WasmStringEqual> {
    static constexpr auto kFunction = Builtin::kWasmStringEqual;
    using arguments_t = std::tuple<V<String>, V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringIsUSVSequence : public Descriptor<WasmStringIsUSVSequence> {
    static constexpr auto kFunction = Builtin::kWasmStringIsUSVSequence;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringViewWtf8Advance
      : public Descriptor<WasmStringViewWtf8Advance> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf8Advance;
    using arguments_t = std::tuple<V<ByteArray>, V<Word32>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory();
  };

  struct WasmStringViewWtf8Encode
      : public Descriptor<WasmStringViewWtf8Encode> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf8Encode;
    using arguments_t = std::tuple<V<WordPtr>, V<Word32>, V<Word32>,
                                   V<ByteArray>, V<Smi>, V<Smi>>;
    using results_t = std::tuple<V<Word32>, V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringViewWtf16Encode
      : public Descriptor<WasmStringViewWtf16Encode> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf16Encode;
    using arguments_t =
        std::tuple<V<WordPtr>, V<Word32>, V<Word32>, V<String>, V<Smi>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties =
        Operator::kNoDeopt | Operator::kNoThrow;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringViewWtf16GetCodeUnit
      : public Descriptor<WasmStringViewWtf16GetCodeUnit> {
    static constexpr auto kFunction = Builtin::kWasmStringViewWtf16GetCodeUnit;
    using arguments_t = std::tuple<V<String>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringCodePointAt : public Descriptor<WasmStringCodePointAt> {
    static constexpr auto kFunction = Builtin::kWasmStringCodePointAt;
    using arguments_t = std::tuple<V<String>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanAllocateWithoutIdentity()
                                              .CanThrowOrTrap();
  };

  struct WasmStringAsIter : public Descriptor<WasmStringAsIter> {
    static constexpr auto kFunction = Builtin::kWasmStringAsIter;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<WasmStringViewIter>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanAllocate();
  };

  struct WasmStringViewIterNext : public Descriptor<WasmStringViewIterNext> {
    static constexpr auto kFunction = Builtin::kWasmStringViewIterNext;
    using arguments_t = std::tuple<V<WasmStringViewIter>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanWriteHeapMemory().CanAllocate();
  };

  struct WasmStringViewIterAdvance
      : public Descriptor<WasmStringViewIterAdvance> {
    static constexpr auto kFunction = Builtin::kWasmStringViewIterAdvance;
    using arguments_t = std::tuple<V<WasmStringViewIter>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteHeapMemory()
                                              .CanAllocateWithoutIdentity();
  };

  struct WasmStringViewIterRewind
      : public Descriptor<WasmStringViewIterRewind> {
    static constexpr auto kFunction = Builtin::kWasmStringViewIterRewind;
    using arguments_t = std::tuple<V<WasmStringViewIter>, V<Word32>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    // Can flatten the string, causing allocation.
    static constexpr OpEffects kEffects = base_effects.CanReadMemory()
                                              .CanWriteHeapMemory()
                                              .CanAllocateWithoutIdentity();
  };

  struct WasmStringViewIterSlice : public Descriptor<WasmStringViewIterSlice> {
    static constexpr auto kFunction = Builtin::kWasmStringViewIterSlice;
    using arguments_t = std::tuple<V<WasmStringViewIter>, V<Word32>>;
    using results_t = std::tuple<V<String>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects =
        base_effects.CanReadMemory().CanAllocateWithoutIdentity();
  };

  struct WasmStringHash : public Descriptor<WasmStringHash> {
    static constexpr auto kFunction = Builtin::kWasmStringHash;
    using arguments_t = std::tuple<V<String>>;
    using results_t = std::tuple<V<Word32>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kEliminatable;
    static constexpr OpEffects kEffects = base_effects.CanReadMemory();
  };

  struct ThrowDataViewDetachedError
      : public Descriptor<ThrowDataViewDetachedError> {
    static constexpr auto kFunction = Builtin::kThrowDataViewDetachedError;
    using arguments_t = std::tuple<>;
    using results_t = std::tuple<OpIndex>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct ThrowDataViewOutOfBounds
      : public Descriptor<ThrowDataViewOutOfBounds> {
    static constexpr auto kFunction = Builtin::kThrowDataViewOutOfBounds;
    using arguments_t = std::tuple<>;
    using results_t = Never;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct ThrowDataViewTypeError : public Descriptor<ThrowDataViewTypeError> {
    static constexpr auto kFunction = Builtin::kThrowDataViewTypeError;
    using arguments_t = std::tuple<V<JSDataView>>;
    using results_t = Never;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects =
        base_effects.CanReadHeapMemory().CanThrowOrTrap();
  };

  struct ThrowIndexOfCalledOnNull
      : public Descriptor<ThrowIndexOfCalledOnNull> {
    static constexpr auto kFunction = Builtin::kThrowIndexOfCalledOnNull;
    using arguments_t = std::tuple<>;
    using results_t = Never;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoWrite;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct ThrowToLowerCaseCalledOnNull
      : public Descriptor<ThrowToLowerCaseCalledOnNull> {
    static constexpr auto kFunction = Builtin::kThrowToLowerCaseCalledOnNull;
    using arguments_t = std::tuple<>;
    using results_t = Never;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoWrite;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct WasmFastApiCallTypeCheckAndUpdateIC
      : public Descriptor<WasmFastApiCallTypeCheckAndUpdateIC> {
    static constexpr auto kFunction =
        Builtin::kWasmFastApiCallTypeCheckAndUpdateIC;
    using arguments_t = std::tuple<V<Object>, V<Object>>;
    using results_t = std::tuple<V<Smi>>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = true;
    static constexpr Operator::Properties kProperties = Operator::kNoWrite;
    static constexpr OpEffects kEffects = base_effects.CanThrowOrTrap();
  };

  struct WasmPropagateException : public Descriptor<WasmPropagateException> {
    static constexpr auto kFunction = Builtin::kWasmPropagateException;
    using arguments_t = std::tuple<>;
    using results_t = Never;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

  struct WasmFXResume : public Descriptor<WasmFXResume> {
    static constexpr auto kFunction = Builtin::kWasmFXResume;
    using arguments_t = std::tuple<V<WordPtr>>;  // StackMemory to be resumed.
    using results_t = std::tuple<>;

    static constexpr bool kNeedsFrameState = false;
    static constexpr bool kNeedsContext = false;
    static constexpr Operator::Properties kProperties = Operator::kNoProperties;
    static constexpr OpEffects kEffects = base_effects.CanCallAnything();
  };

#endif  // V8_ENABLE_WEBASSEMBLY
};

}  // namespace deprecated

}  // namespace v8::internal::compiler::turboshaft

#undef ARG

#endif  // V8_COMPILER_TURBOSHAFT_BUILTIN_CALL_DESCRIPTORS_H_
