// Copyright 2006-2009 the V8 project authors. All rights reserved.
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
#include "parser.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"


namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cc);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Label* rhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm);
static void MultiplyByKnownInt(MacroAssembler* masm,
                               Register source,
                               Register destination,
                               int known_int);
static bool IsEasyToMultiplyBy(int x);



// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.

void DeferredCode::SaveRegisters() {
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    int action = registers_[i];
    if (action == kPush) {
      __ push(RegisterAllocator::ToRegister(i));
    } else if (action != kIgnore && (action & kSyncedFlag) == 0) {
      __ str(RegisterAllocator::ToRegister(i), MemOperand(fp, action));
    }
  }
}


void DeferredCode::RestoreRegisters() {
  // Restore registers in reverse order due to the stack.
  for (int i = RegisterAllocator::kNumRegisters - 1; i >= 0; i--) {
    int action = registers_[i];
    if (action == kPush) {
      __ pop(RegisterAllocator::ToRegister(i));
    } else if (action != kIgnore) {
      action &= ~kSyncedFlag;
      __ ldr(RegisterAllocator::ToRegister(i), MemOperand(fp, action));
    }
  }
}


// -------------------------------------------------------------------------
// CodeGenState implementation.

CodeGenState::CodeGenState(CodeGenerator* owner)
    : owner_(owner),
      typeof_state_(NOT_INSIDE_TYPEOF),
      true_target_(NULL),
      false_target_(NULL),
      previous_(NULL) {
  owner_->set_state(this);
}


CodeGenState::CodeGenState(CodeGenerator* owner,
                           TypeofState typeof_state,
                           JumpTarget* true_target,
                           JumpTarget* false_target)
    : owner_(owner),
      typeof_state_(typeof_state),
      true_target_(true_target),
      false_target_(false_target),
      previous_(owner->state()) {
  owner_->set_state(this);
}


CodeGenState::~CodeGenState() {
  ASSERT(owner_->state() == this);
  owner_->set_state(previous_);
}


// -------------------------------------------------------------------------
// CodeGenerator implementation

CodeGenerator::CodeGenerator(int buffer_size, Handle<Script> script,
                             bool is_eval)
    : is_eval_(is_eval),
      script_(script),
      deferred_(8),
      masm_(new MacroAssembler(NULL, buffer_size)),
      scope_(NULL),
      frame_(NULL),
      allocator_(NULL),
      cc_reg_(al),
      state_(NULL),
      function_return_is_shadowed_(false) {
}


// Calling conventions:
// fp: caller's frame pointer
// sp: stack pointer
// r1: called JS function
// cp: callee's context

void CodeGenerator::GenCode(FunctionLiteral* fun) {
  ZoneList<Statement*>* body = fun->body();

  // Initialize state.
  ASSERT(scope_ == NULL);
  scope_ = fun->scope();
  ASSERT(allocator_ == NULL);
  RegisterAllocator register_allocator(this);
  allocator_ = &register_allocator;
  ASSERT(frame_ == NULL);
  frame_ = new VirtualFrame();
  cc_reg_ = al;
  {
    CodeGenState state(this);

    // Entry:
    // Stack: receiver, arguments
    // lr: return address
    // fp: caller's frame pointer
    // sp: stack pointer
    // r1: called JS function
    // cp: callee's context
    allocator_->Initialize();
    frame_->Enter();
    // tos: code slot
#ifdef DEBUG
    if (strlen(FLAG_stop_at) > 0 &&
        fun->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
      frame_->SpillAll();
      __ stop("stop-at");
    }
#endif

    // Allocate space for locals and initialize them.  This also checks
    // for stack overflow.
    frame_->AllocateStackSlots();
    // Initialize the function return target after the locals are set
    // up, because it needs the expected frame height from the frame.
    function_return_.set_direction(JumpTarget::BIDIRECTIONAL);
    function_return_is_shadowed_ = false;

    VirtualFrame::SpilledScope spilled_scope;
    if (scope_->num_heap_slots() > 0) {
      // Allocate local context.
      // Get outer context and create a new context based on it.
      __ ldr(r0, frame_->Function());
      frame_->EmitPush(r0);
      frame_->CallRuntime(Runtime::kNewContext, 1);  // r0 holds the result

#ifdef DEBUG
      JumpTarget verified_true;
      __ cmp(r0, Operand(cp));
      verified_true.Branch(eq);
      __ stop("NewContext: r0 is expected to be the same as cp");
      verified_true.Bind();
#endif
      // Update context local.
      __ str(cp, frame_->Context());
    }

    // TODO(1241774): Improve this code:
    // 1) only needed if we have a context
    // 2) no need to recompute context ptr every single time
    // 3) don't copy parameter operand code from SlotOperand!
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
      for (int i = 0; i < scope_->num_parameters(); i++) {
        Variable* par = scope_->parameter(i);
        Slot* slot = par->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          ASSERT(!scope_->is_global_scope());  // no parameters in global scope
          __ ldr(r1, frame_->ParameterAt(i));
          // Loads r2 with context; used below in RecordWrite.
          __ str(r1, SlotOperand(slot, r2));
          // Load the offset into r3.
          int slot_offset =
              FixedArray::kHeaderSize + slot->index() * kPointerSize;
          __ mov(r3, Operand(slot_offset));
          __ RecordWrite(r2, r3, r1);
        }
      }
    }

    // Store the arguments object.  This must happen after context
    // initialization because the arguments object may be stored in the
    // context.
    if (scope_->arguments() != NULL) {
      ASSERT(scope_->arguments_shadow() != NULL);
      Comment cmnt(masm_, "[ allocate arguments object");
      { Reference shadow_ref(this, scope_->arguments_shadow());
        { Reference arguments_ref(this, scope_->arguments());
          ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
          __ ldr(r2, frame_->Function());
          // The receiver is below the arguments, the return address,
          // and the frame pointer on the stack.
          const int kReceiverDisplacement = 2 + scope_->num_parameters();
          __ add(r1, fp, Operand(kReceiverDisplacement * kPointerSize));
          __ mov(r0, Operand(Smi::FromInt(scope_->num_parameters())));
          frame_->Adjust(3);
          __ stm(db_w, sp, r0.bit() | r1.bit() | r2.bit());
          frame_->CallStub(&stub, 3);
          frame_->EmitPush(r0);
          arguments_ref.SetValue(NOT_CONST_INIT);
        }
        shadow_ref.SetValue(NOT_CONST_INIT);
      }
      frame_->Drop();  // Value is no longer needed.
    }

    // Generate code to 'execute' declarations and initialize functions
    // (source elements). In case of an illegal redeclaration we need to
    // handle that instead of processing the declarations.
    if (scope_->HasIllegalRedeclaration()) {
      Comment cmnt(masm_, "[ illegal redeclarations");
      scope_->VisitIllegalRedeclaration(this);
    } else {
      Comment cmnt(masm_, "[ declarations");
      ProcessDeclarations(scope_->declarations());
      // Bail out if a stack-overflow exception occurred when processing
      // declarations.
      if (HasStackOverflow()) return;
    }

    if (FLAG_trace) {
      frame_->CallRuntime(Runtime::kTraceEnter, 0);
      // Ignore the return value.
    }

    // Compile the body of the function in a vanilla state. Don't
    // bother compiling all the code if the scope has an illegal
    // redeclaration.
    if (!scope_->HasIllegalRedeclaration()) {
      Comment cmnt(masm_, "[ function body");
#ifdef DEBUG
      bool is_builtin = Bootstrapper::IsActive();
      bool should_trace =
          is_builtin ? FLAG_trace_builtin_calls : FLAG_trace_calls;
      if (should_trace) {
        frame_->CallRuntime(Runtime::kDebugTrace, 0);
        // Ignore the return value.
      }
#endif
      VisitStatementsAndSpill(body);
    }
  }

  // Generate the return sequence if necessary.
  if (has_valid_frame() || function_return_.is_linked()) {
    if (!function_return_.is_linked()) {
      CodeForReturnPosition(fun);
    }
    // exit
    // r0: result
    // sp: stack pointer
    // fp: frame pointer
    // cp: callee's context
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);

    function_return_.Bind();
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns the parameter as it is.
      frame_->EmitPush(r0);
      frame_->CallRuntime(Runtime::kTraceExit, 1);
    }

    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);

    // Tear down the frame which will restore the caller's frame pointer and
    // the link register.
    frame_->Exit();

    // Here we use masm_-> instead of the __ macro to avoid the code coverage
    // tool from instrumenting as we rely on the code size here.
    masm_->add(sp, sp, Operand((scope_->num_parameters() + 1) * kPointerSize));
    masm_->Jump(lr);

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
  scope_ = NULL;
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
      return frame_->ParameterAt(index);

    case Slot::LOCAL:
      return frame_->LocalAt(index);

    case Slot::CONTEXT: {
      // Follow the context chain if necessary.
      ASSERT(!tmp.is(cp));  // do not overwrite context register
      Register context = cp;
      int chain_length = scope()->ContextChainLength(slot->var()->scope());
      for (int i = 0; i < chain_length; i++) {
        // Load the closure.
        // (All contexts, even 'with' contexts, have a closure,
        // and it is the same for all contexts inside a function.
        // There is no need to go to the function context first.)
        __ ldr(tmp, ContextOperand(context, Context::CLOSURE_INDEX));
        // Load the function context (which is the incoming, outer context).
        __ ldr(tmp, FieldMemOperand(tmp, JSFunction::kContextOffset));
        context = tmp;
      }
      // We may have a 'with' context now. Get the function context.
      // (In fact this mov may never be the needed, since the scope analysis
      // may not permit a direct context access in this case and thus we are
      // always at a function context. However it is safe to dereference be-
      // cause the function context of a function context is itself. Before
      // deleting this mov we should try to create a counter-example first,
      // though...)
      __ ldr(tmp, ContextOperand(context, Context::FCONTEXT_INDEX));
      return ContextOperand(tmp, index);
    }

    default:
      UNREACHABLE();
      return MemOperand(r0, 0);
  }
}


MemOperand CodeGenerator::ContextSlotOperandCheckExtensions(
    Slot* slot,
    Register tmp,
    Register tmp2,
    JumpTarget* slow) {
  ASSERT(slot->type() == Slot::CONTEXT);
  Register context = cp;

  for (Scope* s = scope(); s != slot->var()->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ ldr(tmp2, ContextOperand(context, Context::EXTENSION_INDEX));
        __ tst(tmp2, tmp2);
        slow->Branch(ne);
      }
      __ ldr(tmp, ContextOperand(context, Context::CLOSURE_INDEX));
      __ ldr(tmp, FieldMemOperand(tmp, JSFunction::kContextOffset));
      context = tmp;
    }
  }
  // Check that last extension is NULL.
  __ ldr(tmp2, ContextOperand(context, Context::EXTENSION_INDEX));
  __ tst(tmp2, tmp2);
  slow->Branch(ne);
  __ ldr(tmp, ContextOperand(context, Context::FCONTEXT_INDEX));
  return ContextOperand(tmp, slot->index());
}


// Loads a value on TOS. If it is a boolean value, the result may have been
// (partially) translated into branches, or it may have set the condition
// code register. If force_cc is set, the value is forced to set the
// condition code register and no value is pushed. If the condition code
// register was set, has_cc() is true and cc_reg_ contains the condition to
// test for 'true'.
void CodeGenerator::LoadCondition(Expression* x,
                                  TypeofState typeof_state,
                                  JumpTarget* true_target,
                                  JumpTarget* false_target,
                                  bool force_cc) {
  ASSERT(!has_cc());
  int original_height = frame_->height();

  { CodeGenState new_state(this, typeof_state, true_target, false_target);
    Visit(x);

    // If we hit a stack overflow, we may not have actually visited
    // the expression.  In that case, we ensure that we have a
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
    ToBoolean(true_target, false_target);
  }
  ASSERT(!force_cc || !has_valid_frame() || has_cc());
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::Load(Expression* x, TypeofState typeof_state) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  JumpTarget true_target;
  JumpTarget false_target;
  LoadCondition(x, typeof_state, &true_target, &false_target, false);

  if (has_cc()) {
    // Convert cc_reg_ into a boolean value.
    JumpTarget loaded;
    JumpTarget materialize_true;
    materialize_true.Branch(cc_reg_);
    __ LoadRoot(r0, Heap::kFalseValueRootIndex);
    frame_->EmitPush(r0);
    loaded.Jump();
    materialize_true.Bind();
    __ LoadRoot(r0, Heap::kTrueValueRootIndex);
    frame_->EmitPush(r0);
    loaded.Bind();
    cc_reg_ = al;
  }

  if (true_target.is_linked() || false_target.is_linked()) {
    // We have at least one condition value that has been "translated"
    // into a branch, thus it needs to be loaded explicitly.
    JumpTarget loaded;
    if (frame_ != NULL) {
      loaded.Jump();  // Don't lose the current TOS.
    }
    bool both = true_target.is_linked() && false_target.is_linked();
    // Load "true" if necessary.
    if (true_target.is_linked()) {
      true_target.Bind();
      __ LoadRoot(r0, Heap::kTrueValueRootIndex);
      frame_->EmitPush(r0);
    }
    // If both "true" and "false" need to be loaded jump across the code for
    // "false".
    if (both) {
      loaded.Jump();
    }
    // Load "false" if necessary.
    if (false_target.is_linked()) {
      false_target.Bind();
      __ LoadRoot(r0, Heap::kFalseValueRootIndex);
      frame_->EmitPush(r0);
    }
    // A value is loaded on all paths reaching this point.
    loaded.Bind();
  }
  ASSERT(has_valid_frame());
  ASSERT(!has_cc());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::LoadGlobal() {
  VirtualFrame::SpilledScope spilled_scope;
  __ ldr(r0, GlobalObject());
  frame_->EmitPush(r0);
}


void CodeGenerator::LoadGlobalReceiver(Register scratch) {
  VirtualFrame::SpilledScope spilled_scope;
  __ ldr(scratch, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(scratch,
         FieldMemOperand(scratch, GlobalObject::kGlobalReceiverOffset));
  frame_->EmitPush(scratch);
}


// TODO(1241834): Get rid of this function in favor of just using Load, now
// that we have the INSIDE_TYPEOF typeof state. => Need to handle global
// variables w/o reference errors elsewhere.
void CodeGenerator::LoadTypeofExpression(Expression* x) {
  VirtualFrame::SpilledScope spilled_scope;
  Variable* variable = x->AsVariableProxy()->AsVariable();
  if (variable != NULL && !variable->is_this() && variable->is_global()) {
    // NOTE: This is somewhat nasty. We force the compiler to load
    // the variable as if through '<global>.<variable>' to make sure we
    // do not get reference errors.
    Slot global(variable, Slot::CONTEXT, Context::GLOBAL_INDEX);
    Literal key(variable->name());
    // TODO(1241834): Fetch the position from the variable instead of using
    // no position.
    Property property(&global, &key, RelocInfo::kNoPosition);
    LoadAndSpill(&property);
  } else {
    LoadAndSpill(x, INSIDE_TYPEOF);
  }
}


Reference::Reference(CodeGenerator* cgen, Expression* expression)
    : cgen_(cgen), expression_(expression), type_(ILLEGAL) {
  cgen->LoadReference(this);
}


Reference::~Reference() {
  cgen_->UnloadReference(this);
}


void CodeGenerator::LoadReference(Reference* ref) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ LoadReference");
  Expression* e = ref->expression();
  Property* property = e->AsProperty();
  Variable* var = e->AsVariableProxy()->AsVariable();

  if (property != NULL) {
    // The expression is either a property or a variable proxy that rewrites
    // to a property.
    LoadAndSpill(property->obj());
    // We use a named reference if the key is a literal symbol, unless it is
    // a string that can be legally parsed as an integer.  This is because
    // otherwise we will not get into the slow case code that handles [] on
    // String objects.
    Literal* literal = property->key()->AsLiteral();
    uint32_t dummy;
    if (literal != NULL &&
        literal->handle()->IsSymbol() &&
        !String::cast(*(literal->handle()))->AsArrayIndex(&dummy)) {
      ref->set_type(Reference::NAMED);
    } else {
      LoadAndSpill(property->key());
      ref->set_type(Reference::KEYED);
    }
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
    // Anything else is a runtime error.
    LoadAndSpill(e);
    frame_->CallRuntime(Runtime::kThrowReferenceError, 1);
  }
}


void CodeGenerator::UnloadReference(Reference* ref) {
  VirtualFrame::SpilledScope spilled_scope;
  // Pop a reference from the stack while preserving TOS.
  Comment cmnt(masm_, "[ UnloadReference");
  int size = ref->size();
  if (size > 0) {
    frame_->EmitPop(r0);
    frame_->Drop(size);
    frame_->EmitPush(r0);
  }
}


// ECMA-262, section 9.2, page 30: ToBoolean(). Convert the given
// register to a boolean in the condition code register. The code
// may jump to 'false_target' in case the register converts to 'false'.
void CodeGenerator::ToBoolean(JumpTarget* true_target,
                              JumpTarget* false_target) {
  VirtualFrame::SpilledScope spilled_scope;
  // Note: The generated code snippet does not change stack variables.
  //       Only the condition code should be set.
  frame_->EmitPop(r0);

  // Fast case checks

  // Check if the value is 'false'.
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ cmp(r0, ip);
  false_target->Branch(eq);

  // Check if the value is 'true'.
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ cmp(r0, ip);
  true_target->Branch(eq);

  // Check if the value is 'undefined'.
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  false_target->Branch(eq);

  // Check if the value is a smi.
  __ cmp(r0, Operand(Smi::FromInt(0)));
  false_target->Branch(eq);
  __ tst(r0, Operand(kSmiTagMask));
  true_target->Branch(eq);

  // Slow case: call the runtime.
  frame_->EmitPush(r0);
  frame_->CallRuntime(Runtime::kToBool, 1);
  // Convert the result (r0) to a condition code.
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ cmp(r0, ip);

  cc_reg_ = ne;
}


void CodeGenerator::GenericBinaryOperation(Token::Value op,
                                           OverwriteMode overwrite_mode,
                                           int constant_rhs) {
  VirtualFrame::SpilledScope spilled_scope;
  // sp[0] : y
  // sp[1] : x
  // result : r0

  // Stub is entered with a call: 'return address' is in lr.
  switch (op) {
    case Token::ADD:  // fall through.
    case Token::SUB:  // fall through.
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      frame_->EmitPop(r0);  // r0 : y
      frame_->EmitPop(r1);  // r1 : x
      GenericBinaryOpStub stub(op, overwrite_mode, constant_rhs);
      frame_->CallStub(&stub, 0);
      break;
    }

    case Token::COMMA:
      frame_->EmitPop(r0);
      // simply discard left value
      frame_->Drop();
      break;

    default:
      // Other cases should have been handled before this point.
      UNREACHABLE();
      break;
  }
}


class DeferredInlineSmiOperation: public DeferredCode {
 public:
  DeferredInlineSmiOperation(Token::Value op,
                             int value,
                             bool reversed,
                             OverwriteMode overwrite_mode)
      : op_(op),
        value_(value),
        reversed_(reversed),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlinedSmiOperation");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  int value_;
  bool reversed_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiOperation::Generate() {
  switch (op_) {
    case Token::ADD: {
      // Revert optimistic add.
      if (reversed_) {
        __ sub(r0, r0, Operand(Smi::FromInt(value_)));
        __ mov(r1, Operand(Smi::FromInt(value_)));
      } else {
        __ sub(r1, r0, Operand(Smi::FromInt(value_)));
        __ mov(r0, Operand(Smi::FromInt(value_)));
      }
      break;
    }

    case Token::SUB: {
      // Revert optimistic sub.
      if (reversed_) {
        __ rsb(r0, r0, Operand(Smi::FromInt(value_)));
        __ mov(r1, Operand(Smi::FromInt(value_)));
      } else {
        __ add(r1, r0, Operand(Smi::FromInt(value_)));
        __ mov(r0, Operand(Smi::FromInt(value_)));
      }
      break;
    }

    // For these operations there is no optimistic operation that needs to be
    // reverted.
    case Token::MUL:
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND: {
      if (reversed_) {
        __ mov(r1, Operand(Smi::FromInt(value_)));
      } else {
        __ mov(r1, Operand(r0));
        __ mov(r0, Operand(Smi::FromInt(value_)));
      }
      break;
    }

    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      if (!reversed_) {
        __ mov(r1, Operand(r0));
        __ mov(r0, Operand(Smi::FromInt(value_)));
      } else {
        UNREACHABLE();  // Should have been handled in SmiOperation.
      }
      break;
    }

    default:
      // Other cases should have been handled before this point.
      UNREACHABLE();
      break;
  }

  GenericBinaryOpStub stub(op_, overwrite_mode_, value_);
  __ CallStub(&stub);
}


static bool PopCountLessThanEqual2(unsigned int x) {
  x &= x - 1;
  return (x & (x - 1)) == 0;
}


// Returns the index of the lowest bit set.
static int BitPosition(unsigned x) {
  int bit_posn = 0;
  while ((x & 0xf) == 0) {
    bit_posn += 4;
    x >>= 4;
  }
  while ((x & 1) == 0) {
    bit_posn++;
    x >>= 1;
  }
  return bit_posn;
}


