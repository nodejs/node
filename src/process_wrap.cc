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

#include "node.h"
#include "handle_wrap.h"
#include "node_wrap.h"

#include <string.h>
#include <stdlib.h>

namespace node {

using v8::Array;
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

static Cached<String> onexit_sym;

class ProcessWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope(node_isolate);

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Process"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);

    NODE_SET_PROTOTYPE_METHOD(constructor, "spawn", Spawn);
    NODE_SET_PROTOTYPE_METHOD(constructor, "kill", Kill);

    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);

    target->Set(String::NewSymbol("Process"), constructor->GetFunction());
  }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());
    HandleScope scope(node_isolate);
    new ProcessWrap(args.This());
  }

  explicit ProcessWrap(Handle<Object> object)
      : HandleWrap(object, reinterpret_cast<uv_handle_t*>(&process_)) {
  }

  ~ProcessWrap() {
  }

  static void ParseStdioOptions(Local<Object> js_options,
                                uv_process_options_t* options) {
    Local<Array> stdios = js_options
        ->Get(String::NewSymbol("stdio")).As<Array>();
    int len = stdios->Length();
    options->stdio = new uv_stdio_container_t[len];
    options->stdio_count = len;

    for (int i = 0; i < len; i++) {
      Local<Object> stdio = stdios
          ->Get(Number::New(static_cast<double>(i))).As<Object>();
      Local<Value> type = stdio->Get(String::NewSymbol("type"));

      if (type->Equals(String::NewSymbol("ignore"))) {
        options->stdio[i].flags = UV_IGNORE;
      } else if (type->Equals(String::NewSymbol("pipe"))) {
        options->stdio[i].flags = static_cast<uv_stdio_flags>(
            UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE);
        options->stdio[i].data.stream = reinterpret_cast<uv_stream_t*>(
            PipeWrap::Unwrap(stdio
                ->Get(String::NewSymbol("handle")).As<Object>())->UVHandle());
      } else if (type->Equals(String::NewSymbol("wrap"))) {
        uv_stream_t* stream = HandleToStream(
            stdio->Get(String::NewSymbol("handle")).As<Object>());
        assert(stream != NULL);

        options->stdio[i].flags = UV_INHERIT_STREAM;
        options->stdio[i].data.stream = stream;
      } else {
        int fd = static_cast<int>(
            stdio->Get(String::NewSymbol("fd"))->IntegerValue());

        options->stdio[i].flags = UV_INHERIT_FD;
        options->stdio[i].data.fd = fd;
      }
    }
  }

  static void Spawn(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    UNWRAP(ProcessWrap)

    Local<Object> js_options = args[0]->ToObject();

    uv_process_options_t options;
    memset(&options, 0, sizeof(uv_process_options_t));

    options.exit_cb = OnExit;

    // options.uid
    Local<Value> uid_v = js_options->Get(String::NewSymbol("uid"));
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
    Local<Value> gid_v = js_options->Get(String::NewSymbol("gid"));
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
    Local<Value> file_v = js_options->Get(String::NewSymbol("file"));
    String::Utf8Value file(file_v->IsString() ? file_v : Local<Value>());
    if (file.length() > 0) {
      options.file = *file;
    } else {
      return ThrowTypeError("Bad argument");
    }

    // options.args
    Local<Value> argv_v = js_options->Get(String::NewSymbol("args"));
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
    Local<Value> cwd_v = js_options->Get(String::NewSymbol("cwd"));
    String::Utf8Value cwd(cwd_v->IsString() ? cwd_v : Local<Value>());
    if (cwd.length() > 0) {
      options.cwd = *cwd;
    }

    // options.env
    Local<Value> env_v = js_options->Get(String::NewSymbol("envPairs"));
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
    ParseStdioOptions(js_options, &options);

    // options.windows_verbatim_arguments
    if (js_options->Get(String::NewSymbol("windowsVerbatimArguments"))->
          IsTrue()) {
      options.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
    }

    // options.detached
    if (js_options->Get(String::NewSymbol("detached"))->IsTrue()) {
      options.flags |= UV_PROCESS_DETACHED;
    }

    int err = uv_spawn(uv_default_loop(), &wrap->process_, options);

    if (err == 0) {
      assert(wrap->process_.data == wrap);
      wrap->object()->Set(String::New("pid"),
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
    UNWRAP(ProcessWrap)

    int signal = args[0]->Int32Value();
    int err = uv_process_kill(&wrap->process_, signal);
    args.GetReturnValue().Set(err);
  }

  static void OnExit(uv_process_t* handle, int exit_status, int term_signal) {
    HandleScope scope(node_isolate);

    ProcessWrap* wrap = static_cast<ProcessWrap*>(handle->data);
    assert(wrap);
    assert(&wrap->process_ == handle);

    Local<Value> argv[] = {
      Integer::New(exit_status, node_isolate),
      String::New(signo_string(term_signal))
    };

    if (onexit_sym.IsEmpty()) {
      onexit_sym = String::New("onexit");
    }

    MakeCallback(wrap->object(), onexit_sym, ARRAY_SIZE(argv), argv);
  }

  uv_process_t process_;
};


}  // namespace node

NODE_MODULE(node_process_wrap, node::ProcessWrap::Initialize)
