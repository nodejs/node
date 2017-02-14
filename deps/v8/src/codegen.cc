// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen.h"

#if defined(V8_OS_AIX)
#include <fenv.h>  // NOLINT(build/c++11)
#endif

#include <memory>

#include "src/ast/prettyprinter.h"
#include "src/bootstrapper.h"
#include "src/compilation-info.h"
#include "src/debug/debug.h"
#include "src/eh-frame.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


#if defined(V8_OS_WIN)
double modulo(double x, double y) {
  // Workaround MS fmod bugs. ECMA-262 says:
  // dividend is finite and divisor is an infinity => result equals dividend
  // dividend is a zero and divisor is nonzero finite => result equals dividend
  if (!(std::isfinite(x) && (!std::isfinite(y) && !std::isnan(y))) &&
      !(x == 0 && (y != 0 && std::isfinite(y)))) {
    x = fmod(x, y);
  }
  return x;
}
#else  // POSIX

double modulo(double x, double y) {
#if defined(V8_OS_AIX)
  // AIX raises an underflow exception for (Number.MIN_VALUE % Number.MAX_VALUE)
  feclearexcept(FE_ALL_EXCEPT);
  double result = std::fmod(x, y);
  int exception = fetestexcept(FE_UNDERFLOW);
  return (exception ? x : result);
#else
  return std::fmod(x, y);
#endif
}
#endif  // defined(V8_OS_WIN)


#define UNARY_MATH_FUNCTION(name, generator)                             \
  static UnaryMathFunctionWithIsolate fast_##name##_function = nullptr;  \
  double std_##name(double x, Isolate* isolate) { return std::name(x); } \
  void init_fast_##name##_function(Isolate* isolate) {                   \
    if (FLAG_fast_math) fast_##name##_function = generator(isolate);     \
    if (!fast_##name##_function) fast_##name##_function = std_##name;    \
  }                                                                      \
  void lazily_initialize_fast_##name(Isolate* isolate) {                 \
    if (!fast_##name##_function) init_fast_##name##_function(isolate);   \
  }                                                                      \
  double fast_##name(double x, Isolate* isolate) {                       \
    return (*fast_##name##_function)(x, isolate);                        \
  }

UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction)

#undef UNARY_MATH_FUNCTION


#define __ ACCESS_MASM(masm_)

#ifdef DEBUG

Comment::Comment(MacroAssembler* masm, const char* msg)
    : masm_(masm), msg_(msg) {
  __ RecordComment(msg);
}


Comment::~Comment() {
  if (msg_[0] == '[') __ RecordComment("]");
}

#endif  // DEBUG

#undef __


void CodeGenerator::MakeCodePrologue(CompilationInfo* info, const char* kind) {
  bool print_ast = false;
  const char* ftype;

  if (info->isolate()->bootstrapper()->IsActive()) {
    print_ast = FLAG_print_builtin_ast;
    ftype = "builtin";
  } else {
    print_ast = FLAG_print_ast;
    ftype = "user-defined";
  }

  if (FLAG_trace_codegen || print_ast) {
    std::unique_ptr<char[]> name = info->GetDebugName();
    PrintF("[generating %s code for %s function: %s]\n", kind, ftype,
           name.get());
  }

#ifdef DEBUG
  if (info->parse_info() && print_ast) {
    PrintF("--- AST ---\n%s\n",
           AstPrinter(info->isolate()).PrintProgram(info->literal()));
  }
#endif  // DEBUG
}

Handle<Code> CodeGenerator::MakeCodeEpilogue(MacroAssembler* masm,
                                             EhFrameWriter* eh_frame_writer,
                                             CompilationInfo* info,
                                             Handle<Object> self_reference) {
  Isolate* isolate = info->isolate();

  // Allocate and install the code.
  CodeDesc desc;
  Code::Flags flags = info->code_flags();
  bool is_crankshafted =
      Code::ExtractKindFromFlags(flags) == Code::OPTIMIZED_FUNCTION ||
      info->IsStub();
  masm->GetCode(&desc);
  if (eh_frame_writer) eh_frame_writer->GetEhFrame(&desc);

  Handle<Code> code = isolate->factory()->NewCode(
      desc, flags, self_reference, false, is_crankshafted,
      info->prologue_offset(), info->is_debug() && !is_crankshafted);
  isolate->counters()->total_compiled_code_size()->Increment(
      code->instruction_size());
  isolate->heap()->IncrementCodeGeneratedBytes(is_crankshafted,
      code->instruction_size());
  return code;
}


void CodeGenerator::PrintCode(Handle<Code> code, CompilationInfo* info) {
#ifdef ENABLE_DISASSEMBLER
  AllowDeferredHandleDereference allow_deference_for_print_code;
  Isolate* isolate = info->isolate();
  bool print_code =
      isolate->bootstrapper()->IsActive()
          ? FLAG_print_builtin_code
          : (FLAG_print_code || (info->IsStub() && FLAG_print_code_stubs) ||
             (info->IsOptimizing() && FLAG_print_opt_code &&
              info->shared_info()->PassesFilter(FLAG_print_opt_code_filter)));
  if (print_code) {
    std::unique_ptr<char[]> debug_name = info->GetDebugName();
    CodeTracer::Scope tracing_scope(info->isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());

    // Print the source code if available.
    bool print_source =
        info->parse_info() && (code->kind() == Code::OPTIMIZED_FUNCTION ||
                               code->kind() == Code::FUNCTION);
    if (print_source) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      Handle<Script> script = info->script();
      if (!script->IsUndefined(isolate) &&
          !script->source()->IsUndefined(isolate)) {
        os << "--- Raw source ---\n";
        StringCharacterStream stream(String::cast(script->source()),
                                     shared->start_position());
        // fun->end_position() points to the last character in the stream. We
        // need to compensate by adding one to calculate the length.
        int source_len = shared->end_position() - shared->start_position() + 1;
        for (int i = 0; i < source_len; i++) {
          if (stream.HasMore()) {
            os << AsReversiblyEscapedUC16(stream.GetNext());
          }
        }
        os << "\n\n";
      }
    }
    if (info->IsOptimizing()) {
      if (FLAG_print_unopt_code && info->parse_info()) {
        os << "--- Unoptimized code ---\n";
        info->closure()->shared()->code()->Disassemble(debug_name.get(), os);
      }
      os << "--- Optimized code ---\n"
         << "optimization_id = " << info->optimization_id() << "\n";
    } else {
      os << "--- Code ---\n";
    }
    if (print_source) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      os << "source_position = " << shared->start_position() << "\n";
    }
    code->Disassemble(debug_name.get(), os);
    os << "--- End code ---\n";
  }
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace internal
}  // namespace v8
