// This file contains implementation of error APIs exposed in node.h

#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <cstring>

namespace node {

using v8::Context;
using v8::Exception;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

Local<Value> ErrnoException(Isolate* isolate,
                            int errorno,
                            const char* syscall,
                            const char* msg,
                            const char* path) {
  return TryErrnoException(isolate, errorno, syscall, msg, path)
      .ToLocalChecked();
}

MaybeLocal<Value> TryErrnoException(Isolate* isolate,
                                    int errorno,
                                    const char* syscall,
                                    const char* msg,
                                    const char* path) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);

  Local<Value> e;
  Local<String> estring = OneByteString(isolate, errors::errno_string(errorno));
  if (msg == nullptr || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(isolate, msg);

  Local<String> cons =
      String::Concat(isolate, estring, FIXED_ONE_BYTE_STRING(isolate, ", "));
  cons = String::Concat(isolate, cons, message);

  // FIXME(bnoordhuis) It's questionable to interpret the file path as UTF-8.
  Local<String> path_string;
  if (path != nullptr &&
      !String::NewFromUtf8(isolate, path).ToLocal(&path_string)) {
    return {};
  }
  if (path_string.IsEmpty() == false) {
    cons = String::Concat(isolate, cons, FIXED_ONE_BYTE_STRING(isolate, " '"));
    cons = String::Concat(isolate, cons, path_string);
    cons = String::Concat(isolate, cons, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }
  e = Exception::Error(cons);

  Local<Context> context = env->context();
  Local<Object> obj = e.As<Object>();
  if (obj->Set(context, env->errno_string(), Integer::New(isolate, errorno))
          .IsNothing() ||
      obj->Set(context, env->code_string(), estring).IsNothing()) {
    return {};
  }

  if (!path_string.IsEmpty() &&
      obj->Set(context, env->path_string(), path_string).IsNothing()) {
    return {};
  }

  if (syscall != nullptr &&
      obj->Set(context, env->syscall_string(), OneByteString(isolate, syscall))
          .IsNothing()) {
    return {};
  }

  return e;
}

static MaybeLocal<String> StringFromPath(Isolate* isolate, const char* path) {
#ifdef _WIN32
  if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
    Local<String> path_string;
    if (!String::NewFromUtf8(isolate, path + 8).ToLocal(&path_string)) {
      return {};
    }
    return String::Concat(
        isolate, FIXED_ONE_BYTE_STRING(isolate, "\\\\"), path_string);
  } else if (strncmp(path, "\\\\?\\", 4) == 0) {
    return String::NewFromUtf8(isolate, path + 4);
  }
#endif

  return String::NewFromUtf8(isolate, path);
}

Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* message,
                         const char* path,
                         const char* dest) {
  return TryUVException(isolate, errorno, syscall, message, path, dest)
      .ToLocalChecked();
}

MaybeLocal<Value> TryUVException(Isolate* isolate,
                                 int errorno,
                                 const char* syscall,
                                 const char* msg,
                                 const char* path,
                                 const char* dest) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);

  if (!msg || !msg[0])
    msg = uv_strerror(errorno);

  Local<String> js_code = OneByteString(isolate, uv_err_name(errorno));
  Local<String> js_syscall = OneByteString(isolate, syscall);
  Local<String> js_path;
  Local<String> js_dest;

  Local<String> js_msg = js_code;
  js_msg =
      String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, ": "));
  js_msg = String::Concat(isolate, js_msg, OneByteString(isolate, msg));
  js_msg =
      String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, ", "));
  js_msg = String::Concat(isolate, js_msg, js_syscall);

  if (path != nullptr) {
    if (!StringFromPath(isolate, path).ToLocal(&js_path)) {
      return {};
    }

    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, " '"));
    js_msg = String::Concat(isolate, js_msg, js_path);
    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  if (dest != nullptr) {
    if (!StringFromPath(isolate, dest).ToLocal(&js_dest)) {
      return {};
    }

    js_msg = String::Concat(
        isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, " -> '"));
    js_msg = String::Concat(isolate, js_msg, js_dest);
    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  auto context = isolate->GetCurrentContext();
  Local<Object> e;
  if (!Exception::Error(js_msg)->ToObject(context).ToLocal(&e)) {
    return {};
  }

  if (e->Set(context, env->errno_string(), Integer::New(isolate, errorno))
          .IsNothing() ||
      e->Set(context, env->code_string(), js_code).IsNothing() ||
      e->Set(context, env->syscall_string(), js_syscall).IsNothing()) {
    return {};
  }

  if (!js_path.IsEmpty() &&
      e->Set(context, env->path_string(), js_path).IsNothing()) {
    return {};
  }

  if (!js_dest.IsEmpty() &&
      e->Set(context, env->dest_string(), js_dest).IsNothing()) {
    return {};
  }

  return e;
}

#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char* winapi_strerror(const int errorno, bool* must_free) {
  char* errmsg = nullptr;

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr,
                 errorno,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 reinterpret_cast<LPSTR>(&errmsg),
                 0,
                 nullptr);

  if (errmsg) {
    *must_free = true;

    // Remove trailing newlines
    for (int i = strlen(errmsg) - 1;
         i >= 0 && (errmsg[i] == '\n' || errmsg[i] == '\r');
         i--) {
      errmsg[i] = '\0';
    }

    return errmsg;
  } else {
    // FormatMessage failed
    *must_free = false;
    return "Unknown error";
  }
}

Local<Value> WinapiErrnoException(Isolate* isolate,
                                  int errorno,
                                  const char* syscall,
                                  const char* msg,
                                  const char* path) {
  return TryWinapiErrnoException(isolate, errorno, syscall, msg, path)
      .ToLocalChecked();
}

MaybeLocal<Value> TryWinapiErrnoException(Isolate* isolate,
                                          int errorno,
                                          const char* syscall,
                                          const char* msg,
                                          const char* path) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  Local<Value> e;
  bool must_free = false;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno, &must_free);
  }
  Local<String> message = OneByteString(isolate, msg);
  Local<String> path_string;

  if (path) {
    if (!String::NewFromUtf8(isolate, path).ToLocal(&path_string)) {
      return {};
    }

    Local<String> cons1 =
        String::Concat(isolate, message, FIXED_ONE_BYTE_STRING(isolate, " '"));
    Local<String> cons2 = String::Concat(isolate, cons1, path_string);
    Local<String> cons3 =
        String::Concat(isolate, cons2, FIXED_ONE_BYTE_STRING(isolate, "'"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Context> context = env->context();
  Local<Object> obj = e.As<Object>();

  if (obj->Set(context, env->errno_string(), Integer::New(isolate, errorno))
          .IsNothing()) {
    return {};
  }

  if (!path_string.IsEmpty() &&
      obj->Set(context, env->path_string(), path_string).IsNothing()) {
    return {};
  }

  if (syscall != nullptr &&
      obj->Set(context, env->syscall_string(), OneByteString(isolate, syscall))
          .IsNothing()) {
    return {};
  }

  if (must_free) {
    LocalFree(const_cast<char*>(msg));
  }

  return e;
}
#endif  // _WIN32

// Implement the legacy name exposed in node.h. This has not been in fact
// fatal any more, as the user can handle the exception in the
// TryCatch by listening to `uncaughtException`.
// TODO(joyeecheung): deprecate it in favor of a more accurate name.
void FatalException(Isolate* isolate, const v8::TryCatch& try_catch) {
  errors::TriggerUncaughtException(isolate, try_catch);
}

}  // namespace node
