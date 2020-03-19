// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSE_INFO_H_
#define V8_PARSING_PARSE_INFO_H_

#include <map>
#include <memory>
#include <vector>

#include "include/v8.h"
#include "src/base/export-template.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/function-kind.h"
#include "src/objects/function-syntax-kind.h"
#include "src/objects/script.h"
#include "src/parsing/pending-compilation-error-handler.h"
#include "src/parsing/preparse-data.h"

namespace v8 {

class Extension;

namespace internal {

class AccountingAllocator;
class AstRawString;
class AstStringConstants;
class AstValueFactory;
class CompilerDispatcher;
class DeclarationScope;
class FunctionLiteral;
class RuntimeCallStats;
class Logger;
class SourceRangeMap;
class Utf16CharacterStream;
class Zone;

// A container for the inputs, configuration options, and outputs of parsing.
class V8_EXPORT_PRIVATE ParseInfo {
 public:
  explicit ParseInfo(Isolate*);
  ParseInfo(Isolate* isolate, Script script);
  ParseInfo(Isolate* isolate, SharedFunctionInfo shared);

  // Creates a new parse info based on parent top-level |outer_parse_info| for
  // function |literal|.
  static std::unique_ptr<ParseInfo> FromParent(
      const ParseInfo* outer_parse_info, AccountingAllocator* zone_allocator,
      const FunctionLiteral* literal, const AstRawString* function_name);

  ~ParseInfo();

  template <typename LocalIsolate>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Handle<Script> CreateScript(LocalIsolate* isolate, Handle<String> source,
                              ScriptOriginOptions origin_options,
                              NativesFlag natives = NOT_NATIVES_CODE);

  // Either returns the ast-value-factory associcated with this ParseInfo, or
  // creates and returns a new factory if none exists.
  AstValueFactory* GetOrCreateAstValueFactory();

  Zone* zone() const { return zone_.get(); }

// Convenience accessor methods for flags.
#define FLAG_ACCESSOR(flag, getter, setter)     \
  bool getter() const { return GetFlag(flag); } \
  void setter() { SetFlag(flag); }              \
  void setter(bool val) { SetFlag(flag, val); }

  FLAG_ACCESSOR(kToplevel, is_toplevel, set_toplevel)
  FLAG_ACCESSOR(kEager, is_eager, set_eager)
  FLAG_ACCESSOR(kEval, is_eval, set_eval)
  FLAG_ACCESSOR(kStrictMode, is_strict_mode, set_strict_mode)
  FLAG_ACCESSOR(kModule, is_module, set_module)
  FLAG_ACCESSOR(kAllowLazyParsing, allow_lazy_parsing, set_allow_lazy_parsing)
  FLAG_ACCESSOR(kLazyCompile, lazy_compile, set_lazy_compile)
  FLAG_ACCESSOR(kCollectTypeProfile, collect_type_profile,
                set_collect_type_profile)
  FLAG_ACCESSOR(kIsAsmWasmBroken, is_asm_wasm_broken, set_asm_wasm_broken)
  FLAG_ACCESSOR(kContainsAsmModule, contains_asm_module,
                set_contains_asm_module)
  FLAG_ACCESSOR(kCoverageEnabled, coverage_enabled, set_coverage_enabled)
  FLAG_ACCESSOR(kBlockCoverageEnabled, block_coverage_enabled,
                set_block_coverage_enabled)
  FLAG_ACCESSOR(kAllowEvalCache, allow_eval_cache, set_allow_eval_cache)
  FLAG_ACCESSOR(kRequiresInstanceMembersInitializer,
                requires_instance_members_initializer,
                set_requires_instance_members_initializer)
  FLAG_ACCESSOR(kClassScopeHasPrivateBrand, class_scope_has_private_brand,
                set_class_scope_has_private_brand)
  FLAG_ACCESSOR(kHasStaticPrivateMethodsOrAccessors,
                has_static_private_methods_or_accessors,
                set_has_static_private_methods_or_accessors)
  FLAG_ACCESSOR(kMightAlwaysOpt, might_always_opt, set_might_always_opt)
  FLAG_ACCESSOR(kAllowNativeSyntax, allow_natives_syntax,
                set_allow_natives_syntax)
  FLAG_ACCESSOR(kAllowLazyCompile, allow_lazy_compile, set_allow_lazy_compile)
  FLAG_ACCESSOR(kAllowNativeSyntax, allow_native_syntax,
                set_allow_native_syntax)
  FLAG_ACCESSOR(kAllowHarmonyDynamicImport, allow_harmony_dynamic_import,
                set_allow_harmony_dynamic_import)
  FLAG_ACCESSOR(kAllowHarmonyImportMeta, allow_harmony_import_meta,
                set_allow_harmony_import_meta)
  FLAG_ACCESSOR(kAllowHarmonyOptionalChaining, allow_harmony_optional_chaining,
                set_allow_harmony_optional_chaining)
  FLAG_ACCESSOR(kAllowHarmonyPrivateMethods, allow_harmony_private_methods,
                set_allow_harmony_private_methods)
  FLAG_ACCESSOR(kIsOneshotIIFE, is_oneshot_iife, set_is_oneshot_iife)
  FLAG_ACCESSOR(kCollectSourcePositions, collect_source_positions,
                set_collect_source_positions)
  FLAG_ACCESSOR(kAllowHarmonyNullish, allow_harmony_nullish,
                set_allow_harmony_nullish)
  FLAG_ACCESSOR(kAllowHarmonyTopLevelAwait, allow_harmony_top_level_await,
                set_allow_harmony_top_level_await)
  FLAG_ACCESSOR(kREPLMode, is_repl_mode, set_repl_mode)

#undef FLAG_ACCESSOR

