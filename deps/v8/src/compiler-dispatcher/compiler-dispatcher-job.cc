// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-job.h"

#include "src/assert-scope.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/global-handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/unicode-cache.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

CompilerDispatcherJob::CompilerDispatcherJob(Isolate* isolate,
                                             Handle<SharedFunctionInfo> shared,
                                             size_t max_stack_size)
    : isolate_(isolate),
      shared_(Handle<SharedFunctionInfo>::cast(
          isolate_->global_handles()->Create(*shared))),
      max_stack_size_(max_stack_size),
      can_compile_on_background_thread_(false) {
  HandleScope scope(isolate_);
  DCHECK(!shared_->outer_scope_info()->IsTheHole(isolate_));
  Handle<Script> script(Script::cast(shared_->script()), isolate_);
  Handle<String> source(String::cast(script->source()), isolate_);
  can_parse_on_background_thread_ =
      source->IsExternalTwoByteString() || source->IsExternalOneByteString();
}

CompilerDispatcherJob::~CompilerDispatcherJob() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status_ == CompileJobStatus::kInitial ||
         status_ == CompileJobStatus::kDone);
  i::GlobalHandles::Destroy(Handle<Object>::cast(shared_).location());
}

void CompilerDispatcherJob::PrepareToParseOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kInitial);
  HandleScope scope(isolate_);
  unicode_cache_.reset(new UnicodeCache());
  zone_.reset(new Zone(isolate_->allocator()));
  Handle<Script> script(Script::cast(shared_->script()), isolate_);
  DCHECK(script->type() != Script::TYPE_NATIVE);

  Handle<String> source(String::cast(script->source()), isolate_);
  if (source->IsExternalTwoByteString() || source->IsExternalOneByteString()) {
    character_stream_.reset(ScannerStream::For(
        source, shared_->start_position(), shared_->end_position()));
  } else {
    source = String::Flatten(source);
    // Have to globalize the reference here, so it survives between function
    // calls.
    source_ = Handle<String>::cast(isolate_->global_handles()->Create(*source));
    character_stream_.reset(ScannerStream::For(
        source_, shared_->start_position(), shared_->end_position()));
  }
  parse_info_.reset(new ParseInfo(zone_.get()));
  parse_info_->set_isolate(isolate_);
  parse_info_->set_character_stream(character_stream_.get());
  parse_info_->set_lazy();
  parse_info_->set_hash_seed(isolate_->heap()->HashSeed());
  parse_info_->set_is_named_expression(shared_->is_named_expression());
  parse_info_->set_compiler_hints(shared_->compiler_hints());
  parse_info_->set_start_position(shared_->start_position());
  parse_info_->set_end_position(shared_->end_position());
  parse_info_->set_unicode_cache(unicode_cache_.get());
  parse_info_->set_language_mode(shared_->language_mode());

  parser_.reset(new Parser(parse_info_.get()));
  Handle<ScopeInfo> outer_scope_info(
      handle(ScopeInfo::cast(shared_->outer_scope_info())));
  parser_->DeserializeScopeChain(parse_info_.get(),
                                 outer_scope_info->length() > 0
                                     ? MaybeHandle<ScopeInfo>(outer_scope_info)
                                     : MaybeHandle<ScopeInfo>());

  Handle<String> name(String::cast(shared_->name()));
  parse_info_->set_function_name(
      parse_info_->ast_value_factory()->GetString(name));
  status_ = CompileJobStatus::kReadyToParse;
}

void CompilerDispatcherJob::Parse() {
  DCHECK(can_parse_on_background_thread_ ||
         ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kReadyToParse);

  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  std::unique_ptr<DisallowHandleDereference> no_deref;
  // If we can't parse on a background thread, we need to be able to deref the
  // source string.
  if (can_parse_on_background_thread_) {
    no_deref.reset(new DisallowHandleDereference());
  }

  // Nullify the Isolate temporarily so that the parser doesn't accidentally
  // use it.
  parse_info_->set_isolate(nullptr);

  uintptr_t stack_limit = GetCurrentStackPosition() - max_stack_size_ * KB;

  parser_->set_stack_limit(stack_limit);
  parser_->ParseOnBackground(parse_info_.get());

  parse_info_->set_isolate(isolate_);

  status_ = CompileJobStatus::kParsed;
}

