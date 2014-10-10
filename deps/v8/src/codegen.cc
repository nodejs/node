// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/prettyprinter.h"
#include "src/rewriter.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


#if defined(_WIN64)
typedef double (*ModuloFunction)(double, double);
static ModuloFunction modulo_function = NULL;
// Defined in codegen-x64.cc.
ModuloFunction CreateModuloFunction();

void init_modulo_function() {
  modulo_function = CreateModuloFunction();
}


double modulo(double x, double y) {
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  return (*modulo_function)(x, y);
}
#elif defined(_WIN32)

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
  return std::fmod(x, y);
}
#endif  // defined(_WIN64)


#define UNARY_MATH_FUNCTION(name, generator)             \
static UnaryMathFunction fast_##name##_function = NULL;  \
void init_fast_##name##_function() {                     \
  fast_##name##_function = generator;                    \
}                                                        \
double fast_##name(double x) {                           \
  return (*fast_##name##_function)(x);                   \
}

UNARY_MATH_FUNCTION(exp, CreateExpFunction())
UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction())

#undef UNARY_MATH_FUNCTION


void lazily_initialize_fast_exp() {
  if (fast_exp_function == NULL) {
    init_fast_exp_function();
  }
}


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
  bool print_source = false;
  bool print_ast = false;
  const char* ftype;

  if (info->isolate()->bootstrapper()->IsActive()) {
    print_source = FLAG_print_builtin_source;
    print_ast = FLAG_print_builtin_ast;
    ftype = "builtin";
  } else {
    print_source = FLAG_print_source;
    print_ast = FLAG_print_ast;
    ftype = "user-defined";
  }

  if (FLAG_trace_codegen || print_source || print_ast) {
    PrintF("[generating %s code for %s function: ", kind, ftype);
    if (info->IsStub()) {
      const char* name =
          CodeStub::MajorName(info->code_stub()->MajorKey(), true);
      PrintF("%s", name == NULL ? "<unknown>" : name);
    } else {
      AllowDeferredHandleDereference allow_deference_for_trace;
      PrintF("%s", info->function()->debug_name()->ToCString().get());
    }
    PrintF("]\n");
  }

#ifdef DEBUG
  if (!info->IsStub() && print_source) {
    PrintF("--- Source from AST ---\n%s\n",
           PrettyPrinter(info->zone()).PrintProgram(info->function()));
  }

  if (!info->IsStub() && print_ast) {
    PrintF("--- AST ---\n%s\n",
           AstPrinter(info->zone()).PrintProgram(info->function()));
  }
#endif  // DEBUG
}


Handle<Code> CodeGenerator::MakeCodeEpilogue(MacroAssembler* masm,
                                             Code::Flags flags,
                                             CompilationInfo* info) {
  Isolate* isolate = info->isolate();

  // Allocate and install the code.
  CodeDesc desc;
  bool is_crankshafted =
      Code::ExtractKindFromFlags(flags) == Code::OPTIMIZED_FUNCTION ||
      info->IsStub();
  masm->GetCode(&desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, flags, masm->CodeObject(),
                                  false, is_crankshafted,
                                  info->prologue_offset(),
                                  info->is_debug() && !is_crankshafted);
  isolate->counters()->total_compiled_code_size()->Increment(
      code->instruction_size());
  isolate->heap()->IncrementCodeGeneratedBytes(is_crankshafted,
      code->instruction_size());
  return code;
}


void CodeGenerator::PrintCode(Handle<Code> code, CompilationInfo* info) {
#ifdef ENABLE_DISASSEMBLER
  AllowDeferredHandleDereference allow_deference_for_print_code;
  bool print_code = info->isolate()->bootstrapper()->IsActive()
      ? FLAG_print_builtin_code
      : (FLAG_print_code ||
         (info->IsStub() && FLAG_print_code_stubs) ||
         (info->IsOptimizing() && FLAG_print_opt_code));
  if (print_code) {
    // Print the source code if available.
    FunctionLiteral* function = info->function();
    bool print_source = code->kind() == Code::OPTIMIZED_FUNCTION ||
        code->kind() == Code::FUNCTION;

    CodeTracer::Scope tracing_scope(info->isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    if (print_source) {
      Handle<Script> script = info->script();
      if (!script->IsUndefined() && !script->source()->IsUndefined()) {
        os << "--- Raw source ---\n";
        ConsStringIteratorOp op;
        StringCharacterStream stream(String::cast(script->source()),
                                     &op,
                                     function->start_position());
        // fun->end_position() points to the last character in the stream. We
        // need to compensate by adding one to calculate the length.
        int source_len =
            function->end_position() - function->start_position() + 1;
        for (int i = 0; i < source_len; i++) {
          if (stream.HasMore()) {
            os << AsReversiblyEscapedUC16(stream.GetNext());
          }
        }
        os << "\n\n";
      }
    }
    if (info->IsOptimizing()) {
      if (FLAG_print_unopt_code) {
        os << "--- Unoptimized code ---\n";
        info->closure()->shared()->code()->Disassemble(
            function->debug_name()->ToCString().get(), os);
      }
      os << "--- Optimized code ---\n"
         << "optimization_id = " << info->optimization_id() << "\n";
    } else {
      os << "--- Code ---\n";
    }
    if (print_source) {
      os << "source_position = " << function->start_position() << "\n";
    }
    if (info->IsStub()) {
      CodeStub::Major major_key = info->code_stub()->MajorKey();
      code->Disassemble(CodeStub::MajorName(major_key, false), os);
    } else {
      code->Disassemble(function->debug_name()->ToCString().get(), os);
    }
    os << "--- End code ---\n";
  }
#endif  // ENABLE_DISASSEMBLER
}


bool CodeGenerator::RecordPositions(MacroAssembler* masm,
                                    int pos,
                                    bool right_here) {
  if (pos != RelocInfo::kNoPosition) {
    masm->positions_recorder()->RecordStatementPosition(pos);
    masm->positions_recorder()->RecordPosition(pos);
    if (right_here) {
      return masm->positions_recorder()->WriteRecordedPositions();
    }
  }
  return false;
}

} }  // namespace v8::internal