void CodeGenerator::SmiOperation(Token::Value op,
                                 Handle<Object> value,
                                 bool reversed,
                                 OverwriteMode mode) {
  VirtualFrame::SpilledScope spilled_scope;
  // NOTE: This is an attempt to inline (a bit) more of the code for
  // some possible smi operations (like + and -) when (at least) one
  // of the operands is a literal smi. With this optimization, the
  // performance of the system is increased by ~15%, and the generated
  // code size is increased by ~1% (measured on a combination of
  // different benchmarks).

  // sp[0] : operand

  int int_value = Smi::cast(*value)->value();

  JumpTarget exit;
  frame_->EmitPop(r0);

  bool something_to_inline = true;
  switch (op) {
    case Token::ADD: {
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode);

      __ add(r0, r0, Operand(value), SetCC);
      deferred->Branch(vs);
      __ tst(r0, Operand(kSmiTagMask));
      deferred->Branch(ne);
      deferred->BindExit();
      break;
    }

    case Token::SUB: {
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode);

      if (reversed) {
        __ rsb(r0, r0, Operand(value), SetCC);
      } else {
        __ sub(r0, r0, Operand(value), SetCC);
      }
      deferred->Branch(vs);
      __ tst(r0, Operand(kSmiTagMask));
      deferred->Branch(ne);
      deferred->BindExit();
      break;
    }


    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND: {
      DeferredCode* deferred =
        new DeferredInlineSmiOperation(op, int_value, reversed, mode);
      __ tst(r0, Operand(kSmiTagMask));
      deferred->Branch(ne);
      switch (op) {
        case Token::BIT_OR:  __ orr(r0, r0, Operand(value)); break;
        case Token::BIT_XOR: __ eor(r0, r0, Operand(value)); break;
        case Token::BIT_AND: __ and_(r0, r0, Operand(value)); break;
        default: UNREACHABLE();
      }
      deferred->BindExit();
      break;
    }

    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      if (reversed) {
        something_to_inline = false;
        break;
      }
      int shift_value = int_value & 0x1f;  // least significant 5 bits
      DeferredCode* deferred =
        new DeferredInlineSmiOperation(op, shift_value, false, mode);
      __ tst(r0, Operand(kSmiTagMask));
      deferred->Branch(ne);
      __ mov(r2, Operand(r0, ASR, kSmiTagSize));  // remove tags
      switch (op) {
        case Token::SHL: {
          if (shift_value != 0) {
            __ mov(r2, Operand(r2, LSL, shift_value));
          }
          // check that the *unsigned* result fits in a smi
          __ add(r3, r2, Operand(0x40000000), SetCC);
          deferred->Branch(mi);
          break;
        }
        case Token::SHR: {
          // LSR by immediate 0 means shifting 32 bits.
          if (shift_value != 0) {
            __ mov(r2, Operand(r2, LSR, shift_value));
          }
          // check that the *unsigned* result fits in a smi
          // neither of the two high-order bits can be set:
          // - 0x80000000: high bit would be lost when smi tagging
          // - 0x40000000: this number would convert to negative when
          // smi tagging these two cases can only happen with shifts
          // by 0 or 1 when handed a valid smi
          __ and_(r3, r2, Operand(0xc0000000), SetCC);
          deferred->Branch(ne);
          break;
        }
        case Token::SAR: {
          if (shift_value != 0) {
            // ASR by immediate 0 means shifting 32 bits.
            __ mov(r2, Operand(r2, ASR, shift_value));
          }
          break;
        }
        default: UNREACHABLE();
      }
      __ mov(r0, Operand(r2, LSL, kSmiTagSize));
      deferred->BindExit();
      break;
    }

    case Token::MOD: {
      if (reversed || int_value < 2 || !IsPowerOf2(int_value)) {
        something_to_inline = false;
        break;
      }
      DeferredCode* deferred =
        new DeferredInlineSmiOperation(op, int_value, reversed, mode);
      unsigned mask = (0x80000000u | kSmiTagMask);
      __ tst(r0, Operand(mask));
      deferred->Branch(ne);  // Go to deferred code on non-Smis and negative.
      mask = (int_value << kSmiTagSize) - 1;
      __ and_(r0, r0, Operand(mask));
      deferred->BindExit();
      break;
    }

    case Token::MUL: {
      if (!IsEasyToMultiplyBy(int_value)) {
        something_to_inline = false;
        break;
      }
      DeferredCode* deferred =
        new DeferredInlineSmiOperation(op, int_value, reversed, mode);
      unsigned max_smi_that_wont_overflow = Smi::kMaxValue / int_value;
      max_smi_that_wont_overflow <<= kSmiTagSize;
      unsigned mask = 0x80000000u;
      while ((mask & max_smi_that_wont_overflow) == 0) {
        mask |= mask >> 1;
      }
      mask |= kSmiTagMask;
      // This does a single mask that checks for a too high value in a
      // conservative way and for a non-Smi.  It also filters out negative
      // numbers, unfortunately, but since this code is inline we prefer
      // brevity to comprehensiveness.
      __ tst(r0, Operand(mask));
      deferred->Branch(ne);
      MultiplyByKnownInt(masm_, r0, r0, int_value);
      deferred->BindExit();
      break;
    }

    default:
      something_to_inline = false;
      break;
  }

  if (!something_to_inline) {
    if (!reversed) {
      frame_->EmitPush(r0);
      __ mov(r0, Operand(value));
      frame_->EmitPush(r0);
      GenericBinaryOperation(op, mode, int_value);
    } else {
      __ mov(ip, Operand(value));
      frame_->EmitPush(ip);
      frame_->EmitPush(r0);
      GenericBinaryOperation(op, mode, kUnknownIntValue);
    }
  }

  exit.Bind();
}


void CodeGenerator::Comparison(Condition cc,
                               Expression* left,
                               Expression* right,
                               bool strict) {
  if (left != NULL) LoadAndSpill(left);
  if (right != NULL) LoadAndSpill(right);

  VirtualFrame::SpilledScope spilled_scope;
  // sp[0] : y
  // sp[1] : x
  // result : cc register

  // Strict only makes sense for equality comparisons.
  ASSERT(!strict || cc == eq);

  JumpTarget exit;
  JumpTarget smi;
  // Implement '>' and '<=' by reversal to obtain ECMA-262 conversion order.
  if (cc == gt || cc == le) {
    cc = ReverseCondition(cc);
    frame_->EmitPop(r1);
    frame_->EmitPop(r0);
  } else {
    frame_->EmitPop(r0);
    frame_->EmitPop(r1);
  }
  __ orr(r2, r0, Operand(r1));
  __ tst(r2, Operand(kSmiTagMask));
  smi.Branch(eq);

  // Perform non-smi comparison by stub.
  // CompareStub takes arguments in r0 and r1, returns <0, >0 or 0 in r0.
  // We call with 0 args because there are 0 on the stack.
  CompareStub stub(cc, strict);
  frame_->CallStub(&stub, 0);
  __ cmp(r0, Operand(0));
  exit.Jump();

  // Do smi comparisons by pointer comparison.
  smi.Bind();
  __ cmp(r1, Operand(r0));

  exit.Bind();
  cc_reg_ = cc;
}


class CallFunctionStub: public CodeStub {
 public:
  CallFunctionStub(int argc, InLoopFlag in_loop)
      : argc_(argc), in_loop_(in_loop) {}

  void Generate(MacroAssembler* masm);

 private:
  int argc_;
  InLoopFlag in_loop_;

#if defined(DEBUG)
  void Print() { PrintF("CallFunctionStub (argc %d)\n", argc_); }
#endif  // defined(DEBUG)

  Major MajorKey() { return CallFunction; }
  int MinorKey() { return argc_; }
  InLoopFlag InLoop() { return in_loop_; }
};


// Call the function on the stack with the given arguments.
void CodeGenerator::CallWithArguments(ZoneList<Expression*>* args,
                                         int position) {
  VirtualFrame::SpilledScope spilled_scope;
  // Push the arguments ("left-to-right") on the stack.
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    LoadAndSpill(args->at(i));
  }

  // Record the position for debugging purposes.
  CodeForSourcePosition(position);

  // Use the shared code stub to call the function.
  InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub call_function(arg_count, in_loop);
  frame_->CallStub(&call_function, arg_count + 1);

  // Restore context and pop function from the stack.
  __ ldr(cp, frame_->Context());
  frame_->Drop();  // discard the TOS
}


void CodeGenerator::Branch(bool if_true, JumpTarget* target) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(has_cc());
  Condition cc = if_true ? cc_reg_ : NegateCondition(cc_reg_);
  target->Branch(cc);
  cc_reg_ = al;
}


void CodeGenerator::CheckStack() {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ check stack");
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  // Put the lr setup instruction in the delay slot.  kInstrSize is added to
  // the implicit 8 byte offset that always applies to operations with pc and
  // gives a return address 12 bytes down.
  masm_->add(lr, pc, Operand(Assembler::kInstrSize));
  masm_->cmp(sp, Operand(ip));
  StackCheckStub stub;
  // Call the stub if lower.
  masm_->mov(pc,
             Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
                     RelocInfo::CODE_TARGET),
             LeaveCC,
             lo);
}


void CodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  for (int i = 0; frame_ != NULL && i < statements->length(); i++) {
    VisitAndSpill(statements->at(i));
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitBlock(Block* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Block");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  VisitStatementsAndSpill(node->statements());
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  VirtualFrame::SpilledScope spilled_scope;
  frame_->EmitPush(cp);
  __ mov(r0, Operand(pairs));
  frame_->EmitPush(r0);
  __ mov(r0, Operand(Smi::FromInt(is_eval() ? 1 : 0)));
  frame_->EmitPush(r0);
  frame_->CallRuntime(Runtime::kDeclareGlobals, 3);
  // The result is discarded.
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Declaration");
  Variable* var = node->proxy()->var();
  ASSERT(var != NULL);  // must have been resolved
  Slot* slot = var->slot();

  // If it was not possible to allocate the variable at compile time,
  // we need to "declare" it at runtime to make sure it actually
  // exists in the local context.
  if (slot != NULL && slot->type() == Slot::LOOKUP) {
    // Variables with a "LOOKUP" slot were introduced as non-locals
    // during variable resolution and must have mode DYNAMIC.
    ASSERT(var->is_dynamic());
    // For now, just do a runtime call.
    frame_->EmitPush(cp);
    __ mov(r0, Operand(var->name()));
    frame_->EmitPush(r0);
    // Declaration nodes are always declared in only two modes.
    ASSERT(node->mode() == Variable::VAR || node->mode() == Variable::CONST);
    PropertyAttributes attr = node->mode() == Variable::VAR ? NONE : READ_ONLY;
    __ mov(r0, Operand(Smi::FromInt(attr)));
    frame_->EmitPush(r0);
    // Push initial value, if any.
    // Note: For variables we must not push an initial value (such as
    // 'undefined') because we may have a (legal) redeclaration and we
    // must not destroy the current value.
    if (node->mode() == Variable::CONST) {
      __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
      frame_->EmitPush(r0);
    } else if (node->fun() != NULL) {
      LoadAndSpill(node->fun());
    } else {
      __ mov(r0, Operand(0));  // no initial value!
      frame_->EmitPush(r0);
    }
    frame_->CallRuntime(Runtime::kDeclareContextSlot, 4);
    // Ignore the return value (declarations are statements).
    ASSERT(frame_->height() == original_height);
    return;
  }

  ASSERT(!var->is_global());

  // If we have a function or a constant, we need to initialize the variable.
  Expression* val = NULL;
  if (node->mode() == Variable::CONST) {
    val = new Literal(Factory::the_hole_value());
  } else {
    val = node->fun();  // NULL if we don't have a function
  }

  if (val != NULL) {
    {
      // Set initial value.
      Reference target(this, node->proxy());
      LoadAndSpill(val);
      target.SetValue(NOT_CONST_INIT);
      // The reference is removed from the stack (preserving TOS) when
      // it goes out of scope.
    }
    // Get rid of the assigned value (declarations are statements).
    frame_->Drop();
  }
  ASSERT(frame_->height() == original_height);
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
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "// EmptyStatement");
  CodeForStatementPosition(node);
  // nothing to do
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitIfStatement(IfStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ IfStatement");
  // Generate different code depending on which parts of the if statement
  // are present or not.
  bool has_then_stm = node->HasThenStatement();
  bool has_else_stm = node->HasElseStatement();

  CodeForStatementPosition(node);

  JumpTarget exit;
  if (has_then_stm && has_else_stm) {
    Comment cmnt(masm_, "[ IfThenElse");
    JumpTarget then;
    JumpTarget else_;
    // if (cond)
    LoadConditionAndSpill(node->condition(), NOT_INSIDE_TYPEOF,
                          &then, &else_, true);
    if (frame_ != NULL) {
      Branch(false, &else_);
    }
    // then
    if (frame_ != NULL || then.is_linked()) {
      then.Bind();
      VisitAndSpill(node->then_statement());
    }
    if (frame_ != NULL) {
      exit.Jump();
    }
    // else
    if (else_.is_linked()) {
      else_.Bind();
      VisitAndSpill(node->else_statement());
    }

  } else if (has_then_stm) {
    Comment cmnt(masm_, "[ IfThen");
    ASSERT(!has_else_stm);
    JumpTarget then;
    // if (cond)
    LoadConditionAndSpill(node->condition(), NOT_INSIDE_TYPEOF,
                          &then, &exit, true);
    if (frame_ != NULL) {
      Branch(false, &exit);
    }
    // then
    if (frame_ != NULL || then.is_linked()) {
      then.Bind();
      VisitAndSpill(node->then_statement());
    }

  } else if (has_else_stm) {
    Comment cmnt(masm_, "[ IfElse");
    ASSERT(!has_then_stm);
    JumpTarget else_;
    // if (!cond)
    LoadConditionAndSpill(node->condition(), NOT_INSIDE_TYPEOF,
                          &exit, &else_, true);
    if (frame_ != NULL) {
      Branch(true, &exit);
    }
    // else
    if (frame_ != NULL || else_.is_linked()) {
      else_.Bind();
      VisitAndSpill(node->else_statement());
    }

  } else {
    Comment cmnt(masm_, "[ If");
    ASSERT(!has_then_stm && !has_else_stm);
    // if (cond)
    LoadConditionAndSpill(node->condition(), NOT_INSIDE_TYPEOF,
                          &exit, &exit, false);
    if (frame_ != NULL) {
      if (has_cc()) {
        cc_reg_ = al;
      } else {
        frame_->Drop();
      }
    }
  }

  // end
  if (exit.is_linked()) {
    exit.Bind();
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitContinueStatement(ContinueStatement* node) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ContinueStatement");
  CodeForStatementPosition(node);
  node->target()->continue_target()->Jump();
}


void CodeGenerator::VisitBreakStatement(BreakStatement* node) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ BreakStatement");
  CodeForStatementPosition(node);
  node->target()->break_target()->Jump();
}


void CodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ReturnStatement");

  CodeForStatementPosition(node);
  LoadAndSpill(node->expression());
  if (function_return_is_shadowed_) {
    frame_->EmitPop(r0);
    function_return_.Jump();
  } else {
    // Pop the result from the frame and prepare the frame for
    // returning thus making it easier to merge.
    frame_->EmitPop(r0);
    frame_->PrepareForReturn();

    function_return_.Jump();
  }
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ WithEnterStatement");
  CodeForStatementPosition(node);
  LoadAndSpill(node->expression());
  if (node->is_catch_block()) {
    frame_->CallRuntime(Runtime::kPushCatchContext, 1);
  } else {
    frame_->CallRuntime(Runtime::kPushContext, 1);
  }
#ifdef DEBUG
  JumpTarget verified_true;
  __ cmp(r0, Operand(cp));
  verified_true.Branch(eq);
  __ stop("PushContext: r0 is expected to be the same as cp");
  verified_true.Bind();
#endif
  // Update context local.
  __ str(cp, frame_->Context());
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitWithExitStatement(WithExitStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ WithExitStatement");
  CodeForStatementPosition(node);
  // Pop context.
  __ ldr(cp, ContextOperand(cp, Context::PREVIOUS_INDEX));
  // Update context local.
  __ str(cp, frame_->Context());
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ SwitchStatement");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  LoadAndSpill(node->tag());

  JumpTarget next_test;
  JumpTarget fall_through;
  JumpTarget default_entry;
  JumpTarget default_exit(JumpTarget::BIDIRECTIONAL);
  ZoneList<CaseClause*>* cases = node->cases();
  int length = cases->length();
  CaseClause* default_clause = NULL;

  for (int i = 0; i < length; i++) {
    CaseClause* clause = cases->at(i);
    if (clause->is_default()) {
      // Remember the default clause and compile it at the end.
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case clause");
    // Compile the test.
    next_test.Bind();
    next_test.Unuse();
    // Duplicate TOS.
    __ ldr(r0, frame_->Top());
    frame_->EmitPush(r0);
    Comparison(eq, NULL, clause->label(), true);
    Branch(false, &next_test);

    // Before entering the body from the test, remove the switch value from
    // the stack.
    frame_->Drop();

    // Label the body so that fall through is enabled.
    if (i > 0 && cases->at(i - 1)->is_default()) {
      default_exit.Bind();
    } else {
      fall_through.Bind();
      fall_through.Unuse();
    }
    VisitStatementsAndSpill(clause->statements());

    // If control flow can fall through from the body, jump to the next body
    // or the end of the statement.
    if (frame_ != NULL) {
      if (i < length - 1 && cases->at(i + 1)->is_default()) {
        default_entry.Jump();
      } else {
        fall_through.Jump();
      }
    }
  }

  // The final "test" removes the switch value.
  next_test.Bind();
  frame_->Drop();

  // If there is a default clause, compile it.
  if (default_clause != NULL) {
    Comment cmnt(masm_, "[ Default clause");
    default_entry.Bind();
    VisitStatementsAndSpill(default_clause->statements());
    // If control flow can fall out of the default and there is a case after
    // it, jup to that case's body.
    if (frame_ != NULL && default_exit.is_bound()) {
      default_exit.Jump();
    }
  }

  if (fall_through.is_linked()) {
    fall_through.Bind();
  }

  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitDoWhileStatement(DoWhileStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ DoWhileStatement");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  JumpTarget body(JumpTarget::BIDIRECTIONAL);

  // Label the top of the loop for the backward CFG edge.  If the test
  // is always true we can use the continue target, and if the test is
  // always false there is no need.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  switch (info) {
    case ALWAYS_TRUE:
      node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
      node->continue_target()->Bind();
      break;
    case ALWAYS_FALSE:
      node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      break;
    case DONT_KNOW:
      node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      body.Bind();
      break;
  }

  CheckStack();  // TODO(1222600): ignore if body contains calls.
  VisitAndSpill(node->body());

      // Compile the test.
  switch (info) {
    case ALWAYS_TRUE:
      // If control can fall off the end of the body, jump back to the
      // top.
      if (has_valid_frame()) {
        node->continue_target()->Jump();
      }
      break;
    case ALWAYS_FALSE:
      // If we have a continue in the body, we only have to bind its
      // jump target.
      if (node->continue_target()->is_linked()) {
        node->continue_target()->Bind();
      }
      break;
    case DONT_KNOW:
      // We have to compile the test expression if it can be reached by
      // control flow falling out of the body or via continue.
      if (node->continue_target()->is_linked()) {
        node->continue_target()->Bind();
      }
      if (has_valid_frame()) {
        LoadConditionAndSpill(node->cond(), NOT_INSIDE_TYPEOF,
                              &body, node->break_target(), true);
        if (has_valid_frame()) {
          // A invalid frame here indicates that control did not
          // fall out of the test expression.
          Branch(true, &body);
        }
      }
      break;
  }

  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitWhileStatement(WhileStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ WhileStatement");
  CodeForStatementPosition(node);

  // If the test is never true and has no side effects there is no need
  // to compile the test or body.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  // Label the top of the loop with the continue target for the backward
  // CFG edge.
  node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
  node->continue_target()->Bind();

  if (info == DONT_KNOW) {
    JumpTarget body;
    LoadConditionAndSpill(node->cond(), NOT_INSIDE_TYPEOF,
                          &body, node->break_target(), true);
    if (has_valid_frame()) {
      // A NULL frame indicates that control did not fall out of the
      // test expression.
      Branch(false, node->break_target());
    }
    if (has_valid_frame() || body.is_linked()) {
      body.Bind();
    }
  }

  if (has_valid_frame()) {
    CheckStack();  // TODO(1222600): ignore if body contains calls.
    VisitAndSpill(node->body());

    // If control flow can fall out of the body, jump back to the top.
    if (has_valid_frame()) {
      node->continue_target()->Jump();
    }
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitForStatement(ForStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ForStatement");
  CodeForStatementPosition(node);
  if (node->init() != NULL) {
    VisitAndSpill(node->init());
  }

  // If the test is never true there is no need to compile the test or
  // body.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  // If there is no update statement, label the top of the loop with the
  // continue target, otherwise with the loop target.
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  if (node->next() == NULL) {
    node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
    node->continue_target()->Bind();
  } else {
    node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
    loop.Bind();
  }

  // If the test is always true, there is no need to compile it.
  if (info == DONT_KNOW) {
    JumpTarget body;
    LoadConditionAndSpill(node->cond(), NOT_INSIDE_TYPEOF,
                          &body, node->break_target(), true);
    if (has_valid_frame()) {
      Branch(false, node->break_target());
    }
    if (has_valid_frame() || body.is_linked()) {
      body.Bind();
    }
  }

  if (has_valid_frame()) {
    CheckStack();  // TODO(1222600): ignore if body contains calls.
    VisitAndSpill(node->body());

    if (node->next() == NULL) {
      // If there is no update statement and control flow can fall out
      // of the loop, jump directly to the continue label.
      if (has_valid_frame()) {
        node->continue_target()->Jump();
      }
    } else {
      // If there is an update statement and control flow can reach it
      // via falling out of the body of the loop or continuing, we
      // compile the update statement.
      if (node->continue_target()->is_linked()) {
        node->continue_target()->Bind();
      }
      if (has_valid_frame()) {
        // Record source position of the statement as this code which is
        // after the code for the body actually belongs to the loop
        // statement and not the body.
        CodeForStatementPosition(node);
        VisitAndSpill(node->next());
        loop.Jump();
      }
    }
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitForInStatement(ForInStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ForInStatement");
  CodeForStatementPosition(node);

  JumpTarget primitive;
  JumpTarget jsobject;
  JumpTarget fixed_array;
  JumpTarget entry(JumpTarget::BIDIRECTIONAL);
  JumpTarget end_del_check;
  JumpTarget exit;

  // Get the object to enumerate over (converted to JSObject).
  LoadAndSpill(node->enumerable());

  // Both SpiderMonkey and kjs ignore null and undefined in contrast
  // to the specification.  12.6.4 mandates a call to ToObject.
  frame_->EmitPop(r0);
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  exit.Branch(eq);
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r0, ip);
  exit.Branch(eq);

  // Stack layout in body:
  // [iteration counter (Smi)]
  // [length of array]
  // [FixedArray]
  // [Map or 0]
  // [Object]

  // Check if enumerable is already a JSObject
  __ tst(r0, Operand(kSmiTagMask));
  primitive.Branch(eq);
  __ CompareObjectType(r0, r1, r1, FIRST_JS_OBJECT_TYPE);
  jsobject.Branch(hs);

  primitive.Bind();
  frame_->EmitPush(r0);
  Result arg_count(r0);
  __ mov(r0, Operand(0));
  frame_->InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS, &arg_count, 1);

  jsobject.Bind();
  // Get the set of properties (as a FixedArray or Map).
  frame_->EmitPush(r0);  // duplicate the object being enumerated
  frame_->EmitPush(r0);
  frame_->CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a Map, we can do a fast modification check.
  // Otherwise, we got a FixedArray, and we have to do a slow check.
  __ mov(r2, Operand(r0));
  __ ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r1, ip);
  fixed_array.Branch(ne);

  // Get enum cache
  __ mov(r1, Operand(r0));
  __ ldr(r1, FieldMemOperand(r1, Map::kInstanceDescriptorsOffset));
  __ ldr(r1, FieldMemOperand(r1, DescriptorArray::kEnumerationIndexOffset));
  __ ldr(r2,
         FieldMemOperand(r1, DescriptorArray::kEnumCacheBridgeCacheOffset));

  frame_->EmitPush(r0);  // map
  frame_->EmitPush(r2);  // enum cache bridge cache
  __ ldr(r0, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  frame_->EmitPush(r0);
  __ mov(r0, Operand(Smi::FromInt(0)));
  frame_->EmitPush(r0);
  entry.Jump();

  fixed_array.Bind();
  __ mov(r1, Operand(Smi::FromInt(0)));
  frame_->EmitPush(r1);  // insert 0 in place of Map
  frame_->EmitPush(r0);

  // Push the length of the array and the initial index onto the stack.
  __ ldr(r0, FieldMemOperand(r0, FixedArray::kLengthOffset));
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  frame_->EmitPush(r0);
  __ mov(r0, Operand(Smi::FromInt(0)));  // init index
  frame_->EmitPush(r0);

  // Condition.
  entry.Bind();
  // sp[0] : index
  // sp[1] : array/enum cache length
  // sp[2] : array or enum cache
  // sp[3] : 0 or map
  // sp[4] : enumerable
  // Grab the current frame's height for the break and continue
  // targets only after all the state is pushed on the frame.
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);

  __ ldr(r0, frame_->ElementAt(0));  // load the current count
  __ ldr(r1, frame_->ElementAt(1));  // load the length
  __ cmp(r0, Operand(r1));  // compare to the array length
  node->break_target()->Branch(hs);

  __ ldr(r0, frame_->ElementAt(0));

  // Get the i'th entry of the array.
  __ ldr(r2, frame_->ElementAt(2));
  __ add(r2, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r3, MemOperand(r2, r0, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Get Map or 0.
  __ ldr(r2, frame_->ElementAt(3));
  // Check if this (still) matches the map of the enumerable.
  // If not, we have to filter the key.
  __ ldr(r1, frame_->ElementAt(4));
  __ ldr(r1, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ cmp(r1, Operand(r2));
  end_del_check.Branch(eq);

  // Convert the entry to a string (or null if it isn't a property anymore).
  __ ldr(r0, frame_->ElementAt(4));  // push enumerable
  frame_->EmitPush(r0);
  frame_->EmitPush(r3);  // push entry
  Result arg_count_reg(r0);
  __ mov(r0, Operand(1));
  frame_->InvokeBuiltin(Builtins::FILTER_KEY, CALL_JS, &arg_count_reg, 2);
  __ mov(r3, Operand(r0));

  // If the property has been removed while iterating, we just skip it.
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r3, ip);
  node->continue_target()->Branch(eq);

  end_del_check.Bind();
  // Store the entry in the 'each' expression and take another spin in the
  // loop.  r3: i'th entry of the enum cache (or string there of)
  frame_->EmitPush(r3);  // push entry
  { Reference each(this, node->each());
    if (!each.is_illegal()) {
      if (each.size() > 0) {
        __ ldr(r0, frame_->ElementAt(each.size()));
        frame_->EmitPush(r0);
      }
      // If the reference was to a slot we rely on the convenient property
      // that it doesn't matter whether a value (eg, r3 pushed above) is
      // right on top of or right underneath a zero-sized reference.
      each.SetValue(NOT_CONST_INIT);
      if (each.size() > 0) {
        // It's safe to pop the value lying on top of the reference before
        // unloading the reference itself (which preserves the top of stack,
        // ie, now the topmost value of the non-zero sized reference), since
        // we will discard the top of stack after unloading the reference
        // anyway.
        frame_->EmitPop(r0);
      }
    }
  }
  // Discard the i'th entry pushed above or else the remainder of the
  // reference, whichever is currently on top of the stack.
  frame_->Drop();

  // Body.
  CheckStack();  // TODO(1222600): ignore if body contains calls.
  VisitAndSpill(node->body());

  // Next.  Reestablish a spilled frame in case we are coming here via
  // a continue in the body.
  node->continue_target()->Bind();
  frame_->SpillAll();
  frame_->EmitPop(r0);
  __ add(r0, r0, Operand(Smi::FromInt(1)));
  frame_->EmitPush(r0);
  entry.Jump();

  // Cleanup.  No need to spill because VirtualFrame::Drop is safe for
  // any frame.
  node->break_target()->Bind();
  frame_->Drop(5);

  // Exit.
  exit.Bind();
  node->continue_target()->Unuse();
  node->break_target()->Unuse();
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitTryCatchStatement(TryCatchStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ TryCatchStatement");
  CodeForStatementPosition(node);

  JumpTarget try_block;
  JumpTarget exit;

  try_block.Call();
  // --- Catch block ---
  frame_->EmitPush(r0);

  // Store the caught exception in the catch variable.
  { Reference ref(this, node->catch_var());
    ASSERT(ref.is_slot());
    // Here we make use of the convenient property that it doesn't matter
    // whether a value is immediately on top of or underneath a zero-sized
    // reference.
    ref.SetValue(NOT_CONST_INIT);
  }

  // Remove the exception from the stack.
  frame_->Drop();

  VisitStatementsAndSpill(node->catch_block()->statements());
  if (frame_ != NULL) {
    exit.Jump();
  }


  // --- Try block ---
  try_block.Bind();

  frame_->PushTryHandler(TRY_CATCH_HANDLER);
  int handler_height = frame_->height();

  // Shadow the labels for all escapes from the try block, including
  // returns. During shadowing, the original label is hidden as the
  // LabelShadow and operations on the original actually affect the
  // shadowing label.
  //
  // We should probably try to unify the escaping labels and the return
  // label.
  int nof_escapes = node->escaping_targets()->length();
  List<ShadowTarget*> shadows(1 + nof_escapes);

  // Add the shadow target for the function return.
  static const int kReturnShadowIndex = 0;
  shadows.Add(new ShadowTarget(&function_return_));
  bool function_return_was_shadowed = function_return_is_shadowed_;
  function_return_is_shadowed_ = true;
  ASSERT(shadows[kReturnShadowIndex]->other_target() == &function_return_);

  // Add the remaining shadow targets.
  for (int i = 0; i < nof_escapes; i++) {
    shadows.Add(new ShadowTarget(node->escaping_targets()->at(i)));
  }

  // Generate code for the statements in the try block.
  VisitStatementsAndSpill(node->try_block()->statements());

  // Stop the introduced shadowing and count the number of required unlinks.
  // After shadowing stops, the original labels are unshadowed and the
  // LabelShadows represent the formerly shadowing labels.
  bool has_unlinks = false;
  for (int i = 0; i < shadows.length(); i++) {
    shadows[i]->StopShadowing();
    has_unlinks = has_unlinks || shadows[i]->is_linked();
  }
  function_return_is_shadowed_ = function_return_was_shadowed;

  // Get an external reference to the handler address.
  ExternalReference handler_address(Top::k_handler_address);

  // If we can fall off the end of the try block, unlink from try chain.
  if (has_valid_frame()) {
    // The next handler address is on top of the frame.  Unlink from
    // the handler list and drop the rest of this handler from the
    // frame.
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    frame_->EmitPop(r1);
    __ mov(r3, Operand(handler_address));
    __ str(r1, MemOperand(r3));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);
    if (has_unlinks) {
      exit.Jump();
    }
  }

  // Generate unlink code for the (formerly) shadowing labels that have been
  // jumped to.  Deallocate each shadow target.
  for (int i = 0; i < shadows.length(); i++) {
    if (shadows[i]->is_linked()) {
      // Unlink from try chain;
      shadows[i]->Bind();
      // Because we can be jumping here (to spilled code) from unspilled
      // code, we need to reestablish a spilled frame at this block.
      frame_->SpillAll();

      // Reload sp from the top handler, because some statements that we
      // break from (eg, for...in) may have left stuff on the stack.
      __ mov(r3, Operand(handler_address));
      __ ldr(sp, MemOperand(r3));
      frame_->Forget(frame_->height() - handler_height);

      ASSERT(StackHandlerConstants::kNextOffset == 0);
      frame_->EmitPop(r1);
      __ str(r1, MemOperand(r3));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

      if (!function_return_is_shadowed_ && i == kReturnShadowIndex) {
        frame_->PrepareForReturn();
      }
      shadows[i]->other_target()->Jump();
    }
  }

  exit.Bind();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ TryFinallyStatement");
  CodeForStatementPosition(node);

  // State: Used to keep track of reason for entering the finally
  // block. Should probably be extended to hold information for
  // break/continue from within the try block.
  enum { FALLING, THROWING, JUMPING };

  JumpTarget try_block;
  JumpTarget finally_block;

  try_block.Call();

  frame_->EmitPush(r0);  // save exception object on the stack
  // In case of thrown exceptions, this is where we continue.
  __ mov(r2, Operand(Smi::FromInt(THROWING)));
  finally_block.Jump();

  // --- Try block ---
  try_block.Bind();

  frame_->PushTryHandler(TRY_FINALLY_HANDLER);
  int handler_height = frame_->height();

  // Shadow the labels for all escapes from the try block, including
  // returns.  Shadowing hides the original label as the LabelShadow and
  // operations on the original actually affect the shadowing label.
  //
  // We should probably try to unify the escaping labels and the return
  // label.
  int nof_escapes = node->escaping_targets()->length();
  List<ShadowTarget*> shadows(1 + nof_escapes);

  // Add the shadow target for the function return.
  static const int kReturnShadowIndex = 0;
  shadows.Add(new ShadowTarget(&function_return_));
  bool function_return_was_shadowed = function_return_is_shadowed_;
  function_return_is_shadowed_ = true;
  ASSERT(shadows[kReturnShadowIndex]->other_target() == &function_return_);

  // Add the remaining shadow targets.
  for (int i = 0; i < nof_escapes; i++) {
    shadows.Add(new ShadowTarget(node->escaping_targets()->at(i)));
  }

  // Generate code for the statements in the try block.
  VisitStatementsAndSpill(node->try_block()->statements());

  // Stop the introduced shadowing and count the number of required unlinks.
  // After shadowing stops, the original labels are unshadowed and the
  // LabelShadows represent the formerly shadowing labels.
  int nof_unlinks = 0;
  for (int i = 0; i < shadows.length(); i++) {
    shadows[i]->StopShadowing();
    if (shadows[i]->is_linked()) nof_unlinks++;
  }
  function_return_is_shadowed_ = function_return_was_shadowed;

  // Get an external reference to the handler address.
  ExternalReference handler_address(Top::k_handler_address);

  // If we can fall off the end of the try block, unlink from the try
  // chain and set the state on the frame to FALLING.
  if (has_valid_frame()) {
    // The next handler address is on top of the frame.
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    frame_->EmitPop(r1);
    __ mov(r3, Operand(handler_address));
    __ str(r1, MemOperand(r3));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

    // Fake a top of stack value (unneeded when FALLING) and set the
    // state in r2, then jump around the unlink blocks if any.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    frame_->EmitPush(r0);
    __ mov(r2, Operand(Smi::FromInt(FALLING)));
    if (nof_unlinks > 0) {
      finally_block.Jump();
    }
  }

  // Generate code to unlink and set the state for the (formerly)
  // shadowing targets that have been jumped to.
  for (int i = 0; i < shadows.length(); i++) {
    if (shadows[i]->is_linked()) {
      // If we have come from the shadowed return, the return value is
      // in (a non-refcounted reference to) r0.  We must preserve it
      // until it is pushed.
      //
      // Because we can be jumping here (to spilled code) from
      // unspilled code, we need to reestablish a spilled frame at
      // this block.
      shadows[i]->Bind();
      frame_->SpillAll();

      // Reload sp from the top handler, because some statements that
      // we break from (eg, for...in) may have left stuff on the
      // stack.
      __ mov(r3, Operand(handler_address));
      __ ldr(sp, MemOperand(r3));
      frame_->Forget(frame_->height() - handler_height);

      // Unlink this handler and drop it from the frame.  The next
      // handler address is currently on top of the frame.
      ASSERT(StackHandlerConstants::kNextOffset == 0);
      frame_->EmitPop(r1);
      __ str(r1, MemOperand(r3));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

      if (i == kReturnShadowIndex) {
        // If this label shadowed the function return, materialize the
        // return value on the stack.
        frame_->EmitPush(r0);
      } else {
        // Fake TOS for targets that shadowed breaks and continues.
        __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
        frame_->EmitPush(r0);
      }
      __ mov(r2, Operand(Smi::FromInt(JUMPING + i)));
      if (--nof_unlinks > 0) {
        // If this is not the last unlink block, jump around the next.
        finally_block.Jump();
      }
    }
  }

  // --- Finally block ---
  finally_block.Bind();

  // Push the state on the stack.
  frame_->EmitPush(r2);

  // We keep two elements on the stack - the (possibly faked) result
  // and the state - while evaluating the finally block.
  //
  // Generate code for the statements in the finally block.
  VisitStatementsAndSpill(node->finally_block()->statements());

  if (has_valid_frame()) {
    // Restore state and return value or faked TOS.
    frame_->EmitPop(r2);
    frame_->EmitPop(r0);
  }

  // Generate code to jump to the right destination for all used
  // formerly shadowing targets.  Deallocate each shadow target.
  for (int i = 0; i < shadows.length(); i++) {
    if (has_valid_frame() && shadows[i]->is_bound()) {
      JumpTarget* original = shadows[i]->other_target();
      __ cmp(r2, Operand(Smi::FromInt(JUMPING + i)));
      if (!function_return_is_shadowed_ && i == kReturnShadowIndex) {
        JumpTarget skip;
        skip.Branch(ne);
        frame_->PrepareForReturn();
        original->Jump();
        skip.Bind();
      } else {
        original->Branch(eq);
      }
    }
  }

  if (has_valid_frame()) {
    // Check if we need to rethrow the exception.
    JumpTarget exit;
    __ cmp(r2, Operand(Smi::FromInt(THROWING)));
    exit.Branch(ne);

    // Rethrow exception.
    frame_->EmitPush(r0);
    frame_->CallRuntime(Runtime::kReThrow, 1);

    // Done.
    exit.Bind();
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitDebuggerStatement(DebuggerStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ DebuggerStatament");
  CodeForStatementPosition(node);
#ifdef ENABLE_DEBUGGER_SUPPORT
  frame_->CallRuntime(Runtime::kDebugBreak, 0);
#endif
  // Ignore the return value.
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::InstantiateBoilerplate(Handle<JSFunction> boilerplate) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(boilerplate->IsBoilerplate());

  // Create a new closure.
  frame_->EmitPush(cp);
  __ mov(r0, Operand(boilerplate));
  frame_->EmitPush(r0);
  frame_->CallRuntime(Runtime::kNewClosure, 2);
  frame_->EmitPush(r0);
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate = BuildBoilerplate(node);
  // Check for stack-overflow exception.
  if (HasStackOverflow()) {
    ASSERT(frame_->height() == original_height);
    return;
  }
  InstantiateBoilerplate(boilerplate);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ FunctionBoilerplateLiteral");
  InstantiateBoilerplate(node->boilerplate());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitConditional(Conditional* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Conditional");
  JumpTarget then;
  JumpTarget else_;
  LoadConditionAndSpill(node->condition(), NOT_INSIDE_TYPEOF,
                        &then, &else_, true);
  if (has_valid_frame()) {
    Branch(false, &else_);
  }
  if (has_valid_frame() || then.is_linked()) {
    then.Bind();
    LoadAndSpill(node->then_expression(), typeof_state());
  }
  if (else_.is_linked()) {
    JumpTarget exit;
    if (has_valid_frame()) exit.Jump();
    else_.Bind();
    LoadAndSpill(node->else_expression(), typeof_state());
    if (exit.is_linked()) exit.Bind();
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::LoadFromSlot(Slot* slot, TypeofState typeof_state) {
  VirtualFrame::SpilledScope spilled_scope;
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    JumpTarget slow;
    JumpTarget done;

    // Generate fast-case code for variables that might be shadowed by
    // eval-introduced variables.  Eval is used a lot without
    // introducing variables.  In those cases, we do not want to
    // perform a runtime call for all variables in the scope
    // containing the eval.
    if (slot->var()->mode() == Variable::DYNAMIC_GLOBAL) {
      LoadFromGlobalSlotCheckExtensions(slot, typeof_state, r1, r2, &slow);
      // If there was no control flow to slow, we can exit early.
      if (!slow.is_linked()) {
        frame_->EmitPush(r0);
        return;
      }

      done.Jump();

    } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
      Slot* potential_slot = slot->var()->local_if_not_shadowed()->slot();
      // Only generate the fast case for locals that rewrite to slots.
      // This rules out argument loads.
      if (potential_slot != NULL) {
        __ ldr(r0,
               ContextSlotOperandCheckExtensions(potential_slot,
                                                 r1,
                                                 r2,
                                                 &slow));
        if (potential_slot->var()->mode() == Variable::CONST) {
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ cmp(r0, ip);
          __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
        }
        // There is always control flow to slow from
        // ContextSlotOperandCheckExtensions so we have to jump around
        // it.
        done.Jump();
      }
    }

    slow.Bind();
    frame_->EmitPush(cp);
    __ mov(r0, Operand(slot->var()->name()));
    frame_->EmitPush(r0);

    if (typeof_state == INSIDE_TYPEOF) {
      frame_->CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    } else {
      frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    }

    done.Bind();
    frame_->EmitPush(r0);

  } else {
    // Note: We would like to keep the assert below, but it fires because of
    // some nasty code in LoadTypeofExpression() which should be removed...
    // ASSERT(!slot->var()->is_dynamic());

    // Special handling for locals allocated in registers.
    __ ldr(r0, SlotOperand(slot, r2));
    frame_->EmitPush(r0);
    if (slot->var()->mode() == Variable::CONST) {
      // Const slots may contain 'the hole' value (the constant hasn't been
      // initialized yet) which needs to be converted into the 'undefined'
      // value.
      Comment cmnt(masm_, "[ Unhole const");
      frame_->EmitPop(r0);
      __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
      __ cmp(r0, ip);
      __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
      frame_->EmitPush(r0);
    }
  }
}


void CodeGenerator::LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                                      TypeofState typeof_state,
                                                      Register tmp,
                                                      Register tmp2,
                                                      JumpTarget* slow) {
  // Check that no extension objects have been created by calls to
  // eval from the current scope to the global scope.
  Register context = cp;
  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ ldr(tmp2, ContextOperand(context, Context::EXTENSION_INDEX));
        __ tst(tmp2, tmp2);
        slow->Branch(ne);
      }
      // Load next context in chain.
      __ ldr(tmp, ContextOperand(context, Context::CLOSURE_INDEX));
      __ ldr(tmp, FieldMemOperand(tmp, JSFunction::kContextOffset));
      context = tmp;
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s->is_eval_scope()) {
    Label next, fast;
    if (!context.is(tmp)) {
      __ mov(tmp, Operand(context));
    }
    __ bind(&next);
    // Terminate at global context.
    __ ldr(tmp2, FieldMemOperand(tmp, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kGlobalContextMapRootIndex);
    __ cmp(tmp2, ip);
    __ b(eq, &fast);
    // Check that extension is NULL.
    __ ldr(tmp2, ContextOperand(tmp, Context::EXTENSION_INDEX));
    __ tst(tmp2, tmp2);
    slow->Branch(ne);
    // Load next context in chain.
    __ ldr(tmp, ContextOperand(tmp, Context::CLOSURE_INDEX));
    __ ldr(tmp, FieldMemOperand(tmp, JSFunction::kContextOffset));
    __ b(&next);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  // Load the global object.
  LoadGlobal();
  // Setup the name register.
  Result name(r2);
  __ mov(r2, Operand(slot->var()->name()));
  // Call IC stub.
  if (typeof_state == INSIDE_TYPEOF) {
    frame_->CallCodeObject(ic, RelocInfo::CODE_TARGET, &name, 0);
  } else {
    frame_->CallCodeObject(ic, RelocInfo::CODE_TARGET_CONTEXT, &name, 0);
  }

  // Drop the global object. The result is in r0.
  frame_->Drop();
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
    ref.GetValueAndSpill(typeof_state());
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitLiteral(Literal* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Literal");
  __ mov(r0, Operand(node->handle()));
  frame_->EmitPush(r0);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ RexExp Literal");

  // Retrieve the literal array and check the allocated entry.

  // Load the function of this activation.
  __ ldr(r1, frame_->Function());

  // Load the literals array of the function.
  __ ldr(r1, FieldMemOperand(r1, JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ ldr(r2, FieldMemOperand(r1, literal_offset));

  JumpTarget done;
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r2, ip);
  done.Branch(ne);

  // If the entry is undefined we call the runtime system to computed
  // the literal.
  frame_->EmitPush(r1);  // literal array  (0)
  __ mov(r0, Operand(Smi::FromInt(node->literal_index())));
  frame_->EmitPush(r0);  // literal index  (1)
  __ mov(r0, Operand(node->pattern()));  // RegExp pattern (2)
  frame_->EmitPush(r0);
  __ mov(r0, Operand(node->flags()));  // RegExp flags   (3)
  frame_->EmitPush(r0);
  frame_->CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ mov(r2, Operand(r0));

  done.Bind();
  // Push the literal.
  frame_->EmitPush(r2);
  ASSERT(frame_->height() == original_height + 1);
}


// This deferred code stub will be used for creating the boilerplate
// by calling Runtime_CreateObjectLiteralBoilerplate.
// Each created boilerplate is stored in the JSFunction and they are
// therefore context dependent.
class DeferredObjectLiteral: public DeferredCode {
 public:
  explicit DeferredObjectLiteral(ObjectLiteral* node) : node_(node) {
    set_comment("[ DeferredObjectLiteral");
  }

  virtual void Generate();

 private:
  ObjectLiteral* node_;
};


void DeferredObjectLiteral::Generate() {
  // Argument is passed in r1.

  // If the entry is undefined we call the runtime system to compute
  // the literal.
  // Literal array (0).
  __ push(r1);
  // Literal index (1).
  __ mov(r0, Operand(Smi::FromInt(node_->literal_index())));
  __ push(r0);
  // Constant properties (2).
  __ mov(r0, Operand(node_->constant_properties()));
  __ push(r0);
  __ CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  __ mov(r2, Operand(r0));
  // Result is returned in r2.
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ObjectLiteral");

  DeferredObjectLiteral* deferred = new DeferredObjectLiteral(node);

  // Retrieve the literal array and check the allocated entry.

  // Load the function of this activation.
  __ ldr(r1, frame_->Function());

  // Load the literals array of the function.
  __ ldr(r1, FieldMemOperand(r1, JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ ldr(r2, FieldMemOperand(r1, literal_offset));

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code.
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r2, Operand(ip));
  deferred->Branch(eq);
  deferred->BindExit();

  // Push the object literal boilerplate.
  frame_->EmitPush(r2);

  // Clone the boilerplate object.
  Runtime::FunctionId clone_function_id = Runtime::kCloneLiteralBoilerplate;
  if (node->depth() == 1) {
    clone_function_id = Runtime::kCloneShallowLiteralBoilerplate;
  }
  frame_->CallRuntime(clone_function_id, 1);
  frame_->EmitPush(r0);  // save the result
  // r0: cloned object literal

  for (int i = 0; i < node->properties()->length(); i++) {
    ObjectLiteral::Property* property = node->properties()->at(i);
    Literal* key = property->key();
    Expression* value = property->value();
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        break;
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        if (CompileTimeValue::IsCompileTimeValue(property->value())) break;
        // else fall through
      case ObjectLiteral::Property::COMPUTED:  // fall through
      case ObjectLiteral::Property::PROTOTYPE: {
        frame_->EmitPush(r0);  // dup the result
        LoadAndSpill(key);
        LoadAndSpill(value);
        frame_->CallRuntime(Runtime::kSetProperty, 3);
        // restore r0
        __ ldr(r0, frame_->Top());
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        frame_->EmitPush(r0);
        LoadAndSpill(key);
        __ mov(r0, Operand(Smi::FromInt(1)));
        frame_->EmitPush(r0);
        LoadAndSpill(value);
        frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        __ ldr(r0, frame_->Top());
        break;
      }
      case ObjectLiteral::Property::GETTER: {
        frame_->EmitPush(r0);
        LoadAndSpill(key);
        __ mov(r0, Operand(Smi::FromInt(0)));
        frame_->EmitPush(r0);
        LoadAndSpill(value);
        frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        __ ldr(r0, frame_->Top());
        break;
      }
    }
  }
  ASSERT(frame_->height() == original_height + 1);
}


// This deferred code stub will be used for creating the boilerplate
// by calling Runtime_CreateArrayLiteralBoilerplate.
// Each created boilerplate is stored in the JSFunction and they are
// therefore context dependent.
class DeferredArrayLiteral: public DeferredCode {
 public:
  explicit DeferredArrayLiteral(ArrayLiteral* node) : node_(node) {
    set_comment("[ DeferredArrayLiteral");
  }

  virtual void Generate();

 private:
  ArrayLiteral* node_;
};


void DeferredArrayLiteral::Generate() {
  // Argument is passed in r1.

  // If the entry is undefined we call the runtime system to computed
  // the literal.
  // Literal array (0).
  __ push(r1);
  // Literal index (1).
  __ mov(r0, Operand(Smi::FromInt(node_->literal_index())));
  __ push(r0);
  // Constant properties (2).
  __ mov(r0, Operand(node_->literals()));
  __ push(r0);
  __ CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);
  __ mov(r2, Operand(r0));
  // Result is returned in r2.
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ ArrayLiteral");

  DeferredArrayLiteral* deferred = new DeferredArrayLiteral(node);

  // Retrieve the literal array and check the allocated entry.

  // Load the function of this activation.
  __ ldr(r1, frame_->Function());

  // Load the literals array of the function.
  __ ldr(r1, FieldMemOperand(r1, JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ ldr(r2, FieldMemOperand(r1, literal_offset));

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code.
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r2, Operand(ip));
  deferred->Branch(eq);
  deferred->BindExit();

  // Push the object literal boilerplate.
  frame_->EmitPush(r2);

  // Clone the boilerplate object.
  Runtime::FunctionId clone_function_id = Runtime::kCloneLiteralBoilerplate;
  if (node->depth() == 1) {
    clone_function_id = Runtime::kCloneShallowLiteralBoilerplate;
  }
  frame_->CallRuntime(clone_function_id, 1);
  frame_->EmitPush(r0);  // save the result
  // r0: cloned object literal

  // Generate code to set the elements in the array that are not
  // literals.
  for (int i = 0; i < node->values()->length(); i++) {
    Expression* value = node->values()->at(i);

    // If value is a literal the property value is already set in the
    // boilerplate object.
    if (value->AsLiteral() != NULL) continue;
    // If value is a materialized literal the property value is already set
    // in the boilerplate object if it is simple.
    if (CompileTimeValue::IsCompileTimeValue(value)) continue;

    // The property must be set by generated code.
    LoadAndSpill(value);
    frame_->EmitPop(r0);

    // Fetch the object literal.
    __ ldr(r1, frame_->Top());
    // Get the elements array.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));

    // Write to the indexed properties array.
    int offset = i * kPointerSize + FixedArray::kHeaderSize;
    __ str(r0, FieldMemOperand(r1, offset));

    // Update the write barrier for the array address.
    __ mov(r3, Operand(offset));
    __ RecordWrite(r1, r3, r2);
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  // Call runtime routine to allocate the catch extension object and
  // assign the exception value to the catch variable.
  Comment cmnt(masm_, "[ CatchExtensionObject");
  LoadAndSpill(node->key());
  LoadAndSpill(node->value());
  frame_->CallRuntime(Runtime::kCreateCatchExtensionObject, 2);
  frame_->EmitPush(r0);
  ASSERT(frame_->height() == original_height + 1);
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
      __ mov(r0, Operand(Smi::FromInt(0)));
      frame_->EmitPush(r0);
      ASSERT(frame_->height() == original_height + 1);
      return;
    }

    if (node->op() == Token::ASSIGN ||
        node->op() == Token::INIT_VAR ||
        node->op() == Token::INIT_CONST) {
      LoadAndSpill(node->value());

    } else {
      // +=, *= and similar binary assignments.
      // Get the old value of the lhs.
      target.GetValueAndSpill(NOT_INSIDE_TYPEOF);
      Literal* literal = node->value()->AsLiteral();
      bool overwrite =
          (node->value()->AsBinaryOperation() != NULL &&
           node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
      if (literal != NULL && literal->handle()->IsSmi()) {
        SmiOperation(node->binary_op(),
                     literal->handle(),
                     false,
                     overwrite ? OVERWRITE_RIGHT : NO_OVERWRITE);
        frame_->EmitPush(r0);

      } else {
        LoadAndSpill(node->value());
        GenericBinaryOperation(node->binary_op(),
                               overwrite ? OVERWRITE_RIGHT : NO_OVERWRITE);
        frame_->EmitPush(r0);
      }
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
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Throw");

  LoadAndSpill(node->exception());
  CodeForSourcePosition(node->position());
  frame_->CallRuntime(Runtime::kThrow, 1);
  frame_->EmitPush(r0);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitProperty(Property* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ Property");

  { Reference property(this, node);
    property.GetValueAndSpill(typeof_state());
  }
  ASSERT(frame_->height() == original_height + 1);
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
    // ----------------------------------
    // JavaScript example: 'eval(arg)'  // eval is not known to be shadowed
    // ----------------------------------

    // In a call to eval, we first call %ResolvePossiblyDirectEval to
    // resolve the function we need to call and the receiver of the
    // call.  Then we call the resolved function using the given
    // arguments.
    // Prepare stack for call to resolved function.
    LoadAndSpill(function);
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    frame_->EmitPush(r2);  // Slot for receiver
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      LoadAndSpill(args->at(i));
    }

    // Prepare stack for call to ResolvePossiblyDirectEval.
    __ ldr(r1, MemOperand(sp, arg_count * kPointerSize + kPointerSize));
    frame_->EmitPush(r1);
    if (arg_count > 0) {
      __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));
      frame_->EmitPush(r1);
    } else {
      frame_->EmitPush(r2);
    }

    // Resolve the call.
    frame_->CallRuntime(Runtime::kResolvePossiblyDirectEval, 2);

    // Touch up stack with the right values for the function and the receiver.
    __ ldr(r1, FieldMemOperand(r0, FixedArray::kHeaderSize));
    __ str(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ ldr(r1, FieldMemOperand(r0, FixedArray::kHeaderSize + kPointerSize));
    __ str(r1, MemOperand(sp, arg_count * kPointerSize));

    // Call the function.
    CodeForSourcePosition(node->position());

    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub call_function(arg_count, in_loop);
    frame_->CallStub(&call_function, arg_count + 1);

    __ ldr(cp, frame_->Context());
    // Remove the function from the stack.
    frame_->Drop();
    frame_->EmitPush(r0);

  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is global
    // ----------------------------------

    // Push the name of the function and the receiver onto the stack.
    __ mov(r0, Operand(var->name()));
    frame_->EmitPush(r0);

    // Pass the global object as the receiver and let the IC stub
    // patch the stack to use the global proxy as 'this' in the
    // invoked function.
    LoadGlobal();

    // Load the arguments.
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      LoadAndSpill(args->at(i));
    }

    // Setup the receiver register and call the IC initialization code.
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
    CodeForSourcePosition(node->position());
    frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET_CONTEXT,
                           arg_count + 1);
    __ ldr(cp, frame_->Context());
    // Remove the function from the stack.
    frame_->Drop();
    frame_->EmitPush(r0);

  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // ----------------------------------
    // JavaScript example: 'with (obj) foo(1, 2, 3)'  // foo is in obj
    // ----------------------------------

    // Load the function
    frame_->EmitPush(cp);
    __ mov(r0, Operand(var->name()));
    frame_->EmitPush(r0);
    frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    // r0: slot value; r1: receiver

    // Load the receiver.
    frame_->EmitPush(r0);  // function
    frame_->EmitPush(r1);  // receiver

    // Call the function.
    CallWithArguments(args, node->position());
    frame_->EmitPush(r0);

  } else if (property != NULL) {
    // Check if the key is a literal string.
    Literal* literal = property->key()->AsLiteral();

    if (literal != NULL && literal->handle()->IsSymbol()) {
      // ------------------------------------------------------------------
      // JavaScript example: 'object.foo(1, 2, 3)' or 'map["key"](1, 2, 3)'
      // ------------------------------------------------------------------

      // Push the name of the function and the receiver onto the stack.
      __ mov(r0, Operand(literal->handle()));
      frame_->EmitPush(r0);
      LoadAndSpill(property->obj());

      // Load the arguments.
      int arg_count = args->length();
      for (int i = 0; i < arg_count; i++) {
        LoadAndSpill(args->at(i));
      }

      // Set the receiver register and call the IC initialization code.
      InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
      Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
      CodeForSourcePosition(node->position());
      frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET, arg_count + 1);
      __ ldr(cp, frame_->Context());

      // Remove the function from the stack.
      frame_->Drop();

      frame_->EmitPush(r0);  // push after get rid of function from the stack

    } else {
      // -------------------------------------------
      // JavaScript example: 'array[index](1, 2, 3)'
      // -------------------------------------------

      // Load the function to call from the property through a reference.
      Reference ref(this, property);
      ref.GetValueAndSpill(NOT_INSIDE_TYPEOF);  // receiver

      // Pass receiver to called function.
      if (property->is_synthetic()) {
        LoadGlobalReceiver(r0);
      } else {
        __ ldr(r0, frame_->ElementAt(ref.size()));
        frame_->EmitPush(r0);
      }

      // Call the function.
      CallWithArguments(args, node->position());
      frame_->EmitPush(r0);
    }

  } else {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is not global
    // ----------------------------------

    // Load the function.
    LoadAndSpill(function);

    // Pass the global proxy as the receiver.
    LoadGlobalReceiver(r0);

    // Call the function.
    CallWithArguments(args, node->position());
    frame_->EmitPush(r0);
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitCallNew(CallNew* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ CallNew");

  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments. This is different from ordinary calls, where the
  // actual function to call is resolved after the arguments have been
  // evaluated.

  // Compute function to call and use the global object as the
  // receiver. There is no need to use the global proxy here because
  // it will always be replaced with a newly allocated object.
  LoadAndSpill(node->expression());
  LoadGlobal();

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = node->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    LoadAndSpill(args->at(i));
  }

  // r0: the number of arguments.
  Result num_args(r0);
  __ mov(r0, Operand(arg_count));

  // Load the function into r1 as per calling convention.
  Result function(r1);
  __ ldr(r1, frame_->ElementAt(arg_count + 1));

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  CodeForSourcePosition(node->position());
  Handle<Code> ic(Builtins::builtin(Builtins::JSConstructCall));
  frame_->CallCodeObject(ic,
                         RelocInfo::CONSTRUCT_CALL,
                         &num_args,
                         &function,
                         arg_count + 1);

  // Discard old TOS value and push r0 on the stack (same as Pop(), push(r0)).
  __ str(r0, frame_->Top());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::GenerateClassOf(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);
  JumpTarget leave, null, function, non_function_constructor;

  // Load the object into r0.
  LoadAndSpill(args->at(0));
  frame_->EmitPop(r0);

  // If the object is a smi, we return null.
  __ tst(r0, Operand(kSmiTagMask));
  null.Branch(eq);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CompareObjectType(r0, r0, r1, FIRST_JS_OBJECT_TYPE);
  null.Branch(lt);

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ cmp(r1, Operand(JS_FUNCTION_TYPE));
  function.Branch(eq);

  // Check if the constructor in the map is a function.
  __ ldr(r0, FieldMemOperand(r0, Map::kConstructorOffset));
  __ CompareObjectType(r0, r1, r1, JS_FUNCTION_TYPE);
  non_function_constructor.Branch(ne);

  // The r0 register now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(r0, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r0, FieldMemOperand(r0, SharedFunctionInfo::kInstanceClassNameOffset));
  frame_->EmitPush(r0);
  leave.Jump();

  // Functions have class 'Function'.
  function.Bind();
  __ mov(r0, Operand(Factory::function_class_symbol()));
  frame_->EmitPush(r0);
  leave.Jump();

  // Objects with a non-function constructor have class 'Object'.
  non_function_constructor.Bind();
  __ mov(r0, Operand(Factory::Object_symbol()));
  frame_->EmitPush(r0);
  leave.Jump();

  // Non-JS objects have class null.
  null.Bind();
  __ LoadRoot(r0, Heap::kNullValueRootIndex);
  frame_->EmitPush(r0);

  // All done.
  leave.Bind();
}


void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);
  JumpTarget leave;
  LoadAndSpill(args->at(0));
  frame_->EmitPop(r0);  // r0 contains object.
  // if (object->IsSmi()) return the object.
  __ tst(r0, Operand(kSmiTagMask));
  leave.Branch(eq);
  // It is a heap object - get map. If (!object->IsJSValue()) return the object.
  __ CompareObjectType(r0, r1, r1, JS_VALUE_TYPE);
  leave.Branch(ne);
  // Load the value.
  __ ldr(r0, FieldMemOperand(r0, JSValue::kValueOffset));
  leave.Bind();
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateSetValueOf(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 2);
  JumpTarget leave;
  LoadAndSpill(args->at(0));  // Load the object.
  LoadAndSpill(args->at(1));  // Load the value.
  frame_->EmitPop(r0);  // r0 contains value
  frame_->EmitPop(r1);  // r1 contains object
  // if (object->IsSmi()) return object.
  __ tst(r1, Operand(kSmiTagMask));
  leave.Branch(eq);
  // It is a heap object - get map. If (!object->IsJSValue()) return the object.
  __ CompareObjectType(r1, r2, r2, JS_VALUE_TYPE);
  leave.Branch(ne);
  // Store the value.
  __ str(r0, FieldMemOperand(r1, JSValue::kValueOffset));
  // Update the write barrier.
  __ mov(r2, Operand(JSValue::kValueOffset - kHeapObjectTag));
  __ RecordWrite(r1, r2, r3);
  // Leave.
  leave.Bind();
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);
  LoadAndSpill(args->at(0));
  frame_->EmitPop(r0);
  __ tst(r0, Operand(kSmiTagMask));
  cc_reg_ = eq;
}


void CodeGenerator::GenerateLog(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  // See comment in CodeGenerator::GenerateLog in codegen-ia32.cc.
  ASSERT_EQ(args->length(), 3);
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (ShouldGenerateLog(args->at(0))) {
    LoadAndSpill(args->at(1));
    LoadAndSpill(args->at(2));
    __ CallRuntime(Runtime::kLog, 2);
  }
#endif
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);
  LoadAndSpill(args->at(0));
  frame_->EmitPop(r0);
  __ tst(r0, Operand(kSmiTagMask | 0x80000000u));
  cc_reg_ = eq;
}


// This should generate code that performs a charCodeAt() call or returns
// undefined in order to trigger the slow case, Runtime_StringCharCodeAt.
// It is not yet implemented on ARM, so it always goes to the slow case.
void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 2);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);
  LoadAndSpill(args->at(0));
  JumpTarget answer;
  // We need the CC bits to come out as not_equal in the case where the
  // object is a smi.  This can't be done with the usual test opcode so
  // we use XOR to get the right CC bits.
  frame_->EmitPop(r0);
  __ and_(r1, r0, Operand(kSmiTagMask));
  __ eor(r1, r1, Operand(kSmiTagMask), SetCC);
  answer.Branch(ne);
  // It is a heap object - get the map. Check if the object is a JS array.
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  answer.Bind();
  cc_reg_ = eq;
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 0);

  // Get the frame pointer for the calling frame.
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &check_frame_marker);
  __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kMarkerOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
  cc_reg_ = eq;
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 0);

  // Seed the result with the formal parameters count, which will be used
  // in case no arguments adaptor frame is found below the current frame.
  __ mov(r0, Operand(Smi::FromInt(scope_->num_parameters())));

  // Call the shared stub to get to the arguments.length.
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_LENGTH);
  frame_->CallStub(&stub, 0);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateArgumentsAccess(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 1);

  // Satisfy contract with ArgumentsAccessStub:
  // Load the key into r1 and the formal parameters count into r0.
  LoadAndSpill(args->at(0));
  frame_->EmitPop(r1);
  __ mov(r0, Operand(Smi::FromInt(scope_->num_parameters())));

  // Call the shared stub to get to arguments[key].
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  frame_->CallStub(&stub, 0);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateRandomPositiveSmi(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 0);
  __ Call(ExternalReference::random_positive_smi_function().address(),
          RelocInfo::RUNTIME_ENTRY);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateFastMathOp(MathOp op, ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  LoadAndSpill(args->at(0));
  switch (op) {
    case SIN:
      frame_->CallRuntime(Runtime::kMath_sin, 1);
      break;
    case COS:
      frame_->CallRuntime(Runtime::kMath_cos, 1);
      break;
  }
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope;
  ASSERT(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  LoadAndSpill(args->at(0));
  LoadAndSpill(args->at(1));
  frame_->EmitPop(r0);
  frame_->EmitPop(r1);
  __ cmp(r0, Operand(r1));
  cc_reg_ = eq;
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  if (CheckForInlineRuntimeCall(node)) {
    ASSERT((has_cc() && frame_->height() == original_height) ||
           (!has_cc() && frame_->height() == original_height + 1));
    return;
  }

  ZoneList<Expression*>* args = node->arguments();
  Comment cmnt(masm_, "[ CallRuntime");
  Runtime::Function* function = node->function();

  if (function == NULL) {
    // Prepare stack for calling JS runtime function.
    __ mov(r0, Operand(node->name()));
    frame_->EmitPush(r0);
    // Push the builtins object found in the current global object.
    __ ldr(r1, GlobalObject());
    __ ldr(r0, FieldMemOperand(r1, GlobalObject::kBuiltinsOffset));
    frame_->EmitPush(r0);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    LoadAndSpill(args->at(i));
  }

  if (function == NULL) {
    // Call the JS runtime function.
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
    frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET, arg_count + 1);
    __ ldr(cp, frame_->Context());
    frame_->Drop();
    frame_->EmitPush(r0);
  } else {
    // Call the C runtime function.
    frame_->CallRuntime(function, arg_count);
    frame_->EmitPush(r0);
  }
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ UnaryOperation");

  Token::Value op = node->op();

  if (op == Token::NOT) {
    LoadConditionAndSpill(node->expression(),
                          NOT_INSIDE_TYPEOF,
                          false_target(),
                          true_target(),
                          true);
    // LoadCondition may (and usually does) leave a test and branch to
    // be emitted by the caller.  In that case, negate the condition.
    if (has_cc()) cc_reg_ = NegateCondition(cc_reg_);

  } else if (op == Token::DELETE) {
    Property* property = node->expression()->AsProperty();
    Variable* variable = node->expression()->AsVariableProxy()->AsVariable();
    if (property != NULL) {
      LoadAndSpill(property->obj());
      LoadAndSpill(property->key());
      Result arg_count(r0);
      __ mov(r0, Operand(1));  // not counting receiver
      frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, &arg_count, 2);

    } else if (variable != NULL) {
      Slot* slot = variable->slot();
      if (variable->is_global()) {
        LoadGlobal();
        __ mov(r0, Operand(variable->name()));
        frame_->EmitPush(r0);
        Result arg_count(r0);
        __ mov(r0, Operand(1));  // not counting receiver
        frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, &arg_count, 2);

      } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
        // lookup the context holding the named variable
        frame_->EmitPush(cp);
        __ mov(r0, Operand(variable->name()));
        frame_->EmitPush(r0);
        frame_->CallRuntime(Runtime::kLookupContext, 2);
        // r0: context
        frame_->EmitPush(r0);
        __ mov(r0, Operand(variable->name()));
        frame_->EmitPush(r0);
        Result arg_count(r0);
        __ mov(r0, Operand(1));  // not counting receiver
        frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, &arg_count, 2);

      } else {
        // Default: Result of deleting non-global, not dynamically
        // introduced variables is false.
        __ LoadRoot(r0, Heap::kFalseValueRootIndex);
      }

    } else {
      // Default: Result of deleting expressions is true.
      LoadAndSpill(node->expression());  // may have side-effects
      frame_->Drop();
      __ LoadRoot(r0, Heap::kTrueValueRootIndex);
    }
    frame_->EmitPush(r0);

  } else if (op == Token::TYPEOF) {
    // Special case for loading the typeof expression; see comment on
    // LoadTypeofExpression().
    LoadTypeofExpression(node->expression());
    frame_->CallRuntime(Runtime::kTypeof, 1);
    frame_->EmitPush(r0);  // r0 has result

  } else {
    LoadAndSpill(node->expression());
    frame_->EmitPop(r0);
    switch (op) {
      case Token::NOT:
      case Token::DELETE:
      case Token::TYPEOF:
        UNREACHABLE();  // handled above
        break;

      case Token::SUB: {
        bool overwrite =
            (node->expression()->AsBinaryOperation() != NULL &&
             node->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
        UnarySubStub stub(overwrite);
        frame_->CallStub(&stub, 0);
        break;
      }

      case Token::BIT_NOT: {
        // smi check
        JumpTarget smi_label;
        JumpTarget continue_label;
        __ tst(r0, Operand(kSmiTagMask));
        smi_label.Branch(eq);

        frame_->EmitPush(r0);
        Result arg_count(r0);
        __ mov(r0, Operand(0));  // not counting receiver
        frame_->InvokeBuiltin(Builtins::BIT_NOT, CALL_JS, &arg_count, 1);

        continue_label.Jump();
        smi_label.Bind();
        __ mvn(r0, Operand(r0));
        __ bic(r0, r0, Operand(kSmiTagMask));  // bit-clear inverted smi-tag
        continue_label.Bind();
        break;
      }

      case Token::VOID:
        // since the stack top is cached in r0, popping and then
        // pushing a value can be done by just writing to r0.
        __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
        break;

      case Token::ADD: {
        // Smi check.
        JumpTarget continue_label;
        __ tst(r0, Operand(kSmiTagMask));
        continue_label.Branch(eq);
        frame_->EmitPush(r0);
        Result arg_count(r0);
        __ mov(r0, Operand(0));  // not counting receiver
        frame_->InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS, &arg_count, 1);
        continue_label.Bind();
        break;
      }
      default:
        UNREACHABLE();
    }
    frame_->EmitPush(r0);  // r0 has result
  }
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ CountOperation");

  bool is_postfix = node->is_postfix();
  bool is_increment = node->op() == Token::INC;

  Variable* var = node->expression()->AsVariableProxy()->AsVariable();
  bool is_const = (var != NULL && var->mode() == Variable::CONST);

  // Postfix: Make room for the result.
  if (is_postfix) {
     __ mov(r0, Operand(0));
     frame_->EmitPush(r0);
  }

  { Reference target(this, node->expression());
    if (target.is_illegal()) {
      // Spoof the virtual frame to have the expected height (one higher
      // than on entry).
      if (!is_postfix) {
        __ mov(r0, Operand(Smi::FromInt(0)));
        frame_->EmitPush(r0);
      }
      ASSERT(frame_->height() == original_height + 1);
      return;
    }
    target.GetValueAndSpill(NOT_INSIDE_TYPEOF);
    frame_->EmitPop(r0);

    JumpTarget slow;
    JumpTarget exit;

    // Load the value (1) into register r1.
    __ mov(r1, Operand(Smi::FromInt(1)));

    // Check for smi operand.
    __ tst(r0, Operand(kSmiTagMask));
    slow.Branch(ne);

    // Postfix: Store the old value as the result.
    if (is_postfix) {
      __ str(r0, frame_->ElementAt(target.size()));
    }

    // Perform optimistic increment/decrement.
    if (is_increment) {
      __ add(r0, r0, Operand(r1), SetCC);
    } else {
      __ sub(r0, r0, Operand(r1), SetCC);
    }

    // If the increment/decrement didn't overflow, we're done.
    exit.Branch(vc);

    // Revert optimistic increment/decrement.
    if (is_increment) {
      __ sub(r0, r0, Operand(r1));
    } else {
      __ add(r0, r0, Operand(r1));
    }

    // Slow case: Convert to number.
    slow.Bind();
    {
      // Convert the operand to a number.
      frame_->EmitPush(r0);
      Result arg_count(r0);
      __ mov(r0, Operand(0));
      frame_->InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS, &arg_count, 1);
    }
    if (is_postfix) {
      // Postfix: store to result (on the stack).
      __ str(r0, frame_->ElementAt(target.size()));
    }

    // Compute the new value.
    __ mov(r1, Operand(Smi::FromInt(1)));
    frame_->EmitPush(r0);
    frame_->EmitPush(r1);
    if (is_increment) {
      frame_->CallRuntime(Runtime::kNumberAdd, 2);
    } else {
      frame_->CallRuntime(Runtime::kNumberSub, 2);
    }

    // Store the new value in the target if not const.
    exit.Bind();
    frame_->EmitPush(r0);
    if (!is_const) target.SetValue(NOT_CONST_INIT);
  }

  // Postfix: Discard the new value and use the old.
  if (is_postfix) frame_->EmitPop(r0);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ BinaryOperation");
  Token::Value op = node->op();

  // According to ECMA-262 section 11.11, page 58, the binary logical
  // operators must yield the result of one of the two expressions
  // before any ToBoolean() conversions. This means that the value
  // produced by a && or || operator is not necessarily a boolean.

  // NOTE: If the left hand side produces a materialized value (not in
  // the CC register), we force the right hand side to do the
  // same. This is necessary because we may have to branch to the exit
  // after evaluating the left hand side (due to the shortcut
  // semantics), but the compiler must (statically) know if the result
  // of compiling the binary operation is materialized or not.

  if (op == Token::AND) {
    JumpTarget is_true;
    LoadConditionAndSpill(node->left(),
                          NOT_INSIDE_TYPEOF,
                          &is_true,
                          false_target(),
                          false);
    if (has_valid_frame() && !has_cc()) {
      // The left-hand side result is on top of the virtual frame.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      __ ldr(r0, frame_->Top());  // Duplicate the stack top.
      frame_->EmitPush(r0);
      // Avoid popping the result if it converts to 'false' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      ToBoolean(&pop_and_continue, &exit);
      Branch(false, &exit);

      // Pop the result of evaluating the first part.
      pop_and_continue.Bind();
      frame_->EmitPop(r0);

      // Evaluate right side expression.
      is_true.Bind();
      LoadAndSpill(node->right());

      // Exit (always with a materialized value).
      exit.Bind();
    } else if (has_cc() || is_true.is_linked()) {
      // The left-hand side is either (a) partially compiled to
      // control flow with a final branch left to emit or (b) fully
      // compiled to control flow and possibly true.
      if (has_cc()) {
        Branch(false, false_target());
      }
      is_true.Bind();
      LoadConditionAndSpill(node->right(),
                            NOT_INSIDE_TYPEOF,
                            true_target(),
                            false_target(),
                            false);
    } else {
      // Nothing to do.
      ASSERT(!has_valid_frame() && !has_cc() && !is_true.is_linked());
    }

  } else if (op == Token::OR) {
    JumpTarget is_false;
    LoadConditionAndSpill(node->left(),
                          NOT_INSIDE_TYPEOF,
                          true_target(),
                          &is_false,
                          false);
    if (has_valid_frame() && !has_cc()) {
      // The left-hand side result is on top of the virtual frame.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      __ ldr(r0, frame_->Top());
      frame_->EmitPush(r0);
      // Avoid popping the result if it converts to 'true' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      ToBoolean(&exit, &pop_and_continue);
      Branch(true, &exit);

      // Pop the result of evaluating the first part.
      pop_and_continue.Bind();
      frame_->EmitPop(r0);

      // Evaluate right side expression.
      is_false.Bind();
      LoadAndSpill(node->right());

      // Exit (always with a materialized value).
      exit.Bind();
    } else if (has_cc() || is_false.is_linked()) {
      // The left-hand side is either (a) partially compiled to
      // control flow with a final branch left to emit or (b) fully
      // compiled to control flow and possibly false.
      if (has_cc()) {
        Branch(true, true_target());
      }
      is_false.Bind();
      LoadConditionAndSpill(node->right(),
                            NOT_INSIDE_TYPEOF,
                            true_target(),
                            false_target(),
                            false);
    } else {
      // Nothing to do.
      ASSERT(!has_valid_frame() && !has_cc() && !is_false.is_linked());
    }

  } else {
    // Optimize for the case where (at least) one of the expressions
    // is a literal small integer.
    Literal* lliteral = node->left()->AsLiteral();
    Literal* rliteral = node->right()->AsLiteral();
    // NOTE: The code below assumes that the slow cases (calls to runtime)
    // never return a constant/immutable object.
    bool overwrite_left =
        (node->left()->AsBinaryOperation() != NULL &&
         node->left()->AsBinaryOperation()->ResultOverwriteAllowed());
    bool overwrite_right =
        (node->right()->AsBinaryOperation() != NULL &&
         node->right()->AsBinaryOperation()->ResultOverwriteAllowed());

    if (rliteral != NULL && rliteral->handle()->IsSmi()) {
      LoadAndSpill(node->left());
      SmiOperation(node->op(),
                   rliteral->handle(),
                   false,
                   overwrite_right ? OVERWRITE_RIGHT : NO_OVERWRITE);

    } else if (lliteral != NULL && lliteral->handle()->IsSmi()) {
      LoadAndSpill(node->right());
      SmiOperation(node->op(),
                   lliteral->handle(),
                   true,
                   overwrite_left ? OVERWRITE_LEFT : NO_OVERWRITE);

    } else {
      OverwriteMode overwrite_mode = NO_OVERWRITE;
      if (overwrite_left) {
        overwrite_mode = OVERWRITE_LEFT;
      } else if (overwrite_right) {
        overwrite_mode = OVERWRITE_RIGHT;
      }
      LoadAndSpill(node->left());
      LoadAndSpill(node->right());
      GenericBinaryOperation(node->op(), overwrite_mode);
    }
    frame_->EmitPush(r0);
  }
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  __ ldr(r0, frame_->Function());
  frame_->EmitPush(r0);
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ CompareOperation");

  // Get the expressions from the node.
  Expression* left = node->left();
  Expression* right = node->right();
  Token::Value op = node->op();

  // To make null checks efficient, we check if either left or right is the
  // literal 'null'. If so, we optimize the code by inlining a null check
  // instead of calling the (very) general runtime routine for checking
  // equality.
  if (op == Token::EQ || op == Token::EQ_STRICT) {
    bool left_is_null =
        left->AsLiteral() != NULL && left->AsLiteral()->IsNull();
    bool right_is_null =
        right->AsLiteral() != NULL && right->AsLiteral()->IsNull();
    // The 'null' value can only be equal to 'null' or 'undefined'.
    if (left_is_null || right_is_null) {
      LoadAndSpill(left_is_null ? right : left);
      frame_->EmitPop(r0);
      __ LoadRoot(ip, Heap::kNullValueRootIndex);
      __ cmp(r0, ip);

      // The 'null' value is only equal to 'undefined' if using non-strict
      // comparisons.
      if (op != Token::EQ_STRICT) {
        true_target()->Branch(eq);

        __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
        __ cmp(r0, Operand(ip));
        true_target()->Branch(eq);

        __ tst(r0, Operand(kSmiTagMask));
        false_target()->Branch(eq);

        // It can be an undetectable object.
        __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
        __ ldrb(r0, FieldMemOperand(r0, Map::kBitFieldOffset));
        __ and_(r0, r0, Operand(1 << Map::kIsUndetectable));
        __ cmp(r0, Operand(1 << Map::kIsUndetectable));
      }

      cc_reg_ = eq;
      ASSERT(has_cc() && frame_->height() == original_height);
      return;
    }
  }

  // To make typeof testing for natives implemented in JavaScript really
  // efficient, we generate special code for expressions of the form:
  // 'typeof <expression> == <string>'.
  UnaryOperation* operation = left->AsUnaryOperation();
  if ((op == Token::EQ || op == Token::EQ_STRICT) &&
      (operation != NULL && operation->op() == Token::TYPEOF) &&
      (right->AsLiteral() != NULL &&
       right->AsLiteral()->handle()->IsString())) {
    Handle<String> check(String::cast(*right->AsLiteral()->handle()));

    // Load the operand, move it to register r1.
    LoadTypeofExpression(operation->expression());
    frame_->EmitPop(r1);

    if (check->Equals(Heap::number_symbol())) {
      __ tst(r1, Operand(kSmiTagMask));
      true_target()->Branch(eq);
      __ ldr(r1, FieldMemOperand(r1, HeapObject::kMapOffset));
      __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
      __ cmp(r1, ip);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::string_symbol())) {
      __ tst(r1, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      __ ldr(r1, FieldMemOperand(r1, HeapObject::kMapOffset));

      // It can be an undetectable string object.
      __ ldrb(r2, FieldMemOperand(r1, Map::kBitFieldOffset));
      __ and_(r2, r2, Operand(1 << Map::kIsUndetectable));
      __ cmp(r2, Operand(1 << Map::kIsUndetectable));
      false_target()->Branch(eq);

      __ ldrb(r2, FieldMemOperand(r1, Map::kInstanceTypeOffset));
      __ cmp(r2, Operand(FIRST_NONSTRING_TYPE));
      cc_reg_ = lt;

    } else if (check->Equals(Heap::boolean_symbol())) {
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r1, ip);
      true_target()->Branch(eq);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ cmp(r1, ip);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::undefined_symbol())) {
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(r1, ip);
      true_target()->Branch(eq);

      __ tst(r1, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      // It can be an undetectable object.
      __ ldr(r1, FieldMemOperand(r1, HeapObject::kMapOffset));
      __ ldrb(r2, FieldMemOperand(r1, Map::kBitFieldOffset));
      __ and_(r2, r2, Operand(1 << Map::kIsUndetectable));
      __ cmp(r2, Operand(1 << Map::kIsUndetectable));

      cc_reg_ = eq;

    } else if (check->Equals(Heap::function_symbol())) {
      __ tst(r1, Operand(kSmiTagMask));
      false_target()->Branch(eq);
      __ CompareObjectType(r1, r1, r1, JS_FUNCTION_TYPE);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::object_symbol())) {
      __ tst(r1, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
      __ LoadRoot(ip, Heap::kNullValueRootIndex);
      __ cmp(r1, ip);
      true_target()->Branch(eq);

      // It can be an undetectable object.
      __ ldrb(r1, FieldMemOperand(r2, Map::kBitFieldOffset));
      __ and_(r1, r1, Operand(1 << Map::kIsUndetectable));
      __ cmp(r1, Operand(1 << Map::kIsUndetectable));
      false_target()->Branch(eq);

      __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
      __ cmp(r2, Operand(FIRST_JS_OBJECT_TYPE));
      false_target()->Branch(lt);
      __ cmp(r2, Operand(LAST_JS_OBJECT_TYPE));
      cc_reg_ = le;

    } else {
      // Uncommon case: typeof testing against a string literal that is
      // never returned from the typeof operator.
      false_target()->Jump();
    }
    ASSERT(!has_valid_frame() ||
           (has_cc() && frame_->height() == original_height));
    return;
  }

  switch (op) {
    case Token::EQ:
      Comparison(eq, left, right, false);
      break;

    case Token::LT:
      Comparison(lt, left, right);
      break;

    case Token::GT:
      Comparison(gt, left, right);
      break;

    case Token::LTE:
      Comparison(le, left, right);
      break;

    case Token::GTE:
      Comparison(ge, left, right);
      break;

    case Token::EQ_STRICT:
      Comparison(eq, left, right, true);
      break;

    case Token::IN: {
      LoadAndSpill(left);
      LoadAndSpill(right);
      Result arg_count(r0);
      __ mov(r0, Operand(1));  // not counting receiver
      frame_->InvokeBuiltin(Builtins::IN, CALL_JS, &arg_count, 2);
      frame_->EmitPush(r0);
      break;
    }

    case Token::INSTANCEOF: {
      LoadAndSpill(left);
      LoadAndSpill(right);
      InstanceofStub stub;
      frame_->CallStub(&stub, 2);
      // At this point if instanceof succeeded then r0 == 0.
      __ tst(r0, Operand(r0));
      cc_reg_ = eq;
      break;
    }

    default:
      UNREACHABLE();
  }
  ASSERT((has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


#ifdef DEBUG
bool CodeGenerator::HasValidEntryRegisters() { return true; }
#endif


#undef __
#define __ ACCESS_MASM(masm)


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


void Reference::GetValue(TypeofState typeof_state) {
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_illegal());
  ASSERT(!cgen_->has_cc());
  MacroAssembler* masm = cgen_->masm();
  Property* property = expression_->AsProperty();
  if (property != NULL) {
    cgen_->CodeForSourcePosition(property->position());
  }

  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Load from Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      cgen_->LoadFromSlot(slot, typeof_state);
      break;
    }

    case NAMED: {
      // TODO(1241834): Make sure that this it is safe to ignore the
      // distinction between expressions in a typeof and not in a typeof. If
      // there is a chance that reference errors can be thrown below, we
      // must distinguish between the two kinds of loads (typeof expression
      // loads must not throw a reference error).
      VirtualFrame* frame = cgen_->frame();
      Comment cmnt(masm, "[ Load from named Property");
      Handle<String> name(GetName());
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
      // Setup the name register.
      Result name_reg(r2);
      __ mov(r2, Operand(name));
      ASSERT(var == NULL || var->is_global());
      RelocInfo::Mode rmode = (var == NULL)
                            ? RelocInfo::CODE_TARGET
                            : RelocInfo::CODE_TARGET_CONTEXT;
      frame->CallCodeObject(ic, rmode, &name_reg, 0);
      frame->EmitPush(r0);
      break;
    }

    case KEYED: {
      // TODO(1241834): Make sure that this it is safe to ignore the
      // distinction between expressions in a typeof and not in a typeof.

      // TODO(181): Implement inlined version of array indexing once
      // loop nesting is properly tracked on ARM.
      VirtualFrame* frame = cgen_->frame();
      Comment cmnt(masm, "[ Load from keyed Property");
      ASSERT(property != NULL);
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      ASSERT(var == NULL || var->is_global());
      RelocInfo::Mode rmode = (var == NULL)
                            ? RelocInfo::CODE_TARGET
                            : RelocInfo::CODE_TARGET_CONTEXT;
      frame->CallCodeObject(ic, rmode, 0);
      frame->EmitPush(r0);
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
  VirtualFrame* frame = cgen_->frame();
  Property* property = expression_->AsProperty();
  if (property != NULL) {
    cgen_->CodeForSourcePosition(property->position());
  }

  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Store to Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      if (slot->type() == Slot::LOOKUP) {
        ASSERT(slot->var()->is_dynamic());

        // For now, just do a runtime call.
        frame->EmitPush(cp);
        __ mov(r0, Operand(slot->var()->name()));
        frame->EmitPush(r0);

        if (init_state == CONST_INIT) {
          // Same as the case for a normal store, but ignores attribute
          // (e.g. READ_ONLY) of context slot so that we can initialize
          // const properties (introduced via eval("const foo = (some
          // expr);")). Also, uses the current function context instead of
          // the top context.
          //
          // Note that we must declare the foo upon entry of eval(), via a
          // context slot declaration, but we cannot initialize it at the
          // same time, because the const declaration may be at the end of
          // the eval code (sigh...) and the const variable may have been
          // used before (where its value is 'undefined'). Thus, we can only
          // do the initialization when we actually encounter the expression
          // and when the expression operands are defined and valid, and
          // thus we need the split into 2 operations: declaration of the
          // context slot followed by initialization.
          frame->CallRuntime(Runtime::kInitializeConstContextSlot, 3);
        } else {
          frame->CallRuntime(Runtime::kStoreContextSlot, 3);
        }
        // Storing a variable must keep the (new) value on the expression
        // stack. This is necessary for compiling assignment expressions.
        frame->EmitPush(r0);

      } else {
        ASSERT(!slot->var()->is_dynamic());

        JumpTarget exit;
        if (init_state == CONST_INIT) {
          ASSERT(slot->var()->mode() == Variable::CONST);
          // Only the first const initialization must be executed (the slot
          // still contains 'the hole' value). When the assignment is
          // executed, the code is identical to a normal store (see below).
          Comment cmnt(masm, "[ Init const");
          __ ldr(r2, cgen_->SlotOperand(slot, r2));
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ cmp(r2, ip);
          exit.Branch(ne);
        }

        // We must execute the store.  Storing a variable must keep the
        // (new) value on the stack. This is necessary for compiling
        // assignment expressions.
        //
        // Note: We will reach here even with slot->var()->mode() ==
        // Variable::CONST because of const declarations which will
        // initialize consts to 'the hole' value and by doing so, end up
        // calling this code.  r2 may be loaded with context; used below in
        // RecordWrite.
        frame->EmitPop(r0);
        __ str(r0, cgen_->SlotOperand(slot, r2));
        frame->EmitPush(r0);
        if (slot->type() == Slot::CONTEXT) {
          // Skip write barrier if the written value is a smi.
          __ tst(r0, Operand(kSmiTagMask));
          exit.Branch(eq);
          // r2 is loaded with context when calling SlotOperand above.
          int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
          __ mov(r3, Operand(offset));
          __ RecordWrite(r2, r3, r1);
        }
        // If we definitely did not jump over the assignment, we do not need
        // to bind the exit label.  Doing so can defeat peephole
        // optimization.
        if (init_state == CONST_INIT || slot->type() == Slot::CONTEXT) {
          exit.Bind();
        }
      }
      break;
    }

    case NAMED: {
      Comment cmnt(masm, "[ Store to named Property");
      // Call the appropriate IC code.
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      Handle<String> name(GetName());

      Result value(r0);
      frame->EmitPop(r0);

      // Setup the name register.
      Result property_name(r2);
      __ mov(r2, Operand(name));
      frame->CallCodeObject(ic,
                            RelocInfo::CODE_TARGET,
                            &value,
                            &property_name,
                            0);
      frame->EmitPush(r0);
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Store to keyed Property");
      Property* property = expression_->AsProperty();
      ASSERT(property != NULL);
      cgen_->CodeForSourcePosition(property->position());

      // Call IC code.
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      // TODO(1222589): Make the IC grab the values from the stack.
      Result value(r0);
      frame->EmitPop(r0);  // value
      frame->CallCodeObject(ic, RelocInfo::CODE_TARGET, &value, 0);
      frame->EmitPush(r0);
      break;
    }

    default:
      UNREACHABLE();
  }
}


