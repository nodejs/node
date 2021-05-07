// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSE_INFO_H_
#define V8_PARSING_PARSE_INFO_H_

#include <map>
#include <memory>
#include <vector>

#include "include/v8.h"
#include "src/base/bit-field.h"
#include "src/base/export-template.h"
#include "src/base/logging.h"
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

// The flags for a parse + unoptimized compile operation.
#define FLAG_FIELDS(V, _)                                \
  V(is_toplevel, bool, 1, _)                             \
  V(is_eager, bool, 1, _)                                \
  V(is_eval, bool, 1, _)                                 \
  V(outer_language_mode, LanguageMode, 1, _)             \
  V(parse_restriction, ParseRestriction, 1, _)           \
  V(is_module, bool, 1, _)                               \
  V(allow_lazy_parsing, bool, 1, _)                      \
  V(is_lazy_compile, bool, 1, _)                         \
  V(collect_type_profile, bool, 1, _)                    \
  V(coverage_enabled, bool, 1, _)                        \
  V(block_coverage_enabled, bool, 1, _)                  \
  V(is_asm_wasm_broken, bool, 1, _)                      \
  V(class_scope_has_private_brand, bool, 1, _)           \
  V(requires_instance_members_initializer, bool, 1, _)   \
  V(has_static_private_methods_or_accessors, bool, 1, _) \
  V(might_always_opt, bool, 1, _)                        \
  V(allow_natives_syntax, bool, 1, _)                    \
  V(allow_lazy_compile, bool, 1, _)                      \
  V(is_oneshot_iife, bool, 1, _)                         \
  V(collect_source_positions, bool, 1, _)                \
  V(allow_harmony_top_level_await, bool, 1, _)           \
  V(is_repl_mode, bool, 1, _)                            \
  V(allow_harmony_logical_assignment, bool, 1, _)

class V8_EXPORT_PRIVATE UnoptimizedCompileFlags {
 public:
  // Set-up flags for a toplevel compilation.
  static UnoptimizedCompileFlags ForToplevelCompile(
      Isolate* isolate, bool is_user_javascript, LanguageMode language_mode,
      REPLMode repl_mode, ScriptType type = ScriptType::kClassic);

  // Set-up flags for a compiling a particular function (either a lazy compile
  // or a recompile).
  static UnoptimizedCompileFlags ForFunctionCompile(Isolate* isolate,
                                                    SharedFunctionInfo shared);

  // Set-up flags for a full compilation of a given script.
  static UnoptimizedCompileFlags ForScriptCompile(Isolate* isolate,
                                                  Script script);

  // Set-up flags for a parallel toplevel function compilation, based on the
  // flags of an existing toplevel compilation.
  static UnoptimizedCompileFlags ForToplevelFunction(
      const UnoptimizedCompileFlags toplevel_flags,
      const FunctionLiteral* literal);

  // Create flags for a test.
  static UnoptimizedCompileFlags ForTest(Isolate* isolate);

#define FLAG_GET_SET(NAME, TYPE, SIZE, _)                       \
  TYPE NAME() const { return BitFields::NAME::decode(flags_); } \
  UnoptimizedCompileFlags& set_##NAME(TYPE value) {             \
    flags_ = BitFields::NAME::update(flags_, value);            \
    return *this;                                               \
  }

  FLAG_FIELDS(FLAG_GET_SET, _)

  int script_id() const { return script_id_; }
  UnoptimizedCompileFlags& set_script_id(int value) {
    script_id_ = value;
    return *this;
  }

  FunctionKind function_kind() const { return function_kind_; }
  UnoptimizedCompileFlags& set_function_kind(FunctionKind value) {
    function_kind_ = value;
    return *this;
  }

  FunctionSyntaxKind function_syntax_kind() const {
    return function_syntax_kind_;
  }
  UnoptimizedCompileFlags& set_function_syntax_kind(FunctionSyntaxKind value) {
    function_syntax_kind_ = value;
    return *this;
  }

 private:
  struct BitFields {
    DEFINE_BIT_FIELDS(FLAG_FIELDS)
  };

  UnoptimizedCompileFlags(Isolate* isolate, int script_id);

  // Set function info flags based on those in either FunctionLiteral or
  // SharedFunctionInfo |function|
  template <typename T>
  void SetFlagsFromFunction(T function);
  void SetFlagsForToplevelCompile(bool is_collecting_type_profile,
                                  bool is_user_javascript,
                                  LanguageMode language_mode,
                                  REPLMode repl_mode, ScriptType type);
  void SetFlagsForFunctionFromScript(Script script);

  uint32_t flags_;
  int script_id_;
  FunctionKind function_kind_;
  FunctionSyntaxKind function_syntax_kind_;
};

#undef FLAG_FIELDS
class ParseInfo;

// The mutable state for a parse + unoptimized compile operation.
class V8_EXPORT_PRIVATE UnoptimizedCompileState {
 public:
  explicit UnoptimizedCompileState(Isolate*);
  UnoptimizedCompileState(const UnoptimizedCompileState& other) V8_NOEXCEPT;

