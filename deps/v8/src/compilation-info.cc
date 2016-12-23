// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compilation-info.h"

#include "src/api.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/isolate.h"
#include "src/parsing/parse-info.h"

namespace v8 {
namespace internal {

#define PARSE_INFO_GETTER(type, name)  \
  type CompilationInfo::name() const { \
    CHECK(parse_info());               \
    return parse_info()->name();       \
  }

#define PARSE_INFO_GETTER_WITH_DEFAULT(type, name, def) \
  type CompilationInfo::name() const {                  \
    return parse_info() ? parse_info()->name() : def;   \
  }

PARSE_INFO_GETTER(Handle<Script>, script)
PARSE_INFO_GETTER(FunctionLiteral*, literal)
PARSE_INFO_GETTER_WITH_DEFAULT(DeclarationScope*, scope, nullptr)
PARSE_INFO_GETTER(Handle<SharedFunctionInfo>, shared_info)

#undef PARSE_INFO_GETTER
#undef PARSE_INFO_GETTER_WITH_DEFAULT

bool CompilationInfo::has_shared_info() const {
  return parse_info_ && !parse_info_->shared_info().is_null();
}

CompilationInfo::CompilationInfo(ParseInfo* parse_info,
                                 Handle<JSFunction> closure)
    : CompilationInfo(parse_info, {}, Code::ComputeFlags(Code::FUNCTION), BASE,
                      parse_info->isolate(), parse_info->zone()) {
  closure_ = closure;

  // Compiling for the snapshot typically results in different code than
  // compiling later on. This means that code recompiled with deoptimization
  // support won't be "equivalent" (as defined by SharedFunctionInfo::
  // EnableDeoptimizationSupport), so it will replace the old code and all
  // its type feedback. To avoid this, always compile functions in the snapshot
  // with deoptimization support.
  if (isolate_->serializer_enabled()) EnableDeoptimizationSupport();

  if (FLAG_function_context_specialization) MarkAsFunctionContextSpecializing();
  if (FLAG_turbo_source_positions) MarkAsSourcePositionsEnabled();
  if (FLAG_turbo_splitting) MarkAsSplittingEnabled();
}

CompilationInfo::CompilationInfo(Vector<const char> debug_name,
                                 Isolate* isolate, Zone* zone,
                                 Code::Flags code_flags)
    : CompilationInfo(nullptr, debug_name, code_flags, STUB, isolate, zone) {}

CompilationInfo::CompilationInfo(ParseInfo* parse_info,
                                 Vector<const char> debug_name,
                                 Code::Flags code_flags, Mode mode,
                                 Isolate* isolate, Zone* zone)
    : parse_info_(parse_info),
      isolate_(isolate),
      flags_(0),
      code_flags_(code_flags),
      mode_(mode),
      osr_ast_id_(BailoutId::None()),
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
  delete deferred_handles_;
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
bool CompilationInfo::ShouldSelfOptimize() {
  return FLAG_crankshaft &&
         !(literal()->flags() & AstProperties::kDontSelfOptimize) &&
         !literal()->dont_optimize() &&
         literal()->scope()->AllowsLazyCompilation() &&
         !shared_info()->optimization_disabled();
}

void CompilationInfo::ReopenHandlesInNewHandleScope() {
  closure_ = Handle<JSFunction>(*closure_);
}

bool CompilationInfo::has_simple_parameters() {
  return scope()->has_simple_parameters();
}

std::unique_ptr<char[]> CompilationInfo::GetDebugName() const {
  if (parse_info() && parse_info()->literal()) {
    AllowHandleDereference allow_deref;
    return parse_info()->literal()->debug_name()->ToCString();
  }
  if (parse_info() && !parse_info()->shared_info().is_null()) {
    return parse_info()->shared_info()->DebugName()->ToCString();
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
      return StackFrame::WASM;
    case Code::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
    case Code::WASM_TO_JS_FUNCTION:
      return StackFrame::WASM_TO_JS;
    default:
      UNIMPLEMENTED();
      return StackFrame::NONE;
  }
}

int CompilationInfo::GetDeclareGlobalsFlags() const {
  DCHECK(DeclareGlobalsLanguageMode::is_valid(parse_info()->language_mode()));
  return DeclareGlobalsEvalFlag::encode(parse_info()->is_eval()) |
         DeclareGlobalsNativeFlag::encode(parse_info()->is_native()) |
         DeclareGlobalsLanguageMode::encode(parse_info()->language_mode());
}

SourcePositionTableBuilder::RecordingMode
CompilationInfo::SourcePositionRecordingMode() const {
  return parse_info() && parse_info()->is_native()
             ? SourcePositionTableBuilder::OMIT_SOURCE_POSITIONS
             : SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS;
}

bool CompilationInfo::ExpectsJSReceiverAsReceiver() {
  return is_sloppy(parse_info()->language_mode()) && !parse_info()->is_native();
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

void CompilationInfo::SetOptimizing() {
  DCHECK(has_shared_info());
  SetMode(OPTIMIZE);
  optimization_id_ = isolate()->NextOptimizationId();
  code_flags_ = Code::KindField::update(code_flags_, Code::OPTIMIZED_FUNCTION);
}

void CompilationInfo::AddInlinedFunction(
    Handle<SharedFunctionInfo> inlined_function) {
  inlined_functions_.push_back(InlinedFunctionHolder(
      inlined_function, handle(inlined_function->code())));
}

Code::Kind CompilationInfo::output_code_kind() const {
  return Code::ExtractKindFromFlags(code_flags_);
}

}  // namespace internal
}  // namespace v8
