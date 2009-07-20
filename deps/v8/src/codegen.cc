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


// Generate the code. Takes a function literal, generates code for it, assemble
// all the pieces into a Code object. This function is only to be called by
// the compiler.cc code.
Handle<Code> CodeGenerator::MakeCode(FunctionLiteral* flit,
                                     Handle<Script> script,
                                     bool is_eval) {
#ifdef ENABLE_DISASSEMBLER
  bool print_code = Bootstrapper::IsActive()
      ? FLAG_print_builtin_code
      : FLAG_print_code;
#endif

#ifdef DEBUG
  bool print_source = false;
  bool print_ast = false;
  const char* ftype;

  if (Bootstrapper::IsActive()) {
    print_source = FLAG_print_builtin_source;
    print_ast = FLAG_print_builtin_ast;
    ftype = "builtin";
  } else {
    print_source = FLAG_print_source;
    print_ast = FLAG_print_ast;
    ftype = "user-defined";
  }

  if (FLAG_trace_codegen || print_source || print_ast) {
    PrintF("*** Generate code for %s function: ", ftype);
    flit->name()->ShortPrint();
    PrintF(" ***\n");
  }

  if (print_source) {
    PrintF("--- Source from AST ---\n%s\n", PrettyPrinter().PrintProgram(flit));
  }

  if (print_ast) {
    PrintF("--- AST ---\n%s\n", AstPrinter().PrintProgram(flit));
  }
#endif  // DEBUG

  // Generate code.
  const int initial_buffer_size = 4 * KB;
  CodeGenerator cgen(initial_buffer_size, script, is_eval);
  CodeGeneratorScope scope(&cgen);
  cgen.GenCode(flit);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  // Allocate and install the code.  Time the rest of this function as
  // code creation.
  HistogramTimerScope timer(&Counters::code_creation);
  CodeDesc desc;
  cgen.masm()->GetCode(&desc);
  ZoneScopeInfo sinfo(flit->scope());
  InLoopFlag in_loop = (cgen.loop_nesting() != 0) ? IN_LOOP : NOT_IN_LOOP;
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, in_loop);
  Handle<Code> code = Factory::NewCode(desc,
                                       &sinfo,
                                       flags,
                                       cgen.masm()->CodeObject());

  // Add unresolved entries in the code to the fixup list.
  Bootstrapper::AddFixup(*code, cgen.masm());

#ifdef ENABLE_DISASSEMBLER
  if (print_code) {
    // Print the source code if available.
    if (!script->IsUndefined() && !script->source()->IsUndefined()) {
      PrintF("--- Raw source ---\n");
      StringInputBuffer stream(String::cast(script->source()));
      stream.Seek(flit->start_position());
      // flit->end_position() points to the last character in the stream. We
      // need to compensate by adding one to calculate the length.
      int source_len = flit->end_position() - flit->start_position() + 1;
      for (int i = 0; i < source_len; i++) {
        if (stream.has_more()) PrintF("%c", stream.GetNext());
      }
      PrintF("\n\n");
    }
    PrintF("--- Code ---\n");
    code->Disassemble(*flit->name()->ToCString());
  }
#endif  // ENABLE_DISASSEMBLER

  if (!code.is_null()) {
    Counters::total_compiled_code_size.Increment(code->instruction_size());
  }

  return code;
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


// Sets the function info on a function.
// The start_position points to the first '(' character after the function name
// in the full script source. When counting characters in the script source the
// the first character is number 0 (not 1).
void CodeGenerator::SetFunctionInfo(Handle<JSFunction> fun,
                                    int length,
                                    int function_token_position,
                                    int start_position,
                                    int end_position,
                                    bool is_expression,
                                    bool is_toplevel,
                                    Handle<Script> script,
                                    Handle<String> inferred_name) {
  fun->shared()->set_length(length);
  fun->shared()->set_formal_parameter_count(length);
  fun->shared()->set_script(*script);
  fun->shared()->set_function_token_position(function_token_position);
  fun->shared()->set_start_position(start_position);
  fun->shared()->set_end_position(end_position);
  fun->shared()->set_is_expression(is_expression);
  fun->shared()->set_is_toplevel(is_toplevel);
  fun->shared()->set_inferred_name(*inferred_name);
}


