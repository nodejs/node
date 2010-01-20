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
#include "compiler.h"
#include "debug.h"
#include "fast-codegen.h"
#include "parser.h"

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
  SetFunctionPosition(fun);
  int locals_count = fun->scope()->num_stack_slots();

  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  if (locals_count > 0) {
      // Load undefined value here, so the value is ready for the loop below.
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  }
  // Adjust fp to point to caller's fp.
  __ add(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    for (int i = 0; i < locals_count; i++) {
      __ push(ip);
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  if (fun->scope()->num_heap_slots() > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is in r1.
    __ push(r1);
    __ CallRuntime(Runtime::kNewContext, 1);
    function_in_register = false;
    // Context is returned in both r0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = fun->scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Slot* slot = fun->scope()->parameter(i)->slot();
      if (slot != NULL && slot->type() == Slot::CONTEXT) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ ldr(r0, MemOperand(fp, parameter_offset));
        // Store it in the context
        __ str(r0, MemOperand(cp, Context::SlotOffset(slot->index())));
      }
    }
  }

  Variable* arguments = fun->scope()->arguments()->AsVariable();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register) {
      // Load this again, if it's used by the local context below.
      __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    } else {
      __ mov(r3, r1);
    }
    // Receiver is just before the parameters on the caller's stack.
    __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset +
                               fun->num_parameters() * kPointerSize));
    __ mov(r1, Operand(Smi::FromInt(fun->num_parameters())));
    __ stm(db_w, sp, r3.bit() | r2.bit() | r1.bit());

    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiever and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    __ CallStub(&stub);
    // Duplicate the value; move-to-slot operation might clobber registers.
    __ mov(r3, r0);
    Move(arguments->slot(), r0, r1, r2);
    Slot* dot_arguments_slot =
        fun->scope()->arguments_shadow()->AsVariable()->slot();
    Move(dot_arguments_slot, r3, r1, r2);
  }

  // Check the stack for overflow or break request.
  // Put the lr setup instruction in the delay slot.  The kInstrSize is
  // added to the implicit 8 byte offset that always applies to operations
  // with pc and gives a return address 12 bytes down.
  { Comment cmnt(masm_, "[ Stack check");
  __ LoadRoot(r2, Heap::kStackLimitRootIndex);
  __ add(lr, pc, Operand(Assembler::kInstrSize));
  __ cmp(sp, Operand(r2));
  StackCheckStub stub;
  __ mov(pc,
         Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
                 RelocInfo::CODE_TARGET),
         LeaveCC,
         lo);
  }

  { Comment cmnt(masm_, "[ Declarations");
    VisitDeclarations(fun->scope()->declarations());
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }

  { Comment cmnt(masm_, "[ Body");
    ASSERT(loop_depth() == 0);
    VisitStatements(fun->body());
    ASSERT(loop_depth() == 0);
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the
    // body.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence(function_->end_position());
}


void FastCodeGenerator::EmitReturnSequence(int position) {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ b(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r0.
      __ push(r0);
      __ CallRuntime(Runtime::kTraceExit, 1);
    }

    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);

    // Calculate the exact length of the return sequence and make sure that
    // the constant pool is not emitted inside of the return sequence.
    int num_parameters = function_->scope()->num_parameters();
    int32_t sp_delta = (num_parameters + 1) * kPointerSize;
    int return_sequence_length = Assembler::kJSReturnSequenceLength;
    if (!masm_->ImmediateFitsAddrMode1Instruction(sp_delta)) {
      // Additional mov instruction generated.
      return_sequence_length++;
    }
    masm_->BlockConstPoolFor(return_sequence_length);

    CodeGenerator::RecordPositions(masm_, position);
    __ RecordJSReturn();
    __ mov(sp, fp);
    __ ldm(ia_w, sp, fp.bit() | lr.bit());
    __ add(sp, sp, Operand(sp_delta));
    __ Jump(lr);

    // Check that the size of the code used for returning matches what is
    // expected by the debugger. The add instruction above is an addressing
    // mode 1 instruction where there are restrictions on which immediate values
    // can be encoded in the instruction and which immediate values requires
    // use of an additional instruction for moving the immediate to a temporary
    // register.
    ASSERT_EQ(return_sequence_length,
              masm_->InstructionsGeneratedSince(&check_exit_codesize));
  }
}


void FastCodeGenerator::Apply(Expression::Context context,
                              Slot* slot,
                              Register scratch) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
    case Expression::kValue:
    case Expression::kTest:
    case Expression::kValueTest:
    case Expression::kTestValue:
      Move(scratch, slot);
      Apply(context, scratch);
      break;
  }
}