// Count leading zeros in a 32 bit word.  On ARM5 and later it uses the clz
// instruction.  On pre-ARM5 hardware this routine gives the wrong answer for 0
// (31 instead of 32).
static void CountLeadingZeros(
    MacroAssembler* masm,
    Register source,
    Register scratch,
    Register zeros) {
#ifdef CAN_USE_ARMV5_INSTRUCTIONS
  __ clz(zeros, source);  // This instruction is only supported after ARM5.
#else
  __ mov(zeros, Operand(0));
  __ mov(scratch, source);
  // Top 16.
  __ tst(scratch, Operand(0xffff0000));
  __ add(zeros, zeros, Operand(16), LeaveCC, eq);
  __ mov(scratch, Operand(scratch, LSL, 16), LeaveCC, eq);
  // Top 8.
  __ tst(scratch, Operand(0xff000000));
  __ add(zeros, zeros, Operand(8), LeaveCC, eq);
  __ mov(scratch, Operand(scratch, LSL, 8), LeaveCC, eq);
  // Top 4.
  __ tst(scratch, Operand(0xf0000000));
  __ add(zeros, zeros, Operand(4), LeaveCC, eq);
  __ mov(scratch, Operand(scratch, LSL, 4), LeaveCC, eq);
  // Top 2.
  __ tst(scratch, Operand(0xc0000000));
  __ add(zeros, zeros, Operand(2), LeaveCC, eq);
  __ mov(scratch, Operand(scratch, LSL, 2), LeaveCC, eq);
  // Top bit.
  __ tst(scratch, Operand(0x80000000u));
  __ add(zeros, zeros, Operand(1), LeaveCC, eq);
#endif
}


