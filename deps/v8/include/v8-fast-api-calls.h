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
 *      static void FastMethod(v8::ApiObject receiver_obj, int param) {
 *        v8::Object* v8_object = reinterpret_cast<v8::Object*>(&api_object);
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

#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

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
  };

  // kCallbackOptionsType and kInvalidType are not part of the Type enum
  // because they are only used internally. Use values 255 and 254 that
  // are larger than any valid Type enum.
  static constexpr Type kCallbackOptionsType = Type(255);
  static constexpr Type kInvalidType = Type(254);

  enum class ArgFlags : uint8_t {
    kNone = 0,
  };

  explicit constexpr CTypeInfo(Type type, ArgFlags flags = ArgFlags::kNone)
      : type_(type), flags_(flags) {}

  constexpr Type GetType() const { return type_; }

  constexpr ArgFlags GetFlags() const { return flags_; }

  static const CTypeInfo& Invalid() {
    static CTypeInfo invalid = CTypeInfo(kInvalidType);
    return invalid;
  }

 private:
  Type type_;
  ArgFlags flags_;
};

class CFunctionInfo {
 public:
  virtual const CTypeInfo& ReturnInfo() const = 0;
  virtual unsigned int ArgumentCount() const = 0;
  virtual const CTypeInfo& ArgumentInfo(unsigned int index) const = 0;
  virtual bool HasOptions() const = 0;
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
   */
  const ApiObject data;
};

namespace internal {

template <typename T>
struct GetCType;

#define SPECIALIZE_GET_C_TYPE_FOR(ctype, ctypeinfo) \
  template <>                                       \
  struct GetCType<ctype> {                          \
    static constexpr CTypeInfo Get() {              \
      return CTypeInfo(CTypeInfo::Type::ctypeinfo); \
    }                                               \
  };

#define SUPPORTED_C_TYPES(V) \
  V(void, kVoid)             \
  V(bool, kBool)             \
  V(int32_t, kInt32)         \
  V(uint32_t, kUint32)       \
  V(int64_t, kInt64)         \
  V(uint64_t, kUint64)       \
  V(float, kFloat32)         \
  V(double, kFloat64)        \
  V(ApiObject, kV8Value)

SUPPORTED_C_TYPES(SPECIALIZE_GET_C_TYPE_FOR)

template <>
struct GetCType<FastApiCallbackOptions&> {
  static constexpr CTypeInfo Get() {
    return CTypeInfo(CTypeInfo::kCallbackOptionsType);
  }
};

// Helper to count the number of occurances of `T` in `List`
template <typename T, typename... List>
struct count : std::integral_constant<int, 0> {};
template <typename T, typename... Args>
struct count<T, T, Args...>
    : std::integral_constant<std::size_t, 1 + count<T, Args...>::value> {};
template <typename T, typename U, typename... Args>
struct count<T, U, Args...> : count<T, Args...> {};

template <typename R, typename... Args>
class CFunctionInfoImpl : public CFunctionInfo {
 public:
  static constexpr int kOptionsArgCount =
      count<FastApiCallbackOptions&, Args...>();
  static constexpr int kReceiverCount = 1;
  CFunctionInfoImpl()
      : return_info_(internal::GetCType<R>::Get()),
        arg_count_(sizeof...(Args) - kOptionsArgCount),
        arg_info_{internal::GetCType<Args>::Get()...} {
    static_assert(kOptionsArgCount == 0 || kOptionsArgCount == 1,
                  "Only one options parameter is supported.");
    static_assert(sizeof...(Args) >= kOptionsArgCount + kReceiverCount,
                  "The receiver or the fallback argument is missing.");
    constexpr CTypeInfo::Type type = internal::GetCType<R>::Get().GetType();
    static_assert(type == CTypeInfo::Type::kVoid ||
                      type == CTypeInfo::Type::kBool ||
                      type == CTypeInfo::Type::kInt32 ||
                      type == CTypeInfo::Type::kUint32 ||
                      type == CTypeInfo::Type::kFloat32 ||
                      type == CTypeInfo::Type::kFloat64,
                  "64-bit int and api object values are not currently "
                  "supported return types.");
  }

  const CTypeInfo& ReturnInfo() const override { return return_info_; }
  unsigned int ArgumentCount() const override { return arg_count_; }
  const CTypeInfo& ArgumentInfo(unsigned int index) const override {
    if (index >= ArgumentCount()) {
      return CTypeInfo::Invalid();
    }
    return arg_info_[index];
  }
  bool HasOptions() const override { return kOptionsArgCount == 1; }

 private:
  const CTypeInfo return_info_;
  const unsigned int arg_count_;
  const CTypeInfo arg_info_[sizeof...(Args)];
};

}  // namespace internal

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
  V8_DEPRECATED("Use CFunction::Make instead.")
  static CFunction MakeWithFallbackSupport(F* func) {
    return ArgUnwrap<F*>::Make(func);
  }

  template <typename F>
  static CFunction Make(F* func, const CFunctionInfo* type_info) {
    return CFunction(reinterpret_cast<const void*>(func), type_info);
  }

 private:
  const void* address_;
  const CFunctionInfo* type_info_;

  CFunction(const void* address, const CFunctionInfo* type_info);

  template <typename R, typename... Args>
  static CFunctionInfo* GetCFunctionInfo() {
    static internal::CFunctionInfoImpl<R, Args...> instance;
    return &instance;
  }

  template <typename F>
  class ArgUnwrap {
    static_assert(sizeof(F) != sizeof(F),
                  "CFunction must be created from a function pointer.");
  };

  template <typename R, typename... Args>
  class ArgUnwrap<R (*)(Args...)> {
   public:
    static CFunction Make(R (*func)(Args...)) {
      return CFunction(reinterpret_cast<const void*>(func),
                       GetCFunctionInfo<R, Args...>());
    }
  };
};

}  // namespace v8

#endif  // INCLUDE_V8_FAST_API_CALLS_H_
