// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSE_INFO_H_
#define V8_PARSING_PARSE_INFO_H_

#include <map>
#include <memory>
#include <vector>

#include "include/v8.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/parsing/preparsed-scope-data.h"

namespace v8 {

class Extension;

namespace internal {

class AccountingAllocator;
class AstRawString;
class AstStringConstants;
class AstValueFactory;
class DeclarationScope;
class DeferredHandles;
class FunctionLiteral;
class RuntimeCallStats;
class ScriptData;
class SharedFunctionInfo;
class UnicodeCache;
class Utf16CharacterStream;
class Zone;

// A container for the inputs, configuration options, and outputs of parsing.
class V8_EXPORT_PRIVATE ParseInfo : public CompileJobFinishCallback {
 public:
  explicit ParseInfo(AccountingAllocator* zone_allocator);
  ParseInfo(Handle<Script> script);
  ParseInfo(Handle<SharedFunctionInfo> shared);

  // TODO(rmcilroy): Remove once Hydrogen no longer needs this.
  ParseInfo(Handle<SharedFunctionInfo> shared, std::shared_ptr<Zone> zone);

  ~ParseInfo();

  void InitFromIsolate(Isolate* isolate);

  static ParseInfo* AllocateWithoutScript(Handle<SharedFunctionInfo> shared);

  Zone* zone() const { return zone_.get(); }

  std::shared_ptr<Zone> zone_shared() const { return zone_; }

  void set_deferred_handles(std::shared_ptr<DeferredHandles> deferred_handles);
  void set_deferred_handles(DeferredHandles* deferred_handles);
  std::shared_ptr<DeferredHandles> deferred_handles() const {
    return deferred_handles_;
  }

// Convenience accessor methods for flags.
#define FLAG_ACCESSOR(flag, getter, setter)     \
  bool getter() const { return GetFlag(flag); } \
  void setter() { SetFlag(flag); }              \
  void setter(bool val) { SetFlag(flag, val); }

  FLAG_ACCESSOR(kToplevel, is_toplevel, set_toplevel)
  FLAG_ACCESSOR(kEval, is_eval, set_eval)
  FLAG_ACCESSOR(kStrictMode, is_strict_mode, set_strict_mode)
  FLAG_ACCESSOR(kNative, is_native, set_native)
  FLAG_ACCESSOR(kModule, is_module, set_module)
  FLAG_ACCESSOR(kAllowLazyParsing, allow_lazy_parsing, set_allow_lazy_parsing)
  FLAG_ACCESSOR(kAstValueFactoryOwned, ast_value_factory_owned,
                set_ast_value_factory_owned)
  FLAG_ACCESSOR(kIsNamedExpression, is_named_expression,
                set_is_named_expression)
  FLAG_ACCESSOR(kDebug, is_debug, set_is_debug)
  FLAG_ACCESSOR(kSerializing, will_serialize, set_will_serialize)
  FLAG_ACCESSOR(kTailCallEliminationEnabled, is_tail_call_elimination_enabled,
                set_tail_call_elimination_enabled)

#undef FLAG_ACCESSOR

  void set_parse_restriction(ParseRestriction restriction) {
    SetFlag(kParseRestriction, restriction != NO_PARSE_RESTRICTION);
  }

  ParseRestriction parse_restriction() const {
    return GetFlag(kParseRestriction) ? ONLY_SINGLE_FUNCTION_LITERAL
                                      : NO_PARSE_RESTRICTION;
  }

  ScriptCompiler::ExternalSourceStream* source_stream() const {
    return source_stream_;
  }
  void set_source_stream(ScriptCompiler::ExternalSourceStream* source_stream) {
    source_stream_ = source_stream;
  }

  ScriptCompiler::StreamedSource::Encoding source_stream_encoding() const {
    return source_stream_encoding_;
  }
  void set_source_stream_encoding(
      ScriptCompiler::StreamedSource::Encoding source_stream_encoding) {
    source_stream_encoding_ = source_stream_encoding;
  }

  Utf16CharacterStream* character_stream() const { return character_stream_; }
  void set_character_stream(Utf16CharacterStream* character_stream) {
    character_stream_ = character_stream;
  }

  v8::Extension* extension() const { return extension_; }
  void set_extension(v8::Extension* extension) { extension_ = extension; }

  ScriptData** cached_data() const { return cached_data_; }
  void set_cached_data(ScriptData** cached_data) { cached_data_ = cached_data; }

  PreParsedScopeData* preparsed_scope_data() { return &preparsed_scope_data_; }

  ScriptCompiler::CompileOptions compile_options() const {
    return compile_options_;
  }
  void set_compile_options(ScriptCompiler::CompileOptions compile_options) {
    compile_options_ = compile_options;
  }

  DeclarationScope* script_scope() const { return script_scope_; }
  void set_script_scope(DeclarationScope* script_scope) {
    script_scope_ = script_scope;
  }

  DeclarationScope* asm_function_scope() const { return asm_function_scope_; }
  void set_asm_function_scope(DeclarationScope* scope) {
    asm_function_scope_ = scope;
  }

  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }
  void set_ast_value_factory(AstValueFactory* ast_value_factory) {
    ast_value_factory_ = ast_value_factory;
  }

  const AstRawString* function_name() const { return function_name_; }
  void set_function_name(const AstRawString* function_name) {
    function_name_ = function_name;
  }

