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

ParseInfo::ParseInfo(Isolate* isolate, AccountingAllocator* zone_allocator)
    : zone_(std::make_shared<Zone>(zone_allocator, ZONE_NAME)),
      flags_(0),
      extension_(nullptr),
      script_scope_(nullptr),
      unicode_cache_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      function_flags_(0),
      script_id_(-1),
      start_position_(0),
      end_position_(0),
      parameters_end_pos_(kNoSourcePosition),
      function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      max_function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      character_stream_(nullptr),
      ast_value_factory_(nullptr),
      ast_string_constants_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(nullptr),
      source_range_map_(nullptr),
      literal_(nullptr) {
  set_hash_seed(isolate->heap()->HashSeed());
  set_stack_limit(isolate->stack_guard()->real_climit());
  set_unicode_cache(isolate->unicode_cache());
  set_runtime_call_stats(isolate->counters()->runtime_call_stats());
  set_logger(isolate->logger());
  set_ast_string_constants(isolate->ast_string_constants());
  if (isolate->is_block_code_coverage()) set_block_coverage_enabled();
  if (isolate->is_collecting_type_profile()) set_collect_type_profile();
}

ParseInfo::ParseInfo(Isolate* isolate)
    : ParseInfo(isolate, isolate->allocator()) {
  script_id_ = isolate->heap()->NextScriptId();
  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kReserveId, script_id_));
}

ParseInfo::ParseInfo(Isolate* isolate, Handle<SharedFunctionInfo> shared)
    : ParseInfo(isolate, isolate->allocator()) {
  // Do not support re-parsing top-level function of a wrapped script.
  // TODO(yangguo): consider whether we need a top-level function in a
  //                wrapped script at all.
  DCHECK_IMPLIES(is_toplevel(), !Script::cast(shared->script())->is_wrapped());

  set_toplevel(shared->is_toplevel());
  set_wrapped_as_function(shared->is_wrapped());
  set_allow_lazy_parsing(FLAG_lazy_inner_functions);
  set_is_named_expression(shared->is_named_expression());
  set_function_flags(shared->flags());
  set_start_position(shared->StartPosition());
  set_end_position(shared->EndPosition());
  function_literal_id_ = shared->FunctionLiteralId(isolate);
  set_language_mode(shared->language_mode());
  set_asm_wasm_broken(shared->is_asm_wasm_broken());

  Handle<Script> script(Script::cast(shared->script()), isolate);
  set_script(script);

  if (shared->HasOuterScopeInfo()) {
    set_outer_scope_info(handle(shared->GetOuterScopeInfo(), isolate));
  }

  // CollectTypeProfile uses its own feedback slots. If we have existing
  // FeedbackMetadata, we can only collect type profile if the feedback vector
  // has the appropriate slots.
  set_collect_type_profile(
      isolate->is_collecting_type_profile() &&
      (shared->HasFeedbackMetadata()
           ? shared->feedback_metadata()->HasTypeProfileSlot()
           : script->IsUserJavaScript()));
}

ParseInfo::ParseInfo(Isolate* isolate, Handle<Script> script)
    : ParseInfo(isolate, isolate->allocator()) {
  SetScriptForToplevelCompile(isolate, script);
  set_collect_type_profile(isolate->is_collecting_type_profile() &&
                           script->IsUserJavaScript());
}

ParseInfo::~ParseInfo() {}

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

bool ParseInfo::is_declaration() const {
  return SharedFunctionInfo::IsDeclarationBit::decode(function_flags_);
}

FunctionKind ParseInfo::function_kind() const {
  return SharedFunctionInfo::FunctionKindBits::decode(function_flags_);
}

bool ParseInfo::requires_instance_fields_initializer() const {
  return SharedFunctionInfo::RequiresInstanceFieldsInitializer::decode(
      function_flags_);
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

Handle<Script> ParseInfo::CreateScript(Isolate* isolate, Handle<String> source,
                                       ScriptOriginOptions origin_options,
                                       NativesFlag natives) {
  // Create a script object describing the script to be compiled.
  Handle<Script> script;
  if (script_id_ == -1) {
    script = isolate->factory()->NewScript(source);
  } else {
    script = isolate->factory()->NewScriptWithId(source, script_id_);
  }
  if (isolate->NeedsSourcePositionsForProfiling()) {
    Script::InitLineEnds(script);
  }
  switch (natives) {
    case NATIVES_CODE:
      script->set_type(Script::TYPE_NATIVE);
      break;
    case EXTENSION_CODE:
      script->set_type(Script::TYPE_EXTENSION);
      break;
    case INSPECTOR_CODE:
      script->set_type(Script::TYPE_INSPECTOR);
      break;
    case NOT_NATIVES_CODE:
      break;
  }
  script->set_origin_options(origin_options);

  SetScriptForToplevelCompile(isolate, script);
  return script;
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

void ParseInfo::SetScriptForToplevelCompile(Isolate* isolate,
                                            Handle<Script> script) {
  set_script(script);
  set_allow_lazy_parsing();
  set_toplevel();
  set_collect_type_profile(isolate->is_collecting_type_profile() &&
                           script->IsUserJavaScript());
  set_wrapped_as_function(script->is_wrapped());
}

void ParseInfo::set_script(Handle<Script> script) {
  script_ = script;
  DCHECK(script_id_ == -1 || script_id_ == script->id());
  script_id_ = script->id();

  set_native(script->type() == Script::TYPE_NATIVE);
  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_module(script->origin_options().IsModule());
  DCHECK(!(is_eval() && is_module()));

  if (block_coverage_enabled() && script->IsUserJavaScript()) {
    AllocateSourceRangeMap();
  }
}

}  // namespace internal
}  // namespace v8