  void set_parse_restriction(ParseRestriction restriction) {
    SetFlag(kParseRestriction, restriction != NO_PARSE_RESTRICTION);
  }

  ParseRestriction parse_restriction() const {
    return GetFlag(kParseRestriction) ? ONLY_SINGLE_FUNCTION_LITERAL
                                      : NO_PARSE_RESTRICTION;
  }

  Utf16CharacterStream* character_stream() const {
    return character_stream_.get();
  }
  void set_character_stream(
      std::unique_ptr<Utf16CharacterStream> character_stream);
  void ResetCharacterStream();

  v8::Extension* extension() const { return extension_; }
  void set_extension(v8::Extension* extension) { extension_ = extension; }

  void set_consumed_preparse_data(std::unique_ptr<ConsumedPreparseData> data) {
    consumed_preparse_data_.swap(data);
  }
  ConsumedPreparseData* consumed_preparse_data() {
    return consumed_preparse_data_.get();
  }

  DeclarationScope* script_scope() const { return script_scope_; }
  void set_script_scope(DeclarationScope* script_scope) {
    script_scope_ = script_scope;
  }

  AstValueFactory* ast_value_factory() const {
    DCHECK(ast_value_factory_.get());
    return ast_value_factory_.get();
  }

  const AstRawString* function_name() const { return function_name_; }
  void set_function_name(const AstRawString* function_name) {
    function_name_ = function_name;
  }

  FunctionLiteral* literal() const { return literal_; }
  void set_literal(FunctionLiteral* literal) { literal_ = literal; }

  DeclarationScope* scope() const;

  uintptr_t stack_limit() const { return stack_limit_; }
  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }

  uint64_t hash_seed() const { return hash_seed_; }
  void set_hash_seed(uint64_t hash_seed) { hash_seed_ = hash_seed; }

  int start_position() const { return start_position_; }
  void set_start_position(int start_position) {
    start_position_ = start_position;
  }

  int end_position() const { return end_position_; }
  void set_end_position(int end_position) { end_position_ = end_position; }

  int parameters_end_pos() const { return parameters_end_pos_; }
  void set_parameters_end_pos(int parameters_end_pos) {
    parameters_end_pos_ = parameters_end_pos;
  }

  int function_literal_id() const { return function_literal_id_; }
  void set_function_literal_id(int function_literal_id) {
    function_literal_id_ = function_literal_id;
  }

