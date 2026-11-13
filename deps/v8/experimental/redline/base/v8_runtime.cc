// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/v8_runtime.h"

#include <unistd.h>

#include <mutex>

#include "experimental/redline/base/log.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-message.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"

namespace redline {

namespace {

constexpr const char* kLog = "v8";

// V8 uses the platform until the process exits, so it is never deleted.
v8::Platform* g_platform = nullptr;
bool g_initialized = false;
std::mutex g_init_mutex;

std::string ExecutablePath() {
  char buffer[4096];
  ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (n <= 0) {
    return "redline";
  }
  buffer[n] = '\0';
  return buffer;
}

std::string ToStdString(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty()) {
    return "";
  }
  v8::String::Utf8Value utf8(isolate, value);
  return *utf8 == nullptr ? "" : std::string(*utf8, utf8.length());
}

}  // namespace

bool InitializeV8(const std::string& flags, std::string* error) {
  std::scoped_lock<std::mutex> lock(g_init_mutex);
  if (g_initialized) {
    return true;
  }
  std::string exe = ExecutablePath();
  // ICU data and the startup snapshot live next to the executable (as for
  // d8). Failing to find ICU is not fatal for parsing.
  if (!v8::V8::InitializeICUDefaultLocation(exe.c_str())) {
    REDLINE_VLOG(kLog, "ICU data not found next to %s; continuing without it",
                 exe.c_str());
  }
  v8::V8::InitializeExternalStartupData(exe.c_str());
  if (!flags.empty()) {
    v8::V8::SetFlagsFromString(flags.c_str());
  }
  // The oracle only parses; cached compiled scripts would just retain memory
  // (one isolate per worker, all in the same pointer-compression cage).
  v8::V8::SetFlagsFromString("--no-compilation-cache");
  g_platform =
      v8::platform::NewDefaultPlatform(
          /*thread_pool_size=*/2, v8::platform::IdleTaskSupport::kDisabled)
          .release();
  v8::V8::InitializePlatform(g_platform);
  if (!v8::V8::Initialize()) {
    *error = "v8::V8::Initialize() failed";
    return false;
  }
  g_initialized = true;
  REDLINE_VLOG(kLog, "V8 %s initialized in-process (flags: %s)",
               v8::V8::GetVersion(), flags.c_str());
  return true;
}

struct SyntaxChecker::Impl {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator;
  v8::Isolate* isolate = nullptr;
  v8::Global<v8::Context> context;
  uint64_t checks = 0;
};

SyntaxChecker::SyntaxChecker() : impl_(std::make_unique<Impl>()) {
  impl_->allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = impl_->allocator.get();
  // Every worker thread owns one of these isolates and all of them live in
  // the process' single pointer-compression cage (4 GB), so each must stay
  // small: a lazily compiled script of a few hundred KB needs a few MB.
  constexpr size_t kMB = static_cast<size_t>(1024) * 1024;
  params.constraints.set_max_old_generation_size_in_bytes(64 * kMB);
  params.constraints.set_max_young_generation_size_in_bytes(8 * kMB);
  impl_->isolate = v8::Isolate::New(params);
  v8::Isolate::Scope isolate_scope(impl_->isolate);
  v8::HandleScope handle_scope(impl_->isolate);
  v8::Local<v8::Context> context = v8::Context::New(impl_->isolate);
  impl_->context.Reset(impl_->isolate, context);
}

SyntaxChecker::~SyntaxChecker() {
  impl_->context.Reset();
  impl_->isolate->Dispose();
}

bool SyntaxChecker::Accepts(std::string_view source, std::string* message) {
  v8::Isolate* isolate = impl_->isolate;
  v8::Isolate::Scope isolate_scope(isolate);
  // Keep the heap compact: everything from previous checks is garbage.
  if (++impl_->checks % 4096 == 0) {
    isolate->LowMemoryNotification();
  }
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = impl_->context.Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(false);

  v8::Local<v8::String> text;
  if (!v8::String::NewFromUtf8(isolate, source.data(),
                               v8::NewStringType::kNormal,
                               static_cast<int>(source.size()))
           .ToLocal(&text)) {
    if (message) {
      *message = "source is too large or not valid UTF-8";
    }
    return false;
  }
  v8::ScriptOrigin origin(
      v8::String::NewFromUtf8Literal(isolate, "redline-syntax-check.js"));
  v8::ScriptCompiler::Source compiler_source(text, origin);
  v8::Local<v8::UnboundScript> script;
  // Lazy compilation, like d8: inner functions are pre-parsed, which reports
  // every early error, and nothing but the top-level bytecode is generated.
  // (Eager compilation retained the bytecode of every function of every
  // checked program until the next GC; with one isolate per worker that
  // exhausted the pointer-compression cage on big inputs.)
  if (v8::ScriptCompiler::CompileUnboundScript(
          isolate, &compiler_source, v8::ScriptCompiler::kNoCompileOptions)
          .ToLocal(&script)) {
    return true;
  }
  if (message) {
    std::string text_message = "SyntaxError";
    if (try_catch.HasCaught()) {
      v8::Local<v8::Message> v8_message = try_catch.Message();
      text_message = ToStdString(isolate, try_catch.Exception());
      if (!v8_message.IsEmpty()) {
        int line = v8_message->GetLineNumber(context).FromMaybe(0);
        int column = v8_message->GetStartColumn(context).FromMaybe(-1) + 1;
        text_message =
            StringPrintf("%d:%d: %s", line, column, text_message.c_str());
      }
    }
    *message = text_message;
  }
  return false;
}

}  // namespace redline
