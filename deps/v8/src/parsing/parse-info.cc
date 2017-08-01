// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/api.h"
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
      source_stream_(nullptr),
      source_stream_encoding_(ScriptCompiler::StreamedSource::ONE_BYTE),
      character_stream_(nullptr),
      extension_(nullptr),
      compile_options_(ScriptCompiler::kNoCompileOptions),
      script_scope_(nullptr),
      asm_function_scope_(nullptr),
      unicode_cache_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      compiler_hints_(0),
      start_position_(0),
      end_position_(0),
      parameters_end_pos_(kNoSourcePosition),
      function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      max_function_literal_id_(FunctionLiteral::kIdTypeInvalid),
      cached_data_(nullptr),
      ast_value_factory_(nullptr),
      ast_string_constants_(nullptr),
      function_name_(nullptr),
      runtime_call_stats_(nullptr),
      literal_(nullptr),
      deferred_handles_(nullptr) {}

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
  set_shared_info(shared);
  set_module(shared->kind() == FunctionKind::kModule);

  Handle<Script> script(Script::cast(shared->script()));
  set_script(script);
  set_native(script->type() == Script::TYPE_NATIVE);
  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);

  Handle<HeapObject> scope_info(shared->outer_scope_info());
  if (!scope_info->IsTheHole(isolate) &&
      Handle<ScopeInfo>::cast(scope_info)->length() > 0) {
    set_outer_scope_info(Handle<ScopeInfo>::cast(scope_info));
  }
}

ParseInfo::ParseInfo(Handle<SharedFunctionInfo> shared,
                     std::shared_ptr<Zone> zone)
    : ParseInfo(shared) {
  zone_.swap(zone);
}

ParseInfo::ParseInfo(Handle<Script> script)
    : ParseInfo(script->GetIsolate()->allocator()) {
  InitFromIsolate(script->GetIsolate());

  set_allow_lazy_parsing();
  set_toplevel();
  set_script(script);

  set_native(script->type() == Script::TYPE_NATIVE);
  set_eval(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
}

ParseInfo::~ParseInfo() {
  if (ast_value_factory_owned()) {
    delete ast_value_factory_;
    set_ast_value_factory_owned(false);
  }
  ast_value_factory_ = nullptr;
}

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
  p->set_shared_info(shared);
  p->set_module(shared->kind() == FunctionKind::kModule);

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

  Handle<HeapObject> scope_info(shared->outer_scope_info());
  if (!scope_info->IsTheHole(isolate) &&
      Handle<ScopeInfo>::cast(scope_info)->length() > 0) {
    p->set_outer_scope_info(Handle<ScopeInfo>::cast(scope_info));
  }
  return p;
}

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

bool ParseInfo::is_declaration() const {
  return (compiler_hints_ & (1 << SharedFunctionInfo::kIsDeclaration)) != 0;
}

FunctionKind ParseInfo::function_kind() const {
  return SharedFunctionInfo::FunctionKindBits::decode(compiler_hints_);
}

void ParseInfo::set_deferred_handles(
    std::shared_ptr<DeferredHandles> deferred_handles) {
  DCHECK(deferred_handles_.get() == nullptr);
  deferred_handles_.swap(deferred_handles);
}

void ParseInfo::set_deferred_handles(DeferredHandles* deferred_handles) {
  DCHECK(deferred_handles_.get() == nullptr);
  deferred_handles_.reset(deferred_handles);
}

void ParseInfo::InitFromIsolate(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  set_hash_seed(isolate->heap()->HashSeed());
  set_stack_limit(isolate->stack_guard()->real_climit());
  set_unicode_cache(isolate->unicode_cache());
  set_tail_call_elimination_enabled(
      isolate->is_tail_call_elimination_enabled());
  set_runtime_call_stats(isolate->counters()->runtime_call_stats());
  set_ast_string_constants(isolate->ast_string_constants());
}

void ParseInfo::UpdateStatisticsAfterBackgroundParse(Isolate* isolate) {
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

void ParseInfo::ParseFinished(std::unique_ptr<ParseInfo> info) {
  if (info->literal()) {
    base::LockGuard<base::Mutex> access_child_infos(&child_infos_mutex_);
    child_infos_.emplace_back(std::move(info));
  }
}

std::map<int, ParseInfo*> ParseInfo::child_infos() const {
  base::LockGuard<base::Mutex> access_child_infos(&child_infos_mutex_);
  std::map<int, ParseInfo*> rv;
  for (const auto& child_info : child_infos_) {
    DCHECK_NOT_NULL(child_info->literal());
    int start_position = child_info->literal()->start_position();
    rv.insert(std::make_pair(start_position, child_info.get()));
  }
  return rv;
}

#ifdef DEBUG
bool ParseInfo::script_is_native() const {
  return script_->type() == Script::TYPE_NATIVE;
}
#endif  // DEBUG

}  // namespace internal
}  // namespace v8