  FunctionKind function_kind() const { return function_kind_; }
  void set_function_kind(FunctionKind function_kind) {
    function_kind_ = function_kind;
  }

  FunctionSyntaxKind function_syntax_kind() const {
    return function_syntax_kind_;
  }
  void set_function_syntax_kind(FunctionSyntaxKind function_syntax_kind) {
    function_syntax_kind_ = function_syntax_kind;
  }

  bool is_wrapped_as_function() const {
    return function_syntax_kind() == FunctionSyntaxKind::kWrapped;
  }

  int max_function_literal_id() const { return max_function_literal_id_; }
  void set_max_function_literal_id(int max_function_literal_id) {
    max_function_literal_id_ = max_function_literal_id;
  }

  const AstStringConstants* ast_string_constants() const {
    return ast_string_constants_;
  }
  void set_ast_string_constants(
      const AstStringConstants* ast_string_constants) {
    ast_string_constants_ = ast_string_constants;
  }

  RuntimeCallStats* runtime_call_stats() const { return runtime_call_stats_; }
  void set_runtime_call_stats(RuntimeCallStats* runtime_call_stats) {
    runtime_call_stats_ = runtime_call_stats;
  }
  Logger* logger() const { return logger_; }
  void set_logger(Logger* logger) { logger_ = logger; }

  void AllocateSourceRangeMap();
  SourceRangeMap* source_range_map() const { return source_range_map_; }
  void set_source_range_map(SourceRangeMap* source_range_map) {
    source_range_map_ = source_range_map;
  }

  PendingCompilationErrorHandler* pending_error_handler() {
    return &pending_error_handler_;
  }

  class ParallelTasks {
   public:
    explicit ParallelTasks(CompilerDispatcher* compiler_dispatcher)
        : dispatcher_(compiler_dispatcher) {
      DCHECK(dispatcher_);
    }

    void Enqueue(ParseInfo* outer_parse_info, const AstRawString* function_name,
                 FunctionLiteral* literal);

    using EnqueuedJobsIterator =
        std::forward_list<std::pair<FunctionLiteral*, uintptr_t>>::iterator;

    EnqueuedJobsIterator begin() { return enqueued_jobs_.begin(); }
    EnqueuedJobsIterator end() { return enqueued_jobs_.end(); }

    CompilerDispatcher* dispatcher() { return dispatcher_; }

   private:
    CompilerDispatcher* dispatcher_;
    std::forward_list<std::pair<FunctionLiteral*, uintptr_t>> enqueued_jobs_;
  };

  ParallelTasks* parallel_tasks() { return parallel_tasks_.get(); }

  void SetFlagsForToplevelCompile(bool is_collecting_type_profile,
                                  bool is_user_javascript,
                                  LanguageMode language_mode,
                                  REPLMode repl_mode);

  void CheckFlagsForFunctionFromScript(Script script);

  //--------------------------------------------------------------------------
  // TODO(titzer): these should not be part of ParseInfo.
  //--------------------------------------------------------------------------
  Handle<FixedArray> wrapped_arguments() const { return wrapped_arguments_; }
  void set_wrapped_arguments(Handle<FixedArray> wrapped_arguments) {
    wrapped_arguments_ = wrapped_arguments;
  }

  MaybeHandle<ScopeInfo> maybe_outer_scope_info() const {
    return maybe_outer_scope_info_;
  }
  void set_outer_scope_info(Handle<ScopeInfo> outer_scope_info) {
    maybe_outer_scope_info_ = outer_scope_info;
  }

  int script_id() const { return script_id_; }
  //--------------------------------------------------------------------------

  LanguageMode language_mode() const {
    return construct_language_mode(is_strict_mode());
  }
  void set_language_mode(LanguageMode language_mode) {
    STATIC_ASSERT(LanguageModeSize == 2);
    set_strict_mode(is_strict(language_mode));
  }

 private:
  ParseInfo(AccountingAllocator* zone_allocator, int script_id);
  ParseInfo(Isolate*, AccountingAllocator* zone_allocator, int script_id);

