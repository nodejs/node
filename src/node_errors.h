#ifndef SRC_NODE_ERRORS_H_
#define SRC_NODE_ERRORS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils-inl.h"
#include "env.h"
#include "v8.h"

// Use ostringstream to print exact-width integer types
// because the format specifiers are not available on AIX.
#include <sstream>

namespace node {

enum ErrorHandlingMode { CONTEXTIFY_ERROR, FATAL_ERROR, MODULE_ERROR };
void AppendExceptionLine(Environment* env,
                         v8::Local<v8::Value> er,
                         v8::Local<v8::Message> message,
                         enum ErrorHandlingMode mode);

[[noreturn]] void FatalError(const char* location, const char* message);
void OnFatalError(const char* location, const char* message);

// Helpers to construct errors similar to the ones provided by
// lib/internal/errors.js.
// Example: with `V(ERR_INVALID_ARG_TYPE, TypeError)`, there will be
// `node::ERR_INVALID_ARG_TYPE(isolate, "message")` returning
// a `Local<Value>` containing the TypeError with proper code and message

#define ERRORS_WITH_CODE(V)                                                    \
  V(ERR_BUFFER_CONTEXT_NOT_AVAILABLE, Error)                                   \
  V(ERR_BUFFER_OUT_OF_BOUNDS, RangeError)                                      \
  V(ERR_BUFFER_TOO_LARGE, Error)                                               \
  V(ERR_CLOSED_MESSAGE_PORT, Error)                                            \
  V(ERR_CONSTRUCT_CALL_REQUIRED, TypeError)                                    \
  V(ERR_CONSTRUCT_CALL_INVALID, TypeError)                                     \
  V(ERR_CRYPTO_INITIALIZATION_FAILED, Error)                                   \
  V(ERR_CRYPTO_INVALID_AUTH_TAG, TypeError)                                    \
  V(ERR_CRYPTO_INVALID_COUNTER, TypeError)                                     \
  V(ERR_CRYPTO_INVALID_CURVE, TypeError)                                       \
  V(ERR_CRYPTO_INVALID_DIGEST, TypeError)                                      \
  V(ERR_CRYPTO_INVALID_IV, TypeError)                                          \
  V(ERR_CRYPTO_INVALID_JWK, TypeError)                                         \
  V(ERR_CRYPTO_INVALID_KEYLEN, RangeError)                                     \
  V(ERR_CRYPTO_INVALID_KEYPAIR, RangeError)                                    \
  V(ERR_CRYPTO_INVALID_KEYTYPE, RangeError)                                    \
  V(ERR_CRYPTO_INVALID_MESSAGELEN, RangeError)                                 \
  V(ERR_CRYPTO_INVALID_SCRYPT_PARAMS, RangeError)                              \
  V(ERR_CRYPTO_INVALID_STATE, Error)                                           \
  V(ERR_CRYPTO_INVALID_TAG_LENGTH, RangeError)                                 \
  V(ERR_CRYPTO_JWK_UNSUPPORTED_CURVE, Error)                                   \
  V(ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE, Error)                                \
  V(ERR_CRYPTO_OPERATION_FAILED, Error)                                        \
  V(ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH, RangeError)                           \
  V(ERR_CRYPTO_UNKNOWN_CIPHER, Error)                                          \
  V(ERR_CRYPTO_UNKNOWN_DH_GROUP, Error)                                        \
  V(ERR_CRYPTO_UNSUPPORTED_OPERATION, Error)                                   \
  V(ERR_CRYPTO_JOB_INIT_FAILED, Error)                                         \
  V(ERR_DLOPEN_DISABLED, Error)                                                \
  V(ERR_DLOPEN_FAILED, Error)                                                  \
  V(ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE, Error)                            \
  V(ERR_INVALID_ADDRESS, Error)                                                \
  V(ERR_INVALID_ARG_VALUE, TypeError)                                          \
  V(ERR_OSSL_EVP_INVALID_DIGEST, Error)                                        \
  V(ERR_INVALID_ARG_TYPE, TypeError)                                           \
  V(ERR_INVALID_OBJECT_DEFINE_PROPERTY, TypeError)                             \
  V(ERR_INVALID_MODULE, Error)                                                 \
  V(ERR_INVALID_THIS, TypeError)                                               \
  V(ERR_INVALID_TRANSFER_OBJECT, TypeError)                                    \
  V(ERR_MEMORY_ALLOCATION_FAILED, Error)                                       \
  V(ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE, Error)                             \
  V(ERR_MISSING_ARGS, TypeError)                                               \
  V(ERR_MISSING_TRANSFERABLE_IN_TRANSFER_LIST, TypeError)                      \
  V(ERR_MISSING_PASSPHRASE, TypeError)                                         \
  V(ERR_MISSING_PLATFORM_FOR_WORKER, Error)                                    \
  V(ERR_NON_CONTEXT_AWARE_DISABLED, Error)                                     \
  V(ERR_OUT_OF_RANGE, RangeError)                                              \
  V(ERR_SCRIPT_EXECUTION_INTERRUPTED, Error)                                   \
  V(ERR_SCRIPT_EXECUTION_TIMEOUT, Error)                                       \
  V(ERR_STRING_TOO_LONG, Error)                                                \
  V(ERR_TLS_INVALID_PROTOCOL_METHOD, TypeError)                                \
  V(ERR_TLS_PSK_SET_IDENTIY_HINT_FAILED, Error)                                \
  V(ERR_VM_MODULE_CACHED_DATA_REJECTED, Error)                                 \
  V(ERR_VM_MODULE_LINK_FAILURE, Error)                                         \
  V(ERR_WASI_NOT_STARTED, Error)                                               \
  V(ERR_WORKER_INIT_FAILED, Error)                                             \
  V(ERR_PROTO_ACCESS, Error)

#define V(code, type)                                                          \
  template <typename... Args>                                                  \
  inline v8::Local<v8::Value> code(                                            \
      v8::Isolate* isolate, const char* format, Args&&... args) {              \
    std::string message = SPrintF(format, std::forward<Args>(args)...);        \
    v8::Local<v8::String> js_code = OneByteString(isolate, #code);             \
    v8::Local<v8::String> js_msg =                                             \
        OneByteString(isolate, message.c_str(), message.length());             \
    v8::Local<v8::Object> e = v8::Exception::type(js_msg)                      \
                                  ->ToObject(isolate->GetCurrentContext())     \
                                  .ToLocalChecked();                           \
    e->Set(isolate->GetCurrentContext(),                                       \
           OneByteString(isolate, "code"),                                     \
           js_code)                                                            \
        .Check();                                                              \
    return e;                                                                  \
  }                                                                            \
  template <typename... Args>                                                  \
  inline void THROW_##code(                                                    \
      v8::Isolate* isolate, const char* format, Args&&... args) {              \
    isolate->ThrowException(                                                   \
        code(isolate, format, std::forward<Args>(args)...));                   \
  }                                                                            \
  template <typename... Args>                                                  \
  inline void THROW_##code(                                                    \
      Environment* env, const char* format, Args&&... args) {                  \
    THROW_##code(env->isolate(), format, std::forward<Args>(args)...);         \
  }
