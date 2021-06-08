// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/base/logging.h"
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

UnoptimizedCompileFlags::UnoptimizedCompileFlags(Isolate* isolate,
                                                 int script_id)
    : flags_(0),
      script_id_(script_id),
      function_kind_(FunctionKind::kNormalFunction),
      function_syntax_kind_(FunctionSyntaxKind::kDeclaration) {
  set_collect_type_profile(isolate->is_collecting_type_profile());
  set_coverage_enabled(!isolate->is_best_effort_code_coverage());
  set_block_coverage_enabled(isolate->is_block_code_coverage());
  set_might_always_opt(FLAG_always_opt || FLAG_prepare_always_opt);
  set_allow_natives_syntax(FLAG_allow_natives_syntax);
  set_allow_lazy_compile(FLAG_lazy);
  set_collect_source_positions(!FLAG_enable_lazy_source_positions ||
                               isolate->NeedsDetailedOptimizedCodeLineInfo());
  set_allow_harmony_top_level_await(FLAG_harmony_top_level_await);
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForFunctionCompile(
    Isolate* isolate, SharedFunctionInfo shared) {
  Script script = Script::cast(shared.script());

  UnoptimizedCompileFlags flags(isolate, script.id());

  flags.SetFlagsFromFunction(&shared);
  flags.SetFlagsForFunctionFromScript(script);

  flags.set_allow_lazy_parsing(true);
#if V8_ENABLE_WEBASSEMBLY
  flags.set_is_asm_wasm_broken(shared.is_asm_wasm_broken());
#endif  // V8_ENABLE_WEBASSEMBLY
  flags.set_is_repl_mode(shared.is_repl_mode());

  // CollectTypeProfile uses its own feedback slots. If we have existing
  // FeedbackMetadata, we can only collect type profile if the feedback vector
  // has the appropriate slots.
  flags.set_collect_type_profile(
      isolate->is_collecting_type_profile() &&
      (shared.HasFeedbackMetadata()
           ? shared.feedback_metadata().HasTypeProfileSlot()
           : script.IsUserJavaScript()));

  // Do not support re-parsing top-level function of a wrapped script.
  DCHECK_IMPLIES(flags.is_toplevel(), !script.is_wrapped());

  return flags;
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForScriptCompile(
    Isolate* isolate, Script script) {
  UnoptimizedCompileFlags flags(isolate, script.id());

  flags.SetFlagsForFunctionFromScript(script);
  flags.SetFlagsForToplevelCompile(
      isolate->is_collecting_type_profile(), script.IsUserJavaScript(),
      flags.outer_language_mode(), construct_repl_mode(script.is_repl_mode()),
      script.origin_options().IsModule() ? ScriptType::kModule
                                         : ScriptType::kClassic);
  if (script.is_wrapped()) {
    flags.set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
  }

  return flags;
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForToplevelCompile(
    Isolate* isolate, bool is_user_javascript, LanguageMode language_mode,
    REPLMode repl_mode, ScriptType type) {
  UnoptimizedCompileFlags flags(isolate, isolate->GetNextScriptId());
  flags.SetFlagsForToplevelCompile(isolate->is_collecting_type_profile(),
                                   is_user_javascript, language_mode, repl_mode,
                                   type);

  LOG(isolate,
      ScriptEvent(Logger::ScriptEventType::kReserveId, flags.script_id()));
  return flags;
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForToplevelFunction(
    const UnoptimizedCompileFlags toplevel_flags,
    const FunctionLiteral* literal) {
  DCHECK(toplevel_flags.is_toplevel());
  DCHECK(!literal->is_toplevel());

  // Replicate the toplevel flags, then setup the function-specific flags.
  UnoptimizedCompileFlags flags = toplevel_flags;
  flags.SetFlagsFromFunction(literal);

  return flags;
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForTest(Isolate* isolate) {
  return UnoptimizedCompileFlags(isolate, Script::kTemporaryScriptId);
}

template <typename T>
void UnoptimizedCompileFlags::SetFlagsFromFunction(T function) {
  set_outer_language_mode(function->language_mode());
  set_function_kind(function->kind());
  set_function_syntax_kind(function->syntax_kind());
  set_requires_instance_members_initializer(
      function->requires_instance_members_initializer());
  set_class_scope_has_private_brand(function->class_scope_has_private_brand());
  set_has_static_private_methods_or_accessors(
      function->has_static_private_methods_or_accessors());
  set_is_toplevel(function->is_toplevel());
  set_is_oneshot_iife(function->is_oneshot_iife());
}

void UnoptimizedCompileFlags::SetFlagsForToplevelCompile(
    bool is_collecting_type_profile, bool is_user_javascript,
    LanguageMode language_mode, REPLMode repl_mode, ScriptType type) {
  set_allow_lazy_parsing(true);
  set_is_toplevel(true);
  set_collect_type_profile(is_user_javascript && is_collecting_type_profile);
  set_outer_language_mode(
      stricter_language_mode(outer_language_mode(), language_mode));
  set_is_repl_mode((repl_mode == REPLMode::kYes));
  set_is_module(type == ScriptType::kModule);
  DCHECK_IMPLIES(is_eval(), !is_module());

  set_block_coverage_enabled(block_coverage_enabled() && is_user_javascript);
}

void UnoptimizedCompileFlags::SetFlagsForFunctionFromScript(Script script) {
  DCHECK_EQ(script_id(), script.id());

  set_is_eval(script.compilation_type() == Script::COMPILATION_TYPE_EVAL);
  set_is_module(script.origin_options().IsModule());
  DCHECK_IMPLIES(is_eval(), !is_module());

  set_block_coverage_enabled(block_coverage_enabled() &&
                             script.IsUserJavaScript());
}

UnoptimizedCompileState::UnoptimizedCompileState(Isolate* isolate)
    : hash_seed_(HashSeed(isolate)),
      allocator_(isolate->allocator()),
      ast_string_constants_(isolate->ast_string_constants()),
      logger_(isolate->logger()),
      parallel_tasks_(isolate->compiler_dispatcher()->IsEnabled()
                          ? new ParallelTasks(isolate->compiler_dispatcher())
                          : nullptr) {}

UnoptimizedCompileState::UnoptimizedCompileState(
    const UnoptimizedCompileState& other) V8_NOEXCEPT
    : hash_seed_(other.hash_seed()),
      allocator_(other.allocator()),
      ast_string_constants_(other.ast_string_constants()),
      logger_(other.logger()),
      // TODO(leszeks): Should this create a new ParallelTasks instance?
      parallel_tasks_(nullptr) {}

ParseInfo::ParseInfo(const UnoptimizedCompileFlags flags,
                     UnoptimizedCompileState* state)
    : flags_(flags),
      state_(state),
      zone_(std::make_unique<Zone>(state->allocator(), "parser-zone")),
      extension_(nullptr),
      script_scope_(nullptr),
      stack_limit_(0),
      parameters_end_pos_(kNoSourcePosition),
      max_function_literal_id_(kFunctionLiteralIdInvalid),
      character_stream_(nullptr),
      ast_value_factory_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(nullptr),
      source_range_map_(nullptr),
      literal_(nullptr),
      allow_eval_cache_(false),
#if V8_ENABLE_WEBASSEMBLY
      contains_asm_module_(false),
#endif  // V8_ENABLE_WEBASSEMBLY
      language_mode_(flags.outer_language_mode()) {
  if (flags.block_coverage_enabled()) {
    AllocateSourceRangeMap();
  }
}

ParseInfo::ParseInfo(Isolate* isolate, const UnoptimizedCompileFlags flags,
                     UnoptimizedCompileState* state)
    : ParseInfo(flags, state) {
  SetPerThreadState(isolate->stack_guard()->real_climit(),
                    isolate->counters()->runtime_call_stats());
}

// static
std::unique_ptr<ParseInfo> ParseInfo::ForToplevelFunction(
    const UnoptimizedCompileFlags flags, UnoptimizedCompileState* compile_state,
    const FunctionLiteral* literal, const AstRawString* function_name) {
  std::unique_ptr<ParseInfo> result(new ParseInfo(flags, compile_state));

  // Clone the function_name AstRawString into the ParseInfo's own
  // AstValueFactory.
  const AstRawString* cloned_function_name =
      result->GetOrCreateAstValueFactory()->CloneFromOtherFactory(
          function_name);

  // Setup function specific details.
  DCHECK(!literal->is_toplevel());
  result->set_function_name(cloned_function_name);

  return result;
}

ParseInfo::~ParseInfo() = default;

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

template <typename LocalIsolate>
Handle<Script> ParseInfo::CreateScript(
    LocalIsolate* isolate, Handle<String> source,
    MaybeHandle<FixedArray> maybe_wrapped_arguments,
    ScriptOriginOptions origin_options, NativesFlag natives) {
  // Create a script object describing the script to be compiled.
  DCHECK(flags().script_id() >= 0 ||
         flags().script_id() == Script::kTemporaryScriptId);
  Handle<Script> script =
      isolate->factory()->NewScriptWithId(source, flags().script_id());
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
  script->set_is_repl_mode(flags().is_repl_mode());

  DCHECK_EQ(is_wrapped_as_function(), !maybe_wrapped_arguments.is_null());
  if (is_wrapped_as_function()) {
    script->set_wrapped_arguments(*maybe_wrapped_arguments.ToHandleChecked());
  } else if (flags().is_eval()) {
    script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
  }

  CheckFlagsForToplevelCompileFromScript(*script,
                                         isolate->is_collecting_type_profile());
  return script;
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<Script> ParseInfo::CreateScript(
        Isolate* isolate, Handle<String> source,
        MaybeHandle<FixedArray> maybe_wrapped_arguments,
        ScriptOriginOptions origin_options, NativesFlag natives);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Handle<Script> ParseInfo::CreateScript(
        LocalIsolate* isolate, Handle<String> source,
        MaybeHandle<FixedArray> maybe_wrapped_arguments,
        ScriptOriginOptions origin_options, NativesFlag natives);

AstValueFactory* ParseInfo::GetOrCreateAstValueFactory() {
  if (!ast_value_factory_.get()) {
    ast_value_factory_.reset(
        new AstValueFactory(zone(), ast_string_constants(), hash_seed()));
  }
  return ast_value_factory();
}

void ParseInfo::AllocateSourceRangeMap() {
  DCHECK(flags().block_coverage_enabled());
  DCHECK_NULL(source_range_map());
  set_source_range_map(zone()->New<SourceRangeMap>(zone()));
}

void ParseInfo::ResetCharacterStream() { character_stream_.reset(); }

void ParseInfo::set_character_stream(
    std::unique_ptr<Utf16CharacterStream> character_stream) {
  DCHECK_NULL(character_stream_);
  character_stream_.swap(character_stream);
}

void ParseInfo::CheckFlagsForToplevelCompileFromScript(
    Script script, bool is_collecting_type_profile) {
  CheckFlagsForFunctionFromScript(script);
  DCHECK(flags().allow_lazy_parsing());
  DCHECK(flags().is_toplevel());
  DCHECK_EQ(flags().collect_type_profile(),
            is_collecting_type_profile && script.IsUserJavaScript());
  DCHECK_EQ(flags().is_repl_mode(), script.is_repl_mode());

  if (script.is_wrapped()) {
    DCHECK_EQ(flags().function_syntax_kind(), FunctionSyntaxKind::kWrapped);
  }
}

void ParseInfo::CheckFlagsForFunctionFromScript(Script script) {
  DCHECK_EQ(flags().script_id(), script.id());
  // We set "is_eval" for wrapped scripts to get an outer declaration scope.
  // This is a bit hacky, but ok since we can't be both eval and wrapped.
  DCHECK_EQ(flags().is_eval() && !script.is_wrapped(),
            script.compilation_type() == Script::COMPILATION_TYPE_EVAL);
  DCHECK_EQ(flags().is_module(), script.origin_options().IsModule());
  DCHECK_IMPLIES(flags().block_coverage_enabled() && script.IsUserJavaScript(),
                 source_range_map() != nullptr);
}

void UnoptimizedCompileState::ParallelTasks::Enqueue(
    ParseInfo* outer_parse_info, const AstRawString* function_name,
    FunctionLiteral* literal) {
  base::Optional<CompilerDispatcher::JobId> job_id =
      dispatcher_->Enqueue(outer_parse_info, function_name, literal);
  if (job_id) {
    enqueued_jobs_.emplace_front(std::make_pair(literal, *job_id));
  }
}

}  // namespace internal
}  // namespace v8