bool CompilerDispatcherJob::FinalizeParsingOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kParsed);

  if (!source_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }

  if (parse_info_->literal() == nullptr) {
    status_ = CompileJobStatus::kFailed;
  } else {
    status_ = CompileJobStatus::kReadyToAnalyse;
  }

  DeferredHandleScope scope(isolate_);
  {
    Handle<Script> script(Script::cast(shared_->script()), isolate_);

    parse_info_->set_script(script);
    Handle<ScopeInfo> outer_scope_info(
        handle(ScopeInfo::cast(shared_->outer_scope_info())));
    if (outer_scope_info->length() > 0) {
      parse_info_->set_outer_scope_info(outer_scope_info);
    }
    parse_info_->set_shared_info(shared_);

    {
      // Create a canonical handle scope if compiling ignition bytecode. This is
      // required by the constant array builder to de-duplicate objects without
      // dereferencing handles.
      std::unique_ptr<CanonicalHandleScope> canonical;
      if (FLAG_ignition) canonical.reset(new CanonicalHandleScope(isolate_));

      // Do the parsing tasks which need to be done on the main thread. This
      // will also handle parse errors.
      parser_->Internalize(isolate_, script, parse_info_->literal() == nullptr);
    }
    parser_->HandleSourceURLComments(isolate_, script);

    parse_info_->set_character_stream(nullptr);
    parse_info_->set_unicode_cache(nullptr);
    parser_.reset();
    unicode_cache_.reset();
    character_stream_.reset();
  }
  handles_from_parsing_.reset(scope.Detach());

  return status_ != CompileJobStatus::kFailed;
}

bool CompilerDispatcherJob::PrepareToCompileOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kReadyToAnalyse);

  compile_info_.reset(
      new CompilationInfo(parse_info_.get(), Handle<JSFunction>::null()));

  DeferredHandleScope scope(isolate_);
  if (Compiler::Analyze(parse_info_.get())) {
    compile_job_.reset(
        Compiler::PrepareUnoptimizedCompilationJob(compile_info_.get()));
  }
  compile_info_->set_deferred_handles(scope.Detach());

  if (!compile_job_.get()) {
    if (!isolate_->has_pending_exception()) isolate_->StackOverflow();
    status_ = CompileJobStatus::kFailed;
    return false;
  }

  can_compile_on_background_thread_ =
      compile_job_->can_execute_on_background_thread();
  status_ = CompileJobStatus::kReadyToCompile;
  return true;
}

void CompilerDispatcherJob::Compile() {
  DCHECK(status() == CompileJobStatus::kReadyToCompile);
  DCHECK(can_compile_on_background_thread_ ||
         ThreadId::Current().Equals(isolate_->thread_id()));

  // Disallowing of handle dereference and heap access dealt with in
  // CompilationJob::ExecuteJob.

  uintptr_t stack_limit = GetCurrentStackPosition() - max_stack_size_ * KB;
  compile_job_->set_stack_limit(stack_limit);

  CompilationJob::Status status = compile_job_->ExecuteJob();
  USE(status);

  // Always transition to kCompiled - errors will be reported by
  // FinalizeCompilingOnMainThread.
  status_ = CompileJobStatus::kCompiled;
}

bool CompilerDispatcherJob::FinalizeCompilingOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kCompiled);

  if (compile_job_->state() == CompilationJob::State::kFailed ||
      !Compiler::FinalizeCompilationJob(compile_job_.release())) {
    if (!isolate_->has_pending_exception()) isolate_->StackOverflow();
    status_ = CompileJobStatus::kFailed;
    return false;
  }

  zone_.reset();
  parse_info_.reset();
  compile_info_.reset();
  compile_job_.reset();
  handles_from_parsing_.reset();

  status_ = CompileJobStatus::kDone;
  return true;
}

void CompilerDispatcherJob::ResetOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));

  parser_.reset();
  unicode_cache_.reset();
  character_stream_.reset();
  parse_info_.reset();
  zone_.reset();
  handles_from_parsing_.reset();
  compile_info_.reset();
  compile_job_.reset();

  if (!source_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }

  status_ = CompileJobStatus::kInitial;
}

}  // namespace internal
}  // namespace v8