  FunctionLiteral* literal() const { return literal_; }
  void set_literal(FunctionLiteral* literal) { literal_ = literal; }

  DeclarationScope* scope() const;

  UnicodeCache* unicode_cache() const { return unicode_cache_; }
  void set_unicode_cache(UnicodeCache* unicode_cache) {
    unicode_cache_ = unicode_cache;
  }

  uintptr_t stack_limit() const { return stack_limit_; }
  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }

  uint32_t hash_seed() const { return hash_seed_; }
  void set_hash_seed(uint32_t hash_seed) { hash_seed_ = hash_seed; }

  int compiler_hints() const { return compiler_hints_; }
  void set_compiler_hints(int compiler_hints) {
    compiler_hints_ = compiler_hints;
  }

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

  // Getters for individual compiler hints.
  bool is_declaration() const;
  FunctionKind function_kind() const;

  //--------------------------------------------------------------------------
  // TODO(titzer): these should not be part of ParseInfo.
  //--------------------------------------------------------------------------
  Handle<SharedFunctionInfo> shared_info() const { return shared_; }
  Handle<Script> script() const { return script_; }
  MaybeHandle<ScopeInfo> maybe_outer_scope_info() const {
    return maybe_outer_scope_info_;
  }
  void clear_script() { script_ = Handle<Script>::null(); }
  void set_shared_info(Handle<SharedFunctionInfo> shared) { shared_ = shared; }
  void set_outer_scope_info(Handle<ScopeInfo> outer_scope_info) {
    maybe_outer_scope_info_ = outer_scope_info;
  }
  void set_script(Handle<Script> script) { script_ = script; }
  //--------------------------------------------------------------------------

  LanguageMode language_mode() const {
    return construct_language_mode(is_strict_mode());
  }
  void set_language_mode(LanguageMode language_mode) {
    STATIC_ASSERT(LANGUAGE_END == 2);
    set_strict_mode(is_strict(language_mode));
  }

  void ReopenHandlesInNewHandleScope() {
    if (!script_.is_null()) {
      script_ = Handle<Script>(*script_);
    }
    if (!shared_.is_null()) {
      shared_ = Handle<SharedFunctionInfo>(*shared_);
    }
    Handle<ScopeInfo> outer_scope_info;
    if (maybe_outer_scope_info_.ToHandle(&outer_scope_info)) {
      maybe_outer_scope_info_ = Handle<ScopeInfo>(*outer_scope_info);
    }
  }

  void UpdateStatisticsAfterBackgroundParse(Isolate* isolate);

  // The key of the map is the FunctionLiteral's start_position
  std::map<int, ParseInfo*> child_infos() const;

  void ParseFinished(std::unique_ptr<ParseInfo> info) override;

#ifdef DEBUG
  bool script_is_native() const;
#endif  // DEBUG

 private:
  // Various configuration flags for parsing.
  enum Flag {
    // ---------- Input flags ---------------------------
    kToplevel = 1 << 0,
    kLazy = 1 << 1,
    kEval = 1 << 2,
    kStrictMode = 1 << 3,
    kNative = 1 << 4,
    kParseRestriction = 1 << 5,
    kModule = 1 << 6,
    kAllowLazyParsing = 1 << 7,
    kIsNamedExpression = 1 << 8,
    kDebug = 1 << 9,
    kSerializing = 1 << 10,
    kTailCallEliminationEnabled = 1 << 11,
    kAstValueFactoryOwned = 1 << 12,
  };

  //------------- Inputs to parsing and scope analysis -----------------------
  std::shared_ptr<Zone> zone_;
  unsigned flags_;
  ScriptCompiler::ExternalSourceStream* source_stream_;
  ScriptCompiler::StreamedSource::Encoding source_stream_encoding_;
  Utf16CharacterStream* character_stream_;
  v8::Extension* extension_;
  ScriptCompiler::CompileOptions compile_options_;
  DeclarationScope* script_scope_;
  DeclarationScope* asm_function_scope_;
  UnicodeCache* unicode_cache_;
  uintptr_t stack_limit_;
  uint32_t hash_seed_;
  int compiler_hints_;
  int start_position_;
  int end_position_;
  int parameters_end_pos_;
  int function_literal_id_;
  int max_function_literal_id_;

  // TODO(titzer): Move handles out of ParseInfo.
  Handle<SharedFunctionInfo> shared_;
  Handle<Script> script_;
  MaybeHandle<ScopeInfo> maybe_outer_scope_info_;

  //----------- Inputs+Outputs of parsing and scope analysis -----------------
  ScriptData** cached_data_;  // used if available, populated if requested.
  PreParsedScopeData preparsed_scope_data_;
  AstValueFactory* ast_value_factory_;  // used if available, otherwise new.
  const class AstStringConstants* ast_string_constants_;
  const AstRawString* function_name_;
  RuntimeCallStats* runtime_call_stats_;

  //----------- Output of parsing and scope analysis ------------------------
  FunctionLiteral* literal_;
  std::shared_ptr<DeferredHandles> deferred_handles_;

  std::vector<std::unique_ptr<ParseInfo>> child_infos_;
  mutable base::Mutex child_infos_mutex_;

  void SetFlag(Flag f) { flags_ |= f; }
  void SetFlag(Flag f, bool v) { flags_ = v ? flags_ | f : flags_ & ~f; }
  bool GetFlag(Flag f) const { return (flags_ & f) != 0; }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSE_INFO_H_
