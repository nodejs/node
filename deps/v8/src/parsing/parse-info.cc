// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/common/globals.h"
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

ParseInfo::ParseInfo(AccountingAllocator* zone_allocator, int script_id)
    : zone_(std::make_unique<Zone>(zone_allocator, ZONE_NAME)),
      flags_(0),
      extension_(nullptr),
      script_scope_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      function_kind_(FunctionKind::kNormalFunction),
      function_syntax_kind_(FunctionSyntaxKind::kDeclaration),
      script_id_(script_id),
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

ParseInfo::ParseInfo(Isolate* isolate, AccountingAllocator* zone_allocator,
                     int script_id)
    : ParseInfo(zone_allocator, script_id) {
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
  set_allow_harmony_top_level_await(FLAG_harmony_top_level_await);
}

ParseInfo::ParseInfo(Isolate* isolate)
    : ParseInfo(isolate, isolate->allocator(), isolate->GetNextScriptId()) {
  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kReserveId, script_id()));
}

template <typename T>
void ParseInfo::SetFunctionInfo(T function) {
  set_language_mode(function->language_mode());
  set_function_kind(function->kind());
  set_function_syntax_kind(function->syntax_kind());
  set_requires_instance_members_initializer(
      function->requires_instance_members_initializer());
  set_class_scope_has_private_brand(function->class_scope_has_private_brand());
  set_has_static_private_methods_or_accessors(
      function->has_static_private_methods_or_accessors());
  set_toplevel(function->is_toplevel());
  set_is_oneshot_iife(function->is_oneshot_iife());
}

ParseInfo::ParseInfo(Isolate* isolate, SharedFunctionInfo shared)
    : ParseInfo(isolate, isolate->allocator(),
                Script::cast(shared.script()).id()) {
  // Do not support re-parsing top-level function of a wrapped script.
  // TODO(yangguo): consider whether we need a top-level function in a
  //                wrapped script at all.
  DCHECK_IMPLIES(is_toplevel(), !Script::cast(shared.script()).is_wrapped());

  set_allow_lazy_parsing(true);
  set_asm_wasm_broken(shared.is_asm_wasm_broken());

  set_start_position(shared.StartPosition());
  set_end_position(shared.EndPosition());
  function_literal_id_ = shared.function_literal_id();
  SetFunctionInfo(&shared);

  Script script = Script::cast(shared.script());
  SetFlagsForFunctionFromScript(script);
  if (shared.is_wrapped()) {
    DCHECK(script.is_wrapped());
    set_wrapped_arguments(handle(script.wrapped_arguments(), isolate));
  }

  if (shared.HasOuterScopeInfo()) {
    set_outer_scope_info(handle(shared.GetOuterScopeInfo(), isolate));
  }

  set_repl_mode(shared.is_repl_mode());

  // CollectTypeProfile uses its own feedback slots. If we have existing
  // FeedbackMetadata, we can only collect type profile if the feedback vector
  // has the appropriate slots.
  set_collect_type_profile(
      isolate->is_collecting_type_profile() &&
      (shared.HasFeedbackMetadata()
           ? shared.feedback_metadata().HasTypeProfileSlot()
           : script.IsUserJavaScript()));
}

ParseInfo::ParseInfo(Isolate* isolate, Script script)
    : ParseInfo(isolate, isolate->allocator(), script.id()) {
  SetFlagsForToplevelCompileFromScript(isolate, script,
                                       isolate->is_collecting_type_profile());
}