void FastCodeGenerator::Apply(Expression::Context context, Literal* lit) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
    case Expression::kValue:
    case Expression::kTest:
    case Expression::kValueTest:
    case Expression::kTestValue:
      __ mov(ip, Operand(lit->handle()));
      Apply(context, ip);
      break;
  }
}


void FastCodeGenerator::ApplyTOS(Expression::Context context) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      __ Drop(1);
      break;
    case Expression::kValue:
      break;
    case Expression::kTest:
      __ pop(r0);
      TestAndBranch(r0, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ ldr(r0, MemOperand(sp, 0));
      TestAndBranch(r0, true_label_, &discard);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ ldr(r0, MemOperand(sp, 0));
      TestAndBranch(r0, &discard, false_label_);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
    }
  }
}


void FastCodeGenerator::DropAndApply(int count,
                                     Expression::Context context,
                                     Register reg) {
  ASSERT(count > 0);
  ASSERT(!reg.is(sp));
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      __ Drop(count);
      break;
    case Expression::kValue:
      if (count > 1) __ Drop(count - 1);
      __ str(reg, MemOperand(sp));
      break;
    case Expression::kTest:
      __ Drop(count);
      TestAndBranch(reg, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (count > 1) __ Drop(count - 1);
      __ str(reg, MemOperand(sp));
      TestAndBranch(reg, true_label_, &discard);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (count > 1) __ Drop(count - 1);
      __ str(reg, MemOperand(sp));
      TestAndBranch(reg, &discard, false_label_);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
      break;
    }
  }
}


MemOperand FastCodeGenerator::EmitSlotSearch(Slot* slot, Register scratch) {
  switch (slot->type()) {
    case Slot::PARAMETER:
    case Slot::LOCAL:
      return MemOperand(fp, SlotOffset(slot));
    case Slot::CONTEXT: {
      int context_chain_length =
          function_->scope()->ContextChainLength(slot->var()->scope());
      __ LoadContext(scratch, context_chain_length);
      return CodeGenerator::ContextOperand(scratch, slot->index());
    }
    case Slot::LOOKUP:
      UNREACHABLE();
  }
  UNREACHABLE();
  return MemOperand(r0, 0);
}


void FastCodeGenerator::Move(Register destination, Slot* source) {
  // Use destination as scratch.
  MemOperand location = EmitSlotSearch(source, destination);
  __ ldr(destination, location);
}



void FastCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  ASSERT(dst->type() != Slot::LOOKUP);  // Not yet implemented.
  ASSERT(!scratch1.is(src) && !scratch2.is(src));
  MemOperand location = EmitSlotSearch(dst, scratch1);
  __ str(src, location);
  // Emit the write barrier code if the location is in the heap.
  if (dst->type() == Slot::CONTEXT) {
    __ mov(scratch2, Operand(Context::SlotOffset(dst->index())));
    __ RecordWrite(scratch1, scratch2, src);
  }
}



void FastCodeGenerator::TestAndBranch(Register source,
                                      Label* true_label,
                                      Label* false_label) {
  ASSERT_NE(NULL, true_label);
  ASSERT_NE(NULL, false_label);
  // Call the runtime to find the boolean value of the source and then
  // translate it into control flow to the pair of labels.
  __ push(source);
  __ CallRuntime(Runtime::kToBool, 1);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, true_label);
  __ jmp(false_label);
}