static Handle<Code> ComputeLazyCompile(int argc) {
  CALL_HEAP_FUNCTION(StubCache::ComputeLazyCompile(argc), Code);
}


Handle<JSFunction> CodeGenerator::BuildBoilerplate(FunctionLiteral* node) {
#ifdef DEBUG
  // We should not try to compile the same function literal more than
  // once.
  node->mark_as_compiled();
#endif

  // Determine if the function can be lazily compiled. This is
  // necessary to allow some of our builtin JS files to be lazily
  // compiled. These builtins cannot be handled lazily by the parser,
  // since we have to know if a function uses the special natives
  // syntax, which is something the parser records.
  bool allow_lazy = node->AllowsLazyCompilation();

  // Generate code
  Handle<Code> code;
  if (FLAG_lazy && allow_lazy) {
    code = ComputeLazyCompile(node->num_parameters());
  } else {
    // The bodies of function literals have not yet been visited by
    // the AST optimizer/analyzer.
    if (!Rewriter::Optimize(node)) {
      return Handle<JSFunction>::null();
    }

    code = MakeCode(node, script_, false);

    // Check for stack-overflow exception.
    if (code.is_null()) {
      SetStackOverflow();
      return Handle<JSFunction>::null();
    }

    // Function compilation complete.
    LOG(CodeCreateEvent(Logger::FUNCTION_TAG, *code, *node->name()));

#ifdef ENABLE_OPROFILE_AGENT
    OProfileAgent::CreateNativeCodeRegion(*node->name(),
                                          code->instruction_start(),
                                          code->instruction_size());
#endif
  }

  // Create a boilerplate function.
  Handle<JSFunction> function =
      Factory::NewFunctionBoilerplate(node->name(),
                                      node->materialized_literal_count(),
                                      node->contains_array_literal(),
                                      code);
  CodeGenerator::SetFunctionInfo(function, node->num_parameters(),
                                 node->function_token_position(),
                                 node->start_position(), node->end_position(),
                                 node->is_expression(), false, script_,
                                 node->inferred_name());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger that a new function has been added.
  Debugger::OnNewFunction(function);
#endif

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(function,
                                       node->expected_property_count());
  return function;
}


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
        Handle<JSFunction> function = BuildBoilerplate(node->fun());
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
  {&CodeGenerator::GenerateMathCos, "_Math_cos"}
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


void CodeGenerator::CodeForFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    int pos = fun->start_position();
    if (pos != RelocInfo::kNoPosition) {
      masm()->RecordStatementPosition(pos);
      masm()->RecordPosition(pos);
    }
  }
}


void CodeGenerator::CodeForReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    int pos = fun->end_position();
    if (pos != RelocInfo::kNoPosition) {
      masm()->RecordStatementPosition(pos);
      masm()->RecordPosition(pos);
    }
  }
}


void CodeGenerator::CodeForStatementPosition(Node* node) {
  if (FLAG_debug_info) {
    int pos = node->statement_pos();
    if (pos != RelocInfo::kNoPosition) {
      masm()->RecordStatementPosition(pos);
      masm()->RecordPosition(pos);
    }
  }
}


void CodeGenerator::CodeForSourcePosition(int pos) {
  if (FLAG_debug_info) {
    if (pos != RelocInfo::kNoPosition) {
      masm()->RecordPosition(pos);
    }
  }
}


const char* RuntimeStub::GetName() {
  return Runtime::FunctionForId(id_)->stub_name;
}


void RuntimeStub::Generate(MacroAssembler* masm) {
  masm->TailCallRuntime(ExternalReference(id_), num_arguments_);
}


void ArgumentsAccessStub::Generate(MacroAssembler* masm) {
  switch (type_) {
    case READ_LENGTH: GenerateReadLength(masm); break;
    case READ_ELEMENT: GenerateReadElement(masm); break;
    case NEW_OBJECT: GenerateNewObject(masm); break;
  }
}


} }  // namespace v8::internal
