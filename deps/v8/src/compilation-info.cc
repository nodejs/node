// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compilation-info.h"

#include "src/api.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/debug/debug.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/source-position.h"

namespace v8 {
namespace internal {

// TODO(mvstanton): the Code::OPTIMIZED_FUNCTION constant below is
// bogus, it's just that I've eliminated Code::FUNCTION and there isn't
// a "better" value to put in this place.
CompilationInfo::CompilationInfo(Zone* zone, ParseInfo* parse_info,
                                 FunctionLiteral* literal)
    : CompilationInfo({}, Code::OPTIMIZED_FUNCTION, BASE, zone) {
  // NOTE: The parse_info passed here represents the global information gathered
  // during parsing, but does not represent specific details of the actual
  // function literal being compiled for this CompilationInfo. As such,
  // parse_info->literal() might be different from literal, and only global
  // details of the script being parsed are relevant to this CompilationInfo.
  DCHECK_NOT_NULL(literal);
  literal_ = literal;
  source_range_map_ = parse_info->source_range_map();

  if (parse_info->is_eval()) MarkAsEval();
  if (parse_info->is_native()) MarkAsNative();
  if (parse_info->collect_type_profile()) MarkAsCollectTypeProfile();
}

CompilationInfo::CompilationInfo(Zone* zone, Isolate* isolate,
                                 Handle<SharedFunctionInfo> shared,
                                 Handle<JSFunction> closure)
    : CompilationInfo({}, Code::OPTIMIZED_FUNCTION, OPTIMIZE, zone) {
  shared_info_ = shared;
  closure_ = closure;
  optimization_id_ = isolate->NextOptimizationId();
  dependencies_.reset(new CompilationDependencies(isolate, zone));

  if (FLAG_function_context_specialization) MarkAsFunctionContextSpecializing();
  if (FLAG_turbo_splitting) MarkAsSplittingEnabled();

  // Collect source positions for optimized code when profiling or if debugger
  // is active, to be able to get more precise source positions at the price of
  // more memory consumption.
  if (isolate->NeedsSourcePositionsForProfiling()) {
    MarkAsSourcePositionsEnabled();
  }
}

CompilationInfo::CompilationInfo(Vector<const char> debug_name, Zone* zone,
                                 Code::Kind code_kind)
    : CompilationInfo(debug_name, code_kind, STUB, zone) {}

CompilationInfo::CompilationInfo(Vector<const char> debug_name,
                                 Code::Kind code_kind, Mode mode, Zone* zone)
    : literal_(nullptr),
      source_range_map_(nullptr),
      flags_(FLAG_untrusted_code_mitigations ? kUntrustedCodeMitigations : 0),
      code_kind_(code_kind),
      stub_key_(0),
      builtin_index_(Builtins::kNoBuiltinId),
      mode_(mode),
      osr_offset_(BailoutId::None()),
      feedback_vector_spec_(zone),
      zone_(zone),
      deferred_handles_(nullptr),
      dependencies_(nullptr),
      bailout_reason_(BailoutReason::kNoReason),
      parameter_count_(0),
      optimization_id_(-1),
      debug_name_(debug_name) {}

CompilationInfo::~CompilationInfo() {
  if (GetFlag(kDisableFutureOptimization) && has_shared_info()) {
    shared_info()->DisableOptimization(bailout_reason());
  }
  if (dependencies()) {
    dependencies()->Rollback();
  }
}

DeclarationScope* CompilationInfo::scope() const {
  DCHECK_NOT_NULL(literal_);
  return literal_->scope();
}

int CompilationInfo::num_parameters() const {
  return !IsStub() ? scope()->num_parameters() : parameter_count_;
}

int CompilationInfo::num_parameters_including_this() const {
  return num_parameters() + (is_this_defined() ? 1 : 0);
}

bool CompilationInfo::is_this_defined() const { return !IsStub(); }

void CompilationInfo::set_deferred_handles(
    std::shared_ptr<DeferredHandles> deferred_handles) {
  DCHECK_NULL(deferred_handles_);
  deferred_handles_.swap(deferred_handles);
}

void CompilationInfo::set_deferred_handles(DeferredHandles* deferred_handles) {
  DCHECK_NULL(deferred_handles_);
  deferred_handles_.reset(deferred_handles);
}

void CompilationInfo::ReopenHandlesInNewHandleScope() {
  if (!shared_info_.is_null()) {
    shared_info_ = Handle<SharedFunctionInfo>(*shared_info_);
  }
  if (!closure_.is_null()) {
    closure_ = Handle<JSFunction>(*closure_);
  }
}

bool CompilationInfo::has_simple_parameters() {
  return scope()->has_simple_parameters();
}

std::unique_ptr<char[]> CompilationInfo::GetDebugName() const {
  if (literal()) {
    return literal()->GetDebugName();
  }
  if (!shared_info().is_null()) {
    return shared_info()->DebugName()->ToCString();
  }
  Vector<const char> name_vec = debug_name_;
  if (name_vec.is_empty()) name_vec = ArrayVector("unknown");
  std::unique_ptr<char[]> name(new char[name_vec.length() + 1]);
  memcpy(name.get(), name_vec.start(), name_vec.length());
  name[name_vec.length()] = '\0';
  return name;
}

StackFrame::Type CompilationInfo::GetOutputStackFrameType() const {
  switch (code_kind()) {
    case Code::STUB:
    case Code::BYTECODE_HANDLER:
    case Code::BUILTIN:
      return StackFrame::STUB;
    case Code::WASM_FUNCTION:
      return StackFrame::WASM_COMPILED;
    case Code::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
    case Code::WASM_TO_WASM_FUNCTION:
      return StackFrame::WASM_TO_WASM;
    case Code::WASM_TO_JS_FUNCTION:
      return StackFrame::WASM_TO_JS;
    case Code::WASM_INTERPRETER_ENTRY:
      return StackFrame::WASM_INTERPRETER_ENTRY;
    default:
      UNIMPLEMENTED();
      return StackFrame::NONE;
  }
}

int CompilationInfo::GetDeclareGlobalsFlags() const {
  return DeclareGlobalsEvalFlag::encode(is_eval()) |
         DeclareGlobalsNativeFlag::encode(is_native());
}

SourcePositionTableBuilder::RecordingMode
CompilationInfo::SourcePositionRecordingMode() const {
  return is_native() ? SourcePositionTableBuilder::OMIT_SOURCE_POSITIONS
                     : SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS;
}

bool CompilationInfo::has_context() const { return !closure().is_null(); }

Context* CompilationInfo::context() const {
  return has_context() ? closure()->context() : nullptr;
}

bool CompilationInfo::has_native_context() const {
  return !closure().is_null() && (closure()->native_context() != nullptr);
}

Context* CompilationInfo::native_context() const {
  return has_native_context() ? closure()->native_context() : nullptr;
}

bool CompilationInfo::has_global_object() const { return has_native_context(); }

JSGlobalObject* CompilationInfo::global_object() const {
  return has_global_object() ? native_context()->global_object() : nullptr;
}

int CompilationInfo::AddInlinedFunction(
    Handle<SharedFunctionInfo> inlined_function, SourcePosition pos) {
  int id = static_cast<int>(inlined_functions_.size());
  inlined_functions_.push_back(InlinedFunctionHolder(inlined_function, pos));
  return id;
}

}  // namespace internal
}  // namespace v8
