// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Tests of profiles generator and utilities.

#include "src/base/logging.h"
#include "test/cctest/profiler-extension.h"

namespace v8 {
namespace internal {


v8::CpuProfile* ProfilerExtension::last_profile = NULL;
const char* ProfilerExtension::kSource =
    "native function startProfiling();"
    "native function stopProfiling();";

v8::Handle<v8::FunctionTemplate> ProfilerExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::NewFromUtf8(isolate, "startProfiling"))) {
    return v8::FunctionTemplate::New(isolate,
                                     ProfilerExtension::StartProfiling);
  } else if (name->Equals(v8::String::NewFromUtf8(isolate, "stopProfiling"))) {
    return v8::FunctionTemplate::New(isolate,
                                     ProfilerExtension::StopProfiling);
  } else {
    CHECK(false);
    return v8::Handle<v8::FunctionTemplate>();
  }
}


void ProfilerExtension::StartProfiling(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  last_profile = NULL;
  v8::CpuProfiler* cpu_profiler = args.GetIsolate()->GetCpuProfiler();
  cpu_profiler->StartProfiling((args.Length() > 0)
      ? args[0].As<v8::String>()
      : v8::String::Empty(args.GetIsolate()));
}


void ProfilerExtension::StopProfiling(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::CpuProfiler* cpu_profiler = args.GetIsolate()->GetCpuProfiler();
  last_profile = cpu_profiler->StopProfiling((args.Length() > 0)
      ? args[0].As<v8::String>()
      : v8::String::Empty(args.GetIsolate()));
}

} }  // namespace v8::internal
