// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "include/v8-value.h"
#include "src/api/api-inl.h"
#include "src/base/logging.h"
#include "src/objects/js-function.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"

namespace i = v8::internal;

namespace {

constexpr i::Tagged_t kBogusTaggedValue =
    static_cast<i::Tagged_t>(0x5a5a5a5b5a5a5a5bULL);

void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  v8::Local<v8::Message> message = try_catch->Message();
  v8::String::Utf8Value message_str(isolate, message->Get());
  std::cerr << *message_str << std::endl;
  message->PrintCurrentStackTrace(isolate, std::cerr);
  std::cerr << std::flush;
}

bool ReadFile(const char* path, std::string* contents) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) return false;

  *contents = std::string(std::istreambuf_iterator<char>(input),
                          std::istreambuf_iterator<char>());
  return true;
}

void CorruptScriptSourceAndAbort(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info[0]->IsFunction()) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8Literal(
        info.GetIsolate(), "The first argument must be a function"));
    return;
  }
  v8::Local<v8::Function> func_arg = info[0].As<v8::Function>();
  auto target_function =
      i::Cast<i::JSFunction>(*v8::Utils::OpenDirectHandle(*func_arg));
  i::Tagged<i::SharedFunctionInfo> shared = target_function->shared();
  CHECK(i::IsScript(shared->script()));
  auto script = i::Cast<i::Script>(shared->script());
  script->Relaxed_WriteField<i::Tagged_t>(offsetof(i::Script, source_),
                                          kBogusTaggedValue);
  abort();
}

void CorruptSFIAndAbort(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info[0]->IsFunction()) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8Literal(
        info.GetIsolate(), "The first argument must be a function"));
    return;
  }
  v8::Local<v8::Function> func_arg = info[0].As<v8::Function>();
  auto target_function =
      i::Cast<i::JSFunction>(*v8::Utils::OpenDirectHandle(*func_arg));
  target_function->Relaxed_WriteField<i::Tagged_t>(
      offsetof(i::JSFunction, shared_function_info_), kBogusTaggedValue);
  abort();
}

void InstallFunction(v8::Isolate* isolate, v8::Local<v8::Context> context,
                     const char* name, v8::FunctionCallback callback) {
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  v8::Local<v8::String> function_name =
      v8::String::NewFromUtf8(isolate, name, v8::NewStringType::kNormal)
          .ToLocalChecked();
  context->Global()->Set(context, function_name, function).Check();
}

void InstallHelpers(v8::Isolate* isolate, v8::Local<v8::Context> context) {
  InstallFunction(isolate, context, "corruptScriptSourceAndAbort",
                  CorruptScriptSourceAndAbort);
  InstallFunction(isolate, context, "corruptSFIAndAbort", CorruptSFIAndAbort);
}

bool RunScript(v8::Isolate* isolate, v8::Local<v8::Context> context,
               const char* script_path) {
  std::string source_text;
  if (!ReadFile(script_path, &source_text)) {
    fprintf(stderr, "corruption_harness: failed to read script file\n");
    return false;
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, source_text.c_str(),
                              v8::NewStringType::kNormal,
                              static_cast<int>(source_text.size()))
          .ToLocalChecked();

  v8::Local<v8::String> resource_name =
      v8::String::NewFromUtf8(isolate, script_path, v8::NewStringType::kNormal)
          .ToLocalChecked();

  v8::ScriptOrigin origin(resource_name);
  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
    ReportException(isolate, &try_catch);
    return false;
  }

  v8::Local<v8::Value> result;
  if (!script->Run(context).ToLocal(&result)) {
    ReportException(isolate, &try_catch);
    return false;
  }
  return true;
}

struct Options {
  const char* script_path = nullptr;
};

}  // namespace

int main(int argc, char* argv[]) {
  Options options;
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <script>\n", argv[0]);
    return 1;
  }
  options.script_path = argv[1];

  if (!v8::V8::InitializeICUDefaultLocation(argv[0])) {
    fprintf(stderr, "Failed to initialize ICU\n");
    return 1;
  }
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  int exit_code = 0;
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    InstallHelpers(isolate, context);
    if (!RunScript(isolate, context, options.script_path)) {
      exit_code = 1;
    }
  }

  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
  return exit_code;
}
