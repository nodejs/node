// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/compiler-dispatcher/lazy-compile-dispatcher.h"
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
      function_syntax_kind_(FunctionSyntaxKind::kDeclaration),
      parsing_while_debugging_(ParsingWhileDebugging::kNo) {
  set_collect_type_profile(isolate->is_collecting_type_profile());
  set_coverage_enabled(!isolate->is_best_effort_code_coverage());
  set_block_coverage_enabled(isolate->is_block_code_coverage());
  set_might_always_opt(FLAG_always_opt || FLAG_prepare_always_opt);
  set_allow_natives_syntax(FLAG_allow_natives_syntax);
  set_allow_lazy_compile(true);
  set_collect_source_positions(!FLAG_enable_lazy_source_positions ||
                               isolate->NeedsDetailedOptimizedCodeLineInfo());
  set_post_parallel_compile_tasks_for_eager_toplevel(
      FLAG_parallel_compile_tasks_for_eager_toplevel);
  set_post_parallel_compile_tasks_for_lazy(
      FLAG_parallel_compile_tasks_for_lazy);
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForFunctionCompile(
    Isolate* isolate, SharedFunctionInfo shared) {
  Script script = Script::cast(shared.script());

  UnoptimizedCompileFlags flags(isolate, script.id());

  flags.SetFlagsFromFunction(&shared);
  flags.SetFlagsForFunctionFromScript(script);
  flags.set_allow_lazy_parsing(true);
  flags.set_is_lazy_compile(true);

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
                                         : ScriptType::kClassic,
      FLAG_lazy);
  if (script.is_wrapped()) {
    flags.set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
  }

  return flags;
}

// static
UnoptimizedCompileFlags UnoptimizedCompileFlags::ForToplevelCompile(
    Isolate* isolate, bool is_user_javascript, LanguageMode language_mode,
    REPLMode repl_mode, ScriptType type, bool lazy) {
  UnoptimizedCompileFlags flags(isolate, isolate->GetNextScriptId());
  flags.SetFlagsForToplevelCompile(isolate->is_collecting_type_profile(),
                                   is_user_javascript, language_mode, repl_mode,
                                   type, lazy);

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
  set_private_name_lookup_skips_outer_class(
      function->private_name_lookup_skips_outer_class());
  set_is_toplevel(function->is_toplevel());
}

void UnoptimizedCompileFlags::SetFlagsForToplevelCompile(
    bool is_collecting_type_profile, bool is_user_javascript,
    LanguageMode language_mode, REPLMode repl_mode, ScriptType type,
    bool lazy) {
  set_is_toplevel(true);
  set_allow_lazy_parsing(lazy);
  set_allow_lazy_compile(lazy);
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

ReusableUnoptimizedCompileState::ReusableUnoptimizedCompileState(
    Isolate* isolate)
    : hash_seed_(HashSeed(isolate)),
      allocator_(isolate->allocator()),
      logger_(isolate->logger()),
      dispatcher_(isolate->lazy_compile_dispatcher()),
      ast_string_constants_(isolate->ast_string_constants()),
      ast_raw_string_zone_(allocator_,
                           "unoptimized-compile-ast-raw-string-zone"),
      single_parse_zone_(allocator_, "unoptimized-compile-parse-zone"),
      ast_value_factory_(
          new AstValueFactory(ast_raw_string_zone(), single_parse_zone(),
                              ast_string_constants(), hash_seed())) {}

ReusableUnoptimizedCompileState::ReusableUnoptimizedCompileState(
    LocalIsolate* isolate)
    : hash_seed_(HashSeed(isolate)),
      allocator_(isolate->allocator()),
      logger_(isolate->main_thread_logger()),
      dispatcher_(isolate->lazy_compile_dispatcher()),
      ast_string_constants_(isolate->ast_string_constants()),
      ast_raw_string_zone_(allocator_,
                           "unoptimized-compile-ast-raw-string-zone"),
      single_parse_zone_(allocator_, "unoptimized-compile-parse-zone"),
      ast_value_factory_(
          new AstValueFactory(ast_raw_string_zone(), single_parse_zone(),
                              ast_string_constants(), hash_seed())) {}

ReusableUnoptimizedCompileState::~ReusableUnoptimizedCompileState() = default;

ParseInfo::ParseInfo(const UnoptimizedCompileFlags flags,
                     UnoptimizedCompileState* state,
                     ReusableUnoptimizedCompileState* reusable_state,
                     uintptr_t stack_limit,
                     RuntimeCallStats* runtime_call_stats)
    : flags_(flags),
      state_(state),
      reusable_state_(reusable_state),
      extension_(nullptr),
      script_scope_(nullptr),
      stack_limit_(stack_limit),
      parameters_end_pos_(kNoSourcePosition),
      max_function_literal_id_(kFunctionLiteralIdInvalid),
      character_stream_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(runtime_call_stats),
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
                     UnoptimizedCompileState* state,
                     ReusableUnoptimizedCompileState* reusable_state)
    : ParseInfo(flags, state, reusable_state,
                isolate->stack_guard()->real_climit(),
                isolate->counters()->runtime_call_stats()) {}

ParseInfo::ParseInfo(LocalIsolate* isolate, const UnoptimizedCompileFlags flags,
                     UnoptimizedCompileState* state,
                     ReusableUnoptimizedCompileState* reusable_state,
                     uintptr_t stack_limit)
    : ParseInfo(flags, state, reusable_state, stack_limit,
                isolate->runtime_call_stats()) {}

ParseInfo::~ParseInfo() { reusable_state_->NotifySingleParseCompleted(); }

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

template <typename IsolateT>
Handle<Script> ParseInfo::CreateScript(
    IsolateT* isolate, Handle<String> source,
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

}  // namespace internal
}  // namespace v8
