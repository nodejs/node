// Copyright 2009 the V8 project authors. All rights reserved.
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
#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "oprofile-agent.h"
#include "prettyprinter.h"
#include "register-allocator-inl.h"
#include "rewriter.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "stub-cache.h"
#include "virtual-frame-inl.h"

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


CodeGenerator* CodeGeneratorScope::top_ = NULL;


void CodeGenerator::ProcessDeferred() {
  while (!deferred_.is_empty()) {
    DeferredCode* code = deferred_.RemoveLast();
    ASSERT(masm_ == code->masm());
    // Record position of deferred code stub.
    masm_->RecordStatementPosition(code->statement_position());
    if (code->position() != RelocInfo::kNoPosition) {
      masm_->RecordPosition(code->position());
    }
    // Generate the code.
    Comment cmnt(masm_, code->comment());
    masm_->bind(code->entry_label());
    code->SaveRegisters();
    code->Generate();
    code->RestoreRegisters();
    masm_->jmp(code->exit_label());
  }
}


void CodeGenerator::SetFrame(VirtualFrame* new_frame,
                             RegisterFile* non_frame_registers) {
  RegisterFile saved_counts;
  if (has_valid_frame()) {
    frame_->DetachFromCodeGenerator();
    // The remaining register reference counts are the non-frame ones.
    allocator_->SaveTo(&saved_counts);
  }

  if (new_frame != NULL) {
    // Restore the non-frame register references that go with the new frame.
    allocator_->RestoreFrom(non_frame_registers);
    new_frame->AttachToCodeGenerator();
  }

  frame_ = new_frame;
  saved_counts.CopyTo(non_frame_registers);
}


void CodeGenerator::DeleteFrame() {
  if (has_valid_frame()) {
    frame_->DetachFromCodeGenerator();
    frame_ = NULL;
  }
}


void CodeGenerator::MakeCodePrologue(CompilationInfo* info) {
#ifdef DEBUG
  bool print_source = false;
  bool print_ast = false;
  bool print_json_ast = false;
  const char* ftype;

  if (Bootstrapper::IsActive()) {
    print_source = FLAG_print_builtin_source;
    print_ast = FLAG_print_builtin_ast;
    print_json_ast = FLAG_print_builtin_json_ast;
    ftype = "builtin";
  } else {
    print_source = FLAG_print_source;
    print_ast = FLAG_print_ast;
    print_json_ast = FLAG_print_json_ast;
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
  // Allocate and install the code.
  CodeDesc desc;
  masm->GetCode(&desc);
  ZoneScopeInfo sinfo(info->scope());
  Handle<Code> code =
      Factory::NewCode(desc, &sinfo, flags, masm->CodeObject());

#ifdef ENABLE_DISASSEMBLER
  bool print_code = Bootstrapper::IsActive()
      ? FLAG_print_builtin_code
      : FLAG_print_code;
  if (print_code) {
    // Print the source code if available.
    Handle<Script> script = info->script();
    FunctionLiteral* function = info->function();
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
    PrintF("--- Code ---\n");
    code->Disassemble(*function->name()->ToCString());
  }
#endif  // ENABLE_DISASSEMBLER

  if (!code.is_null()) {
    Counters::total_compiled_code_size.Increment(code->instruction_size());
  }
  return code;
}


// Generate the code. Takes a function literal, generates code for it, assemble
// all the pieces into a Code object. This function is only to be called by
// the compiler.cc code.
Handle<Code> CodeGenerator::MakeCode(CompilationInfo* info) {
  Handle<Script> script = info->script();
  if (!script->IsUndefined() && !script->source()->IsUndefined()) {
    int len = String::cast(script->source())->length();
    Counters::total_old_codegen_source_size.Increment(len);
  }
  MakeCodePrologue(info);
  // Generate code.
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);
  CodeGenerator cgen(&masm);
  CodeGeneratorScope scope(&cgen);
  cgen.Generate(info);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  InLoopFlag in_loop = (cgen.loop_nesting() != 0) ? IN_LOOP : NOT_IN_LOOP;
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, in_loop);
  return MakeCodeEpilogue(cgen.masm(), flags, info);
}


#ifdef ENABLE_LOGGING_AND_PROFILING

bool CodeGenerator::ShouldGenerateLog(Expression* type) {
  ASSERT(type != NULL);
  if (!Logger::is_logging() && !CpuProfiler::is_profiling()) return false;
  Handle<String> name = Handle<String>::cast(type->AsLiteral()->handle());
  if (FLAG_log_regexp) {
    static Vector<const char> kRegexp = CStrVector("regexp");
    if (name->IsEqualTo(kRegexp))
      return true;
  }
  return false;
}