  void SetFlagsForFunctionFromScript(Script script);

  template <typename LocalIsolate>
  void SetFlagsForToplevelCompileFromScript(LocalIsolate* isolate,
                                            Script script,
                                            bool is_collecting_type_profile);
  void CheckFlagsForToplevelCompileFromScript(Script script,
                                              bool is_collecting_type_profile);

  // Set function info flags based on those in either FunctionLiteral or
  // SharedFunctionInfo |function|
  template <typename T>
  void SetFunctionInfo(T function);

  // Various configuration flags for parsing.
  enum Flag : uint32_t {
    // ---------- Input flags ---------------------------
    kToplevel = 1u << 0,
    kEager = 1u << 1,
    kEval = 1u << 2,
    kStrictMode = 1u << 3,
    kNative = 1u << 4,
    kParseRestriction = 1u << 5,
    kModule = 1u << 6,
    kAllowLazyParsing = 1u << 7,
    kLazyCompile = 1u << 8,
    kCollectTypeProfile = 1u << 9,
    kCoverageEnabled = 1u << 10,
    kBlockCoverageEnabled = 1u << 11,
    kIsAsmWasmBroken = 1u << 12,
    kAllowEvalCache = 1u << 13,
    kRequiresInstanceMembersInitializer = 1u << 14,
    kContainsAsmModule = 1u << 15,
    kMightAlwaysOpt = 1u << 16,
    kAllowLazyCompile = 1u << 17,
    kAllowNativeSyntax = 1u << 18,
    kAllowHarmonyPublicFields = 1u << 19,
    kAllowHarmonyStaticFields = 1u << 20,
    kAllowHarmonyDynamicImport = 1u << 21,
    kAllowHarmonyImportMeta = 1u << 22,
    kAllowHarmonyOptionalChaining = 1u << 23,
    kHasStaticPrivateMethodsOrAccessors = 1u << 24,
    kAllowHarmonyPrivateMethods = 1u << 25,
    kIsOneshotIIFE = 1u << 26,
    kCollectSourcePositions = 1u << 27,
    kAllowHarmonyNullish = 1u << 28,
    kAllowHarmonyTopLevelAwait = 1u << 29,
    kREPLMode = 1u << 30,
    kClassScopeHasPrivateBrand = 1u << 31,
  };

  //------------- Inputs to parsing and scope analysis -----------------------
  std::unique_ptr<Zone> zone_;
  uint32_t flags_;
  v8::Extension* extension_;
  DeclarationScope* script_scope_;
  uintptr_t stack_limit_;
  uint64_t hash_seed_;
  FunctionKind function_kind_;
  FunctionSyntaxKind function_syntax_kind_;
  int script_id_;
  int start_position_;
  int end_position_;
  int parameters_end_pos_;
  int function_literal_id_;
  int max_function_literal_id_;

  // TODO(titzer): Move handles out of ParseInfo.
  Handle<FixedArray> wrapped_arguments_;
  MaybeHandle<ScopeInfo> maybe_outer_scope_info_;

  //----------- Inputs+Outputs of parsing and scope analysis -----------------
  std::unique_ptr<Utf16CharacterStream> character_stream_;
  std::unique_ptr<ConsumedPreparseData> consumed_preparse_data_;
  std::unique_ptr<AstValueFactory> ast_value_factory_;
  const class AstStringConstants* ast_string_constants_;
  const AstRawString* function_name_;
  RuntimeCallStats* runtime_call_stats_;
  Logger* logger_;
  SourceRangeMap* source_range_map_;  // Used when block coverage is enabled.
  std::unique_ptr<ParallelTasks> parallel_tasks_;

  //----------- Output of parsing and scope analysis ------------------------
  FunctionLiteral* literal_;
  PendingCompilationErrorHandler pending_error_handler_;

  void SetFlag(Flag f) { flags_ |= f; }
  void SetFlag(Flag f, bool v) { flags_ = v ? flags_ | f : flags_ & ~f; }
  bool GetFlag(Flag f) const { return (flags_ & f) != 0; }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSE_INFO_H_
