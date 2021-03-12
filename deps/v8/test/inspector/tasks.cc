// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/tasks.h"

#include <vector>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "test/inspector/isolate-data.h"
#include "test/inspector/utils.h"

namespace v8 {
namespace internal {

void ExecuteStringTask::Run(IsolateData* data) {
  v8::MicrotasksScope microtasks_scope(data->isolate(),
                                       v8::MicrotasksScope::kRunMicrotasks);
  v8::HandleScope handle_scope(data->isolate());
  v8::Local<v8::Context> context = data->GetDefaultContext(context_group_id_);
  v8::Context::Scope context_scope(context);
  v8::ScriptOrigin origin(data->isolate(), ToV8String(data->isolate(), name_),
                          line_offset_, column_offset_,
                          /* resource_is_shared_cross_origin */ false,
                          /* script_id */ -1,
                          /* source_map_url */ v8::Local<v8::Value>(),
                          /* resource_is_opaque */ false,
                          /* is_wasm */ false, is_module_);
  v8::Local<v8::String> source;
  if (expression_.size() != 0)
    source = ToV8String(data->isolate(), expression_);
  else
    source = ToV8String(data->isolate(), expression_utf8_);

  v8::ScriptCompiler::Source scriptSource(source, origin);
  v8::Isolate::SafeForTerminationScope allowTermination(data->isolate());
  if (!is_module_) {
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &scriptSource).ToLocal(&script))
      return;
    v8::MaybeLocal<v8::Value> result;
    result = script->Run(context);
  } else {
    data->RegisterModule(context, name_, &scriptSource);
  }
}

}  // namespace internal
}  // namespace v8