#endif


Handle<Code> CodeGenerator::ComputeCallInitialize(
    int argc,
    InLoopFlag in_loop) {
  if (in_loop == IN_LOOP) {
    // Force the creation of the corresponding stub outside loops,
    // because it may be used when clearing the ICs later - it is
    // possible for a series of IC transitions to lose the in-loop
    // information, and the IC clearing code can't generate a stub
    // that it needs so we need to ensure it is generated already.
    ComputeCallInitialize(argc, NOT_IN_LOOP);
  }
  CALL_HEAP_FUNCTION(
      StubCache::ComputeCallInitialize(argc, in_loop, Code::CALL_IC),
      Code);
}


Handle<Code> CodeGenerator::ComputeKeyedCallInitialize(
    int argc,
    InLoopFlag in_loop) {
  if (in_loop == IN_LOOP) {
    // Force the creation of the corresponding stub outside loops,
    // because it may be used when clearing the ICs later - it is
    // possible for a series of IC transitions to lose the in-loop
    // information, and the IC clearing code can't generate a stub
    // that it needs so we need to ensure it is generated already.
    ComputeKeyedCallInitialize(argc, NOT_IN_LOOP);
  }
  CALL_HEAP_FUNCTION(
      StubCache::ComputeCallInitialize(argc, in_loop, Code::KEYED_CALL_IC),
      Code);
}

void CodeGenerator::ProcessDeclarations(ZoneList<Declaration*>* declarations) {
  int length = declarations->length();
  int globals = 0;
  for (int i = 0; i < length; i++) {
    Declaration* node = declarations->at(i);
    Variable* var = node->proxy()->var();
    Slot* slot = var->slot();

    // If it was not possible to allocate the variable at compile
    // time, we need to "declare" it at runtime to make sure it
    // actually exists in the local context.
    if ((slot != NULL && slot->type() == Slot::LOOKUP) || !var->is_global()) {
      VisitDeclaration(node);
    } else {
      // Count global variables and functions for later processing
      globals++;
    }
  }

  // Return in case of no declared global functions or variables.
  if (globals == 0) return;

  // Compute array of global variable and function declarations.
  Handle<FixedArray> array = Factory::NewFixedArray(2 * globals, TENURED);
  for (int j = 0, i = 0; i < length; i++) {
    Declaration* node = declarations->at(i);
    Variable* var = node->proxy()->var();
    Slot* slot = var->slot();

    if ((slot != NULL && slot->type() == Slot::LOOKUP) || !var->is_global()) {
      // Skip - already processed.
    } else {
      array->set(j++, *(var->name()));
      if (node->fun() == NULL) {
        if (var->mode() == Variable::CONST) {
          // In case this is const property use the hole.
          array->set_the_hole(j++);
        } else {
          array->set_undefined(j++);
        }
      } else {
        Handle<SharedFunctionInfo> function =
            Compiler::BuildFunctionInfo(node->fun(), script(), this);
        // Check for stack-overflow exception.
        if (HasStackOverflow()) return;
        array->set(j++, *function);
      }
    }
  }

  // Invoke the platform-dependent code generator to do the actual
  // declaration the global variables and functions.
  DeclareGlobals(array);
}


// List of special runtime calls which are generated inline. For some of these
// functions the code will be generated inline, and for others a call to a code
// stub will be inlined.