// static
std::unique_ptr<ParseInfo> ParseInfo::FromParent(
    const ParseInfo* outer_parse_info, AccountingAllocator* zone_allocator,
    const FunctionLiteral* literal, const AstRawString* function_name) {
  // Can't use make_unique because the constructor is private.
  std::unique_ptr<ParseInfo> result(
      new ParseInfo(zone_allocator, outer_parse_info->script_id_));

  // Replicate shared state of the outer_parse_info.
  result->flags_ = outer_parse_info->flags_;
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

template <typename LocalIsolate>
Handle<Script> ParseInfo::CreateScript(LocalIsolate* isolate,
                                       Handle<String> source,
                                       ScriptOriginOptions origin_options,
                                       NativesFlag natives) {
  // Create a script object describing the script to be compiled.
  DCHECK_GE(script_id_, 0);
  Handle<Script> script =
      isolate->factory()->NewScriptWithId(source, script_id_);
  if (isolate->NeedsSourcePositionsForProfiling()) {
    Script::InitLineEnds(isolate, script);
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
  script->set_is_repl_mode(is_repl_mode());
  if (is_wrapped_as_function()) {
    script->set_wrapped_arguments(*wrapped_arguments());
  } else if (is_eval()) {
    script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
  }

  CheckFlagsForToplevelCompileFromScript(*script,
                                         isolate->is_collecting_type_profile());
  return script;
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<Script> ParseInfo::CreateScript(Isolate* isolate,
                                           Handle<String> source,
                                           ScriptOriginOptions origin_options,
                                           NativesFlag natives);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<Script> ParseInfo::CreateScript(OffThreadIsolate* isolate,
                                           Handle<String> source,
                                           ScriptOriginOptions origin_options,
                                           NativesFlag natives);

AstValueFactory* ParseInfo::GetOrCreateAstValueFactory() {
  if (!ast_value_factory_.get()) {
    ast_value_factory_.reset(
        new AstValueFactory(zone(), ast_string_constants(), hash_seed()));
  }
  return ast_value_factory();
}

void ParseInfo::AllocateSourceRangeMap() {
  DCHECK(block_coverage_enabled());
  DCHECK_NULL(source_range_map());
  set_source_range_map(new (zone()) SourceRangeMap(zone()));
}

void ParseInfo::ResetCharacterStream() { character_stream_.reset(); }

void ParseInfo::set_character_stream(
    std::unique_ptr<Utf16CharacterStream> character_stream) {
  DCHECK_NULL(character_stream_);
  character_stream_.swap(character_stream);
}

void ParseInfo::SetFlagsForToplevelCompile(bool is_collecting_type_profile,
                                           bool is_user_javascript,
                                           LanguageMode language_mode,
                                           REPLMode repl_mode) {
  set_allow_lazy_parsing();
  set_toplevel();
  set_collect_type_profile(is_user_javascript && is_collecting_type_profile);
  set_language_mode(
      stricter_language_mode(this->language_mode(), language_mode));
  set_repl_mode(repl_mode == REPLMode::kYes);

  if (V8_UNLIKELY(is_user_javascript && block_coverage_enabled())) {
    AllocateSourceRangeMap();
  }
}

template <typename LocalIsolate>
void ParseInfo::SetFlagsForToplevelCompileFromScript(
    LocalIsolate* isolate, Script script, bool is_collecting_type_profile) {
  SetFlagsForFunctionFromScript(script);
  SetFlagsForToplevelCompile(is_collecting_type_profile,
                             script.IsUserJavaScript(), language_mode(),
                             construct_repl_mode(script.is_repl_mode()));

  if (script.is_wrapped()) {
    set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
    set_wrapped_arguments(handle(script.wrapped_arguments(), isolate));
  }
}

void ParseInfo::CheckFlagsForToplevelCompileFromScript(
    Script script, bool is_collecting_type_profile) {
  CheckFlagsForFunctionFromScript(script);
  DCHECK(allow_lazy_parsing());
  DCHECK(is_toplevel());
  DCHECK_EQ(collect_type_profile(),
            is_collecting_type_profile && script.IsUserJavaScript());
  DCHECK_EQ(is_repl_mode(), script.is_repl_mode());

  if (script.is_wrapped()) {
    DCHECK_EQ(function_syntax_kind(), FunctionSyntaxKind::kWrapped);
    DCHECK_EQ(*wrapped_arguments(), script.wrapped_arguments());
  }
}

void ParseInfo::SetFlagsForFunctionFromScript(Script script) {
  DCHECK_EQ(script_id_, script.id());

  set_eval(script.compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_module(script.origin_options().IsModule());
  DCHECK(!(is_eval() && is_module()));

  if (block_coverage_enabled() && script.IsUserJavaScript()) {
    AllocateSourceRangeMap();
  }
}

void ParseInfo::CheckFlagsForFunctionFromScript(Script script) {
  DCHECK_EQ(script_id_, script.id());
  // We set "is_eval" for wrapped functions to get an outer declaration scope.
  // This is a bit hacky, but ok since we can't be both eval and wrapped.
  DCHECK_EQ(is_eval() && !is_wrapped_as_function(),
            script.compilation_type() == Script::COMPILATION_TYPE_EVAL);
  DCHECK_EQ(is_module(), script.origin_options().IsModule());
  DCHECK_IMPLIES(block_coverage_enabled() && script.IsUserJavaScript(),
                 source_range_map() != nullptr);
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
