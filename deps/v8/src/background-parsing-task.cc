// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/background-parsing-task.h"

namespace v8 {
namespace internal {

BackgroundParsingTask::BackgroundParsingTask(
    StreamedSource* source, ScriptCompiler::CompileOptions options,
    int stack_size, Isolate* isolate)
    : source_(source), options_(options), stack_size_(stack_size) {
  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.
  source->info.Reset(new i::CompilationInfoWithZone(source->source_stream.get(),
                                                    source->encoding, isolate));
  source->info->MarkAsGlobal();

  // We don't set the context to the CompilationInfo yet, because the background
  // thread cannot do anything with it anyway. We set it just before compilation
  // on the foreground thread.
  DCHECK(options == ScriptCompiler::kProduceParserCache ||
         options == ScriptCompiler::kProduceCodeCache ||
         options == ScriptCompiler::kNoCompileOptions);
  source->allow_lazy =
      !i::Compiler::DebuggerWantsEagerCompilation(source->info.get());
  source->hash_seed = isolate->heap()->HashSeed();
}


void BackgroundParsingTask::Run() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  ScriptData* script_data = NULL;
  if (options_ == ScriptCompiler::kProduceParserCache ||
      options_ == ScriptCompiler::kProduceCodeCache) {
    source_->info->SetCachedData(&script_data, options_);
  }

  uintptr_t limit = reinterpret_cast<uintptr_t>(&limit) - stack_size_ * KB;
  Parser::ParseInfo parse_info = {limit, source_->hash_seed,
                                  &source_->unicode_cache};

  // Parser needs to stay alive for finalizing the parsing on the main
  // thread. Passing &parse_info is OK because Parser doesn't store it.
  source_->parser.Reset(new Parser(source_->info.get(), &parse_info));
  source_->parser->set_allow_lazy(source_->allow_lazy);
  source_->parser->ParseOnBackground();

  if (script_data != NULL) {
    source_->cached_data.Reset(new ScriptCompiler::CachedData(
        script_data->data(), script_data->length(),
        ScriptCompiler::CachedData::BufferOwned));
    script_data->ReleaseDataOwnership();
    delete script_data;
  }
}
}
}  // namespace v8::internal
