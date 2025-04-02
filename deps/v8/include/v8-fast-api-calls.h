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
 *  - JavaScript array of primitive types
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
 *  - TypedArrays and ArrayBuffers
 *  - arrays of embedder types
 *
 *
 * The API offers a limited support for function overloads:
 *
 * \code
 *    void FastMethod_2Args(int param, bool another_param);
 *    void FastMethod_3Args(int param, bool another_param, int third_param);
 *
 *    v8::CFunction fast_method_2args_c_func =
 *         MakeV8CFunction(FastMethod_2Args);
 *    v8::CFunction fast_method_3args_c_func =
 *         MakeV8CFunction(FastMethod_3Args);
 *    const v8::CFunction fast_method_overloads[] = {fast_method_2args_c_func,
 *                                                   fast_method_3args_c_func};
 *    Local<v8::FunctionTemplate> method_template =
 *        v8::FunctionTemplate::NewWithCFunctionOverloads(
 *            isolate, SlowCallback, data, signature, length,
 *            constructor_behavior, side_effect_type,
 *            {fast_method_overloads, 2});
 * \endcode
 *
 * In this example a single FunctionTemplate is associated to multiple C++
 * functions. The overload resolution is currently only based on the number of
 * arguments passed in a call. For example, if this method_template is
 * registered with a wrapper JS object as described above, a call with two
 * arguments:
 *    obj.method(42, true);
 * will result in a fast call to FastMethod_2Args, while a call with three or
 * more arguments:
 *    obj.method(42, true, 11);
 * will result in a fast call to FastMethod_3Args. Instead a call with less than
 * two arguments, like:
 *    obj.method(42);
 * would not result in a fast call but would fall back to executing the
 * associated SlowCallback.
 */

#ifndef INCLUDE_V8_FAST_API_CALLS_H_
#define INCLUDE_V8_FAST_API_CALLS_H_

#include <stddef.h>
#include <stdint.h>

#include <tuple>
#include <type_traits>

#include "v8-internal.h"      // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-typed-array.h"   // NOLINT(build/include_directory)
#include "v8-value.h"         // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class Isolate;

START_ALLOW_USE_DEPRECATED()
class CTypeInfo {
 public:
  enum class Type : uint8_t {
    kVoid,
    kBool,
    kUint8,
    kInt32,
    kUint32,
    kInt64,
    kUint64,
    kFloat32,
    kFloat64,
    kPointer,
    kV8Value,
    kSeqOneByteString,
    kApiObject,  // This will be deprecated once all users have
                 // migrated from v8::ApiObject to v8::Local<v8::Value>.
    kAny,        // This is added to enable untyped representation of fast
                 // call arguments for test purposes. It can represent any of
                 // the other types stored in the same memory as a union
                 // (see AnyCType declared below). This allows for
                 // uniform passing of arguments w.r.t. their location
                 // (in a register or on the stack), independent of their
                 // actual type. It's currently used by the arm64 simulator
                 // and can be added to the other simulators as well when fast
                 // calls having both GP and FP params need to be supported.
  };

  // kCallbackOptionsType is not part of the Type enum
  // because it is only used internally. Use value 255 that is larger
  // than any valid Type enum.
  static constexpr Type kCallbackOptionsType = Type(255);

  enum class V8_DEPRECATE_SOON(
      "There is no special support in V8 anymore, there is no need to"
      "use a SequenceType") SequenceType : uint8_t {
    kScalar,
    kIsSequence,    // sequence<T>
    kIsArrayBuffer  // ArrayBuffer
  };

  enum class Flags : uint8_t {
    kNone = 0,
    kAllowSharedBit = 1 << 0,   // Must be an ArrayBuffer or TypedArray
    kEnforceRangeBit = 1 << 1,  // T must be integral
    kClampBit = 1 << 2,         // T must be integral
    kIsRestrictedBit = 1 << 3,  // T must be float or double
  };

  explicit constexpr CTypeInfo(Type type, Flags flags = Flags::kNone)
      : type_(type), sequence_type_(SequenceType::kScalar), flags_(flags) {}

  V8_DEPRECATE_SOON("Use CTypeInfo(Type, Flags) instead")
  constexpr CTypeInfo(Type type, SequenceType sequence_type,
                      Flags flags = Flags::kNone)
      : type_(type), sequence_type_(sequence_type), flags_(flags) {}

  typedef uint32_t Identifier;
  explicit constexpr CTypeInfo(Identifier identifier)
      : CTypeInfo(static_cast<Type>(identifier >> 16),
                  static_cast<SequenceType>((identifier >> 8) & 255),
                  static_cast<Flags>(identifier & 255)) {}
  constexpr Identifier GetId() const {
    return static_cast<uint8_t>(type_) << 16 |
           static_cast<uint8_t>(sequence_type_) << 8 |
           static_cast<uint8_t>(flags_);
  }

