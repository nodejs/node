#ifndef SRC_JS_NATIVE_API_H_
#define SRC_JS_NATIVE_API_H_

#include "js_native_api_types.h"

// If you need __declspec(dllimport), either include <node_api.h> instead, or
// define NAPI_EXTERN as __declspec(dllimport) on the compiler's command line.
#ifndef NAPI_EXTERN
#ifdef _WIN32
#define NAPI_EXTERN __declspec(dllexport)
#elif defined(__wasm__)
#define NAPI_EXTERN                                                            \
  __attribute__((visibility("default")))                                       \
  __attribute__((__import_module__("napi")))
#else
#define NAPI_EXTERN __attribute__((visibility("default")))
#endif
#endif

#define NAPI_AUTO_LENGTH SIZE_MAX

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

#if defined(NODE_API_MODULE_USE_VTABLE) &&                                     \
    !defined(NODE_API_MODULE_NO_VTABLE_IMPL)
#define NODE_API_MODULE_USE_VTABLE_IMPL
#endif

#ifdef NODE_API_MODULE_USE_VTABLE_IMPL

// Implement all Node-API functions via vtable indirection.
#undef NAPI_EXTERN
#define NAPI_EXTERN static inline

#ifndef NODE_API_MODULE_NO_VTABLE_FALLBACK

extern node_api_js_vtable g_node_api_js_vtable_fallback;

// Platform-specific atomic pointer read/write with acquire/release semantics.
// Used for thread-safe lazy initialization of function pointers.
#if defined(__cplusplus) && __cplusplus >= 201103L
// C++11 atomics
#include <atomic>
#define NODE_API_READ_POINTER_ACQUIRE(ptr)                                     \
  std::atomic_load_explicit(                                                   \
      reinterpret_cast<std::atomic<void*>*>(                                   \
          const_cast<void**>(reinterpret_cast<void* const*>(ptr))),            \
      std::memory_order_acquire)
#define NODE_API_WRITE_POINTER_RELEASE(ptr, val)                               \
  std::atomic_store_explicit(                                                  \
      reinterpret_cast<std::atomic<void*>*>(                                   \
          const_cast<void**>(reinterpret_cast<void* const*>(ptr))),            \
      static_cast<void*>(val),                                                 \
      std::memory_order_release)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L &&              \
    !defined(__STDC_NO_ATOMICS__)
// C11 atomics
#include <stdatomic.h>
// NOLINTBEGIN (readability/casting) - it must be compilable by C compiler
#define NODE_API_READ_POINTER_ACQUIRE(ptr)                                     \
  atomic_load_explicit((_Atomic(void*)*)ptr, memory_order_acquire)
#define NODE_API_WRITE_POINTER_RELEASE(ptr, val)                               \
  atomic_store_explicit(                                                       \
      (_Atomic(void*)*)ptr, (void*)(val), memory_order_release)
// NOLINTEND (readability/casting)
#else
// Fallback based on volatile
// NOLINTBEGIN (readability/casting) - it must be compilable by C compiler
#define NODE_API_READ_POINTER_ACQUIRE(ptr) (*(void* volatile*)(ptr))
#define NODE_API_WRITE_POINTER_RELEASE(ptr, val)                               \
  (*(void* volatile*)(ptr) = (void*)(val))
// NOLINTEND (readability/casting)
#endif

// Platform-specific symbol loading
#ifndef NODE_API_LOAD_SYMBOL
#ifdef _WIN32
// Minimal Win32 declarations to avoid pulling in windows.h and to sidestep
// typedef redefinition issues. Use namespaced typedefs instead of probing for
// existing ones.
#ifndef WINAPI
#define WINAPI __stdcall
#endif

typedef struct HINSTANCE__* node_api_hmodule;  // Matches Windows HMODULE
typedef intptr_t(WINAPI* node_api_farproc)();  // Matches Windows FARPROC

// Kernel32 imports needed for runtime symbol lookup (keep signatures identical
// to system headers to avoid redefinition conflicts if windows.h is included
// elsewhere).
EXTERN_C_START
__declspec(dllimport) node_api_hmodule WINAPI
    GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) node_api_farproc WINAPI
    GetProcAddress(node_api_hmodule hModule, const char* lpProcName);
EXTERN_C_END

// NOLINTBEGIN (readability/null_usage) - it must be compilable by C compiler
static inline node_api_hmodule node_api_get_runtime_module_handle() {
  // This code should match the code in win_delay_load_hook.cc from node-gyp
  static node_api_hmodule module_handle = NULL;
  node_api_hmodule handle =
      (node_api_hmodule)NODE_API_READ_POINTER_ACQUIRE(&module_handle);
  if (handle == NULL) {
    handle = GetModuleHandleA("libnode.dll");
    if (handle == NULL) {
      handle = GetModuleHandleA(NULL);
    }
    NODE_API_WRITE_POINTER_RELEASE(&module_handle, handle);
  }
  return handle;
}
// NOLINTEND (readability/null_usage)

