#ifndef SRC_NODE_ADDON_MACROS_H_
#define SRC_NODE_ADDON_MACROS_H_

#include "node_abi_versions.h"

#ifdef _WIN32
# define NODE_MODULE_EXPORT __declspec(dllexport)
#else
# define NODE_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#ifdef NODE_SHARED_MODE
# define NODE_CTOR_PREFIX
#else
# define NODE_CTOR_PREFIX static
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define NODE_C_CTOR(fn)                                               \
  NODE_CTOR_PREFIX void __cdecl fn(void);                             \
  __declspec(dllexport, allocate(".CRT$XCU"))                         \
      void (__cdecl*fn ## _)(void) = fn;                              \
  NODE_CTOR_PREFIX void __cdecl fn(void)
#else
#define NODE_C_CTOR(fn)                                               \
  NODE_CTOR_PREFIX void fn(void) __attribute__((constructor));        \
  NODE_CTOR_PREFIX void fn(void)
#endif

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

#define NODE_MODULE_X(modname, regfunc, priv, flags)                  \
  extern "C" {                                                        \
    static node::node_module _module =                                \
    {                                                                 \
      NODE_MODULE_VERSION,                                            \
      flags,                                                          \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      __FILE__,                                                       \
      (node::addon_register_func) (regfunc),                          \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL   /* NOLINT (readability/null_usage) */                    \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module);                                 \
    }                                                                 \
  }

#define NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, priv, flags)    \
  extern "C" {                                                        \
    static node::node_module _module =                                \
    {                                                                 \
      NODE_MODULE_VERSION,                                            \
      flags,                                                          \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      __FILE__,                                                       \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      (node::addon_context_register_func) (regfunc),                  \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL  /* NOLINT (readability/null_usage) */                     \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module);                                 \
    }                                                                 \
  }

// Usage: `NODE_MODULE(NODE_GYP_MODULE_NAME, InitializerFunction)`
// If no NODE_MODULE is declared, Node.js looks for the well-known
// symbol `node_register_module_v${NODE_MODULE_VERSION}`.
#define NODE_MODULE(modname, regfunc)                                 \
  NODE_MODULE_X(modname, regfunc, NULL, 0)  // NOLINT (readability/null_usage)

#define NODE_MODULE_CONTEXT_AWARE(modname, regfunc)                   \
  /* NOLINTNEXTLINE (readability/null_usage) */                       \
  NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, NULL, 0)

/*
 * For backward compatibility in add-on modules.
 */
#define NODE_MODULE_DECL /* nothing */

#define NODE_MODULE_INITIALIZER_BASE node_register_module_v

#define NODE_MODULE_INITIALIZER_X(base, version)                      \
    NODE_MODULE_INITIALIZER_X_HELPER(base, version)

#define NODE_MODULE_INITIALIZER_X_HELPER(base, version) base##version

#define NODE_MODULE_INITIALIZER                                       \
  NODE_MODULE_INITIALIZER_X(NODE_MODULE_INITIALIZER_BASE,             \
      NODE_MODULE_VERSION)

#define NODE_MODULE_INIT()                                            \
  extern "C" NODE_MODULE_EXPORT void                                  \
  NODE_MODULE_INITIALIZER(v8::Local<v8::Object> exports,              \
                          v8::Local<v8::Value> module,                \
                          v8::Local<v8::Context> context);            \
  NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME,                     \
                            NODE_MODULE_INITIALIZER)                  \
  void NODE_MODULE_INITIALIZER(v8::Local<v8::Object> exports,         \
                               v8::Local<v8::Value> module,           \
                               v8::Local<v8::Context> context)

#define NODE_MODULE_ABI_VERSION_TERMINATOR \
  { node_abi_version_terminator, 0 }
#define NODE_MODULE_ABI_VENDOR_VERSION \
  { node_abi_vendor_version, NODE_ABI_VENDOR_VERSION }
#define NODE_MODULE_ABI_ENGINE_VERSION \
  { node_abi_engine_version, NODE_ABI_ENGINE_VERSION }
#define NODE_MODULE_ABI_OPENSSL_VERSION \
  { node_abi_openssl_version, NODE_ABI_OPENSSL_VERSION }
#define NODE_MODULE_ABI_LIBUV_VERSION \
  { node_abi_libuv_version, NODE_ABI_LIBUV_VERSION }
#define NODE_MODULE_ABI_ICU_VERSION \
  { node_abi_icu_version, NODE_ABI_ICU_VERSION }
#define NODE_MODULE_ABI_CARES_VERSION \
  { node_abi_cares_version, NODE_ABI_CARES_VERSION }

#define NODE_MODULE_ABI_DECLARATION_BASE node_module_declare_abi_v

#define NODE_MODULE_ABI_DECLARATION                           \
  NODE_MODULE_INITIALIZER_X(NODE_MODULE_ABI_DECLARATION_BASE, \
  NODE_MODULE_VERSION)

#define NODE_MODULE_DECLARE_ABI(...)                                       \
EXTERN_C_START                                                             \
NODE_MODULE_EXPORT node_abi_version_entry* NODE_MODULE_ABI_DECLARATION() { \
  static node_abi_version_entry versions[] = {                             \
    __VA_ARGS__, NODE_MODULE_ABI_VERSION_TERMINATOR                        \
  };                                                                       \
  return versions;                                                         \
}                                                                          \
EXTERN_C_END

#endif  // SRC_NODE_ADDON_MACROS_H_
