// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This file provides additional API on top of the default one for making
 * API calls, which come from embedder C++ functions. The functions are being
 * called directly from optimized code, doing all the necessary typechecks
 * in the compiler itself, instead of on the embedder side. Hence the "fast"
 * in the name. Example usage might look like:
 *
 * \code
 *    void FastMethod(int param, bool another_param);
 *
 *    v8::FunctionTemplate::New(isolate, SlowCallback, data,
 *                              signature, length, constructor_behavior
 *                              side_effect_type,
 *                              &v8::CFunction::Make(FastMethod));
 * \endcode
 *
 * By design, fast calls are limited by the following requirements, which
 * the embedder should enforce themselves:
 *   - they should not allocate on the JS heap;
 *   - they should not trigger JS execution.
 * To enforce them, the embedder could use the existing
 * v8::Isolate::DisallowJavascriptExecutionScope and a utility similar to
 * Blink's NoAllocationScope:
 * https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/platform/heap/thread_state_scopes.h;l=16
 *
 * Due to these limitations, it's not directly possible to report errors by
 * throwing a JS exception or to otherwise do an allocation. There is an
 * alternative way of creating fast calls that supports falling back to the
 * slow call and then performing the necessary allocation. When one creates
 * the fast method by using CFunction::MakeWithFallbackSupport instead of
 * CFunction::Make, the fast callback gets as last parameter an output variable,
 * through which it can request falling back to the slow call. So one might
 * declare their method like:
 *
 * \code
 *    void FastMethodWithFallback(int param, FastApiCallbackOptions& options);
 * \endcode
 *
 * If the callback wants to signal an error condition or to perform an
 * allocation, it must set options.fallback to true and do an early return from
 * the fast method. Then V8 checks the value of options.fallback and if it's
 * true, falls back to executing the SlowCallback, which is capable of reporting
 * the error (either by throwing a JS exception or logging to the console) or
 * doing the allocation. It's the embedder's responsibility to ensure that the
 * fast callback is idempotent up to the point where error and fallback
 * conditions are checked, because otherwise executing the slow callback might
 * produce visible side-effects twice.
 *
 * An example for custom embedder type support might employ a way to wrap/
 * unwrap various C++ types in JSObject instances, e.g:
 *
 * \code
 *
 *    // Helper method with a check for field count.
 *    template <typename T, int offset>
 *    inline T* GetInternalField(v8::Local<v8::Object> wrapper) {
 *      assert(offset < wrapper->InternalFieldCount());
 *      return reinterpret_cast<T*>(
 *          wrapper->GetAlignedPointerFromInternalField(offset));
 *    }
 *
 *    class CustomEmbedderType {
 *     public:
 *      // Returns the raw C object from a wrapper JS object.
 *      static CustomEmbedderType* Unwrap(v8::Local<v8::Object> wrapper) {
 *        return GetInternalField<CustomEmbedderType,
 *                                kV8EmbedderWrapperObjectIndex>(wrapper);
 *      }
 *      static void FastMethod(v8::Local<v8::Object> receiver_obj, int param) {
 *        CustomEmbedderType* receiver = static_cast<CustomEmbedderType*>(
 *          receiver_obj->GetAlignedPointerFromInternalField(
 *            kV8EmbedderWrapperObjectIndex));
 *
 *        // Type checks are already done by the optimized code.
 *        // Then call some performance-critical method like:
 *        // receiver->Method(param);
 *      }
 *
 *      static void SlowMethod(
 *          const v8::FunctionCallbackInfo<v8::Value>& info) {
 *        v8::Local<v8::Object> instance =
 *          v8::Local<v8::Object>::Cast(info.Holder());
 *        CustomEmbedderType* receiver = Unwrap(instance);
 *        // TODO: Do type checks and extract {param}.
 *        receiver->Method(param);
 *      }
 *    };
 *
 *    // TODO(mslekova): Clean-up these constants
 *    // The constants kV8EmbedderWrapperTypeIndex and
 *    // kV8EmbedderWrapperObjectIndex describe the offsets for the type info
 *    // struct and the native object, when expressed as internal field indices
 *    // within a JSObject. The existance of this helper function assumes that
 *    // all embedder objects have their JSObject-side type info at the same
 *    // offset, but this is not a limitation of the API itself. For a detailed
 *    // use case, see the third example.
 *    static constexpr int kV8EmbedderWrapperTypeIndex = 0;
 *    static constexpr int kV8EmbedderWrapperObjectIndex = 1;
 *
 *    // The following setup function can be templatized based on
 *    // the {embedder_object} argument.
 *    void SetupCustomEmbedderObject(v8::Isolate* isolate,
 *                                   v8::Local<v8::Context> context,
 *                                   CustomEmbedderType* embedder_object) {
 *      isolate->set_embedder_wrapper_type_index(
 *        kV8EmbedderWrapperTypeIndex);
 *      isolate->set_embedder_wrapper_object_index(
 *        kV8EmbedderWrapperObjectIndex);
 *
 *      v8::CFunction c_func =
 *        MakeV8CFunction(CustomEmbedderType::FastMethod);
 *
 *      Local<v8::FunctionTemplate> method_template =
 *        v8::FunctionTemplate::New(
 *          isolate, CustomEmbedderType::SlowMethod, v8::Local<v8::Value>(),
 *          v8::Local<v8::Signature>(), 1, v8::ConstructorBehavior::kAllow,
 *          v8::SideEffectType::kHasSideEffect, &c_func);
 *
 *      v8::Local<v8::ObjectTemplate> object_template =
 *        v8::ObjectTemplate::New(isolate);
 *      object_template->SetInternalFieldCount(
 *        kV8EmbedderWrapperObjectIndex + 1);
 *      object_template->Set(isolate, "method", method_template);
 *
 *      // Instantiate the wrapper JS object.
 *      v8::Local<v8::Object> object =
 *          object_template->NewInstance(context).ToLocalChecked();
 *      object->SetAlignedPointerInInternalField(
 *        kV8EmbedderWrapperObjectIndex,
 *        reinterpret_cast<void*>(embedder_object));
 *
 *      // TODO: Expose {object} where it's necessary.
 *    }
 * \endcode
 *
 * For instance if {object} is exposed via a global "obj" variable,
 * one could write in JS:
 *   function hot_func() {
 *     obj.method(42);
 *   }
 * and once {hot_func} gets optimized, CustomEmbedderType::FastMethod
 * will be called instead of the slow version, with the following arguments:
 *   receiver := the {embedder_object} from above
 *   param := 42
 *
 * Currently supported return types:
 *   - void
 *   - bool
 *   - int32_t
 *   - uint32_t
 *   - float32_t
 *   - float64_t
 * Currently supported argument types:
 *  - pointer to an embedder type
 *  - bool
 *  - int32_t
 *  - uint32_t
 *  - int64_t
 *  - uint64_t
 *  - float32_t
 *  - float64_t
 *
 * The 64-bit integer types currently have the IDL (unsigned) long long
 * semantics: https://heycam.github.io/webidl/#abstract-opdef-converttoint
 * In the future we'll extend the API to also provide conversions from/to
 * BigInt to preserve full precision.
 * The floating point types currently have the IDL (unrestricted) semantics,
 * which is the only one used by WebGL. We plan to add support also for
 * restricted floats/doubles, similarly to the BigInt conversion policies.
 * We also differ from the specific NaN bit pattern that WebIDL prescribes
 * (https://heycam.github.io/webidl/#es-unrestricted-float) in that Blink
 * passes NaN values as-is, i.e. doesn't normalize them.
 *
 * To be supported types:
 *  - arrays of C types
 *  - arrays of embedder types
 */

#ifndef INCLUDE_V8_FAST_API_CALLS_H_
#define INCLUDE_V8_FAST_API_CALLS_H_

#include <stddef.h>
#include <stdint.h>

#include <tuple>
#include <type_traits>

#include "v8.h"        // NOLINT(build/include_directory)
#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

class Isolate;

class CTypeInfo {
 public:
  enum class Type : uint8_t {
    kVoid,
    kBool,
    kInt32,
    kUint32,
    kInt64,
    kUint64,
    kFloat32,
    kFloat64,
    kV8Value,
    kApiObject,  // This will be deprecated once all users have
                 // migrated from v8::ApiObject to v8::Local<v8::Value>.
  };

  // kCallbackOptionsType is not part of the Type enum
  // because it is only used internally. Use value 255 that is larger
  // than any valid Type enum.
  static constexpr Type kCallbackOptionsType = Type(255);

  enum class Flags : uint8_t {
    kNone = 0,
  };

  explicit constexpr CTypeInfo(Type type, Flags flags = Flags::kNone)
      : type_(type), flags_(flags) {}

  constexpr Type GetType() const { return type_; }

  constexpr Flags GetFlags() const { return flags_; }

 private:
  Type type_;
  Flags flags_;
};

class V8_EXPORT CFunctionInfo {
 public:
  // Construct a struct to hold a CFunction's type information.
  // |return_info| describes the function's return type.
  // |arg_info| is an array of |arg_count| CTypeInfos describing the
  //   arguments. Only the last argument may be of the special type
  //   CTypeInfo::kCallbackOptionsType.
  CFunctionInfo(const CTypeInfo& return_info, unsigned int arg_count,
                const CTypeInfo* arg_info);

  const CTypeInfo& ReturnInfo() const { return return_info_; }

  // The argument count, not including the v8::FastApiCallbackOptions
  // if present.
  unsigned int ArgumentCount() const {
    return HasOptions() ? arg_count_ - 1 : arg_count_;
  }

  // |index| must be less than ArgumentCount().
  //  Note: if the last argument passed on construction of CFunctionInfo
  //  has type CTypeInfo::kCallbackOptionsType, it is not included in
  //  ArgumentCount().
  const CTypeInfo& ArgumentInfo(unsigned int index) const;

  bool HasOptions() const {
    // The options arg is always the last one.
    return arg_count_ > 0 && arg_info_[arg_count_ - 1].GetType() ==
                                 CTypeInfo::kCallbackOptionsType;
  }

 private:
  const CTypeInfo return_info_;
  const unsigned int arg_count_;
  const CTypeInfo* arg_info_;
};

class V8_EXPORT CFunction {
 public:
  constexpr CFunction() : address_(nullptr), type_info_(nullptr) {}

  const CTypeInfo& ReturnInfo() const { return type_info_->ReturnInfo(); }

  const CTypeInfo& ArgumentInfo(unsigned int index) const {
    return type_info_->ArgumentInfo(index);
  }

  unsigned int ArgumentCount() const { return type_info_->ArgumentCount(); }

  const void* GetAddress() const { return address_; }
  const CFunctionInfo* GetTypeInfo() const { return type_info_; }

  template <typename F>
  static CFunction Make(F* func) {
    return ArgUnwrap<F*>::Make(func);
  }

  template <typename F>
  V8_DEPRECATED("Use CFunctionBuilder instead.")
  static CFunction MakeWithFallbackSupport(F* func) {
    return ArgUnwrap<F*>::Make(func);
  }

  CFunction(const void* address, const CFunctionInfo* type_info);

 private:
  const void* address_;
  const CFunctionInfo* type_info_;

  template <typename F>
  class ArgUnwrap {
    static_assert(sizeof(F) != sizeof(F),
                  "CFunction must be created from a function pointer.");
  };

  template <typename R, typename... Args>
  class ArgUnwrap<R (*)(Args...)> {
   public:
    static CFunction Make(R (*func)(Args...));
  };
};

struct ApiObject {
  uintptr_t address;
};

/**
 * A struct which may be passed to a fast call callback, like so:
 * \code
 *    void FastMethodWithOptions(int param, FastApiCallbackOptions& options);
 * \endcode
 */
struct FastApiCallbackOptions {
  /**
   * Creates a new instance of FastApiCallbackOptions for testing purpose.  The
   * returned instance may be filled with mock data.
   */
  static FastApiCallbackOptions CreateForTesting(Isolate* isolate) {
    return {false, {0}};
  }

  /**
   * If the callback wants to signal an error condition or to perform an
   * allocation, it must set options.fallback to true and do an early return
   * from the fast method. Then V8 checks the value of options.fallback and if
   * it's true, falls back to executing the SlowCallback, which is capable of
   * reporting the error (either by throwing a JS exception or logging to the
   * console) or doing the allocation. It's the embedder's responsibility to
   * ensure that the fast callback is idempotent up to the point where error and
   * fallback conditions are checked, because otherwise executing the slow
   * callback might produce visible side-effects twice.
   */
  bool fallback;

  /**
   * The `data` passed to the FunctionTemplate constructor, or `undefined`.
   * `data_ptr` allows for default constructing FastApiCallbackOptions.
   */
  union {
    uintptr_t data_ptr;
    v8::Value data;
  };
};

namespace internal {

// Helper to count the number of occurances of `T` in `List`
template <typename T, typename... List>
struct count : std::integral_constant<int, 0> {};
template <typename T, typename... Args>
struct count<T, T, Args...>
    : std::integral_constant<std::size_t, 1 + count<T, Args...>::value> {};
template <typename T, typename U, typename... Args>
struct count<T, U, Args...> : count<T, Args...> {};

template <typename RetBuilder, typename... ArgBuilders>
class CFunctionInfoImpl : public CFunctionInfo {
  static constexpr int kOptionsArgCount =
      count<FastApiCallbackOptions&, ArgBuilders...>();
  static constexpr int kReceiverCount = 1;

  static_assert(kOptionsArgCount == 0 || kOptionsArgCount == 1,
                "Only one options parameter is supported.");

  static_assert(sizeof...(ArgBuilders) >= kOptionsArgCount + kReceiverCount,
                "The receiver or the options argument is missing.");

 public:
  constexpr CFunctionInfoImpl()
      : CFunctionInfo(RetBuilder::Build(), sizeof...(ArgBuilders),
                      arg_info_storage_),
        arg_info_storage_{ArgBuilders::Build()...} {
    constexpr CTypeInfo::Type kReturnType = RetBuilder::Build().GetType();
    static_assert(kReturnType == CTypeInfo::Type::kVoid ||
                      kReturnType == CTypeInfo::Type::kBool ||
                      kReturnType == CTypeInfo::Type::kInt32 ||
                      kReturnType == CTypeInfo::Type::kUint32 ||
                      kReturnType == CTypeInfo::Type::kFloat32 ||
                      kReturnType == CTypeInfo::Type::kFloat64,
                  "64-bit int and api object values are not currently "
                  "supported return types.");
  }

 private:
  const CTypeInfo arg_info_storage_[sizeof...(ArgBuilders)];
};

template <typename T>
struct TypeInfoHelper {
  static_assert(sizeof(T) != sizeof(T), "This type is not supported");
};

#define SPECIALIZE_GET_TYPE_INFO_HELPER_FOR(T, Enum)                          \
  template <>                                                                 \
  struct TypeInfoHelper<T> {                                                  \
    static constexpr CTypeInfo::Flags Flags() {                               \
      return CTypeInfo::Flags::kNone;                                         \
    }                                                                         \
                                                                              \
    static constexpr CTypeInfo::Type Type() { return CTypeInfo::Type::Enum; } \
  };

#define BASIC_C_TYPES(V)            \
  V(void, kVoid)                    \
  V(bool, kBool)                    \
  V(int32_t, kInt32)                \
  V(uint32_t, kUint32)              \
  V(int64_t, kInt64)                \
  V(uint64_t, kUint64)              \
  V(float, kFloat32)                \
  V(double, kFloat64)               \
  V(ApiObject, kApiObject)          \
  V(v8::Local<v8::Value>, kV8Value) \
  V(v8::Local<v8::Object>, kV8Value)

// ApiObject was a temporary solution to wrap the pointer to the v8::Value.
// Please use v8::Local<v8::Value> in new code for the arguments and
// v8::Local<v8::Object> for the receiver, as ApiObject will be deprecated.

BASIC_C_TYPES(SPECIALIZE_GET_TYPE_INFO_HELPER_FOR)

#undef BASIC_C_TYPES

template <>
struct TypeInfoHelper<FastApiCallbackOptions&> {
  static constexpr CTypeInfo::Flags Flags() { return CTypeInfo::Flags::kNone; }

  static constexpr CTypeInfo::Type Type() {
    return CTypeInfo::kCallbackOptionsType;
  }
};

template <typename T, CTypeInfo::Flags... Flags>
class CTypeInfoBuilder {
 public:
  using BaseType = T;

  static constexpr CTypeInfo Build() {
    // Get the flags and merge in any additional flags.
    uint8_t flags = uint8_t(TypeInfoHelper<T>::Flags());
    int unused[] = {0, (flags |= uint8_t(Flags), 0)...};
    // With C++17, we could use a "..." fold expression over a parameter pack.
    // Since we're still using C++14, we have to evaluate an OR expresion while
    // constructing an unused list of 0's. This applies the binary operator
    // for each value in Flags.
    (void)unused;

    // Return the same type with the merged flags.
    return CTypeInfo(TypeInfoHelper<T>::Type(), CTypeInfo::Flags(flags));
  }
};

template <typename RetBuilder, typename... ArgBuilders>
class CFunctionBuilderWithFunction {
 public:
  explicit constexpr CFunctionBuilderWithFunction(const void* fn) : fn_(fn) {}

  template <CTypeInfo::Flags... Flags>
  constexpr auto Ret() {
    return CFunctionBuilderWithFunction<
        CTypeInfoBuilder<typename RetBuilder::BaseType, Flags...>,
        ArgBuilders...>(fn_);
  }

  template <unsigned int N, CTypeInfo::Flags... Flags>
  constexpr auto Arg() {
    // Return a copy of the builder with the Nth arg builder merged with
    // template parameter pack Flags.
    return ArgImpl<N, Flags...>(
        std::make_index_sequence<sizeof...(ArgBuilders)>());
  }

  auto Build() {
    static CFunctionInfoImpl<RetBuilder, ArgBuilders...> instance;
    return CFunction(fn_, &instance);
  }

 private:
  template <bool Merge, unsigned int N, CTypeInfo::Flags... Flags>
  struct GetArgBuilder;

  // Returns the same ArgBuilder as the one at index N, including its flags.
  // Flags in the template parameter pack are ignored.
  template <unsigned int N, CTypeInfo::Flags... Flags>
  struct GetArgBuilder<false, N, Flags...> {
    using type =
        typename std::tuple_element<N, std::tuple<ArgBuilders...>>::type;
  };

  // Returns an ArgBuilder with the same base type as the one at index N,
  // but merges the flags with the flags in the template parameter pack.
  template <unsigned int N, CTypeInfo::Flags... Flags>
  struct GetArgBuilder<true, N, Flags...> {
    using type = CTypeInfoBuilder<
        typename std::tuple_element<N,
                                    std::tuple<ArgBuilders...>>::type::BaseType,
        std::tuple_element<N, std::tuple<ArgBuilders...>>::type::Build()
            .GetFlags(),
        Flags...>;
  };

  // Return a copy of the CFunctionBuilder, but merges the Flags on ArgBuilder
  // index N with the new Flags passed in the template parameter pack.
  template <unsigned int N, CTypeInfo::Flags... Flags, size_t... I>
  constexpr auto ArgImpl(std::index_sequence<I...>) {
    return CFunctionBuilderWithFunction<
        RetBuilder, typename GetArgBuilder<N == I, I, Flags...>::type...>(fn_);
  }

  const void* fn_;
};

class CFunctionBuilder {
 public:
  constexpr CFunctionBuilder() {}

  template <typename R, typename... Args>
  constexpr auto Fn(R (*fn)(Args...)) {
    return CFunctionBuilderWithFunction<CTypeInfoBuilder<R>,
                                        CTypeInfoBuilder<Args>...>(
        reinterpret_cast<const void*>(fn));
  }
};

}  // namespace internal

// static
template <typename R, typename... Args>
CFunction CFunction::ArgUnwrap<R (*)(Args...)>::Make(R (*func)(Args...)) {
  return internal::CFunctionBuilder().Fn(func).Build();
}

using CFunctionBuilder = internal::CFunctionBuilder;

}  // namespace v8

#endif  // INCLUDE_V8_FAST_API_CALLS_H_