void FastCodeGenerator::VisitDeclaration(Declaration* decl) {
  Comment cmnt(masm_, "[ Declaration");
  Variable* var = decl->proxy()->var();
  ASSERT(var != NULL);  // Must have been resolved.
  Slot* slot = var->slot();
  Property* prop = var->AsProperty();

  if (slot != NULL) {
    switch (slot->type()) {
      case Slot::PARAMETER:
      case Slot::LOCAL:
        if (decl->mode() == Variable::CONST) {
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ str(ip, MemOperand(fp, SlotOffset(slot)));
        } else if (decl->fun() != NULL) {
          Visit(decl->fun());
          __ pop(ip);
          __ str(ip, MemOperand(fp, SlotOffset(slot)));
        }
        break;

      case Slot::CONTEXT:
        // We bypass the general EmitSlotSearch because we know more about
        // this specific context.

        // The variable in the decl always resides in the current context.
        ASSERT_EQ(0, function_->scope()->ContextChainLength(var->scope()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ ldr(r1,
                 CodeGenerator::ContextOperand(cp, Context::FCONTEXT_INDEX));
          __ cmp(r1, cp);
          __ Check(eq, "Unexpected declaration in current context.");
        }
        if (decl->mode() == Variable::CONST) {
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ str(ip, CodeGenerator::ContextOperand(cp, slot->index()));
          // No write barrier since the_hole_value is in old space.
        } else if (decl->fun() != NULL) {
          Visit(decl->fun());
          __ pop(r0);
          __ str(r0, CodeGenerator::ContextOperand(cp, slot->index()));
          int offset = Context::SlotOffset(slot->index());
          __ mov(r2, Operand(offset));
          // We know that we have written a function, which is not a smi.
          __ RecordWrite(cp, r2, r0);
        }
        break;

      case Slot::LOOKUP: {
        __ mov(r2, Operand(var->name()));
        // Declaration nodes are always introduced in one of two modes.
        ASSERT(decl->mode() == Variable::VAR ||
               decl->mode() == Variable::CONST);
        PropertyAttributes attr =
            (decl->mode() == Variable::VAR) ? NONE : READ_ONLY;
        __ mov(r1, Operand(Smi::FromInt(attr)));
        // Push initial value, if any.
        // Note: For variables we must not push an initial value (such as
        // 'undefined') because we may have a (legal) redeclaration and we
        // must not destroy the current value.
        if (decl->mode() == Variable::CONST) {
          __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
          __ stm(db_w, sp, cp.bit() | r2.bit() | r1.bit() | r0.bit());
        } else if (decl->fun() != NULL) {
          __ stm(db_w, sp, cp.bit() | r2.bit() | r1.bit());
          Visit(decl->fun());  // Initial value for function decl.
        } else {
          __ mov(r0, Operand(Smi::FromInt(0)));  // No initial value!
          __ stm(db_w, sp, cp.bit() | r2.bit() | r1.bit() | r0.bit());
        }
        __ CallRuntime(Runtime::kDeclareContextSlot, 4);
        break;
      }
    }

  } else if (prop != NULL) {
    if (decl->fun() != NULL || decl->mode() == Variable::CONST) {
      // We are declaring a function or constant that rewrites to a
      // property.  Use (keyed) IC to set the initial value.
      ASSERT_EQ(Expression::kValue, prop->obj()->context());
      Visit(prop->obj());
      ASSERT_EQ(Expression::kValue, prop->key()->context());
      Visit(prop->key());

      if (decl->fun() != NULL) {
        ASSERT_EQ(Expression::kValue, decl->fun()->context());
        Visit(decl->fun());
        __ pop(r0);
      } else {
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
      }

      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);

      // Value in r0 is ignored (declarations are statements).  Receiver
      // and key on stack are discarded.
      __ Drop(2);
    }
  }
}


void FastCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  // The context is the first argument.
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(is_eval_ ? 1 : 0)));
  __ stm(db_w, sp, cp.bit() | r1.bit() | r0.bit());
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate =
      Compiler::BuildBoilerplate(expr, script_, this);
  if (HasStackOverflow()) return;

  ASSERT(boilerplate->IsBoilerplate());

  // Create a new closure.
  __ mov(r0, Operand(boilerplate));
  __ stm(db_w, sp, cp.bit() | r0.bit());
  __ CallRuntime(Runtime::kNewClosure, 2);
  Apply(expr->context(), r0);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr->var(), expr->context());
}


