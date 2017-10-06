// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/background-parsing-task.h"

#include "src/objects-inl.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

void StreamedSource::Release() {
  parser.reset();
  info.reset();
}

BackgroundParsingTask::BackgroundParsingTask(
    StreamedSource* source, ScriptCompiler::CompileOptions options,
    int stack_size, Isolate* isolate)
    : source_(source), stack_size_(stack_size), script_data_(nullptr) {
  // We don't set the context to the CompilationInfo yet, because the background
  // thread cannot do anything with it anyway. We set it just before compilation
  // on the foreground thread.
  DCHECK(options == ScriptCompiler::kProduceParserCache ||
         options == ScriptCompiler::kProduceCodeCache ||
         options == ScriptCompiler::kNoCompileOptions);

  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.
  ParseInfo* info = new ParseInfo(isolate->allocator());
  info->InitFromIsolate(isolate);
  info->set_toplevel();
  source->info.reset(info);
  info->set_source_stream(source->source_stream.get());
  info->set_source_stream_encoding(source->encoding);
  info->set_unicode_cache(&source_->unicode_cache);
  info->set_compile_options(options);
  info->set_allow_lazy_parsing();
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    info->set_runtime_call_stats(new (info->zone()) RuntimeCallStats());
  }

  source_->info->set_cached_data(&script_data_);
  // Parser needs to stay alive for finalizing the parsing on the main
  // thread.
  source_->parser.reset(new Parser(source_->info.get()));
  source_->parser->DeserializeScopeChain(source_->info.get(),
                                         MaybeHandle<ScopeInfo>());
}


void BackgroundParsingTask::Run() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  // Reset the stack limit of the parser to reflect correctly that we're on a
  // background thread.
  uintptr_t stack_limit = GetCurrentStackPosition() - stack_size_ * KB;
  source_->parser->set_stack_limit(stack_limit);

  source_->parser->ParseOnBackground(source_->info.get());

  if (script_data_ != nullptr) {
    source_->cached_data.reset(new ScriptCompiler::CachedData(
        script_data_->data(), script_data_->length(),
        ScriptCompiler::CachedData::BufferOwned));
    script_data_->ReleaseDataOwnership();
    delete script_data_;
    script_data_ = nullptr;
  }
}
}  // namespace internal
}  // namespace v8
