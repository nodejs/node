// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-job.h"

#include "src/assert-scope.h"
#include "src/compilation-info.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler.h"
#include "src/flags.h"
#include "src/global-handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/unicode-cache.h"
#include "src/utils.h"
#include "src/zone/zone.h"

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

CompilerDispatcherJob::CompilerDispatcherJob(Isolate* isolate,
                                             CompilerDispatcherTracer* tracer,
                                             Handle<SharedFunctionInfo> shared,
                                             size_t max_stack_size)
    : isolate_(isolate),
      tracer_(tracer),
      shared_(Handle<SharedFunctionInfo>::cast(
          isolate_->global_handles()->Create(*shared))),
      max_stack_size_(max_stack_size),
      trace_compiler_dispatcher_jobs_(FLAG_trace_compiler_dispatcher_jobs) {
  HandleScope scope(isolate_);
  DCHECK(!shared_->outer_scope_info()->IsTheHole(isolate_));
  Handle<Script> script(Script::cast(shared_->script()), isolate_);
  Handle<String> source(String::cast(script->source()), isolate_);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p] created for ", static_cast<void*>(this));
    shared_->ShortPrint();
    PrintF("\n");
  }
}

CompilerDispatcherJob::~CompilerDispatcherJob() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status_ == CompileJobStatus::kInitial ||
         status_ == CompileJobStatus::kDone);
  i::GlobalHandles::Destroy(Handle<Object>::cast(shared_).location());
}

bool CompilerDispatcherJob::IsAssociatedWith(
    Handle<SharedFunctionInfo> shared) const {
  return *shared_ == *shared;
}

void CompilerDispatcherJob::PrepareToParseOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kInitial);
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kPrepareToParse);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Preparing to parse\n",
           static_cast<void*>(this));
  }
  HandleScope scope(isolate_);
  unicode_cache_.reset(new UnicodeCache());
  zone_.reset(new Zone(isolate_->allocator(), ZONE_NAME));
  Handle<Script> script(Script::cast(shared_->script()), isolate_);
  DCHECK(script->type() != Script::TYPE_NATIVE);

  Handle<String> source(String::cast(script->source()), isolate_);
  if (source->IsExternalTwoByteString() || source->IsExternalOneByteString()) {
    character_stream_.reset(ScannerStream::For(
        source, shared_->start_position(), shared_->end_position()));
  } else {
    source = String::Flatten(source);
    const void* data;
    int offset = 0;
    int length = source->length();

    // Objects in lo_space don't move, so we can just read the contents from
    // any thread.
    if (isolate_->heap()->lo_space()->Contains(*source)) {
      // We need to globalize the handle to the flattened string here, in
      // case it's not referenced from anywhere else.
      source_ =
          Handle<String>::cast(isolate_->global_handles()->Create(*source));
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
      data = zone_->New(byte_len);

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
      wrapper = isolate_->factory()
                    ->NewExternalStringFromOneByte(resource)
                    .ToHandleChecked();
    } else {
      ExternalTwoByteString::Resource* resource =
          new TwoByteWrapper(data, length);
      source_wrapper_.reset(resource);
      wrapper = isolate_->factory()
                    ->NewExternalStringFromTwoByte(resource)
                    .ToHandleChecked();
    }
    wrapper_ =
        Handle<String>::cast(isolate_->global_handles()->Create(*wrapper));

    character_stream_.reset(
        ScannerStream::For(wrapper_, shared_->start_position() - offset,
                           shared_->end_position() - offset));
  }
  parse_info_.reset(new ParseInfo(zone_.get()));
  parse_info_->set_isolate(isolate_);
  parse_info_->set_character_stream(character_stream_.get());
  parse_info_->set_hash_seed(isolate_->heap()->HashSeed());
  parse_info_->set_is_named_expression(shared_->is_named_expression());
  parse_info_->set_compiler_hints(shared_->compiler_hints());
  parse_info_->set_start_position(shared_->start_position());
  parse_info_->set_end_position(shared_->end_position());
  parse_info_->set_unicode_cache(unicode_cache_.get());
  parse_info_->set_language_mode(shared_->language_mode());
  parse_info_->set_function_literal_id(shared_->function_literal_id());

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
  DCHECK(status() == CompileJobStatus::kReadyToParse);
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(
      tracer_, kParse,
      parse_info_->end_position() - parse_info_->start_position());
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Parsing\n", static_cast<void*>(this));
  }

  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

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
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kFinalizeParsing);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Finalizing parsing\n",
           static_cast<void*>(this));
  }

  if (!source_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }
  if (!wrapper_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(wrapper_).location());
    wrapper_ = Handle<String>::null();
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

    // Do the parsing tasks which need to be done on the main thread. This
    // will also handle parse errors.
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

bool CompilerDispatcherJob::PrepareToCompileOnMainThread() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  DCHECK(status() == CompileJobStatus::kReadyToAnalyse);
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kPrepareToCompile);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Preparing to compile\n",
           static_cast<void*>(this));
  }

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

  CHECK(compile_job_->can_execute_on_background_thread());
  status_ = CompileJobStatus::kReadyToCompile;
  return true;
}

void CompilerDispatcherJob::Compile() {
  DCHECK(status() == CompileJobStatus::kReadyToCompile);
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(
      tracer_, kCompile, parse_info_->literal()->ast_node_count());
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Compiling\n", static_cast<void*>(this));
  }

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
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kFinalizeCompiling);
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Finalizing compiling\n",
           static_cast<void*>(this));
  }

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

  if (trace_compiler_dispatcher_jobs_) {
    PrintF("CompilerDispatcherJob[%p]: Resetting\n", static_cast<void*>(this));
  }

  parser_.reset();
  unicode_cache_.reset();
  character_stream_.reset();
  parse_info_.reset();
  handles_from_parsing_.reset();
  compile_info_.reset();
  compile_job_.reset();
  zone_.reset();

  if (!source_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(source_).location());
    source_ = Handle<String>::null();
  }
  if (!wrapper_.is_null()) {
    i::GlobalHandles::Destroy(Handle<Object>::cast(wrapper_).location());
    wrapper_ = Handle<String>::null();
  }

  status_ = CompileJobStatus::kInitial;
}

double CompilerDispatcherJob::EstimateRuntimeOfNextStepInMs() const {
  switch (status_) {
    case CompileJobStatus::kInitial:
      return tracer_->EstimatePrepareToParseInMs();

    case CompileJobStatus::kReadyToParse:
      return tracer_->EstimateParseInMs(parse_info_->end_position() -
                                        parse_info_->start_position());

    case CompileJobStatus::kParsed:
      return tracer_->EstimateFinalizeParsingInMs();

    case CompileJobStatus::kReadyToAnalyse:
      return tracer_->EstimatePrepareToCompileInMs();

    case CompileJobStatus::kReadyToCompile:
      return tracer_->EstimateCompileInMs(
          parse_info_->literal()->ast_node_count());

    case CompileJobStatus::kCompiled:
      return tracer_->EstimateFinalizeCompilingInMs();

    case CompileJobStatus::kFailed:
    case CompileJobStatus::kDone:
      return 0.0;
  }

  UNREACHABLE();
  return 0.0;
}

void CompilerDispatcherJob::ShortPrint() {
  DCHECK(ThreadId::Current().Equals(isolate_->thread_id()));
  shared_->ShortPrint();
}

}  // namespace internal
}  // namespace v8
