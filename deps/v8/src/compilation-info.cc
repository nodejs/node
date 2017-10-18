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

CompilationInfo::CompilationInfo(Zone* zone, Isolate* isolate,
                                 ParseInfo* parse_info,
                                 FunctionLiteral* literal)
    : CompilationInfo(parse_info->script(), {},
                      Code::ComputeFlags(Code::FUNCTION), BASE, isolate, zone) {
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
  if (parse_info->will_serialize()) MarkAsSerializing();
}

CompilationInfo::CompilationInfo(Zone* zone, Isolate* isolate,
                                 Handle<Script> script,
                                 Handle<SharedFunctionInfo> shared,
                                 Handle<JSFunction> closure)
    : CompilationInfo(script, {}, Code::ComputeFlags(Code::OPTIMIZED_FUNCTION),
                      OPTIMIZE, isolate, zone) {
  shared_info_ = shared;
  closure_ = closure;
  optimization_id_ = isolate->NextOptimizationId();

  if (FLAG_function_context_specialization) MarkAsFunctionContextSpecializing();
  if (FLAG_turbo_splitting) MarkAsSplittingEnabled();

  // Collect source positions for optimized code when profiling or if debugger
  // is active, to be able to get more precise source positions at the price of
  // more memory consumption.
  if (isolate_->NeedsSourcePositionsForProfiling()) {
    MarkAsSourcePositionsEnabled();
  }
}

CompilationInfo::CompilationInfo(Vector<const char> debug_name,
                                 Isolate* isolate, Zone* zone,
                                 Code::Flags code_flags)
    : CompilationInfo(Handle<Script>::null(), debug_name, code_flags, STUB,
                      isolate, zone) {}

CompilationInfo::CompilationInfo(Handle<Script> script,
                                 Vector<const char> debug_name,
                                 Code::Flags code_flags, Mode mode,
                                 Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      script_(script),
      literal_(nullptr),
      flags_(0),
      code_flags_(code_flags),
      mode_(mode),
      osr_offset_(BailoutId::None()),
      zone_(zone),
      deferred_handles_(nullptr),
      dependencies_(isolate, zone),
      bailout_reason_(kNoReason),
      prologue_offset_(Code::kPrologueOffsetNotSet),
      parameter_count_(0),
      optimization_id_(-1),
      osr_expr_stack_height_(-1),
      debug_name_(debug_name) {}

CompilationInfo::~CompilationInfo() {
  if (GetFlag(kDisableFutureOptimization) && has_shared_info()) {
    shared_info()->DisableOptimization(bailout_reason());
  }
  dependencies()->Rollback();
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

// Primitive functions are unlikely to be picked up by the stack-walking
// profiler, so they trigger their own optimization when they're called
// for the SharedFunctionInfo::kCallsUntilPrimitiveOptimization-th time.
// TODO(6409) Remove when Full-Codegen dies.
bool CompilationInfo::ShouldSelfOptimize() { return false; }

void CompilationInfo::set_deferred_handles(
    std::shared_ptr<DeferredHandles> deferred_handles) {
  DCHECK(deferred_handles_.get() == nullptr);
  deferred_handles_.swap(deferred_handles);
}

void CompilationInfo::set_deferred_handles(DeferredHandles* deferred_handles) {
  DCHECK(deferred_handles_.get() == nullptr);
  deferred_handles_.reset(deferred_handles);
}

void CompilationInfo::ReopenHandlesInNewHandleScope() {
  if (!script_.is_null()) {
    script_ = Handle<Script>(*script_);
  }
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
    AllowHandleDereference allow_deref;
    return literal()->debug_name()->ToCString();
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
  switch (output_code_kind()) {
    case Code::STUB:
    case Code::BYTECODE_HANDLER:
    case Code::HANDLER:
    case Code::BUILTIN:
#define CASE_KIND(kind) case Code::kind:
      IC_KIND_LIST(CASE_KIND)
#undef CASE_KIND
      return StackFrame::STUB;
    case Code::WASM_FUNCTION:
      return StackFrame::WASM_COMPILED;
    case Code::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
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

Code::Kind CompilationInfo::output_code_kind() const {
  return Code::ExtractKindFromFlags(code_flags_);
}

}  // namespace internal
}  // namespace v8
