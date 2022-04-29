#include <memory>
#include <string>
#include "../common.h"

#define CHECK_NAPI(...)                                                        \
  do {                                                                         \
    napi_status res__ = (__VA_ARGS__);                                         \
    if (res__ != napi_ok) {                                                    \
      return res__;                                                            \
    }                                                                          \
  } while (0)

#define NAPI_CALL(expr) NODE_API_CALL(env, expr);

#ifdef __cpp_lib_span
#include <span>
#endif

#ifdef __cpp_lib_span
using std::span;
#else
/**
 * @brief A span of values that can be used to pass arguments to function.
 *
 * For C++20 we should consider to replace it with std::span.
 */
template <typename T>
struct span {
  constexpr span(std::initializer_list<T> il) noexcept
      : data_{const_cast<T*>(il.begin())}, size_{il.size()} {}
  constexpr span(T* data, size_t size) noexcept : data_{data}, size_{size} {}

  [[nodiscard]] constexpr T* data() const noexcept { return data_; }

  [[nodiscard]] constexpr size_t size() const noexcept { return size_; }

  [[nodiscard]] constexpr T* begin() const noexcept { return data_; }

  [[nodiscard]] constexpr T* end() const noexcept { return data_ + size_; }

  const T& operator[](size_t index) const noexcept { return *(data_ + index); }

 private:
  T* data_;
  size_t size_;
};
#endif  // __cpp_lib_span

struct HostObject {
  virtual napi_value ProxyGet(napi_env env, napi_value target, napi_value key) {
    napi_value result{};
    NAPI_CALL(napi_get_property(env, target, key, &result));
    return result;
  }

  virtual void ProxySet(napi_env env,
                        napi_value target,
                        napi_value key,
                        napi_value value) {
    NODE_API_CALL_RETURN_VOID(env, napi_set_property(env, target, key, value));
  }

  virtual napi_value ProxyOwnKeys(napi_env env, napi_value target) {
    napi_value result{};
    NAPI_CALL(napi_get_all_property_names(env,
                                          target,
                                          napi_key_own_only,
                                          napi_key_enumerable,
                                          napi_key_numbers_to_strings,
                                          &result));
    return result;
  }

  virtual napi_value ProxyGetOwnPropertyDescriptor(napi_env env,
                                                   napi_value target,
                                                   napi_value key) {
    napi_value global{}, objectCtor{}, getDescriptor{}, descriptor{};
    NAPI_CALL(napi_get_global(env, &global));
    NAPI_CALL(napi_get_named_property(env, global, "Object", &objectCtor));
    NAPI_CALL(napi_get_named_property(
        env, objectCtor, "getOwnPropertyDescriptor", &getDescriptor));
    napi_value args[] = {target, key};
    NAPI_CALL(
        napi_call_function(env, nullptr, getDescriptor, 2, args, &descriptor));
    return descriptor;
  }
};

struct SimpleObject : HostObject {
  napi_value ProxyGet(napi_env env,
                      napi_value target,
                      napi_value key) override {
    napi_value result{};
    napi_valuetype keyType{};
    NAPI_CALL(napi_typeof(env, key, &keyType));
    if (keyType == napi_string) {
      size_t strSize;
      NAPI_CALL(napi_get_value_string_utf8(env, key, nullptr, 0, &strSize));
      std::string keyStr(strSize, '\0');
      NAPI_CALL(napi_get_value_string_utf8(
          env, key, &keyStr[0], keyStr.size() + 1, nullptr));
      if (keyStr == "add") {
        auto addCB = [](napi_env env, napi_callback_info info) -> napi_value {
          napi_value result{};
          napi_value args[2]{};
          size_t actualArgCount{2};
          SimpleObject* thisPtr{};
          NAPI_CALL(napi_get_cb_info(env,
                                     info,
                                     &actualArgCount,
                                     args,
                                     nullptr,
                                     reinterpret_cast<void**>(&thisPtr)));
          NODE_API_ASSERT(
              env, actualArgCount == 2, "add expects two parameters");
          napi_valuetype arg0Type{}, arg1Type{};
          NAPI_CALL(napi_typeof(env, args[0], &arg0Type));
          NAPI_CALL(napi_typeof(env, args[1], &arg1Type));
          NODE_API_ASSERT(env, arg0Type == napi_number, "arg0 must be number");
          NODE_API_ASSERT(env, arg1Type == napi_number, "arg1 must be number");
          int32_t arg0{}, arg1{};
          NAPI_CALL(napi_get_value_int32(env, args[0], &arg0));
          NAPI_CALL(napi_get_value_int32(env, args[1], &arg1));
          int32_t res = thisPtr->Add(arg0, arg1);
          NAPI_CALL(napi_create_int32(env, res, &result));
          return result;
        };
        NAPI_CALL(napi_create_function(
            env, "add", NAPI_AUTO_LENGTH, addCB, this, &result));
        return result;
      }
    }
    return HostObject::ProxyGet(env, target, key);
  }

