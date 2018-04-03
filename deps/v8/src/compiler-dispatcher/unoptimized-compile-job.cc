// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/unoptimized-compile-job.h"

#include "src/assert-scope.h"
#include "src/base/optional.h"
#include "src/compilation-info.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler.h"
#include "src/flags.h"
#include "src/global-handles.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/unicode-cache.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace {

class OneByteWrapper : public v8::String::ExternalOneByteStringResource {
 public:
  OneByteWrapper(const void* data, int length) : data_(data), length_(length) {}
  ~OneByteWrapper() override = default;

  const char* data() const override {
    return reinterpret_cast<const char*>(data_);
  }

  size_t length() const override { return static_cast<size_t>(length_); }

 private:
  const void* data_;
  int length_;

  DISALLOW_COPY_AND_ASSIGN(OneByteWrapper);
};

class TwoByteWrapper : public v8::String::ExternalStringResource {
 public:
  TwoByteWrapper(const void* data, int length) : data_(data), length_(length) {}
  ~TwoByteWrapper() override = default;

  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(data_);
  }

  size_t length() const override { return static_cast<size_t>(length_); }

 private:
  const void* data_;
  int length_;

  DISALLOW_COPY_AND_ASSIGN(TwoByteWrapper);
};

}  // namespace

UnoptimizedCompileJob::UnoptimizedCompileJob(Isolate* isolate,
                                             CompilerDispatcherTracer* tracer,
                                             Handle<SharedFunctionInfo> shared,
                                             size_t max_stack_size)
    : CompilerDispatcherJob(Type::kUnoptimizedCompile),
      main_thread_id_(isolate->thread_id().ToInteger()),
      tracer_(tracer),
      allocator_(isolate->allocator()),
      context_(isolate->global_handles()->Create(isolate->context())),
      shared_(isolate->global_handles()->Create(*shared)),
      max_stack_size_(max_stack_size),
      trace_compiler_dispatcher_jobs_(FLAG_trace_compiler_dispatcher_jobs) {
  DCHECK(!shared_->is_toplevel());
  // TODO(rmcilroy): Handle functions with non-empty outer scope info.
  DCHECK(shared_->outer_scope_info()->IsTheHole(isolate) ||
         ScopeInfo::cast(shared_->outer_scope_info())->length() == 0);
  HandleScope scope(isolate);
  Handle<Script> script(Script::cast(shared_->script()), isolate);
  Handle<String> source(String::cast(script->source()), isolate);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p] created for ", static_cast<void*>(this));
    ShortPrintOnMainThread();
    PrintF(" in initial state.\n");
  }
}

UnoptimizedCompileJob::~UnoptimizedCompileJob() {
  DCHECK(status() == Status::kInitial || status() == Status::kDone);
  if (!shared_.is_null()) {
    DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
    i::GlobalHandles::Destroy(Handle<Object>::cast(shared_).location());
  }
  if (!context_.is_null()) {
    DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
    i::GlobalHandles::Destroy(Handle<Object>::cast(context_).location());
  }
}

bool UnoptimizedCompileJob::IsAssociatedWith(
    Handle<SharedFunctionInfo> shared) const {
  return *shared_ == *shared;
}