// Takes a Smi and converts to an IEEE 64 bit floating point value in two
// registers.  The format is 1 sign bit, 11 exponent bits (biased 1023) and
// 52 fraction bits (20 in the first word, 32 in the second).  Zeros is a
// scratch register.  Destroys the source register.  No GC occurs during this
// stub so you don't have to set up the frame.
class ConvertToDoubleStub : public CodeStub {
 public:
  ConvertToDoubleStub(Register result_reg_1,
                      Register result_reg_2,
                      Register source_reg,
                      Register scratch_reg)
      : result1_(result_reg_1),
        result2_(result_reg_2),
        source_(source_reg),
        zeros_(scratch_reg) { }

 private:
  Register result1_;
  Register result2_;
  Register source_;
  Register zeros_;

  // Minor key encoding in 16 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 14> {};

  Major MajorKey() { return ConvertToDouble; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return  result1_.code() +
           (result2_.code() << 4) +
           (source_.code() << 8) +
           (zeros_.code() << 12);
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "ConvertToDoubleStub"; }

#ifdef DEBUG
  void Print() { PrintF("ConvertToDoubleStub\n"); }
#endif
};


void ConvertToDoubleStub::Generate(MacroAssembler* masm) {
#ifndef BIG_ENDIAN_FLOATING_POINT
  Register exponent = result1_;
  Register mantissa = result2_;
#else
  Register exponent = result2_;
  Register mantissa = result1_;
#endif
  Label not_special;
  // Convert from Smi to integer.
  __ mov(source_, Operand(source_, ASR, kSmiTagSize));
  // Move sign bit from source to destination.  This works because the sign bit
  // in the exponent word of the double has the same position and polarity as
  // the 2's complement sign bit in a Smi.
  ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ and_(exponent, source_, Operand(HeapNumber::kSignMask), SetCC);
  // Subtract from 0 if source was negative.
  __ rsb(source_, source_, Operand(0), LeaveCC, ne);
  __ cmp(source_, Operand(1));
  __ b(gt, &not_special);

  // We have -1, 0 or 1, which we treat specially.
  __ cmp(source_, Operand(0));
  // For 1 or -1 we need to or in the 0 exponent (biased to 1023).
  static const uint32_t exponent_word_for_1 =
      HeapNumber::kExponentBias << HeapNumber::kExponentShift;
  __ orr(exponent, exponent, Operand(exponent_word_for_1), LeaveCC, ne);
  // 1, 0 and -1 all have 0 for the second word.
  __ mov(mantissa, Operand(0));
  __ Ret();

  __ bind(&not_special);
  // Count leading zeros.  Uses result2 for a scratch register on pre-ARM5.
  // Gets the wrong answer for 0, but we already checked for that case above.
  CountLeadingZeros(masm, source_, mantissa, zeros_);
  // Compute exponent and or it into the exponent register.
  // We use result2 as a scratch register here.
  __ rsb(mantissa, zeros_, Operand(31 + HeapNumber::kExponentBias));
  __ orr(exponent,
         exponent,
         Operand(mantissa, LSL, HeapNumber::kExponentShift));
  // Shift up the source chopping the top bit off.
  __ add(zeros_, zeros_, Operand(1));
  // This wouldn't work for 1.0 or -1.0 as the shift would be 32 which means 0.
  __ mov(source_, Operand(source_, LSL, zeros_));
  // Compute lower part of fraction (last 12 bits).
  __ mov(mantissa, Operand(source_, LSL, HeapNumber::kMantissaBitsInTopWord));
  // And the top (top 20 bits).
  __ orr(exponent,
         exponent,
         Operand(source_, LSR, 32 - HeapNumber::kMantissaBitsInTopWord));
  __ Ret();
}