  int32_t Add(int32_t x, int32_t y) { return x + y; }
};

struct RefHolder {
  RefHolder(std::nullptr_t = nullptr) noexcept {}
  explicit RefHolder(napi_env env, napi_value value) : env_(env) {
    // Start with 2 to avoid ever going to 0 that creates a weak ref.
    napi_create_reference(env, value, 2, &ref_);
  }

  // The class is movable.
  RefHolder(RefHolder&& other) noexcept
      : env_(std::exchange(other.env_, nullptr)),
        ref_(std::exchange(other.ref_, nullptr)) {}
  RefHolder& operator=(RefHolder&& other) noexcept {
    if (this != &other) {
      RefHolder temp(std::move(*this));
      env_ = std::exchange(other.env_, nullptr);
      ref_ = std::exchange(other.ref_, nullptr);
    }
    return *this;
  }

  // The class is not copyable.
  RefHolder(const RefHolder& other) = delete;
  RefHolder& operator=(const RefHolder& other) = delete;

  ~RefHolder() noexcept {
    if (env_ != nullptr && ref_ != nullptr) {
      uint32_t refCount{};
      napi_reference_unref(env_, ref_, &refCount);
      if (refCount == 1) {
        napi_delete_reference(env_, ref_);
      }
    }
  }

  operator napi_value() const {
    napi_value result{};
    if (ref_ != nullptr) {
      napi_get_reference_value(env_, ref_, &result);
    }
    return result;
  }

  explicit operator bool() const noexcept { return ref_ != nullptr; }

 private:
  napi_env env_{};
  napi_ref ref_{};
};