void FastCodeGenerator::EmitVariableLoad(Variable* var,
                                         Expression::Context context) {
  Expression* rewrite = var->rewrite();
  if (rewrite == NULL) {
    ASSERT(var->is_global());
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in r2 and the global
    // object on the stack.
    __ ldr(ip, CodeGenerator::GlobalObject());
    __ push(ip);
    __ mov(r2, Operand(var->name()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    DropAndApply(1, context, r0);
  } else if (rewrite->AsSlot() != NULL) {
    Slot* slot = rewrite->AsSlot();
    if (FLAG_debug_code) {
      switch (slot->type()) {
        case Slot::PARAMETER:
        case Slot::LOCAL: {
          Comment cmnt(masm_, "Stack slot");
          break;
        }
        case Slot::CONTEXT: {
          Comment cmnt(masm_, "Context slot");
          break;
        }
        case Slot::LOOKUP:
          UNIMPLEMENTED();
          break;
      }
    }
    Apply(context, slot, r0);
  } else {
    Comment cmnt(masm_, "Variable rewritten to property");
    // A variable has been rewritten into an explicit access to an object
    // property.
    Property* property = rewrite->AsProperty();
    ASSERT_NOT_NULL(property);

    // The only property expressions that can occur are of the form
    // "slot[literal]".

    // Assert that the object is in a slot.
    Variable* object_var = property->obj()->AsVariableProxy()->AsVariable();
    ASSERT_NOT_NULL(object_var);
    Slot* object_slot = object_var->slot();
    ASSERT_NOT_NULL(object_slot);

    // Load the object.
    Move(r2, object_slot);

    // Assert that the key is a smi.
    Literal* key_literal = property->key()->AsLiteral();
    ASSERT_NOT_NULL(key_literal);
    ASSERT(key_literal->handle()->IsSmi());

    // Load the key.
    __ mov(r1, Operand(key_literal->handle()));

    // Push both as arguments to ic.
    __ stm(db_w, sp, r2.bit() | r1.bit());

    // Do a keyed property load.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);

    // Drop key and object left on the stack by IC, and push the result.
    DropAndApply(2, context, r0);
  }
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label done;
  // Registers will be used as follows:
  // r4 = JS function, literals array
  // r3 = literal index
  // r2 = RegExp pattern
  // r1 = RegExp flags
  // r0 = temp + return value (RegExp literal)
  __ ldr(r0, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r4,  FieldMemOperand(r0, JSFunction::kLiteralsOffset));
  int literal_offset =
    FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ ldr(r0, FieldMemOperand(r4, literal_offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(ne, &done);
  __ mov(r3, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r2, Operand(expr->pattern()));
  __ mov(r1, Operand(expr->flags()));
  __ stm(db_w, sp, r4.bit() | r3.bit() | r2.bit() | r1.bit());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ bind(&done);
  Apply(expr->context(), r0);
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ ldr(r2, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r2, FieldMemOperand(r2, JSFunction::kLiteralsOffset));
  __ mov(r1, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r0, Operand(expr->constant_properties()));
  __ stm(db_w, sp, r2.bit() | r1.bit() | r0.bit());
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 3);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 3);
  }

  // If result_saved == true: The result is saved on top of the
  //  stack and in r0.
  // If result_saved == false: The result not on the stack, just in r0.
  bool result_saved = false;

  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(r0);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();

      case ObjectLiteral::Property::MATERIALIZED_LITERAL:   // Fall through.
        ASSERT(!CompileTimeValue::IsCompileTimeValue(property->value()));
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          Visit(value);
          ASSERT_EQ(Expression::kValue, value->context());
          __ pop(r0);
          __ mov(r2, Operand(key->handle()));
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ Call(ic, RelocInfo::CODE_TARGET);
          // StoreIC leaves the receiver on the stack.
          __ ldr(r0, MemOperand(sp));  // Restore result into r0.
          break;
        }
        // Fall through.

      case ObjectLiteral::Property::PROTOTYPE:
        __ push(r0);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kSetProperty, 3);
        __ ldr(r0, MemOperand(sp));  // Restore result into r0.
        break;

      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER:
        __ push(r0);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        __ mov(r1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(r1);
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        __ ldr(r0, MemOperand(sp));  // Restore result into r0
        break;
    }
  }
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ Drop(1);
      break;
    case Expression::kValue:
      if (!result_saved) __ push(r0);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(r0);
      TestAndBranch(r0, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(r0);
      TestAndBranch(r0, true_label_, &discard);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(r0);
      TestAndBranch(r0, &discard, false_label_);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->constant_elements()));
  __ stm(db_w, sp, r3.bit() | r2.bit() | r1.bit());
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else {
    __ CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  }

  bool result_saved = false;  // Is the result saved to the stack?

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  ZoneList<Expression*>* subexprs = expr->values();
  for (int i = 0, len = subexprs->length(); i < len; i++) {
    Expression* subexpr = subexprs->at(i);
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (subexpr->AsLiteral() != NULL ||
        CompileTimeValue::IsCompileTimeValue(subexpr)) {
      continue;
    }

    if (!result_saved) {
      __ push(r0);
      result_saved = true;
    }
    Visit(subexpr);
    ASSERT_EQ(Expression::kValue, subexpr->context());

    // Store the subexpression value in the array's elements.
    __ pop(r0);  // Subexpression value.
    __ ldr(r1, MemOperand(sp));  // Copy of array literal.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ str(r0, FieldMemOperand(r1, offset));

    // Update the write barrier for the array store with r0 as the scratch
    // register.
    __ mov(r2, Operand(offset));
    __ RecordWrite(r1, r2, r0);
  }

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ Drop(1);
      break;
    case Expression::kValue:
      if (!result_saved) __ push(r0);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(r0);
      TestAndBranch(r0, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(r0);
      TestAndBranch(r0, true_label_, &discard);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(r0);
      TestAndBranch(r0, &discard, false_label_);
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::EmitNamedPropertyLoad(Property* prop,
                                              Expression::Context context) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ mov(r2, Operand(key->handle()));
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  Apply(context, r0);
}


void FastCodeGenerator::EmitKeyedPropertyLoad(Property* prop,
                                              Expression::Context context) {
  SetSourcePosition(prop->position());
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  Apply(context, r0);
}