  constexpr Type GetType() const { return type_; }
  V8_DEPRECATE_SOON("Use the constant SequenceType::kScalar instead")
  constexpr SequenceType GetSequenceType() const { return sequence_type_; }
  constexpr Flags GetFlags() const { return flags_; }

  static constexpr bool IsIntegralType(Type type) {
    return type == Type::kUint8 || type == Type::kInt32 ||
           type == Type::kUint32 || type == Type::kInt64 ||
           type == Type::kUint64;
  }

  static constexpr bool IsFloatingPointType(Type type) {
    return type == Type::kFloat32 || type == Type::kFloat64;
  }

  static constexpr bool IsPrimitive(Type type) {
    return IsIntegralType(type) || IsFloatingPointType(type) ||
           type == Type::kBool;
  }

 private:
  Type type_;
  SequenceType sequence_type_;
  Flags flags_;
};
END_ALLOW_USE_DEPRECATED()

struct FastOneByteString {
  const char* data;
  uint32_t length;
};

class V8_EXPORT CFunctionInfo {
 public:
  enum class Int64Representation : uint8_t {
    kNumber = 0,  // Use numbers to represent 64 bit integers.
    kBigInt = 1,  // Use BigInts to represent 64 bit integers.
  };

  // Construct a struct to hold a CFunction's type information.
  // |return_info| describes the function's return type.
  // |arg_info| is an array of |arg_count| CTypeInfos describing the
  //   arguments. Only the last argument may be of the special type
  //   CTypeInfo::kCallbackOptionsType.
  CFunctionInfo(const CTypeInfo& return_info, unsigned int arg_count,
                const CTypeInfo* arg_info,
                Int64Representation repr = Int64Representation::kNumber);

  const CTypeInfo& ReturnInfo() const { return return_info_; }

  // The argument count, not including the v8::FastApiCallbackOptions
  // if present.
  unsigned int ArgumentCount() const {
    return HasOptions() ? arg_count_ - 1 : arg_count_;
  }

  Int64Representation GetInt64Representation() const { return repr_; }

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
  const Int64Representation repr_;
  const unsigned int arg_count_;
  const CTypeInfo* arg_info_;
};

struct FastApiCallbackOptions;

// Provided for testing.
union V8_TRIVIAL_ABI AnyCType {
  AnyCType() : int64_value(0) {}

#if defined(V8_ENABLE_LOCAL_OFF_STACK_CHECK) && V8_HAS_ATTRIBUTE_TRIVIAL_ABI
  // In this case, Local<T> is not trivially copyable and the implicit
  // copy constructor and copy assignment for the union are deleted.
  AnyCType(const AnyCType& other) : int64_value(other.int64_value) {}
  AnyCType& operator=(const AnyCType& other) {
    int64_value = other.int64_value;
    return *this;
  }
#endif

  bool bool_value;
  int32_t int32_value;
  uint32_t uint32_value;
  int64_t int64_value;
  uint64_t uint64_value;
  float float_value;
  double double_value;
  void* pointer_value;
  Local<Object> object_value;
  Local<Array> sequence_value;
  const FastOneByteString* string_value;
  FastApiCallbackOptions* options_value;
};

static_assert(
    sizeof(AnyCType) == 8,
    "The union AnyCType should have size == 64 bits, as this is assumed "
    "by EffectControlLinearizer.");

class V8_EXPORT CFunction {
 public:
  constexpr CFunction() : address_(nullptr), type_info_(nullptr) {}

  const CTypeInfo& ReturnInfo() const { return type_info_->ReturnInfo(); }

  const CTypeInfo& ArgumentInfo(unsigned int index) const {
    return type_info_->ArgumentInfo(index);
  }

  unsigned int ArgumentCount() const { return type_info_->ArgumentCount(); }

  const void* GetAddress() const { return address_; }
  CFunctionInfo::Int64Representation GetInt64Representation() const {
    return type_info_->GetInt64Representation();
  }
  const CFunctionInfo* GetTypeInfo() const { return type_info_; }

  enum class OverloadResolution { kImpossible, kAtRuntime, kAtCompileTime };

  template <typename F>
  static CFunction Make(F* func,
                        CFunctionInfo::Int64Representation int64_rep =
                            CFunctionInfo::Int64Representation::kNumber) {
    CFunction result = ArgUnwrap<F*>::Make(func, int64_rep);
    result.GetInt64Representation();
    return result;
  }