struct ProxyWrapModule {
  static napi_status Init(napi_env env, napi_value exports) {
    napi_value globalValue{};
    CHECK_NAPI(napi_get_global(env, &globalValue));

    napi_property_descriptor descriptors[] = {
        GetMethodDescriptor<&ProxyWrapModule::CreateSimpleObject>(
            env, "CreateSimpleObject")};
    CHECK_NAPI(napi_define_properties(
        env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

    CHECK_NAPI(napi_set_instance_data(
        env,
        std::make_unique<ProxyWrapModule>().release(),
        [](napi_env /*env*/, void* finalize_data, void* /*finalize_hint*/) {
          std::unique_ptr<ProxyWrapModule> moduleInfo{
              reinterpret_cast<ProxyWrapModule*>(finalize_data)};
        },
        nullptr));
    return napi_ok;
  }

  template <napi_value (ProxyWrapModule::*func)(napi_env env,
                                                napi_callback_info info)>
  static napi_property_descriptor GetMethodDescriptor(napi_env env,
                                                      const char* name) {
    return napi_property_descriptor{name,
                                    nullptr,
                                    [](napi_env env, napi_callback_info info) {
                                      return (GetThis(env)->*func)(env, info);
                                    },
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    napi_default,
                                    nullptr};
  }

  static ProxyWrapModule* GetThis(napi_env env) {
    ProxyWrapModule* module{};
    NAPI_CALL(napi_get_instance_data(env, reinterpret_cast<void**>(&module)));
    return module;
  }

  napi_status GetProxyConstructor(napi_env env, napi_value* result) {
    if (!proxyConstructor_) {
      napi_value global{}, proxyCtor{};
      CHECK_NAPI(napi_get_global(env, &global));
      CHECK_NAPI(napi_get_named_property(env, global, "Proxy", &proxyCtor));
      proxyConstructor_ = RefHolder(env, proxyCtor);
    }
    *result = proxyConstructor_;
    return napi_ok;
  }

  napi_status GetProxyHandler(napi_env env, napi_value* result) {
    if (!proxyHandler_) {
      napi_value proxyHandler{};
      CHECK_NAPI(napi_create_object(env, &proxyHandler));

      CHECK_NAPI(SetProxyTrap<&ProxyWrapModule::ProxyGetTrap, 3>(
          env, proxyHandler, "get"));
      CHECK_NAPI(SetProxyTrap<&ProxyWrapModule::ProxySetTrap, 4>(
          env, proxyHandler, "set"));
      CHECK_NAPI(SetProxyTrap<&ProxyWrapModule::ProxyOwnKeysTrap, 1>(
          env, proxyHandler, "ownKeys"));
      CHECK_NAPI(
          SetProxyTrap<&ProxyWrapModule::ProxyGetOwnPropertyDescriptorTrap, 2>(
              env, proxyHandler, "getOwnPropertyDescriptor"));

      proxyHandler_ = RefHolder(env, proxyHandler);
    }
    *result = proxyHandler_;
    return napi_ok;
  }

  // Sets Proxy trap method as a pointer to NapiJsiRuntime instance method.
  template <napi_value (ProxyWrapModule::*trapMethod)(napi_env,
                                                      span<napi_value>),
            size_t argCount>
  napi_status SetProxyTrap(napi_env env,
                           napi_value handler,
                           const char* propertyName) {
    napi_callback proxyTrap =
        [](napi_env env, napi_callback_info info) noexcept -> napi_value {
      ProxyWrapModule* module{};
      napi_value args[argCount]{};
      size_t actualArgCount{argCount};
      NAPI_CALL(napi_get_cb_info(env,
                                 info,
                                 &actualArgCount,
                                 args,
                                 nullptr,
                                 reinterpret_cast<void**>(&module)));
      NODE_API_ASSERT_BASE(env,
                           actualArgCount == argCount,
                           "proxy trap requires argCount arguments.",
                           nullptr);
      return (module->*trapMethod)(env, span<napi_value>(args, argCount));
    };

    napi_value trapFunc{};
    CHECK_NAPI(napi_create_function(
        env, propertyName, NAPI_AUTO_LENGTH, proxyTrap, this, &trapFunc));
    return napi_set_named_property(env, handler, propertyName, trapFunc);
  }

  napi_value ProxyGetTrap(napi_env env, span<napi_value> args) {
    // args[0] - the Proxy target object.
    // args[1] - the name of the property to set.
    // args[2] - the Proxy object (unused).
    napi_value target = args[0];
    napi_value key = args[1];
    HostObject* hostObject{};
    NAPI_CALL(napi_unwrap(env, target, reinterpret_cast<void**>(&hostObject)));
    return hostObject->ProxyGet(env, target, key);
  }

  napi_value ProxySetTrap(napi_env env, span<napi_value> args) {
    // args[0] - the Proxy target object.
    // args[1] - the name of the property to set.
    // args[2] - the new value of the property to set.
    // args[3] - the Proxy object (unused).
    napi_value target = args[0];
    napi_value key = args[1];
    napi_value value = args[2];
    HostObject* hostObject{};
    NAPI_CALL(napi_unwrap(env, target, reinterpret_cast<void**>(&hostObject)));
    hostObject->ProxySet(env, target, key, value);
    napi_value undefined{};
    NAPI_CALL(napi_get_undefined(env, &undefined));
    return undefined;
  }

  napi_value ProxyOwnKeysTrap(napi_env env, span<napi_value> args) {
    // args[0] - the Proxy target object.
    napi_value target = args[0];
    HostObject* hostObject{};
    NAPI_CALL(napi_unwrap(env, target, reinterpret_cast<void**>(&hostObject)));
    return hostObject->ProxyOwnKeys(env, target);
  }

  napi_value ProxyGetOwnPropertyDescriptorTrap(napi_env env,
                                               span<napi_value> args) {
    // args[0] - the Proxy target object.
    // args[1] - the property
    napi_value target = args[0];
    napi_value key = args[1];
    HostObject* hostObject{};
    NAPI_CALL(napi_unwrap(env, target, reinterpret_cast<void**>(&hostObject)));
    return hostObject->ProxyGetOwnPropertyDescriptor(env, target, key);
  }

  napi_value CreateSimpleObject(napi_env env, napi_callback_info /*info*/) {
    SimpleObject* obj = new SimpleObject();
    napi_value result{}, target{}, proxyConstructor{}, proxyHandler{};
    NAPI_CALL(napi_create_object(env, &target));
    NAPI_CALL(napi_wrap(
        env,
        target,
        obj,
        [](napi_env env, void* finalize_data, void* finalize_hint) {
          delete reinterpret_cast<SimpleObject*>(finalize_data);
        },
        nullptr,
        nullptr));
    NAPI_CALL(GetProxyConstructor(env, &proxyConstructor));
    NAPI_CALL(GetProxyHandler(env, &proxyHandler));

    napi_value args[] = {target, proxyHandler};
    NAPI_CALL(napi_new_instance(env, proxyConstructor, 2, args, &result));
    return result;
  }

 private:
  RefHolder proxyConstructor_;
  RefHolder proxyHandler_;
};

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(ProxyWrapModule::Init(env, exports));
  return exports;
}
EXTERN_C_END
