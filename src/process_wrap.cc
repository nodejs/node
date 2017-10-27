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

#include "env-inl.h"
#include "handle_wrap.h"
#include "node_wrap.h"
#include "util-inl.h"

#include <string.h>
#include <stdlib.h>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace {

class ProcessWrap : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
    Environment* env = Environment::GetCurrent(context);
    Local<FunctionTemplate> constructor = env->NewFunctionTemplate(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    Local<String> processString =
        FIXED_ONE_BYTE_STRING(env->isolate(), "Process");
    constructor->SetClassName(processString);

    AsyncWrap::AddWrapMethods(env, constructor);

    env->SetProtoMethod(constructor, "close", HandleWrap::Close);

    env->SetProtoMethod(constructor, "spawn", Spawn);
    env->SetProtoMethod(constructor, "kill", Kill);

    env->SetProtoMethod(constructor, "ref", HandleWrap::Ref);
    env->SetProtoMethod(constructor, "unref", HandleWrap::Unref);
    env->SetProtoMethod(constructor, "hasRef", HandleWrap::HasRef);

    target->Set(processString, constructor->GetFunction());
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    CHECK(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args);
    new ProcessWrap(env, args.This());
  }

  ProcessWrap(Environment* env, Local<Object> object)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&process_),
                   AsyncWrap::PROVIDER_PROCESSWRAP) {
  }

  static void ParseStdioOptions(Environment* env,
                                Local<Object> js_options,
                                uv_process_options_t* options) {
    Local<Context> context = env->context();
    Local<String> stdio_key = env->stdio_string();
    Local<Array> stdios =
        js_options->Get(context, stdio_key).ToLocalChecked().As<Array>();

    uint32_t len = stdios->Length();
    options->stdio = new uv_stdio_container_t[len];
    options->stdio_count = len;

    for (uint32_t i = 0; i < len; i++) {
      Local<Object> stdio =
          stdios->Get(context, i).ToLocalChecked().As<Object>();
      Local<Value> type =
          stdio->Get(context, env->type_string()).ToLocalChecked();

      if (type->Equals(env->ignore_string())) {
        options->stdio[i].flags = UV_IGNORE;
      } else if (type->Equals(env->pipe_string())) {
        options->stdio[i].flags = static_cast<uv_stdio_flags>(
            UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE);
        Local<String> handle_key = env->handle_string();
        Local<Object> handle =
            stdio->Get(context, handle_key).ToLocalChecked().As<Object>();
        CHECK(!handle.IsEmpty());
        options->stdio[i].data.stream =
            reinterpret_cast<uv_stream_t*>(
                Unwrap<PipeWrap>(handle)->UVHandle());
      } else if (type->Equals(env->wrap_string())) {
        Local<String> handle_key = env->handle_string();
        Local<Object> handle =
            stdio->Get(context, handle_key).ToLocalChecked().As<Object>();
        uv_stream_t* stream = HandleToStream(env, handle);
        CHECK_NE(stream, nullptr);

        options->stdio[i].flags = UV_INHERIT_STREAM;
        options->stdio[i].data.stream = stream;
      } else {
        Local<String> fd_key = env->fd_string();
        int fd = static_cast<int>(
            stdio->Get(context, fd_key).ToLocalChecked()->IntegerValue());
        options->stdio[i].flags = UV_INHERIT_FD;
        options->stdio[i].data.fd = fd;
      }
    }
  }

  static void Spawn(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    Local<Context> context = env->context();
    ProcessWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

    Local<Object> js_options = args[0]->ToObject(env->isolate());

    uv_process_options_t options;
    memset(&options, 0, sizeof(uv_process_options_t));

    options.exit_cb = OnExit;

    // options.uid
    Local<Value> uid_v =
        js_options->Get(context, env->uid_string()).ToLocalChecked();
    if (!uid_v->IsUndefined() && !uid_v->IsNull()) {
      CHECK(uid_v->IsInt32());
      const int32_t uid = uid_v->Int32Value(context).FromJust();
      options.flags |= UV_PROCESS_SETUID;
      options.uid = static_cast<uv_uid_t>(uid);
    }

    // options.gid
    Local<Value> gid_v =
        js_options->Get(context, env->gid_string()).ToLocalChecked();
    if (!gid_v->IsUndefined() && !gid_v->IsNull()) {
      CHECK(gid_v->IsInt32());
      const int32_t gid = gid_v->Int32Value(context).FromJust();
      options.flags |= UV_PROCESS_SETGID;
      options.gid = static_cast<uv_gid_t>(gid);
    }

    // TODO(bnoordhuis) is this possible to do without mallocing ?

    // options.file
    Local<Value> file_v =
        js_options->Get(context, env->file_string()).ToLocalChecked();
    CHECK(file_v->IsString());
    node::Utf8Value file(env->isolate(), file_v);
    options.file = *file;

    // options.args
    Local<Value> argv_v =
        js_options->Get(context, env->args_string()).ToLocalChecked();
    if (!argv_v.IsEmpty() && argv_v->IsArray()) {
      Local<Array> js_argv = Local<Array>::Cast(argv_v);
      int argc = js_argv->Length();
      // Heap allocate to detect errors. +1 is for nullptr.
      options.args = new char*[argc + 1];
      for (int i = 0; i < argc; i++) {
        node::Utf8Value arg(env->isolate(),
                            js_argv->Get(context, i).ToLocalChecked());
        options.args[i] = strdup(*arg);
        CHECK_NE(options.args[i], nullptr);
      }
      options.args[argc] = nullptr;
    }

    // options.cwd
    Local<Value> cwd_v =
        js_options->Get(context, env->cwd_string()).ToLocalChecked();
    node::Utf8Value cwd(env->isolate(),
                        cwd_v->IsString() ? cwd_v : Local<Value>());
    if (cwd.length() > 0) {
      options.cwd = *cwd;
    }

    // options.env
    Local<Value> env_v =
        js_options->Get(context, env->env_pairs_string()).ToLocalChecked();
    if (!env_v.IsEmpty() && env_v->IsArray()) {
      Local<Array> env_opt = Local<Array>::Cast(env_v);
      int envc = env_opt->Length();
      options.env = new char*[envc + 1];  // Heap allocated to detect errors.
      for (int i = 0; i < envc; i++) {
        node::Utf8Value pair(env->isolate(),
                             env_opt->Get(context, i).ToLocalChecked());
        options.env[i] = strdup(*pair);
        CHECK_NE(options.env[i], nullptr);
      }
      options.env[envc] = nullptr;
    }

    // options.stdio
    ParseStdioOptions(env, js_options, &options);

    // options.windowsHide
    Local<Value> hide_v =
        js_options->Get(context, env->windows_hide_string()).ToLocalChecked();

    if (hide_v->IsTrue()) {
      options.flags |= UV_PROCESS_WINDOWS_HIDE;
    }

    // options.windows_verbatim_arguments
    Local<Value> wva_v =
        js_options->Get(context, env->windows_verbatim_arguments_string())
            .ToLocalChecked();

    if (wva_v->IsTrue()) {
      options.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
    }

    // options.detached
    Local<Value> detached_v =
        js_options->Get(context, env->detached_string()).ToLocalChecked();

    if (detached_v->IsTrue()) {
      options.flags |= UV_PROCESS_DETACHED;
    }

    int err = uv_spawn(env->event_loop(), &wrap->process_, &options);

    if (err == 0) {
      CHECK_EQ(wrap->process_.data, wrap);
      wrap->object()->Set(context, env->pid_string(),
                          Integer::New(env->isolate(),
                                       wrap->process_.pid)).FromJust();
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
    Environment* env = Environment::GetCurrent(args);
    ProcessWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
    int signal = args[0]->Int32Value(env->context()).FromJust();
    int err = uv_process_kill(&wrap->process_, signal);
    args.GetReturnValue().Set(err);
  }

  static void OnExit(uv_process_t* handle,
                     int64_t exit_status,
                     int term_signal) {
    ProcessWrap* wrap = static_cast<ProcessWrap*>(handle->data);
    CHECK_NE(wrap, nullptr);
    CHECK_EQ(&wrap->process_, handle);

    Environment* env = wrap->env();
    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());

    Local<Value> argv[] = {
      Number::New(env->isolate(), static_cast<double>(exit_status)),
      OneByteString(env->isolate(), signo_string(term_signal))
    };

    wrap->MakeCallback(env->onexit_string(), arraysize(argv), argv);
  }

  uv_process_t process_;
};


}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(process_wrap, node::ProcessWrap::Initialize)
