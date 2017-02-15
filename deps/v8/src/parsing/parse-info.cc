// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parse-info.h"

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"

namespace v8 {
namespace internal {

ParseInfo::ParseInfo(Zone* zone)
    : zone_(zone),
      flags_(0),
      source_stream_(nullptr),
      source_stream_encoding_(ScriptCompiler::StreamedSource::ONE_BYTE),
      character_stream_(nullptr),
      extension_(nullptr),
      compile_options_(ScriptCompiler::kNoCompileOptions),
      script_scope_(nullptr),
      unicode_cache_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      compiler_hints_(0),
      start_position_(0),
      end_position_(0),
      isolate_(nullptr),
      cached_data_(nullptr),
      ast_value_factory_(nullptr),
      function_name_(nullptr),
      literal_(nullptr) {}

ParseInfo::ParseInfo(Zone* zone, Handle<JSFunction> function)
    : ParseInfo(zone, Handle<SharedFunctionInfo>(function->shared())) {
  if (!function->context()->IsNativeContext()) {
    set_outer_scope_info(handle(function->context()->scope_info()));
  }
}

ParseInfo::ParseInfo(Zone* zone, Handle<SharedFunctionInfo> shared)
    : ParseInfo(zone) {
  isolate_ = shared->GetIsolate();

  set_lazy();
  set_hash_seed(isolate_->heap()->HashSeed());
  set_is_named_expression(shared->is_named_expression());
  set_calls_eval(shared->scope_info()->CallsEval());
  set_compiler_hints(shared->compiler_hints());
  set_start_position(shared->start_position());
  set_end_position(shared->end_position());
  set_stack_limit(isolate_->stack_guard()->real_climit());
  set_unicode_cache(isolate_->unicode_cache());
  set_language_mode(shared->language_mode());
  set_shared_info(shared);

  Handle<Script> script(Script::cast(shared->script()));
  set_script(script);
  if (!script.is_null() && script->type() == Script::TYPE_NATIVE) {
    set_native();
  }
}

ParseInfo::ParseInfo(Zone* zone, Handle<Script> script) : ParseInfo(zone) {
  isolate_ = script->GetIsolate();

  set_hash_seed(isolate_->heap()->HashSeed());
  set_stack_limit(isolate_->stack_guard()->real_climit());
  set_unicode_cache(isolate_->unicode_cache());
  set_script(script);

  if (script->type() == Script::TYPE_NATIVE) {
    set_native();
  }
}

ParseInfo::~ParseInfo() {
  if (ast_value_factory_owned()) {
    delete ast_value_factory_;
    set_ast_value_factory_owned(false);
  }
  ast_value_factory_ = nullptr;
}

DeclarationScope* ParseInfo::scope() const { return literal()->scope(); }

bool ParseInfo::is_declaration() const {
  return (compiler_hints_ & (1 << SharedFunctionInfo::kIsDeclaration)) != 0;
}

bool ParseInfo::requires_class_field_init() const {
  return (compiler_hints_ &
          (1 << SharedFunctionInfo::kRequiresClassFieldInit)) != 0;
}
bool ParseInfo::is_class_field_initializer() const {
  return (compiler_hints_ &
          (1 << SharedFunctionInfo::kIsClassFieldInitializer)) != 0;
}

FunctionKind ParseInfo::function_kind() const {
  return SharedFunctionInfo::FunctionKindBits::decode(compiler_hints_);
}

#ifdef DEBUG
bool ParseInfo::script_is_native() const {
  return script_->type() == Script::TYPE_NATIVE;
}
#endif  // DEBUG

}  // namespace internal
}  // namespace v8
