// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "ic-inl.h"
#include "parser.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"
#include "virtual-frame-inl.h"



namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)



// -----------------------------------------------------------------------------
// Platform-specific DeferredCode functions.


void DeferredCode::SaveRegisters() {
  UNIMPLEMENTED_MIPS();
}


void DeferredCode::RestoreRegisters() {
  UNIMPLEMENTED_MIPS();
}


// -----------------------------------------------------------------------------
// CodeGenState implementation.

CodeGenState::CodeGenState(CodeGenerator* owner)
    : owner_(owner),
      true_target_(NULL),
      false_target_(NULL),
      previous_(NULL) {
  owner_->set_state(this);
}


CodeGenState::CodeGenState(CodeGenerator* owner,
                           JumpTarget* true_target,
                           JumpTarget* false_target)
    : owner_(owner),
      true_target_(true_target),
      false_target_(false_target),
      previous_(owner->state()) {
  owner_->set_state(this);
}


CodeGenState::~CodeGenState() {
  ASSERT(owner_->state() == this);
  owner_->set_state(previous_);
}


// -----------------------------------------------------------------------------
// CodeGenerator implementation

CodeGenerator::CodeGenerator(MacroAssembler* masm)
    : deferred_(8),
      masm_(masm),
      frame_(NULL),
      allocator_(NULL),
      cc_reg_(cc_always),
      state_(NULL),
      function_return_is_shadowed_(false) {
}


// Calling conventions:
// fp: caller's frame pointer
// sp: stack pointer
// a1: called JS function
// cp: callee's context

void CodeGenerator::Generate(CompilationInfo* info) {
  // Record the position for debugging purposes.
  CodeForFunctionPosition(info->function());

  // Initialize state.
  info_ = info;
  ASSERT(allocator_ == NULL);
  RegisterAllocator register_allocator(this);
  allocator_ = &register_allocator;
  ASSERT(frame_ == NULL);
  frame_ = new VirtualFrame();
  cc_reg_ = cc_always;

  {
    CodeGenState state(this);

    // Registers:
    // a1: called JS function
    // ra: return address
    // fp: caller's frame pointer
    // sp: stack pointer
    // cp: callee's context
    //
    // Stack:
    // arguments
    // receiver

    frame_->Enter();

    // Allocate space for locals and initialize them.
    frame_->AllocateStackSlots();

    // Initialize the function return target.
    function_return_.set_direction(JumpTarget::BIDIRECTIONAL);
    function_return_is_shadowed_ = false;

    VirtualFrame::SpilledScope spilled_scope;
    if (scope()->num_heap_slots() > 0) {
      UNIMPLEMENTED_MIPS();
    }

    {
      Comment cmnt2(masm_, "[ copy context parameters into .context");

      // Note that iteration order is relevant here! If we have the same
      // parameter twice (e.g., function (x, y, x)), and that parameter
      // needs to be copied into the context, it must be the last argument
      // passed to the parameter that needs to be copied. This is a rare
      // case so we don't check for it, instead we rely on the copying
      // order: such a parameter is copied repeatedly into the same
      // context location and thus the last value is what is seen inside
      // the function.
      for (int i = 0; i < scope()->num_parameters(); i++) {
        UNIMPLEMENTED_MIPS();
      }
    }

    // Store the arguments object.  This must happen after context
    // initialization because the arguments object may be stored in the
    // context.
    if (scope()->arguments() != NULL) {
      UNIMPLEMENTED_MIPS();
    }

    // Generate code to 'execute' declarations and initialize functions
    // (source elements). In case of an illegal redeclaration we need to
    // handle that instead of processing the declarations.
    if (scope()->HasIllegalRedeclaration()) {
      Comment cmnt(masm_, "[ illegal redeclarations");
      scope()->VisitIllegalRedeclaration(this);
    } else {
      Comment cmnt(masm_, "[ declarations");
      ProcessDeclarations(scope()->declarations());
      // Bail out if a stack-overflow exception occurred when processing
      // declarations.
      if (HasStackOverflow()) return;
    }

    if (FLAG_trace) {
      UNIMPLEMENTED_MIPS();
    }

    // Compile the body of the function in a vanilla state. Don't
    // bother compiling all the code if the scope has an illegal
    // redeclaration.
    if (!scope()->HasIllegalRedeclaration()) {
      Comment cmnt(masm_, "[ function body");
#ifdef DEBUG
      bool is_builtin = Bootstrapper::IsActive();
      bool should_trace =
          is_builtin ? FLAG_trace_builtin_calls : FLAG_trace_calls;
      if (should_trace) {
        UNIMPLEMENTED_MIPS();
      }
#endif
      VisitStatementsAndSpill(info->function()->body());
    }
  }

  if (has_valid_frame() || function_return_.is_linked()) {
    if (!function_return_.is_linked()) {
      CodeForReturnPosition(info->function());
    }
    // Registers:
    // v0: result
    // sp: stack pointer
    // fp: frame pointer
    // cp: callee's context

    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);

    function_return_.Bind();
    if (FLAG_trace) {
      UNIMPLEMENTED_MIPS();
    }

    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);

    masm_->mov(sp, fp);
    masm_->lw(fp, MemOperand(sp, 0));
    masm_->lw(ra, MemOperand(sp, 4));
    masm_->addiu(sp, sp, 8);

    // Here we use masm_-> instead of the __ macro to avoid the code coverage
    // tool from instrumenting as we rely on the code size here.
    // TODO(MIPS): Should we be able to use more than 0x1ffe parameters?
    masm_->addiu(sp, sp, (scope()->num_parameters() + 1) * kPointerSize);
    masm_->Jump(ra);
    // The Jump automatically generates a nop in the branch delay slot.

    // Check that the size of the code used for returning matches what is
    // expected by the debugger.
    ASSERT_EQ(kJSReturnSequenceLength,
              masm_->InstructionsGeneratedSince(&check_exit_codesize));
  }

  // Code generation state must be reset.
  ASSERT(!has_cc());
  ASSERT(state_ == NULL);
  ASSERT(!function_return_is_shadowed_);
  function_return_.Unuse();
  DeleteFrame();

  // Process any deferred code using the register allocator.
  if (!HasStackOverflow()) {
    ProcessDeferred();
  }

  allocator_ = NULL;
}


