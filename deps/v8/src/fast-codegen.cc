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

#include "codegen-inl.h"
#include "fast-codegen.h"
#include "stub-cache.h"
#include "debug.h"

namespace v8 {
namespace internal {

Handle<Code> FastCodeGenerator::MakeCode(FunctionLiteral* fun,
                                         Handle<Script> script,
                                         bool is_eval) {
  CodeGenerator::MakeCodePrologue(fun);
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);
  FastCodeGenerator cgen(&masm, script, is_eval);
  cgen.Generate(fun);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, NOT_IN_LOOP);
  return CodeGenerator::MakeCodeEpilogue(fun, &masm, flags, script);
}


int FastCodeGenerator::SlotOffset(Slot* slot) {
  ASSERT(slot != NULL);
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -slot->index() * kPointerSize;
  // Adjust by a (parameter or local) base offset.
  switch (slot->type()) {
    case Slot::PARAMETER:
      offset += (function_->scope()->num_parameters() + 1) * kPointerSize;
      break;
    case Slot::LOCAL:
      offset += JavaScriptFrameConstants::kLocal0Offset;
      break;
    default:
      UNREACHABLE();
  }
  return offset;
}


// All platform macro assemblers in {ia32,x64,arm} have a push(Register)
// function.
void FastCodeGenerator::Move(Location destination, Register source) {
  switch (destination.type()) {
    case Location::kUninitialized:
      UNREACHABLE();
    case Location::kEffect:
      break;
    case Location::kValue:
      masm_->push(source);
      break;
  }
}


// All platform macro assemblers in {ia32,x64,arm} have a pop(Register)
// function.
void FastCodeGenerator::Move(Register destination, Location source) {
  switch (source.type()) {
    case Location::kUninitialized:  // Fall through.
    case Location::kEffect:
      UNREACHABLE();
    case Location::kValue:
      masm_->pop(destination);
  }
}


void FastCodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
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
      UNREACHABLE();
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

    if ((slot == NULL || slot->type() != Slot::LOOKUP) && var->is_global()) {
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

Handle<JSFunction> FastCodeGenerator::BuildBoilerplate(FunctionLiteral* fun) {
#ifdef DEBUG
  // We should not try to compile the same function literal more than
  // once.
  fun->mark_as_compiled();
#endif

  // Generate code
  Handle<Code> code = CodeGenerator::ComputeLazyCompile(fun->num_parameters());
  // Check for stack-overflow exception.
  if (code.is_null()) {
    SetStackOverflow();
    return Handle<JSFunction>::null();
  }

  // Create a boilerplate function.
  Handle<JSFunction> function =
      Factory::NewFunctionBoilerplate(fun->name(),
                                      fun->materialized_literal_count(),
                                      code);
  CodeGenerator::SetFunctionInfo(function, fun, false, script_);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger that a new function has been added.
  Debugger::OnNewFunction(function);
#endif

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(function,
                                       fun->expected_property_count());
  return function;
}


void FastCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->start_position());
  }
}


void FastCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->end_position());
  }
}


void FastCodeGenerator::SetStatementPosition(Statement* stmt) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
  }
}


void FastCodeGenerator::SetSourcePosition(int pos) {
  if (FLAG_debug_info && pos != RelocInfo::kNoPosition) {
    masm_->RecordPosition(pos);
  }
}


void FastCodeGenerator::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  SetStatementPosition(stmt);
  VisitStatements(stmt->statements());
}


void FastCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  Visit(stmt->expression());
}


void FastCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  Comment cmnt(masm_, "[ EmptyStatement");
  SetStatementPosition(stmt);
}


void FastCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSlot(Slot* expr) {
  // Slots do not appear directly in the AST.
  UNREACHABLE();
}


void FastCodeGenerator::VisitLiteral(Literal* expr) {
  Move(expr->location(), expr);
}


void FastCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


} }  // namespace v8::internal