  class ParallelTasks {
   public:
    explicit ParallelTasks(CompilerDispatcher* compiler_dispatcher)
        : dispatcher_(compiler_dispatcher) {
      DCHECK_NOT_NULL(dispatcher_);
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

  uint64_t hash_seed() const { return hash_seed_; }
  AccountingAllocator* allocator() const { return allocator_; }
  const AstStringConstants* ast_string_constants() const {
    return ast_string_constants_;
  }
  Logger* logger() const { return logger_; }
  PendingCompilationErrorHandler* pending_error_handler() {
    return &pending_error_handler_;
  }
  const PendingCompilationErrorHandler* pending_error_handler() const {
    return &pending_error_handler_;
  }
  ParallelTasks* parallel_tasks() const { return parallel_tasks_.get(); }

 private:
  uint64_t hash_seed_;
  AccountingAllocator* allocator_;
  const AstStringConstants* ast_string_constants_;
  PendingCompilationErrorHandler pending_error_handler_;
  Logger* logger_;
  std::unique_ptr<ParallelTasks> parallel_tasks_;
};

// A container for the inputs, configuration options, and outputs of parsing.
class V8_EXPORT_PRIVATE ParseInfo {
 public:
  ParseInfo(Isolate* isolate, const UnoptimizedCompileFlags flags,
            UnoptimizedCompileState* state);

  // Creates a new parse info based on parent top-level |outer_parse_info| for
  // function |literal|.
  static std::unique_ptr<ParseInfo> ForToplevelFunction(
      const UnoptimizedCompileFlags flags,
      UnoptimizedCompileState* compile_state, const FunctionLiteral* literal,
      const AstRawString* function_name);

  ~ParseInfo();

  template <typename LocalIsolate>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Handle<Script> CreateScript(LocalIsolate* isolate, Handle<String> source,
                              MaybeHandle<FixedArray> maybe_wrapped_arguments,
                              ScriptOriginOptions origin_options,
                              NativesFlag natives = NOT_NATIVES_CODE);

  // Either returns the ast-value-factory associcated with this ParseInfo, or
  // creates and returns a new factory if none exists.
  AstValueFactory* GetOrCreateAstValueFactory();

  Zone* zone() const { return zone_.get(); }

  const UnoptimizedCompileFlags& flags() const { return flags_; }

  // Getters for state.
  uint64_t hash_seed() const { return state_->hash_seed(); }
  AccountingAllocator* allocator() const { return state_->allocator(); }
  const AstStringConstants* ast_string_constants() const {
    return state_->ast_string_constants();
  }
  Logger* logger() const { return state_->logger(); }
  PendingCompilationErrorHandler* pending_error_handler() {
    return state_->pending_error_handler();
  }
  UnoptimizedCompileState::ParallelTasks* parallel_tasks() const {
    return state_->parallel_tasks();
  }
  const UnoptimizedCompileState* state() const { return state_; }

  // Accessors for per-thread state.
  uintptr_t stack_limit() const { return stack_limit_; }
  RuntimeCallStats* runtime_call_stats() const { return runtime_call_stats_; }
  void SetPerThreadState(uintptr_t stack_limit,
                         RuntimeCallStats* runtime_call_stats) {
    stack_limit_ = stack_limit;
    runtime_call_stats_ = runtime_call_stats;
  }

  // Accessor methods for output flags.
  bool allow_eval_cache() const { return allow_eval_cache_; }
  void set_allow_eval_cache(bool value) { allow_eval_cache_ = value; }
  bool contains_asm_module() const { return contains_asm_module_; }
  void set_contains_asm_module(bool value) { contains_asm_module_ = value; }
  LanguageMode language_mode() const { return language_mode_; }
  void set_language_mode(LanguageMode value) { language_mode_ = value; }

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

  int parameters_end_pos() const { return parameters_end_pos_; }
  void set_parameters_end_pos(int parameters_end_pos) {
    parameters_end_pos_ = parameters_end_pos;
  }

  bool is_wrapped_as_function() const {
    return flags().function_syntax_kind() == FunctionSyntaxKind::kWrapped;
  }

  int max_function_literal_id() const { return max_function_literal_id_; }
  void set_max_function_literal_id(int max_function_literal_id) {
    max_function_literal_id_ = max_function_literal_id;
  }

  void AllocateSourceRangeMap();
  SourceRangeMap* source_range_map() const { return source_range_map_; }
  void set_source_range_map(SourceRangeMap* source_range_map) {
    source_range_map_ = source_range_map;
  }

  void CheckFlagsForFunctionFromScript(Script script);

 private:
  ParseInfo(const UnoptimizedCompileFlags flags,
            UnoptimizedCompileState* state);

  void CheckFlagsForToplevelCompileFromScript(Script script,
                                              bool is_collecting_type_profile);

  //------------- Inputs to parsing and scope analysis -----------------------
  const UnoptimizedCompileFlags flags_;
  UnoptimizedCompileState* state_;

  std::unique_ptr<Zone> zone_;
  v8::Extension* extension_;
  DeclarationScope* script_scope_;
  uintptr_t stack_limit_;
  int parameters_end_pos_;
  int max_function_literal_id_;

  //----------- Inputs+Outputs of parsing and scope analysis -----------------
  std::unique_ptr<Utf16CharacterStream> character_stream_;
  std::unique_ptr<ConsumedPreparseData> consumed_preparse_data_;
  std::unique_ptr<AstValueFactory> ast_value_factory_;
  const AstRawString* function_name_;
  RuntimeCallStats* runtime_call_stats_;
  SourceRangeMap* source_range_map_;  // Used when block coverage is enabled.

  //----------- Output of parsing and scope analysis ------------------------
  FunctionLiteral* literal_;
  bool allow_eval_cache_ : 1;
  bool contains_asm_module_ : 1;
  LanguageMode language_mode_ : 1;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSE_INFO_H_
