// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-job.h"

#include "src/assert-scope.h"
#include "src/global-handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/unicode-cache.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

CompilerDispatcherJob::CompilerDispatcherJob(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             size_t max_stack_size)
    : isolate_(isolate),
      function_(Handle<JSFunction>::cast(
          isolate_->global_handles()->Create(*function))),
      max_stack_size_(max_stack_size) {
  HandleScope scope(isolate_);
  Handle<SharedFunctionInfo> shared(function_->shared(), isolate_);
  Handle<Script> script(Script::cast(shared->script()), isolate_);
  Handle<String> source(String::cast(script->source()), isolate_);
  can_parse_on_background_thread_ =
      source->IsExternalTwoByteString() || source->IsExternalOneByteString();
}

CompilerDispatcherJob::~CompilerDispatcherJob() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status_ == CompileJobStatus::kInitial ||
         status_ == CompileJobStatus::kDone);
  i::GlobalHandles::Destroy(Handle<Object>::cast(function_).location());
}

void CompilerDispatcherJob::PrepareToParseOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kInitial);
  HandleScope scope(isolate_);
  unicode_cache_.reset(new UnicodeCache());
  zone_.reset(new Zone(isolate_->allocator()));
  Handle<SharedFunctionInfo> shared(function_->shared(), isolate_);
  Handle<Script> script(Script::cast(shared->script()), isolate_);
  DCHECK(script->type() != Script::TYPE_NATIVE);

  Handle<String> source(String::cast(script->source()), isolate_);
  if (source->IsExternalTwoByteString()) {
    character_stream_.reset(new ExternalTwoByteStringUtf16CharacterStream(
        Handle<ExternalTwoByteString>::cast(source), shared->start_position(),
        shared->end_position()));
  } else if (source->IsExternalOneByteString()) {
    character_stream_.reset(new ExternalOneByteStringUtf16CharacterStream(
        Handle<ExternalOneByteString>::cast(source), shared->start_position(),
        shared->end_position()));
  } else {
    source = String::Flatten(source);
    // Have to globalize the reference here, so it survives between function
    // calls.
    source_ = Handle<String>::cast(isolate_->global_handles()->Create(*source));
    character_stream_.reset(new GenericStringUtf16CharacterStream(
        source_, shared->start_position(), shared->end_position()));
  }
  parse_info_.reset(new ParseInfo(zone_.get()));
  parse_info_->set_isolate(isolate_);
  parse_info_->set_character_stream(character_stream_.get());
  parse_info_->set_lazy();
  parse_info_->set_hash_seed(isolate_->heap()->HashSeed());
  parse_info_->set_is_named_expression(shared->is_named_expression());
  parse_info_->set_calls_eval(shared->scope_info()->CallsEval());
  parse_info_->set_compiler_hints(shared->compiler_hints());
  parse_info_->set_start_position(shared->start_position());
  parse_info_->set_end_position(shared->end_position());
  parse_info_->set_unicode_cache(unicode_cache_.get());
  parse_info_->set_language_mode(shared->language_mode());

  parser_.reset(new Parser(parse_info_.get()));
  parser_->DeserializeScopeChain(
      parse_info_.get(), handle(function_->context(), isolate_),
      Scope::DeserializationMode::kDeserializeOffHeap);

  Handle<String> name(String::cast(shared->name()));
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

  uintptr_t stack_limit =
      reinterpret_cast<uintptr_t>(&stack_limit) - max_stack_size_ * KB;

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
    status_ = CompileJobStatus::kReadyToCompile;
  }

  DeferredHandleScope scope(isolate_);
  {
    // Create a canonical handle scope before internalizing parsed values if
    // compiling bytecode. This is required for off-thread bytecode generation.
    std::unique_ptr<CanonicalHandleScope> canonical;
    if (FLAG_ignition) canonical.reset(new CanonicalHandleScope(isolate_));

    Handle<SharedFunctionInfo> shared(function_->shared(), isolate_);
    Handle<Script> script(Script::cast(shared->script()), isolate_);

    parse_info_->set_script(script);
    parse_info_->set_context(handle(function_->context(), isolate_));

    // Do the parsing tasks which need to be done on the main thread. This will
    // also handle parse errors.
    parser_->Internalize(isolate_, script, parse_info_->literal() == nullptr);
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

void CompilerDispatcherJob::ResetOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));

  parser_.reset();
  unicode_cache_.reset();
  character_stream_.reset();
  parse_info_.reset();
  zone_.reset();
  handles_from_parsing_.reset();

  if (!source_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }

  status_ = CompileJobStatus::kInitial;
}

}  // namespace internal
}  // namespace v8
