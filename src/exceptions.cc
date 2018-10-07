#include "node.h"
#include "node_internals.h"
#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"
#include "uv.h"

#include <string.h>

namespace node {

using v8::Exception;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Object;
using v8::String;
using v8::Value;

Local<Value> ErrnoException(Isolate* isolate,
                            int errorno,
                            const char* syscall,
                            const char* msg,
                            const char* path) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Value> e;
  Local<String> estring = OneByteString(isolate, errno_string(errorno));
  if (msg == nullptr || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(isolate, msg);

  Local<String> cons =
      String::Concat(isolate, estring, FIXED_ONE_BYTE_STRING(isolate, ", "));
  cons = String::Concat(isolate, cons, message);

  Local<String> path_string;
  if (path != nullptr) {
    // FIXME(bnoordhuis) It's questionable to interpret the file path as UTF-8.
    path_string = String::NewFromUtf8(isolate, path, v8::NewStringType::kNormal)
                      .ToLocalChecked();
  }

  if (path_string.IsEmpty() == false) {
    cons = String::Concat(isolate, cons, FIXED_ONE_BYTE_STRING(isolate, " '"));
    cons = String::Concat(isolate, cons, path_string);
    cons = String::Concat(isolate, cons, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }
  e = Exception::Error(cons);

  Local<Object> obj = e.As<Object>();
  obj->Set(env->errno_string(), Integer::New(isolate, errorno));
  obj->Set(env->code_string(), estring);

  if (path_string.IsEmpty() == false) {
    obj->Set(env->path_string(), path_string);
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(isolate, syscall));
  }

  return e;
}

static Local<String> StringFromPath(Isolate* isolate, const char* path) {
#ifdef _WIN32
  if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
    return String::Concat(
        isolate,
        FIXED_ONE_BYTE_STRING(isolate, "\\\\"),
        String::NewFromUtf8(isolate, path + 8, v8::NewStringType::kNormal)
            .ToLocalChecked());
  } else if (strncmp(path, "\\\\?\\", 4) == 0) {
    return String::NewFromUtf8(isolate, path + 4, v8::NewStringType::kNormal)
        .ToLocalChecked();
  }
#endif

  return String::NewFromUtf8(isolate, path, v8::NewStringType::kNormal)
      .ToLocalChecked();
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path) {
  return UVException(isolate, errorno, syscall, msg, path, nullptr);
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path,
                         const char* dest) {
  Environment* env = Environment::GetCurrent(isolate);

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
    js_path = StringFromPath(isolate, path);

    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, " '"));
    js_msg = String::Concat(isolate, js_msg, js_path);
    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  if (dest != nullptr) {
    js_dest = StringFromPath(isolate, dest);

    js_msg = String::Concat(
        isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, " -> '"));
    js_msg = String::Concat(isolate, js_msg, js_dest);
    js_msg =
        String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  Local<Object> e =
    Exception::Error(js_msg)->ToObject(isolate->GetCurrentContext())
      .ToLocalChecked();

  e->Set(env->errno_string(), Integer::New(isolate, errorno));
  e->Set(env->code_string(), js_code);
  e->Set(env->syscall_string(), js_syscall);
  if (!js_path.IsEmpty())
    e->Set(env->path_string(), js_path);
  if (!js_dest.IsEmpty())
    e->Set(env->dest_string(), js_dest);

  return e;
}

#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char* winapi_strerror(const int errorno, bool* must_free) {
  char* errmsg = nullptr;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, nullptr);

  if (errmsg) {
    *must_free = true;

    // Remove trailing newlines
    for (int i = strlen(errmsg) - 1;
        i >= 0 && (errmsg[i] == '\n' || errmsg[i] == '\r'); i--) {
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
  Environment* env = Environment::GetCurrent(isolate);
  Local<Value> e;
  bool must_free = false;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno, &must_free);
  }
  Local<String> message = OneByteString(isolate, msg);

  if (path) {
    Local<String> cons1 =
        String::Concat(isolate, message, FIXED_ONE_BYTE_STRING(isolate, " '"));
    Local<String> cons2 = String::Concat(
        isolate,
        cons1,
        String::NewFromUtf8(isolate, path, v8::NewStringType::kNormal)
            .ToLocalChecked());
    Local<String> cons3 =
        String::Concat(isolate, cons2, FIXED_ONE_BYTE_STRING(isolate, "'"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Object> obj = e.As<Object>();
  obj->Set(env->errno_string(), Integer::New(isolate, errorno));

  if (path != nullptr) {
    obj->Set(env->path_string(),
             String::NewFromUtf8(isolate, path, v8::NewStringType::kNormal)
                 .ToLocalChecked());
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(isolate, syscall));
  }

  if (must_free)
    LocalFree((HLOCAL)msg);

  return e;
}
#endif

}  // namespace node