void UnoptimizedCompileJob::PrepareOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
  DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);
  DCHECK_EQ(status(), Status::kInitial);
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kPrepare);

  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Preparing to parse\n",
           static_cast<void*>(this));
  }

  HandleScope scope(isolate);
  unicode_cache_.reset(new UnicodeCache());
  Handle<Script> script(Script::cast(shared_->script()), isolate);
  DCHECK(script->type() != Script::TYPE_NATIVE);

  Handle<String> source(String::cast(script->source()), isolate);
  parse_info_.reset(new ParseInfo(isolate->allocator()));
  parse_info_->InitFromIsolate(isolate);
  if (source->IsExternalTwoByteString() || source->IsExternalOneByteString()) {
    std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(
        source, shared_->start_position(), shared_->end_position()));
    parse_info_->set_character_stream(std::move(stream));
  } else {
    source = String::Flatten(source);
    const void* data;
    int offset = 0;
    int length = source->length();

    // Objects in lo_space don't move, so we can just read the contents from
    // any thread.
    if (isolate->heap()->lo_space()->Contains(*source)) {
      // We need to globalize the handle to the flattened string here, in
      // case it's not referenced from anywhere else.
      source_ = isolate->global_handles()->Create(*source);
      DisallowHeapAllocation no_allocation;
      String::FlatContent content = source->GetFlatContent();
      DCHECK(content.IsFlat());
      data =
          content.IsOneByte()
              ? reinterpret_cast<const void*>(content.ToOneByteVector().start())
              : reinterpret_cast<const void*>(content.ToUC16Vector().start());
    } else {
      // Otherwise, create a copy of the part of the string we'll parse in the
      // zone.
      length = (shared_->end_position() - shared_->start_position());
      offset = shared_->start_position();

      int byte_len = length * (source->IsOneByteRepresentation() ? 1 : 2);
      data = parse_info_->zone()->New(byte_len);

      DisallowHeapAllocation no_allocation;
      String::FlatContent content = source->GetFlatContent();
      DCHECK(content.IsFlat());
      if (content.IsOneByte()) {
        MemCopy(const_cast<void*>(data),
                &content.ToOneByteVector().at(shared_->start_position()),
                byte_len);
      } else {
        MemCopy(const_cast<void*>(data),
                &content.ToUC16Vector().at(shared_->start_position()),
                byte_len);
      }
    }
    Handle<String> wrapper;
    if (source->IsOneByteRepresentation()) {
      ExternalOneByteString::Resource* resource =
          new OneByteWrapper(data, length);
      source_wrapper_.reset(resource);
      wrapper = isolate->factory()
                    ->NewExternalStringFromOneByte(resource)
                    .ToHandleChecked();
    } else {
      ExternalTwoByteString::Resource* resource =
          new TwoByteWrapper(data, length);
      source_wrapper_.reset(resource);
      wrapper = isolate->factory()
                    ->NewExternalStringFromTwoByte(resource)
                    .ToHandleChecked();
    }
    wrapper_ = isolate->global_handles()->Create(*wrapper);
    std::unique_ptr<Utf16CharacterStream> stream(
        ScannerStream::For(wrapper_, shared_->start_position() - offset,
                           shared_->end_position() - offset));
    parse_info_->set_character_stream(std::move(stream));
  }
  parse_info_->set_hash_seed(isolate->heap()->HashSeed());
  parse_info_->set_is_named_expression(shared_->is_named_expression());
  parse_info_->set_compiler_hints(shared_->compiler_hints());
  parse_info_->set_start_position(shared_->start_position());
  parse_info_->set_end_position(shared_->end_position());
  parse_info_->set_unicode_cache(unicode_cache_.get());
  parse_info_->set_language_mode(shared_->language_mode());
  parse_info_->set_function_literal_id(shared_->function_literal_id());
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    parse_info_->set_runtime_call_stats(new (parse_info_->zone())
                                            RuntimeCallStats());
  }

  parser_.reset(new Parser(parse_info_.get()));
  MaybeHandle<ScopeInfo> outer_scope_info;
  if (!shared_->outer_scope_info()->IsTheHole(isolate) &&
      ScopeInfo::cast(shared_->outer_scope_info())->length() > 0) {
    outer_scope_info = handle(ScopeInfo::cast(shared_->outer_scope_info()));
  }
  parser_->DeserializeScopeChain(parse_info_.get(), outer_scope_info);

  Handle<String> name(shared_->name());
  parse_info_->set_function_name(
      parse_info_->ast_value_factory()->GetString(name));
  set_status(Status::kPrepared);
}

void UnoptimizedCompileJob::Compile(bool on_background_thread) {
  DCHECK_EQ(status(), Status::kPrepared);
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(
      tracer_, kCompile,
      parse_info_->end_position() - parse_info_->start_position());
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Compiling\n", static_cast<void*>(this));
  }

  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  parse_info_->set_on_background_thread(on_background_thread);
  uintptr_t stack_limit = GetCurrentStackPosition() - max_stack_size_ * KB;
  parser_->set_stack_limit(stack_limit);
  parse_info_->set_stack_limit(stack_limit);
  parser_->ParseOnBackground(parse_info_.get());

  if (parse_info_->literal() == nullptr) {
    // Parser sets error in pending error handler.
    set_status(Status::kHasErrorsToReport);
    return;
  }

  if (!Compiler::Analyze(parse_info_.get())) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kHasErrorsToReport);
    return;
  }

  compilation_job_.reset(interpreter::Interpreter::NewCompilationJob(
      parse_info_.get(), parse_info_->literal(), allocator_));

  if (!compilation_job_.get()) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kHasErrorsToReport);
    return;
  }

  if (compilation_job_->ExecuteJob() != CompilationJob::SUCCEEDED) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kHasErrorsToReport);
    return;
  }

  set_status(Status::kCompiled);
}

