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
 * An example for custom embedder type support might employ a way to wrap/
 * unwrap various C++ types in JSObject instances, e.g:
 *
 * \code
 *
 *    // Represents the way this type system maps C++ and JS values.
 *    struct WrapperTypeInfo {
 *      // Store e.g. a method to map from exposed C++ types to the already
 *      // created v8::FunctionTemplate's for instantiating them.
 *    };
 *
 *    // Helper method with a sanity check.
 *    template <typename T, int offset>
 *    inline T* GetInternalField(v8::Local<v8::Object> wrapper) {
 *      assert(offset < wrapper->InternalFieldCount());
 *      return reinterpret_cast<T*>(
 *          wrapper->GetAlignedPointerFromInternalField(offset));
 *    }
 *
 *    // Returns the type info from a wrapper JS object.
 *    inline const WrapperTypeInfo* ToWrapperTypeInfo(
 *        v8::Local<v8::Object> wrapper) {
 *      return GetInternalField<WrapperTypeInfo,
 *                              kV8EmbedderWrapperTypeIndex>(wrapper);
 *    }
 *
 *    class CustomEmbedderType {
 *     public:
 *      static constexpr const WrapperTypeInfo* GetWrapperTypeInfo() {
 *        return &custom_type_wrapper_type_info;
 *      }
 *      // Returns the raw C object from a wrapper JS object.
 *      static CustomEmbedderType* Unwrap(v8::Local<v8::Object> wrapper) {
 *        return GetInternalField<CustomEmbedderType,
 *                                kV8EmbedderWrapperObjectIndex>(wrapper);
 *      }
 *      static void FastMethod(CustomEmbedderType* receiver, int param) {
 *        assert(receiver != nullptr);
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
 *        FastMethod(receiver, param);
 *      }
 *
 *     private:
 *      static const WrapperTypeInfo custom_type_wrapper_type_info;
 *    };
 *
 *    // Support for custom embedder types via specialization of WrapperTraits.
 *    namespace v8 {
 *      template <>
 *      class WrapperTraits<CustomEmbedderType> {
 *        public:
 *          static const void* GetTypeInfo() {
 *            // We use the already defined machinery for the custom type.
 *            return CustomEmbedderType::GetWrapperTypeInfo();
 *          }
 *      };
 *    }  // namespace v8
 *
 *    // The constants kV8EmbedderWrapperTypeIndex and
 *    // kV8EmbedderWrapperObjectIndex describe the offsets for the type info
 *    // struct (the one returned by WrapperTraits::GetTypeInfo) and the
 *    // native object, when expressed as internal field indices within a
 *    // JSObject. The existance of this helper function assumes that all
 *    // embedder objects have their JSObject-side type info at the same
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
 *      object_template->Set(
            v8::String::NewFromUtf8Literal(isolate, "method"), method_template);
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
 * Currently only void return types are supported.
 * Currently supported argument types:
 *  - pointer to an embedder type
 *  - bool
 *  - int32_t
 *  - uint32_t
 * To be supported types:
 *  - int64_t
 *  - uint64_t
 *  - float32_t
 *  - float64_t
 *  - arrays of C types
 *  - arrays of embedder types
 */

#ifndef INCLUDE_V8_FAST_API_CALLS_H_
#define INCLUDE_V8_FAST_API_CALLS_H_

#include <stddef.h>
#include <stdint.h>

#include "v8config.h"  // NOLINT(build/include)

namespace v8 {

class CTypeInfo {
 public:
  enum class Type : char {
    kVoid,
    kBool,
    kInt32,
    kUint32,
    kInt64,
    kUint64,
    kFloat32,
    kFloat64,
    kUnwrappedApiObject,
  };

  enum ArgFlags : char {
    None = 0,
    IsArrayBit = 1 << 0,  // This argument is first in an array of values.
  };

  static CTypeInfo FromWrapperType(const void* wrapper_type_info,
                                   ArgFlags flags = ArgFlags::None) {
    uintptr_t wrapper_type_info_ptr =
        reinterpret_cast<uintptr_t>(wrapper_type_info);
    // Check that the lower kIsWrapperTypeBit bits are 0's.
    CHECK_EQ(
        wrapper_type_info_ptr & ~(static_cast<uintptr_t>(~0)
                                  << static_cast<uintptr_t>(kIsWrapperTypeBit)),
        0);
    // TODO(mslekova): Refactor the manual bit manipulations to use
    // PointerWithPayload instead.
    return CTypeInfo(wrapper_type_info_ptr | flags | kIsWrapperTypeBit);
  }

  static constexpr CTypeInfo FromCType(Type ctype,
                                       ArgFlags flags = ArgFlags::None) {
    // ctype cannot be Type::kUnwrappedApiObject.
    return CTypeInfo(
        ((static_cast<uintptr_t>(ctype) << kTypeOffset) & kTypeMask) | flags);
  }

  const void* GetWrapperInfo() const;

  constexpr Type GetType() const {
    if (payload_ & kIsWrapperTypeBit) {
      return Type::kUnwrappedApiObject;
    }
    return static_cast<Type>((payload_ & kTypeMask) >> kTypeOffset);
  }

