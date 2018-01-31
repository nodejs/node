// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/api.h"
#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/heap/heap-inl.h"
#include "src/objects-inl.h"
#include "src/objects/scope-info.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

ParseInfo::ParseInfo(AccountingAllocator* zone_allocator)
    : zone_(std::make_shared<Zone>(zone_allocator, ZONE_NAME)),
      flags_(0),
      extension_(nullptr),
      compile_options_(ScriptCompiler::kNoCompileOptions),
      script_scope_(nullptr),
      unicode_cache_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      compiler_hints_(0),
      start_position_(0),
      end_position_(0),
      parameters_end_pos_(kNoSourcePosition),
      function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      max_function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      character_stream_(nullptr),
      cached_data_(nullptr),
      ast_value_factory_(nullptr),
      ast_string_constants_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(nullptr),
      source_range_map_(nullptr),
      literal_(nullptr) {}

ParseInfo::ParseInfo(Handle<SharedFunctionInfo> shared)
    : ParseInfo(shared->GetIsolate()->allocator()) {
  Isolate* isolate = shared->GetIsolate();
  InitFromIsolate(isolate);

  set_toplevel(shared->is_toplevel());
  set_allow_lazy_parsing(FLAG_lazy_inner_functions);
  set_is_named_expression(shared->is_named_expression());
  set_compiler_hints(shared->compiler_hints());
  set_start_position(shared->start_position());
  set_end_position(shared->end_position());
  function_literal_id_ = shared->function_literal_id();
  set_language_mode(shared->language_mode());
  set_asm_wasm_broken(shared->is_asm_wasm_broken());
  set_requires_instance_fields_initializer(
      shared->requires_instance_fields_initializer());

  Handle<Script> script(Script::cast(shared->script()));
  set_script(script);
  set_native(script->type() == Script::TYPE_NATIVE);
  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_module(script->origin_options().IsModule());
  DCHECK(!(is_eval() && is_module()));

  Handle<HeapObject> scope_info(shared->outer_scope_info());
  if (!scope_info->IsTheHole(isolate) &&
      Handle<ScopeInfo>::cast(scope_info)->length() > 0) {
    set_outer_scope_info(Handle<ScopeInfo>::cast(scope_info));
  }

  // CollectTypeProfile uses its own feedback slots. If we have existing
  // FeedbackMetadata, we can only collect type profile if the feedback vector
  // has the appropriate slots.
  set_collect_type_profile(
      isolate->is_collecting_type_profile() &&
      (shared->feedback_metadata()->length() == 0
           ? script->IsUserJavaScript()
           : shared->feedback_metadata()->HasTypeProfileSlot()));
  if (block_coverage_enabled() && script->IsUserJavaScript()) {
    AllocateSourceRangeMap();
  }
}

ParseInfo::ParseInfo(Handle<Script> script)
    : ParseInfo(script->GetIsolate()->allocator()) {
  InitFromIsolate(script->GetIsolate());

  set_allow_lazy_parsing();
  set_toplevel();
  set_script(script);

  set_native(script->type() == Script::TYPE_NATIVE);
  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_module(script->origin_options().IsModule());
  DCHECK(!(is_eval() && is_module()));

  set_collect_type_profile(script->GetIsolate()->is_collecting_type_profile() &&
                           script->IsUserJavaScript());
  if (block_coverage_enabled() && script->IsUserJavaScript()) {
    AllocateSourceRangeMap();
  }
}

ParseInfo::~ParseInfo() {}