#define INLINE_RUNTIME_ENTRY(Name, argc, ressize)                             \
    {&CodeGenerator::Generate##Name,  "_" #Name, argc},                       \

CodeGenerator::InlineRuntimeLUT CodeGenerator::kInlineRuntimeLUT[] = {
  INLINE_RUNTIME_FUNCTION_LIST(INLINE_RUNTIME_ENTRY)
};

#undef INLINE_RUNTIME_ENTRY

CodeGenerator::InlineRuntimeLUT* CodeGenerator::FindInlineRuntimeLUT(
    Handle<String> name) {
  const int entries_count =
      sizeof(kInlineRuntimeLUT) / sizeof(InlineRuntimeLUT);
  for (int i = 0; i < entries_count; i++) {
    InlineRuntimeLUT* entry = &kInlineRuntimeLUT[i];
    if (name->IsEqualTo(CStrVector(entry->name))) {
      return entry;
    }
  }
  return NULL;
}


bool CodeGenerator::CheckForInlineRuntimeCall(CallRuntime* node) {
  ZoneList<Expression*>* args = node->arguments();
  Handle<String> name = node->name();
  if (name->length() > 0 && name->Get(0) == '_') {
    InlineRuntimeLUT* entry = FindInlineRuntimeLUT(name);
    if (entry != NULL) {
      ((*this).*(entry->method))(args);
      return true;
    }
  }
  return false;
}


bool CodeGenerator::PatchInlineRuntimeEntry(Handle<String> name,
    const CodeGenerator::InlineRuntimeLUT& new_entry,
    CodeGenerator::InlineRuntimeLUT* old_entry) {
  InlineRuntimeLUT* entry = FindInlineRuntimeLUT(name);
  if (entry == NULL) return false;
  if (old_entry != NULL) {
    old_entry->name = entry->name;
    old_entry->method = entry->method;
  }
  entry->name = new_entry.name;
  entry->method = new_entry.method;
  return true;
}


int CodeGenerator::InlineRuntimeCallArgumentsCount(Handle<String> name) {
  CodeGenerator::InlineRuntimeLUT* f =
      CodeGenerator::FindInlineRuntimeLUT(name);
  if (f != NULL) return f->nargs;
  return -1;
}


// Simple condition analysis.  ALWAYS_TRUE and ALWAYS_FALSE represent a
// known result for the test expression, with no side effects.
CodeGenerator::ConditionAnalysis CodeGenerator::AnalyzeCondition(
    Expression* cond) {
  if (cond == NULL) return ALWAYS_TRUE;

  Literal* lit = cond->AsLiteral();
  if (lit == NULL) return DONT_KNOW;

  if (lit->IsTrue()) {
    return ALWAYS_TRUE;
  } else if (lit->IsFalse()) {
    return ALWAYS_FALSE;
  }

  return DONT_KNOW;
}


bool CodeGenerator::RecordPositions(MacroAssembler* masm,
                                    int pos,
                                    bool right_here) {
  if (pos != RelocInfo::kNoPosition) {
    masm->RecordStatementPosition(pos);
    masm->RecordPosition(pos);
    if (right_here) {
      return masm->WriteRecordedPositions();
    }
  }
  return false;
}


void CodeGenerator::CodeForFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) RecordPositions(masm(), fun->start_position(), false);
}


void CodeGenerator::CodeForReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) RecordPositions(masm(), fun->end_position(), false);
}


void CodeGenerator::CodeForStatementPosition(Statement* stmt) {
  if (FLAG_debug_info) RecordPositions(masm(), stmt->statement_pos(), false);
}


void CodeGenerator::CodeForDoWhileConditionPosition(DoWhileStatement* stmt) {
  if (FLAG_debug_info)
    RecordPositions(masm(), stmt->condition_position(), false);
}


void CodeGenerator::CodeForSourcePosition(int pos) {
  if (FLAG_debug_info && pos != RelocInfo::kNoPosition) {
    masm()->RecordPosition(pos);
  }
}


const char* GenericUnaryOpStub::GetName() {
  switch (op_) {
    case Token::SUB:
      return overwrite_
          ? "GenericUnaryOpStub_SUB_Overwrite"
          : "GenericUnaryOpStub_SUB_Alloc";
    case Token::BIT_NOT:
      return overwrite_
          ? "GenericUnaryOpStub_BIT_NOT_Overwrite"
          : "GenericUnaryOpStub_BIT_NOT_Alloc";
    default:
      UNREACHABLE();
      return "<unknown>";
  }
}


void ArgumentsAccessStub::Generate(MacroAssembler* masm) {
  switch (type_) {
    case READ_ELEMENT: GenerateReadElement(masm); break;
    case NEW_OBJECT: GenerateNewObject(masm); break;
  }
}


int CEntryStub::MinorKey() {
  ASSERT(result_size_ <= 2);
#ifdef _WIN64
  return ExitFrameModeBits::encode(mode_)
         | IndirectResultBits::encode(result_size_ > 1);
#else
  return ExitFrameModeBits::encode(mode_);
#endif
}


bool ApiGetterEntryStub::GetCustomCache(Code** code_out) {
  Object* cache = info()->load_stub_cache();
  if (cache->IsUndefined()) {
    return false;
  } else {
    *code_out = Code::cast(cache);
    return true;
  }
}


void ApiGetterEntryStub::SetCustomCache(Code* value) {
  info()->set_load_stub_cache(value);
}


} }  // namespace v8::internal