// This stub can convert a signed int32 to a heap number (double).  It does
// not work for int32s that are in Smi range!  No GC occurs during this stub
// so you don't have to set up the frame.
class WriteInt32ToHeapNumberStub : public CodeStub {
 public:
  WriteInt32ToHeapNumberStub(Register the_int,
                             Register the_heap_number,
                             Register scratch)
      : the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch) { }

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;

  // Minor key encoding in 16 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 14> {};

  Major MajorKey() { return WriteInt32ToHeapNumber; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return  the_int_.code() +
           (the_heap_number_.code() << 4) +
           (scratch_.code() << 8);
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "WriteInt32ToHeapNumberStub"; }

#ifdef DEBUG
  void Print() { PrintF("WriteInt32ToHeapNumberStub\n"); }
#endif
};


// See comment for class.
void WriteInt32ToHeapNumberStub::Generate(MacroAssembler *masm) {
  Label max_negative_int;
  // the_int_ has the answer which is a signed int32 but not a Smi.
  // We test for the special value that has a different exponent.  This test
  // has the neat side effect of setting the flags according to the sign.
  ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ cmp(the_int_, Operand(0x80000000u));
  __ b(eq, &max_negative_int);
  // Set up the correct exponent in scratch_.  All non-Smi int32s have the same.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).
  uint32_t non_smi_exponent =
      (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
  __ mov(scratch_, Operand(non_smi_exponent));
  // Set the sign bit in scratch_ if the value was negative.
  __ orr(scratch_, scratch_, Operand(HeapNumber::kSignMask), LeaveCC, cs);
  // Subtract from 0 if the value was negative.
  __ rsb(the_int_, the_int_, Operand(0), LeaveCC, cs);
  // We should be masking the implict first digit of the mantissa away here,
  // but it just ends up combining harmlessly with the last digit of the
  // exponent that happens to be 1.  The sign bit is 0 so we shift 10 to get
  // the most significant 1 to hit the last bit of the 12 bit sign and exponent.
  ASSERT(((1 << HeapNumber::kExponentShift) & non_smi_exponent) != 0);
  const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
  __ orr(scratch_, scratch_, Operand(the_int_, LSR, shift_distance));
  __ str(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kExponentOffset));
  __ mov(scratch_, Operand(the_int_, LSL, 32 - shift_distance));
  __ str(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kMantissaOffset));
  __ Ret();

  __ bind(&max_negative_int);
  // The max negative int32 is stored as a positive number in the mantissa of
  // a double because it uses a sign bit instead of using two's complement.
  // The actual mantissa bits stored are all 0 because the implicit most
  // significant 1 bit is not stored.
  non_smi_exponent += 1 << HeapNumber::kExponentShift;
  __ mov(ip, Operand(HeapNumber::kSignMask | non_smi_exponent));
  __ str(ip, FieldMemOperand(the_heap_number_, HeapNumber::kExponentOffset));
  __ mov(ip, Operand(0));
  __ str(ip, FieldMemOperand(the_heap_number_, HeapNumber::kMantissaOffset));
  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cc) {
  Label not_identical;
  __ cmp(r0, Operand(r1));
  __ b(ne, &not_identical);

  Register exp_mask_reg = r5;
  __ mov(exp_mask_reg, Operand(HeapNumber::kExponentMask));

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  Label heap_number, return_equal;
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if (cc == lt || cc == gt) {
    __ CompareObjectType(r0, r4, r4, FIRST_JS_OBJECT_TYPE);
    __ b(ge, slow);
  } else {
    __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
    __ b(eq, &heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cc != eq) {
      __ cmp(r4, Operand(FIRST_JS_OBJECT_TYPE));
      __ b(ge, slow);
    }
  }
  __ bind(&return_equal);
  if (cc == lt) {
    __ mov(r0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cc == gt) {
    __ mov(r0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(r0, Operand(0));        // Things are <=, >=, ==, === themselves.
  }
  __ mov(pc, Operand(lr));  // Return.

  // For less and greater we don't have to check for NaN since the result of
  // x < x is false regardless.  For the others here is some code to check
  // for NaN.
  if (cc != lt && cc != gt) {
    __ bind(&heap_number);
    // It is a heap number, so return non-equal if it's NaN and equal if it's
    // not NaN.
    // The representation of NaN values has all exponent bits (52..62) set,
    // and not all mantissa bits (0..51) clear.
    // Read top bits of double representation (second word of value).
    __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    __ and_(r3, r2, Operand(exp_mask_reg));
    __ cmp(r3, Operand(exp_mask_reg));
    __ b(ne, &return_equal);

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ mov(r2, Operand(r2, LSL, HeapNumber::kNonMantissaBitsInTopWord));
    // Or with all low-bits of mantissa.
    __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
    __ orr(r0, r3, Operand(r2), SetCC);
    // For equal we already have the right value in r0:  Return zero (equal)
    // if all bits in mantissa are zero (it's an Infinity) and non-zero if not
    // (it's a NaN).  For <= and >= we need to load r0 with the failing value
    // if it's a NaN.
    if (cc != eq) {
      // All-zero means Infinity means equal.
      __ mov(pc, Operand(lr), LeaveCC, eq);  // Return equal
      if (cc == le) {
        __ mov(r0, Operand(GREATER));  // NaN <= NaN should fail.
      } else {
        __ mov(r0, Operand(LESS));     // NaN >= NaN should fail.
      }
    }
    __ mov(pc, Operand(lr));  // Return.
  }
  // No fall through here.

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Label* rhs_not_nan,
                                    Label* slow,
                                    bool strict) {
  Label lhs_is_smi;
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &lhs_is_smi);

  // Rhs is a Smi.  Check whether the non-smi is a heap number.
  __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed.  Return non-equal (r0 is already not zero)
    __ mov(pc, Operand(lr), LeaveCC, ne);  // Return.
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Rhs is a smi, lhs is a number.
  __ push(lr);
  __ mov(r7, Operand(r1));
  ConvertToDoubleStub stub1(r3, r2, r7, r6);
  __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
  // r3 and r2 are rhs as double.
  __ ldr(r1, FieldMemOperand(r0, HeapNumber::kValueOffset + kPointerSize));
  __ ldr(r0, FieldMemOperand(r0, HeapNumber::kValueOffset));
  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a Smi.
  __ pop(lr);
  __ jmp(rhs_not_nan);

  __ bind(&lhs_is_smi);
  // Lhs is a Smi.  Check whether the non-smi is a heap number.
  __ CompareObjectType(r1, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed.  Return non-equal.
    __ mov(r0, Operand(1), LeaveCC, ne);  // Non-zero indicates not equal.
    __ mov(pc, Operand(lr), LeaveCC, ne);  // Return.
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Lhs is a smi, rhs is a number.
  // r0 is Smi and r1 is heap number.
  __ push(lr);
  __ ldr(r2, FieldMemOperand(r1, HeapNumber::kValueOffset));
  __ ldr(r3, FieldMemOperand(r1, HeapNumber::kValueOffset + kPointerSize));
  __ mov(r7, Operand(r0));
  ConvertToDoubleStub stub2(r1, r0, r7, r6);
  __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
  __ pop(lr);
  // Fall through to both_loaded_as_doubles.
}


void EmitNanCheck(MacroAssembler* masm, Label* rhs_not_nan, Condition cc) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register lhs_exponent = exp_first ? r0 : r1;
  Register rhs_exponent = exp_first ? r2 : r3;
  Register lhs_mantissa = exp_first ? r1 : r0;
  Register rhs_mantissa = exp_first ? r3 : r2;
  Label one_is_nan, neither_is_nan;

  Register exp_mask_reg = r5;

  __ mov(exp_mask_reg, Operand(HeapNumber::kExponentMask));
  __ and_(r4, rhs_exponent, Operand(exp_mask_reg));
  __ cmp(r4, Operand(exp_mask_reg));
  __ b(ne, rhs_not_nan);
  __ mov(r4,
         Operand(rhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(rhs_mantissa, Operand(0));
  __ b(ne, &one_is_nan);

  __ bind(rhs_not_nan);
  __ mov(exp_mask_reg, Operand(HeapNumber::kExponentMask));
  __ and_(r4, lhs_exponent, Operand(exp_mask_reg));
  __ cmp(r4, Operand(exp_mask_reg));
  __ b(ne, &neither_is_nan);
  __ mov(r4,
         Operand(lhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(lhs_mantissa, Operand(0));
  __ b(eq, &neither_is_nan);

  __ bind(&one_is_nan);
  // NaN comparisons always fail.
  // Load whatever we need in r0 to make the comparison fail.
  if (cc == lt || cc == le) {
    __ mov(r0, Operand(GREATER));
  } else {
    __ mov(r0, Operand(LESS));
  }
  __ mov(pc, Operand(lr));  // Return.

  __ bind(&neither_is_nan);
}


// See comment at call site.
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register lhs_exponent = exp_first ? r0 : r1;
  Register rhs_exponent = exp_first ? r2 : r3;
  Register lhs_mantissa = exp_first ? r1 : r0;
  Register rhs_mantissa = exp_first ? r3 : r2;

  // r0, r1, r2, r3 have the two doubles.  Neither is a NaN.
  if (cc == eq) {
    // Doubles are not equal unless they have the same bit pattern.
    // Exception: 0 and -0.
    __ cmp(lhs_mantissa, Operand(rhs_mantissa));
    __ orr(r0, lhs_mantissa, Operand(rhs_mantissa), LeaveCC, ne);
    // Return non-zero if the numbers are unequal.
    __ mov(pc, Operand(lr), LeaveCC, ne);

    __ sub(r0, lhs_exponent, Operand(rhs_exponent), SetCC);
    // If exponents are equal then return 0.
    __ mov(pc, Operand(lr), LeaveCC, eq);

    // Exponents are unequal.  The only way we can return that the numbers
    // are equal is if one is -0 and the other is 0.  We already dealt
    // with the case where both are -0 or both are 0.
    // We start by seeing if the mantissas (that are equal) or the bottom
    // 31 bits of the rhs exponent are non-zero.  If so we return not
    // equal.
    __ orr(r4, rhs_mantissa, Operand(rhs_exponent, LSL, kSmiTagSize), SetCC);
    __ mov(r0, Operand(r4), LeaveCC, ne);
    __ mov(pc, Operand(lr), LeaveCC, ne);  // Return conditionally.
    // Now they are equal if and only if the lhs exponent is zero in its
    // low 31 bits.
    __ mov(r0, Operand(lhs_exponent, LSL, kSmiTagSize));
    __ mov(pc, Operand(lr));
  } else {
    // Call a native function to do a comparison between two non-NaNs.
    // Call C routine that may not cause GC or other trouble.
    __ mov(r5, Operand(ExternalReference::compare_doubles()));
    __ Jump(r5);  // Tail call.
  }
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm) {
    // If either operand is a JSObject or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
    Label first_non_object;
    // Get the type of the first operand into r2 and compare it with
    // FIRST_JS_OBJECT_TYPE.
    __ CompareObjectType(r0, r2, r2, FIRST_JS_OBJECT_TYPE);
    __ b(lt, &first_non_object);

    // Return non-zero (r0 is not zero)
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ mov(pc, Operand(lr));  // Return.

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ cmp(r2, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    __ CompareObjectType(r1, r3, r3, FIRST_JS_OBJECT_TYPE);
    __ b(ge, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ cmp(r3, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  __ CompareObjectType(r0, r2, r2, HEAP_NUMBER_TYPE);
  __ b(ne, not_heap_numbers);
  __ CompareObjectType(r1, r3, r3, HEAP_NUMBER_TYPE);
  __ b(ne, slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  __ ldr(r2, FieldMemOperand(r1, HeapNumber::kValueOffset));
  __ ldr(r3, FieldMemOperand(r1, HeapNumber::kValueOffset + kPointerSize));
  __ ldr(r1, FieldMemOperand(r0, HeapNumber::kValueOffset + kPointerSize));
  __ ldr(r0, FieldMemOperand(r0, HeapNumber::kValueOffset));
  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for symbol-to-symbol equality.
static void EmitCheckForSymbols(MacroAssembler* masm, Label* slow) {
  // r2 is object type of r0.
  __ tst(r2, Operand(kIsNotStringMask));
  __ b(ne, slow);
  __ tst(r2, Operand(kIsSymbolMask));
  __ b(eq, slow);
  __ CompareObjectType(r1, r3, r3, FIRST_NONSTRING_TYPE);
  __ b(ge, slow);
  __ tst(r3, Operand(kIsSymbolMask));
  __ b(eq, slow);

  // Both are symbols.  We already checked they weren't the same pointer
  // so they are not equal.
  __ mov(r0, Operand(1));   // Non-zero indicates not equal.
  __ mov(pc, Operand(lr));  // Return.
}


// On entry r0 and r1 are the things to be compared.  On exit r0 is 0,
// positive or negative to indicate the result of the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, rhs_not_nan;

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc_);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(0, Smi::FromInt(0));
  __ and_(r2, r0, Operand(r1));
  __ tst(r2, Operand(kSmiTagMask));
  __ b(ne, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to rhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison and the numbers have been loaded into r0, r1, r2, r3 as doubles.
  EmitSmiNonsmiComparison(masm, &rhs_not_nan, &slow, strict_);

  __ bind(&both_loaded_as_doubles);
  // r0, r1, r2, r3 are the double representations of the left hand side
  // and the right hand side.

  // Checks for NaN in the doubles we have loaded.  Can return the answer or
  // fall through if neither is a NaN.  Also binds rhs_not_nan.
  EmitNanCheck(masm, &rhs_not_nan, cc_);

  // Compares two doubles in r0, r1, r2, r3 that are not NaNs.  Returns the
  // answer.  Never falls through.
  EmitTwoNonNanDoubleComparison(masm, cc_);

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi.  The objects are in r0 and r1.
  if (strict_) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm);
  }

  Label check_for_symbols;
  // Check for heap-number-heap-number comparison.  Can jump to slow case,
  // or load both doubles into r0, r1, r2, r3 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to check_for_symbols.
  // In this case r2 will contain the type of r0.
  EmitCheckForTwoHeapNumbers(masm,
                             &both_loaded_as_doubles,
                             &check_for_symbols,
                             &slow);

  __ bind(&check_for_symbols);
  if (cc_ == eq) {
    // Either jumps to slow or returns the answer.  Assumes that r2 is the type
    // of r0 on entry.
    EmitCheckForSymbols(masm, &slow);
  }

  __ bind(&slow);
  __ push(lr);
  __ push(r1);
  __ push(r0);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  int arg_count = 1;  // Not counting receiver.
  if (cc_ == eq) {
    native = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native = Builtins::COMPARE;
    int ncr;  // NaN compare result
    if (cc_ == lt || cc_ == le) {
      ncr = GREATER;
    } else {
      ASSERT(cc_ == gt || cc_ == ge);  // remaining cases
      ncr = LESS;
    }
    arg_count++;
    __ mov(r0, Operand(Smi::FromInt(ncr)));
    __ push(r0);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ mov(r0, Operand(arg_count));
  __ InvokeBuiltin(native, CALL_JS);
  __ cmp(r0, Operand(0));
  __ pop(pc);
}


// Allocates a heap number or jumps to the label if the young space is full and
// a scavenge is needed.
static void AllocateHeapNumber(
    MacroAssembler* masm,
    Label* need_gc,       // Jump here if young space is full.
    Register result,  // The tagged address of the new heap number.
    Register scratch1,  // A scratch register.
    Register scratch2) {  // Another scratch register.
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  __ AllocateInNewSpace(HeapNumber::kSize / kPointerSize,
                        result,
                        scratch1,
                        scratch2,
                        need_gc,
                        TAG_OBJECT);

  // Get heap number map and store it in the allocated object.
  __ LoadRoot(scratch1, Heap::kHeapNumberMapRootIndex);
  __ str(scratch1, FieldMemOperand(result, HeapObject::kMapOffset));
}


// We fall into this code if the operands were Smis, but the result was
// not (eg. overflow).  We branch into this code (to the not_smi label) if
// the operands were not both Smi.  The operands are in r0 and r1.  In order
// to call the C-implemented binary fp operation routines we need to end up
// with the double precision floating point operands in r0 and r1 (for the
// value in r1) and r2 and r3 (for the value in r0).
static void HandleBinaryOpSlowCases(MacroAssembler* masm,
                                    Label* not_smi,
                                    const Builtins::JavaScript& builtin,
                                    Token::Value operation,
                                    OverwriteMode mode) {
  Label slow, slow_pop_2_first, do_the_call;
  Label r0_is_smi, r1_is_smi, finished_loading_r0, finished_loading_r1;
  // Smi-smi case (overflow).
  // Since both are Smis there is no heap number to overwrite, so allocate.
  // The new heap number is in r5.  r6 and r7 are scratch.
  AllocateHeapNumber(masm, &slow, r5, r6, r7);
  // Write Smi from r0 to r3 and r2 in double format.  r6 is scratch.
  __ mov(r7, Operand(r0));
  ConvertToDoubleStub stub1(r3, r2, r7, r6);
  __ push(lr);
  __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
  // Write Smi from r1 to r1 and r0 in double format.  r6 is scratch.
  __ mov(r7, Operand(r1));
  ConvertToDoubleStub stub2(r1, r0, r7, r6);
  __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
  __ pop(lr);
  __ jmp(&do_the_call);  // Tail call.  No return.

  // We jump to here if something goes wrong (one param is not a number of any
  // sort or new-space allocation fails).
  __ bind(&slow);
  __ push(r1);
  __ push(r0);
  __ mov(r0, Operand(1));  // Set number of arguments.
  __ InvokeBuiltin(builtin, JUMP_JS);  // Tail call.  No return.

  // We branch here if at least one of r0 and r1 is not a Smi.
  __ bind(not_smi);
  if (mode == NO_OVERWRITE) {
    // In the case where there is no chance of an overwritable float we may as
    // well do the allocation immediately while r0 and r1 are untouched.
    AllocateHeapNumber(masm, &slow, r5, r6, r7);
  }

  // Move r0 to a double in r2-r3.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &r0_is_smi);  // It's a Smi so don't check it's a heap number.
  __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
  __ b(ne, &slow);
  if (mode == OVERWRITE_RIGHT) {
    __ mov(r5, Operand(r0));  // Overwrite this heap number.
  }
  // Calling convention says that second double is in r2 and r3.
  __ ldr(r2, FieldMemOperand(r0, HeapNumber::kValueOffset));
  __ ldr(r3, FieldMemOperand(r0, HeapNumber::kValueOffset + 4));
  __ jmp(&finished_loading_r0);
  __ bind(&r0_is_smi);
  if (mode == OVERWRITE_RIGHT) {
    // We can't overwrite a Smi so get address of new heap number into r5.
    AllocateHeapNumber(masm, &slow, r5, r6, r7);
  }
  // Write Smi from r0 to r3 and r2 in double format.
  __ mov(r7, Operand(r0));
  ConvertToDoubleStub stub3(r3, r2, r7, r6);
  __ push(lr);
  __ Call(stub3.GetCode(), RelocInfo::CODE_TARGET);
  __ pop(lr);
  __ bind(&finished_loading_r0);

  // Move r1 to a double in r0-r1.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &r1_is_smi);  // It's a Smi so don't check it's a heap number.
  __ CompareObjectType(r1, r4, r4, HEAP_NUMBER_TYPE);
  __ b(ne, &slow);
  if (mode == OVERWRITE_LEFT) {
    __ mov(r5, Operand(r1));  // Overwrite this heap number.
  }
  // Calling convention says that first double is in r0 and r1.
  __ ldr(r0, FieldMemOperand(r1, HeapNumber::kValueOffset));
  __ ldr(r1, FieldMemOperand(r1, HeapNumber::kValueOffset + 4));
  __ jmp(&finished_loading_r1);
  __ bind(&r1_is_smi);
  if (mode == OVERWRITE_LEFT) {
    // We can't overwrite a Smi so get address of new heap number into r5.
    AllocateHeapNumber(masm, &slow, r5, r6, r7);
  }
  // Write Smi from r1 to r1 and r0 in double format.
  __ mov(r7, Operand(r1));
  ConvertToDoubleStub stub4(r1, r0, r7, r6);
  __ push(lr);
  __ Call(stub4.GetCode(), RelocInfo::CODE_TARGET);
  __ pop(lr);
  __ bind(&finished_loading_r1);

  __ bind(&do_the_call);
  // r0: Left value (least significant part of mantissa).
  // r1: Left value (sign, exponent, top of mantissa).
  // r2: Right value (least significant part of mantissa).
  // r3: Right value (sign, exponent, top of mantissa).
  // r5: Address of heap number for result.
  __ push(lr);   // For later.
  __ push(r5);   // Address of heap number that is answer.
  __ AlignStack(0);
  // Call C routine that may not cause GC or other trouble.
  __ mov(r5, Operand(ExternalReference::double_fp_operation(operation)));
  __ Call(r5);
  __ pop(r4);  // Address of heap number.
  __ cmp(r4, Operand(Smi::FromInt(0)));
  __ pop(r4, eq);  // Conditional pop instruction to get rid of alignment push.
  // Store answer in the overwritable heap number.
#if !defined(USE_ARM_EABI)
  // Double returned in fp coprocessor register 0 and 1, encoded as register
  // cr8.  Offsets must be divisible by 4 for coprocessor so we need to
  // substract the tag from r4.
  __ sub(r5, r4, Operand(kHeapObjectTag));
  __ stc(p1, cr8, MemOperand(r5, HeapNumber::kValueOffset));
#else
  // Double returned in registers 0 and 1.
  __ str(r0, FieldMemOperand(r4, HeapNumber::kValueOffset));
  __ str(r1, FieldMemOperand(r4, HeapNumber::kValueOffset + 4));
#endif
  __ mov(r0, Operand(r4));
  // And we are done.
  __ pop(pc);
}


// Tries to get a signed int32 out of a double precision floating point heap
// number.  Rounds towards 0.  Fastest for doubles that are in the ranges
// -0x7fffffff to -0x40000000 or 0x40000000 to 0x7fffffff.  This corresponds
// almost to the range of signed int32 values that are not Smis.  Jumps to the
// label 'slow' if the double isn't in the range -0x80000000.0 to 0x80000000.0
// (excluding the endpoints).
static void GetInt32(MacroAssembler* masm,
                     Register source,
                     Register dest,
                     Register scratch,
                     Register scratch2,
                     Label* slow) {
  Label right_exponent, done;
  // Get exponent word.
  __ ldr(scratch, FieldMemOperand(source, HeapNumber::kExponentOffset));
  // Get exponent alone in scratch2.
  __ and_(scratch2, scratch, Operand(HeapNumber::kExponentMask));
  // Load dest with zero.  We use this either for the final shift or
  // for the answer.
  __ mov(dest, Operand(0));
  // Check whether the exponent matches a 32 bit signed int that is not a Smi.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).  This is
  // the exponent that we are fastest at and also the highest exponent we can
  // handle here.
  const uint32_t non_smi_exponent =
      (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
  __ cmp(scratch2, Operand(non_smi_exponent));
  // If we have a match of the int32-but-not-Smi exponent then skip some logic.
  __ b(eq, &right_exponent);
  // If the exponent is higher than that then go to slow case.  This catches
  // numbers that don't fit in a signed int32, infinities and NaNs.
  __ b(gt, slow);

  // We know the exponent is smaller than 30 (biased).  If it is less than
  // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
  // it rounds to zero.
  const uint32_t zero_exponent =
      (HeapNumber::kExponentBias + 0) << HeapNumber::kExponentShift;
  __ sub(scratch2, scratch2, Operand(zero_exponent), SetCC);
  // Dest already has a Smi zero.
  __ b(lt, &done);
  // We have a shifted exponent between 0 and 30 in scratch2.
  __ mov(dest, Operand(scratch2, LSR, HeapNumber::kExponentShift));
  // We now have the exponent in dest.  Subtract from 30 to get
  // how much to shift down.
  __ rsb(dest, dest, Operand(30));

  __ bind(&right_exponent);
  // Get the top bits of the mantissa.
  __ and_(scratch2, scratch, Operand(HeapNumber::kMantissaMask));
  // Put back the implicit 1.
  __ orr(scratch2, scratch2, Operand(1 << HeapNumber::kExponentShift));
  // Shift up the mantissa bits to take up the space the exponent used to take.
  // We just orred in the implicit bit so that took care of one and we want to
  // leave the sign bit 0 so we subtract 2 bits from the shift distance.
  const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
  __ mov(scratch2, Operand(scratch2, LSL, shift_distance));
  // Put sign in zero flag.
  __ tst(scratch, Operand(HeapNumber::kSignMask));
  // Get the second half of the double.  For some exponents we don't actually
  // need this because the bits get shifted out again, but it's probably slower
  // to test than just to do it.
  __ ldr(scratch, FieldMemOperand(source, HeapNumber::kMantissaOffset));
  // Shift down 22 bits to get the last 10 bits.
  __ orr(scratch, scratch2, Operand(scratch, LSR, 32 - shift_distance));
  // Move down according to the exponent.
  __ mov(dest, Operand(scratch, LSR, dest));
  // Fix sign if sign bit was set.
  __ rsb(dest, dest, Operand(0), LeaveCC, ne);
  __ bind(&done);
}


// For bitwise ops where the inputs are not both Smis we here try to determine
// whether both inputs are either Smis or at least heap numbers that can be
// represented by a 32 bit signed value.  We truncate towards zero as required
// by the ES spec.  If this is the case we do the bitwise op and see if the
// result is a Smi.  If so, great, otherwise we try to find a heap number to
// write the answer into (either by allocating or by overwriting).
// On entry the operands are in r0 and r1.  On exit the answer is in r0.
void GenericBinaryOpStub::HandleNonSmiBitwiseOp(MacroAssembler* masm) {
  Label slow, result_not_a_smi;
  Label r0_is_smi, r1_is_smi;
  Label done_checking_r0, done_checking_r1;

  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &r1_is_smi);  // It's a Smi so don't check it's a heap number.
  __ CompareObjectType(r1, r4, r4, HEAP_NUMBER_TYPE);
  __ b(ne, &slow);
  GetInt32(masm, r1, r3, r4, r5, &slow);
  __ jmp(&done_checking_r1);
  __ bind(&r1_is_smi);
  __ mov(r3, Operand(r1, ASR, 1));
  __ bind(&done_checking_r1);

  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &r0_is_smi);  // It's a Smi so don't check it's a heap number.
  __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
  __ b(ne, &slow);
  GetInt32(masm, r0, r2, r4, r5, &slow);
  __ jmp(&done_checking_r0);
  __ bind(&r0_is_smi);
  __ mov(r2, Operand(r0, ASR, 1));
  __ bind(&done_checking_r0);

  // r0 and r1: Original operands (Smi or heap numbers).
  // r2 and r3: Signed int32 operands.
  switch (op_) {
    case Token::BIT_OR:  __ orr(r2, r2, Operand(r3)); break;
    case Token::BIT_XOR: __ eor(r2, r2, Operand(r3)); break;
    case Token::BIT_AND: __ and_(r2, r2, Operand(r3)); break;
    case Token::SAR:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, ASR, r2));
      break;
    case Token::SHR:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, LSR, r2), SetCC);
      // SHR is special because it is required to produce a positive answer.
      // The code below for writing into heap numbers isn't capable of writing
      // the register as an unsigned int so we go to slow case if we hit this
      // case.
      __ b(mi, &slow);
      break;
    case Token::SHL:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, LSL, r2));
      break;
    default: UNREACHABLE();
  }
  // check that the *signed* result fits in a smi
  __ add(r3, r2, Operand(0x40000000), SetCC);
  __ b(mi, &result_not_a_smi);
  __ mov(r0, Operand(r2, LSL, kSmiTagSize));
  __ Ret();

  Label have_to_allocate, got_a_heap_number;
  __ bind(&result_not_a_smi);
  switch (mode_) {
    case OVERWRITE_RIGHT: {
      __ tst(r0, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(r0));
      break;
    }
    case OVERWRITE_LEFT: {
      __ tst(r1, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(r1));
      break;
    }
    case NO_OVERWRITE: {
      // Get a new heap number in r5.  r6 and r7 are scratch.
      AllocateHeapNumber(masm, &slow, r5, r6, r7);
    }
    default: break;
  }
  __ bind(&got_a_heap_number);
  // r2: Answer as signed int32.
  // r5: Heap number to write answer into.

  // Nothing can go wrong now, so move the heap number to r0, which is the
  // result.
  __ mov(r0, Operand(r5));

  // Tail call that writes the int32 in r2 to the heap number in r0, using
  // r3 as scratch.  r0 is preserved and returned.
  WriteInt32ToHeapNumberStub stub(r2, r0, r3);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  if (mode_ != NO_OVERWRITE) {
    __ bind(&have_to_allocate);
    // Get a new heap number in r5.  r6 and r7 are scratch.
    AllocateHeapNumber(masm, &slow, r5, r6, r7);
    __ jmp(&got_a_heap_number);
  }

  // If all else failed then we go to the runtime system.
  __ bind(&slow);
  __ push(r1);  // restore stack
  __ push(r0);
  __ mov(r0, Operand(1));  // 1 argument (not counting receiver).
  switch (op_) {
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_JS);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_JS);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_JS);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_JS);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_JS);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_JS);
      break;
    default:
      UNREACHABLE();
  }
}