ERRORS_WITH_CODE(V)
#undef V

// Errors with predefined static messages

#define PREDEFINED_ERROR_MESSAGES(V)                                           \
  V(ERR_BUFFER_CONTEXT_NOT_AVAILABLE,                                          \
    "Buffer is not available for the current Context")                         \
  V(ERR_CLOSED_MESSAGE_PORT, "Cannot send data on closed MessagePort")         \
  V(ERR_CONSTRUCT_CALL_INVALID, "Constructor cannot be called")                \
  V(ERR_CONSTRUCT_CALL_REQUIRED, "Cannot call constructor without `new`")      \
  V(ERR_CRYPTO_INITIALIZATION_FAILED, "Initialization failed")                 \
  V(ERR_CRYPTO_INVALID_AUTH_TAG, "Invalid authentication tag")                 \
  V(ERR_CRYPTO_INVALID_COUNTER, "Invalid counter")                             \
  V(ERR_CRYPTO_INVALID_CURVE, "Invalid EC curve name")                         \
  V(ERR_CRYPTO_INVALID_DIGEST, "Invalid digest")                               \
  V(ERR_CRYPTO_INVALID_IV, "Invalid initialization vector")                    \
  V(ERR_CRYPTO_INVALID_JWK, "Invalid JWK format")                              \
  V(ERR_CRYPTO_INVALID_KEYLEN, "Invalid key length")                           \
  V(ERR_CRYPTO_INVALID_KEYPAIR, "Invalid key pair")                            \
  V(ERR_CRYPTO_INVALID_KEYTYPE, "Invalid key type")                            \
  V(ERR_CRYPTO_INVALID_MESSAGELEN, "Invalid message length")                   \
  V(ERR_CRYPTO_INVALID_SCRYPT_PARAMS, "Invalid scrypt params")                 \
  V(ERR_CRYPTO_INVALID_STATE, "Invalid state")                                 \
  V(ERR_CRYPTO_INVALID_TAG_LENGTH, "Invalid taglength")                        \
  V(ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE, "Unsupported JWK Key Type.")          \
  V(ERR_CRYPTO_OPERATION_FAILED, "Operation failed")                           \
  V(ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH,                                       \
    "Input buffers must have the same byte length")                            \
  V(ERR_CRYPTO_UNKNOWN_CIPHER, "Unknown cipher")                               \
  V(ERR_CRYPTO_UNKNOWN_DH_GROUP, "Unknown DH group")                           \
  V(ERR_CRYPTO_UNSUPPORTED_OPERATION, "Unsupported crypto operation")          \
  V(ERR_CRYPTO_JOB_INIT_FAILED, "Failed to initialize crypto job config")      \
  V(ERR_DLOPEN_FAILED, "DLOpen failed")                                        \
  V(ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE,                                   \
    "Context not associated with Node.js environment")                         \
  V(ERR_INVALID_ADDRESS, "Invalid socket address")                             \
  V(ERR_INVALID_MODULE, "No such module")                                      \
  V(ERR_INVALID_THIS, "Value of \"this\" is the wrong type")                   \
  V(ERR_INVALID_TRANSFER_OBJECT, "Found invalid object in transferList")       \
  V(ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate memory")                 \
  V(ERR_OSSL_EVP_INVALID_DIGEST, "Invalid digest used")                        \
  V(ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE,                                    \
    "A message object could not be deserialized successfully in the target "   \
    "vm.Context")                                                              \
  V(ERR_MISSING_TRANSFERABLE_IN_TRANSFER_LIST,                                 \
    "Object that needs transfer was found in message but not listed "          \
    "in transferList")                                                         \
  V(ERR_MISSING_PLATFORM_FOR_WORKER,                                           \
    "The V8 platform used by this instance of Node does not support "          \
    "creating Workers")                                                        \
  V(ERR_NON_CONTEXT_AWARE_DISABLED,                                            \
    "Loading non context-aware native modules has been disabled")              \
  V(ERR_SCRIPT_EXECUTION_INTERRUPTED,                                          \
    "Script execution was interrupted by `SIGINT`")                            \
  V(ERR_TLS_PSK_SET_IDENTIY_HINT_FAILED, "Failed to set PSK identity hint")    \
  V(ERR_WASI_NOT_STARTED, "wasi.start() has not been called")                  \
  V(ERR_WORKER_INIT_FAILED, "Worker initialization failure")                   \
  V(ERR_PROTO_ACCESS,                                                          \
    "Accessing Object.prototype.__proto__ has been "                           \
    "disallowed with --disable-proto=throw")

#define V(code, message)                                                     \
  inline v8::Local<v8::Value> code(v8::Isolate* isolate) {                   \
    return code(isolate, message);                                           \
  }                                                                          \
  inline void THROW_ ## code(v8::Isolate* isolate) {                         \
    isolate->ThrowException(code(isolate, message));                         \
  }                                                                          \
  inline void THROW_ ## code(Environment* env) {                             \
    THROW_ ## code(env->isolate());                                          \
  }
  PREDEFINED_ERROR_MESSAGES(V)
