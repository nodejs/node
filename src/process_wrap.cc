// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <string.h>
#include <stdlib.h>

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

class ProcessWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "Process"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);

    NODE_SET_PROTOTYPE_METHOD(constructor, "spawn", Spawn);
    NODE_SET_PROTOTYPE_METHOD(constructor, "kill", Kill);

    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);

    target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Process"),
                constructor->GetFunction());
  }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());
    new ProcessWrap(env, args.This());
  }

  ProcessWrap(Environment* env, Handle<Object> object)
      : HandleWrap(env, object, reinterpret_cast<uv_handle_t*>(&process_)) {
  }

  ~ProcessWrap() {
  }

  static void ParseStdioOptions(Environment* env,
                                Local<Object> js_options,
                                uv_process_options_t* options) {
    Local<String> stdio_key =
        FIXED_ONE_BYTE_STRING(node_isolate, "stdio");
    Local<Array> stdios = js_options->Get(stdio_key).As<Array>();

    uint32_t len = stdios->Length();
    options->stdio = new uv_stdio_container_t[len];
    options->stdio_count = len;

    for (uint32_t i = 0; i < len; i++) {
      Local<Object> stdio = stdios->Get(i).As<Object>();
      Local<Value> type =
          stdio->Get(FIXED_ONE_BYTE_STRING(node_isolate, "type"));

      if (type->Equals(FIXED_ONE_BYTE_STRING(node_isolate, "ignore"))) {
        options->stdio[i].flags = UV_IGNORE;
      } else if (type->Equals(FIXED_ONE_BYTE_STRING(node_isolate, "pipe"))) {
        options->stdio[i].flags = static_cast<uv_stdio_flags>(
            UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE);
        Local<String> handle_key =
            FIXED_ONE_BYTE_STRING(node_isolate, "handle");
        Local<Object> handle = stdio->Get(handle_key).As<Object>();
        options->stdio[i].data.stream =
            reinterpret_cast<uv_stream_t*>(
                Unwrap<PipeWrap>(handle)->UVHandle());
      } else if (type->Equals(FIXED_ONE_BYTE_STRING(node_isolate, "wrap"))) {
        Local<String> handle_key =
            FIXED_ONE_BYTE_STRING(node_isolate, "handle");
        Local<Object> handle = stdio->Get(handle_key).As<Object>();
        uv_stream_t* stream = HandleToStream(env, handle);
        assert(stream != NULL);

        options->stdio[i].flags = UV_INHERIT_STREAM;
        options->stdio[i].data.stream = stream;
      } else {
        Local<String> fd_key = FIXED_ONE_BYTE_STRING(node_isolate, "fd");
        int fd = static_cast<int>(stdio->Get(fd_key)->IntegerValue());
        options->stdio[i].flags = UV_INHERIT_FD;
        options->stdio[i].data.fd = fd;
      }
    }
  }

  static void Spawn(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());

    ProcessWrap* wrap = Unwrap<ProcessWrap>(args.This());

    Local<Object> js_options = args[0]->ToObject();

    uv_process_options_t options;
    memset(&options, 0, sizeof(uv_process_options_t));

    options.exit_cb = OnExit;

    // options.uid
    Local<Value> uid_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "uid"));
    if (uid_v->IsInt32()) {
      int32_t uid = uid_v->Int32Value();
      if (uid & ~((uv_uid_t) ~0)) {
        return ThrowRangeError("options.uid is out of range");
      }
      options.flags |= UV_PROCESS_SETUID;
      options.uid = (uv_uid_t) uid;
    } else if (!uid_v->IsUndefined() && !uid_v->IsNull()) {
      return ThrowTypeError("options.uid should be a number");
    }

    // options.gid
    Local<Value> gid_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "gid"));
    if (gid_v->IsInt32()) {
      int32_t gid = gid_v->Int32Value();
      if (gid & ~((uv_gid_t) ~0)) {
        return ThrowRangeError("options.gid is out of range");
      }
      options.flags |= UV_PROCESS_SETGID;
      options.gid = (uv_gid_t) gid;
    } else if (!gid_v->IsUndefined() && !gid_v->IsNull()) {
      return ThrowTypeError("options.gid should be a number");
    }

    // TODO(bnoordhuis) is this possible to do without mallocing ?

    // options.file
    Local<Value> file_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "file"));
    String::Utf8Value file(file_v->IsString() ? file_v : Local<Value>());
    if (file.length() > 0) {
      options.file = *file;
    } else {
      return ThrowTypeError("Bad argument");
    }

    // options.args
    Local<Value> argv_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "args"));
    if (!argv_v.IsEmpty() && argv_v->IsArray()) {
      Local<Array> js_argv = Local<Array>::Cast(argv_v);
      int argc = js_argv->Length();
      // Heap allocate to detect errors. +1 is for NULL.
      options.args = new char*[argc + 1];
      for (int i = 0; i < argc; i++) {
        String::Utf8Value arg(js_argv->Get(i));
        options.args[i] = strdup(*arg);
      }
      options.args[argc] = NULL;
    }

    // options.cwd
    Local<Value> cwd_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "cwd"));
    String::Utf8Value cwd(cwd_v->IsString() ? cwd_v : Local<Value>());
    if (cwd.length() > 0) {
      options.cwd = *cwd;
    }

    // options.env
    Local<Value> env_v =
        js_options->Get(FIXED_ONE_BYTE_STRING(node_isolate, "envPairs"));
    if (!env_v.IsEmpty() && env_v->IsArray()) {
      Local<Array> env = Local<Array>::Cast(env_v);
      int envc = env->Length();
      options.env = new char*[envc + 1];  // Heap allocated to detect errors.
      for (int i = 0; i < envc; i++) {
        String::Utf8Value pair(env->Get(i));
        options.env[i] = strdup(*pair);
      }
      options.env[envc] = NULL;
    }

    // options.stdio
    ParseStdioOptions(env, js_options, &options);

    // options.windows_verbatim_arguments
    Local<String> windows_verbatim_arguments_key =
        FIXED_ONE_BYTE_STRING(node_isolate, "windowsVerbatimArguments");
    if (js_options->Get(windows_verbatim_arguments_key)->IsTrue()) {
      options.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
    }

    // options.detached
    Local<String> detached_key =
        FIXED_ONE_BYTE_STRING(node_isolate, "detached");
    if (js_options->Get(detached_key)->IsTrue()) {
      options.flags |= UV_PROCESS_DETACHED;
    }

    int err = uv_spawn(env->event_loop(), &wrap->process_, &options);

    if (err == 0) {
      assert(wrap->process_.data == wrap);
      wrap->object()->Set(FIXED_ONE_BYTE_STRING(node_isolate, "pid"),
                          Integer::New(wrap->process_.pid, node_isolate));
    }

    if (options.args) {
      for (int i = 0; options.args[i]; i++) free(options.args[i]);
      delete [] options.args;
    }

    if (options.env) {
      for (int i = 0; options.env[i]; i++) free(options.env[i]);
      delete [] options.env;
    }

    delete[] options.stdio;

    args.GetReturnValue().Set(err);
  }

  static void Kill(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    ProcessWrap* wrap = Unwrap<ProcessWrap>(args.This());

    int signal = args[0]->Int32Value();
    int err = uv_process_kill(&wrap->process_, signal);
    args.GetReturnValue().Set(err);
  }

  static void OnExit(uv_process_t* handle,
                     int64_t exit_status,
                     int term_signal) {
    ProcessWrap* wrap = static_cast<ProcessWrap*>(handle->data);
    assert(wrap != NULL);
    assert(&wrap->process_ == handle);

    Environment* env = wrap->env();
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());

    Local<Value> argv[] = {
      Number::New(node_isolate, static_cast<double>(exit_status)),
      OneByteString(node_isolate, signo_string(term_signal))
    };

    MakeCallback(env,
                 wrap->object(),
                 env->onexit_string(),
                 ARRAY_SIZE(argv),
                 argv);
  }

  uv_process_t process_;
};


}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_process_wrap, node::ProcessWrap::Initialize)