#define NODE_API_LOAD_SYMBOL(name)                                             \
  GetProcAddress(node_api_get_runtime_module_handle(), name)
#else
// Minimal POSIX dlsym declaration to avoid pulling in dlfcn.h
EXTERN_C_START
void* dlsym(void* handle, const char* symbol);
EXTERN_C_END

#ifndef RTLD_DEFAULT
// On macOS RTLD_DEFAULT is ((void*)-2); other platforms typically use NULL.
#if defined(__APPLE__)
#define RTLD_DEFAULT ((void*)-2)
#else
#define RTLD_DEFAULT ((void*)0)
#endif
#endif

#define NODE_API_LOAD_SYMBOL(name) dlsym(RTLD_DEFAULT, name)
#endif
#endif  // NODE_API_LOAD_SYMBOL

#define NODE_API_VTABLE_IMPL_FALLBACK(                                         \
    vtable, ret, func_name, method_name, ...)                                  \
  const node_api_##vtable* vtable = &g_node_api_##vtable##_fallback;           \
  if (!NODE_API_READ_POINTER_ACQUIRE(&vtable->method_name)) {                  \
    NODE_API_WRITE_POINTER_RELEASE(&vtable->method_name,                       \
                                   NODE_API_LOAD_SYMBOL(#func_name));          \
  }                                                                            \
  ret vtable->method_name(__VA_ARGS__)

#else  // NODE_API_MODULE_NO_VTABLE_FALLBACK

// Platform-specific abort that generates a debugger-friendly crash
#ifdef _MSC_VER
#define NODE_API_UNREACHABLE() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define NODE_API_UNREACHABLE() __builtin_trap()
#else
#define NODE_API_UNREACHABLE() ((void)(*(volatile int*)0 = 0))
#endif

#define NODE_API_VTABLE_IMPL_FALLBACK(...) NODE_API_UNREACHABLE()

#endif  // NODE_API_MODULE_NO_VTABLE_FALLBACK

#define NODE_API_VTABLE_IMPL_BASE(vtable, func_name, method_name, obj, ...)    \
  {                                                                            \
    if (!obj) {                                                                \
      return napi_invalid_arg;                                                 \
    } else if (obj->sentinel == NODE_API_VT_SENTINEL) {                        \
      return obj->vtable->method_name(__VA_ARGS__);                            \
    } else {                                                                   \
      NODE_API_VTABLE_IMPL_FALLBACK(                                           \
          vtable, return, func_name, method_name, __VA_ARGS__);                \
    }                                                                          \
  }

#define NODE_API_JS_VTABLE_IMPL(func_name, method_name, env, ...)              \
  NODE_API_VTABLE_IMPL_BASE(                                                   \
      js_vtable, func_name, method_name, env, env, __VA_ARGS__)

#else  // NODE_API_MODULE_USE_VTABLE_IMPL

#define NODE_API_JS_VTABLE_IMPL(...)

#endif  // NODE_API_MODULE_USE_VTABLE_IMPL

EXTERN_C_START

NAPI_EXTERN napi_status NAPI_CDECL napi_get_last_error_info(
    node_api_basic_env env, const napi_extended_error_info** result)
    NODE_API_JS_VTABLE_IMPL(napi_get_last_error_info,
                            get_last_error_info,
                            env,
                            result);

// Getters for defined singletons
NAPI_EXTERN napi_status NAPI_CDECL napi_get_undefined(napi_env env,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_undefined, get_undefined, env, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_null(napi_env env,
                                                 napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_null, get_null, env, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_global(napi_env env,
                                                   napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_global, get_global, env, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_boolean(napi_env env,
                                                    bool value,
                                                    napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_boolean, get_boolean, env, value, result);

// Methods to create Primitive types/Objects
NAPI_EXTERN napi_status NAPI_CDECL napi_create_object(napi_env env,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_object, create_object, env, result);

#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_CREATE_OBJECT_WITH_PROPERTIES
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_object_with_properties(napi_env env,
                                   napi_value prototype_or_null,
                                   napi_value* property_names,
                                   napi_value* property_values,
                                   size_t property_count,
                                   napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_object_with_properties,
                            create_object_with_properties,
                            env,
                            prototype_or_null,
                            property_names,
                            property_values,
                            property_count,
                            result);
#endif  // NAPI_EXPERIMENTAL

NAPI_EXTERN napi_status NAPI_CDECL napi_create_array(napi_env env,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_array, create_array, env, result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_create_array_with_length(napi_env env, size_t length, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_array_with_length,
                            create_array_with_length,
                            env,
                            length,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_double(napi_env env,
                                                      double value,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_double, create_double, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_int32(napi_env env,
                                                     int32_t value,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_int32, create_int32, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_uint32(napi_env env,
                                                      uint32_t value,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_uint32, create_uint32, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_int64(napi_env env,
                                                     int64_t value,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_int64, create_int64, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_latin1(napi_env env,
                                                             const char* str,
                                                             size_t length,
                                                             napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_string_latin1,
                            create_string_latin1,
                            env,
                            str,
                            length,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_utf8(napi_env env,
                                                           const char* str,
                                                           size_t length,
                                                           napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_string_utf8, create_string_utf8, env, str, length, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_utf16(napi_env env,
                                                            const char16_t* str,
                                                            size_t length,
                                                            napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_string_utf16,
                            create_string_utf16,
                            env,
                            str,
                            length,
                            result);

#if NAPI_VERSION >= 10
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_external_string_latin1(
    napi_env env,
    char* str,
    size_t length,
    node_api_basic_finalize finalize_callback,
    void* finalize_hint,
    napi_value* result,
    bool* copied)
    NODE_API_JS_VTABLE_IMPL(node_api_create_external_string_latin1,
                            create_external_string_latin1,
                            env,
                            str,
                            length,
                            finalize_callback,
                            finalize_hint,
                            result,
                            copied);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_external_string_utf16(
    napi_env env,
    char16_t* str,
    size_t length,
    node_api_basic_finalize finalize_callback,
    void* finalize_hint,
    napi_value* result,
    bool* copied) NODE_API_JS_VTABLE_IMPL(node_api_create_external_string_utf16,
                                          create_external_string_utf16,
                                          env,
                                          str,
                                          length,
                                          finalize_callback,
                                          finalize_hint,
                                          result,
                                          copied);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_latin1(
    napi_env env, const char* str, size_t length, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(node_api_create_property_key_latin1,
                            create_property_key_latin1,
                            env,
                            str,
                            length,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_utf8(
    napi_env env, const char* str, size_t length, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(node_api_create_property_key_utf8,
                            create_property_key_utf8,
                            env,
                            str,
                            length,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_utf16(
    napi_env env, const char16_t* str, size_t length, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(node_api_create_property_key_utf16,
                            create_property_key_utf16,
                            env,
                            str,
                            length,
                            result);

#endif  // NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL napi_create_symbol(napi_env env,
                                                      napi_value description,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_symbol, create_symbol, env, description, result);

#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL
node_api_symbol_for(napi_env env,
                    const char* utf8description,
                    size_t length,
                    napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        node_api_symbol_for, symbol_for, env, utf8description, length, result);
#endif  // NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL napi_create_function(napi_env env,
                                                        const char* utf8name,
                                                        size_t length,
                                                        napi_callback cb,
                                                        void* data,
                                                        napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_function,
                            create_function,
                            env,
                            utf8name,
                            length,
                            cb,
                            data,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_error(napi_env env,
                                                     napi_value code,
                                                     napi_value msg,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_error, create_error, env, code, msg, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_type_error(napi_env env,
                                                          napi_value code,
                                                          napi_value msg,
                                                          napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_type_error, create_type_error, env, code, msg, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_range_error(napi_env env,
                                                           napi_value code,
                                                           napi_value msg,
                                                           napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_range_error, create_range_error, env, code, msg, result);

#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_syntax_error(
    napi_env env, napi_value code, napi_value msg, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(node_api_create_syntax_error,
                            create_syntax_error,
                            env,
                            code,
                            msg,
                            result);
#endif  // NAPI_VERSION >= 9

// Methods to get the native napi_value from Primitive type
NAPI_EXTERN napi_status NAPI_CDECL napi_typeof(napi_env env,
                                               napi_value value,
                                               napi_valuetype* result)
    NODE_API_JS_VTABLE_IMPL(napi_typeof, type_of, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_double(napi_env env,
                                                         napi_value value,
                                                         double* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_double, get_value_double, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_int32(napi_env env,
                                                        napi_value value,
                                                        int32_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_int32, get_value_int32, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_uint32(napi_env env,
                                                         napi_value value,
                                                         uint32_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_uint32, get_value_uint32, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_int64(napi_env env,
                                                        napi_value value,
                                                        int64_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_int64, get_value_int64, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bool(napi_env env,
                                                       napi_value value,
                                                       bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_bool, get_value_bool, env, value, result);

// Copies LATIN-1 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_latin1(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_value_string_latin1,
                            get_value_string_latin1,
                            env,
                            value,
                            buf,
                            bufsize,
                            result);

// Copies UTF-8 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_utf8(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_value_string_utf8,
                            get_value_string_utf8,
                            env,
                            value,
                            buf,
                            bufsize,
                            result);

// Copies UTF-16 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_utf16(napi_env env,
                                                               napi_value value,
                                                               char16_t* buf,
                                                               size_t bufsize,
                                                               size_t* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_value_string_utf16,
                            get_value_string_utf16,
                            env,
                            value,
                            buf,
                            bufsize,
                            result);

// Methods to coerce values
// These APIs may execute user scripts
NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_bool(napi_env env,
                                                       napi_value value,
                                                       napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_coerce_to_bool, coerce_to_bool, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_number(napi_env env,
                                                         napi_value value,
                                                         napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_coerce_to_number, coerce_to_number, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_object(napi_env env,
                                                         napi_value value,
                                                         napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_coerce_to_object, coerce_to_object, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_string(napi_env env,
                                                         napi_value value,
                                                         napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_coerce_to_string, coerce_to_string, env, value, result);

// Methods to work with Objects
#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_SET_PROTOTYPE
NAPI_EXTERN napi_status NAPI_CDECL node_api_set_prototype(napi_env env,
                                                          napi_value object,
                                                          napi_value value)
    NODE_API_JS_VTABLE_IMPL(
        node_api_set_prototype, set_prototype, env, object, value);
#endif
NAPI_EXTERN napi_status NAPI_CDECL napi_get_prototype(napi_env env,
                                                      napi_value object,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_prototype, get_prototype, env, object, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_property_names(napi_env env,
                                                           napi_value object,
                                                           napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_property_names, get_property_names, env, object, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_set_property(napi_env env,
                                                     napi_value object,
                                                     napi_value key,
                                                     napi_value value)
    NODE_API_JS_VTABLE_IMPL(
        napi_set_property, set_property, env, object, key, value);

NAPI_EXTERN napi_status NAPI_CDECL
napi_has_property(napi_env env, napi_value object, napi_value key, bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_has_property, has_property, env, object, key, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_property(napi_env env,
                                                     napi_value object,
                                                     napi_value key,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_property, get_property, env, object, key, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_delete_property(napi_env env,
                                                        napi_value object,
                                                        napi_value key,
                                                        bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_delete_property, delete_property, env, object, key, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_has_own_property(napi_env env,
                                                         napi_value object,
                                                         napi_value key,
                                                         bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_has_own_property, has_own_property, env, object, key, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_set_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           napi_value value)
    NODE_API_JS_VTABLE_IMPL(napi_set_named_property,
                            set_named_property,
                            env,
                            object,
                            utf8name,
                            value);

NAPI_EXTERN napi_status NAPI_CDECL napi_has_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_has_named_property,
                            has_named_property,
                            env,
                            object,
                            utf8name,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_named_property,
                            get_named_property,
                            env,
                            object,
                            utf8name,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_set_element(napi_env env,
                                                    napi_value object,
                                                    uint32_t index,
                                                    napi_value value)
    NODE_API_JS_VTABLE_IMPL(
        napi_set_element, set_element, env, object, index, value);

NAPI_EXTERN napi_status NAPI_CDECL
napi_has_element(napi_env env, napi_value object, uint32_t index, bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_has_element, has_element, env, object, index, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_element(napi_env env,
                                                    napi_value object,
                                                    uint32_t index,
                                                    napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_element, get_element, env, object, index, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_delete_element(napi_env env,
                                                       napi_value object,
                                                       uint32_t index,
                                                       bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_delete_element, delete_element, env, object, index, result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_define_properties(napi_env env,
                       napi_value object,
                       size_t property_count,
                       const napi_property_descriptor* properties)
    NODE_API_JS_VTABLE_IMPL(napi_define_properties,
                            define_properties,
                            env,
                            object,
                            property_count,
                            properties);

// Methods to work with Arrays
NAPI_EXTERN napi_status NAPI_CDECL napi_is_array(napi_env env,
                                                 napi_value value,
                                                 bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_is_array, is_array, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_array_length(napi_env env,
                                                         napi_value value,
                                                         uint32_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_array_length, get_array_length, env, value, result);

// Methods to compare values
NAPI_EXTERN napi_status NAPI_CDECL
napi_strict_equals(napi_env env, napi_value lhs, napi_value rhs, bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_strict_equals, strict_equals, env, lhs, rhs, result);

// Methods to work with Functions
NAPI_EXTERN napi_status NAPI_CDECL napi_call_function(napi_env env,
                                                      napi_value recv,
                                                      napi_value func,
                                                      size_t argc,
                                                      const napi_value* argv,
                                                      napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_call_function, call_function, env, recv, func, argc, argv, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_new_instance(napi_env env,
                                                     napi_value constructor,
                                                     size_t argc,
                                                     const napi_value* argv,
                                                     napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_new_instance, new_instance, env, constructor, argc, argv, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_instanceof(napi_env env,
                                                   napi_value object,
                                                   napi_value constructor,
                                                   bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_instanceof,
                            instanceof
                            , env, object, constructor, result);

// Methods to work with napi_callbacks

// Gets all callback info in a single call. (Ugly, but faster.)
NAPI_EXTERN napi_status NAPI_CDECL napi_get_cb_info(
    napi_env env,               // [in] Node-API environment handle
    napi_callback_info cbinfo,  // [in] Opaque callback-info handle
    size_t* argc,      // [in-out] Specifies the size of the provided argv array
                       // and receives the actual count of args.
    napi_value* argv,  // [out] Array of values
    napi_value* this_arg,  // [out] Receives the JS 'this' arg for the call
    void** data)           // [out] Receives the data pointer for the callback.
    NODE_API_JS_VTABLE_IMPL(
        napi_get_cb_info, get_cb_info, env, cbinfo, argc, argv, this_arg, data);

NAPI_EXTERN napi_status NAPI_CDECL
napi_get_new_target(napi_env env, napi_callback_info cbinfo, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_new_target, get_new_target, env, cbinfo, result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_define_class(napi_env env,
                  const char* utf8name,
                  size_t length,
                  napi_callback constructor,
                  void* data,
                  size_t property_count,
                  const napi_property_descriptor* properties,
                  napi_value* result) NODE_API_JS_VTABLE_IMPL(napi_define_class,
                                                              define_class,
                                                              env,
                                                              utf8name,
                                                              length,
                                                              constructor,
                                                              data,
                                                              property_count,
                                                              properties,
                                                              result);

// Methods to work with external data objects
NAPI_EXTERN napi_status NAPI_CDECL
napi_wrap(napi_env env,
          napi_value js_object,
          void* native_object,
          node_api_basic_finalize finalize_cb,
          void* finalize_hint,
          napi_ref* result) NODE_API_JS_VTABLE_IMPL(napi_wrap,
                                                    wrap,
                                                    env,
                                                    js_object,
                                                    native_object,
                                                    finalize_cb,
                                                    finalize_hint,
                                                    result);

NAPI_EXTERN napi_status NAPI_CDECL napi_unwrap(napi_env env,
                                               napi_value js_object,
                                               void** result)
    NODE_API_JS_VTABLE_IMPL(napi_unwrap, unwrap, env, js_object, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_remove_wrap(napi_env env,
                                                    napi_value js_object,
                                                    void** result)
    NODE_API_JS_VTABLE_IMPL(
        napi_remove_wrap, remove_wrap, env, js_object, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_external(
    napi_env env,
    void* data,
    node_api_basic_finalize finalize_cb,
    void* finalize_hint,
    napi_value* result) NODE_API_JS_VTABLE_IMPL(napi_create_external,
                                                create_external,
                                                env,
                                                data,
                                                finalize_cb,
                                                finalize_hint,
                                                result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_external(napi_env env,
                                                           napi_value value,
                                                           void** result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_value_external, get_value_external, env, value, result);

// Methods to control object lifespan

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
NAPI_EXTERN napi_status NAPI_CDECL napi_create_reference(
    napi_env env, napi_value value, uint32_t initial_refcount, napi_ref* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_reference,
                            create_reference,
                            env,
                            value,
                            initial_refcount,
                            result);

// Deletes a reference. The referenced value is released, and may
// be GC'd unless there are other references to it.
NAPI_EXTERN napi_status NAPI_CDECL napi_delete_reference(node_api_basic_env env,
                                                         napi_ref ref)
    NODE_API_JS_VTABLE_IMPL(napi_delete_reference, delete_reference, env, ref);

// Increments the reference count, optionally returning the resulting count.
// After this call the  reference will be a strong reference because its
// refcount is >0, and the referenced object is effectively "pinned".
// Calling this when the refcount is 0 and the object is unavailable
// results in an error.
NAPI_EXTERN napi_status NAPI_CDECL napi_reference_ref(napi_env env,
                                                      napi_ref ref,
                                                      uint32_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_reference_ref, reference_ref, env, ref, result);

// Decrements the reference count, optionally returning the resulting count.
// If the result is 0 the reference is now weak and the object may be GC'd
// at any time if there are no other references. Calling this when the
// refcount is already 0 results in an error.
NAPI_EXTERN napi_status NAPI_CDECL napi_reference_unref(napi_env env,
                                                        napi_ref ref,
                                                        uint32_t* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_reference_unref, reference_unref, env, ref, result);

// Attempts to get a referenced value. If the reference is weak,
// the value might no longer be available, in that case the call
// is still successful but the result is NULL.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_reference_value(napi_env env,
                                                            napi_ref ref,
                                                            napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_reference_value, get_reference_value, env, ref, result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_open_handle_scope(napi_env env, napi_handle_scope* result)
    NODE_API_JS_VTABLE_IMPL(napi_open_handle_scope,
                            open_handle_scope,
                            env,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_close_handle_scope(napi_env env, napi_handle_scope scope)
    NODE_API_JS_VTABLE_IMPL(napi_close_handle_scope,
                            close_handle_scope,
                            env,
                            scope);

NAPI_EXTERN napi_status NAPI_CDECL napi_open_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope* result)
    NODE_API_JS_VTABLE_IMPL(napi_open_escapable_handle_scope,
                            open_escapable_handle_scope,
                            env,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_close_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope scope)
    NODE_API_JS_VTABLE_IMPL(napi_close_escapable_handle_scope,
                            close_escapable_handle_scope,
                            env,
                            scope);

NAPI_EXTERN napi_status NAPI_CDECL
napi_escape_handle(napi_env env,
                   napi_escapable_handle_scope scope,
                   napi_value escapee,
                   napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_escape_handle, escape_handle, env, scope, escapee, result);

// Methods to support error handling
NAPI_EXTERN napi_status NAPI_CDECL napi_throw(napi_env env, napi_value error)
    NODE_API_JS_VTABLE_IMPL(napi_throw, throw_value, env, error);

NAPI_EXTERN napi_status NAPI_CDECL napi_throw_error(napi_env env,
                                                    const char* code,
                                                    const char* msg)
    NODE_API_JS_VTABLE_IMPL(napi_throw_error, throw_error, env, code, msg);

NAPI_EXTERN napi_status NAPI_CDECL napi_throw_type_error(napi_env env,
                                                         const char* code,
                                                         const char* msg)
    NODE_API_JS_VTABLE_IMPL(
        napi_throw_type_error, throw_type_error, env, code, msg);

NAPI_EXTERN napi_status NAPI_CDECL napi_throw_range_error(napi_env env,
                                                          const char* code,
                                                          const char* msg)
    NODE_API_JS_VTABLE_IMPL(
        napi_throw_range_error, throw_range_error, env, code, msg);

#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL node_api_throw_syntax_error(napi_env env,
                                                               const char* code,
                                                               const char* msg)
    NODE_API_JS_VTABLE_IMPL(
        node_api_throw_syntax_error, throw_syntax_error, env, code, msg);
#endif  // NAPI_VERSION >= 9

NAPI_EXTERN napi_status NAPI_CDECL napi_is_error(napi_env env,
                                                 napi_value value,
                                                 bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_is_error, is_error, env, value, result);

// Methods to support catching exceptions
NAPI_EXTERN napi_status NAPI_CDECL napi_is_exception_pending(napi_env env,
                                                             bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_is_exception_pending,
                            is_exception_pending,
                            env,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_get_and_clear_last_exception(napi_env env, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_and_clear_last_exception,
                            get_and_clear_last_exception,
                            env,
                            result);

// Methods to work with array buffers and typed arrays
NAPI_EXTERN napi_status NAPI_CDECL napi_is_arraybuffer(napi_env env,
                                                       napi_value value,
                                                       bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_is_arraybuffer, is_arraybuffer, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_arraybuffer(napi_env env,
                                                           size_t byte_length,
                                                           void** data,
                                                           napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_arraybuffer,
                            create_arraybuffer,
                            env,
                            byte_length,
                            data,
                            result);

#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_external_arraybuffer(napi_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 node_api_basic_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_external_arraybuffer,
                            create_external_arraybuffer,
                            env,
                            external_data,
                            byte_length,
                            finalize_cb,
                            finalize_hint,
                            result);

#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

NAPI_EXTERN napi_status NAPI_CDECL napi_get_arraybuffer_info(
    napi_env env, napi_value arraybuffer, void** data, size_t* byte_length)
    NODE_API_JS_VTABLE_IMPL(napi_get_arraybuffer_info,
                            get_arraybuffer_info,
                            env,
                            arraybuffer,
                            data,
                            byte_length);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_typedarray(napi_env env,
                                                      napi_value value,
                                                      bool* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_is_typedarray, is_typedarray, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_typedarray(
    napi_env env,
    napi_typedarray_type type,
    size_t length,
    napi_value arraybuffer,
    size_t byte_offset,
    napi_value* result) NODE_API_JS_VTABLE_IMPL(napi_create_typedarray,
                                                create_typedarray,
                                                env,
                                                type,
                                                length,
                                                arraybuffer,
                                                byte_offset,
                                                result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_typedarray_info(
    napi_env env,
    napi_value typedarray,
    napi_typedarray_type* type,
    size_t* length,
    void** data,
    napi_value* arraybuffer,
    size_t* byte_offset) NODE_API_JS_VTABLE_IMPL(napi_get_typedarray_info,
                                                 get_typedarray_info,
                                                 env,
                                                 typedarray,
                                                 type,
                                                 length,
                                                 data,
                                                 arraybuffer,
                                                 byte_offset);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_dataview(napi_env env,
                                                        size_t length,
                                                        napi_value arraybuffer,
                                                        size_t byte_offset,
                                                        napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_dataview,
                            create_dataview,
                            env,
                            length,
                            arraybuffer,
                            byte_offset,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_dataview(napi_env env,
                                                    napi_value value,
                                                    bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_is_dataview, is_dataview, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_dataview_info(
    napi_env env,
    napi_value dataview,
    size_t* bytelength,
    void** data,
    napi_value* arraybuffer,
    size_t* byte_offset) NODE_API_JS_VTABLE_IMPL(napi_get_dataview_info,
                                                 get_dataview_info,
                                                 env,
                                                 dataview,
                                                 bytelength,
                                                 data,
                                                 arraybuffer,
                                                 byte_offset);

#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_SHAREDARRAYBUFFER
NAPI_EXTERN napi_status NAPI_CDECL
node_api_is_sharedarraybuffer(napi_env env, napi_value value, bool* result)
    NODE_API_JS_VTABLE_IMPL(node_api_is_sharedarraybuffer,
                            is_sharedarraybuffer,
                            env,
                            value,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_sharedarraybuffer(
    napi_env env, size_t byte_length, void** data, napi_value* result)
    NODE_API_JS_VTABLE_IMPL(node_api_create_sharedarraybuffer,
                            create_sharedarraybuffer,
                            env,
                            byte_length,
                            data,
                            result);
#endif  // NAPI_EXPERIMENTAL

// version management
NAPI_EXTERN napi_status NAPI_CDECL napi_get_version(node_api_basic_env env,
                                                    uint32_t* result)
    NODE_API_JS_VTABLE_IMPL(napi_get_version, get_version, env, result);

// Promises
NAPI_EXTERN napi_status NAPI_CDECL napi_create_promise(napi_env env,
                                                       napi_deferred* deferred,
                                                       napi_value* promise)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_promise, create_promise, env, deferred, promise);

NAPI_EXTERN napi_status NAPI_CDECL napi_resolve_deferred(napi_env env,
                                                         napi_deferred deferred,
                                                         napi_value resolution)
    NODE_API_JS_VTABLE_IMPL(
        napi_resolve_deferred, resolve_deferred, env, deferred, resolution);

NAPI_EXTERN napi_status NAPI_CDECL napi_reject_deferred(napi_env env,
                                                        napi_deferred deferred,
                                                        napi_value rejection)
    NODE_API_JS_VTABLE_IMPL(
        napi_reject_deferred, reject_deferred, env, deferred, rejection);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_promise(napi_env env,
                                                   napi_value value,
                                                   bool* is_promise)
    NODE_API_JS_VTABLE_IMPL(
        napi_is_promise, is_promise, env, value, is_promise);

// Running a script
NAPI_EXTERN napi_status NAPI_CDECL napi_run_script(napi_env env,
                                                   napi_value script,
                                                   napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_run_script, run_script, env, script, result);

// Memory management
NAPI_EXTERN napi_status NAPI_CDECL napi_adjust_external_memory(
    node_api_basic_env env, int64_t change_in_bytes, int64_t* adjusted_value)
    NODE_API_JS_VTABLE_IMPL(napi_adjust_external_memory,
                            adjust_external_memory,
                            env,
                            change_in_bytes,
                            adjusted_value);

#if NAPI_VERSION >= 5

// Dates
NAPI_EXTERN napi_status NAPI_CDECL napi_create_date(napi_env env,
                                                    double time,
                                                    napi_value* result)
    NODE_API_JS_VTABLE_IMPL(napi_create_date, create_date, env, time, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_date(napi_env env,
                                                napi_value value,
                                                bool* is_date)
    NODE_API_JS_VTABLE_IMPL(napi_is_date, is_date, env, value, is_date);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_date_value(napi_env env,
                                                       napi_value value,
                                                       double* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_get_date_value, get_date_value, env, value, result);

// Add finalizer for pointer
NAPI_EXTERN napi_status NAPI_CDECL
napi_add_finalizer(napi_env env,
                   napi_value js_object,
                   void* finalize_data,
                   node_api_basic_finalize finalize_cb,
                   void* finalize_hint,
                   napi_ref* result) NODE_API_JS_VTABLE_IMPL(napi_add_finalizer,
                                                             add_finalizer,
                                                             env,
                                                             js_object,
                                                             finalize_data,
                                                             finalize_cb,
                                                             finalize_hint,
                                                             result);

#endif  // NAPI_VERSION >= 5

#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_POST_FINALIZER

NAPI_EXTERN napi_status NAPI_CDECL node_api_post_finalizer(
    node_api_basic_env env,
    napi_finalize finalize_cb,
    void* finalize_data,
    void* finalize_hint) NODE_API_JS_VTABLE_IMPL(node_api_post_finalizer,
                                                 post_finalizer,
                                                 env,
                                                 finalize_cb,
                                                 finalize_data,
                                                 finalize_hint);

#endif  // NAPI_EXPERIMENTAL

#if NAPI_VERSION >= 6

// BigInt
NAPI_EXTERN napi_status NAPI_CDECL napi_create_bigint_int64(napi_env env,
                                                            int64_t value,
                                                            napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_bigint_int64, create_bigint_int64, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_bigint_uint64(napi_env env,
                                                             uint64_t value,
                                                             napi_value* result)
    NODE_API_JS_VTABLE_IMPL(
        napi_create_bigint_uint64, create_bigint_uint64, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_bigint_words(
    napi_env env,
    int sign_bit,
    size_t word_count,
    const uint64_t* words,
    napi_value* result) NODE_API_JS_VTABLE_IMPL(napi_create_bigint_words,
                                                create_bigint_words,
                                                env,
                                                sign_bit,
                                                word_count,
                                                words,
                                                result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bigint_int64(napi_env env,
                                                               napi_value value,
                                                               int64_t* result,
                                                               bool* lossless)
    NODE_API_JS_VTABLE_IMPL(napi_get_value_bigint_int64,
                            get_value_bigint_int64,
                            env,
                            value,
                            result,
                            lossless);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bigint_uint64(
    napi_env env, napi_value value, uint64_t* result, bool* lossless)
    NODE_API_JS_VTABLE_IMPL(napi_get_value_bigint_uint64,
                            get_value_bigint_uint64,
                            env,
                            value,
                            result,
                            lossless);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bigint_words(
    napi_env env,
    napi_value value,
    int* sign_bit,
    size_t* word_count,
    uint64_t* words) NODE_API_JS_VTABLE_IMPL(napi_get_value_bigint_words,
                                             get_value_bigint_words,
                                             env,
                                             value,
                                             sign_bit,
                                             word_count,
                                             words);

// Object
NAPI_EXTERN napi_status NAPI_CDECL napi_get_all_property_names(
    napi_env env,
    napi_value object,
    napi_key_collection_mode key_mode,
    napi_key_filter key_filter,
    napi_key_conversion key_conversion,
    napi_value* result) NODE_API_JS_VTABLE_IMPL(napi_get_all_property_names,
                                                get_all_property_names,
                                                env,
                                                object,
                                                key_mode,
                                                key_filter,
                                                key_conversion,
                                                result);

// Instance data
NAPI_EXTERN napi_status NAPI_CDECL napi_set_instance_data(
    node_api_basic_env env,
    void* data,
    napi_finalize finalize_cb,
    void* finalize_hint) NODE_API_JS_VTABLE_IMPL(napi_set_instance_data,
                                                 set_instance_data,
                                                 env,
                                                 data,
                                                 finalize_cb,
                                                 finalize_hint);

NAPI_EXTERN napi_status NAPI_CDECL
napi_get_instance_data(node_api_basic_env env, void** data)
    NODE_API_JS_VTABLE_IMPL(napi_get_instance_data,
                            get_instance_data,
                            env,
                            data);

#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7
// ArrayBuffer detaching
NAPI_EXTERN napi_status NAPI_CDECL
napi_detach_arraybuffer(napi_env env, napi_value arraybuffer)
    NODE_API_JS_VTABLE_IMPL(napi_detach_arraybuffer,
                            detach_arraybuffer,
                            env,
                            arraybuffer);

NAPI_EXTERN napi_status NAPI_CDECL
napi_is_detached_arraybuffer(napi_env env, napi_value value, bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_is_detached_arraybuffer,
                            is_detached_arraybuffer,
                            env,
                            value,
                            result);

#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8
// Type tagging
NAPI_EXTERN napi_status NAPI_CDECL napi_type_tag_object(
    napi_env env, napi_value value, const napi_type_tag* type_tag)
    NODE_API_JS_VTABLE_IMPL(
        napi_type_tag_object, type_tag_object, env, value, type_tag);

NAPI_EXTERN napi_status NAPI_CDECL napi_check_object_type_tag(
    napi_env env, napi_value value, const napi_type_tag* type_tag, bool* result)
    NODE_API_JS_VTABLE_IMPL(napi_check_object_type_tag,
                            check_object_type_tag,
                            env,
                            value,
                            type_tag,
                            result);

NAPI_EXTERN napi_status NAPI_CDECL napi_object_freeze(napi_env env,
                                                      napi_value object)
    NODE_API_JS_VTABLE_IMPL(napi_object_freeze, object_freeze, env, object);

NAPI_EXTERN napi_status NAPI_CDECL napi_object_seal(napi_env env,
                                                    napi_value object)
    NODE_API_JS_VTABLE_IMPL(napi_object_seal, object_seal, env, object);

#endif  // NAPI_VERSION >= 8

EXTERN_C_END

#endif  // SRC_JS_NATIVE_API_H_