  constexpr bool IsArray() const { return payload_ & ArgFlags::IsArrayBit; }

 private:
  explicit constexpr CTypeInfo(uintptr_t payload) : payload_(payload) {}

  // That must be the last bit after ArgFlags.
  static constexpr uintptr_t kIsWrapperTypeBit = 1 << 1;
  static constexpr uintptr_t kWrapperTypeInfoMask = static_cast<uintptr_t>(~0)
                                                    << 2;

  static constexpr unsigned int kTypeOffset = kIsWrapperTypeBit;
  static constexpr unsigned int kTypeSize = 8 - kTypeOffset;
  static constexpr uintptr_t kTypeMask =
      (~(static_cast<uintptr_t>(~0) << kTypeSize)) << kTypeOffset;

  const uintptr_t payload_;
};

class CFunctionInfo {
 public:
  virtual const CTypeInfo& ReturnInfo() const = 0;
  virtual unsigned int ArgumentCount() const = 0;
  virtual const CTypeInfo& ArgumentInfo(unsigned int index) const = 0;
};

template <typename T>
class WrapperTraits {
 public:
  static const void* GetTypeInfo() {
    static_assert(sizeof(T) != sizeof(T),
                  "WrapperTraits must be specialized for this type.");
    return nullptr;
  }
};

namespace internal {

template <typename T>
struct GetCType {
  static_assert(sizeof(T) != sizeof(T), "Unsupported CType");
};

#define SPECIALIZE_GET_C_TYPE_FOR(ctype, ctypeinfo)            \
  template <>                                                  \
  struct GetCType<ctype> {                                     \
    static constexpr CTypeInfo Get() {                         \
      return CTypeInfo::FromCType(CTypeInfo::Type::ctypeinfo); \
    }                                                          \
  };

#define SUPPORTED_C_TYPES(V) \
  V(void, kVoid)             \
  V(bool, kBool)             \
  V(int32_t, kInt32)         \
  V(uint32_t, kUint32)       \
  V(int64_t, kInt64)         \
  V(uint64_t, kUint64)       \
  V(float, kFloat32)         \
  V(double, kFloat64)

SUPPORTED_C_TYPES(SPECIALIZE_GET_C_TYPE_FOR)

template <typename T, typename = void>
struct EnableIfHasWrapperTypeInfo {};

template <>
struct EnableIfHasWrapperTypeInfo<void> {};

template <typename T>
struct EnableIfHasWrapperTypeInfo<T, decltype(WrapperTraits<T>::GetTypeInfo(),
                                              void())> {
  typedef void type;
};

// T* where T is a primitive (array of primitives).
template <typename T, typename = void>
struct GetCTypePointerImpl {
  static constexpr CTypeInfo Get() {
    return CTypeInfo::FromCType(GetCType<T>::Get().GetType(),
                                CTypeInfo::IsArrayBit);
  }
};

// T* where T is an API object.
template <typename T>
struct GetCTypePointerImpl<T, typename EnableIfHasWrapperTypeInfo<T>::type> {
  static constexpr CTypeInfo Get() {
    return CTypeInfo::FromWrapperType(WrapperTraits<T>::GetTypeInfo());
  }
};

// T** where T is a primitive. Not allowed.
template <typename T, typename = void>
struct GetCTypePointerPointerImpl {
  static_assert(sizeof(T**) != sizeof(T**), "Unsupported type");
};

// T** where T is an API object (array of API objects).
template <typename T>
struct GetCTypePointerPointerImpl<
    T, typename EnableIfHasWrapperTypeInfo<T>::type> {
  static constexpr CTypeInfo Get() {
    return CTypeInfo::FromWrapperType(WrapperTraits<T>::GetTypeInfo(),
                                      CTypeInfo::IsArrayBit);
  }
};

template <typename T>
struct GetCType<T**> : public GetCTypePointerPointerImpl<T> {};

template <typename T>
struct GetCType<T*> : public GetCTypePointerImpl<T> {};

template <typename R, typename... Args>
class CFunctionInfoImpl : public CFunctionInfo {
 public:
  CFunctionInfoImpl()
      : return_info_(i::GetCType<R>::Get()),
        arg_count_(sizeof...(Args)),
        arg_info_{i::GetCType<Args>::Get()...} {
    static_assert(i::GetCType<R>::Get().GetType() == CTypeInfo::Type::kVoid,
                  "Only void return types are currently supported.");
  }

  const CTypeInfo& ReturnInfo() const override { return return_info_; }
  unsigned int ArgumentCount() const override { return arg_count_; }
  const CTypeInfo& ArgumentInfo(unsigned int index) const override {
    CHECK_LT(index, ArgumentCount());
    return arg_info_[index];
  }

 private:
  CTypeInfo return_info_;
  const unsigned int arg_count_;
  CTypeInfo arg_info_[sizeof...(Args)];
};

}  // namespace internal

class V8_EXPORT CFunction {
 public:
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