// Can we multiply by x with max two shifts and an add.
// This answers yes to all integers from 2 to 10.
static bool IsEasyToMultiplyBy(int x) {
  if (x < 2) return false;                          // Avoid special cases.
  if (x > (Smi::kMaxValue + 1) >> 2) return false;  // Almost always overflows.
  if (IsPowerOf2(x)) return true;                   // Simple shift.
  if (PopCountLessThanEqual2(x)) return true;       // Shift and add and shift.
  if (IsPowerOf2(x + 1)) return true;               // Patterns like 11111.
  return false;
}


// Can multiply by anything that IsEasyToMultiplyBy returns true for.
// Source and destination may be the same register.  This routine does
// not set carry and overflow the way a mul instruction would.
static void MultiplyByKnownInt(MacroAssembler* masm,
                               Register source,
                               Register destination,
                               int known_int) {
  if (IsPowerOf2(known_int)) {
    __ mov(destination, Operand(source, LSL, BitPosition(known_int)));
  } else if (PopCountLessThanEqual2(known_int)) {
    int first_bit = BitPosition(known_int);
    int second_bit = BitPosition(known_int ^ (1 << first_bit));
    __ add(destination, source, Operand(source, LSL, second_bit - first_bit));
    if (first_bit != 0) {
      __ mov(destination, Operand(destination, LSL, first_bit));
    }
  } else {
    ASSERT(IsPowerOf2(known_int + 1));  // Patterns like 1111.
    int the_bit = BitPosition(known_int + 1);
    __ rsb(destination, source, Operand(source, LSL, the_bit));
  }
}