void CodeGenerator::LoadReference(Reference* ref) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ LoadReference");
  Expression* e = ref->expression();
  Property* property = e->AsProperty();
  Variable* var = e->AsVariableProxy()->AsVariable();

  if (property != NULL) {
    UNIMPLEMENTED_MIPS();
  } else if (var != NULL) {
    // The expression is a variable proxy that does not rewrite to a
    // property.  Global variables are treated as named property references.
    if (var->is_global()) {
      LoadGlobal();
      ref->set_type(Reference::NAMED);
    } else {
      ASSERT(var->slot() != NULL);
      ref->set_type(Reference::SLOT);
    }
  } else {
    UNIMPLEMENTED_MIPS();
  }
}


void CodeGenerator::UnloadReference(Reference* ref) {
  VirtualFrame::SpilledScope spilled_scope;
  // Pop a reference from the stack while preserving TOS.
  Comment cmnt(masm_, "[ UnloadReference");
  int size = ref->size();
  if (size > 0) {
    frame_->EmitPop(a0);
    frame_->Drop(size);
    frame_->EmitPush(a0);
  }
  ref->set_unloaded();
}


MemOperand CodeGenerator::SlotOperand(Slot* slot, Register tmp) {
  // Currently, this assertion will fail if we try to assign to
  // a constant variable that is constant because it is read-only
  // (such as the variable referring to a named function expression).
  // We need to implement assignments to read-only variables.
  // Ideally, we should do this during AST generation (by converting
  // such assignments into expression statements); however, in general
  // we may not be able to make the decision until past AST generation,
  // that is when the entire program is known.
  ASSERT(slot != NULL);
  int index = slot->index();
  switch (slot->type()) {
    case Slot::PARAMETER:
      UNIMPLEMENTED_MIPS();
      return MemOperand(no_reg, 0);

    case Slot::LOCAL:
      return frame_->LocalAt(index);

    case Slot::CONTEXT: {
      UNIMPLEMENTED_MIPS();
      return MemOperand(no_reg, 0);
    }

    default:
      UNREACHABLE();
      return MemOperand(no_reg, 0);
  }
}


