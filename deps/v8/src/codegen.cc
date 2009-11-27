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

namespace v8 {
namespace internal {


CodeGenerator* CodeGeneratorScope::top_ = NULL;


DeferredCode::DeferredCode()
    : masm_(CodeGeneratorScope::Current()->masm()),
      statement_position_(masm_->current_statement_position()),
      position_(masm_->current_position()) {
  ASSERT(statement_position_ != RelocInfo::kNoPosition);
  ASSERT(position_ != RelocInfo::kNoPosition);

  CodeGeneratorScope::Current()->AddDeferred(this);
#ifdef DEBUG
  comment_ = "";
#endif

  // Copy the register locations from the code generator's frame.
  // These are the registers that will be spilled on entry to the
  // deferred code and restored on exit.
  VirtualFrame* frame = CodeGeneratorScope::Current()->frame();
  int sp_offset = frame->fp_relative(frame->stack_pointer_);
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    int loc = frame->register_location(i);
    if (loc == VirtualFrame::kIllegalIndex) {
      registers_[i] = kIgnore;
    } else if (frame->elements_[loc].is_synced()) {
      // Needs to be restored on exit but not saved on entry.
      registers_[i] = frame->fp_relative(loc) | kSyncedFlag;
    } else {
      int offset = frame->fp_relative(loc);
      registers_[i] = (offset < sp_offset) ? kPush : offset;
    }
  }
}


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


void CodeGenerator::MakeCodePrologue(FunctionLiteral* fun) {
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
    fun->name()->ShortPrint();
    PrintF(" ***\n");
  }

  if (print_source) {
    PrintF("--- Source from AST ---\n%s\n", PrettyPrinter().PrintProgram(fun));
  }

  if (print_ast) {
    PrintF("--- AST ---\n%s\n", AstPrinter().PrintProgram(fun));
  }

  if (print_json_ast) {
    JsonAstBuilder builder;
    PrintF("%s", builder.BuildProgram(fun));
  }
#endif  // DEBUG
}


