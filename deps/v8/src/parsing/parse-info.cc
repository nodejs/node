// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/base/template-utils.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/scope-info.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

ParseInfo::ParseInfo(AccountingAllocator* zone_allocator)
    : zone_(base::make_unique<Zone>(zone_allocator, ZONE_NAME)),
      flags_(0),
      extension_(nullptr),
      script_scope_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      function_kind_(FunctionKind::kNormalFunction),
      function_syntax_kind_(FunctionSyntaxKind::kDeclaration),
      script_id_(-1),
      start_position_(0),
      end_position_(0),
      parameters_end_pos_(kNoSourcePosition),
      function_literal_id_(kFunctionLiteralIdInvalid),
      max_function_literal_id_(kFunctionLiteralIdInvalid),
      character_stream_(nullptr),
      ast_value_factory_(nullptr),
      ast_string_constants_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(nullptr),
      source_range_map_(nullptr),
      literal_(nullptr) {}

ParseInfo::ParseInfo(Isolate* isolate, AccountingAllocator* zone_allocator)
    : ParseInfo(zone_allocator) {
  set_hash_seed(HashSeed(isolate));
  set_stack_limit(isolate->stack_guard()->real_climit());
  set_runtime_call_stats(isolate->counters()->runtime_call_stats());
  set_logger(isolate->logger());
  set_ast_string_constants(isolate->ast_string_constants());
  set_collect_source_positions(!FLAG_enable_lazy_source_positions ||
                               isolate->NeedsDetailedOptimizedCodeLineInfo());
  if (!isolate->is_best_effort_code_coverage()) set_coverage_enabled();
  if (isolate->is_block_code_coverage()) set_block_coverage_enabled();
  if (isolate->is_collecting_type_profile()) set_collect_type_profile();
  if (isolate->compiler_dispatcher()->IsEnabled()) {
    parallel_tasks_.reset(new ParallelTasks(isolate->compiler_dispatcher()));
  }
  set_might_always_opt(FLAG_always_opt || FLAG_prepare_always_opt);
  set_allow_lazy_compile(FLAG_lazy);
  set_allow_natives_syntax(FLAG_allow_natives_syntax);
  set_allow_harmony_dynamic_import(FLAG_harmony_dynamic_import);
  set_allow_harmony_import_meta(FLAG_harmony_import_meta);
  set_allow_harmony_optional_chaining(FLAG_harmony_optional_chaining);
  set_allow_harmony_nullish(FLAG_harmony_nullish);
  set_allow_harmony_private_methods(FLAG_harmony_private_methods);
}

ParseInfo::ParseInfo(Isolate* isolate)
    : ParseInfo(isolate, isolate->allocator()) {
  script_id_ = isolate->heap()->NextScriptId();
  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kReserveId, script_id_));
}

template <typename T>
void ParseInfo::SetFunctionInfo(T function) {
  set_language_mode(function->language_mode());
  set_function_kind(function->kind());
  set_function_syntax_kind(function->syntax_kind());
  set_requires_instance_members_initializer(
      function->requires_instance_members_initializer());
  set_toplevel(function->is_toplevel());
  set_is_oneshot_iife(function->is_oneshot_iife());
}

ParseInfo::ParseInfo(Isolate* isolate, Handle<SharedFunctionInfo> shared)
    : ParseInfo(isolate, isolate->allocator()) {
  // Do not support re-parsing top-level function of a wrapped script.
  // TODO(yangguo): consider whether we need a top-level function in a
  //                wrapped script at all.
  DCHECK_IMPLIES(is_toplevel(), !Script::cast(shared->script()).is_wrapped());

  set_allow_lazy_parsing(true);
  set_asm_wasm_broken(shared->is_asm_wasm_broken());

  set_start_position(shared->StartPosition());
  set_end_position(shared->EndPosition());
  function_literal_id_ = shared->function_literal_id();
  SetFunctionInfo(shared);

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
           ? shared->feedback_metadata().HasTypeProfileSlot()
           : script->IsUserJavaScript()));
}

ParseInfo::ParseInfo(Isolate* isolate, Handle<Script> script)
    : ParseInfo(isolate, isolate->allocator()) {
  SetScriptForToplevelCompile(isolate, script);
  set_collect_type_profile(isolate->is_collecting_type_profile() &&
                           script->IsUserJavaScript());
}

// static
std::unique_ptr<ParseInfo> ParseInfo::FromParent(
    const ParseInfo* outer_parse_info, AccountingAllocator* zone_allocator,
    const FunctionLiteral* literal, const AstRawString* function_name) {
  std::unique_ptr<ParseInfo> result =
      base::make_unique<ParseInfo>(zone_allocator);

  // Replicate shared state of the outer_parse_info.
  result->flags_ = outer_parse_info->flags_;
  result->script_id_ = outer_parse_info->script_id_;
  result->set_logger(outer_parse_info->logger());
  result->set_ast_string_constants(outer_parse_info->ast_string_constants());
  result->set_hash_seed(outer_parse_info->hash_seed());

  DCHECK_EQ(outer_parse_info->parameters_end_pos(), kNoSourcePosition);
  DCHECK_NULL(outer_parse_info->extension());
  DCHECK(outer_parse_info->maybe_outer_scope_info().is_null());

  // Clone the function_name AstRawString into the ParseInfo's own
  // AstValueFactory.
  const AstRawString* cloned_function_name =
      result->GetOrCreateAstValueFactory()->CloneFromOtherFactory(
          function_name);

  // Setup function specific details.
  DCHECK(!literal->is_toplevel());
  result->set_function_name(cloned_function_name);
  result->set_start_position(literal->start_position());
  result->set_end_position(literal->end_position());
  result->set_function_literal_id(literal->function_literal_id());
  result->SetFunctionInfo(literal);

  return result;
}

ParseInfo::~ParseInfo() = default;

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

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
  if (script->is_wrapped()) {
    set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
  }
}

void ParseInfo::set_script(Handle<Script> script) {
  script_ = script;
  DCHECK(script_id_ == -1 || script_id_ == script->id());
  script_id_ = script->id();

  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_module(script->origin_options().IsModule());
  DCHECK(!(is_eval() && is_module()));

  if (block_coverage_enabled() && script->IsUserJavaScript()) {
    AllocateSourceRangeMap();
  }
}

void ParseInfo::ParallelTasks::Enqueue(ParseInfo* outer_parse_info,
                                       const AstRawString* function_name,
                                       FunctionLiteral* literal) {
  base::Optional<CompilerDispatcher::JobId> job_id =
      dispatcher_->Enqueue(outer_parse_info, function_name, literal);
  if (job_id) {
    enqueued_jobs_.emplace_front(std::make_pair(literal, *job_id));
  }
}

}  // namespace internal
}  // namespace v8
