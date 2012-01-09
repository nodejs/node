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

#include <node.h>
#include <handle_wrap.h>
#include <pipe_wrap.h>
#include <string.h>
#include <stdlib.h>

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  ProcessWrap* wrap =  \
      static_cast<ProcessWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    uv_err_t err; \
    err.code = UV_EBADF; \
    SetErrno(err); \
    return scope.Close(Integer::New(-1)); \
  }

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Array;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;


class ProcessWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Process"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);

    NODE_SET_PROTOTYPE_METHOD(constructor, "spawn", Spawn);
    NODE_SET_PROTOTYPE_METHOD(constructor, "kill", Kill);

    target->Set(String::NewSymbol("Process"), constructor->GetFunction());
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    ProcessWrap *wrap = new ProcessWrap(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

  ProcessWrap(Handle<Object> object) : HandleWrap(object, NULL) { }
  ~ProcessWrap() { }

  static Handle<Value> Spawn(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    Local<Object> js_options = args[0]->ToObject();

    uv_process_options_t options;
    memset(&options, 0, sizeof(uv_process_options_t));

    options.exit_cb = OnExit;

    // TODO is this possible to do without mallocing ?

    // options.file
    Local<Value> file_v = js_options->Get(String::New("file"));
    if (!file_v.IsEmpty() && file_v->IsString()) {
      String::Utf8Value file(file_v->ToString());
      options.file = strdup(*file);
    }

    // options.args
    Local<Value> argv_v = js_options->Get(String::New("args"));
    if (!argv_v.IsEmpty() && argv_v->IsArray()) {
      Local<Array> js_argv = Local<Array>::Cast(argv_v);
      int argc = js_argv->Length();
      // Heap allocate to detect errors. +1 is for NULL.
      options.args = new char*[argc + 1];
      for (int i = 0; i < argc; i++) {
        String::Utf8Value arg(js_argv->Get(i)->ToString());
        options.args[i] = strdup(*arg);
      }
      options.args[argc] = NULL;
    }

    // options.cwd
    Local<Value> cwd_v = js_options->Get(String::New("cwd"));
    if (!cwd_v.IsEmpty() && cwd_v->IsString()) {
      String::Utf8Value cwd(cwd_v->ToString());
      if (cwd.length() > 0) {
        options.cwd = strdup(*cwd);
      }
    }

    // options.env
    Local<Value> env_v = js_options->Get(String::New("envPairs"));
    if (!env_v.IsEmpty() && env_v->IsArray()) {
      Local<Array> env = Local<Array>::Cast(env_v);
      int envc = env->Length();
      options.env = new char*[envc + 1]; // Heap allocated to detect errors.
      for (int i = 0; i < envc; i++) {
        String::Utf8Value pair(env->Get(i)->ToString());
        options.env[i] = strdup(*pair);
      }
      options.env[envc] = NULL;
    }

    // options.stdin_stream
    Local<Value> stdin_stream_v = js_options->Get(String::New("stdinStream"));
    if (!stdin_stream_v.IsEmpty() && stdin_stream_v->IsObject()) {
      PipeWrap* stdin_wrap = PipeWrap::Unwrap(stdin_stream_v->ToObject());
      options.stdin_stream = stdin_wrap->UVHandle();
    }

    // options.stdout_stream
    Local<Value> stdout_stream_v = js_options->Get(String::New("stdoutStream"));
    if (!stdout_stream_v.IsEmpty() && stdout_stream_v->IsObject()) {
      PipeWrap* stdout_wrap = PipeWrap::Unwrap(stdout_stream_v->ToObject());
      options.stdout_stream = stdout_wrap->UVHandle();
    }

    // options.stderr_stream
    Local<Value> stderr_stream_v = js_options->Get(String::New("stderrStream"));
    if (!stderr_stream_v.IsEmpty() && stderr_stream_v->IsObject()) {
      PipeWrap* stderr_wrap = PipeWrap::Unwrap(stderr_stream_v->ToObject());
      options.stderr_stream = stderr_wrap->UVHandle();
    }

    // options.windows_verbatim_arguments
#if defined(_WIN32)
    options.windows_verbatim_arguments = js_options->
        Get(String::NewSymbol("windowsVerbatimArguments"))->IsTrue();
#endif

    int r = uv_spawn(uv_default_loop(), &wrap->process_, options);

    if (r) {
      SetErrno(uv_last_error(uv_default_loop()));
    }
    else {
      wrap->SetHandle((uv_handle_t*)&wrap->process_);
      assert(wrap->process_.data == wrap);
      wrap->object_->Set(String::New("pid"), Integer::New(wrap->process_.pid));
    }

    if (options.args) {
      for (int i = 0; options.args[i]; i++) free(options.args[i]);
      delete [] options.args;
    }

    free(options.cwd);
    free((void*)options.file);

    if (options.env) {
      for (int i = 0; options.env[i]; i++) free(options.env[i]);
      delete [] options.env;
    }

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Kill(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int signal = args[0]->Int32Value();

    int r = uv_process_kill(&wrap->process_, signal);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static void OnExit(uv_process_t* handle, int exit_status, int term_signal) {
    HandleScope scope;

    ProcessWrap* wrap = static_cast<ProcessWrap*>(handle->data);
    assert(wrap);
    assert(&wrap->process_ == handle);

    Local<Value> argv[2] = {
      Integer::New(exit_status),
      String::New(signo_string(term_signal))
    };

    MakeCallback(wrap->object_, "onexit", 2, argv);
  }

  uv_process_t process_;
};


}  // namespace node

NODE_MODULE(node_process_wrap, node::ProcessWrap::Initialize)