void FastCodeGenerator::EmitCompoundAssignmentOp(Token::Value op,
                                                 Expression::Context context) {
  __ pop(r0);
  __ pop(r1);
  GenericBinaryOpStub stub(op,
                           NO_OVERWRITE);
  __ CallStub(&stub);
  Apply(context, r0);
}


void FastCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Expression::Context context) {
  ASSERT(var != NULL);
  ASSERT(var->is_global() || var->slot() != NULL);
  if (var->is_global()) {
    // Assignment to a global variable.  Use inline caching for the
    // assignment.  Right-hand-side value is passed in r0, variable name in
    // r2, and the global object on the stack.
    __ pop(r0);
    __ mov(r2, Operand(var->name()));
    __ ldr(ip, CodeGenerator::GlobalObject());
    __ push(ip);
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Overwrite the global object on the stack with the result if needed.
    DropAndApply(1, context, r0);

  } else if (var->slot() != NULL) {
    Slot* slot = var->slot();
    switch (slot->type()) {
      case Slot::LOCAL:
      case Slot::PARAMETER: {
        MemOperand target = MemOperand(fp, SlotOffset(slot));
        switch (context) {
          case Expression::kUninitialized:
            UNREACHABLE();
          case Expression::kEffect:
            // Perform assignment and discard value.
            __ pop(r0);
            __ str(r0, target);
            break;
          case Expression::kValue:
            // Perform assignment and preserve value.
            __ ldr(r0, MemOperand(sp));
            __ str(r0, target);
            break;
          case Expression::kTest:
            // Perform assignment and test (and discard) value.
            __ pop(r0);
            __ str(r0, target);
            TestAndBranch(r0, true_label_, false_label_);
            break;
          case Expression::kValueTest: {
            Label discard;
            __ ldr(r0, MemOperand(sp));
            __ str(r0, target);
            TestAndBranch(r0, true_label_, &discard);
            __ bind(&discard);
            __ Drop(1);
            __ jmp(false_label_);
            break;
          }
          case Expression::kTestValue: {
            Label discard;
            __ ldr(r0, MemOperand(sp));
            __ str(r0, target);
            TestAndBranch(r0, &discard, false_label_);
            __ bind(&discard);
            __ Drop(1);
            __ jmp(true_label_);
            break;
          }
        }
        break;
      }

      case Slot::CONTEXT: {
        MemOperand target = EmitSlotSearch(slot, r1);
        __ pop(r0);
        __ str(r0, target);

        // RecordWrite may destroy all its register arguments.
        if (context == Expression::kValue) {
          __ push(r0);
        } else if (context != Expression::kEffect) {
          __ mov(r3, r0);
        }
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;

        // Update the write barrier for the array store with r0 as the scratch
        // register.  Skip the write barrier if the value written (r1) is a smi.
        // The smi test is part of RecordWrite on other platforms, not on arm.
        Label exit;
        __ tst(r0, Operand(kSmiTagMask));
        __ b(eq, &exit);

        __ mov(r2, Operand(offset));
        __ RecordWrite(r1, r2, r0);
        __ bind(&exit);
        if (context != Expression::kEffect && context != Expression::kValue) {
          Apply(context, r3);
        }
        break;
      }

      case Slot::LOOKUP:
        UNREACHABLE();
        break;
    }
  } else {
    // Variables rewritten as properties are not treated as variables in
    // assignments.
    UNREACHABLE();
  }
}


void FastCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->AsLiteral() != NULL);

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ ldr(ip, MemOperand(sp, kPointerSize));  // Receiver is under value.
    __ push(ip);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(r0);
  __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    __ ldr(ip, MemOperand(sp, kPointerSize));  // Receiver is under value.
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
  }

  DropAndApply(1, expr->context(), r0);
}


void FastCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    // Receiver is under the key and value.
    __ ldr(ip, MemOperand(sp, 2 * kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(r0);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    // Reciever is under the key and value.
    __ ldr(ip, MemOperand(sp, 2 * kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
  }

  // Receiver and key are still on stack.
  DropAndApply(2, expr->context(), r0);
}


void FastCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  // Record the source position for the property load.
  SetSourcePosition(expr->position());

  // Evaluate receiver.
  Visit(expr->obj());

  if (key->IsPropertyName()) {
    // Do a named property load.  The IC expects the property name in r2 and
    // the receiver on the stack.
    __ mov(r2, Operand(key->AsLiteral()->handle()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    DropAndApply(1, expr->context(), r0);
  } else {
    // Do a keyed property load.
    Visit(expr->key());
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Drop key and receiver left on the stack by IC.
    DropAndApply(2, expr->context(), r0);
  }
}

void FastCodeGenerator::EmitCallWithIC(Call* expr,
                                       Handle<Object> ignored,
                                       RelocInfo::Mode mode) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                         NOT_IN_LOOP);
  __ Call(ic, mode);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndApply(1, expr->context(), r0);
}


void FastCodeGenerator::EmitCallWithStub(Call* expr) {
  // Code common for calls using the call stub.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  CallFunctionStub stub(arg_count, NOT_IN_LOOP);
  __ CallStub(&stub);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndApply(1, expr->context(), r0);
}


void FastCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  if (var != NULL && var->is_possibly_eval()) {
    // Call to the identifier 'eval'.
    UNREACHABLE();
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Call to a global variable.
    __ mov(r1, Operand(var->name()));
    // Push global object as receiver for the call IC lookup.
    __ ldr(r0, CodeGenerator::GlobalObject());
    __ stm(db_w, sp, r1.bit() | r0.bit());
    EmitCallWithIC(expr, var->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot.
    UNREACHABLE();
  } else if (fun->AsProperty() != NULL) {
    // Call to an object property.
    Property* prop = fun->AsProperty();
    Literal* key = prop->key()->AsLiteral();
    if (key != NULL && key->handle()->IsSymbol()) {
      // Call to a named property, use call IC.
      __ mov(r0, Operand(key->handle()));
      __ push(r0);
      Visit(prop->obj());
      EmitCallWithIC(expr, key->handle(), RelocInfo::CODE_TARGET);
    } else {
      // Call to a keyed property, use keyed load IC followed by function
      // call.
      Visit(prop->obj());
      Visit(prop->key());
      // Record source code position for IC call.
      SetSourcePosition(prop->position());
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      // Load receiver object into r1.
      if (prop->is_synthetic()) {
        __ ldr(r1, CodeGenerator::GlobalObject());
        __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
      } else {
        __ ldr(r1, MemOperand(sp, kPointerSize));
      }
      // Overwrite (object, key) with (function, receiver).
      __ str(r0, MemOperand(sp, kPointerSize));
      __ str(r1, MemOperand(sp));
      EmitCallWithStub(expr);
    }
  } else {
    // Call to some other expression.  If the expression is an anonymous
    // function literal not called in a loop, mark it as one that should
    // also use the fast code generator.
    FunctionLiteral* lit = fun->AsFunctionLiteral();
    if (lit != NULL &&
        lit->name()->Equals(Heap::empty_string()) &&
        loop_depth() == 0) {
      lit->set_try_fast_codegen(true);
    }
    Visit(fun);
    // Load global receiver object.
    __ ldr(r1, CodeGenerator::GlobalObject());
    __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
    __ push(r1);
    // Emit function call.
    EmitCallWithStub(expr);
  }
}


void FastCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.
  // Push function on the stack.
  Visit(expr->expression());
  ASSERT_EQ(Expression::kValue, expr->expression()->context());

  // Push global object (receiver).
  __ ldr(r0, CodeGenerator::GlobalObject());
  __ push(r0);
  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
    // If location is value, it is already on the stack,
    // so nothing to do here.
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(expr->position());

  // Load function, arg_count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  // Function is in sp[arg_count + 1].
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in r0, or pop it.
  DropAndApply(1, expr->context(), r0);
}


void FastCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  Comment cmnt(masm_, "[ CallRuntime");
  ZoneList<Expression*>* args = expr->arguments();

  if (expr->is_jsruntime()) {
    // Prepare for calling JS runtime function.
    __ mov(r1, Operand(expr->name()));
    __ ldr(r0, CodeGenerator::GlobalObject());
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kBuiltinsOffset));
    __ stm(db_w, sp, r1.bit() | r0.bit());
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function.
    Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                           NOT_IN_LOOP);
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Discard the function left on TOS.
    DropAndApply(1, expr->context(), r0);
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
    Apply(expr->context(), r0);
  }
}


void FastCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      Visit(expr->expression());
      ASSERT_EQ(Expression::kEffect, expr->expression()->context());
      switch (expr->context()) {
        case Expression::kUninitialized:
          UNREACHABLE();
          break;
        case Expression::kEffect:
          break;
        case Expression::kValue:
          __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
          __ push(ip);
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
          __ push(ip);
        case Expression::kTest:
        case Expression::kValueTest:
          __ jmp(false_label_);
          break;
      }
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      ASSERT_EQ(Expression::kTest, expr->expression()->context());

      Label push_true, push_false, done;
      switch (expr->context()) {
        case Expression::kUninitialized:
          UNREACHABLE();
          break;

        case Expression::kEffect:
          VisitForControl(expr->expression(), &done, &done);
          __ bind(&done);
          break;

        case Expression::kValue:
          VisitForControl(expr->expression(), &push_false, &push_true);
          __ bind(&push_true);
          __ LoadRoot(ip, Heap::kTrueValueRootIndex);
          __ push(ip);
          __ jmp(&done);
          __ bind(&push_false);
          __ LoadRoot(ip, Heap::kFalseValueRootIndex);
          __ push(ip);
          __ bind(&done);
          break;

        case Expression::kTest:
          VisitForControl(expr->expression(), false_label_, true_label_);
          break;

        case Expression::kValueTest:
          VisitForControl(expr->expression(), false_label_, &push_true);
          __ bind(&push_true);
          __ LoadRoot(ip, Heap::kTrueValueRootIndex);
          __ push(ip);
          __ jmp(true_label_);
          break;

        case Expression::kTestValue:
          VisitForControl(expr->expression(), &push_false, true_label_);
          __ bind(&push_false);
          __ LoadRoot(ip, Heap::kFalseValueRootIndex);
          __ push(ip);
          __ jmp(false_label_);
          break;
      }
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      ASSERT_EQ(Expression::kValue, expr->expression()->context());

      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (proxy != NULL &&
          !proxy->var()->is_this() &&
          proxy->var()->is_global()) {
        Comment cmnt(masm_, "Global variable");
        __ ldr(r0, CodeGenerator::GlobalObject());
        __ push(r0);
        __ mov(r2, Operand(proxy->name()));
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ Call(ic, RelocInfo::CODE_TARGET);
        __ str(r0, MemOperand(sp));
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ mov(r0, Operand(proxy->name()));
        __ stm(db_w, sp, cp.bit() | r0.bit());
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(r0);
      } else {
        // This expression cannot throw a reference error at the top level.
        Visit(expr->expression());
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Apply(expr->context(), r0);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");

  // Expression can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->expression()->AsProperty();
  // In case of a property we use the uninitialized expression context
  // of the key to detect a named property.
  if (prop != NULL) {
    assign_type = (prop->key()->context() == Expression::kUninitialized)
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  // Evaluate expression and get value.
  if (assign_type == VARIABLE) {
    ASSERT(expr->expression()->AsVariableProxy()->var() != NULL);
    EmitVariableLoad(expr->expression()->AsVariableProxy()->var(),
                     Expression::kValue);
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && expr->context() != Expression::kEffect) {
      ASSERT(expr->context() != Expression::kUninitialized);
      __ mov(ip, Operand(Smi::FromInt(0)));
      __ push(ip);
    }
    Visit(prop->obj());
    ASSERT_EQ(Expression::kValue, prop->obj()->context());
    if (assign_type == NAMED_PROPERTY) {
      EmitNamedPropertyLoad(prop, Expression::kValue);
    } else {
      Visit(prop->key());
      ASSERT_EQ(Expression::kValue, prop->key()->context());
      EmitKeyedPropertyLoad(prop, Expression::kValue);
    }
  }

  // Convert to number.
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    switch (expr->context()) {
      case Expression::kUninitialized:
        UNREACHABLE();
      case Expression::kEffect:
        // Do not save result.
        break;
      case Expression::kValue:
      case Expression::kTest:
      case Expression::kTestValue:
      case Expression::kValueTest:
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(r0);
            break;
          case NAMED_PROPERTY:
            __ str(r0, MemOperand(sp, kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ str(r0, MemOperand(sp, 2 * kPointerSize));
            break;
        }
        break;
    }
  }

  // Call runtime for +1/-1.
  if (expr->op() == Token::INC) {
    __ mov(ip, Operand(Smi::FromInt(1)));
  } else {
    __ mov(ip, Operand(Smi::FromInt(-1)));
  }
  __ stm(db_w, sp, ip.bit() | r0.bit());
  __ CallRuntime(Runtime::kNumberAdd, 2);

  // Store the value returned in r0.
  switch (assign_type) {
    case VARIABLE:
      __ push(r0);
      if (expr->is_postfix()) {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Expression::kEffect);
        // For all contexts except kEffect: We have the result on
        // top of the stack.
        if (expr->context() != Expression::kEffect) {
          ApplyTOS(expr->context());
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               expr->context());
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        __ Drop(1);  // Result is on the stack under the receiver.
        if (expr->context() != Expression::kEffect) {
          ApplyTOS(expr->context());
        }
      } else {
        DropAndApply(1, expr->context(), r0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        __ Drop(2);  // Result is on the stack under the key and the receiver.
        if (expr->context() != Expression::kEffect) {
          ApplyTOS(expr->context());
        }
      } else {
        DropAndApply(2, expr->context(), r0);
      }
      break;
    }
  }
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  Comment cmnt(masm_, "[ BinaryOperation");
  switch (expr->op()) {
    case Token::COMMA:
      ASSERT_EQ(Expression::kEffect, expr->left()->context());
      ASSERT_EQ(expr->context(), expr->right()->context());
      Visit(expr->left());
      Visit(expr->right());
      break;

    case Token::OR:
    case Token::AND:
      EmitLogicalOperation(expr);
      break;

    case Token::ADD:
    case Token::SUB:
    case Token::DIV:
    case Token::MOD:
    case Token::MUL:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      ASSERT_EQ(Expression::kValue, expr->left()->context());
      ASSERT_EQ(Expression::kValue, expr->right()->context());

      Visit(expr->left());
      Visit(expr->right());
      __ pop(r0);
      __ pop(r1);
      GenericBinaryOpStub stub(expr->op(),
                               NO_OVERWRITE);
      __ CallStub(&stub);
      Apply(expr->context(), r0);

      break;
    }
    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  ASSERT_EQ(Expression::kValue, expr->left()->context());
  ASSERT_EQ(Expression::kValue, expr->right()->context());
  Visit(expr->left());
  Visit(expr->right());

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.
  Label push_true, push_false, done;
  // Initially assume we are in a test context.
  Label* if_true = true_label_;
  Label* if_false = false_label_;
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;
    case Expression::kEffect:
      if_true = &done;
      if_false = &done;
      break;
    case Expression::kValue:
      if_true = &push_true;
      if_false = &push_false;
      break;
    case Expression::kTest:
      break;
    case Expression::kValueTest:
      if_true = &push_true;
      break;
    case Expression::kTestValue:
      if_false = &push_false;
      break;
  }

  switch (expr->op()) {
    case Token::IN: {
      __ InvokeBuiltin(Builtins::IN, CALL_JS);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r0, ip);
      __ b(eq, if_true);
      __ jmp(if_false);
      break;
    }

    case Token::INSTANCEOF: {
      InstanceofStub stub;
      __ CallStub(&stub);
      __ tst(r0, r0);
      __ b(eq, if_true);  // The stub returns 0 for true.
      __ jmp(if_false);
      break;
    }

    default: {
      Condition cc = eq;
      bool strict = false;
      switch (expr->op()) {
        case Token::EQ_STRICT:
          strict = true;
          // Fall through
        case Token::EQ:
          cc = eq;
          __ pop(r0);
          __ pop(r1);
          break;
        case Token::LT:
          cc = lt;
          __ pop(r0);
          __ pop(r1);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = lt;
          __ pop(r1);
          __ pop(r0);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = ge;
          __ pop(r1);
          __ pop(r0);
          break;
        case Token::GTE:
          cc = ge;
          __ pop(r0);
          __ pop(r1);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

      // The comparison stub expects the smi vs. smi case to be handled
      // before it is called.
      Label slow_case;
      __ orr(r2, r0, Operand(r1));
      __ tst(r2, Operand(kSmiTagMask));
      __ b(ne, &slow_case);
      __ cmp(r1, r0);
      __ b(cc, if_true);
      __ jmp(if_false);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ tst(r0, r0);
      __ b(cc, if_true);
      __ jmp(if_false);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;

    case Expression::kEffect:
      __ bind(&done);
      break;

    case Expression::kValue:
      __ bind(&push_true);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ push(ip);
      __ jmp(&done);
      __ bind(&push_false);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ push(ip);
      __ bind(&done);
      break;

    case Expression::kTest:
      break;

    case Expression::kValueTest:
      __ bind(&push_true);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ push(ip);
      __ jmp(true_label_);
      break;

    case Expression::kTestValue:
      __ bind(&push_false);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ push(ip);
      __ jmp(false_label_);
      break;
  }
}


void FastCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  Apply(expr->context(), r0);
}


Register FastCodeGenerator::result_register() { return r0; }


Register FastCodeGenerator::context_register() { return cp; }


void FastCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ str(value, MemOperand(fp, frame_offset));
}


void FastCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ ldr(dst, CodeGenerator::ContextOperand(cp, context_index));
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FastCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(r1));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta)
  __ sub(r1, lr, Operand(masm_->CodeObject()));
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  ASSERT_EQ(0, kSmiTag);
  __ add(r1, r1, Operand(r1));  // Convert to smi.
  __ push(r1);
}


void FastCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(r1));
  // Restore result register from stack.
  __ pop(r1);
  // Uncook return address and return.
  __ pop(result_register());
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ mov(r1, Operand(r1, ASR, 1));  // Un-smi-tag value.
  __ add(pc, r1, Operand(masm_->CodeObject()));
}


#undef __

} }  // namespace v8::internal
