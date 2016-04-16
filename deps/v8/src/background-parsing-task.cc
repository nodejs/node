// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/background-parsing-task.h"
#include "src/debug/debug.h"

namespace v8 {
namespace internal {

BackgroundParsingTask::BackgroundParsingTask(
    StreamedSource* source, ScriptCompiler::CompileOptions options,
    int stack_size, Isolate* isolate)
    : source_(source), stack_size_(stack_size) {
  // We don't set the context to the CompilationInfo yet, because the background
  // thread cannot do anything with it anyway. We set it just before compilation
  // on the foreground thread.
  DCHECK(options == ScriptCompiler::kProduceParserCache ||
         options == ScriptCompiler::kProduceCodeCache ||
         options == ScriptCompiler::kNoCompileOptions);

  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.
  Zone* zone = new Zone();
  ParseInfo* info = new ParseInfo(zone);
  source->zone.Reset(zone);
  source->info.Reset(info);
  info->set_isolate(isolate);
  info->set_source_stream(source->source_stream.get());
  info->set_source_stream_encoding(source->encoding);
  info->set_hash_seed(isolate->heap()->HashSeed());
  info->set_global();
  info->set_unicode_cache(&source_->unicode_cache);
  info->set_compile_options(options);
  info->set_allow_lazy_parsing(true);
}


void BackgroundParsingTask::Run() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  ScriptData* script_data = NULL;
  ScriptCompiler::CompileOptions options = source_->info->compile_options();
  if (options == ScriptCompiler::kProduceParserCache ||
      options == ScriptCompiler::kProduceCodeCache) {
    source_->info->set_cached_data(&script_data);
  }

  uintptr_t stack_limit =
      reinterpret_cast<uintptr_t>(&stack_limit) - stack_size_ * KB;

  source_->info->set_stack_limit(stack_limit);
  // Parser needs to stay alive for finalizing the parsing on the main
  // thread. Passing &parse_info is OK because Parser doesn't store it.
  source_->parser.Reset(new Parser(source_->info.get()));
  source_->parser->ParseOnBackground(source_->info.get());

  if (script_data != NULL) {
    source_->cached_data.Reset(new ScriptCompiler::CachedData(
        script_data->data(), script_data->length(),
        ScriptCompiler::CachedData::BufferOwned));
    script_data->ReleaseDataOwnership();
    delete script_data;
  }
}
}  // namespace internal
}  // namespace v8