void UnoptimizedCompileJob::FinalizeOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
  DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);
  DCHECK_EQ(status(), Status::kCompiled);
  DCHECK_NOT_NULL(parse_info_->literal());
  DCHECK_NOT_NULL(compilation_job_.get());
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kFinalize);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Finalizing compiling\n",
           static_cast<void*>(this));
  }

  Handle<Script> script(Script::cast(shared_->script()), isolate);
  parse_info_->set_script(script);
  parser_->UpdateStatistics(isolate, script);
  parse_info_->UpdateBackgroundParseStatisticsOnMainThread(isolate);
  parser_->HandleSourceURLComments(isolate, script);

  {
    HandleScope scope(isolate);
    // Internalize ast values onto the heap.
    parse_info_->ast_value_factory()->Internalize(isolate);
    // Allocate scope infos for the literal.
    DeclarationScope::AllocateScopeInfos(parse_info_.get(), isolate,
                                         AnalyzeMode::kRegular);
    compilation_job_->compilation_info()->set_shared_info(shared_);
    if (compilation_job_->state() == CompilationJob::State::kFailed ||
        !Compiler::FinalizeCompilationJob(compilation_job_.release(),
                                          isolate)) {
      if (!isolate->has_pending_exception()) isolate->StackOverflow();
      set_status(Status::kFailed);
      return;
    }
  }

  ResetDataOnMainThread(isolate);
  set_status(Status::kDone);
}

void UnoptimizedCompileJob::ReportErrorsOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
  DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);
  DCHECK_EQ(status(), Status::kHasErrorsToReport);

  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Reporting Errors\n",
           static_cast<void*>(this));
  }

  // Ensure we report errors in the correct context for the job.
  SaveContext save(isolate);
  isolate->set_context(context());

  Handle<Script> script(Script::cast(shared_->script()), isolate);
  parse_info_->pending_error_handler()->ReportErrors(
      isolate, script, parse_info_->ast_value_factory());

  ResetDataOnMainThread(isolate);
  set_status(Status::kFailed);
}

void UnoptimizedCompileJob::ResetDataOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
  DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);

  compilation_job_.reset();
  parser_.reset();
  unicode_cache_.reset();
  parse_info_.reset();

  if (!source_.is_null()) {
    DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
    DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }
  if (!wrapper_.is_null()) {
    DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
    DCHECK_EQ(isolate->thread_id().ToInteger(), main_thread_id_);
    i::GlobalHandles::Destroy(Handle<Object>::cast(wrapper_).location());
    wrapper_ = Handle<String>::null();
  }
}

void UnoptimizedCompileJob::ResetOnMainThread(Isolate* isolate) {
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Resetting\n", static_cast<void*>(this));
  }

  ResetDataOnMainThread(isolate);
  set_status(Status::kInitial);
}

double UnoptimizedCompileJob::EstimateRuntimeOfNextStepInMs() const {
  switch (status()) {
    case Status::kInitial:
      return tracer_->EstimatePrepareInMs();
    case Status::kPrepared:
      return tracer_->EstimateCompileInMs(parse_info_->end_position() -
                                          parse_info_->start_position());
    case Status::kCompiled:
      return tracer_->EstimateFinalizeInMs();

    case Status::kHasErrorsToReport:
    case Status::kFailed:
    case Status::kDone:
      return 0.0;
  }

  UNREACHABLE();
}

void UnoptimizedCompileJob::ShortPrintOnMainThread() {
  DCHECK_EQ(ThreadId::Current().ToInteger(), main_thread_id_);
  DCHECK(!shared_.is_null());
  shared_->ShortPrint();
}

}  // namespace internal
}  // namespace v8