// This function (as opposed to MultiplyByKnownInt) takes the known int in a
// a register for the cases where it doesn't know a good trick, and may deliver
// a result that needs shifting.
static void MultiplyByKnownInt2(
    MacroAssembler* masm,
    Register result,
    Register source,
    Register known_int_register,   // Smi tagged.
    int known_int,
    int* required_shift) {  // Including Smi tag shift
  switch (known_int) {
    case 3:
      __ add(result, source, Operand(source, LSL, 1));
      *required_shift = 1;
      break;
    case 5:
      __ add(result, source, Operand(source, LSL, 2));
      *required_shift = 1;
      break;
    case 6:
      __ add(result, source, Operand(source, LSL, 1));
      *required_shift = 2;
      break;
    case 7:
      __ rsb(result, source, Operand(source, LSL, 3));
      *required_shift = 1;
      break;
    case 9:
      __ add(result, source, Operand(source, LSL, 3));
      *required_shift = 1;
      break;
    case 10:
      __ add(result, source, Operand(source, LSL, 2));
      *required_shift = 2;
      break;
    default:
      ASSERT(!IsPowerOf2(known_int));  // That would be very inefficient.
      __ mul(result, source, known_int_register);
      *required_shift = 0;
  }
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  // r1 : x
  // r0 : y
  // result : r0

  // All ops need to know whether we are dealing with two Smis.  Set up r2 to
  // tell us that.
  __ orr(r2, r1, Operand(r0));  // r2 = x | y;

  switch (op_) {
    case Token::ADD: {
      Label not_smi;
      // Fast path.
      ASSERT(kSmiTag == 0);  // Adjust code below.
      __ tst(r2, Operand(kSmiTagMask));
      __ b(ne, &not_smi);
      __ add(r0, r1, Operand(r0), SetCC);  // Add y optimistically.
      // Return if no overflow.
      __ Ret(vc);
      __ sub(r0, r0, Operand(r1));  // Revert optimistic add.

      HandleBinaryOpSlowCases(masm,
                              &not_smi,
                              Builtins::ADD,
                              Token::ADD,
                              mode_);
      break;
    }

    case Token::SUB: {
      Label not_smi;
      // Fast path.
      ASSERT(kSmiTag == 0);  // Adjust code below.
      __ tst(r2, Operand(kSmiTagMask));
      __ b(ne, &not_smi);
      __ sub(r0, r1, Operand(r0), SetCC);  // Subtract y optimistically.
      // Return if no overflow.
      __ Ret(vc);
      __ sub(r0, r1, Operand(r0));  // Revert optimistic subtract.

      HandleBinaryOpSlowCases(masm,
                              &not_smi,
                              Builtins::SUB,
                              Token::SUB,
                              mode_);
      break;
    }

    case Token::MUL: {
      Label not_smi, slow;
      ASSERT(kSmiTag == 0);  // adjust code below
      __ tst(r2, Operand(kSmiTagMask));
      __ b(ne, &not_smi);
      // Remove tag from one operand (but keep sign), so that result is Smi.
      __ mov(ip, Operand(r0, ASR, kSmiTagSize));
      // Do multiplication
      __ smull(r3, r2, r1, ip);  // r3 = lower 32 bits of ip*r1.
      // Go slow on overflows (overflow bit is not set).
      __ mov(ip, Operand(r3, ASR, 31));
      __ cmp(ip, Operand(r2));  // no overflow if higher 33 bits are identical
      __ b(ne, &slow);
      // Go slow on zero result to handle -0.
      __ tst(r3, Operand(r3));
      __ mov(r0, Operand(r3), LeaveCC, ne);
      __ Ret(ne);
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ add(r2, r0, Operand(r1), SetCC);
      __ mov(r0, Operand(Smi::FromInt(0)), LeaveCC, pl);
      __ Ret(pl);  // Return Smi 0 if the non-zero one was positive.
      // Slow case.  We fall through here if we multiplied a negative number
      // with 0, because that would mean we should produce -0.
      __ bind(&slow);

      HandleBinaryOpSlowCases(masm,
                              &not_smi,
                              Builtins::MUL,
                              Token::MUL,
                              mode_);
      break;
    }

    case Token::DIV:
    case Token::MOD: {
      Label not_smi;
      if (specialized_on_rhs_) {
        Label smi_is_unsuitable;
        __ BranchOnNotSmi(r1, &not_smi);
        if (IsPowerOf2(constant_rhs_)) {
          if (op_ == Token::MOD) {
            __ and_(r0,
                    r1,
                    Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)),
                    SetCC);
            // We now have the answer, but if the input was negative we also
            // have the sign bit.  Our work is done if the result is
            // positive or zero:
            __ Ret(pl);
            // A mod of a negative left hand side must return a negative number.
            // Unfortunately if the answer is 0 then we must return -0.  And we
            // already optimistically trashed r0 so we may need to restore it.
            __ eor(r0, r0, Operand(0x80000000u), SetCC);
            // Next two instructions are conditional on the answer being -0.
            __ mov(r0, Operand(Smi::FromInt(constant_rhs_)), LeaveCC, eq);
            __ b(eq, &smi_is_unsuitable);
            // We need to subtract the dividend.  Eg. -3 % 4 == -3.
            __ sub(r0, r0, Operand(Smi::FromInt(constant_rhs_)));
          } else {
            ASSERT(op_ == Token::DIV);
            __ tst(r1,
                   Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)));
            __ b(ne, &smi_is_unsuitable);  // Go slow on negative or remainder.
            int shift = 0;
            int d = constant_rhs_;
            while ((d & 1) == 0) {
              d >>= 1;
              shift++;
            }
            __ mov(r0, Operand(r1, LSR, shift));
            __ bic(r0, r0, Operand(kSmiTagMask));
          }
        } else {
          // Not a power of 2.
          __ tst(r1, Operand(0x80000000u));
          __ b(ne, &smi_is_unsuitable);
          // Find a fixed point reciprocal of the divisor so we can divide by
          // multiplying.
          double divisor = 1.0 / constant_rhs_;
          int shift = 32;
          double scale = 4294967296.0;  // 1 << 32.
          uint32_t mul;
          // Maximise the precision of the fixed point reciprocal.
          while (true) {
            mul = static_cast<uint32_t>(scale * divisor);
            if (mul >= 0x7fffffff) break;
            scale *= 2.0;
            shift++;
          }
          mul++;
          __ mov(r2, Operand(mul));
          __ umull(r3, r2, r2, r1);
          __ mov(r2, Operand(r2, LSR, shift - 31));
          // r2 is r1 / rhs.  r2 is not Smi tagged.
          // r0 is still the known rhs.  r0 is Smi tagged.
          // r1 is still the unkown lhs.  r1 is Smi tagged.
          int required_r4_shift = 0;  // Including the Smi tag shift of 1.
          // r4 = r2 * r0.
          MultiplyByKnownInt2(masm,
                              r4,
                              r2,
                              r0,
                              constant_rhs_,
                              &required_r4_shift);
          // r4 << required_r4_shift is now the Smi tagged rhs * (r1 / rhs).
          if (op_ == Token::DIV) {
            __ sub(r3, r1, Operand(r4, LSL, required_r4_shift), SetCC);
            __ b(ne, &smi_is_unsuitable);  // There was a remainder.
            __ mov(r0, Operand(r2, LSL, kSmiTagSize));
          } else {
            ASSERT(op_ == Token::MOD);
            __ sub(r0, r1, Operand(r4, LSL, required_r4_shift));
          }
        }
        __ Ret();
        __ bind(&smi_is_unsuitable);
      } else {
        __ jmp(&not_smi);
      }
      HandleBinaryOpSlowCases(masm,
                              &not_smi,
                              op_ == Token::MOD ? Builtins::MOD : Builtins::DIV,
                              op_,
                              mode_);
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHR:
    case Token::SHL: {
      Label slow;
      ASSERT(kSmiTag == 0);  // adjust code below
      __ tst(r2, Operand(kSmiTagMask));
      __ b(ne, &slow);
      switch (op_) {
        case Token::BIT_OR:  __ orr(r0, r0, Operand(r1)); break;
        case Token::BIT_AND: __ and_(r0, r0, Operand(r1)); break;
        case Token::BIT_XOR: __ eor(r0, r0, Operand(r1)); break;
        case Token::SAR:
          // Remove tags from right operand.
          __ mov(r2, Operand(r0, ASR, kSmiTagSize));  // y
          // Use only the 5 least significant bits of the shift count.
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r0, Operand(r1, ASR, r2));
          // Smi tag result.
          __ bic(r0, r0, Operand(kSmiTagMask));
          break;
        case Token::SHR:
          // Remove tags from operands.  We can't do this on a 31 bit number
          // because then the 0s get shifted into bit 30 instead of bit 31.
          __ mov(r3, Operand(r1, ASR, kSmiTagSize));  // x
          __ mov(r2, Operand(r0, ASR, kSmiTagSize));  // y
          // Use only the 5 least significant bits of the shift count.
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r3, Operand(r3, LSR, r2));
          // Unsigned shift is not allowed to produce a negative number, so
          // check the sign bit and the sign bit after Smi tagging.
          __ tst(r3, Operand(0xc0000000));
          __ b(ne, &slow);
          // Smi tag result.
          __ mov(r0, Operand(r3, LSL, kSmiTagSize));
          break;
        case Token::SHL:
          // Remove tags from operands.
          __ mov(r3, Operand(r1, ASR, kSmiTagSize));  // x
          __ mov(r2, Operand(r0, ASR, kSmiTagSize));  // y
          // Use only the 5 least significant bits of the shift count.
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r3, Operand(r3, LSL, r2));
          // Check that the signed result fits in a Smi.
          __ add(r2, r3, Operand(0x40000000), SetCC);
          __ b(mi, &slow);
          __ mov(r0, Operand(r3, LSL, kSmiTagSize));
          break;
        default: UNREACHABLE();
      }
      __ Ret();
      __ bind(&slow);
      HandleNonSmiBitwiseOp(masm);
      break;
    }

    default: UNREACHABLE();
  }
  // This code should be unreachable.
  __ stop("Unreachable");
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  // Do tail-call to runtime routine.  Runtime routines expect at least one
  // argument, so give it a Smi.
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ push(r0);
  __ TailCallRuntime(ExternalReference(Runtime::kStackGuard), 1, 1);

  __ StubReturn(1);
}


void UnarySubStub::Generate(MacroAssembler* masm) {
  Label undo;
  Label slow;
  Label not_smi;

  // Enter runtime system if the value is not a smi.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(ne, &not_smi);

  // Enter runtime system if the value of the expression is zero
  // to make sure that we switch between 0 and -0.
  __ cmp(r0, Operand(0));
  __ b(eq, &slow);

  // The value of the expression is a smi that is not zero.  Try
  // optimistic subtraction '0 - value'.
  __ rsb(r1, r0, Operand(0), SetCC);
  __ b(vs, &slow);

  __ mov(r0, Operand(r1));  // Set r0 to result.
  __ StubReturn(1);

  // Enter runtime system.
  __ bind(&slow);
  __ push(r0);
  __ mov(r0, Operand(0));  // Set number of arguments.
  __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_JS);

  __ bind(&not_smi);
  __ CompareObjectType(r0, r1, r1, HEAP_NUMBER_TYPE);
  __ b(ne, &slow);
  // r0 is a heap number.  Get a new heap number in r1.
  if (overwrite_) {
    __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
    __ str(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
  } else {
    AllocateHeapNumber(masm, &slow, r1, r2, r3);
    __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
    __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    __ str(r3, FieldMemOperand(r1, HeapNumber::kMantissaOffset));
    __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
    __ str(r2, FieldMemOperand(r1, HeapNumber::kExponentOffset));
    __ mov(r0, Operand(r1));
  }
  __ StubReturn(1);
}


int CEntryStub::MinorKey() {
  ASSERT(result_size_ <= 2);
  // Result returned in r0 or r0+r1 by default.
  return 0;
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // r0 holds the exception.

  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // Drop the sp to the top of the handler.
  __ mov(r3, Operand(ExternalReference(Top::k_handler_address)));
  __ ldr(sp, MemOperand(r3));

  // Restore the next handler and frame pointer, discard handler state.
  ASSERT(StackHandlerConstants::kNextOffset == 0);
  __ pop(r2);
  __ str(r2, MemOperand(r3));
  ASSERT(StackHandlerConstants::kFPOffset == 2 * kPointerSize);
  __ ldm(ia_w, sp, r3.bit() | fp.bit());  // r3: discarded state.

  // Before returning we restore the context from the frame pointer if
  // not NULL.  The frame pointer is NULL in the exception handler of a
  // JS entry frame.
  __ cmp(fp, Operand(0));
  // Set cp to NULL if fp is NULL.
  __ mov(cp, Operand(0), LeaveCC, eq);
  // Restore cp otherwise.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset), ne);
#ifdef DEBUG
  if (FLAG_debug_code) {
    __ mov(lr, Operand(pc));
  }
#endif
  ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ pop(pc);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // Drop sp to the top stack handler.
  __ mov(r3, Operand(ExternalReference(Top::k_handler_address)));
  __ ldr(sp, MemOperand(r3));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  __ ldr(r2, MemOperand(sp, kStateOffset));
  __ cmp(r2, Operand(StackHandler::ENTRY));
  __ b(eq, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  __ ldr(sp, MemOperand(sp, kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  ASSERT(StackHandlerConstants::kNextOffset == 0);
  __ pop(r2);
  __ str(r2, MemOperand(r3));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(Top::k_external_caught_exception_address);
    __ mov(r0, Operand(false));
    __ mov(r2, Operand(external_caught));
    __ str(r0, MemOperand(r2));

    // Set pending exception and r0 to out of memory exception.
    Failure* out_of_memory = Failure::OutOfMemoryException();
    __ mov(r0, Operand(reinterpret_cast<int32_t>(out_of_memory)));
    __ mov(r2, Operand(ExternalReference(Top::k_pending_exception_address)));
    __ str(r0, MemOperand(r2));
  }

  // Stack layout at this point. See also StackHandlerConstants.
  // sp ->   state (ENTRY)
  //         fp
  //         lr

  // Discard handler state (r2 is not used) and restore frame pointer.
  ASSERT(StackHandlerConstants::kFPOffset == 2 * kPointerSize);
  __ ldm(ia_w, sp, r2.bit() | fp.bit());  // r2: discarded state.
  // Before returning we restore the context from the frame pointer if
  // not NULL.  The frame pointer is NULL in the exception handler of a
  // JS entry frame.
  __ cmp(fp, Operand(0));
  // Set cp to NULL if fp is NULL.
  __ mov(cp, Operand(0), LeaveCC, eq);
  // Restore cp otherwise.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset), ne);
#ifdef DEBUG
  if (FLAG_debug_code) {
    __ mov(lr, Operand(pc));
  }
#endif
  ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ pop(pc);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              ExitFrame::Mode mode,
                              bool do_gc,
                              bool always_allocate) {
  // r0: result parameter for PerformGC, if any
  // r4: number of arguments including receiver  (C callee-saved)
  // r5: pointer to builtin function  (C callee-saved)
  // r6: pointer to the first argument (C callee-saved)

  if (do_gc) {
    // Passing r0.
    ExternalReference gc_reference = ExternalReference::perform_gc_function();
    __ Call(gc_reference.address(), RelocInfo::RUNTIME_ENTRY);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate) {
    __ mov(r0, Operand(scope_depth));
    __ ldr(r1, MemOperand(r0));
    __ add(r1, r1, Operand(1));
    __ str(r1, MemOperand(r0));
  }

  // Call C built-in.
  // r0 = argc, r1 = argv
  __ mov(r0, Operand(r4));
  __ mov(r1, Operand(r6));

  // TODO(1242173): To let the GC traverse the return address of the exit
  // frames, we need to know where the return address is. Right now,
  // we push it on the stack to be able to find it again, but we never
  // restore from it in case of changes, which makes it impossible to
  // support moving the C entry code stub. This should be fixed, but currently
  // this is OK because the CEntryStub gets generated so early in the V8 boot
  // sequence that it is not moving ever.
  masm->add(lr, pc, Operand(4));  // compute return address: (pc + 8) + 4
  masm->push(lr);
  masm->Jump(r5);

  if (always_allocate) {
    // It's okay to clobber r2 and r3 here. Don't mess with r0 and r1
    // though (contain the result).
    __ mov(r2, Operand(scope_depth));
    __ ldr(r3, MemOperand(r2));
    __ sub(r3, r3, Operand(1));
    __ str(r3, MemOperand(r2));
  }

  // check for failure result
  Label failure_returned;
  ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  // Lower 2 bits of r2 are 0 iff r0 has failure tag.
  __ add(r2, r0, Operand(1));
  __ tst(r2, Operand(kFailureTagMask));
  __ b(eq, &failure_returned);

  // Exit C frame and return.
  // r0:r1: result
  // sp: stack pointer
  // fp: frame pointer
  __ LeaveExitFrame(mode);

  // check if we should retry or throw exception
  Label retry;
  __ bind(&failure_returned);
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ tst(r0, Operand(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ b(eq, &retry);

  // Special handling of out of memory exceptions.
  Failure* out_of_memory = Failure::OutOfMemoryException();
  __ cmp(r0, Operand(reinterpret_cast<int32_t>(out_of_memory)));
  __ b(eq, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  __ mov(ip, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r3, MemOperand(ip));
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ ldr(r0, MemOperand(ip));
  __ str(r3, MemOperand(ip));

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(r0, Operand(Factory::termination_exception()));
  __ b(eq, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  __ bind(&retry);  // pass last failure (r0) as parameter (r0) when retrying
}


void CEntryStub::GenerateBody(MacroAssembler* masm, bool is_debug_break) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // r0: number of arguments including receiver
  // r1: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  ExitFrame::Mode mode = is_debug_break
      ? ExitFrame::MODE_DEBUG
      : ExitFrame::MODE_NORMAL;

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(mode);

  // r4: number of arguments (C callee-saved)
  // r5: pointer to builtin function (C callee-saved)
  // r6: pointer to first argument (C callee-saved)

  Label throw_normal_exception;
  Label throw_termination_exception;
  Label throw_out_of_memory_exception;

  // Call into the runtime system.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               mode,
               false,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               mode,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ mov(r0, Operand(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               mode,
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
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // [sp+0]: argv

  Label invoke, exit;

  // Called from C, so do not pop argc and args on exit (preserve sp)
  // No need to save register-passed args
  // Save callee-saved registers (incl. cp and fp), sp, and lr
  __ stm(db_w, sp, kCalleeSaved | lr.bit());

  // Get address of argv, see stm above.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  __ add(r4, sp, Operand((kNumCalleeSaved + 1)*kPointerSize));
  __ ldr(r4, MemOperand(r4));  // argv

  // Push a frame with special values setup to mark it as an entry frame.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  __ mov(r8, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ mov(r7, Operand(Smi::FromInt(marker)));
  __ mov(r6, Operand(Smi::FromInt(marker)));
  __ mov(r5, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  __ ldr(r5, MemOperand(r5));
  __ stm(db_w, sp, r5.bit() | r6.bit() | r7.bit() | r8.bit());

  // Setup frame pointer for the frame to be pushed.
  __ add(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Call a faked try-block that does the invoke.
  __ bl(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  // Coming in here the fp will be invalid because the PushTryHandler below
  // sets it to 0 to signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ str(r0, MemOperand(ip));
  __ mov(r0, Operand(reinterpret_cast<int32_t>(Failure::Exception())));
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r0-r4, r5-r7 are available.
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bl(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ mov(ip, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r5, MemOperand(ip));
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ str(r5, MemOperand(ip));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ mov(ip, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ mov(ip, Operand(entry));
  }
  __ ldr(ip, MemOperand(ip));  // deref address

  // Branch and link to JSEntryTrampoline.  We don't use the double underscore
  // macro for the add instruction because we don't want the coverage tool
  // inserting instructions here after we read the pc.
  __ mov(lr, Operand(pc));
  masm->add(pc, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Unlink this frame from the handler chain. When reading the
  // address of the next handler, there is no need to use the address
  // displacement since the current stack pointer (sp) points directly
  // to the stack handler.
  __ ldr(r3, MemOperand(sp, StackHandlerConstants::kNextOffset));
  __ mov(ip, Operand(ExternalReference(Top::k_handler_address)));
  __ str(r3, MemOperand(ip));
  // No need to restore registers
  __ add(sp, sp, Operand(StackHandlerConstants::kSize));


  __ bind(&exit);  // r0 holds result
  // Restore the top frame descriptors from the stack.
  __ pop(r3);
  __ mov(ip, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  __ str(r3, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ add(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved registers and return.
#ifdef DEBUG
  if (FLAG_debug_code) {
    __ mov(lr, Operand(pc));
  }
#endif
  __ ldm(ia_w, sp, kCalleeSaved | pc.bit());
}


// This stub performs an instanceof, calling the builtin function if
// necessary.  Uses r1 for the object, r0 for the function that it may
// be an instance of (these are fetched from the stack).
void InstanceofStub::Generate(MacroAssembler* masm) {
  // Get the object - slow case for smis (we may need to throw an exception
  // depending on the rhs).
  Label slow, loop, is_instance, is_not_instance;
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));
  __ BranchOnSmi(r0, &slow);

  // Check that the left hand is a JS object and put map in r3.
  __ CompareObjectType(r0, r3, r2, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &slow);
  __ cmp(r2, Operand(LAST_JS_OBJECT_TYPE));
  __ b(gt, &slow);

  // Get the prototype of the function (r4 is result, r2 is scratch).
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));
  __ TryGetFunctionPrototype(r1, r4, r2, &slow);

  // Check that the function prototype is a JS object.
  __ BranchOnSmi(r4, &slow);
  __ CompareObjectType(r4, r5, r5, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &slow);
  __ cmp(r5, Operand(LAST_JS_OBJECT_TYPE));
  __ b(gt, &slow);

  // Register mapping: r3 is object map and r4 is function prototype.
  // Get prototype of object into r2.
  __ ldr(r2, FieldMemOperand(r3, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  __ bind(&loop);
  __ cmp(r2, Operand(r4));
  __ b(eq, &is_instance);
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r2, ip);
  __ b(eq, &is_not_instance);
  __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ ldr(r2, FieldMemOperand(r2, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ pop();
  __ pop();
  __ mov(pc, Operand(lr));  // Return.

  __ bind(&is_not_instance);
  __ mov(r0, Operand(Smi::FromInt(1)));
  __ pop();
  __ pop();
  __ mov(pc, Operand(lr));  // Return.

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  __ mov(r0, Operand(1));  // Arg count without receiver.
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_JS);
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor);

  // Nothing to do: The formal number of parameters has already been
  // passed in register r0 by calling function. Just return it.
  __ Jump(lr);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame and return it.
  __ bind(&adaptor);
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Jump(lr);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The displacement is the offset of the last parameter (if any)
  // relative to the frame pointer.
  static const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ BranchOnNotSmi(r1, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register eax. Use unsigned comparison to get negative
  // check for free.
  __ cmp(r1, r0);
  __ b(cs, &slow);

  // Read the argument from the stack and return it.
  __ sub(r3, r0, r1);
  __ add(r3, fp, Operand(r3, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, MemOperand(r3, kDisplacement));
  __ Jump(lr);

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmp(r1, r0);
  __ b(cs, &slow);

  // Read the argument from the adaptor frame and return it.
  __ sub(r3, r0, r1);
  __ add(r3, r2, Operand(r3, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, MemOperand(r3, kDisplacement));
  __ Jump(lr);

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ push(r1);
  __ TailCallRuntime(ExternalReference(Runtime::kGetArgumentsProperty), 1, 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &runtime);

  // Patch the arguments.length and the parameters pointer.
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ str(r0, MemOperand(sp, 0 * kPointerSize));
  __ add(r3, r2, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(r3, r3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ str(r3, MemOperand(sp, 1 * kPointerSize));

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kNewArgumentsFast), 3, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;
  // Get the function to call from the stack.
  // function, receiver [, arguments]
  __ ldr(r1, MemOperand(sp, (argc_ + 1) * kPointerSize));

  // Check that the function is really a JavaScript function.
  // r1: pushed function (to be verified)
  __ BranchOnSmi(r1, &slow);
  // Get the map of the function object.
  __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
  __ b(ne, &slow);

  // Fast-case: Invoke the function now.
  // r1: pushed function
  ParameterCount actual(argc_);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  __ mov(r0, Operand(argc_));  // Setup the number of arguments.
  __ mov(r2, Operand(0));
  __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
  __ Jump(Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline)),
          RelocInfo::CODE_TARGET);
}


int CompareStub::MinorKey() {
  // Encode the two parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) >> 28 < (1 << 15));
  return (static_cast<unsigned>(cc_) >> 27) | (strict_ ? 1 : 0);
}


#undef __

} }  // namespace v8::internal
