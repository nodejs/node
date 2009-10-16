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

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o r1: the JS function object being called (ie, ourselves)
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm.h for its layout.
void FastCodeGenerator::Generate(FunctionLiteral* fun) {
  function_ = fun;
  // ARM does NOT call SetFunctionPosition.

  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  // Adjust fp to point to caller's fp.
  __ add(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = fun->scope()->num_stack_slots();
    if (locals_count > 0) {
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    }
    if (FLAG_check_stack) {
      __ LoadRoot(r2, Heap::kStackLimitRootIndex);
    }
    for (int i = 0; i < locals_count; i++) {
      __ push(ip);
    }
  }

  if (FLAG_check_stack) {
    // Put the lr setup instruction in the delay slot.  The kInstrSize is
    // added to the implicit 8 byte offset that always applies to operations
    // with pc and gives a return address 12 bytes down.
    Comment cmnt(masm_, "[ Stack check");
    __ add(lr, pc, Operand(Assembler::kInstrSize));
    __ cmp(sp, Operand(r2));
    StackCheckStub stub;
    __ mov(pc,
           Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
                   RelocInfo::CODE_TARGET),
           LeaveCC,
           lo);
  }

  { Comment cmnt(masm_, "[ Body");
    VisitStatements(fun->body());
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the
    // body.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    SetReturnPosition(fun);
    __ RecordJSReturn();
    __ mov(sp, fp);
    __ ldm(ia_w, sp, fp.bit() | lr.bit());
    int num_parameters = function_->scope()->num_parameters();
    __ add(sp, sp, Operand((num_parameters + 1) * kPointerSize));
    __ Jump(lr);
  }
}


void FastCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  Visit(stmt->expression());
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Visit(stmt->expression());
  __ pop(r0);
  __ RecordJSReturn();
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
    int num_parameters = function_->scope()->num_parameters();
  __ add(sp, sp, Operand((num_parameters + 1) * kPointerSize));
  __ Jump(lr);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  Expression* rewrite = expr->var()->rewrite();
  ASSERT(rewrite != NULL);

  Slot* slot = rewrite->AsSlot();
  ASSERT(slot != NULL);
  { Comment cmnt(masm_, "[ Slot");
    if (expr->location().is_temporary()) {
      __ ldr(ip, MemOperand(fp, SlotOffset(slot)));
      __ push(ip);
    } else {
      ASSERT(expr->location().is_nowhere());
    }
  }
}


void FastCodeGenerator::VisitLiteral(Literal* expr) {
  Comment cmnt(masm_, "[ Literal");
  if (expr->location().is_temporary()) {
    __ mov(ip, Operand(expr->handle()));
    __ push(ip);
  } else {
    ASSERT(expr->location().is_nowhere());
  }
}


void FastCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");
  ASSERT(expr->op() == Token::ASSIGN || expr->op() == Token::INIT_VAR);

  Visit(expr->value());

  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL && var->slot() != NULL);

  if (expr->location().is_temporary()) {
    __ ldr(ip, MemOperand(sp));
  } else {
    ASSERT(expr->location().is_nowhere());
    __ pop(ip);
  }
  __ str(ip, MemOperand(fp, SlotOffset(var->slot())));
}


} }  // namespace v8::internal