#undef V

// Errors with predefined non-static messages
inline void THROW_ERR_SCRIPT_EXECUTION_TIMEOUT(Environment* env,
                                               int64_t timeout) {
  std::ostringstream message;
  message << "Script execution timed out after ";
  message << timeout << "ms";
  THROW_ERR_SCRIPT_EXECUTION_TIMEOUT(env, message.str().c_str());
}

inline v8::Local<v8::Value> ERR_BUFFER_TOO_LARGE(v8::Isolate* isolate) {
  char message[128];
  snprintf(message, sizeof(message),
      "Cannot create a Buffer larger than 0x%zx bytes",
      v8::TypedArray::kMaxLength);
  return ERR_BUFFER_TOO_LARGE(isolate, message);
}

inline v8::Local<v8::Value> ERR_STRING_TOO_LONG(v8::Isolate* isolate) {
  char message[128];
  snprintf(message, sizeof(message),
      "Cannot create a string longer than 0x%x characters",
      v8::String::kMaxLength);
  return ERR_STRING_TOO_LONG(isolate, message);
}

#define THROW_AND_RETURN_IF_NOT_BUFFER(env, val, prefix)                     \
  do {                                                                       \
    if (!Buffer::HasInstance(val))                                           \
      return node::THROW_ERR_INVALID_ARG_TYPE(env,                           \
                                              prefix " must be a buffer");   \
  } while (0)