// Loads a value on TOS. If it is a boolean value, the result may have been
// (partially) translated into branches, or it may have set the condition
// code register. If force_cc is set, the value is forced to set the
// condition code register and no value is pushed. If the condition code
// register was set, has_cc() is true and cc_reg_ contains the condition to
// test for 'true'.
void CodeGenerator::LoadCondition(Expression* x,
                                  JumpTarget* true_target,
                                  JumpTarget* false_target,
                                  bool force_cc) {
  ASSERT(!has_cc());
  int original_height = frame_->height();

  { CodeGenState new_state(this, true_target, false_target);
    Visit(x);

    // If we hit a stack overflow, we may not have actually visited
    // the expression. In that case, we ensure that we have a
    // valid-looking frame state because we will continue to generate
    // code as we unwind the C++ stack.
    //
    // It's possible to have both a stack overflow and a valid frame
    // state (eg, a subexpression overflowed, visiting it returned
    // with a dummied frame state, and visiting this expression
    // returned with a normal-looking state).
    if (HasStackOverflow() &&
        has_valid_frame() &&
        !has_cc() &&
        frame_->height() == original_height) {
      true_target->Jump();
    }
  }
  if (force_cc && frame_ != NULL && !has_cc()) {
    // Convert the TOS value to a boolean in the condition code register.
    UNIMPLEMENTED_MIPS();
  }
  ASSERT(!force_cc || !has_valid_frame() || has_cc());
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::Load(Expression* x) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  JumpTarget true_target;
  JumpTarget false_target;
  LoadCondition(x, &true_target, &false_target, false);

  if (has_cc()) {
    UNIMPLEMENTED_MIPS();
  }

  if (true_target.is_linked() || false_target.is_linked()) {
    UNIMPLEMENTED_MIPS();
  }
  ASSERT(has_valid_frame());
  ASSERT(!has_cc());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::LoadGlobal() {
  VirtualFrame::SpilledScope spilled_scope;
  __ lw(a0, GlobalObject());
  frame_->EmitPush(a0);
}


void CodeGenerator::LoadFromSlot(Slot* slot, TypeofState typeof_state) {
  VirtualFrame::SpilledScope spilled_scope;
  if (slot->type() == Slot::LOOKUP) {
    UNIMPLEMENTED_MIPS();
  } else {
    __ lw(a0, SlotOperand(slot, a2));
    frame_->EmitPush(a0);
    if (slot->var()->mode() == Variable::CONST) {
      UNIMPLEMENTED_MIPS();
    }
  }
}


void CodeGenerator::StoreToSlot(Slot* slot, InitState init_state) {
  ASSERT(slot != NULL);
  if (slot->type() == Slot::LOOKUP) {
      UNIMPLEMENTED_MIPS();
  } else {
    ASSERT(!slot->var()->is_dynamic());

    JumpTarget exit;
    if (init_state == CONST_INIT) {
      UNIMPLEMENTED_MIPS();
    }

    // We must execute the store. Storing a variable must keep the
    // (new) value on the stack. This is necessary for compiling
    // assignment expressions.
    //
    // Note: We will reach here even with slot->var()->mode() ==
    // Variable::CONST because of const declarations which will
    // initialize consts to 'the hole' value and by doing so, end up
    // calling this code. a2 may be loaded with context; used below in
    // RecordWrite.
    frame_->EmitPop(a0);
    __ sw(a0, SlotOperand(slot, a2));
    frame_->EmitPush(a0);
    if (slot->type() == Slot::CONTEXT) {
      UNIMPLEMENTED_MIPS();
    }
    // If we definitely did not jump over the assignment, we do not need
    // to bind the exit label. Doing so can defeat peephole
    // optimization.
    if (init_state == CONST_INIT || slot->type() == Slot::CONTEXT) {
      exit.Bind();
    }
  }
}


void CodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
  VirtualFrame::SpilledScope spilled_scope;
  for (int i = 0; frame_ != NULL && i < statements->length(); i++) {
    VisitAndSpill(statements->at(i));
  }
}