Handle<Code> CodeGenerator::MakeCodeEpilogue(FunctionLiteral* fun,
                                             MacroAssembler* masm,
                                             Code::Flags flags,
                                             Handle<Script> script) {
  // Allocate and install the code.
  CodeDesc desc;
  masm->GetCode(&desc);
  ZoneScopeInfo sinfo(fun->scope());
  Handle<Code> code =
      Factory::NewCode(desc, &sinfo, flags, masm->CodeObject());

  // Add unresolved entries in the code to the fixup list.
  Bootstrapper::AddFixup(*code, masm);

#ifdef ENABLE_DISASSEMBLER
  bool print_code = Bootstrapper::IsActive()
      ? FLAG_print_builtin_code
      : FLAG_print_code;
  if (print_code) {
    // Print the source code if available.
    if (!script->IsUndefined() && !script->source()->IsUndefined()) {
      PrintF("--- Raw source ---\n");
      StringInputBuffer stream(String::cast(script->source()));
      stream.Seek(fun->start_position());
      // fun->end_position() points to the last character in the stream. We
      // need to compensate by adding one to calculate the length.
      int source_len = fun->end_position() - fun->start_position() + 1;
      for (int i = 0; i < source_len; i++) {
        if (stream.has_more()) PrintF("%c", stream.GetNext());
      }
      PrintF("\n\n");
    }
    PrintF("--- Code ---\n");
    code->Disassemble(*fun->name()->ToCString());
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
Handle<Code> CodeGenerator::MakeCode(FunctionLiteral* fun,
                                     Handle<Script> script,
                                     bool is_eval) {
  MakeCodePrologue(fun);
  // Generate code.
  const int kInitialBufferSize = 4 * KB;
  CodeGenerator cgen(kInitialBufferSize, script, is_eval);
  CodeGeneratorScope scope(&cgen);
  cgen.GenCode(fun);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  InLoopFlag in_loop = (cgen.loop_nesting() != 0) ? IN_LOOP : NOT_IN_LOOP;
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, in_loop);
  return MakeCodeEpilogue(fun, cgen.masm(), flags, script);
}


#ifdef ENABLE_LOGGING_AND_PROFILING

bool CodeGenerator::ShouldGenerateLog(Expression* type) {
  ASSERT(type != NULL);
  if (!Logger::is_logging()) return false;
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
  CALL_HEAP_FUNCTION(StubCache::ComputeCallInitialize(argc, in_loop), Code);
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
        Handle<JSFunction> function =
            Compiler::BuildBoilerplate(node->fun(), script(), this);
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



// Special cases: These 'runtime calls' manipulate the current
// frame and are only used 1 or two places, so we generate them
// inline instead of generating calls to them.  They are used
// for implementing Function.prototype.call() and
// Function.prototype.apply().
CodeGenerator::InlineRuntimeLUT CodeGenerator::kInlineRuntimeLUT[] = {
  {&CodeGenerator::GenerateIsSmi, "_IsSmi"},
  {&CodeGenerator::GenerateIsNonNegativeSmi, "_IsNonNegativeSmi"},
  {&CodeGenerator::GenerateIsArray, "_IsArray"},
  {&CodeGenerator::GenerateIsConstructCall, "_IsConstructCall"},
  {&CodeGenerator::GenerateArgumentsLength, "_ArgumentsLength"},
  {&CodeGenerator::GenerateArgumentsAccess, "_Arguments"},
  {&CodeGenerator::GenerateClassOf, "_ClassOf"},
  {&CodeGenerator::GenerateValueOf, "_ValueOf"},
  {&CodeGenerator::GenerateSetValueOf, "_SetValueOf"},
  {&CodeGenerator::GenerateFastCharCodeAt, "_FastCharCodeAt"},
  {&CodeGenerator::GenerateObjectEquals, "_ObjectEquals"},
  {&CodeGenerator::GenerateLog, "_Log"},
  {&CodeGenerator::GenerateRandomPositiveSmi, "_RandomPositiveSmi"},
  {&CodeGenerator::GenerateMathSin, "_Math_sin"},
  {&CodeGenerator::GenerateMathCos, "_Math_cos"},
  {&CodeGenerator::GenerateIsObject, "_IsObject"},
  {&CodeGenerator::GenerateIsFunction, "_IsFunction"},
};


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


void CodeGenerator::RecordPositions(MacroAssembler* masm, int pos) {
  if (pos != RelocInfo::kNoPosition) {
    masm->RecordStatementPosition(pos);
    masm->RecordPosition(pos);
  }
}


void CodeGenerator::CodeForFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) RecordPositions(masm(), fun->start_position());
}


void CodeGenerator::CodeForReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) RecordPositions(masm(), fun->end_position());
}


void CodeGenerator::CodeForStatementPosition(Statement* stmt) {
  if (FLAG_debug_info) RecordPositions(masm(), stmt->statement_pos());
}

void CodeGenerator::CodeForDoWhileConditionPosition(DoWhileStatement* stmt) {
  if (FLAG_debug_info) RecordPositions(masm(), stmt->condition_position());
}

void CodeGenerator::CodeForSourcePosition(int pos) {
  if (FLAG_debug_info && pos != RelocInfo::kNoPosition) {
    masm()->RecordPosition(pos);
  }
}


const char* RuntimeStub::GetName() {
  return Runtime::FunctionForId(id_)->stub_name;
}


void RuntimeStub::Generate(MacroAssembler* masm) {
  Runtime::Function* f = Runtime::FunctionForId(id_);
  masm->TailCallRuntime(ExternalReference(f),
                        num_arguments_,
                        f->result_size);
}


void ArgumentsAccessStub::Generate(MacroAssembler* masm) {
  switch (type_) {
    case READ_LENGTH: GenerateReadLength(masm); break;
    case READ_ELEMENT: GenerateReadElement(masm); break;
    case NEW_OBJECT: GenerateNewObject(masm); break;
  }
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
