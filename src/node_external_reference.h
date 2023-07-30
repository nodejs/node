#ifndef SRC_NODE_EXTERNAL_REFERENCE_H_
#define SRC_NODE_EXTERNAL_REFERENCE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cinttypes>
#include <vector>
#include "v8-fast-api-calls.h"
#include "v8.h"

namespace node {

using CFunctionCallbackWithOneByteString =
    uint32_t (*)(v8::Local<v8::Value>, const v8::FastOneByteString&);
using CFunctionCallback = void (*)(v8::Local<v8::Value> receiver);
using CFunctionCallbackReturnDouble =
    double (*)(v8::Local<v8::Object> receiver);
using CFunctionCallbackWithInt64 = void (*)(v8::Local<v8::Object> receiver,
                                            int64_t);
using CFunctionCallbackWithBool = void (*)(v8::Local<v8::Object> receiver,
                                           bool);
using CFunctionCallbackWithString =
    bool (*)(v8::Local<v8::Value>, const v8::FastOneByteString& input);
using CFunctionCallbackWithStrings =
    bool (*)(v8::Local<v8::Value>,
             const v8::FastOneByteString& input,
             const v8::FastOneByteString& base);
using CFunctionWithUint32 = uint32_t (*)(v8::Local<v8::Value>,
                                         const uint32_t input);

// This class manages the external references from the V8 heap
// to the C++ addresses in Node.js.
class ExternalReferenceRegistry {
 public:
  ExternalReferenceRegistry();

#define ALLOWED_EXTERNAL_REFERENCE_TYPES(V)                                    \
  V(CFunctionCallback)                                                         \
  V(CFunctionCallbackWithOneByteString)                                        \
  V(CFunctionCallbackReturnDouble)                                             \
  V(CFunctionCallbackWithInt64)                                                \
  V(CFunctionCallbackWithBool)                                                 \
  V(CFunctionCallbackWithString)                                               \
  V(CFunctionCallbackWithStrings)                                              \
  V(CFunctionWithUint32)                                                       \
  V(const v8::CFunctionInfo*)                                                  \
  V(v8::FunctionCallback)                                                      \
  V(v8::AccessorGetterCallback)                                                \
  V(v8::AccessorSetterCallback)                                                \
  V(v8::AccessorNameGetterCallback)                                            \
  V(v8::AccessorNameSetterCallback)                                            \
  V(v8::GenericNamedPropertyDefinerCallback)                                   \
  V(v8::GenericNamedPropertyDeleterCallback)                                   \
  V(v8::GenericNamedPropertyEnumeratorCallback)                                \
  V(v8::GenericNamedPropertyQueryCallback)                                     \
  V(v8::GenericNamedPropertySetterCallback)                                    \
  V(v8::IndexedPropertySetterCallback)                                         \
  V(v8::IndexedPropertyDefinerCallback)                                        \
  V(v8::IndexedPropertyDeleterCallback)                                        \
  V(v8::IndexedPropertyQueryCallback)                                          \
  V(v8::IndexedPropertyDescriptorCallback)                                     \
  V(const v8::String::ExternalStringResourceBase*)

#define V(ExternalReferenceType)                                               \
  void Register(ExternalReferenceType addr) { RegisterT(addr); }
  ALLOWED_EXTERNAL_REFERENCE_TYPES(V)
#undef V

  // This can be called only once.
  const std::vector<intptr_t>& external_references();

  bool is_empty() { return external_references_.empty(); }

 private:
  template <typename T>
  void RegisterT(T* address) {
    external_references_.push_back(reinterpret_cast<intptr_t>(address));
  }
  bool is_finalized_ = false;
  std::vector<intptr_t> external_references_;
};

#define EXTERNAL_REFERENCE_BINDING_LIST_BASE(V)                                \
  V(async_wrap)                                                                \
  V(binding)                                                                   \
  V(blob)                                                                      \
  V(buffer)                                                                    \
  V(builtins)                                                                  \
  V(cares_wrap)                                                                \
  V(contextify)                                                                \
  V(credentials)                                                               \
  V(encoding_binding)                                                          \
  V(env_var)                                                                   \
  V(errors)                                                                    \
  V(fs)                                                                        \
  V(fs_dir)                                                                    \
  V(fs_event_wrap)                                                             \
  V(handle_wrap)                                                               \
  V(heap_utils)                                                                \
  V(messaging)                                                                 \
  V(mksnapshot)                                                                \
  V(module_wrap)                                                               \
  V(options)                                                                   \
  V(os)                                                                        \
  V(performance)                                                               \
  V(permission)                                                                \
  V(process_methods)                                                           \
  V(process_object)                                                            \
  V(report)                                                                    \
  V(task_queue)                                                                \
  V(tcp_wrap)                                                                  \
  V(tty_wrap)                                                                  \
  V(url)                                                                       \
  V(util)                                                                      \
  V(pipe_wrap)                                                                 \
  V(sea)                                                                       \
  V(serdes)                                                                    \
  V(string_decoder)                                                            \
  V(stream_wrap)                                                               \
  V(signal_wrap)                                                               \
  V(trace_events)                                                              \
  V(timers)                                                                    \
  V(types)                                                                     \
  V(uv)                                                                        \
  V(v8)                                                                        \
  V(zlib)                                                                      \
  V(wasm_web_api)                                                              \
  V(worker)

#if NODE_HAVE_I18N_SUPPORT
#define EXTERNAL_REFERENCE_BINDING_LIST_I18N(V) V(icu)
#else
#define EXTERNAL_REFERENCE_BINDING_LIST_I18N(V)
#endif  // NODE_HAVE_I18N_SUPPORT

#if HAVE_INSPECTOR
#define EXTERNAL_REFERENCE_BINDING_LIST_INSPECTOR(V)                           \
  V(inspector)                                                                 \
  V(profiler)
#else
#define EXTERNAL_REFERENCE_BINDING_LIST_INSPECTOR(V)
#endif  // HAVE_INSPECTOR

#if HAVE_OPENSSL
#define EXTERNAL_REFERENCE_BINDING_LIST_CRYPTO(V) V(crypto) V(tls_wrap)
#else
#define EXTERNAL_REFERENCE_BINDING_LIST_CRYPTO(V)
#endif  // HAVE_OPENSSL

#define EXTERNAL_REFERENCE_BINDING_LIST(V)                                     \
  EXTERNAL_REFERENCE_BINDING_LIST_BASE(V)                                      \
  EXTERNAL_REFERENCE_BINDING_LIST_INSPECTOR(V)                                 \
  EXTERNAL_REFERENCE_BINDING_LIST_I18N(V)                                      \
  EXTERNAL_REFERENCE_BINDING_LIST_CRYPTO(V)

}  // namespace node

// Declare all the external reference registration functions here,
// and define them later with #NODE_BINDING_EXTERNAL_REFERENCE(modname, func);
#define V(modname)                                                             \
  void _register_external_reference_##modname(                                 \
      node::ExternalReferenceRegistry* registry);
EXTERNAL_REFERENCE_BINDING_LIST(V)
#undef V

#define NODE_BINDING_EXTERNAL_REFERENCE(modname, func)                         \
  void _register_external_reference_##modname(                                 \
      node::ExternalReferenceRegistry* registry) {                             \
    func(registry);                                                            \
  }
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_EXTERNAL_REFERENCE_H_
