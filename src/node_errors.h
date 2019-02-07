#ifndef SRC_NODE_ERRORS_H_
#define SRC_NODE_ERRORS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "util-inl.h"
#include "env-inl.h"
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

void PrintErrorString(const char* format, ...);

void ReportException(Environment* env, const v8::TryCatch& try_catch);

void FatalException(v8::Isolate* isolate,
                    v8::Local<v8::Value> error,
                    v8::Local<v8::Message> message);

void FatalException(const v8::FunctionCallbackInfo<v8::Value>& args);

// Helpers to construct errors similar to the ones provided by
// lib/internal/errors.js.
// Example: with `V(ERR_INVALID_ARG_TYPE, TypeError)`, there will be
// `node::ERR_INVALID_ARG_TYPE(isolate, "message")` returning
// a `Local<Value>` containing the TypeError with proper code and message

#define ERRORS_WITH_CODE(V)                                                  \
  V(ERR_BUFFER_CONTEXT_NOT_AVAILABLE, Error)                                 \
  V(ERR_BUFFER_OUT_OF_BOUNDS, RangeError)                                    \
  V(ERR_BUFFER_TOO_LARGE, Error)                                             \
  V(ERR_CANNOT_TRANSFER_OBJECT, TypeError)                                   \
  V(ERR_CLOSED_MESSAGE_PORT, Error)                                          \
  V(ERR_CONSTRUCT_CALL_REQUIRED, Error)                                      \
  V(ERR_INVALID_ARG_VALUE, TypeError)                                        \
  V(ERR_INVALID_ARG_TYPE, TypeError)                                         \
  V(ERR_INVALID_TRANSFER_OBJECT, TypeError)                                  \
  V(ERR_MEMORY_ALLOCATION_FAILED, Error)                                     \
  V(ERR_MISSING_ARGS, TypeError)                                             \
  V(ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST, TypeError)                    \
  V(ERR_MISSING_MODULE, Error)                                               \
  V(ERR_MISSING_PLATFORM_FOR_WORKER, Error)                                  \
  V(ERR_OUT_OF_RANGE, RangeError)                                            \
  V(ERR_SCRIPT_EXECUTION_INTERRUPTED, Error)                                 \
  V(ERR_SCRIPT_EXECUTION_TIMEOUT, Error)                                     \
  V(ERR_STRING_TOO_LONG, Error)                                              \
  V(ERR_TLS_INVALID_PROTOCOL_METHOD, TypeError)                              \
  V(ERR_TRANSFERRING_EXTERNALIZED_SHAREDARRAYBUFFER, TypeError)              \

#define V(code, type)                                                         \
  inline v8::Local<v8::Value> code(v8::Isolate* isolate,                      \
                                   const char* message)       {               \
    v8::Local<v8::String> js_code = OneByteString(isolate, #code);            \
    v8::Local<v8::String> js_msg = OneByteString(isolate, message);           \
    v8::Local<v8::Object> e =                                                 \
        v8::Exception::type(js_msg)->ToObject(                                \
            isolate->GetCurrentContext()).ToLocalChecked();                   \
    e->Set(isolate->GetCurrentContext(), OneByteString(isolate, "code"),      \
           js_code).FromJust();                                               \
    return e;                                                                 \
  }                                                                           \
  inline void THROW_ ## code(v8::Isolate* isolate, const char* message) {     \
    isolate->ThrowException(code(isolate, message));                          \
  }                                                                           \
  inline void THROW_ ## code(Environment* env, const char* message) {         \
    THROW_ ## code(env->isolate(), message);                                  \
  }
  ERRORS_WITH_CODE(V)
#undef V

// Errors with predefined static messages

#define PREDEFINED_ERROR_MESSAGES(V)                                         \
  V(ERR_BUFFER_CONTEXT_NOT_AVAILABLE,                                        \
    "Buffer is not available for the current Context")                       \
  V(ERR_CANNOT_TRANSFER_OBJECT, "Cannot transfer object of unsupported type")\
  V(ERR_CLOSED_MESSAGE_PORT, "Cannot send data on closed MessagePort")       \
  V(ERR_CONSTRUCT_CALL_REQUIRED, "Cannot call constructor without `new`")    \
  V(ERR_INVALID_TRANSFER_OBJECT, "Found invalid object in transferList")     \
  V(ERR_MEMORY_ALLOCATION_FAILED, "Failed to allocate memory")               \
  V(ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST,                               \
    "MessagePort was found in message but not listed in transferList")       \
  V(ERR_MISSING_PLATFORM_FOR_WORKER,                                         \
    "The V8 platform used by this instance of Node does not support "        \
    "creating Workers")                                                      \
  V(ERR_SCRIPT_EXECUTION_INTERRUPTED,                                        \
    "Script execution was interrupted by `SIGINT`")                          \
  V(ERR_TRANSFERRING_EXTERNALIZED_SHAREDARRAYBUFFER,                         \
    "Cannot serialize externalized SharedArrayBuffer")                       \

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

const char* errno_string(int errorno);

}  // namespace errors

void DecorateErrorStack(Environment* env,
                        const errors::TryCatchScope& try_catch);
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_ERRORS_H_