// static
ParseInfo* ParseInfo::AllocateWithoutScript(Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  ParseInfo* p = new ParseInfo(isolate->allocator());

  p->InitFromIsolate(isolate);
  p->set_toplevel(shared->is_toplevel());
  p->set_allow_lazy_parsing(FLAG_lazy_inner_functions);
  p->set_is_named_expression(shared->is_named_expression());
  p->set_compiler_hints(shared->compiler_hints());
  p->set_start_position(shared->start_position());
  p->set_end_position(shared->end_position());
  p->function_literal_id_ = shared->function_literal_id();
  p->set_language_mode(shared->language_mode());

  // BUG(5946): This function exists as a workaround until we can
  // get rid of %SetCode in our native functions. The ParseInfo
  // is explicitly set up for the case that:
  // a) you have a native built-in,
  // b) it's being run for the 2nd-Nth time in an isolate,
  // c) we've already compiled bytecode and therefore don't need
  //    to parse.
  // We tolerate a ParseInfo without a Script in this case.
  p->set_native(true);
  p->set_eval(false);
  p->set_module(false);
  DCHECK_NE(shared->kind(), FunctionKind::kModule);

  Handle<HeapObject> scope_info(shared->outer_scope_info());
  if (!scope_info->IsTheHole(isolate) &&
      Handle<ScopeInfo>::cast(scope_info)->length() > 0) {
    p->set_outer_scope_info(Handle<ScopeInfo>::cast(scope_info));
  }
  return p;
}

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

bool ParseInfo::is_declaration() const {
  return SharedFunctionInfo::IsDeclarationBit::decode(compiler_hints_);
}

FunctionKind ParseInfo::function_kind() const {
  return SharedFunctionInfo::FunctionKindBits::decode(compiler_hints_);
}

void ParseInfo::InitFromIsolate(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  set_hash_seed(isolate->heap()->HashSeed());
  set_stack_limit(isolate->stack_guard()->real_climit());
  set_unicode_cache(isolate->unicode_cache());
  set_runtime_call_stats(isolate->counters()->runtime_call_stats());
  set_logger(isolate->logger());
  set_ast_string_constants(isolate->ast_string_constants());
  if (isolate->is_block_code_coverage()) set_block_coverage_enabled();
  if (isolate->is_collecting_type_profile()) set_collect_type_profile();
}

void ParseInfo::EmitBackgroundParseStatisticsOnBackgroundThread() {
  // If runtime call stats was enabled by tracing, emit a trace event at the
  // end of background parsing on the background thread.
  if (runtime_call_stats_ &&
      (FLAG_runtime_stats &
       v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    auto value = v8::tracing::TracedValue::Create();
    runtime_call_stats_->Dump(value.get());
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"),
                         "V8.RuntimeStats", TRACE_EVENT_SCOPE_THREAD,
                         "runtime-call-stats", std::move(value));
  }
}

void ParseInfo::UpdateBackgroundParseStatisticsOnMainThread(Isolate* isolate) {
  // Copy over the counters from the background thread to the main counters on
  // the isolate.
  RuntimeCallStats* main_call_stats = isolate->counters()->runtime_call_stats();
  if (FLAG_runtime_stats ==
      v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE) {
    DCHECK_NE(main_call_stats, runtime_call_stats());
    DCHECK_NOT_NULL(main_call_stats);
    DCHECK_NOT_NULL(runtime_call_stats());
    main_call_stats->Add(runtime_call_stats());
  }
  set_runtime_call_stats(main_call_stats);
}

void ParseInfo::ShareZone(ParseInfo* other) {
  DCHECK_EQ(0, zone_->allocation_size());
  zone_ = other->zone_;
}

AstValueFactory* ParseInfo::GetOrCreateAstValueFactory() {
  if (!ast_value_factory_.get()) {
    ast_value_factory_.reset(
        new AstValueFactory(zone(), ast_string_constants(), hash_seed()));
  }
  return ast_value_factory();
}

void ParseInfo::ShareAstValueFactory(ParseInfo* other) {
  DCHECK(!ast_value_factory_.get());
  ast_value_factory_ = other->ast_value_factory_;
}

void ParseInfo::AllocateSourceRangeMap() {
  DCHECK(block_coverage_enabled());
  set_source_range_map(new (zone()) SourceRangeMap(zone()));
}

void ParseInfo::ResetCharacterStream() { character_stream_.reset(); }

void ParseInfo::set_character_stream(
    std::unique_ptr<Utf16CharacterStream> character_stream) {
  DCHECK_NULL(character_stream_);
  character_stream_.swap(character_stream);
}

}  // namespace internal
}  // namespace v8
