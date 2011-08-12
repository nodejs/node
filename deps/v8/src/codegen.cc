// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "bootstrapper.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "prettyprinter.h"
#include "rewriter.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

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


void CodeGenerator::MakeCodePrologue(CompilationInfo* info) {
#ifdef DEBUG
  bool print_source = false;
  bool print_ast = false;
  bool print_json_ast = false;
  const char* ftype;

  if (Isolate::Current()->bootstrapper()->IsActive()) {
    print_source = FLAG_print_builtin_source;
    print_ast = FLAG_print_builtin_ast;
    print_json_ast = FLAG_print_builtin_json_ast;
    ftype = "builtin";
  } else {
    print_source = FLAG_print_source;
    print_ast = FLAG_print_ast;
    print_json_ast = FLAG_print_json_ast;
    Vector<const char> filter = CStrVector(FLAG_hydrogen_filter);
    if (print_source && !filter.is_empty()) {
      print_source = info->function()->name()->IsEqualTo(filter);
    }
    if (print_ast && !filter.is_empty()) {
      print_ast = info->function()->name()->IsEqualTo(filter);
    }
    if (print_json_ast && !filter.is_empty()) {
      print_json_ast = info->function()->name()->IsEqualTo(filter);
    }
    ftype = "user-defined";
  }

  if (FLAG_trace_codegen || print_source || print_ast) {
    PrintF("*** Generate code for %s function: ", ftype);
    info->function()->name()->ShortPrint();
    PrintF(" ***\n");
  }

  if (print_source) {
    PrintF("--- Source from AST ---\n%s\n",
           PrettyPrinter().PrintProgram(info->function()));
  }

  if (print_ast) {
    PrintF("--- AST ---\n%s\n",
           AstPrinter().PrintProgram(info->function()));
  }

  if (print_json_ast) {
    JsonAstBuilder builder;
    PrintF("%s", builder.BuildProgram(info->function()));
  }
#endif  // DEBUG
}


Handle<Code> CodeGenerator::MakeCodeEpilogue(MacroAssembler* masm,
                                             Code::Flags flags,
                                             CompilationInfo* info) {
  Isolate* isolate = info->isolate();

  // Allocate and install the code.
  CodeDesc desc;
  masm->GetCode(&desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, flags, masm->CodeObject());

  if (!code.is_null()) {
    isolate->counters()->total_compiled_code_size()->Increment(
        code->instruction_size());
  }
  return code;
}


void CodeGenerator::PrintCode(Handle<Code> code, CompilationInfo* info) {
#ifdef ENABLE_DISASSEMBLER
  bool print_code = Isolate::Current()->bootstrapper()->IsActive()
      ? FLAG_print_builtin_code
      : (FLAG_print_code || (info->IsOptimizing() && FLAG_print_opt_code));
  Vector<const char> filter = CStrVector(FLAG_hydrogen_filter);
  FunctionLiteral* function = info->function();
  bool match = filter.is_empty() || function->debug_name()->IsEqualTo(filter);
  if (print_code && match) {
    // Print the source code if available.
    Handle<Script> script = info->script();
    if (!script->IsUndefined() && !script->source()->IsUndefined()) {
      PrintF("--- Raw source ---\n");
      StringInputBuffer stream(String::cast(script->source()));
      stream.Seek(function->start_position());
      // fun->end_position() points to the last character in the stream. We
      // need to compensate by adding one to calculate the length.
      int source_len =
          function->end_position() - function->start_position() + 1;
      for (int i = 0; i < source_len; i++) {
        if (stream.has_more()) PrintF("%c", stream.GetNext());
      }
      PrintF("\n\n");
    }
    if (info->IsOptimizing()) {
      if (FLAG_print_unopt_code) {
        PrintF("--- Unoptimized code ---\n");
        info->closure()->shared()->code()->Disassemble(
            *function->debug_name()->ToCString());
      }
      PrintF("--- Optimized code ---\n");
    } else {
      PrintF("--- Code ---\n");
    }
    code->Disassemble(*function->debug_name()->ToCString());
  }
#endif  // ENABLE_DISASSEMBLER
}


bool CodeGenerator::ShouldGenerateLog(Expression* type) {
  ASSERT(type != NULL);
  Isolate* isolate = Isolate::Current();
  if (!isolate->logger()->is_logging() && !CpuProfiler::is_profiling(isolate)) {
    return false;
  }
  Handle<String> name = Handle<String>::cast(type->AsLiteral()->handle());
  if (FLAG_log_regexp) {
    if (name->IsEqualTo(CStrVector("regexp")))
      return true;
  }
  return false;
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


void ArgumentsAccessStub::Generate(MacroAssembler* masm) {
  switch (type_) {
    case READ_ELEMENT:
      GenerateReadElement(masm);
      break;
    case NEW_NON_STRICT_FAST:
      GenerateNewNonStrictFast(masm);
      break;
    case NEW_NON_STRICT_SLOW:
      GenerateNewNonStrictSlow(masm);
      break;
    case NEW_STRICT:
      GenerateNewStrict(masm);
      break;
  }
}


int CEntryStub::MinorKey() {
  ASSERT(result_size_ == 1 || result_size_ == 2);
  int result = save_doubles_ ? 1 : 0;
#ifdef _WIN64
  return result | ((result_size_ == 1) ? 0 : 2);
#else
  return result;
#endif
}


} }  // namespace v8::internal