#define THROW_AND_RETURN_IF_NOT_STRING(env, val, prefix)                     \
  do {                                                                       \
    if (!val->IsString())                                                    \
      return node::THROW_ERR_INVALID_ARG_TYPE(env,                           \
                                              prefix " must be a string");   \
  } while (0)

namespace errors {

class TryCatchScope : public v8::TryCatch {
 public:
  enum class CatchMode { kNormal, kFatal };

  explicit TryCatchScope(Environment* env, CatchMode mode = CatchMode::kNormal)
      : v8::TryCatch(env->isolate()), env_(env), mode_(mode) {}
  ~TryCatchScope();

  // Since the dtor is not virtual we need to make sure no one creates
  // object of it in the free store that might be held by polymorphic pointers.
  void* operator new(std::size_t count) = delete;
  void* operator new[](std::size_t count) = delete;
  TryCatchScope(TryCatchScope&) = delete;
  TryCatchScope(TryCatchScope&&) = delete;
  TryCatchScope operator=(TryCatchScope&) = delete;
  TryCatchScope operator=(TryCatchScope&&) = delete;

 private:
  Environment* env_;
  CatchMode mode_;
};

// Trigger the global uncaught exception handler `process._fatalException`
// in JS land (which emits the 'uncaughtException' event). If that returns
// true, continue program execution, otherwise exit the process.
void TriggerUncaughtException(v8::Isolate* isolate,
                              const v8::TryCatch& try_catch);
void TriggerUncaughtException(v8::Isolate* isolate,
                              v8::Local<v8::Value> error,
                              v8::Local<v8::Message> message,
                              bool from_promise = false);

const char* errno_string(int errorno);
void PerIsolateMessageListener(v8::Local<v8::Message> message,
                               v8::Local<v8::Value> error);

void DecorateErrorStack(Environment* env,
                        const errors::TryCatchScope& try_catch);
}  // namespace errors

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_ERRORS_H_