  // Provided for testing purposes.
  template <typename R, typename... Args, typename R_Patch,
            typename... Args_Patch>
  static CFunction Make(R (*func)(Args...),
                        R_Patch (*patching_func)(Args_Patch...),
                        CFunctionInfo::Int64Representation int64_rep =
                            CFunctionInfo::Int64Representation::kNumber) {
    CFunction c_func = ArgUnwrap<R (*)(Args...)>::Make(func, int64_rep);
    static_assert(
        sizeof...(Args_Patch) == sizeof...(Args),
        "The patching function must have the same number of arguments.");
    c_func.address_ = reinterpret_cast<void*>(patching_func);
    return c_func;
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
    static CFunction Make(R (*func)(Args...),
                          CFunctionInfo::Int64Representation int64_rep =
                              CFunctionInfo::Int64Representation::kNumber);
  };
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
    return {};
  }

  v8::Isolate* isolate = nullptr;

  /**
   * The `data` passed to the FunctionTemplate constructor, or `undefined`.
   */
  v8::Local<v8::Value> data;
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

template <CFunctionInfo::Int64Representation Representation,
          typename RetBuilder, typename... ArgBuilders>
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
                      arg_info_storage_, Representation),
        arg_info_storage_{ArgBuilders::Build()...} {
    constexpr CTypeInfo::Type kReturnType = RetBuilder::Build().GetType();
    static_assert(kReturnType == CTypeInfo::Type::kVoid ||
                      kReturnType == CTypeInfo::Type::kBool ||
                      kReturnType == CTypeInfo::Type::kInt32 ||
                      kReturnType == CTypeInfo::Type::kUint32 ||
                      kReturnType == CTypeInfo::Type::kInt64 ||
                      kReturnType == CTypeInfo::Type::kUint64 ||
                      kReturnType == CTypeInfo::Type::kFloat32 ||
                      kReturnType == CTypeInfo::Type::kFloat64 ||
                      kReturnType == CTypeInfo::Type::kPointer ||
                      kReturnType == CTypeInfo::Type::kAny,
                  "String and api object values are not currently "
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

template <CTypeInfo::Type type>
struct CTypeInfoTraits {};

#define DEFINE_TYPE_INFO_TRAITS(CType, Enum)      \
  template <>                                     \
  struct CTypeInfoTraits<CTypeInfo::Type::Enum> { \
    using ctype = CType;                          \
  };

#define PRIMITIVE_C_TYPES(V) \
  V(bool, kBool)             \
  V(uint8_t, kUint8)         \
  V(int32_t, kInt32)         \
  V(uint32_t, kUint32)       \
  V(int64_t, kInt64)         \
  V(uint64_t, kUint64)       \
  V(float, kFloat32)         \
  V(double, kFloat64)        \
  V(void*, kPointer)

// Same as above, but includes deprecated types for compatibility.
#define ALL_C_TYPES(V)               \
  PRIMITIVE_C_TYPES(V)               \
  V(void, kVoid)                     \
  V(v8::Local<v8::Value>, kV8Value)  \
  V(v8::Local<v8::Object>, kV8Value) \
  V(v8::Local<v8::Array>, kV8Value)  \
  V(AnyCType, kAny)

// ApiObject was a temporary solution to wrap the pointer to the v8::Value.
// Please use v8::Local<v8::Value> in new code for the arguments and
// v8::Local<v8::Object> for the receiver, as ApiObject will be deprecated.

ALL_C_TYPES(SPECIALIZE_GET_TYPE_INFO_HELPER_FOR)
PRIMITIVE_C_TYPES(DEFINE_TYPE_INFO_TRAITS)

#undef PRIMITIVE_C_TYPES
#undef ALL_C_TYPES

#undef TYPED_ARRAY_C_TYPES

template <>
struct TypeInfoHelper<FastApiCallbackOptions&> {
  static constexpr CTypeInfo::Flags Flags() { return CTypeInfo::Flags::kNone; }

  static constexpr CTypeInfo::Type Type() {
    return CTypeInfo::kCallbackOptionsType;
  }
};

template <>
struct TypeInfoHelper<const FastOneByteString&> {
  static constexpr CTypeInfo::Flags Flags() { return CTypeInfo::Flags::kNone; }

  static constexpr CTypeInfo::Type Type() {
    return CTypeInfo::Type::kSeqOneByteString;
  }
};

#define STATIC_ASSERT_IMPLIES(COND, ASSERTION, MSG) \
  static_assert(((COND) == 0) || (ASSERTION), MSG)

}  // namespace internal

template <typename T, CTypeInfo::Flags... Flags>
class V8_EXPORT CTypeInfoBuilder {
 public:
  using BaseType = T;

  static constexpr CTypeInfo Build() {
    constexpr CTypeInfo::Flags kFlags =
        MergeFlags(internal::TypeInfoHelper<T>::Flags(), Flags...);
    constexpr CTypeInfo::Type kType = internal::TypeInfoHelper<T>::Type();

    STATIC_ASSERT_IMPLIES(
        uint8_t(kFlags) & uint8_t(CTypeInfo::Flags::kEnforceRangeBit),
        CTypeInfo::IsIntegralType(kType),
        "kEnforceRangeBit is only allowed for integral types.");
    STATIC_ASSERT_IMPLIES(
        uint8_t(kFlags) & uint8_t(CTypeInfo::Flags::kClampBit),
        CTypeInfo::IsIntegralType(kType),
        "kClampBit is only allowed for integral types.");
    STATIC_ASSERT_IMPLIES(
        uint8_t(kFlags) & uint8_t(CTypeInfo::Flags::kIsRestrictedBit),
        CTypeInfo::IsFloatingPointType(kType),
        "kIsRestrictedBit is only allowed for floating point types.");

    // Return the same type with the merged flags.
    return CTypeInfo(internal::TypeInfoHelper<T>::Type(), kFlags);
  }

 private:
  template <typename... Rest>
  static constexpr CTypeInfo::Flags MergeFlags(CTypeInfo::Flags flags,
                                               Rest... rest) {
    return CTypeInfo::Flags(uint8_t(flags) | uint8_t(MergeFlags(rest...)));
  }
  static constexpr CTypeInfo::Flags MergeFlags() { return CTypeInfo::Flags(0); }
};

namespace internal {
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

  // Provided for testing purposes.
  template <typename Ret, typename... Args>
  auto Patch(Ret (*patching_func)(Args...)) {
    static_assert(
        sizeof...(Args) == sizeof...(ArgBuilders),
        "The patching function must have the same number of arguments.");
    fn_ = reinterpret_cast<void*>(patching_func);
    return *this;
  }

  template <CFunctionInfo::Int64Representation Representation =
                CFunctionInfo::Int64Representation::kNumber>
  auto Build() {
    static CFunctionInfoImpl<Representation, RetBuilder, ArgBuilders...>
        instance;
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

  // Return a copy of the CFunctionBuilder, but merges the Flags on
  // ArgBuilder index N with the new Flags passed in the template parameter
  // pack.
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
CFunction CFunction::ArgUnwrap<R (*)(Args...)>::Make(
    R (*func)(Args...), CFunctionInfo::Int64Representation int64_rep) {
  if (int64_rep == CFunctionInfo::Int64Representation::kNumber) {
    return internal::CFunctionBuilder().Fn(func).Build();
  }
  return internal::CFunctionBuilder()
      .Fn(func)
      .template Build<CFunctionInfo::Int64Representation::kBigInt>();
}

using CFunctionBuilder = internal::CFunctionBuilder;

static constexpr CTypeInfo kTypeInfoInt32 = CTypeInfo(CTypeInfo::Type::kInt32);
static constexpr CTypeInfo kTypeInfoFloat64 =
    CTypeInfo(CTypeInfo::Type::kFloat64);

/**
 * Copies the contents of this JavaScript array to a C++ buffer with
 * a given max_length. A CTypeInfo is passed as an argument,
 * instructing different rules for conversion (e.g. restricted float/double).
 * The element type T of the destination array must match the C type
 * corresponding to the CTypeInfo (specified by CTypeInfoTraits).
 * If the array length is larger than max_length or the array is of
 * unsupported type, the operation will fail, returning false. Generally, an
 * array which contains objects, undefined, null or anything not convertible
 * to the requested destination type, is considered unsupported. The operation
 * returns true on success. `type_info` will be used for conversions.
 */
template <CTypeInfo::Identifier type_info_id, typename T>
bool V8_EXPORT V8_WARN_UNUSED_RESULT TryToCopyAndConvertArrayToCppBuffer(
    Local<Array> src, T* dst, uint32_t max_length);

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<int32_t>::Build().GetId(),
                                    int32_t>(Local<Array> src, int32_t* dst,
                                             uint32_t max_length);

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<uint32_t>::Build().GetId(),
                                    uint32_t>(Local<Array> src, uint32_t* dst,
                                              uint32_t max_length);

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<float>::Build().GetId(),
                                    float>(Local<Array> src, float* dst,
                                           uint32_t max_length);

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<double>::Build().GetId(),
                                    double>(Local<Array> src, double* dst,
                                            uint32_t max_length);

}  // namespace v8

#endif  // INCLUDE_V8_FAST_API_CALLS_H_