void CodeGenerator::VisitBlock(Block* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  VirtualFrame::SpilledScope spilled_scope;
  frame_->EmitPush(cp);
  __ li(t0, Operand(pairs));
  frame_->EmitPush(t0);
  __ li(t0, Operand(Smi::FromInt(is_eval() ? 1 : 0)));
  frame_->EmitPush(t0);
  frame_->CallRuntime(Runtime::kDeclareGlobals, 3);
  // The result is discarded.
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitExpressionStatement(ExpressionStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ExpressionStatement");
  CodeForStatementPosition(node);
  Expression* expression = node->expression();
  expression->MarkAsStatement();
  LoadAndSpill(expression);
  frame_->Drop();
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitEmptyStatement(EmptyStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitIfStatement(IfStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitContinueStatement(ContinueStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitBreakStatement(BreakStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ReturnStatement");

  CodeForStatementPosition(node);
  LoadAndSpill(node->expression());
  if (function_return_is_shadowed_) {
    frame_->EmitPop(v0);
    function_return_.Jump();
  } else {
    // Pop the result from the frame and prepare the frame for
    // returning thus making it easier to merge.
    frame_->EmitPop(v0);
    frame_->PrepareForReturn();

    function_return_.Jump();
  }
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitWithExitStatement(WithExitStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitDoWhileStatement(DoWhileStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitWhileStatement(WhileStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitForStatement(ForStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitForInStatement(ForInStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitTryCatchStatement(TryCatchStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitDebuggerStatement(DebuggerStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitConditional(Conditional* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitSlot(Slot* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Slot");
  LoadFromSlot(node, typeof_state());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitVariableProxy(VariableProxy* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ VariableProxy");

  Variable* var = node->var();
  Expression* expr = var->rewrite();
  if (expr != NULL) {
    Visit(expr);
  } else {
    ASSERT(var->is_global());
    Reference ref(this, node);
    ref.GetValueAndSpill();
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitLiteral(Literal* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Literal");
  __ li(t0, Operand(node->handle()));
  frame_->EmitPush(t0);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Assignment");

  { Reference target(this, node->target());
    if (target.is_illegal()) {
      // Fool the virtual frame into thinking that we left the assignment's
      // value on the frame.
      frame_->EmitPush(zero_reg);
      ASSERT(frame_->height() == original_height + 1);
      return;
    }

    if (node->op() == Token::ASSIGN ||
        node->op() == Token::INIT_VAR ||
        node->op() == Token::INIT_CONST) {
      LoadAndSpill(node->value());
    } else {
      UNIMPLEMENTED_MIPS();
    }

    Variable* var = node->target()->AsVariableProxy()->AsVariable();
    if (var != NULL &&
        (var->mode() == Variable::CONST) &&
        node->op() != Token::INIT_VAR && node->op() != Token::INIT_CONST) {
      // Assignment ignored - leave the value on the stack.
    } else {
      CodeForSourcePosition(node->position());
      if (node->op() == Token::INIT_CONST) {
        // Dynamic constant initializations must use the function context
        // and initialize the actual constant declared. Dynamic variable
        // initializations are simply assignments and use SetValue.
        target.SetValue(CONST_INIT);
      } else {
        target.SetValue(NOT_CONST_INIT);
      }
    }
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitThrow(Throw* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitProperty(Property* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCall(Call* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Call");

  Expression* function = node->expression();
  ZoneList<Expression*>* args = node->arguments();

  // Standard function call.
  // Check if the function is a variable or a property.
  Variable* var = function->AsVariableProxy()->AsVariable();
  Property* property = function->AsProperty();

  // ------------------------------------------------------------------------
  // Fast-case: Use inline caching.
  // ---
  // According to ECMA-262, section 11.2.3, page 44, the function to call
  // must be resolved after the arguments have been evaluated. The IC code
  // automatically handles this by loading the arguments before the function
  // is resolved in cache misses (this also holds for megamorphic calls).
  // ------------------------------------------------------------------------

  if (var != NULL && var->is_possibly_eval()) {
    UNIMPLEMENTED_MIPS();
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is global
    // ----------------------------------

    int arg_count = args->length();

    // We need sp to be 8 bytes aligned when calling the stub.
    __ SetupAlignedCall(t0, arg_count);

    // Pass the global object as the receiver and let the IC stub
    // patch the stack to use the global proxy as 'this' in the
    // invoked function.
    LoadGlobal();

    // Load the arguments.
    for (int i = 0; i < arg_count; i++) {
      LoadAndSpill(args->at(i));
    }

    // Setup the receiver register and call the IC initialization code.
    __ li(a2, Operand(var->name()));
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
    CodeForSourcePosition(node->position());
    frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET_CONTEXT,
                           arg_count + 1);
    __ ReturnFromAlignedCall();
    __ lw(cp, frame_->Context());
    // Remove the function from the stack.
    frame_->EmitPush(v0);

  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    UNIMPLEMENTED_MIPS();
  } else if (property != NULL) {
    UNIMPLEMENTED_MIPS();
  } else {
    UNIMPLEMENTED_MIPS();
  }

  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitCallNew(CallNew* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateClassOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateSetValueOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateLog(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateMathPow(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateMathCos(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateMathSin(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateMathSqrt(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


// This should generate code that performs a charCodeAt() call or returns
// undefined in order to trigger the slow case, Runtime_StringCharCodeAt.
// It is not yet implemented on ARM, so it always goes to the slow case.
void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateCharFromCode(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsRegExp(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateArguments(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateRandomPositiveSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsObject(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsFunction(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsUndetectableObject(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateStringAdd(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateSubString(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateStringCompare(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateRegExpExec(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateNumberToString(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
  UNIMPLEMENTED_MIPS();
}


#ifdef DEBUG
bool CodeGenerator::HasValidEntryRegisters() { return true; }
#endif


#undef __
#define __ ACCESS_MASM(masm)

// -----------------------------------------------------------------------------
// Reference support

Reference::Reference(CodeGenerator* cgen,
                     Expression* expression,
                     bool persist_after_get)
    : cgen_(cgen),
      expression_(expression),
      type_(ILLEGAL),
      persist_after_get_(persist_after_get) {
  cgen->LoadReference(this);
}


Reference::~Reference() {
  ASSERT(is_unloaded() || is_illegal());
}


Handle<String> Reference::GetName() {
  ASSERT(type_ == NAMED);
  Property* property = expression_->AsProperty();
  if (property == NULL) {
    // Global variable reference treated as a named property reference.
    VariableProxy* proxy = expression_->AsVariableProxy();
    ASSERT(proxy->AsVariable() != NULL);
    ASSERT(proxy->AsVariable()->is_global());
    return proxy->name();
  } else {
    Literal* raw_name = property->key()->AsLiteral();
    ASSERT(raw_name != NULL);
    return Handle<String>(String::cast(*raw_name->handle()));
  }
}


void Reference::GetValue() {
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_illegal());
  ASSERT(!cgen_->has_cc());
  Property* property = expression_->AsProperty();
  if (property != NULL) {
    cgen_->CodeForSourcePosition(property->position());
  }

  switch (type_) {
    case SLOT: {
      UNIMPLEMENTED_MIPS();
      break;
    }

    case NAMED: {
      UNIMPLEMENTED_MIPS();
      break;
    }

    case KEYED: {
      UNIMPLEMENTED_MIPS();
      break;
    }

    default:
      UNREACHABLE();
  }
}


void Reference::SetValue(InitState init_state) {
  ASSERT(!is_illegal());
  ASSERT(!cgen_->has_cc());
  MacroAssembler* masm = cgen_->masm();
  Property* property = expression_->AsProperty();
  if (property != NULL) {
    cgen_->CodeForSourcePosition(property->position());
  }

  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Store to Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      cgen_->StoreToSlot(slot, init_state);
      cgen_->UnloadReference(this);
      break;
    }

    case NAMED: {
      UNIMPLEMENTED_MIPS();
      break;
    }

    case KEYED: {
      UNIMPLEMENTED_MIPS();
      break;
    }

    default:
      UNREACHABLE();
  }
}


// On entry a0 and a1 are the things to be compared. On exit v0 is 0,
// positive or negative to indicate the result of the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x765);
}


Handle<Code> GetBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info) {
  UNIMPLEMENTED_MIPS();
  return Handle<Code>::null();
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x790);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x808);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x815);
}

void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate) {
  // s0: number of arguments including receiver (C callee-saved)
  // s1: pointer to the first argument          (C callee-saved)
  // s2: pointer to builtin function            (C callee-saved)

  if (do_gc) {
    UNIMPLEMENTED_MIPS();
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate) {
    UNIMPLEMENTED_MIPS();
  }

  // Call C built-in.
  // a0 = argc, a1 = argv
  __ mov(a0, s0);
  __ mov(a1, s1);

  __ CallBuiltin(s2);

  if (always_allocate) {
    UNIMPLEMENTED_MIPS();
  }

  // Check for failure result.
  Label failure_returned;
  ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ addiu(a2, v0, 1);
  __ andi(t0, a2, kFailureTagMask);
  __ Branch(eq, &failure_returned, t0, Operand(zero_reg));

  // Exit C frame and return.
  // v0:v1: result
  // sp: stack pointer
  // fp: frame pointer
  __ LeaveExitFrame(mode_);

  // Check if we should retry or throw exception.
  Label retry;
  __ bind(&failure_returned);
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ andi(t0, v0, ((1 << kFailureTypeTagSize) - 1) << kFailureTagSize);
  __ Branch(eq, &retry, t0, Operand(zero_reg));

  // Special handling of out of memory exceptions.
  Failure* out_of_memory = Failure::OutOfMemoryException();
  __ Branch(eq, throw_out_of_memory_exception,
            v0, Operand(reinterpret_cast<int32_t>(out_of_memory)));

  // Retrieve the pending exception and clear the variable.
  __ LoadExternalReference(t0, ExternalReference::the_hole_value_location());
  __ lw(a3, MemOperand(t0));
  __ LoadExternalReference(t0,
      ExternalReference(Top::k_pending_exception_address));
  __ lw(v0, MemOperand(t0));
  __ sw(a3, MemOperand(t0));

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ Branch(eq, throw_termination_exception,
            v0, Operand(Factory::termination_exception()));

  // Handle normal exception.
  __ b(throw_normal_exception);
  __ nop();   // Branch delay slot nop.

  __ bind(&retry);  // pass last failure (r0) as parameter (r0) when retrying
}

void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(mode_, s0, s1, s2);

  // s0: number of arguments (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  Label throw_normal_exception;
  Label throw_termination_exception;
  Label throw_out_of_memory_exception;

  // Call into the runtime system.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               false,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ li(v0, Operand(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  GenerateThrowUncatchable(masm, OUT_OF_MEMORY);

  __ bind(&throw_termination_exception);
  GenerateThrowUncatchable(masm, TERMINATION);

  __ bind(&throw_normal_exception);
  GenerateThrowTOS(masm);
}

void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, exit;

  // Registers:
  // a0: entry address
  // a1: function
  // a2: reveiver
  // a3: argc
  //
  // Stack:
  // 4 args slots
  // args

  // Save callee saved registers on the stack.
  __ MultiPush((kCalleeSaved | ra.bit()) & ~sp.bit());

  // We build an EntryFrame.
  __ li(t3, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ li(t2, Operand(Smi::FromInt(marker)));
  __ li(t1, Operand(Smi::FromInt(marker)));
  __ LoadExternalReference(t0, ExternalReference(Top::k_c_entry_fp_address));
  __ lw(t0, MemOperand(t0));
  __ MultiPush(t0.bit() | t1.bit() | t2.bit() | t3.bit());

  // Setup frame pointer for the frame to be pushed.
  __ addiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Load argv in s0 register.
  __ lw(s0, MemOperand(sp, (kNumCalleeSaved + 1) * kPointerSize +
                           StandardFrameConstants::kCArgsSlotsSize));

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xff...f)  |
  // callee saved registers + ra
  // 4 args slots
  // args

  // Call a faked try-block that does the invoke.
  __ bal(&invoke);
  __ nop();   // Branch delay slot nop.

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  // Coming in here the fp will be invalid because the PushTryHandler below
  // sets it to 0 to signal the existence of the JSEntry frame.
  __ LoadExternalReference(t0,
      ExternalReference(Top::k_pending_exception_address));
  __ sw(v0, MemOperand(t0));  // We come back from 'invoke'. result is in v0.
  __ li(v0, Operand(reinterpret_cast<int32_t>(Failure::Exception())));
  __ b(&exit);
  __ nop();   // Branch delay slot nop.

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ LoadExternalReference(t0, ExternalReference::the_hole_value_location());
  __ lw(t1, MemOperand(t0));
  __ LoadExternalReference(t0,
      ExternalReference(Top::k_pending_exception_address));
  __ sw(t1, MemOperand(t0));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // 4 args slots
  // args

  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ LoadExternalReference(t0, construct_entry);
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ LoadExternalReference(t0, entry);
  }
  __ lw(t9, MemOperand(t0));  // deref address

  // Call JSEntryTrampoline.
  __ addiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  __ CallBuiltin(t9);

  // Unlink this frame from the handler chain. When reading the
  // address of the next handler, there is no need to use the address
  // displacement since the current stack pointer (sp) points directly
  // to the stack handler.
  __ lw(t1, MemOperand(sp, StackHandlerConstants::kNextOffset));
  __ LoadExternalReference(t0, ExternalReference(Top::k_handler_address));
  __ sw(t1, MemOperand(t0));

  // This restores sp to its position before PushTryHandler.
  __ addiu(sp, sp, StackHandlerConstants::kSize);

  __ bind(&exit);  // v0 holds result
  // Restore the top frame descriptors from the stack.
  __ Pop(t1);
  __ LoadExternalReference(t0, ExternalReference(Top::k_c_entry_fp_address));
  __ sw(t1, MemOperand(t0));

  // Reset the stack to the callee saved registers.
  __ addiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Restore callee saved registers from the stack.
  __ MultiPop((kCalleeSaved | ra.bit()) & ~sp.bit());
  // Return.
  __ Jump(ra);
}


// This stub performs an instanceof, calling the builtin function if
// necessary. Uses a1 for the object, a0 for the function that it may
// be an instance of (these are fetched from the stack).
void InstanceofStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x845);
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x851);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x857);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x863);
}


const char* CompareStub::GetName() {
  UNIMPLEMENTED_MIPS();
  return NULL;  // UNIMPLEMENTED RETURN
}


int CompareStub::MinorKey() {
  // Encode the two parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) >> 28 < (1 << 15));
  return (static_cast<unsigned>(cc_) >> 27) | (strict_ ? 1 : 0);
}


#undef __

} }  // namespace v8::internal
