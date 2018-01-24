// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/background-parsing-task.h"

#include "src/counters.h"
#include "src/objects-inl.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

void StreamedSource::Release() {
  parser.reset();
  info.reset();
}

BackgroundParsingTask::BackgroundParsingTask(
    StreamedSource* source, ScriptCompiler::CompileOptions options,
    int stack_size, Isolate* isolate)
    : source_(source),
      stack_size_(stack_size),
      script_data_(nullptr),
      timer_(isolate->counters()->compile_script_on_background()) {
  // We don't set the context to the CompilationInfo yet, because the background
  // thread cannot do anything with it anyway. We set it just before compilation
  // on the foreground thread.
  DCHECK(options == ScriptCompiler::kProduceParserCache ||
         options == ScriptCompiler::kProduceCodeCache ||
         options == ScriptCompiler::kProduceFullCodeCache ||
         options == ScriptCompiler::kNoCompileOptions);

  VMState<PARSER> state(isolate);

  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.
  ParseInfo* info = new ParseInfo(isolate->allocator());
  info->InitFromIsolate(isolate);
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    info->set_runtime_call_stats(new (info->zone()) RuntimeCallStats());
  } else {
    info->set_runtime_call_stats(nullptr);
  }
  info->set_toplevel();
  std::unique_ptr<Utf16CharacterStream> stream(
      ScannerStream::For(source->source_stream.get(), source->encoding,
                         info->runtime_call_stats()));
  info->set_character_stream(std::move(stream));
  info->set_unicode_cache(&source_->unicode_cache);
  info->set_compile_options(options);
  info->set_allow_lazy_parsing();
  if (V8_UNLIKELY(info->block_coverage_enabled())) {
    info->AllocateSourceRangeMap();
  }
  info->set_cached_data(&script_data_);
  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
  info->set_language_mode(
      stricter_language_mode(info->language_mode(), language_mode));

  source->info.reset(info);
  allocator_ = isolate->allocator();

  // Parser needs to stay alive for finalizing the parsing on the main
  // thread.
  source_->parser.reset(new Parser(source_->info.get()));
  source_->parser->DeserializeScopeChain(source_->info.get(),
                                         MaybeHandle<ScopeInfo>());
}

void BackgroundParsingTask::Run() {
  TimedHistogramScope timer(timer_);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  source_->info->set_on_background_thread(true);

  // Reset the stack limit of the parser to reflect correctly that we're on a
  // background thread.
  uintptr_t old_stack_limit = source_->info->stack_limit();
  uintptr_t stack_limit = GetCurrentStackPosition() - stack_size_ * KB;
  source_->info->set_stack_limit(stack_limit);
  source_->parser->set_stack_limit(stack_limit);

  source_->parser->ParseOnBackground(source_->info.get());
  if (FLAG_background_compile && source_->info->literal() != nullptr) {
    // Parsing has succeeded, compile.
    source_->outer_function_job = Compiler::CompileTopLevelOnBackgroundThread(
        source_->info.get(), allocator_, &source_->inner_function_jobs);
  }

  if (script_data_ != nullptr) {
    source_->cached_data.reset(new ScriptCompiler::CachedData(
        script_data_->data(), script_data_->length(),
        ScriptCompiler::CachedData::BufferOwned));
    script_data_->ReleaseDataOwnership();
    delete script_data_;
    script_data_ = nullptr;
  }

  source_->info->EmitBackgroundParseStatisticsOnBackgroundThread();

  source_->info->set_on_background_thread(false);
  source_->info->set_stack_limit(old_stack_limit);
}

}  // namespace internal
}  // namespace v8
