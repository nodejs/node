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

#if defined(V8_TARGET_ARCH_X64)

#include "bootstrapper.h"
#include "code-stubs.h"
#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "ic-inl.h"
#include "parser.h"
#include "regexp-macro-assembler.h"
#include "register-allocator-inl.h"
#include "scopes.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

// -------------------------------------------------------------------------
// Platform-specific FrameRegisterState functions.

void FrameRegisterState::Save(MacroAssembler* masm) const {
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    int action = registers_[i];
    if (action == kPush) {
      __ push(RegisterAllocator::ToRegister(i));
    } else if (action != kIgnore && (action & kSyncedFlag) == 0) {
      __ movq(Operand(rbp, action), RegisterAllocator::ToRegister(i));
    }
  }
}


void FrameRegisterState::Restore(MacroAssembler* masm) const {
  // Restore registers in reverse order due to the stack.
  for (int i = RegisterAllocator::kNumRegisters - 1; i >= 0; i--) {
    int action = registers_[i];
    if (action == kPush) {
      __ pop(RegisterAllocator::ToRegister(i));
    } else if (action != kIgnore) {
      action &= ~kSyncedFlag;
      __ movq(RegisterAllocator::ToRegister(i), Operand(rbp, action));
    }
  }
}


#undef __
#define __ ACCESS_MASM(masm_)

// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.

void DeferredCode::SaveRegisters() {
  frame_state_.Save(masm_);
}


void DeferredCode::RestoreRegisters() {
  frame_state_.Restore(masm_);
}


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void VirtualFrameRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  frame_state_->Save(masm);
}


void VirtualFrameRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  frame_state_->Restore(masm);
}


void ICRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterInternalFrame();
}


void ICRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveInternalFrame();
}


// -------------------------------------------------------------------------
// CodeGenState implementation.

CodeGenState::CodeGenState(CodeGenerator* owner)
    : owner_(owner),
      destination_(NULL),
      previous_(NULL) {
  owner_->set_state(this);
}


CodeGenState::CodeGenState(CodeGenerator* owner,
                           ControlDestination* destination)
    : owner_(owner),
      destination_(destination),
      previous_(owner->state()) {
  owner_->set_state(this);
}


CodeGenState::~CodeGenState() {
  ASSERT(owner_->state() == this);
  owner_->set_state(previous_);
}


// -------------------------------------------------------------------------
// CodeGenerator implementation.

CodeGenerator::CodeGenerator(MacroAssembler* masm)
    : deferred_(8),
      masm_(masm),
      info_(NULL),
      frame_(NULL),
      allocator_(NULL),
      state_(NULL),
      loop_nesting_(0),
      function_return_is_shadowed_(false),
      in_spilled_code_(false) {
}


// Calling conventions:
// rbp: caller's frame pointer
// rsp: stack pointer
// rdi: called JS function
// rsi: callee's context

void CodeGenerator::Generate(CompilationInfo* info) {
  // Record the position for debugging purposes.
  CodeForFunctionPosition(info->function());
  Comment cmnt(masm_, "[ function compiled by virtual frame code generator");

  // Initialize state.
  info_ = info;
  ASSERT(allocator_ == NULL);
  RegisterAllocator register_allocator(this);
  allocator_ = &register_allocator;
  ASSERT(frame_ == NULL);
  frame_ = new VirtualFrame();
  set_in_spilled_code(false);

  // Adjust for function-level loop nesting.
  ASSERT_EQ(0, loop_nesting_);
  loop_nesting_ = info->loop_nesting();

  JumpTarget::set_compiling_deferred_code(false);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    frame_->SpillAll();
    __ int3();
  }
#endif

  // New scope to get automatic timing calculation.
  { HistogramTimerScope codegen_timer(&Counters::code_generation);
    CodeGenState state(this);

    // Entry:
    // Stack: receiver, arguments, return address.
    // rbp: caller's frame pointer
    // rsp: stack pointer
    // rdi: called JS function
    // rsi: callee's context
    allocator_->Initialize();

    frame_->Enter();

    // Allocate space for locals and initialize them.
    frame_->AllocateStackSlots();

    // Allocate the local context if needed.
    int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (heap_slots > 0) {
      Comment cmnt(masm_, "[ allocate local context");
      // Allocate local context.
      // Get outer context and create a new context based on it.
      frame_->PushFunction();
      Result context;
      if (heap_slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(heap_slots);
        context = frame_->CallStub(&stub, 1);
      } else {
        context = frame_->CallRuntime(Runtime::kNewContext, 1);
      }

      // Update context local.
      frame_->SaveContextRegister();

      // Verify that the runtime call result and rsi agree.
      if (FLAG_debug_code) {
        __ cmpq(context.reg(), rsi);
        __ Assert(equal, "Runtime::NewContext should end up in rsi");
      }
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
      for (int i = 0; i < scope()->num_parameters(); i++) {
        Variable* par = scope()->parameter(i);
        Slot* slot = par->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          // The use of SlotOperand below is safe in unspilled code
          // because the slot is guaranteed to be a context slot.
          //
          // There are no parameters in the global scope.
          ASSERT(!scope()->is_global_scope());
          frame_->PushParameterAt(i);
          Result value = frame_->Pop();
          value.ToRegister();

          // SlotOperand loads context.reg() with the context object
          // stored to, used below in RecordWrite.
          Result context = allocator_->Allocate();
          ASSERT(context.is_valid());
          __ movq(SlotOperand(slot, context.reg()), value.reg());
          int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
          Result scratch = allocator_->Allocate();
          ASSERT(scratch.is_valid());
          frame_->Spill(context.reg());
          frame_->Spill(value.reg());
          __ RecordWrite(context.reg(), offset, value.reg(), scratch.reg());
        }
      }
    }

    // Store the arguments object.  This must happen after context
    // initialization because the arguments object may be stored in
    // the context.
    if (ArgumentsMode() != NO_ARGUMENTS_ALLOCATION) {
      StoreArgumentsObject(true);
    }

    // Initialize ThisFunction reference if present.
    if (scope()->is_function_scope() && scope()->function() != NULL) {
      frame_->Push(Factory::the_hole_value());
      StoreToSlot(scope()->function()->slot(), NOT_CONST_INIT);
    }

    // Initialize the function return target after the locals are set
    // up, because it needs the expected frame height from the frame.
    function_return_.set_direction(JumpTarget::BIDIRECTIONAL);
    function_return_is_shadowed_ = false;

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
      frame_->CallRuntime(Runtime::kTraceEnter, 0);
      // Ignore the return value.
    }
    CheckStack();

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
        frame_->CallRuntime(Runtime::kDebugTrace, 0);
        // Ignore the return value.
      }
#endif
      VisitStatements(info->function()->body());

      // Handle the return from the function.
      if (has_valid_frame()) {
        // If there is a valid frame, control flow can fall off the end of
        // the body.  In that case there is an implicit return statement.
        ASSERT(!function_return_is_shadowed_);
        CodeForReturnPosition(info->function());
        frame_->PrepareForReturn();
        Result undefined(Factory::undefined_value());
        if (function_return_.is_bound()) {
          function_return_.Jump(&undefined);
        } else {
          function_return_.Bind(&undefined);
          GenerateReturnSequence(&undefined);
        }
      } else if (function_return_.is_linked()) {
        // If the return target has dangling jumps to it, then we have not
        // yet generated the return sequence.  This can happen when (a)
        // control does not flow off the end of the body so we did not
        // compile an artificial return statement just above, and (b) there
        // are return statements in the body but (c) they are all shadowed.
        Result return_value;
        function_return_.Bind(&return_value);
        GenerateReturnSequence(&return_value);
      }
    }
  }

  // Adjust for function-level loop nesting.
  ASSERT_EQ(loop_nesting_, info->loop_nesting());
  loop_nesting_ = 0;

  // Code generation state must be reset.
  ASSERT(state_ == NULL);
  ASSERT(!function_return_is_shadowed_);
  function_return_.Unuse();
  DeleteFrame();

  // Process any deferred code using the register allocator.
  if (!HasStackOverflow()) {
    HistogramTimerScope deferred_timer(&Counters::deferred_code_generation);
    JumpTarget::set_compiling_deferred_code(true);
    ProcessDeferred();
    JumpTarget::set_compiling_deferred_code(false);
  }

  // There is no need to delete the register allocator, it is a
  // stack-allocated local.
  allocator_ = NULL;
}


Operand CodeGenerator::SlotOperand(Slot* slot, Register tmp) {
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
      ASSERT(!tmp.is(rsi));  // do not overwrite context register
      Register context = rsi;
      int chain_length = scope()->ContextChainLength(slot->var()->scope());
      for (int i = 0; i < chain_length; i++) {
        // Load the closure.
        // (All contexts, even 'with' contexts, have a closure,
        // and it is the same for all contexts inside a function.
        // There is no need to go to the function context first.)
        __ movq(tmp, ContextOperand(context, Context::CLOSURE_INDEX));
        // Load the function context (which is the incoming, outer context).
        __ movq(tmp, FieldOperand(tmp, JSFunction::kContextOffset));
        context = tmp;
      }
      // We may have a 'with' context now. Get the function context.
      // (In fact this mov may never be the needed, since the scope analysis
      // may not permit a direct context access in this case and thus we are
      // always at a function context. However it is safe to dereference be-
      // cause the function context of a function context is itself. Before
      // deleting this mov we should try to create a counter-example first,
      // though...)
      __ movq(tmp, ContextOperand(context, Context::FCONTEXT_INDEX));
      return ContextOperand(tmp, index);
    }

    default:
      UNREACHABLE();
      return Operand(rsp, 0);
  }
}


Operand CodeGenerator::ContextSlotOperandCheckExtensions(Slot* slot,
                                                         Result tmp,
                                                         JumpTarget* slow) {
  ASSERT(slot->type() == Slot::CONTEXT);
  ASSERT(tmp.is_register());
  Register context = rsi;

  for (Scope* s = scope(); s != slot->var()->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX),
                Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      __ movq(tmp.reg(), ContextOperand(context, Context::CLOSURE_INDEX));
      __ movq(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp.reg();
    }
  }
  // Check that last extension is NULL.
  __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX), Immediate(0));
  slow->Branch(not_equal, not_taken);
  __ movq(tmp.reg(), ContextOperand(context, Context::FCONTEXT_INDEX));
  return ContextOperand(tmp.reg(), slot->index());
}


// Emit code to load the value of an expression to the top of the
// frame. If the expression is boolean-valued it may be compiled (or
// partially compiled) into control flow to the control destination.
// If force_control is true, control flow is forced.
void CodeGenerator::LoadCondition(Expression* expr,
                                  ControlDestination* dest,
                                  bool force_control) {
  ASSERT(!in_spilled_code());
  int original_height = frame_->height();

  { CodeGenState new_state(this, dest);
    Visit(expr);

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
        !dest->is_used() &&
        frame_->height() == original_height) {
      dest->Goto(true);
    }
  }

  if (force_control && !dest->is_used()) {
    // Convert the TOS value into flow to the control destination.
    ToBoolean(dest);
  }

  ASSERT(!(force_control && !dest->is_used()));
  ASSERT(dest->is_used() || frame_->height() == original_height + 1);
}


void CodeGenerator::LoadAndSpill(Expression* expression) {
  ASSERT(in_spilled_code());
  set_in_spilled_code(false);
  Load(expression);
  frame_->SpillAll();
  set_in_spilled_code(true);
}


void CodeGenerator::Load(Expression* expr) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  ASSERT(!in_spilled_code());
  JumpTarget true_target;
  JumpTarget false_target;
  ControlDestination dest(&true_target, &false_target, true);
  LoadCondition(expr, &dest, false);

  if (dest.false_was_fall_through()) {
    // The false target was just bound.
    JumpTarget loaded;
    frame_->Push(Factory::false_value());
    // There may be dangling jumps to the true target.
    if (true_target.is_linked()) {
      loaded.Jump();
      true_target.Bind();
      frame_->Push(Factory::true_value());
      loaded.Bind();
    }

  } else if (dest.is_used()) {
    // There is true, and possibly false, control flow (with true as
    // the fall through).
    JumpTarget loaded;
    frame_->Push(Factory::true_value());
    if (false_target.is_linked()) {
      loaded.Jump();
      false_target.Bind();
      frame_->Push(Factory::false_value());
      loaded.Bind();
    }

  } else {
    // We have a valid value on top of the frame, but we still may
    // have dangling jumps to the true and false targets from nested
    // subexpressions (eg, the left subexpressions of the
    // short-circuited boolean operators).
    ASSERT(has_valid_frame());
    if (true_target.is_linked() || false_target.is_linked()) {
      JumpTarget loaded;
      loaded.Jump();  // Don't lose the current TOS.
      if (true_target.is_linked()) {
        true_target.Bind();
        frame_->Push(Factory::true_value());
        if (false_target.is_linked()) {
          loaded.Jump();
        }
      }
      if (false_target.is_linked()) {
        false_target.Bind();
        frame_->Push(Factory::false_value());
      }
      loaded.Bind();
    }
  }

  ASSERT(has_valid_frame());
  ASSERT(frame_->height() == original_height + 1);
}


void CodeGenerator::LoadGlobal() {
  if (in_spilled_code()) {
    frame_->EmitPush(GlobalObject());
  } else {
    Result temp = allocator_->Allocate();
    __ movq(temp.reg(), GlobalObject());
    frame_->Push(&temp);
  }
}


void CodeGenerator::LoadGlobalReceiver() {
  Result temp = allocator_->Allocate();
  Register reg = temp.reg();
  __ movq(reg, GlobalObject());
  __ movq(reg, FieldOperand(reg, GlobalObject::kGlobalReceiverOffset));
  frame_->Push(&temp);
}


void CodeGenerator::LoadTypeofExpression(Expression* expr) {
  // Special handling of identifiers as subexpressions of typeof.
  Variable* variable = expr->AsVariableProxy()->AsVariable();
  if (variable != NULL && !variable->is_this() && variable->is_global()) {
    // For a global variable we build the property reference
    // <global>.<variable> and perform a (regular non-contextual) property
    // load to make sure we do not get reference errors.
    Slot global(variable, Slot::CONTEXT, Context::GLOBAL_INDEX);
    Literal key(variable->name());
    Property property(&global, &key, RelocInfo::kNoPosition);
    Reference ref(this, &property);
    ref.GetValue();
  } else if (variable != NULL && variable->slot() != NULL) {
    // For a variable that rewrites to a slot, we signal it is the immediate
    // subexpression of a typeof.
    LoadFromSlotCheckForArguments(variable->slot(), INSIDE_TYPEOF);
  } else {
    // Anything else can be handled normally.
    Load(expr);
  }
}


ArgumentsAllocationMode CodeGenerator::ArgumentsMode() {
  if (scope()->arguments() == NULL) return NO_ARGUMENTS_ALLOCATION;
  ASSERT(scope()->arguments_shadow() != NULL);
  // We don't want to do lazy arguments allocation for functions that
  // have heap-allocated contexts, because it interfers with the
  // uninitialized const tracking in the context objects.
  return (scope()->num_heap_slots() > 0)
      ? EAGER_ARGUMENTS_ALLOCATION
      : LAZY_ARGUMENTS_ALLOCATION;
}


Result CodeGenerator::StoreArgumentsObject(bool initial) {
  ArgumentsAllocationMode mode = ArgumentsMode();
  ASSERT(mode != NO_ARGUMENTS_ALLOCATION);

  Comment cmnt(masm_, "[ store arguments object");
  if (mode == LAZY_ARGUMENTS_ALLOCATION && initial) {
    // When using lazy arguments allocation, we store the hole value
    // as a sentinel indicating that the arguments object hasn't been
    // allocated yet.
    frame_->Push(Factory::the_hole_value());
  } else {
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    frame_->PushFunction();
    frame_->PushReceiverSlotAddress();
    frame_->Push(Smi::FromInt(scope()->num_parameters()));
    Result result = frame_->CallStub(&stub, 3);
    frame_->Push(&result);
  }

  Variable* arguments = scope()->arguments()->var();
  Variable* shadow = scope()->arguments_shadow()->var();
  ASSERT(arguments != NULL && arguments->slot() != NULL);
  ASSERT(shadow != NULL && shadow->slot() != NULL);
  JumpTarget done;
  bool skip_arguments = false;
  if (mode == LAZY_ARGUMENTS_ALLOCATION && !initial) {
    // We have to skip storing into the arguments slot if it has
    // already been written to. This can happen if the a function
    // has a local variable named 'arguments'.
    LoadFromSlot(arguments->slot(), NOT_INSIDE_TYPEOF);
    Result probe = frame_->Pop();
    if (probe.is_constant()) {
      // We have to skip updating the arguments object if it has
      // been assigned a proper value.
      skip_arguments = !probe.handle()->IsTheHole();
    } else {
      __ CompareRoot(probe.reg(), Heap::kTheHoleValueRootIndex);
      probe.Unuse();
      done.Branch(not_equal);
    }
  }
  if (!skip_arguments) {
    StoreToSlot(arguments->slot(), NOT_CONST_INIT);
    if (mode == LAZY_ARGUMENTS_ALLOCATION) done.Bind();
  }
  StoreToSlot(shadow->slot(), NOT_CONST_INIT);
  return frame_->Pop();
}

//------------------------------------------------------------------------------
// CodeGenerator implementation of variables, lookups, and stores.

Reference::Reference(CodeGenerator* cgen,
                     Expression* expression,
                     bool  persist_after_get)
    : cgen_(cgen),
      expression_(expression),
      type_(ILLEGAL),
      persist_after_get_(persist_after_get) {
  cgen->LoadReference(this);
}


Reference::~Reference() {
  ASSERT(is_unloaded() || is_illegal());
}


void CodeGenerator::LoadReference(Reference* ref) {
  // References are loaded from both spilled and unspilled code.  Set the
  // state to unspilled to allow that (and explicitly spill after
  // construction at the construction sites).
  bool was_in_spilled_code = in_spilled_code_;
  in_spilled_code_ = false;

  Comment cmnt(masm_, "[ LoadReference");
  Expression* e = ref->expression();
  Property* property = e->AsProperty();
  Variable* var = e->AsVariableProxy()->AsVariable();

  if (property != NULL) {
    // The expression is either a property or a variable proxy that rewrites
    // to a property.
    Load(property->obj());
    if (property->key()->IsPropertyName()) {
      ref->set_type(Reference::NAMED);
    } else {
      Load(property->key());
      ref->set_type(Reference::KEYED);
    }
  } else if (var != NULL) {
    // The expression is a variable proxy that does not rewrite to a
    // property.  Global variables are treated as named property references.
    if (var->is_global()) {
      // If rax is free, the register allocator prefers it.  Thus the code
      // generator will load the global object into rax, which is where
      // LoadIC wants it.  Most uses of Reference call LoadIC directly
      // after the reference is created.
      frame_->Spill(rax);
      LoadGlobal();
      ref->set_type(Reference::NAMED);
    } else {
      ASSERT(var->slot() != NULL);
      ref->set_type(Reference::SLOT);
    }
  } else {
    // Anything else is a runtime error.
    Load(e);
    frame_->CallRuntime(Runtime::kThrowReferenceError, 1);
  }

  in_spilled_code_ = was_in_spilled_code;
}


void CodeGenerator::UnloadReference(Reference* ref) {
  // Pop a reference from the stack while preserving TOS.
  Comment cmnt(masm_, "[ UnloadReference");
  frame_->Nip(ref->size());
  ref->set_unloaded();
}


// ECMA-262, section 9.2, page 30: ToBoolean(). Pop the top of stack and
// convert it to a boolean in the condition code register or jump to
// 'false_target'/'true_target' as appropriate.
void CodeGenerator::ToBoolean(ControlDestination* dest) {
  Comment cmnt(masm_, "[ ToBoolean");

  // The value to convert should be popped from the frame.
  Result value = frame_->Pop();
  value.ToRegister();

  if (value.is_number()) {
    // Fast case if TypeInfo indicates only numbers.
    if (FLAG_debug_code) {
      __ AbortIfNotNumber(value.reg());
    }
    // Smi => false iff zero.
    __ SmiCompare(value.reg(), Smi::FromInt(0));
    if (value.is_smi()) {
      value.Unuse();
      dest->Split(not_zero);
    } else {
      dest->false_target()->Branch(equal);
      Condition is_smi = masm_->CheckSmi(value.reg());
      dest->true_target()->Branch(is_smi);
      __ xorpd(xmm0, xmm0);
      __ ucomisd(xmm0, FieldOperand(value.reg(), HeapNumber::kValueOffset));
      value.Unuse();
      dest->Split(not_zero);
    }
  } else {
    // Fast case checks.
    // 'false' => false.
    __ CompareRoot(value.reg(), Heap::kFalseValueRootIndex);
    dest->false_target()->Branch(equal);

    // 'true' => true.
    __ CompareRoot(value.reg(), Heap::kTrueValueRootIndex);
    dest->true_target()->Branch(equal);

    // 'undefined' => false.
    __ CompareRoot(value.reg(), Heap::kUndefinedValueRootIndex);
    dest->false_target()->Branch(equal);

    // Smi => false iff zero.
    __ SmiCompare(value.reg(), Smi::FromInt(0));
    dest->false_target()->Branch(equal);
    Condition is_smi = masm_->CheckSmi(value.reg());
    dest->true_target()->Branch(is_smi);

    // Call the stub for all other cases.
    frame_->Push(&value);  // Undo the Pop() from above.
    ToBooleanStub stub;
    Result temp = frame_->CallStub(&stub, 1);
    // Convert the result to a condition code.
    __ testq(temp.reg(), temp.reg());
    temp.Unuse();
    dest->Split(not_equal);
  }
}


// Call the specialized stub for a binary operation.
class DeferredInlineBinaryOperation: public DeferredCode {
 public:
  DeferredInlineBinaryOperation(Token::Value op,
                                Register dst,
                                Register left,
                                Register right,
                                OverwriteMode mode)
      : op_(op), dst_(dst), left_(left), right_(right), mode_(mode) {
    set_comment("[ DeferredInlineBinaryOperation");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  Register dst_;
  Register left_;
  Register right_;
  OverwriteMode mode_;
};


void DeferredInlineBinaryOperation::Generate() {
  Label done;
  if ((op_ == Token::ADD)
      || (op_ == Token::SUB)
      || (op_ == Token::MUL)
      || (op_ == Token::DIV)) {
    Label call_runtime;
    Label left_smi, right_smi, load_right, do_op;
    __ JumpIfSmi(left_, &left_smi);
    __ CompareRoot(FieldOperand(left_, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);
    __ movsd(xmm0, FieldOperand(left_, HeapNumber::kValueOffset));
    if (mode_ == OVERWRITE_LEFT) {
      __ movq(dst_, left_);
    }
    __ jmp(&load_right);

    __ bind(&left_smi);
    __ SmiToInteger32(left_, left_);
    __ cvtlsi2sd(xmm0, left_);
    __ Integer32ToSmi(left_, left_);
    if (mode_ == OVERWRITE_LEFT) {
      Label alloc_failure;
      __ AllocateHeapNumber(dst_, no_reg, &call_runtime);
    }

    __ bind(&load_right);
    __ JumpIfSmi(right_, &right_smi);
    __ CompareRoot(FieldOperand(right_, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);
    __ movsd(xmm1, FieldOperand(right_, HeapNumber::kValueOffset));
    if (mode_ == OVERWRITE_RIGHT) {
      __ movq(dst_, right_);
    } else if (mode_ == NO_OVERWRITE) {
      Label alloc_failure;
      __ AllocateHeapNumber(dst_, no_reg, &call_runtime);
    }
    __ jmp(&do_op);

    __ bind(&right_smi);
    __ SmiToInteger32(right_, right_);
    __ cvtlsi2sd(xmm1, right_);
    __ Integer32ToSmi(right_, right_);
    if (mode_ == OVERWRITE_RIGHT || mode_ == NO_OVERWRITE) {
      Label alloc_failure;
      __ AllocateHeapNumber(dst_, no_reg, &call_runtime);
    }

    __ bind(&do_op);
    switch (op_) {
      case Token::ADD: __ addsd(xmm0, xmm1); break;
      case Token::SUB: __ subsd(xmm0, xmm1); break;
      case Token::MUL: __ mulsd(xmm0, xmm1); break;
      case Token::DIV: __ divsd(xmm0, xmm1); break;
      default: UNREACHABLE();
    }
    __ movsd(FieldOperand(dst_, HeapNumber::kValueOffset), xmm0);
    __ jmp(&done);

    __ bind(&call_runtime);
  }
  GenericBinaryOpStub stub(op_, mode_, NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, left_, right_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
  __ bind(&done);
}


static TypeInfo CalculateTypeInfo(TypeInfo operands_type,
                                  Token::Value op,
                                  const Result& right,
                                  const Result& left) {
  // Set TypeInfo of result according to the operation performed.
  // We rely on the fact that smis have a 32 bit payload on x64.
  STATIC_ASSERT(kSmiValueSize == 32);
  switch (op) {
    case Token::COMMA:
      return right.type_info();
    case Token::OR:
    case Token::AND:
      // Result type can be either of the two input types.
      return operands_type;
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
      // Result is always a smi.
      return TypeInfo::Smi();
    case Token::SAR:
    case Token::SHL:
      // Result is always a smi.
      return TypeInfo::Smi();
    case Token::SHR:
      // Result of x >>> y is always a smi if masked y >= 1, otherwise a number.
      return (right.is_constant() && right.handle()->IsSmi()
                     && (Smi::cast(*right.handle())->value() & 0x1F) >= 1)
          ? TypeInfo::Smi()
          : TypeInfo::Number();
    case Token::ADD:
      if (operands_type.IsNumber()) {
        return TypeInfo::Number();
      } else if (left.type_info().IsString() || right.type_info().IsString()) {
        return TypeInfo::String();
      } else {
        return TypeInfo::Unknown();
      }
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      // Result is always a number.
      return TypeInfo::Number();
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return TypeInfo::Unknown();
}


void CodeGenerator::GenericBinaryOperation(BinaryOperation* expr,
                                           OverwriteMode overwrite_mode) {
  Comment cmnt(masm_, "[ BinaryOperation");
  Token::Value op = expr->op();
  Comment cmnt_token(masm_, Token::String(op));

  if (op == Token::COMMA) {
    // Simply discard left value.
    frame_->Nip(1);
    return;
  }

  Result right = frame_->Pop();
  Result left = frame_->Pop();

  if (op == Token::ADD) {
    const bool left_is_string = left.type_info().IsString();
    const bool right_is_string = right.type_info().IsString();
    // Make sure constant strings have string type info.
    ASSERT(!(left.is_constant() && left.handle()->IsString()) ||
           left_is_string);
    ASSERT(!(right.is_constant() && right.handle()->IsString()) ||
           right_is_string);
    if (left_is_string || right_is_string) {
      frame_->Push(&left);
      frame_->Push(&right);
      Result answer;
      if (left_is_string) {
        if (right_is_string) {
          StringAddStub stub(NO_STRING_CHECK_IN_STUB);
          answer = frame_->CallStub(&stub, 2);
        } else {
          answer =
            frame_->InvokeBuiltin(Builtins::STRING_ADD_LEFT, CALL_FUNCTION, 2);
        }
      } else if (right_is_string) {
        answer =
          frame_->InvokeBuiltin(Builtins::STRING_ADD_RIGHT, CALL_FUNCTION, 2);
      }
      answer.set_type_info(TypeInfo::String());
      frame_->Push(&answer);
      return;
    }
    // Neither operand is known to be a string.
  }

  bool left_is_smi_constant = left.is_constant() && left.handle()->IsSmi();
  bool left_is_non_smi_constant = left.is_constant() && !left.handle()->IsSmi();
  bool right_is_smi_constant = right.is_constant() && right.handle()->IsSmi();
  bool right_is_non_smi_constant =
      right.is_constant() && !right.handle()->IsSmi();

  if (left_is_smi_constant && right_is_smi_constant) {
    // Compute the constant result at compile time, and leave it on the frame.
    int left_int = Smi::cast(*left.handle())->value();
    int right_int = Smi::cast(*right.handle())->value();
    if (FoldConstantSmis(op, left_int, right_int)) return;
  }

  // Get number type of left and right sub-expressions.
  TypeInfo operands_type =
      TypeInfo::Combine(left.type_info(), right.type_info());

  TypeInfo result_type = CalculateTypeInfo(operands_type, op, right, left);

  Result answer;
  if (left_is_non_smi_constant || right_is_non_smi_constant) {
    // Go straight to the slow case, with no smi code.
    GenericBinaryOpStub stub(op,
                             overwrite_mode,
                             NO_SMI_CODE_IN_STUB,
                             operands_type);
    answer = GenerateGenericBinaryOpStubCall(&stub, &left, &right);
  } else if (right_is_smi_constant) {
    answer = ConstantSmiBinaryOperation(expr, &left, right.handle(),
                                        false, overwrite_mode);
  } else if (left_is_smi_constant) {
    answer = ConstantSmiBinaryOperation(expr, &right, left.handle(),
                                        true, overwrite_mode);
  } else {
    // Set the flags based on the operation, type and loop nesting level.
    // Bit operations always assume they likely operate on Smis. Still only
    // generate the inline Smi check code if this operation is part of a loop.
    // For all other operations only inline the Smi check code for likely smis
    // if the operation is part of a loop.
    if (loop_nesting() > 0 &&
        (Token::IsBitOp(op) ||
         operands_type.IsInteger32() ||
         expr->type()->IsLikelySmi())) {
      answer = LikelySmiBinaryOperation(expr, &left, &right, overwrite_mode);
    } else {
      GenericBinaryOpStub stub(op,
                               overwrite_mode,
                               NO_GENERIC_BINARY_FLAGS,
                               operands_type);
      answer = GenerateGenericBinaryOpStubCall(&stub, &left, &right);
    }
  }

  answer.set_type_info(result_type);
  frame_->Push(&answer);
}


bool CodeGenerator::FoldConstantSmis(Token::Value op, int left, int right) {
  Object* answer_object = Heap::undefined_value();
  switch (op) {
    case Token::ADD:
      // Use intptr_t to detect overflow of 32-bit int.
      if (Smi::IsValid(static_cast<intptr_t>(left) + right)) {
        answer_object = Smi::FromInt(left + right);
      }
      break;
    case Token::SUB:
      // Use intptr_t to detect overflow of 32-bit int.
      if (Smi::IsValid(static_cast<intptr_t>(left) - right)) {
        answer_object = Smi::FromInt(left - right);
      }
      break;
    case Token::MUL: {
        double answer = static_cast<double>(left) * right;
        if (answer >= Smi::kMinValue && answer <= Smi::kMaxValue) {
          // If the product is zero and the non-zero factor is negative,
          // the spec requires us to return floating point negative zero.
          if (answer != 0 || (left >= 0 && right >= 0)) {
            answer_object = Smi::FromInt(static_cast<int>(answer));
          }
        }
      }
      break;
    case Token::DIV:
    case Token::MOD:
      break;
    case Token::BIT_OR:
      answer_object = Smi::FromInt(left | right);
      break;
    case Token::BIT_AND:
      answer_object = Smi::FromInt(left & right);
      break;
    case Token::BIT_XOR:
      answer_object = Smi::FromInt(left ^ right);
      break;

    case Token::SHL: {
        int shift_amount = right & 0x1F;
        if (Smi::IsValid(left << shift_amount)) {
          answer_object = Smi::FromInt(left << shift_amount);
        }
        break;
      }
    case Token::SHR: {
        int shift_amount = right & 0x1F;
        unsigned int unsigned_left = left;
        unsigned_left >>= shift_amount;
        if (unsigned_left <= static_cast<unsigned int>(Smi::kMaxValue)) {
          answer_object = Smi::FromInt(unsigned_left);
        }
        break;
      }
    case Token::SAR: {
        int shift_amount = right & 0x1F;
        unsigned int unsigned_left = left;
        if (left < 0) {
          // Perform arithmetic shift of a negative number by
          // complementing number, logical shifting, complementing again.
          unsigned_left = ~unsigned_left;
          unsigned_left >>= shift_amount;
          unsigned_left = ~unsigned_left;
        } else {
          unsigned_left >>= shift_amount;
        }
        ASSERT(Smi::IsValid(static_cast<int32_t>(unsigned_left)));
        answer_object = Smi::FromInt(static_cast<int32_t>(unsigned_left));
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
  if (answer_object == Heap::undefined_value()) {
    return false;
  }
  frame_->Push(Handle<Object>(answer_object));
  return true;
}


void CodeGenerator::JumpIfBothSmiUsingTypeInfo(Result* left,
                                               Result* right,
                                               JumpTarget* both_smi) {
  TypeInfo left_info = left->type_info();
  TypeInfo right_info = right->type_info();
  if (left_info.IsDouble() || left_info.IsString() ||
      right_info.IsDouble() || right_info.IsString()) {
    // We know that left and right are not both smi.  Don't do any tests.
    return;
  }

  if (left->reg().is(right->reg())) {
    if (!left_info.IsSmi()) {
      Condition is_smi = masm()->CheckSmi(left->reg());
      both_smi->Branch(is_smi);
    } else {
      if (FLAG_debug_code) __ AbortIfNotSmi(left->reg());
      left->Unuse();
      right->Unuse();
      both_smi->Jump();
    }
  } else if (!left_info.IsSmi()) {
    if (!right_info.IsSmi()) {
      Condition is_smi = masm()->CheckBothSmi(left->reg(), right->reg());
      both_smi->Branch(is_smi);
    } else {
      Condition is_smi = masm()->CheckSmi(left->reg());
      both_smi->Branch(is_smi);
    }
  } else {
    if (FLAG_debug_code) __ AbortIfNotSmi(left->reg());
    if (!right_info.IsSmi()) {
      Condition is_smi = masm()->CheckSmi(right->reg());
      both_smi->Branch(is_smi);
    } else {
      if (FLAG_debug_code) __ AbortIfNotSmi(right->reg());
      left->Unuse();
      right->Unuse();
      both_smi->Jump();
    }
  }
}


void CodeGenerator::JumpIfNotSmiUsingTypeInfo(Register reg,
                                              TypeInfo type,
                                              DeferredCode* deferred) {
  if (!type.IsSmi()) {
        __ JumpIfNotSmi(reg, deferred->entry_label());
  }
  if (FLAG_debug_code) {
    __ AbortIfNotSmi(reg);
  }
}


void CodeGenerator::JumpIfNotBothSmiUsingTypeInfo(Register left,
                                                  Register right,
                                                  TypeInfo left_info,
                                                  TypeInfo right_info,
                                                  DeferredCode* deferred) {
  if (!left_info.IsSmi() && !right_info.IsSmi()) {
    __ JumpIfNotBothSmi(left, right, deferred->entry_label());
  } else if (!left_info.IsSmi()) {
    __ JumpIfNotSmi(left, deferred->entry_label());
  } else if (!right_info.IsSmi()) {
    __ JumpIfNotSmi(right, deferred->entry_label());
  }
  if (FLAG_debug_code) {
    __ AbortIfNotSmi(left);
    __ AbortIfNotSmi(right);
  }
}


// Implements a binary operation using a deferred code object and some
// inline code to operate on smis quickly.
Result CodeGenerator::LikelySmiBinaryOperation(BinaryOperation* expr,
                                               Result* left,
                                               Result* right,
                                               OverwriteMode overwrite_mode) {
  // Copy the type info because left and right may be overwritten.
  TypeInfo left_type_info = left->type_info();
  TypeInfo right_type_info = right->type_info();
  Token::Value op = expr->op();
  Result answer;
  // Special handling of div and mod because they use fixed registers.
  if (op == Token::DIV || op == Token::MOD) {
    // We need rax as the quotient register, rdx as the remainder
    // register, neither left nor right in rax or rdx, and left copied
    // to rax.
    Result quotient;
    Result remainder;
    bool left_is_in_rax = false;
    // Step 1: get rax for quotient.
    if ((left->is_register() && left->reg().is(rax)) ||
        (right->is_register() && right->reg().is(rax))) {
      // One or both is in rax.  Use a fresh non-rdx register for
      // them.
      Result fresh = allocator_->Allocate();
      ASSERT(fresh.is_valid());
      if (fresh.reg().is(rdx)) {
        remainder = fresh;
        fresh = allocator_->Allocate();
        ASSERT(fresh.is_valid());
      }
      if (left->is_register() && left->reg().is(rax)) {
        quotient = *left;
        *left = fresh;
        left_is_in_rax = true;
      }
      if (right->is_register() && right->reg().is(rax)) {
        quotient = *right;
        *right = fresh;
      }
      __ movq(fresh.reg(), rax);
    } else {
      // Neither left nor right is in rax.
      quotient = allocator_->Allocate(rax);
    }
    ASSERT(quotient.is_register() && quotient.reg().is(rax));
    ASSERT(!(left->is_register() && left->reg().is(rax)));
    ASSERT(!(right->is_register() && right->reg().is(rax)));

    // Step 2: get rdx for remainder if necessary.
    if (!remainder.is_valid()) {
      if ((left->is_register() && left->reg().is(rdx)) ||
          (right->is_register() && right->reg().is(rdx))) {
        Result fresh = allocator_->Allocate();
        ASSERT(fresh.is_valid());
        if (left->is_register() && left->reg().is(rdx)) {
          remainder = *left;
          *left = fresh;
        }
        if (right->is_register() && right->reg().is(rdx)) {
          remainder = *right;
          *right = fresh;
        }
        __ movq(fresh.reg(), rdx);
      } else {
        // Neither left nor right is in rdx.
        remainder = allocator_->Allocate(rdx);
      }
    }
    ASSERT(remainder.is_register() && remainder.reg().is(rdx));
    ASSERT(!(left->is_register() && left->reg().is(rdx)));
    ASSERT(!(right->is_register() && right->reg().is(rdx)));

    left->ToRegister();
    right->ToRegister();
    frame_->Spill(rax);
    frame_->Spill(rdx);

    // Check that left and right are smi tagged.
    DeferredInlineBinaryOperation* deferred =
        new DeferredInlineBinaryOperation(op,
                                          (op == Token::DIV) ? rax : rdx,
                                          left->reg(),
                                          right->reg(),
                                          overwrite_mode);
    JumpIfNotBothSmiUsingTypeInfo(left->reg(), right->reg(),
                                  left_type_info, right_type_info, deferred);

    if (op == Token::DIV) {
      __ SmiDiv(rax, left->reg(), right->reg(), deferred->entry_label());
      deferred->BindExit();
      left->Unuse();
      right->Unuse();
      answer = quotient;
    } else {
      ASSERT(op == Token::MOD);
      __ SmiMod(rdx, left->reg(), right->reg(), deferred->entry_label());
      deferred->BindExit();
      left->Unuse();
      right->Unuse();
      answer = remainder;
    }
    ASSERT(answer.is_valid());
    return answer;
  }

  // Special handling of shift operations because they use fixed
  // registers.
  if (op == Token::SHL || op == Token::SHR || op == Token::SAR) {
    // Move left out of rcx if necessary.
    if (left->is_register() && left->reg().is(rcx)) {
      *left = allocator_->Allocate();
      ASSERT(left->is_valid());
      __ movq(left->reg(), rcx);
    }
    right->ToRegister(rcx);
    left->ToRegister();
    ASSERT(left->is_register() && !left->reg().is(rcx));
    ASSERT(right->is_register() && right->reg().is(rcx));

    // We will modify right, it must be spilled.
    frame_->Spill(rcx);

    // Use a fresh answer register to avoid spilling the left operand.
    answer = allocator_->Allocate();
    ASSERT(answer.is_valid());
    // Check that both operands are smis using the answer register as a
    // temporary.
    DeferredInlineBinaryOperation* deferred =
        new DeferredInlineBinaryOperation(op,
                                          answer.reg(),
                                          left->reg(),
                                          rcx,
                                          overwrite_mode);

    Label do_op;
    if (right_type_info.IsSmi()) {
      if (FLAG_debug_code) {
        __ AbortIfNotSmi(right->reg());
      }
      __ movq(answer.reg(), left->reg());
      // If left is not known to be a smi, check if it is.
      // If left is not known to be a number, and it isn't a smi, check if
      // it is a HeapNumber.
      if (!left_type_info.IsSmi()) {
        __ JumpIfSmi(answer.reg(), &do_op);
        if (!left_type_info.IsNumber()) {
          // Branch if not a heapnumber.
          __ Cmp(FieldOperand(answer.reg(), HeapObject::kMapOffset),
                 Factory::heap_number_map());
          deferred->Branch(not_equal);
        }
        // Load integer value into answer register using truncation.
        __ cvttsd2si(answer.reg(),
                     FieldOperand(answer.reg(), HeapNumber::kValueOffset));
        // Branch if we might have overflowed.
        // (False negative for Smi::kMinValue)
        __ cmpq(answer.reg(), Immediate(0x80000000));
        deferred->Branch(equal);
        // TODO(lrn): Inline shifts on int32 here instead of first smi-tagging.
        __ Integer32ToSmi(answer.reg(), answer.reg());
      } else {
        // Fast case - both are actually smis.
        if (FLAG_debug_code) {
          __ AbortIfNotSmi(left->reg());
        }
      }
    } else {
      JumpIfNotBothSmiUsingTypeInfo(left->reg(), rcx,
                                    left_type_info, right_type_info, deferred);
    }
    __ bind(&do_op);

    // Perform the operation.
    switch (op) {
      case Token::SAR:
        __ SmiShiftArithmeticRight(answer.reg(), left->reg(), rcx);
        break;
      case Token::SHR: {
        __ SmiShiftLogicalRight(answer.reg(),
                              left->reg(),
                              rcx,
                              deferred->entry_label());
        break;
      }
      case Token::SHL: {
        __ SmiShiftLeft(answer.reg(),
                        left->reg(),
                        rcx);
        break;
      }
      default:
        UNREACHABLE();
    }
    deferred->BindExit();
    left->Unuse();
    right->Unuse();
    ASSERT(answer.is_valid());
    return answer;
  }

  // Handle the other binary operations.
  left->ToRegister();
  right->ToRegister();
  // A newly allocated register answer is used to hold the answer.  The
  // registers containing left and right are not modified so they don't
  // need to be spilled in the fast case.
  answer = allocator_->Allocate();
  ASSERT(answer.is_valid());

  // Perform the smi tag check.
  DeferredInlineBinaryOperation* deferred =
      new DeferredInlineBinaryOperation(op,
                                        answer.reg(),
                                        left->reg(),
                                        right->reg(),
                                        overwrite_mode);
  JumpIfNotBothSmiUsingTypeInfo(left->reg(), right->reg(),
                                left_type_info, right_type_info, deferred);

  switch (op) {
    case Token::ADD:
      __ SmiAdd(answer.reg(),
                left->reg(),
                right->reg(),
                deferred->entry_label());
      break;

    case Token::SUB:
      __ SmiSub(answer.reg(),
                left->reg(),
                right->reg(),
                deferred->entry_label());
      break;

    case Token::MUL: {
      __ SmiMul(answer.reg(),
                left->reg(),
                right->reg(),
                deferred->entry_label());
      break;
    }

    case Token::BIT_OR:
      __ SmiOr(answer.reg(), left->reg(), right->reg());
      break;

    case Token::BIT_AND:
      __ SmiAnd(answer.reg(), left->reg(), right->reg());
      break;

    case Token::BIT_XOR:
      __ SmiXor(answer.reg(), left->reg(), right->reg());
      break;

    default:
      UNREACHABLE();
      break;
  }
  deferred->BindExit();
  left->Unuse();
  right->Unuse();
  ASSERT(answer.is_valid());
  return answer;
}


// Call the appropriate binary operation stub to compute src op value
// and leave the result in dst.
class DeferredInlineSmiOperation: public DeferredCode {
 public:
  DeferredInlineSmiOperation(Token::Value op,
                             Register dst,
                             Register src,
                             Smi* value,
                             OverwriteMode overwrite_mode)
      : op_(op),
        dst_(dst),
        src_(src),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiOperation");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  Register dst_;
  Register src_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiOperation::Generate() {
  // For mod we don't generate all the Smi code inline.
  GenericBinaryOpStub stub(
      op_,
      overwrite_mode_,
      (op_ == Token::MOD) ? NO_GENERIC_BINARY_FLAGS : NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, src_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


// Call the appropriate binary operation stub to compute value op src
// and leave the result in dst.
class DeferredInlineSmiOperationReversed: public DeferredCode {
 public:
  DeferredInlineSmiOperationReversed(Token::Value op,
                                     Register dst,
                                     Smi* value,
                                     Register src,
                                     OverwriteMode overwrite_mode)
      : op_(op),
        dst_(dst),
        value_(value),
        src_(src),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiOperationReversed");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  Register dst_;
  Smi* value_;
  Register src_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiOperationReversed::Generate() {
  GenericBinaryOpStub stub(
      op_,
      overwrite_mode_,
      NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, value_, src_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}
class DeferredInlineSmiAdd: public DeferredCode {
 public:
  DeferredInlineSmiAdd(Register dst,
                       Smi* value,
                       OverwriteMode overwrite_mode)
      : dst_(dst), value_(value), overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiAdd");
  }

  virtual void Generate();

 private:
  Register dst_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiAdd::Generate() {
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


// The result of value + src is in dst.  It either overflowed or was not
// smi tagged.  Undo the speculative addition and call the appropriate
// specialized stub for add.  The result is left in dst.
class DeferredInlineSmiAddReversed: public DeferredCode {
 public:
  DeferredInlineSmiAddReversed(Register dst,
                               Smi* value,
                               OverwriteMode overwrite_mode)
      : dst_(dst), value_(value), overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiAddReversed");
  }

  virtual void Generate();

 private:
  Register dst_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiAddReversed::Generate() {
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, value_, dst_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


class DeferredInlineSmiSub: public DeferredCode {
 public:
  DeferredInlineSmiSub(Register dst,
                       Smi* value,
                       OverwriteMode overwrite_mode)
      : dst_(dst), value_(value), overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiSub");
  }

  virtual void Generate();

 private:
  Register dst_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiSub::Generate() {
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


Result CodeGenerator::ConstantSmiBinaryOperation(BinaryOperation* expr,
                                                 Result* operand,
                                                 Handle<Object> value,
                                                 bool reversed,
                                                 OverwriteMode overwrite_mode) {
  // Generate inline code for a binary operation when one of the
  // operands is a constant smi.  Consumes the argument "operand".
  if (IsUnsafeSmi(value)) {
    Result unsafe_operand(value);
    if (reversed) {
      return LikelySmiBinaryOperation(expr, &unsafe_operand, operand,
                               overwrite_mode);
    } else {
      return LikelySmiBinaryOperation(expr, operand, &unsafe_operand,
                               overwrite_mode);
    }
  }

  // Get the literal value.
  Smi* smi_value = Smi::cast(*value);
  int int_value = smi_value->value();

  Token::Value op = expr->op();
  Result answer;
  switch (op) {
    case Token::ADD: {
      operand->ToRegister();
      frame_->Spill(operand->reg());
      DeferredCode* deferred = NULL;
      if (reversed) {
        deferred = new DeferredInlineSmiAddReversed(operand->reg(),
                                                    smi_value,
                                                    overwrite_mode);
      } else {
        deferred = new DeferredInlineSmiAdd(operand->reg(),
                                            smi_value,
                                            overwrite_mode);
      }
      JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                deferred);
      __ SmiAddConstant(operand->reg(),
                        operand->reg(),
                        smi_value,
                        deferred->entry_label());
      deferred->BindExit();
      answer = *operand;
      break;
    }

    case Token::SUB: {
      if (reversed) {
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(expr, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        answer = *operand;
        DeferredCode* deferred = new DeferredInlineSmiSub(operand->reg(),
                                                          smi_value,
                                                          overwrite_mode);
        JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                  deferred);
        // A smi currently fits in a 32-bit Immediate.
        __ SmiSubConstant(operand->reg(),
                          operand->reg(),
                          smi_value,
                          deferred->entry_label());
        deferred->BindExit();
        operand->Unuse();
      }
      break;
    }

    case Token::SAR:
      if (reversed) {
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(expr, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        operand->ToRegister();
        frame_->Spill(operand->reg());
        DeferredInlineSmiOperation* deferred =
            new DeferredInlineSmiOperation(op,
                                           operand->reg(),
                                           operand->reg(),
                                           smi_value,
                                           overwrite_mode);
        JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                  deferred);
        __ SmiShiftArithmeticRightConstant(operand->reg(),
                                           operand->reg(),
                                           shift_value);
        deferred->BindExit();
        answer = *operand;
      }
      break;

    case Token::SHR:
      if (reversed) {
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(expr, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        operand->ToRegister();
        answer = allocator()->Allocate();
        ASSERT(answer.is_valid());
        DeferredInlineSmiOperation* deferred =
            new DeferredInlineSmiOperation(op,
                                           answer.reg(),
                                           operand->reg(),
                                           smi_value,
                                           overwrite_mode);
        JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                  deferred);
        __ SmiShiftLogicalRightConstant(answer.reg(),
                                        operand->reg(),
                                        shift_value,
                                        deferred->entry_label());
        deferred->BindExit();
        operand->Unuse();
      }
      break;

    case Token::SHL:
      if (reversed) {
        operand->ToRegister();

        // We need rcx to be available to hold operand, and to be spilled.
        // SmiShiftLeft implicitly modifies rcx.
        if (operand->reg().is(rcx)) {
          frame_->Spill(operand->reg());
          answer = allocator()->Allocate();
        } else {
          Result rcx_reg = allocator()->Allocate(rcx);
          // answer must not be rcx.
          answer = allocator()->Allocate();
          // rcx_reg goes out of scope.
        }

        DeferredInlineSmiOperationReversed* deferred =
            new DeferredInlineSmiOperationReversed(op,
                                                   answer.reg(),
                                                   smi_value,
                                                   operand->reg(),
                                                   overwrite_mode);
        JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                  deferred);

        __ Move(answer.reg(), smi_value);
        __ SmiShiftLeft(answer.reg(), answer.reg(), operand->reg());
        operand->Unuse();

        deferred->BindExit();
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        operand->ToRegister();
        if (shift_value == 0) {
          // Spill operand so it can be overwritten in the slow case.
          frame_->Spill(operand->reg());
          DeferredInlineSmiOperation* deferred =
              new DeferredInlineSmiOperation(op,
                                             operand->reg(),
                                             operand->reg(),
                                             smi_value,
                                             overwrite_mode);
          JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                    deferred);
          deferred->BindExit();
          answer = *operand;
        } else {
          // Use a fresh temporary for nonzero shift values.
          answer = allocator()->Allocate();
          ASSERT(answer.is_valid());
          DeferredInlineSmiOperation* deferred =
              new DeferredInlineSmiOperation(op,
                                             answer.reg(),
                                             operand->reg(),
                                             smi_value,
                                             overwrite_mode);
          JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                    deferred);
          __ SmiShiftLeftConstant(answer.reg(),
                                  operand->reg(),
                                  shift_value);
          deferred->BindExit();
          operand->Unuse();
        }
      }
      break;

    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND: {
      operand->ToRegister();
      frame_->Spill(operand->reg());
      if (reversed) {
        // Bit operations with a constant smi are commutative.
        // We can swap left and right operands with no problem.
        // Swap left and right overwrite modes.  0->0, 1->2, 2->1.
        overwrite_mode = static_cast<OverwriteMode>((2 * overwrite_mode) % 3);
      }
      DeferredCode* deferred =  new DeferredInlineSmiOperation(op,
                                                               operand->reg(),
                                                               operand->reg(),
                                                               smi_value,
                                                               overwrite_mode);
      JumpIfNotSmiUsingTypeInfo(operand->reg(), operand->type_info(),
                                deferred);
      if (op == Token::BIT_AND) {
        __ SmiAndConstant(operand->reg(), operand->reg(), smi_value);
      } else if (op == Token::BIT_XOR) {
        if (int_value != 0) {
          __ SmiXorConstant(operand->reg(), operand->reg(), smi_value);
        }
      } else {
        ASSERT(op == Token::BIT_OR);
        if (int_value != 0) {
          __ SmiOrConstant(operand->reg(), operand->reg(), smi_value);
        }
      }
      deferred->BindExit();
      answer = *operand;
      break;
    }

    // Generate inline code for mod of powers of 2 and negative powers of 2.
    case Token::MOD:
      if (!reversed &&
          int_value != 0 &&
          (IsPowerOf2(int_value) || IsPowerOf2(-int_value))) {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        DeferredCode* deferred =
            new DeferredInlineSmiOperation(op,
                                           operand->reg(),
                                           operand->reg(),
                                           smi_value,
                                           overwrite_mode);
        // Check for negative or non-Smi left hand side.
        __ JumpIfNotPositiveSmi(operand->reg(), deferred->entry_label());
        if (int_value < 0) int_value = -int_value;
        if (int_value == 1) {
          __ Move(operand->reg(), Smi::FromInt(0));
        } else {
          __ SmiAndConstant(operand->reg(),
                            operand->reg(),
                            Smi::FromInt(int_value - 1));
        }
        deferred->BindExit();
        answer = *operand;
        break;  // This break only applies if we generated code for MOD.
      }
      // Fall through if we did not find a power of 2 on the right hand side!
      // The next case must be the default.

    default: {
      Result constant_operand(value);
      if (reversed) {
        answer = LikelySmiBinaryOperation(expr, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        answer = LikelySmiBinaryOperation(expr, operand, &constant_operand,
                                          overwrite_mode);
      }
      break;
    }
  }
  ASSERT(answer.is_valid());
  return answer;
}


static bool CouldBeNaN(const Result& result) {
  if (result.type_info().IsSmi()) return false;
  if (result.type_info().IsInteger32()) return false;
  if (!result.is_constant()) return true;
  if (!result.handle()->IsHeapNumber()) return false;
  return isnan(HeapNumber::cast(*result.handle())->value());
}


// Convert from signed to unsigned comparison to match the way EFLAGS are set
// by FPU and XMM compare instructions.
static Condition DoubleCondition(Condition cc) {
  switch (cc) {
    case less:          return below;
    case equal:         return equal;
    case less_equal:    return below_equal;
    case greater:       return above;
    case greater_equal: return above_equal;
    default:            UNREACHABLE();
  }
  UNREACHABLE();
  return equal;
}


void CodeGenerator::Comparison(AstNode* node,
                               Condition cc,
                               bool strict,
                               ControlDestination* dest) {
  // Strict only makes sense for equality comparisons.
  ASSERT(!strict || cc == equal);

  Result left_side;
  Result right_side;
  // Implement '>' and '<=' by reversal to obtain ECMA-262 conversion order.
  if (cc == greater || cc == less_equal) {
    cc = ReverseCondition(cc);
    left_side = frame_->Pop();
    right_side = frame_->Pop();
  } else {
    right_side = frame_->Pop();
    left_side = frame_->Pop();
  }
  ASSERT(cc == less || cc == equal || cc == greater_equal);

  // If either side is a constant smi, optimize the comparison.
  bool left_side_constant_smi = false;
  bool left_side_constant_null = false;
  bool left_side_constant_1_char_string = false;
  if (left_side.is_constant()) {
    left_side_constant_smi = left_side.handle()->IsSmi();
    left_side_constant_null = left_side.handle()->IsNull();
    left_side_constant_1_char_string =
        (left_side.handle()->IsString() &&
         String::cast(*left_side.handle())->length() == 1 &&
         String::cast(*left_side.handle())->IsAsciiRepresentation());
  }
  bool right_side_constant_smi = false;
  bool right_side_constant_null = false;
  bool right_side_constant_1_char_string = false;
  if (right_side.is_constant()) {
    right_side_constant_smi = right_side.handle()->IsSmi();
    right_side_constant_null = right_side.handle()->IsNull();
    right_side_constant_1_char_string =
        (right_side.handle()->IsString() &&
         String::cast(*right_side.handle())->length() == 1 &&
         String::cast(*right_side.handle())->IsAsciiRepresentation());
  }

  if (left_side_constant_smi || right_side_constant_smi) {
    bool is_loop_condition = (node->AsExpression() != NULL) &&
        node->AsExpression()->is_loop_condition();
    ConstantSmiComparison(cc, strict, dest, &left_side, &right_side,
                          left_side_constant_smi, right_side_constant_smi,
                          is_loop_condition);
  } else if (left_side_constant_1_char_string ||
             right_side_constant_1_char_string) {
    if (left_side_constant_1_char_string && right_side_constant_1_char_string) {
      // Trivial case, comparing two constants.
      int left_value = String::cast(*left_side.handle())->Get(0);
      int right_value = String::cast(*right_side.handle())->Get(0);
      switch (cc) {
        case less:
          dest->Goto(left_value < right_value);
          break;
        case equal:
          dest->Goto(left_value == right_value);
          break;
        case greater_equal:
          dest->Goto(left_value >= right_value);
          break;
        default:
          UNREACHABLE();
      }
    } else {
      // Only one side is a constant 1 character string.
      // If left side is a constant 1-character string, reverse the operands.
      // Since one side is a constant string, conversion order does not matter.
      if (left_side_constant_1_char_string) {
        Result temp = left_side;
        left_side = right_side;
        right_side = temp;
        cc = ReverseCondition(cc);
        // This may reintroduce greater or less_equal as the value of cc.
        // CompareStub and the inline code both support all values of cc.
      }
      // Implement comparison against a constant string, inlining the case
      // where both sides are strings.
      left_side.ToRegister();

      // Here we split control flow to the stub call and inlined cases
      // before finally splitting it to the control destination.  We use
      // a jump target and branching to duplicate the virtual frame at
      // the first split.  We manually handle the off-frame references
      // by reconstituting them on the non-fall-through path.
      JumpTarget is_not_string, is_string;
      Register left_reg = left_side.reg();
      Handle<Object> right_val = right_side.handle();
      ASSERT(StringShape(String::cast(*right_val)).IsSymbol());
      Condition is_smi = masm()->CheckSmi(left_reg);
      is_not_string.Branch(is_smi, &left_side);
      Result temp = allocator_->Allocate();
      ASSERT(temp.is_valid());
      __ movq(temp.reg(),
              FieldOperand(left_reg, HeapObject::kMapOffset));
      __ movzxbl(temp.reg(),
                 FieldOperand(temp.reg(), Map::kInstanceTypeOffset));
      // If we are testing for equality then make use of the symbol shortcut.
      // Check if the left hand side has the same type as the right hand
      // side (which is always a symbol).
      if (cc == equal) {
        Label not_a_symbol;
        STATIC_ASSERT(kSymbolTag != 0);
        // Ensure that no non-strings have the symbol bit set.
        STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
        __ testb(temp.reg(), Immediate(kIsSymbolMask));  // Test the symbol bit.
        __ j(zero, &not_a_symbol);
        // They are symbols, so do identity compare.
        __ Cmp(left_reg, right_side.handle());
        dest->true_target()->Branch(equal);
        dest->false_target()->Branch(not_equal);
        __ bind(&not_a_symbol);
      }
      // Call the compare stub if the left side is not a flat ascii string.
      __ andb(temp.reg(),
              Immediate(kIsNotStringMask |
                        kStringRepresentationMask |
                        kStringEncodingMask));
      __ cmpb(temp.reg(),
              Immediate(kStringTag | kSeqStringTag | kAsciiStringTag));
      temp.Unuse();
      is_string.Branch(equal, &left_side);

      // Setup and call the compare stub.
      is_not_string.Bind(&left_side);
      CompareStub stub(cc, strict, kCantBothBeNaN);
      Result result = frame_->CallStub(&stub, &left_side, &right_side);
      result.ToRegister();
      __ testq(result.reg(), result.reg());
      result.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_string.Bind(&left_side);
      // left_side is a sequential ASCII string.
      ASSERT(left_side.reg().is(left_reg));
      right_side = Result(right_val);
      Result temp2 = allocator_->Allocate();
      ASSERT(temp2.is_valid());
      // Test string equality and comparison.
      if (cc == equal) {
        Label comparison_done;
        __ SmiCompare(FieldOperand(left_side.reg(), String::kLengthOffset),
                Smi::FromInt(1));
        __ j(not_equal, &comparison_done);
        uint8_t char_value =
            static_cast<uint8_t>(String::cast(*right_val)->Get(0));
        __ cmpb(FieldOperand(left_side.reg(), SeqAsciiString::kHeaderSize),
                Immediate(char_value));
        __ bind(&comparison_done);
      } else {
        __ movq(temp2.reg(),
                FieldOperand(left_side.reg(), String::kLengthOffset));
        __ SmiSubConstant(temp2.reg(), temp2.reg(), Smi::FromInt(1));
        Label comparison;
        // If the length is 0 then the subtraction gave -1 which compares less
        // than any character.
        __ j(negative, &comparison);
        // Otherwise load the first character.
        __ movzxbl(temp2.reg(),
                   FieldOperand(left_side.reg(), SeqAsciiString::kHeaderSize));
        __ bind(&comparison);
        // Compare the first character of the string with the
        // constant 1-character string.
        uint8_t char_value =
            static_cast<uint8_t>(String::cast(*right_side.handle())->Get(0));
        __ cmpb(temp2.reg(), Immediate(char_value));
        Label characters_were_different;
        __ j(not_equal, &characters_were_different);
        // If the first character is the same then the long string sorts after
        // the short one.
        __ SmiCompare(FieldOperand(left_side.reg(), String::kLengthOffset),
                      Smi::FromInt(1));
        __ bind(&characters_were_different);
      }
      temp2.Unuse();
      left_side.Unuse();
      right_side.Unuse();
      dest->Split(cc);
    }
  } else {
    // Neither side is a constant Smi, constant 1-char string, or constant null.
    // If either side is a non-smi constant, or known to be a heap number,
    // skip the smi check.
    bool known_non_smi =
        (left_side.is_constant() && !left_side.handle()->IsSmi()) ||
        (right_side.is_constant() && !right_side.handle()->IsSmi()) ||
        left_side.type_info().IsDouble() ||
        right_side.type_info().IsDouble();

    NaNInformation nan_info =
        (CouldBeNaN(left_side) && CouldBeNaN(right_side)) ?
        kBothCouldBeNaN :
        kCantBothBeNaN;

    // Inline number comparison handling any combination of smi's and heap
    // numbers if:
    //   code is in a loop
    //   the compare operation is different from equal
    //   compare is not a for-loop comparison
    // The reason for excluding equal is that it will most likely be done
    // with smi's (not heap numbers) and the code to comparing smi's is inlined
    // separately. The same reason applies for for-loop comparison which will
    // also most likely be smi comparisons.
    bool is_loop_condition = (node->AsExpression() != NULL)
        && node->AsExpression()->is_loop_condition();
    bool inline_number_compare =
        loop_nesting() > 0 && cc != equal && !is_loop_condition;

    // Left and right needed in registers for the following code.
    left_side.ToRegister();
    right_side.ToRegister();

    if (known_non_smi) {
      // Inlined equality check:
      // If at least one of the objects is not NaN, then if the objects
      // are identical, they are equal.
      if (nan_info == kCantBothBeNaN && cc == equal) {
        __ cmpq(left_side.reg(), right_side.reg());
        dest->true_target()->Branch(equal);
      }

      // Inlined number comparison:
      if (inline_number_compare) {
        GenerateInlineNumberComparison(&left_side, &right_side, cc, dest);
      }

      // End of in-line compare, call out to the compare stub. Don't include
      // number comparison in the stub if it was inlined.
      CompareStub stub(cc, strict, nan_info, !inline_number_compare);
      Result answer = frame_->CallStub(&stub, &left_side, &right_side);
      __ testq(answer.reg(), answer.reg());  // Sets both zero and sign flag.
      answer.Unuse();
      dest->Split(cc);
    } else {
      // Here we split control flow to the stub call and inlined cases
      // before finally splitting it to the control destination.  We use
      // a jump target and branching to duplicate the virtual frame at
      // the first split.  We manually handle the off-frame references
      // by reconstituting them on the non-fall-through path.
      JumpTarget is_smi;
      Register left_reg = left_side.reg();
      Register right_reg = right_side.reg();

      // In-line check for comparing two smis.
      JumpIfBothSmiUsingTypeInfo(&left_side, &right_side, &is_smi);

      if (has_valid_frame()) {
        // Inline the equality check if both operands can't be a NaN. If both
        // objects are the same they are equal.
        if (nan_info == kCantBothBeNaN && cc == equal) {
          __ cmpq(left_side.reg(), right_side.reg());
          dest->true_target()->Branch(equal);
        }

        // Inlined number comparison:
        if (inline_number_compare) {
          GenerateInlineNumberComparison(&left_side, &right_side, cc, dest);
        }

        // End of in-line compare, call out to the compare stub. Don't include
        // number comparison in the stub if it was inlined.
        CompareStub stub(cc, strict, nan_info, !inline_number_compare);
        Result answer = frame_->CallStub(&stub, &left_side, &right_side);
        __ testq(answer.reg(), answer.reg());  // Sets both zero and sign flags.
        answer.Unuse();
        if (is_smi.is_linked()) {
          dest->true_target()->Branch(cc);
          dest->false_target()->Jump();
        } else {
          dest->Split(cc);
        }
      }

      if (is_smi.is_linked()) {
        is_smi.Bind();
        left_side = Result(left_reg);
        right_side = Result(right_reg);
        __ SmiCompare(left_side.reg(), right_side.reg());
        right_side.Unuse();
        left_side.Unuse();
        dest->Split(cc);
      }
    }
  }
}


void CodeGenerator::ConstantSmiComparison(Condition cc,
                                          bool strict,
                                          ControlDestination* dest,
                                          Result* left_side,
                                          Result* right_side,
                                          bool left_side_constant_smi,
                                          bool right_side_constant_smi,
                                          bool is_loop_condition) {
  if (left_side_constant_smi && right_side_constant_smi) {
    // Trivial case, comparing two constants.
    int left_value = Smi::cast(*left_side->handle())->value();
    int right_value = Smi::cast(*right_side->handle())->value();
    switch (cc) {
      case less:
        dest->Goto(left_value < right_value);
        break;
      case equal:
        dest->Goto(left_value == right_value);
        break;
      case greater_equal:
        dest->Goto(left_value >= right_value);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // Only one side is a constant Smi.
    // If left side is a constant Smi, reverse the operands.
    // Since one side is a constant Smi, conversion order does not matter.
    if (left_side_constant_smi) {
      Result* temp = left_side;
      left_side = right_side;
      right_side = temp;
      cc = ReverseCondition(cc);
      // This may re-introduce greater or less_equal as the value of cc.
      // CompareStub and the inline code both support all values of cc.
    }
    // Implement comparison against a constant Smi, inlining the case
    // where both sides are Smis.
    left_side->ToRegister();
    Register left_reg = left_side->reg();
    Smi* constant_smi = Smi::cast(*right_side->handle());

    if (left_side->is_smi()) {
      if (FLAG_debug_code) {
        __ AbortIfNotSmi(left_reg);
      }
      // Test smi equality and comparison by signed int comparison.
      // Both sides are smis, so we can use an Immediate.
      __ SmiCompare(left_reg, constant_smi);
      left_side->Unuse();
      right_side->Unuse();
      dest->Split(cc);
    } else {
      // Only the case where the left side could possibly be a non-smi is left.
      JumpTarget is_smi;
      if (cc == equal) {
        // We can do the equality comparison before the smi check.
        __ SmiCompare(left_reg, constant_smi);
        dest->true_target()->Branch(equal);
        Condition left_is_smi = masm_->CheckSmi(left_reg);
        dest->false_target()->Branch(left_is_smi);
      } else {
        // Do the smi check, then the comparison.
        Condition left_is_smi = masm_->CheckSmi(left_reg);
        is_smi.Branch(left_is_smi, left_side, right_side);
      }

      // Jump or fall through to here if we are comparing a non-smi to a
      // constant smi.  If the non-smi is a heap number and this is not
      // a loop condition, inline the floating point code.
      if (!is_loop_condition) {
        // Right side is a constant smi and left side has been checked
        // not to be a smi.
        JumpTarget not_number;
        __ Cmp(FieldOperand(left_reg, HeapObject::kMapOffset),
               Factory::heap_number_map());
        not_number.Branch(not_equal, left_side);
        __ movsd(xmm1,
                 FieldOperand(left_reg, HeapNumber::kValueOffset));
        int value = constant_smi->value();
        if (value == 0) {
          __ xorpd(xmm0, xmm0);
        } else {
          Result temp = allocator()->Allocate();
          __ movl(temp.reg(), Immediate(value));
          __ cvtlsi2sd(xmm0, temp.reg());
          temp.Unuse();
        }
        __ ucomisd(xmm1, xmm0);
        // Jump to builtin for NaN.
        not_number.Branch(parity_even, left_side);
        left_side->Unuse();
        dest->true_target()->Branch(DoubleCondition(cc));
        dest->false_target()->Jump();
        not_number.Bind(left_side);
      }

      // Setup and call the compare stub.
      CompareStub stub(cc, strict, kCantBothBeNaN);
      Result result = frame_->CallStub(&stub, left_side, right_side);
      result.ToRegister();
      __ testq(result.reg(), result.reg());
      result.Unuse();
      if (cc == equal) {
        dest->Split(cc);
      } else {
        dest->true_target()->Branch(cc);
        dest->false_target()->Jump();

        // It is important for performance for this case to be at the end.
        is_smi.Bind(left_side, right_side);
        __ SmiCompare(left_reg, constant_smi);
        left_side->Unuse();
        right_side->Unuse();
        dest->Split(cc);
      }
    }
  }
}


// Load a comparison operand into into a XMM register. Jump to not_numbers jump
// target passing the left and right result if the operand is not a number.
static void LoadComparisonOperand(MacroAssembler* masm_,
                                  Result* operand,
                                  XMMRegister xmm_reg,
                                  Result* left_side,
                                  Result* right_side,
                                  JumpTarget* not_numbers) {
  Label done;
  if (operand->type_info().IsDouble()) {
    // Operand is known to be a heap number, just load it.
    __ movsd(xmm_reg, FieldOperand(operand->reg(), HeapNumber::kValueOffset));
  } else if (operand->type_info().IsSmi()) {
    // Operand is known to be a smi. Convert it to double and keep the original
    // smi.
    __ SmiToInteger32(kScratchRegister, operand->reg());
    __ cvtlsi2sd(xmm_reg, kScratchRegister);
  } else {
    // Operand type not known, check for smi or heap number.
    Label smi;
    __ JumpIfSmi(operand->reg(), &smi);
    if (!operand->type_info().IsNumber()) {
      __ LoadRoot(kScratchRegister, Heap::kHeapNumberMapRootIndex);
      __ cmpq(FieldOperand(operand->reg(), HeapObject::kMapOffset),
              kScratchRegister);
      not_numbers->Branch(not_equal, left_side, right_side, taken);
    }
    __ movsd(xmm_reg, FieldOperand(operand->reg(), HeapNumber::kValueOffset));
    __ jmp(&done);

    __ bind(&smi);
    // Comvert smi to float and keep the original smi.
    __ SmiToInteger32(kScratchRegister, operand->reg());
    __ cvtlsi2sd(xmm_reg, kScratchRegister);
    __ jmp(&done);
  }
  __ bind(&done);
}


void CodeGenerator::GenerateInlineNumberComparison(Result* left_side,
                                                   Result* right_side,
                                                   Condition cc,
                                                   ControlDestination* dest) {
  ASSERT(left_side->is_register());
  ASSERT(right_side->is_register());

  JumpTarget not_numbers;
  // Load left and right operand into registers xmm0 and xmm1 and compare.
  LoadComparisonOperand(masm_, left_side, xmm0, left_side, right_side,
                        &not_numbers);
  LoadComparisonOperand(masm_, right_side, xmm1, left_side, right_side,
                        &not_numbers);
  __ ucomisd(xmm0, xmm1);
  // Bail out if a NaN is involved.
  not_numbers.Branch(parity_even, left_side, right_side);

  // Split to destination targets based on comparison.
  left_side->Unuse();
  right_side->Unuse();
  dest->true_target()->Branch(DoubleCondition(cc));
  dest->false_target()->Jump();

  not_numbers.Bind(left_side, right_side);
}


// Call the function just below TOS on the stack with the given
// arguments. The receiver is the TOS.
void CodeGenerator::CallWithArguments(ZoneList<Expression*>* args,
                                      CallFunctionFlags flags,
                                      int position) {
  // Push the arguments ("left-to-right") on the stack.
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
    frame_->SpillTop();
  }

  // Record the position for debugging purposes.
  CodeForSourcePosition(position);

  // Use the shared code stub to call the function.
  InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub call_function(arg_count, in_loop, flags);
  Result answer = frame_->CallStub(&call_function, arg_count + 1);
  // Restore context and replace function on the stack with the
  // result of the stub invocation.
  frame_->RestoreContextRegister();
  frame_->SetElementAt(0, &answer);
}


void CodeGenerator::CallApplyLazy(Expression* applicand,
                                  Expression* receiver,
                                  VariableProxy* arguments,
                                  int position) {
  // An optimized implementation of expressions of the form
  // x.apply(y, arguments).
  // If the arguments object of the scope has not been allocated,
  // and x.apply is Function.prototype.apply, this optimization
  // just copies y and the arguments of the current function on the
  // stack, as receiver and arguments, and calls x.
  // In the implementation comments, we call x the applicand
  // and y the receiver.
  ASSERT(ArgumentsMode() == LAZY_ARGUMENTS_ALLOCATION);
  ASSERT(arguments->IsArguments());

  // Load applicand.apply onto the stack. This will usually
  // give us a megamorphic load site. Not super, but it works.
  Load(applicand);
  frame()->Dup();
  Handle<String> name = Factory::LookupAsciiSymbol("apply");
  frame()->Push(name);
  Result answer = frame()->CallLoadIC(RelocInfo::CODE_TARGET);
  __ nop();
  frame()->Push(&answer);

  // Load the receiver and the existing arguments object onto the
  // expression stack. Avoid allocating the arguments object here.
  Load(receiver);
  LoadFromSlot(scope()->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);

  // Emit the source position information after having loaded the
  // receiver and the arguments.
  CodeForSourcePosition(position);
  // Contents of frame at this point:
  // Frame[0]: arguments object of the current function or the hole.
  // Frame[1]: receiver
  // Frame[2]: applicand.apply
  // Frame[3]: applicand.

  // Check if the arguments object has been lazily allocated
  // already. If so, just use that instead of copying the arguments
  // from the stack. This also deals with cases where a local variable
  // named 'arguments' has been introduced.
  frame_->Dup();
  Result probe = frame_->Pop();
  { VirtualFrame::SpilledScope spilled_scope;
    Label slow, done;
    bool try_lazy = true;
    if (probe.is_constant()) {
      try_lazy = probe.handle()->IsTheHole();
    } else {
      __ CompareRoot(probe.reg(), Heap::kTheHoleValueRootIndex);
      probe.Unuse();
      __ j(not_equal, &slow);
    }

    if (try_lazy) {
      Label build_args;
      // Get rid of the arguments object probe.
      frame_->Drop();  // Can be called on a spilled frame.
      // Stack now has 3 elements on it.
      // Contents of stack at this point:
      // rsp[0]: receiver
      // rsp[1]: applicand.apply
      // rsp[2]: applicand.

      // Check that the receiver really is a JavaScript object.
      __ movq(rax, Operand(rsp, 0));
      Condition is_smi = masm_->CheckSmi(rax);
      __ j(is_smi, &build_args);
      // We allow all JSObjects including JSFunctions.  As long as
      // JS_FUNCTION_TYPE is the last instance type and it is right
      // after LAST_JS_OBJECT_TYPE, we do not have to check the upper
      // bound.
      STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      STATIC_ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
      __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(below, &build_args);

      // Check that applicand.apply is Function.prototype.apply.
      __ movq(rax, Operand(rsp, kPointerSize));
      is_smi = masm_->CheckSmi(rax);
      __ j(is_smi, &build_args);
      __ CmpObjectType(rax, JS_FUNCTION_TYPE, rcx);
      __ j(not_equal, &build_args);
      __ movq(rcx, FieldOperand(rax, JSFunction::kCodeEntryOffset));
      __ subq(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
      Handle<Code> apply_code(Builtins::builtin(Builtins::FunctionApply));
      __ Cmp(rcx, apply_code);
      __ j(not_equal, &build_args);

      // Check that applicand is a function.
      __ movq(rdi, Operand(rsp, 2 * kPointerSize));
      is_smi = masm_->CheckSmi(rdi);
      __ j(is_smi, &build_args);
      __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
      __ j(not_equal, &build_args);

      // Copy the arguments to this function possibly from the
      // adaptor frame below it.
      Label invoke, adapted;
      __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
      __ SmiCompare(Operand(rdx, StandardFrameConstants::kContextOffset),
                    Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
      __ j(equal, &adapted);

      // No arguments adaptor frame. Copy fixed number of arguments.
      __ Set(rax, scope()->num_parameters());
      for (int i = 0; i < scope()->num_parameters(); i++) {
        __ push(frame_->ParameterAt(i));
      }
      __ jmp(&invoke);

      // Arguments adaptor frame present. Copy arguments from there, but
      // avoid copying too many arguments to avoid stack overflows.
      __ bind(&adapted);
      static const uint32_t kArgumentsLimit = 1 * KB;
      __ SmiToInteger32(rax,
                        Operand(rdx,
                                ArgumentsAdaptorFrameConstants::kLengthOffset));
      __ movl(rcx, rax);
      __ cmpl(rax, Immediate(kArgumentsLimit));
      __ j(above, &build_args);

      // Loop through the arguments pushing them onto the execution
      // stack. We don't inform the virtual frame of the push, so we don't
      // have to worry about getting rid of the elements from the virtual
      // frame.
      Label loop;
      // rcx is a small non-negative integer, due to the test above.
      __ testl(rcx, rcx);
      __ j(zero, &invoke);
      __ bind(&loop);
      __ push(Operand(rdx, rcx, times_pointer_size, 1 * kPointerSize));
      __ decl(rcx);
      __ j(not_zero, &loop);

      // Invoke the function.
      __ bind(&invoke);
      ParameterCount actual(rax);
      __ InvokeFunction(rdi, actual, CALL_FUNCTION);
      // Drop applicand.apply and applicand from the stack, and push
      // the result of the function call, but leave the spilled frame
      // unchanged, with 3 elements, so it is correct when we compile the
      // slow-case code.
      __ addq(rsp, Immediate(2 * kPointerSize));
      __ push(rax);
      // Stack now has 1 element:
      //   rsp[0]: result
      __ jmp(&done);

      // Slow-case: Allocate the arguments object since we know it isn't
      // there, and fall-through to the slow-case where we call
      // applicand.apply.
      __ bind(&build_args);
      // Stack now has 3 elements, because we have jumped from where:
      // rsp[0]: receiver
      // rsp[1]: applicand.apply
      // rsp[2]: applicand.

      // StoreArgumentsObject requires a correct frame, and may modify it.
      Result arguments_object = StoreArgumentsObject(false);
      frame_->SpillAll();
      arguments_object.ToRegister();
      frame_->EmitPush(arguments_object.reg());
      arguments_object.Unuse();
      // Stack and frame now have 4 elements.
      __ bind(&slow);
    }

    // Generic computation of x.apply(y, args) with no special optimization.
    // Flip applicand.apply and applicand on the stack, so
    // applicand looks like the receiver of the applicand.apply call.
    // Then process it as a normal function call.
    __ movq(rax, Operand(rsp, 3 * kPointerSize));
    __ movq(rbx, Operand(rsp, 2 * kPointerSize));
    __ movq(Operand(rsp, 2 * kPointerSize), rax);
    __ movq(Operand(rsp, 3 * kPointerSize), rbx);

    CallFunctionStub call_function(2, NOT_IN_LOOP, NO_CALL_FUNCTION_FLAGS);
    Result res = frame_->CallStub(&call_function, 3);
    // The function and its two arguments have been dropped.
    frame_->Drop(1);  // Drop the receiver as well.
    res.ToRegister();
    frame_->EmitPush(res.reg());
    // Stack now has 1 element:
    //   rsp[0]: result
    if (try_lazy) __ bind(&done);
  }  // End of spilled scope.
  // Restore the context register after a call.
  frame_->RestoreContextRegister();
}


class DeferredStackCheck: public DeferredCode {
 public:
  DeferredStackCheck() {
    set_comment("[ DeferredStackCheck");
  }

  virtual void Generate();
};


void DeferredStackCheck::Generate() {
  StackCheckStub stub;
  __ CallStub(&stub);
}


void CodeGenerator::CheckStack() {
  DeferredStackCheck* deferred = new DeferredStackCheck;
  __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
  deferred->Branch(below);
  deferred->BindExit();
}


void CodeGenerator::VisitAndSpill(Statement* statement) {
  ASSERT(in_spilled_code());
  set_in_spilled_code(false);
  Visit(statement);
  if (frame_ != NULL) {
    frame_->SpillAll();
  }
  set_in_spilled_code(true);
}


void CodeGenerator::VisitStatementsAndSpill(ZoneList<Statement*>* statements) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  ASSERT(in_spilled_code());
  set_in_spilled_code(false);
  VisitStatements(statements);
  if (frame_ != NULL) {
    frame_->SpillAll();
  }
  set_in_spilled_code(true);

  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  ASSERT(!in_spilled_code());
  for (int i = 0; has_valid_frame() && i < statements->length(); i++) {
    Visit(statements->at(i));
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitBlock(Block* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ Block");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  VisitStatements(node->statements());
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.  The inevitable call
  // will sync frame elements to memory anyway, so we do it eagerly to
  // allow us to push the arguments directly into place.
  frame_->SyncRange(0, frame_->element_count() - 1);

  __ movq(kScratchRegister, pairs, RelocInfo::EMBEDDED_OBJECT);
  frame_->EmitPush(rsi);  // The context is the first argument.
  frame_->EmitPush(kScratchRegister);
  frame_->EmitPush(Smi::FromInt(is_eval() ? 1 : 0));
  Result ignored = frame_->CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
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
    // For now, just do a runtime call.  Sync the virtual frame eagerly
    // so we can simply push the arguments into place.
    frame_->SyncRange(0, frame_->element_count() - 1);
    frame_->EmitPush(rsi);
    __ movq(kScratchRegister, var->name(), RelocInfo::EMBEDDED_OBJECT);
    frame_->EmitPush(kScratchRegister);
    // Declaration nodes are always introduced in one of two modes.
    ASSERT(node->mode() == Variable::VAR || node->mode() == Variable::CONST);
    PropertyAttributes attr = node->mode() == Variable::VAR ? NONE : READ_ONLY;
    frame_->EmitPush(Smi::FromInt(attr));
    // Push initial value, if any.
    // Note: For variables we must not push an initial value (such as
    // 'undefined') because we may have a (legal) redeclaration and we
    // must not destroy the current value.
    if (node->mode() == Variable::CONST) {
      frame_->EmitPush(Heap::kTheHoleValueRootIndex);
    } else if (node->fun() != NULL) {
      Load(node->fun());
    } else {
      frame_->EmitPush(Smi::FromInt(0));  // no initial value!
    }
    Result ignored = frame_->CallRuntime(Runtime::kDeclareContextSlot, 4);
    // Ignore the return value (declarations are statements).
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
      // Set the initial value.
      Reference target(this, node->proxy());
      Load(val);
      target.SetValue(NOT_CONST_INIT);
      // The reference is removed from the stack (preserving TOS) when
      // it goes out of scope.
    }
    // Get rid of the assigned value (declarations are statements).
    frame_->Drop();
  }
}


void CodeGenerator::VisitExpressionStatement(ExpressionStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ ExpressionStatement");
  CodeForStatementPosition(node);
  Expression* expression = node->expression();
  expression->MarkAsStatement();
  Load(expression);
  // Remove the lingering expression result from the top of stack.
  frame_->Drop();
}


void CodeGenerator::VisitEmptyStatement(EmptyStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "// EmptyStatement");
  CodeForStatementPosition(node);
  // nothing to do
}


void CodeGenerator::VisitIfStatement(IfStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ IfStatement");
  // Generate different code depending on which parts of the if statement
  // are present or not.
  bool has_then_stm = node->HasThenStatement();
  bool has_else_stm = node->HasElseStatement();

  CodeForStatementPosition(node);
  JumpTarget exit;
  if (has_then_stm && has_else_stm) {
    JumpTarget then;
    JumpTarget else_;
    ControlDestination dest(&then, &else_, true);
    LoadCondition(node->condition(), &dest, true);

    if (dest.false_was_fall_through()) {
      // The else target was bound, so we compile the else part first.
      Visit(node->else_statement());

      // We may have dangling jumps to the then part.
      if (then.is_linked()) {
        if (has_valid_frame()) exit.Jump();
        then.Bind();
        Visit(node->then_statement());
      }
    } else {
      // The then target was bound, so we compile the then part first.
      Visit(node->then_statement());

      if (else_.is_linked()) {
        if (has_valid_frame()) exit.Jump();
        else_.Bind();
        Visit(node->else_statement());
      }
    }

  } else if (has_then_stm) {
    ASSERT(!has_else_stm);
    JumpTarget then;
    ControlDestination dest(&then, &exit, true);
    LoadCondition(node->condition(), &dest, true);

    if (dest.false_was_fall_through()) {
      // The exit label was bound.  We may have dangling jumps to the
      // then part.
      if (then.is_linked()) {
        exit.Unuse();
        exit.Jump();
        then.Bind();
        Visit(node->then_statement());
      }
    } else {
      // The then label was bound.
      Visit(node->then_statement());
    }

  } else if (has_else_stm) {
    ASSERT(!has_then_stm);
    JumpTarget else_;
    ControlDestination dest(&exit, &else_, false);
    LoadCondition(node->condition(), &dest, true);

    if (dest.true_was_fall_through()) {
      // The exit label was bound.  We may have dangling jumps to the
      // else part.
      if (else_.is_linked()) {
        exit.Unuse();
        exit.Jump();
        else_.Bind();
        Visit(node->else_statement());
      }
    } else {
      // The else label was bound.
      Visit(node->else_statement());
    }

  } else {
    ASSERT(!has_then_stm && !has_else_stm);
    // We only care about the condition's side effects (not its value
    // or control flow effect).  LoadCondition is called without
    // forcing control flow.
    ControlDestination dest(&exit, &exit, true);
    LoadCondition(node->condition(), &dest, false);
    if (!dest.is_used()) {
      // We got a value on the frame rather than (or in addition to)
      // control flow.
      frame_->Drop();
    }
  }

  if (exit.is_linked()) {
    exit.Bind();
  }
}


void CodeGenerator::VisitContinueStatement(ContinueStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ ContinueStatement");
  CodeForStatementPosition(node);
  node->target()->continue_target()->Jump();
}


void CodeGenerator::VisitBreakStatement(BreakStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ BreakStatement");
  CodeForStatementPosition(node);
  node->target()->break_target()->Jump();
}


void CodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ ReturnStatement");

  CodeForStatementPosition(node);
  Load(node->expression());
  Result return_value = frame_->Pop();
  masm()->WriteRecordedPositions();
  if (function_return_is_shadowed_) {
    function_return_.Jump(&return_value);
  } else {
    frame_->PrepareForReturn();
    if (function_return_.is_bound()) {
      // If the function return label is already bound we reuse the
      // code by jumping to the return site.
      function_return_.Jump(&return_value);
    } else {
      function_return_.Bind(&return_value);
      GenerateReturnSequence(&return_value);
    }
  }
}


void CodeGenerator::GenerateReturnSequence(Result* return_value) {
  // The return value is a live (but not currently reference counted)
  // reference to rax.  This is safe because the current frame does not
  // contain a reference to rax (it is prepared for the return by spilling
  // all registers).
  if (FLAG_trace) {
    frame_->Push(return_value);
    *return_value = frame_->CallRuntime(Runtime::kTraceExit, 1);
  }
  return_value->ToRegister(rax);

  // Add a label for checking the size of the code used for returning.
#ifdef DEBUG
  Label check_exit_codesize;
  masm_->bind(&check_exit_codesize);
#endif

  // Leave the frame and return popping the arguments and the
  // receiver.
  frame_->Exit();
  masm_->ret((scope()->num_parameters() + 1) * kPointerSize);
  DeleteFrame();

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Add padding that will be overwritten by a debugger breakpoint.
  // frame_->Exit() generates "movq rsp, rbp; pop rbp; ret k"
  // with length 7 (3 + 1 + 3).
  const int kPadding = Assembler::kJSReturnSequenceLength - 7;
  for (int i = 0; i < kPadding; ++i) {
    masm_->int3();
  }
  // Check that the size of the code used for returning matches what is
  // expected by the debugger.
  ASSERT_EQ(Assembler::kJSReturnSequenceLength,
            masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
#endif
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ WithEnterStatement");
  CodeForStatementPosition(node);
  Load(node->expression());
  Result context;
  if (node->is_catch_block()) {
    context = frame_->CallRuntime(Runtime::kPushCatchContext, 1);
  } else {
    context = frame_->CallRuntime(Runtime::kPushContext, 1);
  }

  // Update context local.
  frame_->SaveContextRegister();

  // Verify that the runtime call result and rsi agree.
  if (FLAG_debug_code) {
    __ cmpq(context.reg(), rsi);
    __ Assert(equal, "Runtime::NewContext should end up in rsi");
  }
}


void CodeGenerator::VisitWithExitStatement(WithExitStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ WithExitStatement");
  CodeForStatementPosition(node);
  // Pop context.
  __ movq(rsi, ContextOperand(rsi, Context::PREVIOUS_INDEX));
  // Update context local.
  frame_->SaveContextRegister();
}


void CodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ SwitchStatement");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  // Compile the switch value.
  Load(node->tag());

  ZoneList<CaseClause*>* cases = node->cases();
  int length = cases->length();
  CaseClause* default_clause = NULL;

  JumpTarget next_test;
  // Compile the case label expressions and comparisons.  Exit early
  // if a comparison is unconditionally true.  The target next_test is
  // bound before the loop in order to indicate control flow to the
  // first comparison.
  next_test.Bind();
  for (int i = 0; i < length && !next_test.is_unused(); i++) {
    CaseClause* clause = cases->at(i);
    // The default is not a test, but remember it for later.
    if (clause->is_default()) {
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case comparison");
    // We recycle the same target next_test for each test.  Bind it if
    // the previous test has not done so and then unuse it for the
    // loop.
    if (next_test.is_linked()) {
      next_test.Bind();
    }
    next_test.Unuse();

    // Duplicate the switch value.
    frame_->Dup();

    // Compile the label expression.
    Load(clause->label());

    // Compare and branch to the body if true or the next test if
    // false.  Prefer the next test as a fall through.
    ControlDestination dest(clause->body_target(), &next_test, false);
    Comparison(node, equal, true, &dest);

    // If the comparison fell through to the true target, jump to the
    // actual body.
    if (dest.true_was_fall_through()) {
      clause->body_target()->Unuse();
      clause->body_target()->Jump();
    }
  }

  // If there was control flow to a next test from the last one
  // compiled, compile a jump to the default or break target.
  if (!next_test.is_unused()) {
    if (next_test.is_linked()) {
      next_test.Bind();
    }
    // Drop the switch value.
    frame_->Drop();
    if (default_clause != NULL) {
      default_clause->body_target()->Jump();
    } else {
      node->break_target()->Jump();
    }
  }

  // The last instruction emitted was a jump, either to the default
  // clause or the break target, or else to a case body from the loop
  // that compiles the tests.
  ASSERT(!has_valid_frame());
  // Compile case bodies as needed.
  for (int i = 0; i < length; i++) {
    CaseClause* clause = cases->at(i);

    // There are two ways to reach the body: from the corresponding
    // test or as the fall through of the previous body.
    if (clause->body_target()->is_linked() || has_valid_frame()) {
      if (clause->body_target()->is_linked()) {
        if (has_valid_frame()) {
          // If we have both a jump to the test and a fall through, put
          // a jump on the fall through path to avoid the dropping of
          // the switch value on the test path.  The exception is the
          // default which has already had the switch value dropped.
          if (clause->is_default()) {
            clause->body_target()->Bind();
          } else {
            JumpTarget body;
            body.Jump();
            clause->body_target()->Bind();
            frame_->Drop();
            body.Bind();
          }
        } else {
          // No fall through to worry about.
          clause->body_target()->Bind();
          if (!clause->is_default()) {
            frame_->Drop();
          }
        }
      } else {
        // Otherwise, we have only fall through.
        ASSERT(has_valid_frame());
      }

      // We are now prepared to compile the body.
      Comment cmnt(masm_, "[ Case body");
      VisitStatements(clause->statements());
    }
    clause->body_target()->Unuse();
  }

  // We may not have a valid frame here so bind the break target only
  // if needed.
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
}


void CodeGenerator::VisitDoWhileStatement(DoWhileStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ DoWhileStatement");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  JumpTarget body(JumpTarget::BIDIRECTIONAL);
  IncrementLoopNesting();

  ConditionAnalysis info = AnalyzeCondition(node->cond());
  // Label the top of the loop for the backward jump if necessary.
  switch (info) {
    case ALWAYS_TRUE:
      // Use the continue target.
      node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
      node->continue_target()->Bind();
      break;
    case ALWAYS_FALSE:
      // No need to label it.
      node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      break;
    case DONT_KNOW:
      // Continue is the test, so use the backward body target.
      node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      body.Bind();
      break;
  }

  CheckStack();  // TODO(1222600): ignore if body contains calls.
  Visit(node->body());

  // Compile the test.
  switch (info) {
    case ALWAYS_TRUE:
      // If control flow can fall off the end of the body, jump back
      // to the top and bind the break target at the exit.
      if (has_valid_frame()) {
        node->continue_target()->Jump();
      }
      if (node->break_target()->is_linked()) {
        node->break_target()->Bind();
      }
      break;
    case ALWAYS_FALSE:
      // We may have had continues or breaks in the body.
      if (node->continue_target()->is_linked()) {
        node->continue_target()->Bind();
      }
      if (node->break_target()->is_linked()) {
        node->break_target()->Bind();
      }
      break;
    case DONT_KNOW:
      // We have to compile the test expression if it can be reached by
      // control flow falling out of the body or via continue.
      if (node->continue_target()->is_linked()) {
        node->continue_target()->Bind();
      }
      if (has_valid_frame()) {
        Comment cmnt(masm_, "[ DoWhileCondition");
        CodeForDoWhileConditionPosition(node);
        ControlDestination dest(&body, node->break_target(), false);
        LoadCondition(node->cond(), &dest, true);
      }
      if (node->break_target()->is_linked()) {
        node->break_target()->Bind();
      }
      break;
  }

  DecrementLoopNesting();
  node->continue_target()->Unuse();
  node->break_target()->Unuse();
}


void CodeGenerator::VisitWhileStatement(WhileStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ WhileStatement");
  CodeForStatementPosition(node);

  // If the condition is always false and has no side effects, we do not
  // need to compile anything.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  // Do not duplicate conditions that may have function literal
  // subexpressions.  This can cause us to compile the function literal
  // twice.
  bool test_at_bottom = !node->may_have_function_literal();
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  IncrementLoopNesting();
  JumpTarget body;
  if (test_at_bottom) {
    body.set_direction(JumpTarget::BIDIRECTIONAL);
  }

  // Based on the condition analysis, compile the test as necessary.
  switch (info) {
    case ALWAYS_TRUE:
      // We will not compile the test expression.  Label the top of the
      // loop with the continue target.
      node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
      node->continue_target()->Bind();
      break;
    case DONT_KNOW: {
      if (test_at_bottom) {
        // Continue is the test at the bottom, no need to label the test
        // at the top.  The body is a backward target.
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      } else {
        // Label the test at the top as the continue target.  The body
        // is a forward-only target.
        node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      }
      // Compile the test with the body as the true target and preferred
      // fall-through and with the break target as the false target.
      ControlDestination dest(&body, node->break_target(), true);
      LoadCondition(node->cond(), &dest, true);

      if (dest.false_was_fall_through()) {
        // If we got the break target as fall-through, the test may have
        // been unconditionally false (if there are no jumps to the
        // body).
        if (!body.is_linked()) {
          DecrementLoopNesting();
          return;
        }

        // Otherwise, jump around the body on the fall through and then
        // bind the body target.
        node->break_target()->Unuse();
        node->break_target()->Jump();
        body.Bind();
      }
      break;
    }
    case ALWAYS_FALSE:
      UNREACHABLE();
      break;
  }

  CheckStack();  // TODO(1222600): ignore if body contains calls.
  Visit(node->body());

  // Based on the condition analysis, compile the backward jump as
  // necessary.
  switch (info) {
    case ALWAYS_TRUE:
      // The loop body has been labeled with the continue target.
      if (has_valid_frame()) {
        node->continue_target()->Jump();
      }
      break;
    case DONT_KNOW:
      if (test_at_bottom) {
        // If we have chosen to recompile the test at the bottom,
        // then it is the continue target.
        if (node->continue_target()->is_linked()) {
          node->continue_target()->Bind();
        }
        if (has_valid_frame()) {
          // The break target is the fall-through (body is a backward
          // jump from here and thus an invalid fall-through).
          ControlDestination dest(&body, node->break_target(), false);
          LoadCondition(node->cond(), &dest, true);
        }
      } else {
        // If we have chosen not to recompile the test at the bottom,
        // jump back to the one at the top.
        if (has_valid_frame()) {
          node->continue_target()->Jump();
        }
      }
      break;
    case ALWAYS_FALSE:
      UNREACHABLE();
      break;
  }

  // The break target may be already bound (by the condition), or there
  // may not be a valid frame.  Bind it only if needed.
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  DecrementLoopNesting();
}


void CodeGenerator::SetTypeForStackSlot(Slot* slot, TypeInfo info) {
  ASSERT(slot->type() == Slot::LOCAL || slot->type() == Slot::PARAMETER);
  if (slot->type() == Slot::LOCAL) {
    frame_->SetTypeForLocalAt(slot->index(), info);
  } else {
    frame_->SetTypeForParamAt(slot->index(), info);
  }
  if (FLAG_debug_code && info.IsSmi()) {
    if (slot->type() == Slot::LOCAL) {
      frame_->PushLocalAt(slot->index());
    } else {
      frame_->PushParameterAt(slot->index());
    }
    Result var = frame_->Pop();
    var.ToRegister();
    __ AbortIfNotSmi(var.reg());
  }
}


void CodeGenerator::GenerateFastSmiLoop(ForStatement* node) {
  // A fast smi loop is a for loop with an initializer
  // that is a simple assignment of a smi to a stack variable,
  // a test that is a simple test of that variable against a smi constant,
  // and a step that is a increment/decrement of the variable, and
  // where the variable isn't modified in the loop body.
  // This guarantees that the variable is always a smi.

  Variable* loop_var = node->loop_variable();
  Smi* initial_value = *Handle<Smi>::cast(node->init()
      ->StatementAsSimpleAssignment()->value()->AsLiteral()->handle());
  Smi* limit_value = *Handle<Smi>::cast(
      node->cond()->AsCompareOperation()->right()->AsLiteral()->handle());
  Token::Value compare_op =
      node->cond()->AsCompareOperation()->op();
  bool increments =
      node->next()->StatementAsCountOperation()->op() == Token::INC;

  // Check that the condition isn't initially false.
  bool initially_false = false;
  int initial_int_value = initial_value->value();
  int limit_int_value = limit_value->value();
  switch (compare_op) {
    case Token::LT:
      initially_false = initial_int_value >= limit_int_value;
      break;
    case Token::LTE:
      initially_false = initial_int_value > limit_int_value;
      break;
    case Token::GT:
      initially_false = initial_int_value <= limit_int_value;
      break;
    case Token::GTE:
      initially_false = initial_int_value < limit_int_value;
      break;
    default:
      UNREACHABLE();
  }
  if (initially_false) return;

  // Only check loop condition at the end.

  Visit(node->init());

  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  // Set type and stack height of BreakTargets.
  node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  IncrementLoopNesting();
  loop.Bind();

  // Set number type of the loop variable to smi.
  CheckStack();  // TODO(1222600): ignore if body contains calls.

  SetTypeForStackSlot(loop_var->slot(), TypeInfo::Smi());
  Visit(node->body());

  if (node->continue_target()->is_linked()) {
    node->continue_target()->Bind();
  }

  if (has_valid_frame()) {
    CodeForStatementPosition(node);
    Slot* loop_var_slot = loop_var->slot();
    if (loop_var_slot->type() == Slot::LOCAL) {
      frame_->TakeLocalAt(loop_var_slot->index());
    } else {
      ASSERT(loop_var_slot->type() == Slot::PARAMETER);
      frame_->TakeParameterAt(loop_var_slot->index());
    }
    Result loop_var_result = frame_->Pop();
    if (!loop_var_result.is_register()) {
      loop_var_result.ToRegister();
    }
    Register loop_var_reg = loop_var_result.reg();
    frame_->Spill(loop_var_reg);
    if (increments) {
      __ SmiAddConstant(loop_var_reg,
                        loop_var_reg,
                        Smi::FromInt(1));
    } else {
      __ SmiSubConstant(loop_var_reg,
                        loop_var_reg,
                        Smi::FromInt(1));
    }

    frame_->Push(&loop_var_result);
    if (loop_var_slot->type() == Slot::LOCAL) {
      frame_->StoreToLocalAt(loop_var_slot->index());
    } else {
      ASSERT(loop_var_slot->type() == Slot::PARAMETER);
      frame_->StoreToParameterAt(loop_var_slot->index());
    }
    frame_->Drop();

    __ SmiCompare(loop_var_reg, limit_value);
    Condition condition;
    switch (compare_op) {
      case Token::LT:
        condition = less;
        break;
      case Token::LTE:
        condition = less_equal;
        break;
      case Token::GT:
        condition = greater;
        break;
      case Token::GTE:
        condition = greater_equal;
        break;
      default:
        condition = never;
        UNREACHABLE();
    }
    loop.Branch(condition);
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  DecrementLoopNesting();
}


void CodeGenerator::VisitForStatement(ForStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ ForStatement");
  CodeForStatementPosition(node);

  if (node->is_fast_smi_loop()) {
    GenerateFastSmiLoop(node);
    return;
  }

  // Compile the init expression if present.
  if (node->init() != NULL) {
    Visit(node->init());
  }

  // If the condition is always false and has no side effects, we do not
  // need to compile anything else.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  // Do not duplicate conditions that may have function literal
  // subexpressions.  This can cause us to compile the function literal
  // twice.
  bool test_at_bottom = !node->may_have_function_literal();
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  IncrementLoopNesting();

  // Target for backward edge if no test at the bottom, otherwise
  // unused.
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);

  // Target for backward edge if there is a test at the bottom,
  // otherwise used as target for test at the top.
  JumpTarget body;
  if (test_at_bottom) {
    body.set_direction(JumpTarget::BIDIRECTIONAL);
  }

  // Based on the condition analysis, compile the test as necessary.
  switch (info) {
    case ALWAYS_TRUE:
      // We will not compile the test expression.  Label the top of the
      // loop.
      if (node->next() == NULL) {
        // Use the continue target if there is no update expression.
        node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else {
        // Otherwise use the backward loop target.
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
        loop.Bind();
      }
      break;
    case DONT_KNOW: {
      if (test_at_bottom) {
        // Continue is either the update expression or the test at the
        // bottom, no need to label the test at the top.
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      } else if (node->next() == NULL) {
        // We are not recompiling the test at the bottom and there is no
        // update expression.
        node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else {
        // We are not recompiling the test at the bottom and there is an
        // update expression.
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
        loop.Bind();
      }

      // Compile the test with the body as the true target and preferred
      // fall-through and with the break target as the false target.
      ControlDestination dest(&body, node->break_target(), true);
      LoadCondition(node->cond(), &dest, true);

      if (dest.false_was_fall_through()) {
        // If we got the break target as fall-through, the test may have
        // been unconditionally false (if there are no jumps to the
        // body).
        if (!body.is_linked()) {
          DecrementLoopNesting();
          return;
        }

        // Otherwise, jump around the body on the fall through and then
        // bind the body target.
        node->break_target()->Unuse();
        node->break_target()->Jump();
        body.Bind();
      }
      break;
    }
    case ALWAYS_FALSE:
      UNREACHABLE();
      break;
  }

  CheckStack();  // TODO(1222600): ignore if body contains calls.

  Visit(node->body());

  // If there is an update expression, compile it if necessary.
  if (node->next() != NULL) {
    if (node->continue_target()->is_linked()) {
      node->continue_target()->Bind();
    }

    // Control can reach the update by falling out of the body or by a
    // continue.
    if (has_valid_frame()) {
      // Record the source position of the statement as this code which
      // is after the code for the body actually belongs to the loop
      // statement and not the body.
      CodeForStatementPosition(node);
      Visit(node->next());
    }
  }

  // Based on the condition analysis, compile the backward jump as
  // necessary.
  switch (info) {
    case ALWAYS_TRUE:
      if (has_valid_frame()) {
        if (node->next() == NULL) {
          node->continue_target()->Jump();
        } else {
          loop.Jump();
        }
      }
      break;
    case DONT_KNOW:
      if (test_at_bottom) {
        if (node->continue_target()->is_linked()) {
          // We can have dangling jumps to the continue target if there
          // was no update expression.
          node->continue_target()->Bind();
        }
        // Control can reach the test at the bottom by falling out of
        // the body, by a continue in the body, or from the update
        // expression.
        if (has_valid_frame()) {
          // The break target is the fall-through (body is a backward
          // jump from here).
          ControlDestination dest(&body, node->break_target(), false);
          LoadCondition(node->cond(), &dest, true);
        }
      } else {
        // Otherwise, jump back to the test at the top.
        if (has_valid_frame()) {
          if (node->next() == NULL) {
            node->continue_target()->Jump();
          } else {
            loop.Jump();
          }
        }
      }
      break;
    case ALWAYS_FALSE:
      UNREACHABLE();
      break;
  }

  // The break target may be already bound (by the condition), or there
  // may not be a valid frame.  Bind it only if needed.
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  DecrementLoopNesting();
}


void CodeGenerator::VisitForInStatement(ForInStatement* node) {
  ASSERT(!in_spilled_code());
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
  frame_->EmitPop(rax);

  // rax: value to be iterated over
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  exit.Branch(equal);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  exit.Branch(equal);

  // Stack layout in body:
  // [iteration counter (smi)] <- slot 0
  // [length of array]         <- slot 1
  // [FixedArray]              <- slot 2
  // [Map or 0]                <- slot 3
  // [Object]                  <- slot 4

  // Check if enumerable is already a JSObject
  // rax: value to be iterated over
  Condition is_smi = masm_->CheckSmi(rax);
  primitive.Branch(is_smi);
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
  jsobject.Branch(above_equal);

  primitive.Bind();
  frame_->EmitPush(rax);
  frame_->InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION, 1);
  // function call returns the value in rax, which is where we want it below

  jsobject.Bind();
  // Get the set of properties (as a FixedArray or Map).
  // rax: value to be iterated over
  frame_->EmitPush(rax);  // Push the object being iterated over.


  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  JumpTarget call_runtime;
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  JumpTarget check_prototype;
  JumpTarget use_cache;
  __ movq(rcx, rax);
  loop.Bind();
  // Check that there are no elements.
  __ movq(rdx, FieldOperand(rcx, JSObject::kElementsOffset));
  __ CompareRoot(rdx, Heap::kEmptyFixedArrayRootIndex);
  call_runtime.Branch(not_equal);
  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in ebx for the subsequent
  // prototype load.
  __ movq(rbx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rdx, FieldOperand(rbx, Map::kInstanceDescriptorsOffset));
  __ CompareRoot(rdx, Heap::kEmptyDescriptorArrayRootIndex);
  call_runtime.Branch(equal);
  // Check that there in an enum cache in the non-empty instance
  // descriptors.  This is the case if the next enumeration index
  // field does not contain a smi.
  __ movq(rdx, FieldOperand(rdx, DescriptorArray::kEnumerationIndexOffset));
  is_smi = masm_->CheckSmi(rdx);
  call_runtime.Branch(is_smi);
  // For all objects but the receiver, check that the cache is empty.
  __ cmpq(rcx, rax);
  check_prototype.Branch(equal);
  __ movq(rdx, FieldOperand(rdx, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ CompareRoot(rdx, Heap::kEmptyFixedArrayRootIndex);
  call_runtime.Branch(not_equal);
  check_prototype.Bind();
  // Load the prototype from the map and loop if non-null.
  __ movq(rcx, FieldOperand(rbx, Map::kPrototypeOffset));
  __ CompareRoot(rcx, Heap::kNullValueRootIndex);
  loop.Branch(not_equal);
  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  __ movq(rax, FieldOperand(rax, HeapObject::kMapOffset));
  use_cache.Jump();

  call_runtime.Bind();
  // Call the runtime to get the property names for the object.
  frame_->EmitPush(rax);  // push the Object (slot 4) for the runtime call
  frame_->CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a Map, we can do a fast modification check.
  // Otherwise, we got a FixedArray, and we have to do a slow check.
  // rax: map or fixed array (result from call to
  // Runtime::kGetPropertyNamesFast)
  __ movq(rdx, rax);
  __ movq(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ CompareRoot(rcx, Heap::kMetaMapRootIndex);
  fixed_array.Branch(not_equal);

  use_cache.Bind();
  // Get enum cache
  // rax: map (either the result from a call to
  // Runtime::kGetPropertyNamesFast or has been fetched directly from
  // the object)
  __ movq(rcx, rax);
  __ movq(rcx, FieldOperand(rcx, Map::kInstanceDescriptorsOffset));
  // Get the bridge array held in the enumeration index field.
  __ movq(rcx, FieldOperand(rcx, DescriptorArray::kEnumerationIndexOffset));
  // Get the cache from the bridge array.
  __ movq(rdx, FieldOperand(rcx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  frame_->EmitPush(rax);  // <- slot 3
  frame_->EmitPush(rdx);  // <- slot 2
  __ movq(rax, FieldOperand(rdx, FixedArray::kLengthOffset));
  frame_->EmitPush(rax);  // <- slot 1
  frame_->EmitPush(Smi::FromInt(0));  // <- slot 0
  entry.Jump();

  fixed_array.Bind();
  // rax: fixed array (result from call to Runtime::kGetPropertyNamesFast)
  frame_->EmitPush(Smi::FromInt(0));  // <- slot 3
  frame_->EmitPush(rax);  // <- slot 2

  // Push the length of the array and the initial index onto the stack.
  __ movq(rax, FieldOperand(rax, FixedArray::kLengthOffset));
  frame_->EmitPush(rax);  // <- slot 1
  frame_->EmitPush(Smi::FromInt(0));  // <- slot 0

  // Condition.
  entry.Bind();
  // Grab the current frame's height for the break and continue
  // targets only after all the state is pushed on the frame.
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);

  __ movq(rax, frame_->ElementAt(0));  // load the current count
  __ SmiCompare(frame_->ElementAt(1), rax);  // compare to the array length
  node->break_target()->Branch(below_equal);

  // Get the i'th entry of the array.
  __ movq(rdx, frame_->ElementAt(2));
  SmiIndex index = masm_->SmiToIndex(rbx, rax, kPointerSizeLog2);
  __ movq(rbx,
          FieldOperand(rdx, index.reg, index.scale, FixedArray::kHeaderSize));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case rax: current iteration count rbx: i'th entry
  // of the enum cache
  __ movq(rdx, frame_->ElementAt(3));
  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  // rax: current iteration count
  // rbx: i'th entry of the enum cache
  // rdx: expected map value
  __ movq(rcx, frame_->ElementAt(4));
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ cmpq(rcx, rdx);
  end_del_check.Branch(equal);

  // Convert the entry to a string (or null if it isn't a property anymore).
  frame_->EmitPush(frame_->ElementAt(4));  // push enumerable
  frame_->EmitPush(rbx);  // push entry
  frame_->InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION, 2);
  __ movq(rbx, rax);

  // If the property has been removed while iterating, we just skip it.
  __ SmiCompare(rbx, Smi::FromInt(0));
  node->continue_target()->Branch(equal);

  end_del_check.Bind();
  // Store the entry in the 'each' expression and take another spin in the
  // loop.  rdx: i'th entry of the enum cache (or string there of)
  frame_->EmitPush(rbx);
  { Reference each(this, node->each());
    // Loading a reference may leave the frame in an unspilled state.
    frame_->SpillAll();
    if (!each.is_illegal()) {
      if (each.size() > 0) {
        frame_->EmitPush(frame_->ElementAt(each.size()));
        each.SetValue(NOT_CONST_INIT);
        frame_->Drop(2);  // Drop the original and the copy of the element.
      } else {
        // If the reference has size zero then we can use the value below
        // the reference as if it were above the reference, instead of pushing
        // a new copy of it above the reference.
        each.SetValue(NOT_CONST_INIT);
        frame_->Drop();  // Drop the original of the element.
      }
    }
  }
  // Unloading a reference may leave the frame in an unspilled state.
  frame_->SpillAll();

  // Body.
  CheckStack();  // TODO(1222600): ignore if body contains calls.
  VisitAndSpill(node->body());

  // Next.  Reestablish a spilled frame in case we are coming here via
  // a continue in the body.
  node->continue_target()->Bind();
  frame_->SpillAll();
  frame_->EmitPop(rax);
  __ SmiAddConstant(rax, rax, Smi::FromInt(1));
  frame_->EmitPush(rax);
  entry.Jump();

  // Cleanup.  No need to spill because VirtualFrame::Drop is safe for
  // any frame.
  node->break_target()->Bind();
  frame_->Drop(5);

  // Exit.
  exit.Bind();

  node->continue_target()->Unuse();
  node->break_target()->Unuse();
}


void CodeGenerator::VisitTryCatchStatement(TryCatchStatement* node) {
  ASSERT(!in_spilled_code());
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ TryCatchStatement");
  CodeForStatementPosition(node);

  JumpTarget try_block;
  JumpTarget exit;

  try_block.Call();
  // --- Catch block ---
  frame_->EmitPush(rax);

  // Store the caught exception in the catch variable.
  Variable* catch_var = node->catch_var()->var();
  ASSERT(catch_var != NULL && catch_var->slot() != NULL);
  StoreToSlot(catch_var->slot(), NOT_CONST_INIT);

  // Remove the exception from the stack.
  frame_->Drop();

  VisitStatementsAndSpill(node->catch_block()->statements());
  if (has_valid_frame()) {
    exit.Jump();
  }


  // --- Try block ---
  try_block.Bind();

  frame_->PushTryHandler(TRY_CATCH_HANDLER);
  int handler_height = frame_->height();

  // Shadow the jump targets for all escapes from the try block, including
  // returns.  During shadowing, the original target is hidden as the
  // ShadowTarget and operations on the original actually affect the
  // shadowing target.
  //
  // We should probably try to unify the escaping targets and the return
  // target.
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
  // After shadowing stops, the original targets are unshadowed and the
  // ShadowTargets represent the formerly shadowing targets.
  bool has_unlinks = false;
  for (int i = 0; i < shadows.length(); i++) {
    shadows[i]->StopShadowing();
    has_unlinks = has_unlinks || shadows[i]->is_linked();
  }
  function_return_is_shadowed_ = function_return_was_shadowed;

  // Get an external reference to the handler address.
  ExternalReference handler_address(Top::k_handler_address);

  // Make sure that there's nothing left on the stack above the
  // handler structure.
  if (FLAG_debug_code) {
    __ movq(kScratchRegister, handler_address);
    __ cmpq(rsp, Operand(kScratchRegister, 0));
    __ Assert(equal, "stack pointer should point to top handler");
  }

  // If we can fall off the end of the try block, unlink from try chain.
  if (has_valid_frame()) {
    // The next handler address is on top of the frame.  Unlink from
    // the handler list and drop the rest of this handler from the
    // frame.
    STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
    __ movq(kScratchRegister, handler_address);
    frame_->EmitPop(Operand(kScratchRegister, 0));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);
    if (has_unlinks) {
      exit.Jump();
    }
  }

  // Generate unlink code for the (formerly) shadowing targets that
  // have been jumped to.  Deallocate each shadow target.
  Result return_value;
  for (int i = 0; i < shadows.length(); i++) {
    if (shadows[i]->is_linked()) {
      // Unlink from try chain; be careful not to destroy the TOS if
      // there is one.
      if (i == kReturnShadowIndex) {
        shadows[i]->Bind(&return_value);
        return_value.ToRegister(rax);
      } else {
        shadows[i]->Bind();
      }
      // Because we can be jumping here (to spilled code) from
      // unspilled code, we need to reestablish a spilled frame at
      // this block.
      frame_->SpillAll();

      // Reload sp from the top handler, because some statements that we
      // break from (eg, for...in) may have left stuff on the stack.
      __ movq(kScratchRegister, handler_address);
      __ movq(rsp, Operand(kScratchRegister, 0));
      frame_->Forget(frame_->height() - handler_height);

      STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
      __ movq(kScratchRegister, handler_address);
      frame_->EmitPop(Operand(kScratchRegister, 0));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

      if (i == kReturnShadowIndex) {
        if (!function_return_is_shadowed_) frame_->PrepareForReturn();
        shadows[i]->other_target()->Jump(&return_value);
      } else {
        shadows[i]->other_target()->Jump();
      }
    }
  }

  exit.Bind();
}


void CodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* node) {
  ASSERT(!in_spilled_code());
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

  frame_->EmitPush(rax);
  // In case of thrown exceptions, this is where we continue.
  __ Move(rcx, Smi::FromInt(THROWING));
  finally_block.Jump();

  // --- Try block ---
  try_block.Bind();

  frame_->PushTryHandler(TRY_FINALLY_HANDLER);
  int handler_height = frame_->height();

  // Shadow the jump targets for all escapes from the try block, including
  // returns.  During shadowing, the original target is hidden as the
  // ShadowTarget and operations on the original actually affect the
  // shadowing target.
  //
  // We should probably try to unify the escaping targets and the return
  // target.
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
  // After shadowing stops, the original targets are unshadowed and the
  // ShadowTargets represent the formerly shadowing targets.
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
    STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
    __ movq(kScratchRegister, handler_address);
    frame_->EmitPop(Operand(kScratchRegister, 0));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

    // Fake a top of stack value (unneeded when FALLING) and set the
    // state in ecx, then jump around the unlink blocks if any.
    frame_->EmitPush(Heap::kUndefinedValueRootIndex);
    __ Move(rcx, Smi::FromInt(FALLING));
    if (nof_unlinks > 0) {
      finally_block.Jump();
    }
  }

  // Generate code to unlink and set the state for the (formerly)
  // shadowing targets that have been jumped to.
  for (int i = 0; i < shadows.length(); i++) {
    if (shadows[i]->is_linked()) {
      // If we have come from the shadowed return, the return value is
      // on the virtual frame.  We must preserve it until it is
      // pushed.
      if (i == kReturnShadowIndex) {
        Result return_value;
        shadows[i]->Bind(&return_value);
        return_value.ToRegister(rax);
      } else {
        shadows[i]->Bind();
      }
      // Because we can be jumping here (to spilled code) from
      // unspilled code, we need to reestablish a spilled frame at
      // this block.
      frame_->SpillAll();

      // Reload sp from the top handler, because some statements that
      // we break from (eg, for...in) may have left stuff on the
      // stack.
      __ movq(kScratchRegister, handler_address);
      __ movq(rsp, Operand(kScratchRegister, 0));
      frame_->Forget(frame_->height() - handler_height);

      // Unlink this handler and drop it from the frame.
      STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
      __ movq(kScratchRegister, handler_address);
      frame_->EmitPop(Operand(kScratchRegister, 0));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

      if (i == kReturnShadowIndex) {
        // If this target shadowed the function return, materialize
        // the return value on the stack.
        frame_->EmitPush(rax);
      } else {
        // Fake TOS for targets that shadowed breaks and continues.
        frame_->EmitPush(Heap::kUndefinedValueRootIndex);
      }
      __ Move(rcx, Smi::FromInt(JUMPING + i));
      if (--nof_unlinks > 0) {
        // If this is not the last unlink block, jump around the next.
        finally_block.Jump();
      }
    }
  }

  // --- Finally block ---
  finally_block.Bind();

  // Push the state on the stack.
  frame_->EmitPush(rcx);

  // We keep two elements on the stack - the (possibly faked) result
  // and the state - while evaluating the finally block.
  //
  // Generate code for the statements in the finally block.
  VisitStatementsAndSpill(node->finally_block()->statements());

  if (has_valid_frame()) {
    // Restore state and return value or faked TOS.
    frame_->EmitPop(rcx);
    frame_->EmitPop(rax);
  }

  // Generate code to jump to the right destination for all used
  // formerly shadowing targets.  Deallocate each shadow target.
  for (int i = 0; i < shadows.length(); i++) {
    if (has_valid_frame() && shadows[i]->is_bound()) {
      BreakTarget* original = shadows[i]->other_target();
      __ SmiCompare(rcx, Smi::FromInt(JUMPING + i));
      if (i == kReturnShadowIndex) {
        // The return value is (already) in rax.
        Result return_value = allocator_->Allocate(rax);
        ASSERT(return_value.is_valid());
        if (function_return_is_shadowed_) {
          original->Branch(equal, &return_value);
        } else {
          // Branch around the preparation for return which may emit
          // code.
          JumpTarget skip;
          skip.Branch(not_equal);
          frame_->PrepareForReturn();
          original->Jump(&return_value);
          skip.Bind();
        }
      } else {
        original->Branch(equal);
      }
    }
  }

  if (has_valid_frame()) {
    // Check if we need to rethrow the exception.
    JumpTarget exit;
    __ SmiCompare(rcx, Smi::FromInt(THROWING));
    exit.Branch(not_equal);

    // Rethrow exception.
    frame_->EmitPush(rax);  // undo pop from above
    frame_->CallRuntime(Runtime::kReThrow, 1);

    // Done.
    exit.Bind();
  }
}


void CodeGenerator::VisitDebuggerStatement(DebuggerStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ DebuggerStatement");
  CodeForStatementPosition(node);
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Spill everything, even constants, to the frame.
  frame_->SpillAll();

  frame_->DebugBreak();
  // Ignore the return value.
#endif
}


void CodeGenerator::InstantiateFunction(
    Handle<SharedFunctionInfo> function_info) {
  // The inevitable call will sync frame elements to memory anyway, so
  // we do it eagerly to allow us to push the arguments directly into
  // place.
  frame_->SyncRange(0, frame_->element_count() - 1);

  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && function_info->num_literals() == 0) {
    FastNewClosureStub stub;
    frame_->Push(function_info);
    Result answer = frame_->CallStub(&stub, 1);
    frame_->Push(&answer);
  } else {
    // Call the runtime to instantiate the function based on the
    // shared function info.
    frame_->EmitPush(rsi);
    frame_->EmitPush(function_info);
    Result result = frame_->CallRuntime(Runtime::kNewClosure, 2);
    frame_->Push(&result);
  }
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function info and instantiate it.
  Handle<SharedFunctionInfo> function_info =
      Compiler::BuildFunctionInfo(node, script(), this);
  // Check for stack-overflow exception.
  if (HasStackOverflow()) return;
  InstantiateFunction(function_info);
}


void CodeGenerator::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* node) {
  Comment cmnt(masm_, "[ SharedFunctionInfoLiteral");
  InstantiateFunction(node->shared_function_info());
}


void CodeGenerator::VisitConditional(Conditional* node) {
  Comment cmnt(masm_, "[ Conditional");
  JumpTarget then;
  JumpTarget else_;
  JumpTarget exit;
  ControlDestination dest(&then, &else_, true);
  LoadCondition(node->condition(), &dest, true);

  if (dest.false_was_fall_through()) {
    // The else target was bound, so we compile the else part first.
    Load(node->else_expression());

    if (then.is_linked()) {
      exit.Jump();
      then.Bind();
      Load(node->then_expression());
    }
  } else {
    // The then target was bound, so we compile the then part first.
    Load(node->then_expression());

    if (else_.is_linked()) {
      exit.Jump();
      else_.Bind();
      Load(node->else_expression());
    }
  }

  exit.Bind();
}


void CodeGenerator::LoadFromSlot(Slot* slot, TypeofState typeof_state) {
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    JumpTarget slow;
    JumpTarget done;
    Result value;

    // Generate fast case for loading from slots that correspond to
    // local/global variables or arguments unless they are shadowed by
    // eval-introduced bindings.
    EmitDynamicLoadFromSlotFastCase(slot,
                                    typeof_state,
                                    &value,
                                    &slow,
                                    &done);

    slow.Bind();
    // A runtime call is inevitable.  We eagerly sync frame elements
    // to memory so that we can push the arguments directly into place
    // on top of the frame.
    frame_->SyncRange(0, frame_->element_count() - 1);
    frame_->EmitPush(rsi);
    __ movq(kScratchRegister, slot->var()->name(), RelocInfo::EMBEDDED_OBJECT);
    frame_->EmitPush(kScratchRegister);
    if (typeof_state == INSIDE_TYPEOF) {
       value =
         frame_->CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    } else {
       value = frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    }

    done.Bind(&value);
    frame_->Push(&value);

  } else if (slot->var()->mode() == Variable::CONST) {
    // Const slots may contain 'the hole' value (the constant hasn't been
    // initialized yet) which needs to be converted into the 'undefined'
    // value.
    //
    // We currently spill the virtual frame because constants use the
    // potentially unsafe direct-frame access of SlotOperand.
    VirtualFrame::SpilledScope spilled_scope;
    Comment cmnt(masm_, "[ Load const");
    JumpTarget exit;
    __ movq(rcx, SlotOperand(slot, rcx));
    __ CompareRoot(rcx, Heap::kTheHoleValueRootIndex);
    exit.Branch(not_equal);
    __ LoadRoot(rcx, Heap::kUndefinedValueRootIndex);
    exit.Bind();
    frame_->EmitPush(rcx);

  } else if (slot->type() == Slot::PARAMETER) {
    frame_->PushParameterAt(slot->index());

  } else if (slot->type() == Slot::LOCAL) {
    frame_->PushLocalAt(slot->index());

  } else {
    // The other remaining slot types (LOOKUP and GLOBAL) cannot reach
    // here.
    //
    // The use of SlotOperand below is safe for an unspilled frame
    // because it will always be a context slot.
    ASSERT(slot->type() == Slot::CONTEXT);
    Result temp = allocator_->Allocate();
    ASSERT(temp.is_valid());
    __ movq(temp.reg(), SlotOperand(slot, temp.reg()));
    frame_->Push(&temp);
  }
}


void CodeGenerator::LoadFromSlotCheckForArguments(Slot* slot,
                                                  TypeofState state) {
  LoadFromSlot(slot, state);

  // Bail out quickly if we're not using lazy arguments allocation.
  if (ArgumentsMode() != LAZY_ARGUMENTS_ALLOCATION) return;

  // ... or if the slot isn't a non-parameter arguments slot.
  if (slot->type() == Slot::PARAMETER || !slot->is_arguments()) return;

  // Pop the loaded value from the stack.
  Result value = frame_->Pop();

  // If the loaded value is a constant, we know if the arguments
  // object has been lazily loaded yet.
  if (value.is_constant()) {
    if (value.handle()->IsTheHole()) {
      Result arguments = StoreArgumentsObject(false);
      frame_->Push(&arguments);
    } else {
      frame_->Push(&value);
    }
    return;
  }

  // The loaded value is in a register. If it is the sentinel that
  // indicates that we haven't loaded the arguments object yet, we
  // need to do it now.
  JumpTarget exit;
  __ CompareRoot(value.reg(), Heap::kTheHoleValueRootIndex);
  frame_->Push(&value);
  exit.Branch(not_equal);
  Result arguments = StoreArgumentsObject(false);
  frame_->SetElementAt(0, &arguments);
  exit.Bind();
}


Result CodeGenerator::LoadFromGlobalSlotCheckExtensions(
    Slot* slot,
    TypeofState typeof_state,
    JumpTarget* slow) {
  // Check that no extension objects have been created by calls to
  // eval from the current scope to the global scope.
  Register context = rsi;
  Result tmp = allocator_->Allocate();
  ASSERT(tmp.is_valid());  // All non-reserved registers were available.

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX),
               Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      // Load next context in chain.
      __ movq(tmp.reg(), ContextOperand(context, Context::CLOSURE_INDEX));
      __ movq(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp.reg();
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.  If we have reached an eval scope, we check
    // all extensions from this point.
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s->is_eval_scope()) {
    // Loop up the context chain.  There is no frame effect so it is
    // safe to use raw labels here.
    Label next, fast;
    if (!context.is(tmp.reg())) {
      __ movq(tmp.reg(), context);
    }
    // Load map for comparison into register, outside loop.
    __ LoadRoot(kScratchRegister, Heap::kGlobalContextMapRootIndex);
    __ bind(&next);
    // Terminate at global context.
    __ cmpq(kScratchRegister, FieldOperand(tmp.reg(), HeapObject::kMapOffset));
    __ j(equal, &fast);
    // Check that extension is NULL.
    __ cmpq(ContextOperand(tmp.reg(), Context::EXTENSION_INDEX), Immediate(0));
    slow->Branch(not_equal);
    // Load next context in chain.
    __ movq(tmp.reg(), ContextOperand(tmp.reg(), Context::CLOSURE_INDEX));
    __ movq(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
    __ jmp(&next);
    __ bind(&fast);
  }
  tmp.Unuse();

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  LoadGlobal();
  frame_->Push(slot->var()->name());
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
                         ? RelocInfo::CODE_TARGET
                         : RelocInfo::CODE_TARGET_CONTEXT;
  Result answer = frame_->CallLoadIC(mode);
  // A test rax instruction following the call signals that the inobject
  // property case was inlined.  Ensure that there is not a test rax
  // instruction here.
  masm_->nop();
  return answer;
}


void CodeGenerator::EmitDynamicLoadFromSlotFastCase(Slot* slot,
                                                    TypeofState typeof_state,
                                                    Result* result,
                                                    JumpTarget* slow,
                                                    JumpTarget* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  if (slot->var()->mode() == Variable::DYNAMIC_GLOBAL) {
    *result = LoadFromGlobalSlotCheckExtensions(slot, typeof_state, slow);
    done->Jump(result);

  } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
    Slot* potential_slot = slot->var()->local_if_not_shadowed()->slot();
    Expression* rewrite = slot->var()->local_if_not_shadowed()->rewrite();
    if (potential_slot != NULL) {
      // Generate fast case for locals that rewrite to slots.
      // Allocate a fresh register to use as a temp in
      // ContextSlotOperandCheckExtensions and to hold the result
      // value.
      *result = allocator_->Allocate();
      ASSERT(result->is_valid());
      __ movq(result->reg(),
              ContextSlotOperandCheckExtensions(potential_slot,
                                                *result,
                                                slow));
      if (potential_slot->var()->mode() == Variable::CONST) {
        __ CompareRoot(result->reg(), Heap::kTheHoleValueRootIndex);
        done->Branch(not_equal, result);
        __ LoadRoot(result->reg(), Heap::kUndefinedValueRootIndex);
      }
      done->Jump(result);
    } else if (rewrite != NULL) {
      // Generate fast case for argument loads.
      Property* property = rewrite->AsProperty();
      if (property != NULL) {
        VariableProxy* obj_proxy = property->obj()->AsVariableProxy();
        Literal* key_literal = property->key()->AsLiteral();
        if (obj_proxy != NULL &&
            key_literal != NULL &&
            obj_proxy->IsArguments() &&
            key_literal->handle()->IsSmi()) {
          // Load arguments object if there are no eval-introduced
          // variables. Then load the argument from the arguments
          // object using keyed load.
          Result arguments = allocator()->Allocate();
          ASSERT(arguments.is_valid());
          __ movq(arguments.reg(),
                  ContextSlotOperandCheckExtensions(obj_proxy->var()->slot(),
                                                    arguments,
                                                    slow));
          frame_->Push(&arguments);
          frame_->Push(key_literal->handle());
          *result = EmitKeyedLoad();
          done->Jump(result);
        }
      }
    }
  }
}


void CodeGenerator::StoreToSlot(Slot* slot, InitState init_state) {
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    // For now, just do a runtime call.  Since the call is inevitable,
    // we eagerly sync the virtual frame so we can directly push the
    // arguments into place.
    frame_->SyncRange(0, frame_->element_count() - 1);

    frame_->EmitPush(rsi);
    frame_->EmitPush(slot->var()->name());

    Result value;
    if (init_state == CONST_INIT) {
      // Same as the case for a normal store, but ignores attribute
      // (e.g. READ_ONLY) of context slot so that we can initialize const
      // properties (introduced via eval("const foo = (some expr);")). Also,
      // uses the current function context instead of the top context.
      //
      // Note that we must declare the foo upon entry of eval(), via a
      // context slot declaration, but we cannot initialize it at the same
      // time, because the const declaration may be at the end of the eval
      // code (sigh...) and the const variable may have been used before
      // (where its value is 'undefined'). Thus, we can only do the
      // initialization when we actually encounter the expression and when
      // the expression operands are defined and valid, and thus we need the
      // split into 2 operations: declaration of the context slot followed
      // by initialization.
      value = frame_->CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    } else {
      value = frame_->CallRuntime(Runtime::kStoreContextSlot, 3);
    }
    // Storing a variable must keep the (new) value on the expression
    // stack. This is necessary for compiling chained assignment
    // expressions.
    frame_->Push(&value);
  } else {
    ASSERT(!slot->var()->is_dynamic());

    JumpTarget exit;
    if (init_state == CONST_INIT) {
      ASSERT(slot->var()->mode() == Variable::CONST);
      // Only the first const initialization must be executed (the slot
      // still contains 'the hole' value). When the assignment is executed,
      // the code is identical to a normal store (see below).
      //
      // We spill the frame in the code below because the direct-frame
      // access of SlotOperand is potentially unsafe with an unspilled
      // frame.
      VirtualFrame::SpilledScope spilled_scope;
      Comment cmnt(masm_, "[ Init const");
      __ movq(rcx, SlotOperand(slot, rcx));
      __ CompareRoot(rcx, Heap::kTheHoleValueRootIndex);
      exit.Branch(not_equal);
    }

    // We must execute the store.  Storing a variable must keep the (new)
    // value on the stack. This is necessary for compiling assignment
    // expressions.
    //
    // Note: We will reach here even with slot->var()->mode() ==
    // Variable::CONST because of const declarations which will initialize
    // consts to 'the hole' value and by doing so, end up calling this code.
    if (slot->type() == Slot::PARAMETER) {
      frame_->StoreToParameterAt(slot->index());
    } else if (slot->type() == Slot::LOCAL) {
      frame_->StoreToLocalAt(slot->index());
    } else {
      // The other slot types (LOOKUP and GLOBAL) cannot reach here.
      //
      // The use of SlotOperand below is safe for an unspilled frame
      // because the slot is a context slot.
      ASSERT(slot->type() == Slot::CONTEXT);
      frame_->Dup();
      Result value = frame_->Pop();
      value.ToRegister();
      Result start = allocator_->Allocate();
      ASSERT(start.is_valid());
      __ movq(SlotOperand(slot, start.reg()), value.reg());
      // RecordWrite may destroy the value registers.
      //
      // TODO(204): Avoid actually spilling when the value is not
      // needed (probably the common case).
      frame_->Spill(value.reg());
      int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
      Result temp = allocator_->Allocate();
      ASSERT(temp.is_valid());
      __ RecordWrite(start.reg(), offset, value.reg(), temp.reg());
      // The results start, value, and temp are unused by going out of
      // scope.
    }

    exit.Bind();
  }
}


void CodeGenerator::VisitSlot(Slot* node) {
  Comment cmnt(masm_, "[ Slot");
  LoadFromSlotCheckForArguments(node, NOT_INSIDE_TYPEOF);
}


void CodeGenerator::VisitVariableProxy(VariableProxy* node) {
  Comment cmnt(masm_, "[ VariableProxy");
  Variable* var = node->var();
  Expression* expr = var->rewrite();
  if (expr != NULL) {
    Visit(expr);
  } else {
    ASSERT(var->is_global());
    Reference ref(this, node);
    ref.GetValue();
  }
}


void CodeGenerator::VisitLiteral(Literal* node) {
  Comment cmnt(masm_, "[ Literal");
  frame_->Push(node->handle());
}


void CodeGenerator::LoadUnsafeSmi(Register target, Handle<Object> value) {
  UNIMPLEMENTED();
  // TODO(X64): Implement security policy for loads of smis.
}


bool CodeGenerator::IsUnsafeSmi(Handle<Object> value) {
  return false;
}


// Materialize the regexp literal 'node' in the literals array
// 'literals' of the function.  Leave the regexp boilerplate in
// 'boilerplate'.
class DeferredRegExpLiteral: public DeferredCode {
 public:
  DeferredRegExpLiteral(Register boilerplate,
                        Register literals,
                        RegExpLiteral* node)
      : boilerplate_(boilerplate), literals_(literals), node_(node) {
    set_comment("[ DeferredRegExpLiteral");
  }

  void Generate();

 private:
  Register boilerplate_;
  Register literals_;
  RegExpLiteral* node_;
};


void DeferredRegExpLiteral::Generate() {
  // Since the entry is undefined we call the runtime system to
  // compute the literal.
  // Literal array (0).
  __ push(literals_);
  // Literal index (1).
  __ Push(Smi::FromInt(node_->literal_index()));
  // RegExp pattern (2).
  __ Push(node_->pattern());
  // RegExp flags (3).
  __ Push(node_->flags());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  if (!boilerplate_.is(rax)) __ movq(boilerplate_, rax);
}


class DeferredAllocateInNewSpace: public DeferredCode {
 public:
  DeferredAllocateInNewSpace(int size,
                             Register target,
                             int registers_to_save = 0)
    : size_(size), target_(target), registers_to_save_(registers_to_save) {
    ASSERT(size >= kPointerSize && size <= Heap::MaxObjectSizeInNewSpace());
    set_comment("[ DeferredAllocateInNewSpace");
  }
  void Generate();

 private:
  int size_;
  Register target_;
  int registers_to_save_;
};


void DeferredAllocateInNewSpace::Generate() {
  for (int i = 0; i < kNumRegs; i++) {
    if (registers_to_save_ & (1 << i)) {
      Register save_register = { i };
      __ push(save_register);
    }
  }
  __ Push(Smi::FromInt(size_));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  if (!target_.is(rax)) {
    __ movq(target_, rax);
  }
  for (int i = kNumRegs - 1; i >= 0; i--) {
    if (registers_to_save_ & (1 << i)) {
      Register save_register = { i };
      __ pop(save_register);
    }
  }
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
  Comment cmnt(masm_, "[ RegExp Literal");

  // Retrieve the literals array and check the allocated entry.  Begin
  // with a writable copy of the function of this activation in a
  // register.
  frame_->PushFunction();
  Result literals = frame_->Pop();
  literals.ToRegister();
  frame_->Spill(literals.reg());

  // Load the literals array of the function.
  __ movq(literals.reg(),
          FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  Result boilerplate = allocator_->Allocate();
  ASSERT(boilerplate.is_valid());
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ movq(boilerplate.reg(), FieldOperand(literals.reg(), literal_offset));

  // Check whether we need to materialize the RegExp object.  If so,
  // jump to the deferred code passing the literals array.
  DeferredRegExpLiteral* deferred =
      new DeferredRegExpLiteral(boilerplate.reg(), literals.reg(), node);
  __ CompareRoot(boilerplate.reg(), Heap::kUndefinedValueRootIndex);
  deferred->Branch(equal);
  deferred->BindExit();

  // Register of boilerplate contains RegExp object.

  Result tmp = allocator()->Allocate();
  ASSERT(tmp.is_valid());

  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;

  DeferredAllocateInNewSpace* allocate_fallback =
      new DeferredAllocateInNewSpace(size, literals.reg());
  frame_->Push(&boilerplate);
  frame_->SpillTop();
  __ AllocateInNewSpace(size,
                        literals.reg(),
                        tmp.reg(),
                        no_reg,
                        allocate_fallback->entry_label(),
                        TAG_OBJECT);
  allocate_fallback->BindExit();
  boilerplate = frame_->Pop();
  // Copy from boilerplate to clone and return clone.

  for (int i = 0; i < size; i += kPointerSize) {
    __ movq(tmp.reg(), FieldOperand(boilerplate.reg(), i));
    __ movq(FieldOperand(literals.reg(), i), tmp.reg());
  }
  frame_->Push(&literals);
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  // Load a writable copy of the function of this activation in a
  // register.
  frame_->PushFunction();
  Result literals = frame_->Pop();
  literals.ToRegister();
  frame_->Spill(literals.reg());

  // Load the literals array of the function.
  __ movq(literals.reg(),
          FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));
  // Literal array.
  frame_->Push(&literals);
  // Literal index.
  frame_->Push(Smi::FromInt(node->literal_index()));
  // Constant properties.
  frame_->Push(node->constant_properties());
  // Should the object literal have fast elements?
  frame_->Push(Smi::FromInt(node->fast_elements() ? 1 : 0));
  Result clone;
  if (node->depth() > 1) {
    clone = frame_->CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    clone = frame_->CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }
  frame_->Push(&clone);

  for (int i = 0; i < node->properties()->length(); i++) {
    ObjectLiteral::Property* property = node->properties()->at(i);
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        break;
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        if (CompileTimeValue::IsCompileTimeValue(property->value())) break;
        // else fall through.
      case ObjectLiteral::Property::COMPUTED: {
        Handle<Object> key(property->key()->handle());
        if (key->IsSymbol()) {
          // Duplicate the object as the IC receiver.
          frame_->Dup();
          Load(property->value());
          Result ignored =
              frame_->CallStoreIC(Handle<String>::cast(key), false);
          // A test rax instruction following the store IC call would
          // indicate the presence of an inlined version of the
          // store. Add a nop to indicate that there is no such
          // inlined version.
          __ nop();
          break;
        }
        // Fall through
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        // Duplicate the object as an argument to the runtime call.
        frame_->Dup();
        Load(property->key());
        Load(property->value());
        Result ignored = frame_->CallRuntime(Runtime::kSetProperty, 3);
        // Ignore the result.
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        // Duplicate the object as an argument to the runtime call.
        frame_->Dup();
        Load(property->key());
        frame_->Push(Smi::FromInt(1));
        Load(property->value());
        Result ignored = frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        // Ignore the result.
        break;
      }
      case ObjectLiteral::Property::GETTER: {
        // Duplicate the object as an argument to the runtime call.
        frame_->Dup();
        Load(property->key());
        frame_->Push(Smi::FromInt(0));
        Load(property->value());
        Result ignored = frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        // Ignore the result.
        break;
      }
      default: UNREACHABLE();
    }
  }
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  // Load a writable copy of the function of this activation in a
  // register.
  frame_->PushFunction();
  Result literals = frame_->Pop();
  literals.ToRegister();
  frame_->Spill(literals.reg());

  // Load the literals array of the function.
  __ movq(literals.reg(),
          FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));

  frame_->Push(&literals);
  frame_->Push(Smi::FromInt(node->literal_index()));
  frame_->Push(node->constant_elements());
  int length = node->values()->length();
  Result clone;
  if (node->constant_elements()->map() == Heap::fixed_cow_array_map()) {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS, length);
    clone = frame_->CallStub(&stub, 3);
    __ IncrementCounter(&Counters::cow_arrays_created_stub, 1);
  } else if (node->depth() > 1) {
    clone = frame_->CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    clone = frame_->CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::CLONE_ELEMENTS, length);
    clone = frame_->CallStub(&stub, 3);
  }
  frame_->Push(&clone);

  // Generate code to set the elements in the array that are not
  // literals.
  for (int i = 0; i < length; i++) {
    Expression* value = node->values()->at(i);

    if (!CompileTimeValue::ArrayLiteralElementNeedsInitialization(value)) {
      continue;
    }

    // The property must be set by generated code.
    Load(value);

    // Get the property value off the stack.
    Result prop_value = frame_->Pop();
    prop_value.ToRegister();

    // Fetch the array literal while leaving a copy on the stack and
    // use it to get the elements array.
    frame_->Dup();
    Result elements = frame_->Pop();
    elements.ToRegister();
    frame_->Spill(elements.reg());
    // Get the elements FixedArray.
    __ movq(elements.reg(),
            FieldOperand(elements.reg(), JSObject::kElementsOffset));

    // Write to the indexed properties array.
    int offset = i * kPointerSize + FixedArray::kHeaderSize;
    __ movq(FieldOperand(elements.reg(), offset), prop_value.reg());

    // Update the write barrier for the array address.
    frame_->Spill(prop_value.reg());  // Overwritten by the write barrier.
    Result scratch = allocator_->Allocate();
    ASSERT(scratch.is_valid());
    __ RecordWrite(elements.reg(), offset, prop_value.reg(), scratch.reg());
  }
}


void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* node) {
  ASSERT(!in_spilled_code());
  // Call runtime routine to allocate the catch extension object and
  // assign the exception value to the catch variable.
  Comment cmnt(masm_, "[ CatchExtensionObject");
  Load(node->key());
  Load(node->value());
  Result result =
      frame_->CallRuntime(Runtime::kCreateCatchExtensionObject, 2);
  frame_->Push(&result);
}


void CodeGenerator::EmitSlotAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Comment cmnt(masm(), "[ Variable Assignment");
  Variable* var = node->target()->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL);
  Slot* slot = var->slot();
  ASSERT(slot != NULL);

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // For a compound assignment the right-hand side is a binary operation
    // between the current property value and the actual right-hand side.
    LoadFromSlotCheckForArguments(slot, NOT_INSIDE_TYPEOF);
    Load(node->value());

    // Perform the binary operation.
    bool overwrite_value = node->value()->ResultOverwriteAllowed();
    // Construct the implicit binary operation.
    BinaryOperation expr(node);
    GenericBinaryOperation(&expr,
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
    // For non-compound assignment just load the right-hand side.
    Load(node->value());
  }

  // Perform the assignment.
  if (var->mode() != Variable::CONST || node->op() == Token::INIT_CONST) {
    CodeForSourcePosition(node->position());
    StoreToSlot(slot,
                node->op() == Token::INIT_CONST ? CONST_INIT : NOT_CONST_INIT);
  }
  ASSERT(frame()->height() == original_height + 1);
}


void CodeGenerator::EmitNamedPropertyAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Comment cmnt(masm(), "[ Named Property Assignment");
  Variable* var = node->target()->AsVariableProxy()->AsVariable();
  Property* prop = node->target()->AsProperty();
  ASSERT(var == NULL || (prop == NULL && var->is_global()));

  // Initialize name and evaluate the receiver sub-expression if necessary. If
  // the receiver is trivial it is not placed on the stack at this point, but
  // loaded whenever actually needed.
  Handle<String> name;
  bool is_trivial_receiver = false;
  if (var != NULL) {
    name = var->name();
  } else {
    Literal* lit = prop->key()->AsLiteral();
    ASSERT_NOT_NULL(lit);
    name = Handle<String>::cast(lit->handle());
    // Do not materialize the receiver on the frame if it is trivial.
    is_trivial_receiver = prop->obj()->IsTrivial();
    if (!is_trivial_receiver) Load(prop->obj());
  }

  // Change to slow case in the beginning of an initialization block to
  // avoid the quadratic behavior of repeatedly adding fast properties.
  if (node->starts_initialization_block()) {
    // Initialization block consists of assignments of the form expr.x = ..., so
    // this will never be an assignment to a variable, so there must be a
    // receiver object.
    ASSERT_EQ(NULL, var);
    if (is_trivial_receiver) {
      frame()->Push(prop->obj());
    } else {
      frame()->Dup();
    }
    Result ignored = frame()->CallRuntime(Runtime::kToSlowProperties, 1);
  }

  // Change to fast case at the end of an initialization block. To prepare for
  // that add an extra copy of the receiver to the frame, so that it can be
  // converted back to fast case after the assignment.
  if (node->ends_initialization_block() && !is_trivial_receiver) {
    frame()->Dup();
  }

  // Stack layout:
  // [tos]   : receiver (only materialized if non-trivial)
  // [tos+1] : receiver if at the end of an initialization block

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // For a compound assignment the right-hand side is a binary operation
    // between the current property value and the actual right-hand side.
    if (is_trivial_receiver) {
      frame()->Push(prop->obj());
    } else if (var != NULL) {
      // The LoadIC stub expects the object in rax.
      // Freeing rax causes the code generator to load the global into it.
      frame_->Spill(rax);
      LoadGlobal();
    } else {
      frame()->Dup();
    }
    Result value = EmitNamedLoad(name, var != NULL);
    frame()->Push(&value);
    Load(node->value());

    bool overwrite_value = node->value()->ResultOverwriteAllowed();
    // Construct the implicit binary operation.
    BinaryOperation expr(node);
    GenericBinaryOperation(&expr,
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
    // For non-compound assignment just load the right-hand side.
    Load(node->value());
  }

  // Stack layout:
  // [tos]   : value
  // [tos+1] : receiver (only materialized if non-trivial)
  // [tos+2] : receiver if at the end of an initialization block

  // Perform the assignment.  It is safe to ignore constants here.
  ASSERT(var == NULL || var->mode() != Variable::CONST);
  ASSERT_NE(Token::INIT_CONST, node->op());
  if (is_trivial_receiver) {
    Result value = frame()->Pop();
    frame()->Push(prop->obj());
    frame()->Push(&value);
  }
  CodeForSourcePosition(node->position());
  bool is_contextual = (var != NULL);
  Result answer = EmitNamedStore(name, is_contextual);
  frame()->Push(&answer);

  // Stack layout:
  // [tos]   : result
  // [tos+1] : receiver if at the end of an initialization block

  if (node->ends_initialization_block()) {
    ASSERT_EQ(NULL, var);
    // The argument to the runtime call is the receiver.
    if (is_trivial_receiver) {
      frame()->Push(prop->obj());
    } else {
      // A copy of the receiver is below the value of the assignment.  Swap
      // the receiver and the value of the assignment expression.
      Result result = frame()->Pop();
      Result receiver = frame()->Pop();
      frame()->Push(&result);
      frame()->Push(&receiver);
    }
    Result ignored = frame_->CallRuntime(Runtime::kToFastProperties, 1);
  }

  // Stack layout:
  // [tos]   : result

  ASSERT_EQ(frame()->height(), original_height + 1);
}


void CodeGenerator::EmitKeyedPropertyAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Comment cmnt(masm_, "[ Keyed Property Assignment");
  Property* prop = node->target()->AsProperty();
  ASSERT_NOT_NULL(prop);

  // Evaluate the receiver subexpression.
  Load(prop->obj());

  // Change to slow case in the beginning of an initialization block to
  // avoid the quadratic behavior of repeatedly adding fast properties.
  if (node->starts_initialization_block()) {
    frame_->Dup();
    Result ignored = frame_->CallRuntime(Runtime::kToSlowProperties, 1);
  }

  // Change to fast case at the end of an initialization block. To prepare for
  // that add an extra copy of the receiver to the frame, so that it can be
  // converted back to fast case after the assignment.
  if (node->ends_initialization_block()) {
    frame_->Dup();
  }

  // Evaluate the key subexpression.
  Load(prop->key());

  // Stack layout:
  // [tos]   : key
  // [tos+1] : receiver
  // [tos+2] : receiver if at the end of an initialization block

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // For a compound assignment the right-hand side is a binary operation
    // between the current property value and the actual right-hand side.
    // Duplicate receiver and key for loading the current property value.
    frame()->PushElementAt(1);
    frame()->PushElementAt(1);
    Result value = EmitKeyedLoad();
    frame()->Push(&value);
    Load(node->value());

    // Perform the binary operation.
    bool overwrite_value = node->value()->ResultOverwriteAllowed();
    BinaryOperation expr(node);
    GenericBinaryOperation(&expr,
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
    // For non-compound assignment just load the right-hand side.
    Load(node->value());
  }

  // Stack layout:
  // [tos]   : value
  // [tos+1] : key
  // [tos+2] : receiver
  // [tos+3] : receiver if at the end of an initialization block

  // Perform the assignment.  It is safe to ignore constants here.
  ASSERT(node->op() != Token::INIT_CONST);
  CodeForSourcePosition(node->position());
  Result answer = EmitKeyedStore(prop->key()->type());
  frame()->Push(&answer);

  // Stack layout:
  // [tos]   : result
  // [tos+1] : receiver if at the end of an initialization block

  // Change to fast case at the end of an initialization block.
  if (node->ends_initialization_block()) {
    // The argument to the runtime call is the extra copy of the receiver,
    // which is below the value of the assignment.  Swap the receiver and
    // the value of the assignment expression.
    Result result = frame()->Pop();
    Result receiver = frame()->Pop();
    frame()->Push(&result);
    frame()->Push(&receiver);
    Result ignored = frame_->CallRuntime(Runtime::kToFastProperties, 1);
  }

  // Stack layout:
  // [tos]   : result

  ASSERT(frame()->height() == original_height + 1);
}


void CodeGenerator::VisitAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Variable* var = node->target()->AsVariableProxy()->AsVariable();
  Property* prop = node->target()->AsProperty();

  if (var != NULL && !var->is_global()) {
    EmitSlotAssignment(node);

  } else if ((prop != NULL && prop->key()->IsPropertyName()) ||
             (var != NULL && var->is_global())) {
    // Properties whose keys are property names and global variables are
    // treated as named property references.  We do not need to consider
    // global 'this' because it is not a valid left-hand side.
    EmitNamedPropertyAssignment(node);

  } else if (prop != NULL) {
    // Other properties (including rewritten parameters for a function that
    // uses arguments) are keyed property assignments.
    EmitKeyedPropertyAssignment(node);

  } else {
    // Invalid left-hand side.
    Load(node->target());
    Result result = frame()->CallRuntime(Runtime::kThrowReferenceError, 1);
    // The runtime call doesn't actually return but the code generator will
    // still generate code and expects a certain frame height.
    frame()->Push(&result);
  }

  ASSERT(frame()->height() == original_height + 1);
}


void CodeGenerator::VisitThrow(Throw* node) {
  Comment cmnt(masm_, "[ Throw");
  Load(node->exception());
  Result result = frame_->CallRuntime(Runtime::kThrow, 1);
  frame_->Push(&result);
}


void CodeGenerator::VisitProperty(Property* node) {
  Comment cmnt(masm_, "[ Property");
  Reference property(this, node);
  property.GetValue();
}


void CodeGenerator::VisitCall(Call* node) {
  Comment cmnt(masm_, "[ Call");

  ZoneList<Expression*>* args = node->arguments();

  // Check if the function is a variable or a property.
  Expression* function = node->expression();
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

    // Prepare the stack for the call to the resolved function.
    Load(function);

    // Allocate a frame slot for the receiver.
    frame_->Push(Factory::undefined_value());

    // Load the arguments.
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      Load(args->at(i));
      frame_->SpillTop();
    }

    // Result to hold the result of the function resolution and the
    // final result of the eval call.
    Result result;

    // If we know that eval can only be shadowed by eval-introduced
    // variables we attempt to load the global eval function directly
    // in generated code. If we succeed, there is no need to perform a
    // context lookup in the runtime system.
    JumpTarget done;
    if (var->slot() != NULL && var->mode() == Variable::DYNAMIC_GLOBAL) {
      ASSERT(var->slot()->type() == Slot::LOOKUP);
      JumpTarget slow;
      // Prepare the stack for the call to
      // ResolvePossiblyDirectEvalNoLookup by pushing the loaded
      // function, the first argument to the eval call and the
      // receiver.
      Result fun = LoadFromGlobalSlotCheckExtensions(var->slot(),
                                                     NOT_INSIDE_TYPEOF,
                                                     &slow);
      frame_->Push(&fun);
      if (arg_count > 0) {
        frame_->PushElementAt(arg_count);
      } else {
        frame_->Push(Factory::undefined_value());
      }
      frame_->PushParameterAt(-1);

      // Resolve the call.
      result =
          frame_->CallRuntime(Runtime::kResolvePossiblyDirectEvalNoLookup, 3);

      done.Jump(&result);
      slow.Bind();
    }

    // Prepare the stack for the call to ResolvePossiblyDirectEval by
    // pushing the loaded function, the first argument to the eval
    // call and the receiver.
    frame_->PushElementAt(arg_count + 1);
    if (arg_count > 0) {
      frame_->PushElementAt(arg_count);
    } else {
      frame_->Push(Factory::undefined_value());
    }
    frame_->PushParameterAt(-1);

    // Resolve the call.
    result = frame_->CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // If we generated fast-case code bind the jump-target where fast
    // and slow case merge.
    if (done.is_linked()) done.Bind(&result);

    // The runtime call returns a pair of values in rax (function) and
    // rdx (receiver). Touch up the stack with the right values.
    Result receiver = allocator_->Allocate(rdx);
    frame_->SetElementAt(arg_count + 1, &result);
    frame_->SetElementAt(arg_count, &receiver);
    receiver.Unuse();

    // Call the function.
    CodeForSourcePosition(node->position());
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub call_function(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    result = frame_->CallStub(&call_function, arg_count + 1);

    // Restore the context and overwrite the function on the stack with
    // the result.
    frame_->RestoreContextRegister();
    frame_->SetElementAt(0, &result);

  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is global
    // ----------------------------------

    // Pass the global object as the receiver and let the IC stub
    // patch the stack to use the global proxy as 'this' in the
    // invoked function.
    LoadGlobal();

    // Load the arguments.
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      Load(args->at(i));
      frame_->SpillTop();
    }

    // Push the name of the function on the frame.
    frame_->Push(var->name());

    // Call the IC initialization code.
    CodeForSourcePosition(node->position());
    Result result = frame_->CallCallIC(RelocInfo::CODE_TARGET_CONTEXT,
                                       arg_count,
                                       loop_nesting());
    frame_->RestoreContextRegister();
    // Replace the function on the stack with the result.
    frame_->Push(&result);

  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // ----------------------------------
    // JavaScript examples:
    //
    //  with (obj) foo(1, 2, 3)  // foo may be in obj.
    //
    //  function f() {};
    //  function g() {
    //    eval(...);
    //    f();  // f could be in extension object.
    //  }
    // ----------------------------------

    JumpTarget slow, done;
    Result function;

    // Generate fast case for loading functions from slots that
    // correspond to local/global variables or arguments unless they
    // are shadowed by eval-introduced bindings.
    EmitDynamicLoadFromSlotFastCase(var->slot(),
                                    NOT_INSIDE_TYPEOF,
                                    &function,
                                    &slow,
                                    &done);

    slow.Bind();
    // Load the function from the context.  Sync the frame so we can
    // push the arguments directly into place.
    frame_->SyncRange(0, frame_->element_count() - 1);
    frame_->EmitPush(rsi);
    frame_->EmitPush(var->name());
    frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    // The runtime call returns a pair of values in rax and rdx.  The
    // looked-up function is in rax and the receiver is in rdx.  These
    // register references are not ref counted here.  We spill them
    // eagerly since they are arguments to an inevitable call (and are
    // not sharable by the arguments).
    ASSERT(!allocator()->is_used(rax));
    frame_->EmitPush(rax);

    // Load the receiver.
    ASSERT(!allocator()->is_used(rdx));
    frame_->EmitPush(rdx);

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      JumpTarget call;
      call.Jump();
      done.Bind(&function);
      frame_->Push(&function);
      LoadGlobalReceiver();
      call.Bind();
    }

    // Call the function.
    CallWithArguments(args, NO_CALL_FUNCTION_FLAGS, node->position());

  } else if (property != NULL) {
    // Check if the key is a literal string.
    Literal* literal = property->key()->AsLiteral();

    if (literal != NULL && literal->handle()->IsSymbol()) {
      // ------------------------------------------------------------------
      // JavaScript example: 'object.foo(1, 2, 3)' or 'map["key"](1, 2, 3)'
      // ------------------------------------------------------------------

      Handle<String> name = Handle<String>::cast(literal->handle());

      if (ArgumentsMode() == LAZY_ARGUMENTS_ALLOCATION &&
          name->IsEqualTo(CStrVector("apply")) &&
          args->length() == 2 &&
          args->at(1)->AsVariableProxy() != NULL &&
          args->at(1)->AsVariableProxy()->IsArguments()) {
        // Use the optimized Function.prototype.apply that avoids
        // allocating lazily allocated arguments objects.
        CallApplyLazy(property->obj(),
                      args->at(0),
                      args->at(1)->AsVariableProxy(),
                      node->position());

      } else {
        // Push the receiver onto the frame.
        Load(property->obj());

        // Load the arguments.
        int arg_count = args->length();
        for (int i = 0; i < arg_count; i++) {
          Load(args->at(i));
          frame_->SpillTop();
        }

        // Push the name of the function onto the frame.
        frame_->Push(name);

        // Call the IC initialization code.
        CodeForSourcePosition(node->position());
        Result result = frame_->CallCallIC(RelocInfo::CODE_TARGET,
                                           arg_count,
                                           loop_nesting());
        frame_->RestoreContextRegister();
        frame_->Push(&result);
      }

    } else {
      // -------------------------------------------
      // JavaScript example: 'array[index](1, 2, 3)'
      // -------------------------------------------

      // Load the function to call from the property through a reference.
      if (property->is_synthetic()) {
        Reference ref(this, property, false);
        ref.GetValue();
        // Use global object as receiver.
        LoadGlobalReceiver();
       // Call the function.
        CallWithArguments(args, RECEIVER_MIGHT_BE_VALUE, node->position());
      } else {
        // Push the receiver onto the frame.
        Load(property->obj());

        // Load the arguments.
        int arg_count = args->length();
        for (int i = 0; i < arg_count; i++) {
          Load(args->at(i));
          frame_->SpillTop();
        }

        // Load the name of the function.
        Load(property->key());

        // Call the IC initialization code.
        CodeForSourcePosition(node->position());
        Result result = frame_->CallKeyedCallIC(RelocInfo::CODE_TARGET,
                                                arg_count,
                                                loop_nesting());
        frame_->RestoreContextRegister();
        frame_->Push(&result);
      }
    }
  } else {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is not global
    // ----------------------------------

    // Load the function.
    Load(function);

    // Pass the global proxy as the receiver.
    LoadGlobalReceiver();

    // Call the function.
    CallWithArguments(args, NO_CALL_FUNCTION_FLAGS, node->position());
  }
}


void CodeGenerator::VisitCallNew(CallNew* node) {
  Comment cmnt(masm_, "[ CallNew");

  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments. This is different from ordinary calls, where the
  // actual function to call is resolved after the arguments have been
  // evaluated.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.
  Load(node->expression());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = node->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  CodeForSourcePosition(node->position());
  Result result = frame_->CallConstructor(arg_count);
  frame_->Push(&result);
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  Condition is_smi = masm_->CheckSmi(value.reg());
  value.Unuse();
  destination()->Split(is_smi);
}


void CodeGenerator::GenerateLog(ZoneList<Expression*>* args) {
  // Conditionally generate a log call.
  // Args:
  //   0 (literal string): The type of logging (corresponds to the flags).
  //     This is used to determine whether or not to generate the log call.
  //   1 (string): Format string.  Access the string at argument index 2
  //     with '%2s' (see Logger::LogRuntime for all the formats).
  //   2 (array): Arguments to the format string.
  ASSERT_EQ(args->length(), 3);
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (ShouldGenerateLog(args->at(0))) {
    Load(args->at(1));
    Load(args->at(2));
    frame_->CallRuntime(Runtime::kLog, 2);
  }
#endif
  // Finally, we're expected to leave a value on the top of the stack.
  frame_->Push(Factory::undefined_value());
}


void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  Condition positive_smi = masm_->CheckPositiveSmi(value.reg());
  value.Unuse();
  destination()->Split(positive_smi);
}


class DeferredStringCharCodeAt : public DeferredCode {
 public:
  DeferredStringCharCodeAt(Register object,
                           Register index,
                           Register scratch,
                           Register result)
      : result_(result),
        char_code_at_generator_(object,
                                index,
                                scratch,
                                result,
                                &need_conversion_,
                                &need_conversion_,
                                &index_out_of_range_,
                                STRING_INDEX_IS_NUMBER) {}

  StringCharCodeAtGenerator* fast_case_generator() {
    return &char_code_at_generator_;
  }

  virtual void Generate() {
    VirtualFrameRuntimeCallHelper call_helper(frame_state());
    char_code_at_generator_.GenerateSlow(masm(), call_helper);

    __ bind(&need_conversion_);
    // Move the undefined value into the result register, which will
    // trigger conversion.
    __ LoadRoot(result_, Heap::kUndefinedValueRootIndex);
    __ jmp(exit_label());

    __ bind(&index_out_of_range_);
    // When the index is out of range, the spec requires us to return
    // NaN.
    __ LoadRoot(result_, Heap::kNanValueRootIndex);
    __ jmp(exit_label());
  }

 private:
  Register result_;

  Label need_conversion_;
  Label index_out_of_range_;

  StringCharCodeAtGenerator char_code_at_generator_;
};


// This generates code that performs a String.prototype.charCodeAt() call
// or returns a smi in order to trigger conversion.
void CodeGenerator::GenerateStringCharCodeAt(ZoneList<Expression*>* args) {
  Comment(masm_, "[ GenerateStringCharCodeAt");
  ASSERT(args->length() == 2);

  Load(args->at(0));
  Load(args->at(1));
  Result index = frame_->Pop();
  Result object = frame_->Pop();
  object.ToRegister();
  index.ToRegister();
  // We might mutate the object register.
  frame_->Spill(object.reg());

  // We need two extra registers.
  Result result = allocator()->Allocate();
  ASSERT(result.is_valid());
  Result scratch = allocator()->Allocate();
  ASSERT(scratch.is_valid());

  DeferredStringCharCodeAt* deferred =
      new DeferredStringCharCodeAt(object.reg(),
                                   index.reg(),
                                   scratch.reg(),
                                   result.reg());
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->Push(&result);
}


class DeferredStringCharFromCode : public DeferredCode {
 public:
  DeferredStringCharFromCode(Register code,
                             Register result)
      : char_from_code_generator_(code, result) {}

  StringCharFromCodeGenerator* fast_case_generator() {
    return &char_from_code_generator_;
  }

  virtual void Generate() {
    VirtualFrameRuntimeCallHelper call_helper(frame_state());
    char_from_code_generator_.GenerateSlow(masm(), call_helper);
  }

 private:
  StringCharFromCodeGenerator char_from_code_generator_;
};


// Generates code for creating a one-char string from a char code.
void CodeGenerator::GenerateStringCharFromCode(ZoneList<Expression*>* args) {
  Comment(masm_, "[ GenerateStringCharFromCode");
  ASSERT(args->length() == 1);

  Load(args->at(0));

  Result code = frame_->Pop();
  code.ToRegister();
  ASSERT(code.is_valid());

  Result result = allocator()->Allocate();
  ASSERT(result.is_valid());

  DeferredStringCharFromCode* deferred = new DeferredStringCharFromCode(
      code.reg(), result.reg());
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->Push(&result);
}


class DeferredStringCharAt : public DeferredCode {
 public:
  DeferredStringCharAt(Register object,
                       Register index,
                       Register scratch1,
                       Register scratch2,
                       Register result)
      : result_(result),
        char_at_generator_(object,
                           index,
                           scratch1,
                           scratch2,
                           result,
                           &need_conversion_,
                           &need_conversion_,
                           &index_out_of_range_,
                           STRING_INDEX_IS_NUMBER) {}

  StringCharAtGenerator* fast_case_generator() {
    return &char_at_generator_;
  }

  virtual void Generate() {
    VirtualFrameRuntimeCallHelper call_helper(frame_state());
    char_at_generator_.GenerateSlow(masm(), call_helper);

    __ bind(&need_conversion_);
    // Move smi zero into the result register, which will trigger
    // conversion.
    __ Move(result_, Smi::FromInt(0));
    __ jmp(exit_label());

    __ bind(&index_out_of_range_);
    // When the index is out of range, the spec requires us to return
    // the empty string.
    __ LoadRoot(result_, Heap::kEmptyStringRootIndex);
    __ jmp(exit_label());
  }

 private:
  Register result_;

  Label need_conversion_;
  Label index_out_of_range_;

  StringCharAtGenerator char_at_generator_;
};


// This generates code that performs a String.prototype.charAt() call
// or returns a smi in order to trigger conversion.
void CodeGenerator::GenerateStringCharAt(ZoneList<Expression*>* args) {
  Comment(masm_, "[ GenerateStringCharAt");
  ASSERT(args->length() == 2);

  Load(args->at(0));
  Load(args->at(1));
  Result index = frame_->Pop();
  Result object = frame_->Pop();
  object.ToRegister();
  index.ToRegister();
  // We might mutate the object register.
  frame_->Spill(object.reg());

  // We need three extra registers.
  Result result = allocator()->Allocate();
  ASSERT(result.is_valid());
  Result scratch1 = allocator()->Allocate();
  ASSERT(scratch1.is_valid());
  Result scratch2 = allocator()->Allocate();
  ASSERT(scratch2.is_valid());

  DeferredStringCharAt* deferred =
      new DeferredStringCharAt(object.reg(),
                               index.reg(),
                               scratch1.reg(),
                               scratch2.reg(),
                               result.reg());
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->Push(&result);
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  Condition is_smi = masm_->CheckSmi(value.reg());
  destination()->false_target()->Branch(is_smi);
  // It is a heap object - get map.
  // Check if the object is a JS array or not.
  __ CmpObjectType(value.reg(), JS_ARRAY_TYPE, kScratchRegister);
  value.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  Condition is_smi = masm_->CheckSmi(value.reg());
  destination()->false_target()->Branch(is_smi);
  // It is a heap object - get map.
  // Check if the object is a regexp.
  __ CmpObjectType(value.reg(), JS_REGEXP_TYPE, kScratchRegister);
  value.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsObject(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (typeof(arg) === 'object' || %_ClassOf(arg) == 'RegExp')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();
  Condition is_smi = masm_->CheckSmi(obj.reg());
  destination()->false_target()->Branch(is_smi);

  __ Move(kScratchRegister, Factory::null_value());
  __ cmpq(obj.reg(), kScratchRegister);
  destination()->true_target()->Branch(equal);

  __ movq(kScratchRegister, FieldOperand(obj.reg(), HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ testb(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
          Immediate(1 << Map::kIsUndetectable));
  destination()->false_target()->Branch(not_zero);
  __ movzxbq(kScratchRegister,
             FieldOperand(kScratchRegister, Map::kInstanceTypeOffset));
  __ cmpq(kScratchRegister, Immediate(FIRST_JS_OBJECT_TYPE));
  destination()->false_target()->Branch(below);
  __ cmpq(kScratchRegister, Immediate(LAST_JS_OBJECT_TYPE));
  obj.Unuse();
  destination()->Split(below_equal);
}


void CodeGenerator::GenerateIsSpecObject(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (typeof(arg) === 'object' || %_ClassOf(arg) == 'RegExp' ||
  // typeof(arg) == function).
  // It includes undetectable objects (as opposed to IsObject).
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  Condition is_smi = masm_->CheckSmi(value.reg());
  destination()->false_target()->Branch(is_smi);
  // Check that this is an object.
  __ CmpObjectType(value.reg(), FIRST_JS_OBJECT_TYPE, kScratchRegister);
  value.Unuse();
  destination()->Split(above_equal);
}


// Deferred code to check whether the String JavaScript object is safe for using
// default value of. This code is called after the bit caching this information
// in the map has been checked with the map for the object in the map_result_
// register. On return the register map_result_ contains 1 for true and 0 for
// false.
class DeferredIsStringWrapperSafeForDefaultValueOf : public DeferredCode {
 public:
  DeferredIsStringWrapperSafeForDefaultValueOf(Register object,
                                               Register map_result,
                                               Register scratch1,
                                               Register scratch2)
      : object_(object),
        map_result_(map_result),
        scratch1_(scratch1),
        scratch2_(scratch2) { }

  virtual void Generate() {
    Label false_result;

    // Check that map is loaded as expected.
    if (FLAG_debug_code) {
      __ cmpq(map_result_, FieldOperand(object_, HeapObject::kMapOffset));
      __ Assert(equal, "Map not in expected register");
    }

    // Check for fast case object. Generate false result for slow case object.
    __ movq(scratch1_, FieldOperand(object_, JSObject::kPropertiesOffset));
    __ movq(scratch1_, FieldOperand(scratch1_, HeapObject::kMapOffset));
    __ CompareRoot(scratch1_, Heap::kHashTableMapRootIndex);
    __ j(equal, &false_result);

    // Look for valueOf symbol in the descriptor array, and indicate false if
    // found. The type is not checked, so if it is a transition it is a false
    // negative.
    __ movq(map_result_,
           FieldOperand(map_result_, Map::kInstanceDescriptorsOffset));
    __ movq(scratch1_, FieldOperand(map_result_, FixedArray::kLengthOffset));
    // map_result_: descriptor array
    // scratch1_: length of descriptor array
    // Calculate the end of the descriptor array.
    SmiIndex index = masm_->SmiToIndex(scratch2_, scratch1_, kPointerSizeLog2);
    __ lea(scratch1_,
           Operand(
               map_result_, index.reg, index.scale, FixedArray::kHeaderSize));
    // Calculate location of the first key name.
    __ addq(map_result_,
            Immediate(FixedArray::kHeaderSize +
                      DescriptorArray::kFirstIndex * kPointerSize));
    // Loop through all the keys in the descriptor array. If one of these is the
    // symbol valueOf the result is false.
    Label entry, loop;
    __ jmp(&entry);
    __ bind(&loop);
    __ movq(scratch2_, FieldOperand(map_result_, 0));
    __ Cmp(scratch2_, Factory::value_of_symbol());
    __ j(equal, &false_result);
    __ addq(map_result_, Immediate(kPointerSize));
    __ bind(&entry);
    __ cmpq(map_result_, scratch1_);
    __ j(not_equal, &loop);

    // Reload map as register map_result_ was used as temporary above.
    __ movq(map_result_, FieldOperand(object_, HeapObject::kMapOffset));

    // If a valueOf property is not found on the object check that it's
    // prototype is the un-modified String prototype. If not result is false.
    __ movq(scratch1_, FieldOperand(map_result_, Map::kPrototypeOffset));
    __ testq(scratch1_, Immediate(kSmiTagMask));
    __ j(zero, &false_result);
    __ movq(scratch1_, FieldOperand(scratch1_, HeapObject::kMapOffset));
    __ movq(scratch2_,
            Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
    __ movq(scratch2_,
            FieldOperand(scratch2_, GlobalObject::kGlobalContextOffset));
    __ cmpq(scratch1_,
            CodeGenerator::ContextOperand(
                scratch2_, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
    __ j(not_equal, &false_result);
    // Set the bit in the map to indicate that it has been checked safe for
    // default valueOf and set true result.
    __ or_(FieldOperand(map_result_, Map::kBitField2Offset),
           Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
    __ Set(map_result_, 1);
    __ jmp(exit_label());
    __ bind(&false_result);
    // Set false result.
    __ Set(map_result_, 0);
  }

 private:
  Register object_;
  Register map_result_;
  Register scratch1_;
  Register scratch2_;
};


void CodeGenerator::GenerateIsStringWrapperSafeForDefaultValueOf(
    ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();  // Pop the string wrapper.
  obj.ToRegister();
  ASSERT(obj.is_valid());
  if (FLAG_debug_code) {
    __ AbortIfSmi(obj.reg());
  }

  // Check whether this map has already been checked to be safe for default
  // valueOf.
  Result map_result = allocator()->Allocate();
  ASSERT(map_result.is_valid());
  __ movq(map_result.reg(), FieldOperand(obj.reg(), HeapObject::kMapOffset));
  __ testb(FieldOperand(map_result.reg(), Map::kBitField2Offset),
           Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  destination()->true_target()->Branch(not_zero);

  // We need an additional two scratch registers for the deferred code.
  Result temp1 = allocator()->Allocate();
  ASSERT(temp1.is_valid());
  Result temp2 = allocator()->Allocate();
  ASSERT(temp2.is_valid());

  DeferredIsStringWrapperSafeForDefaultValueOf* deferred =
      new DeferredIsStringWrapperSafeForDefaultValueOf(
          obj.reg(), map_result.reg(), temp1.reg(), temp2.reg());
  deferred->Branch(zero);
  deferred->BindExit();
  __ testq(map_result.reg(), map_result.reg());
  obj.Unuse();
  map_result.Unuse();
  temp1.Unuse();
  temp2.Unuse();
  destination()->Split(not_equal);
}


void CodeGenerator::GenerateIsFunction(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (%_ClassOf(arg) === 'Function')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();
  Condition is_smi = masm_->CheckSmi(obj.reg());
  destination()->false_target()->Branch(is_smi);
  __ CmpObjectType(obj.reg(), JS_FUNCTION_TYPE, kScratchRegister);
  obj.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();
  Condition is_smi = masm_->CheckSmi(obj.reg());
  destination()->false_target()->Branch(is_smi);
  __ movq(kScratchRegister, FieldOperand(obj.reg(), HeapObject::kMapOffset));
  __ movzxbl(kScratchRegister,
             FieldOperand(kScratchRegister, Map::kBitFieldOffset));
  __ testl(kScratchRegister, Immediate(1 << Map::kIsUndetectable));
  obj.Unuse();
  destination()->Split(not_zero);
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  // Get the frame pointer for the calling frame.
  Result fp = allocator()->Allocate();
  __ movq(fp.reg(), Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ SmiCompare(Operand(fp.reg(), StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &check_frame_marker);
  __ movq(fp.reg(), Operand(fp.reg(), StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ SmiCompare(Operand(fp.reg(), StandardFrameConstants::kMarkerOffset),
                Smi::FromInt(StackFrame::CONSTRUCT));
  fp.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Result fp = allocator_->Allocate();
  Result result = allocator_->Allocate();
  ASSERT(fp.is_valid() && result.is_valid());

  Label exit;

  // Get the number of formal parameters.
  __ Move(result.reg(), Smi::FromInt(scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ movq(fp.reg(), Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(fp.reg(), StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ movq(result.reg(),
          Operand(fp.reg(), ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  result.set_type_info(TypeInfo::Smi());
  if (FLAG_debug_code) {
    __ AbortIfNotSmi(result.reg());
  }
  frame_->Push(&result);
}


void CodeGenerator::GenerateClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  JumpTarget leave, null, function, non_function_constructor;
  Load(args->at(0));  // Load the object.
  Result obj = frame_->Pop();
  obj.ToRegister();
  frame_->Spill(obj.reg());

  // If the object is a smi, we return null.
  Condition is_smi = masm_->CheckSmi(obj.reg());
  null.Branch(is_smi);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.

  __ CmpObjectType(obj.reg(), FIRST_JS_OBJECT_TYPE, obj.reg());
  null.Branch(below);

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ CmpInstanceType(obj.reg(), JS_FUNCTION_TYPE);
  function.Branch(equal);

  // Check if the constructor in the map is a function.
  __ movq(obj.reg(), FieldOperand(obj.reg(), Map::kConstructorOffset));
  __ CmpObjectType(obj.reg(), JS_FUNCTION_TYPE, kScratchRegister);
  non_function_constructor.Branch(not_equal);

  // The obj register now contains the constructor function. Grab the
  // instance class name from there.
  __ movq(obj.reg(),
          FieldOperand(obj.reg(), JSFunction::kSharedFunctionInfoOffset));
  __ movq(obj.reg(),
          FieldOperand(obj.reg(),
                       SharedFunctionInfo::kInstanceClassNameOffset));
  frame_->Push(&obj);
  leave.Jump();

  // Functions have class 'Function'.
  function.Bind();
  frame_->Push(Factory::function_class_symbol());
  leave.Jump();

  // Objects with a non-function constructor have class 'Object'.
  non_function_constructor.Bind();
  frame_->Push(Factory::Object_symbol());
  leave.Jump();

  // Non-JS objects have class null.
  null.Bind();
  frame_->Push(Factory::null_value());

  // All done.
  leave.Bind();
}


void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  JumpTarget leave;
  Load(args->at(0));  // Load the object.
  frame_->Dup();
  Result object = frame_->Pop();
  object.ToRegister();
  ASSERT(object.is_valid());
  // if (object->IsSmi()) return object.
  Condition is_smi = masm_->CheckSmi(object.reg());
  leave.Branch(is_smi);
  // It is a heap object - get map.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  // if (!object->IsJSValue()) return object.
  __ CmpObjectType(object.reg(), JS_VALUE_TYPE, temp.reg());
  leave.Branch(not_equal);
  __ movq(temp.reg(), FieldOperand(object.reg(), JSValue::kValueOffset));
  object.Unuse();
  frame_->SetElementAt(0, &temp);
  leave.Bind();
}


void CodeGenerator::GenerateSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);
  JumpTarget leave;
  Load(args->at(0));  // Load the object.
  Load(args->at(1));  // Load the value.
  Result value = frame_->Pop();
  Result object = frame_->Pop();
  value.ToRegister();
  object.ToRegister();

  // if (object->IsSmi()) return value.
  Condition is_smi = masm_->CheckSmi(object.reg());
  leave.Branch(is_smi, &value);

  // It is a heap object - get its map.
  Result scratch = allocator_->Allocate();
  ASSERT(scratch.is_valid());
  // if (!object->IsJSValue()) return value.
  __ CmpObjectType(object.reg(), JS_VALUE_TYPE, scratch.reg());
  leave.Branch(not_equal, &value);

  // Store the value.
  __ movq(FieldOperand(object.reg(), JSValue::kValueOffset), value.reg());
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  Result duplicate_value = allocator_->Allocate();
  ASSERT(duplicate_value.is_valid());
  __ movq(duplicate_value.reg(), value.reg());
  // The object register is also overwritten by the write barrier and
  // possibly aliased in the frame.
  frame_->Spill(object.reg());
  __ RecordWrite(object.reg(), JSValue::kValueOffset, duplicate_value.reg(),
                 scratch.reg());
  object.Unuse();
  scratch.Unuse();
  duplicate_value.Unuse();

  // Leave.
  leave.Bind(&value);
  frame_->Push(&value);
}


void CodeGenerator::GenerateArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in rdx and the formal
  // parameter count in rax.
  Load(args->at(0));
  Result key = frame_->Pop();
  // Explicitly create a constant result.
  Result count(Handle<Smi>(Smi::FromInt(scope()->num_parameters())));
  // Call the shared stub to get to arguments[key].
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  Result result = frame_->CallStub(&stub, &key, &count);
  frame_->Push(&result);
}


void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  Load(args->at(0));
  Load(args->at(1));
  Result right = frame_->Pop();
  Result left = frame_->Pop();
  right.ToRegister();
  left.ToRegister();
  __ cmpq(right.reg(), left.reg());
  right.Unuse();
  left.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateGetFramePointer(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  // RBP value is aligned, so it should be tagged as a smi (without necesarily
  // being padded as a smi, so it should not be treated as a smi.).
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  Result rbp_as_smi = allocator_->Allocate();
  ASSERT(rbp_as_smi.is_valid());
  __ movq(rbp_as_smi.reg(), rbp);
  frame_->Push(&rbp_as_smi);
}


void CodeGenerator::GenerateRandomHeapNumber(
    ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  frame_->SpillAll();

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;
  __ AllocateHeapNumber(rbx, rcx, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ movq(rbx, rax);

  __ bind(&heapnumber_allocated);

  // Return a random uint32 number in rax.
  // The fresh HeapNumber is in rbx, which is callee-save on both x64 ABIs.
  __ PrepareCallCFunction(0);
  __ CallCFunction(ExternalReference::random_uint32_function(), 0);

  // Convert 32 random bits in rax to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  __ movl(rcx, Immediate(0x49800000));  // 1.0 x 2^20 as single.
  __ movd(xmm1, rcx);
  __ movd(xmm0, rax);
  __ cvtss2sd(xmm1, xmm1);
  __ xorpd(xmm0, xmm1);
  __ subsd(xmm0, xmm1);
  __ movsd(FieldOperand(rbx, HeapNumber::kValueOffset), xmm0);

  __ movq(rax, rbx);
  Result result = allocator_->Allocate(rax);
  frame_->Push(&result);
}


void CodeGenerator::GenerateStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Load(args->at(0));
  Load(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  Result answer = frame_->CallStub(&stub, 2);
  frame_->Push(&answer);
}


void CodeGenerator::GenerateSubString(ZoneList<Expression*>* args) {
  ASSERT_EQ(3, args->length());

  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));

  SubStringStub stub;
  Result answer = frame_->CallStub(&stub, 3);
  frame_->Push(&answer);
}


void CodeGenerator::GenerateStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Load(args->at(0));
  Load(args->at(1));

  StringCompareStub stub;
  Result answer = frame_->CallStub(&stub, 2);
  frame_->Push(&answer);
}


void CodeGenerator::GenerateRegExpExec(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 4);

  // Load the arguments on the stack and call the runtime system.
  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));
  Load(args->at(3));
  RegExpExecStub stub;
  Result result = frame_->CallStub(&stub, 4);
  frame_->Push(&result);
}


void CodeGenerator::GenerateRegExpConstructResult(ZoneList<Expression*>* args) {
  // No stub. This code only occurs a few times in regexp.js.
  const int kMaxInlineLength = 100;
  ASSERT_EQ(3, args->length());
  Load(args->at(0));  // Size of array, smi.
  Load(args->at(1));  // "index" property value.
  Load(args->at(2));  // "input" property value.
  {
    VirtualFrame::SpilledScope spilled_scope;

    Label slowcase;
    Label done;
    __ movq(r8, Operand(rsp, kPointerSize * 2));
    __ JumpIfNotSmi(r8, &slowcase);
    __ SmiToInteger32(rbx, r8);
    __ cmpl(rbx, Immediate(kMaxInlineLength));
    __ j(above, &slowcase);
    // Smi-tagging is equivalent to multiplying by 2.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    // Allocate RegExpResult followed by FixedArray with size in ebx.
    // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
    // Elements:  [Map][Length][..elements..]
    __ AllocateInNewSpace(JSRegExpResult::kSize + FixedArray::kHeaderSize,
                          times_pointer_size,
                          rbx,  // In: Number of elements.
                          rax,  // Out: Start of allocation (tagged).
                          rcx,  // Out: End of allocation.
                          rdx,  // Scratch register
                          &slowcase,
                          TAG_OBJECT);
    // rax: Start of allocated area, object-tagged.
    // rbx: Number of array elements as int32.
    // r8: Number of array elements as smi.

    // Set JSArray map to global.regexp_result_map().
    __ movq(rdx, ContextOperand(rsi, Context::GLOBAL_INDEX));
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalContextOffset));
    __ movq(rdx, ContextOperand(rdx, Context::REGEXP_RESULT_MAP_INDEX));
    __ movq(FieldOperand(rax, HeapObject::kMapOffset), rdx);

    // Set empty properties FixedArray.
    __ Move(FieldOperand(rax, JSObject::kPropertiesOffset),
            Factory::empty_fixed_array());

    // Set elements to point to FixedArray allocated right after the JSArray.
    __ lea(rcx, Operand(rax, JSRegExpResult::kSize));
    __ movq(FieldOperand(rax, JSObject::kElementsOffset), rcx);

    // Set input, index and length fields from arguments.
    __ pop(FieldOperand(rax, JSRegExpResult::kInputOffset));
    __ pop(FieldOperand(rax, JSRegExpResult::kIndexOffset));
    __ lea(rsp, Operand(rsp, kPointerSize));
    __ movq(FieldOperand(rax, JSArray::kLengthOffset), r8);

    // Fill out the elements FixedArray.
    // rax: JSArray.
    // rcx: FixedArray.
    // rbx: Number of elements in array as int32.

    // Set map.
    __ Move(FieldOperand(rcx, HeapObject::kMapOffset),
            Factory::fixed_array_map());
    // Set length.
    __ Integer32ToSmi(rdx, rbx);
    __ movq(FieldOperand(rcx, FixedArray::kLengthOffset), rdx);
    // Fill contents of fixed-array with the-hole.
    __ Move(rdx, Factory::the_hole_value());
    __ lea(rcx, FieldOperand(rcx, FixedArray::kHeaderSize));
    // Fill fixed array elements with hole.
    // rax: JSArray.
    // rbx: Number of elements in array that remains to be filled, as int32.
    // rcx: Start of elements in FixedArray.
    // rdx: the hole.
    Label loop;
    __ testl(rbx, rbx);
    __ bind(&loop);
    __ j(less_equal, &done);  // Jump if ecx is negative or zero.
    __ subl(rbx, Immediate(1));
    __ movq(Operand(rcx, rbx, times_pointer_size, 0), rdx);
    __ jmp(&loop);

    __ bind(&slowcase);
    __ CallRuntime(Runtime::kRegExpConstructResult, 3);

    __ bind(&done);
  }
  frame_->Forget(3);
  frame_->Push(rax);
}


void CodeGenerator::GenerateRegExpCloneResult(ZoneList<Expression*>* args) {
  ASSERT_EQ(1, args->length());

  Load(args->at(0));
  Result object_result = frame_->Pop();
  object_result.ToRegister(rax);
  object_result.Unuse();
  {
    VirtualFrame::SpilledScope spilled_scope;

    Label done;
    __ JumpIfSmi(rax, &done);

    // Load JSRegExpResult map into rdx.
    // Arguments to this function should be results of calling RegExp exec,
    // which is either an unmodified JSRegExpResult or null. Anything not having
    // the unmodified JSRegExpResult map is returned unmodified.
    // This also ensures that elements are fast.

    __ movq(rdx, ContextOperand(rsi, Context::GLOBAL_INDEX));
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalContextOffset));
    __ movq(rdx, ContextOperand(rdx, Context::REGEXP_RESULT_MAP_INDEX));
    __ cmpq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ j(not_equal, &done);

    if (FLAG_debug_code) {
      // Check that object really has empty properties array, as the map
      // should guarantee.
      __ CompareRoot(FieldOperand(rax, JSObject::kPropertiesOffset),
                     Heap::kEmptyFixedArrayRootIndex);
      __ Check(equal, "JSRegExpResult: default map but non-empty properties.");
    }

    DeferredAllocateInNewSpace* allocate_fallback =
        new DeferredAllocateInNewSpace(JSRegExpResult::kSize,
                                       rbx,
                                       rdx.bit() | rax.bit());

    // All set, copy the contents to a new object.
    __ AllocateInNewSpace(JSRegExpResult::kSize,
                          rbx,
                          no_reg,
                          no_reg,
                          allocate_fallback->entry_label(),
                          TAG_OBJECT);
    __ bind(allocate_fallback->exit_label());

    STATIC_ASSERT(JSRegExpResult::kSize % (2 * kPointerSize) == 0);
    // There is an even number of fields, so unroll the loop once
    // for efficiency.
    for (int i = 0; i < JSRegExpResult::kSize; i += 2 * kPointerSize) {
      STATIC_ASSERT(JSObject::kMapOffset % (2 * kPointerSize) == 0);
      if (i != JSObject::kMapOffset) {
        // The map was already loaded into edx.
        __ movq(rdx, FieldOperand(rax, i));
      }
      __ movq(rcx, FieldOperand(rax, i + kPointerSize));

      STATIC_ASSERT(JSObject::kElementsOffset % (2 * kPointerSize) == 0);
      if (i == JSObject::kElementsOffset) {
        // If the elements array isn't empty, make it copy-on-write
        // before copying it.
        Label empty;
        __ CompareRoot(rdx, Heap::kEmptyFixedArrayRootIndex);
        __ j(equal, &empty);
        __ LoadRoot(kScratchRegister, Heap::kFixedCOWArrayMapRootIndex);
        __ movq(FieldOperand(rdx, HeapObject::kMapOffset), kScratchRegister);
        __ bind(&empty);
      }
      __ movq(FieldOperand(rbx, i), rdx);
      __ movq(FieldOperand(rbx, i + kPointerSize), rcx);
    }
    __ movq(rax, rbx);

    __ bind(&done);
  }
  frame_->Push(rax);
}


class DeferredSearchCache: public DeferredCode {
 public:
  DeferredSearchCache(Register dst,
                      Register cache,
                      Register key,
                      Register scratch)
      : dst_(dst), cache_(cache), key_(key), scratch_(scratch) {
    set_comment("[ DeferredSearchCache");
  }

  virtual void Generate();

 private:
  Register dst_;    // on invocation index of finger (as int32), on exit
                    // holds value being looked up.
  Register cache_;  // instance of JSFunctionResultCache.
  Register key_;    // key being looked up.
  Register scratch_;
};


// Return a position of the element at |index| + |additional_offset|
// in FixedArray pointer to which is held in |array|.  |index| is int32.
static Operand ArrayElement(Register array,
                            Register index,
                            int additional_offset = 0) {
  int offset = FixedArray::kHeaderSize + additional_offset * kPointerSize;
  return FieldOperand(array, index, times_pointer_size, offset);
}


void DeferredSearchCache::Generate() {
  Label first_loop, search_further, second_loop, cache_miss;

  Immediate kEntriesIndexImm = Immediate(JSFunctionResultCache::kEntriesIndex);
  Immediate kEntrySizeImm = Immediate(JSFunctionResultCache::kEntrySize);

  // Check the cache from finger to start of the cache.
  __ bind(&first_loop);
  __ subl(dst_, kEntrySizeImm);
  __ cmpl(dst_, kEntriesIndexImm);
  __ j(less, &search_further);

  __ cmpq(ArrayElement(cache_, dst_), key_);
  __ j(not_equal, &first_loop);

  __ Integer32ToSmiField(
      FieldOperand(cache_, JSFunctionResultCache::kFingerOffset), dst_);
  __ movq(dst_, ArrayElement(cache_, dst_, 1));
  __ jmp(exit_label());

  __ bind(&search_further);

  // Check the cache from end of cache up to finger.
  __ SmiToInteger32(dst_,
                    FieldOperand(cache_,
                                 JSFunctionResultCache::kCacheSizeOffset));
  __ SmiToInteger32(scratch_,
                    FieldOperand(cache_, JSFunctionResultCache::kFingerOffset));

  __ bind(&second_loop);
  __ subl(dst_, kEntrySizeImm);
  __ cmpl(dst_, scratch_);
  __ j(less_equal, &cache_miss);

  __ cmpq(ArrayElement(cache_, dst_), key_);
  __ j(not_equal, &second_loop);

  __ Integer32ToSmiField(
      FieldOperand(cache_, JSFunctionResultCache::kFingerOffset), dst_);
  __ movq(dst_, ArrayElement(cache_, dst_, 1));
  __ jmp(exit_label());

  __ bind(&cache_miss);
  __ push(cache_);  // store a reference to cache
  __ push(key_);  // store a key
  __ push(Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ push(key_);
  // On x64 function must be in rdi.
  __ movq(rdi, FieldOperand(cache_, JSFunctionResultCache::kFactoryOffset));
  ParameterCount expected(1);
  __ InvokeFunction(rdi, expected, CALL_FUNCTION);

  // Find a place to put new cached value into.
  Label add_new_entry, update_cache;
  __ movq(rcx, Operand(rsp, kPointerSize));  // restore the cache
  // Possible optimization: cache size is constant for the given cache
  // so technically we could use a constant here.  However, if we have
  // cache miss this optimization would hardly matter much.

  // Check if we could add new entry to cache.
  __ SmiToInteger32(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ SmiToInteger32(r9,
                    FieldOperand(rcx, JSFunctionResultCache::kCacheSizeOffset));
  __ cmpl(rbx, r9);
  __ j(greater, &add_new_entry);

  // Check if we could evict entry after finger.
  __ SmiToInteger32(rdx,
                    FieldOperand(rcx, JSFunctionResultCache::kFingerOffset));
  __ addl(rdx, kEntrySizeImm);
  Label forward;
  __ cmpl(rbx, rdx);
  __ j(greater, &forward);
  // Need to wrap over the cache.
  __ movl(rdx, kEntriesIndexImm);
  __ bind(&forward);
  __ movl(r9, rdx);
  __ jmp(&update_cache);

  __ bind(&add_new_entry);
  // r9 holds cache size as int32.
  __ leal(rbx, Operand(r9, JSFunctionResultCache::kEntrySize));
  __ Integer32ToSmiField(
      FieldOperand(rcx, JSFunctionResultCache::kCacheSizeOffset), rbx);

  // Update the cache itself.
  // r9 holds the index as int32.
  __ bind(&update_cache);
  __ pop(rbx);  // restore the key
  __ Integer32ToSmiField(
      FieldOperand(rcx, JSFunctionResultCache::kFingerOffset), r9);
  // Store key.
  __ movq(ArrayElement(rcx, r9), rbx);
  __ RecordWrite(rcx, 0, rbx, r9);

  // Store value.
  __ pop(rcx);  // restore the cache.
  __ SmiToInteger32(rdx,
                    FieldOperand(rcx, JSFunctionResultCache::kFingerOffset));
  __ incl(rdx);
  // Backup rax, because the RecordWrite macro clobbers its arguments.
  __ movq(rbx, rax);
  __ movq(ArrayElement(rcx, rdx), rax);
  __ RecordWrite(rcx, 0, rbx, rdx);

  if (!dst_.is(rax)) {
    __ movq(dst_, rax);
  }
}


void CodeGenerator::GenerateGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      Top::global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    frame_->Push(Factory::undefined_value());
    return;
  }

  Load(args->at(1));
  Result key = frame_->Pop();
  key.ToRegister();

  Result cache = allocator()->Allocate();
  ASSERT(cache.is_valid());
  __ movq(cache.reg(), ContextOperand(rsi, Context::GLOBAL_INDEX));
  __ movq(cache.reg(),
          FieldOperand(cache.reg(), GlobalObject::kGlobalContextOffset));
  __ movq(cache.reg(),
          ContextOperand(cache.reg(), Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ movq(cache.reg(),
          FieldOperand(cache.reg(), FixedArray::OffsetOfElementAt(cache_id)));

  Result tmp = allocator()->Allocate();
  ASSERT(tmp.is_valid());

  Result scratch = allocator()->Allocate();
  ASSERT(scratch.is_valid());

  DeferredSearchCache* deferred = new DeferredSearchCache(tmp.reg(),
                                                          cache.reg(),
                                                          key.reg(),
                                                          scratch.reg());

  const int kFingerOffset =
      FixedArray::OffsetOfElementAt(JSFunctionResultCache::kFingerIndex);
  // tmp.reg() now holds finger offset as a smi.
  __ SmiToInteger32(tmp.reg(), FieldOperand(cache.reg(), kFingerOffset));
  __ cmpq(key.reg(), FieldOperand(cache.reg(),
                                  tmp.reg(), times_pointer_size,
                                  FixedArray::kHeaderSize));
  deferred->Branch(not_equal);
  __ movq(tmp.reg(), FieldOperand(cache.reg(),
                                  tmp.reg(), times_pointer_size,
                                  FixedArray::kHeaderSize + kPointerSize));

  deferred->BindExit();
  frame_->Push(&tmp);
}


void CodeGenerator::GenerateNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and jump to the runtime.
  Load(args->at(0));

  NumberToStringStub stub;
  Result result = frame_->CallStub(&stub, 1);
  frame_->Push(&result);
}


class DeferredSwapElements: public DeferredCode {
 public:
  DeferredSwapElements(Register object, Register index1, Register index2)
      : object_(object), index1_(index1), index2_(index2) {
    set_comment("[ DeferredSwapElements");
  }

  virtual void Generate();

 private:
  Register object_, index1_, index2_;
};


void DeferredSwapElements::Generate() {
  __ push(object_);
  __ push(index1_);
  __ push(index2_);
  __ CallRuntime(Runtime::kSwapElements, 3);
}


void CodeGenerator::GenerateSwapElements(ZoneList<Expression*>* args) {
  Comment cmnt(masm_, "[ GenerateSwapElements");

  ASSERT_EQ(3, args->length());

  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));

  Result index2 = frame_->Pop();
  index2.ToRegister();

  Result index1 = frame_->Pop();
  index1.ToRegister();

  Result object = frame_->Pop();
  object.ToRegister();

  Result tmp1 = allocator()->Allocate();
  tmp1.ToRegister();
  Result tmp2 = allocator()->Allocate();
  tmp2.ToRegister();

  frame_->Spill(object.reg());
  frame_->Spill(index1.reg());
  frame_->Spill(index2.reg());

  DeferredSwapElements* deferred = new DeferredSwapElements(object.reg(),
                                                            index1.reg(),
                                                            index2.reg());

  // Fetch the map and check if array is in fast case.
  // Check that object doesn't require security checks and
  // has no indexed interceptor.
  __ CmpObjectType(object.reg(), FIRST_JS_OBJECT_TYPE, tmp1.reg());
  deferred->Branch(below);
  __ testb(FieldOperand(tmp1.reg(), Map::kBitFieldOffset),
           Immediate(KeyedLoadIC::kSlowCaseBitFieldMask));
  deferred->Branch(not_zero);

  // Check the object's elements are in fast case and writable.
  __ movq(tmp1.reg(), FieldOperand(object.reg(), JSObject::kElementsOffset));
  __ CompareRoot(FieldOperand(tmp1.reg(), HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  deferred->Branch(not_equal);

  // Check that both indices are smis.
  Condition both_smi = __ CheckBothSmi(index1.reg(), index2.reg());
  deferred->Branch(NegateCondition(both_smi));

  // Bring addresses into index1 and index2.
  __ SmiToInteger32(index1.reg(), index1.reg());
  __ lea(index1.reg(), FieldOperand(tmp1.reg(),
                                    index1.reg(),
                                    times_pointer_size,
                                    FixedArray::kHeaderSize));
  __ SmiToInteger32(index2.reg(), index2.reg());
  __ lea(index2.reg(), FieldOperand(tmp1.reg(),
                                    index2.reg(),
                                    times_pointer_size,
                                    FixedArray::kHeaderSize));

  // Swap elements.
  __ movq(object.reg(), Operand(index1.reg(), 0));
  __ movq(tmp2.reg(), Operand(index2.reg(), 0));
  __ movq(Operand(index2.reg(), 0), object.reg());
  __ movq(Operand(index1.reg(), 0), tmp2.reg());

  Label done;
  __ InNewSpace(tmp1.reg(), tmp2.reg(), equal, &done);
  // Possible optimization: do a check that both values are Smis
  // (or them and test against Smi mask.)

  __ movq(tmp2.reg(), tmp1.reg());
  RecordWriteStub recordWrite1(tmp2.reg(), index1.reg(), object.reg());
  __ CallStub(&recordWrite1);

  RecordWriteStub recordWrite2(tmp1.reg(), index2.reg(), object.reg());
  __ CallStub(&recordWrite2);

  __ bind(&done);

  deferred->BindExit();
  frame_->Push(Factory::undefined_value());
}


void CodeGenerator::GenerateCallFunction(ZoneList<Expression*>* args) {
  Comment cmnt(masm_, "[ GenerateCallFunction");

  ASSERT(args->length() >= 2);

  int n_args = args->length() - 2;  // for receiver and function.
  Load(args->at(0));  // receiver
  for (int i = 0; i < n_args; i++) {
    Load(args->at(i + 1));
  }
  Load(args->at(n_args + 1));  // function
  Result result = frame_->CallJSFunction(n_args);
  frame_->Push(&result);
}


// Generates the Math.pow method. Only handles special cases and
// branches to the runtime system for everything else. Please note
// that this function assumes that the callsite has executed ToNumber
// on both arguments.
void CodeGenerator::GenerateMathPow(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);
  Load(args->at(0));
  Load(args->at(1));

  Label allocate_return;
  // Load the two operands while leaving the values on the frame.
  frame()->Dup();
  Result exponent = frame()->Pop();
  exponent.ToRegister();
  frame()->Spill(exponent.reg());
  frame()->PushElementAt(1);
  Result base = frame()->Pop();
  base.ToRegister();
  frame()->Spill(base.reg());

  Result answer = allocator()->Allocate();
  ASSERT(answer.is_valid());
  ASSERT(!exponent.reg().is(base.reg()));
  JumpTarget call_runtime;

  // Save 1 in xmm3 - we need this several times later on.
  __ movl(answer.reg(), Immediate(1));
  __ cvtlsi2sd(xmm3, answer.reg());

  Label exponent_nonsmi;
  Label base_nonsmi;
  // If the exponent is a heap number go to that specific case.
  __ JumpIfNotSmi(exponent.reg(), &exponent_nonsmi);
  __ JumpIfNotSmi(base.reg(), &base_nonsmi);

  // Optimized version when y is an integer.
  Label powi;
  __ SmiToInteger32(base.reg(), base.reg());
  __ cvtlsi2sd(xmm0, base.reg());
  __ jmp(&powi);
  // exponent is smi and base is a heapnumber.
  __ bind(&base_nonsmi);
  __ CompareRoot(FieldOperand(base.reg(), HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  call_runtime.Branch(not_equal);

  __ movsd(xmm0, FieldOperand(base.reg(), HeapNumber::kValueOffset));

  // Optimized version of pow if y is an integer.
  __ bind(&powi);
  __ SmiToInteger32(exponent.reg(), exponent.reg());

  // Save exponent in base as we need to check if exponent is negative later.
  // We know that base and exponent are in different registers.
  __ movl(base.reg(), exponent.reg());

  // Get absolute value of exponent.
  Label no_neg;
  __ cmpl(exponent.reg(), Immediate(0));
  __ j(greater_equal, &no_neg);
  __ negl(exponent.reg());
  __ bind(&no_neg);

  // Load xmm1 with 1.
  __ movsd(xmm1, xmm3);
  Label while_true;
  Label no_multiply;

  __ bind(&while_true);
  __ shrl(exponent.reg(), Immediate(1));
  __ j(not_carry, &no_multiply);
  __ mulsd(xmm1, xmm0);
  __ bind(&no_multiply);
  __ testl(exponent.reg(), exponent.reg());
  __ mulsd(xmm0, xmm0);
  __ j(not_zero, &while_true);

  // x has the original value of y - if y is negative return 1/result.
  __ testl(base.reg(), base.reg());
  __ j(positive, &allocate_return);
  // Special case if xmm1 has reached infinity.
  __ movl(answer.reg(), Immediate(0x7FB00000));
  __ movd(xmm0, answer.reg());
  __ cvtss2sd(xmm0, xmm0);
  __ ucomisd(xmm0, xmm1);
  call_runtime.Branch(equal);
  __ divsd(xmm3, xmm1);
  __ movsd(xmm1, xmm3);
  __ jmp(&allocate_return);

  // exponent (or both) is a heapnumber - no matter what we should now work
  // on doubles.
  __ bind(&exponent_nonsmi);
  __ CompareRoot(FieldOperand(exponent.reg(), HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  call_runtime.Branch(not_equal);
  __ movsd(xmm1, FieldOperand(exponent.reg(), HeapNumber::kValueOffset));
  // Test if exponent is nan.
  __ ucomisd(xmm1, xmm1);
  call_runtime.Branch(parity_even);

  Label base_not_smi;
  Label handle_special_cases;
  __ JumpIfNotSmi(base.reg(), &base_not_smi);
  __ SmiToInteger32(base.reg(), base.reg());
  __ cvtlsi2sd(xmm0, base.reg());
  __ jmp(&handle_special_cases);
  __ bind(&base_not_smi);
  __ CompareRoot(FieldOperand(base.reg(), HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  call_runtime.Branch(not_equal);
  __ movl(answer.reg(), FieldOperand(base.reg(), HeapNumber::kExponentOffset));
  __ andl(answer.reg(), Immediate(HeapNumber::kExponentMask));
  __ cmpl(answer.reg(), Immediate(HeapNumber::kExponentMask));
  // base is NaN or +/-Infinity
  call_runtime.Branch(greater_equal);
  __ movsd(xmm0, FieldOperand(base.reg(), HeapNumber::kValueOffset));

  // base is in xmm0 and exponent is in xmm1.
  __ bind(&handle_special_cases);
  Label not_minus_half;
  // Test for -0.5.
  // Load xmm2 with -0.5.
  __ movl(answer.reg(), Immediate(0xBF000000));
  __ movd(xmm2, answer.reg());
  __ cvtss2sd(xmm2, xmm2);
  // xmm2 now has -0.5.
  __ ucomisd(xmm2, xmm1);
  __ j(not_equal, &not_minus_half);

  // Calculates reciprocal of square root.
  // Note that 1/sqrt(x) = sqrt(1/x))
  __ divsd(xmm3, xmm0);
  __ movsd(xmm1, xmm3);
  __ sqrtsd(xmm1, xmm1);
  __ jmp(&allocate_return);

  // Test for 0.5.
  __ bind(&not_minus_half);
  // Load xmm2 with 0.5.
  // Since xmm3 is 1 and xmm2 is -0.5 this is simply xmm2 + xmm3.
  __ addsd(xmm2, xmm3);
  // xmm2 now has 0.5.
  __ ucomisd(xmm2, xmm1);
  call_runtime.Branch(not_equal);

  // Calculates square root.
  __ movsd(xmm1, xmm0);
  __ sqrtsd(xmm1, xmm1);

  JumpTarget done;
  Label failure, success;
  __ bind(&allocate_return);
  // Make a copy of the frame to enable us to handle allocation
  // failure after the JumpTarget jump.
  VirtualFrame* clone = new VirtualFrame(frame());
  __ AllocateHeapNumber(answer.reg(), exponent.reg(), &failure);
  __ movsd(FieldOperand(answer.reg(), HeapNumber::kValueOffset), xmm1);
  // Remove the two original values from the frame - we only need those
  // in the case where we branch to runtime.
  frame()->Drop(2);
  exponent.Unuse();
  base.Unuse();
  done.Jump(&answer);
  // Use the copy of the original frame as our current frame.
  RegisterFile empty_regs;
  SetFrame(clone, &empty_regs);
  // If we experience an allocation failure we branch to runtime.
  __ bind(&failure);
  call_runtime.Bind();
  answer = frame()->CallRuntime(Runtime::kMath_pow_cfunction, 2);

  done.Bind(&answer);
  frame()->Push(&answer);
}


void CodeGenerator::GenerateMathSin(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);
  Load(args->at(0));
  TranscendentalCacheStub stub(TranscendentalCache::SIN);
  Result result = frame_->CallStub(&stub, 1);
  frame_->Push(&result);
}


void CodeGenerator::GenerateMathCos(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);
  Load(args->at(0));
  TranscendentalCacheStub stub(TranscendentalCache::COS);
  Result result = frame_->CallStub(&stub, 1);
  frame_->Push(&result);
}


// Generates the Math.sqrt method. Please note - this function assumes that
// the callsite has executed ToNumber on the argument.
void CodeGenerator::GenerateMathSqrt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));

  // Leave original value on the frame if we need to call runtime.
  frame()->Dup();
  Result result = frame()->Pop();
  result.ToRegister();
  frame()->Spill(result.reg());
  Label runtime;
  Label non_smi;
  Label load_done;
  JumpTarget end;

  __ JumpIfNotSmi(result.reg(), &non_smi);
  __ SmiToInteger32(result.reg(), result.reg());
  __ cvtlsi2sd(xmm0, result.reg());
  __ jmp(&load_done);
  __ bind(&non_smi);
  __ CompareRoot(FieldOperand(result.reg(), HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &runtime);
  __ movsd(xmm0, FieldOperand(result.reg(), HeapNumber::kValueOffset));

  __ bind(&load_done);
  __ sqrtsd(xmm0, xmm0);
  // A copy of the virtual frame to allow us to go to runtime after the
  // JumpTarget jump.
  Result scratch = allocator()->Allocate();
  VirtualFrame* clone = new VirtualFrame(frame());
  __ AllocateHeapNumber(result.reg(), scratch.reg(), &runtime);

  __ movsd(FieldOperand(result.reg(), HeapNumber::kValueOffset), xmm0);
  frame()->Drop(1);
  scratch.Unuse();
  end.Jump(&result);
  // We only branch to runtime if we have an allocation error.
  // Use the copy of the original frame as our current frame.
  RegisterFile empty_regs;
  SetFrame(clone, &empty_regs);
  __ bind(&runtime);
  result = frame()->CallRuntime(Runtime::kMath_sqrt, 1);

  end.Bind(&result);
  frame()->Push(&result);
}


void CodeGenerator::GenerateIsRegExpEquivalent(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());
  Load(args->at(0));
  Load(args->at(1));
  Result right_res = frame_->Pop();
  Result left_res = frame_->Pop();
  right_res.ToRegister();
  left_res.ToRegister();
  Result tmp_res = allocator()->Allocate();
  ASSERT(tmp_res.is_valid());
  Register right = right_res.reg();
  Register left = left_res.reg();
  Register tmp = tmp_res.reg();
  right_res.Unuse();
  left_res.Unuse();
  tmp_res.Unuse();
  __ cmpq(left, right);
  destination()->true_target()->Branch(equal);
  // Fail if either is a non-HeapObject.
  Condition either_smi =
      masm()->CheckEitherSmi(left, right, tmp);
  destination()->false_target()->Branch(either_smi);
  __ movq(tmp, FieldOperand(left, HeapObject::kMapOffset));
  __ cmpb(FieldOperand(tmp, Map::kInstanceTypeOffset),
          Immediate(JS_REGEXP_TYPE));
  destination()->false_target()->Branch(not_equal);
  __ cmpq(tmp, FieldOperand(right, HeapObject::kMapOffset));
  destination()->false_target()->Branch(not_equal);
  __ movq(tmp, FieldOperand(left, JSRegExp::kDataOffset));
  __ cmpq(tmp, FieldOperand(right, JSRegExp::kDataOffset));
  destination()->Split(equal);
}


void CodeGenerator::GenerateHasCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  __ testl(FieldOperand(value.reg(), String::kHashFieldOffset),
           Immediate(String::kContainsCachedArrayIndexMask));
  value.Unuse();
  destination()->Split(zero);
}


void CodeGenerator::GenerateGetCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result string = frame_->Pop();
  string.ToRegister();

  Result number = allocator()->Allocate();
  ASSERT(number.is_valid());
  __ movl(number.reg(), FieldOperand(string.reg(), String::kHashFieldOffset));
  __ IndexFromHash(number.reg(), number.reg());
  string.Unuse();
  frame_->Push(&number);
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
  if (CheckForInlineRuntimeCall(node)) {
    return;
  }

  ZoneList<Expression*>* args = node->arguments();
  Comment cmnt(masm_, "[ CallRuntime");
  Runtime::Function* function = node->function();

  if (function == NULL) {
    // Push the builtins object found in the current global object.
    Result temp = allocator()->Allocate();
    ASSERT(temp.is_valid());
    __ movq(temp.reg(), GlobalObject());
    __ movq(temp.reg(),
            FieldOperand(temp.reg(), GlobalObject::kBuiltinsOffset));
    frame_->Push(&temp);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
  }

  if (function == NULL) {
    // Call the JS runtime function.
    frame_->Push(node->name());
    Result answer = frame_->CallCallIC(RelocInfo::CODE_TARGET,
                                       arg_count,
                                       loop_nesting_);
    frame_->RestoreContextRegister();
    frame_->Push(&answer);
  } else {
    // Call the C runtime function.
    Result answer = frame_->CallRuntime(function, arg_count);
    frame_->Push(&answer);
  }
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
  Comment cmnt(masm_, "[ UnaryOperation");

  Token::Value op = node->op();

  if (op == Token::NOT) {
    // Swap the true and false targets but keep the same actual label
    // as the fall through.
    destination()->Invert();
    LoadCondition(node->expression(), destination(), true);
    // Swap the labels back.
    destination()->Invert();

  } else if (op == Token::DELETE) {
    Property* property = node->expression()->AsProperty();
    if (property != NULL) {
      Load(property->obj());
      Load(property->key());
      Result answer = frame_->InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION, 2);
      frame_->Push(&answer);
      return;
    }

    Variable* variable = node->expression()->AsVariableProxy()->AsVariable();
    if (variable != NULL) {
      Slot* slot = variable->slot();
      if (variable->is_global()) {
        LoadGlobal();
        frame_->Push(variable->name());
        Result answer = frame_->InvokeBuiltin(Builtins::DELETE,
                                              CALL_FUNCTION, 2);
        frame_->Push(&answer);
        return;

      } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
        // Call the runtime to look up the context holding the named
        // variable.  Sync the virtual frame eagerly so we can push the
        // arguments directly into place.
        frame_->SyncRange(0, frame_->element_count() - 1);
        frame_->EmitPush(rsi);
        frame_->EmitPush(variable->name());
        Result context = frame_->CallRuntime(Runtime::kLookupContext, 2);
        ASSERT(context.is_register());
        frame_->EmitPush(context.reg());
        context.Unuse();
        frame_->EmitPush(variable->name());
        Result answer = frame_->InvokeBuiltin(Builtins::DELETE,
                                              CALL_FUNCTION, 2);
        frame_->Push(&answer);
        return;
      }

      // Default: Result of deleting non-global, not dynamically
      // introduced variables is false.
      frame_->Push(Factory::false_value());

    } else {
      // Default: Result of deleting expressions is true.
      Load(node->expression());  // may have side-effects
      frame_->SetElementAt(0, Factory::true_value());
    }

  } else if (op == Token::TYPEOF) {
    // Special case for loading the typeof expression; see comment on
    // LoadTypeofExpression().
    LoadTypeofExpression(node->expression());
    Result answer = frame_->CallRuntime(Runtime::kTypeof, 1);
    frame_->Push(&answer);

  } else if (op == Token::VOID) {
    Expression* expression = node->expression();
    if (expression && expression->AsLiteral() && (
        expression->AsLiteral()->IsTrue() ||
        expression->AsLiteral()->IsFalse() ||
        expression->AsLiteral()->handle()->IsNumber() ||
        expression->AsLiteral()->handle()->IsString() ||
        expression->AsLiteral()->handle()->IsJSRegExp() ||
        expression->AsLiteral()->IsNull())) {
      // Omit evaluating the value of the primitive literal.
      // It will be discarded anyway, and can have no side effect.
      frame_->Push(Factory::undefined_value());
    } else {
      Load(node->expression());
      frame_->SetElementAt(0, Factory::undefined_value());
    }

  } else {
    bool can_overwrite = node->expression()->ResultOverwriteAllowed();
    UnaryOverwriteMode overwrite =
        can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
    bool no_negative_zero = node->expression()->no_negative_zero();
    Load(node->expression());
    switch (op) {
      case Token::NOT:
      case Token::DELETE:
      case Token::TYPEOF:
        UNREACHABLE();  // handled above
        break;

      case Token::SUB: {
        GenericUnaryOpStub stub(
            Token::SUB,
            overwrite,
            no_negative_zero ? kIgnoreNegativeZero : kStrictNegativeZero);
        Result operand = frame_->Pop();
        Result answer = frame_->CallStub(&stub, &operand);
        answer.set_type_info(TypeInfo::Number());
        frame_->Push(&answer);
        break;
      }

      case Token::BIT_NOT: {
        // Smi check.
        JumpTarget smi_label;
        JumpTarget continue_label;
        Result operand = frame_->Pop();
        operand.ToRegister();

        Condition is_smi = masm_->CheckSmi(operand.reg());
        smi_label.Branch(is_smi, &operand);

        GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
        Result answer = frame_->CallStub(&stub, &operand);
        continue_label.Jump(&answer);

        smi_label.Bind(&answer);
        answer.ToRegister();
        frame_->Spill(answer.reg());
        __ SmiNot(answer.reg(), answer.reg());
        continue_label.Bind(&answer);
        answer.set_type_info(TypeInfo::Smi());
        frame_->Push(&answer);
        break;
      }

      case Token::ADD: {
        // Smi check.
        JumpTarget continue_label;
        Result operand = frame_->Pop();
        TypeInfo operand_info = operand.type_info();
        operand.ToRegister();
        Condition is_smi = masm_->CheckSmi(operand.reg());
        continue_label.Branch(is_smi, &operand);
        frame_->Push(&operand);
        Result answer = frame_->InvokeBuiltin(Builtins::TO_NUMBER,
                                              CALL_FUNCTION, 1);

        continue_label.Bind(&answer);
        if (operand_info.IsSmi()) {
          answer.set_type_info(TypeInfo::Smi());
        } else if (operand_info.IsInteger32()) {
          answer.set_type_info(TypeInfo::Integer32());
        } else {
          answer.set_type_info(TypeInfo::Number());
        }
        frame_->Push(&answer);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
}


// The value in dst was optimistically incremented or decremented.
// The result overflowed or was not smi tagged.  Call into the runtime
// to convert the argument to a number, and call the specialized add
// or subtract stub.  The result is left in dst.
class DeferredPrefixCountOperation: public DeferredCode {
 public:
  DeferredPrefixCountOperation(Register dst,
                               bool is_increment,
                               TypeInfo input_type)
      : dst_(dst), is_increment_(is_increment), input_type_(input_type) {
    set_comment("[ DeferredCountOperation");
  }

  virtual void Generate();

 private:
  Register dst_;
  bool is_increment_;
  TypeInfo input_type_;
};


void DeferredPrefixCountOperation::Generate() {
  Register left;
  if (input_type_.IsNumber()) {
    left = dst_;
  } else {
    __ push(dst_);
    __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
    left = rax;
  }

  GenericBinaryOpStub stub(is_increment_ ? Token::ADD : Token::SUB,
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS,
                           TypeInfo::Number());
  stub.GenerateCall(masm_, left, Smi::FromInt(1));

  if (!dst_.is(rax)) __ movq(dst_, rax);
}


// The value in dst was optimistically incremented or decremented.
// The result overflowed or was not smi tagged.  Call into the runtime
// to convert the argument to a number.  Update the original value in
// old.  Call the specialized add or subtract stub.  The result is
// left in dst.
class DeferredPostfixCountOperation: public DeferredCode {
 public:
  DeferredPostfixCountOperation(Register dst,
                                Register old,
                                bool is_increment,
                                TypeInfo input_type)
      : dst_(dst),
        old_(old),
        is_increment_(is_increment),
        input_type_(input_type) {
    set_comment("[ DeferredCountOperation");
  }

  virtual void Generate();

 private:
  Register dst_;
  Register old_;
  bool is_increment_;
  TypeInfo input_type_;
};


void DeferredPostfixCountOperation::Generate() {
  Register left;
  if (input_type_.IsNumber()) {
    __ push(dst_);  // Save the input to use as the old value.
    left = dst_;
  } else {
    __ push(dst_);
    __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
    __ push(rax);  // Save the result of ToNumber to use as the old value.
    left = rax;
  }

  GenericBinaryOpStub stub(is_increment_ ? Token::ADD : Token::SUB,
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS,
                           TypeInfo::Number());
  stub.GenerateCall(masm_, left, Smi::FromInt(1));

  if (!dst_.is(rax)) __ movq(dst_, rax);
  __ pop(old_);
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
  Comment cmnt(masm_, "[ CountOperation");

  bool is_postfix = node->is_postfix();
  bool is_increment = node->op() == Token::INC;

  Variable* var = node->expression()->AsVariableProxy()->AsVariable();
  bool is_const = (var != NULL && var->mode() == Variable::CONST);

  // Postfix operations need a stack slot under the reference to hold
  // the old value while the new value is being stored.  This is so that
  // in the case that storing the new value requires a call, the old
  // value will be in the frame to be spilled.
  if (is_postfix) frame_->Push(Smi::FromInt(0));

  // A constant reference is not saved to, so the reference is not a
  // compound assignment reference.
  { Reference target(this, node->expression(), !is_const);
    if (target.is_illegal()) {
      // Spoof the virtual frame to have the expected height (one higher
      // than on entry).
      if (!is_postfix) frame_->Push(Smi::FromInt(0));
      return;
    }
    target.TakeValue();

    Result new_value = frame_->Pop();
    new_value.ToRegister();

    Result old_value;  // Only allocated in the postfix case.
    if (is_postfix) {
      // Allocate a temporary to preserve the old value.
      old_value = allocator_->Allocate();
      ASSERT(old_value.is_valid());
      __ movq(old_value.reg(), new_value.reg());

      // The return value for postfix operations is ToNumber(input).
      // Keep more precise type info if the input is some kind of
      // number already. If the input is not a number we have to wait
      // for the deferred code to convert it.
      if (new_value.type_info().IsNumber()) {
        old_value.set_type_info(new_value.type_info());
      }
    }
    // Ensure the new value is writable.
    frame_->Spill(new_value.reg());

    DeferredCode* deferred = NULL;
    if (is_postfix) {
      deferred = new DeferredPostfixCountOperation(new_value.reg(),
                                                   old_value.reg(),
                                                   is_increment,
                                                   new_value.type_info());
    } else {
      deferred = new DeferredPrefixCountOperation(new_value.reg(),
                                                  is_increment,
                                                  new_value.type_info());
    }

    if (new_value.is_smi()) {
      if (FLAG_debug_code) { __ AbortIfNotSmi(new_value.reg()); }
    } else {
      __ JumpIfNotSmi(new_value.reg(), deferred->entry_label());
    }
    if (is_increment) {
      __ SmiAddConstant(new_value.reg(),
                        new_value.reg(),
                        Smi::FromInt(1),
                        deferred->entry_label());
    } else {
      __ SmiSubConstant(new_value.reg(),
                        new_value.reg(),
                        Smi::FromInt(1),
                        deferred->entry_label());
    }
    deferred->BindExit();

    // Postfix count operations return their input converted to
    // number. The case when the input is already a number is covered
    // above in the allocation code for old_value.
    if (is_postfix && !new_value.type_info().IsNumber()) {
      old_value.set_type_info(TypeInfo::Number());
    }

    new_value.set_type_info(TypeInfo::Number());

    // Postfix: store the old value in the allocated slot under the
    // reference.
    if (is_postfix) frame_->SetElementAt(target.size(), &old_value);

    frame_->Push(&new_value);
    // Non-constant: update the reference.
    if (!is_const) target.SetValue(NOT_CONST_INIT);
  }

  // Postfix: drop the new value and use the old.
  if (is_postfix) frame_->Drop();
}


void CodeGenerator::GenerateLogicalBooleanOperation(BinaryOperation* node) {
  // According to ECMA-262 section 11.11, page 58, the binary logical
  // operators must yield the result of one of the two expressions
  // before any ToBoolean() conversions. This means that the value
  // produced by a && or || operator is not necessarily a boolean.

  // NOTE: If the left hand side produces a materialized value (not
  // control flow), we force the right hand side to do the same. This
  // is necessary because we assume that if we get control flow on the
  // last path out of an expression we got it on all paths.
  if (node->op() == Token::AND) {
    JumpTarget is_true;
    ControlDestination dest(&is_true, destination()->false_target(), true);
    LoadCondition(node->left(), &dest, false);

    if (dest.false_was_fall_through()) {
      // The current false target was used as the fall-through.  If
      // there are no dangling jumps to is_true then the left
      // subexpression was unconditionally false.  Otherwise we have
      // paths where we do have to evaluate the right subexpression.
      if (is_true.is_linked()) {
        // We need to compile the right subexpression.  If the jump to
        // the current false target was a forward jump then we have a
        // valid frame, we have just bound the false target, and we
        // have to jump around the code for the right subexpression.
        if (has_valid_frame()) {
          destination()->false_target()->Unuse();
          destination()->false_target()->Jump();
        }
        is_true.Bind();
        // The left subexpression compiled to control flow, so the
        // right one is free to do so as well.
        LoadCondition(node->right(), destination(), false);
      } else {
        // We have actually just jumped to or bound the current false
        // target but the current control destination is not marked as
        // used.
        destination()->Use(false);
      }

    } else if (dest.is_used()) {
      // The left subexpression compiled to control flow (and is_true
      // was just bound), so the right is free to do so as well.
      LoadCondition(node->right(), destination(), false);

    } else {
      // We have a materialized value on the frame, so we exit with
      // one on all paths.  There are possibly also jumps to is_true
      // from nested subexpressions.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      // Avoid popping the result if it converts to 'false' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      //
      // Duplicate the TOS value. The duplicate will be popped by
      // ToBoolean.
      frame_->Dup();
      ControlDestination dest(&pop_and_continue, &exit, true);
      ToBoolean(&dest);

      // Pop the result of evaluating the first part.
      frame_->Drop();

      // Compile right side expression.
      is_true.Bind();
      Load(node->right());

      // Exit (always with a materialized value).
      exit.Bind();
    }

  } else {
    ASSERT(node->op() == Token::OR);
    JumpTarget is_false;
    ControlDestination dest(destination()->true_target(), &is_false, false);
    LoadCondition(node->left(), &dest, false);

    if (dest.true_was_fall_through()) {
      // The current true target was used as the fall-through.  If
      // there are no dangling jumps to is_false then the left
      // subexpression was unconditionally true.  Otherwise we have
      // paths where we do have to evaluate the right subexpression.
      if (is_false.is_linked()) {
        // We need to compile the right subexpression.  If the jump to
        // the current true target was a forward jump then we have a
        // valid frame, we have just bound the true target, and we
        // have to jump around the code for the right subexpression.
        if (has_valid_frame()) {
          destination()->true_target()->Unuse();
          destination()->true_target()->Jump();
        }
        is_false.Bind();
        // The left subexpression compiled to control flow, so the
        // right one is free to do so as well.
        LoadCondition(node->right(), destination(), false);
      } else {
        // We have just jumped to or bound the current true target but
        // the current control destination is not marked as used.
        destination()->Use(true);
      }

    } else if (dest.is_used()) {
      // The left subexpression compiled to control flow (and is_false
      // was just bound), so the right is free to do so as well.
      LoadCondition(node->right(), destination(), false);

    } else {
      // We have a materialized value on the frame, so we exit with
      // one on all paths.  There are possibly also jumps to is_false
      // from nested subexpressions.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      // Avoid popping the result if it converts to 'true' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      //
      // Duplicate the TOS value. The duplicate will be popped by
      // ToBoolean.
      frame_->Dup();
      ControlDestination dest(&exit, &pop_and_continue, false);
      ToBoolean(&dest);

      // Pop the result of evaluating the first part.
      frame_->Drop();

      // Compile right side expression.
      is_false.Bind();
      Load(node->right());

      // Exit (always with a materialized value).
      exit.Bind();
    }
  }
}

void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
  Comment cmnt(masm_, "[ BinaryOperation");

  if (node->op() == Token::AND || node->op() == Token::OR) {
    GenerateLogicalBooleanOperation(node);
  } else {
    // NOTE: The code below assumes that the slow cases (calls to runtime)
    // never return a constant/immutable object.
    OverwriteMode overwrite_mode = NO_OVERWRITE;
    if (node->left()->ResultOverwriteAllowed()) {
      overwrite_mode = OVERWRITE_LEFT;
    } else if (node->right()->ResultOverwriteAllowed()) {
      overwrite_mode = OVERWRITE_RIGHT;
    }

    if (node->left()->IsTrivial()) {
      Load(node->right());
      Result right = frame_->Pop();
      frame_->Push(node->left());
      frame_->Push(&right);
    } else {
      Load(node->left());
      Load(node->right());
    }
    GenericBinaryOperation(node, overwrite_mode);
  }
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
  frame_->PushFunction();
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
  Comment cmnt(masm_, "[ CompareOperation");

  // Get the expressions from the node.
  Expression* left = node->left();
  Expression* right = node->right();
  Token::Value op = node->op();
  // To make typeof testing for natives implemented in JavaScript really
  // efficient, we generate special code for expressions of the form:
  // 'typeof <expression> == <string>'.
  UnaryOperation* operation = left->AsUnaryOperation();
  if ((op == Token::EQ || op == Token::EQ_STRICT) &&
      (operation != NULL && operation->op() == Token::TYPEOF) &&
      (right->AsLiteral() != NULL &&
       right->AsLiteral()->handle()->IsString())) {
    Handle<String> check(Handle<String>::cast(right->AsLiteral()->handle()));

    // Load the operand and move it to a register.
    LoadTypeofExpression(operation->expression());
    Result answer = frame_->Pop();
    answer.ToRegister();

    if (check->Equals(Heap::number_symbol())) {
      Condition is_smi = masm_->CheckSmi(answer.reg());
      destination()->true_target()->Branch(is_smi);
      frame_->Spill(answer.reg());
      __ movq(answer.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ CompareRoot(answer.reg(), Heap::kHeapNumberMapRootIndex);
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::string_symbol())) {
      Condition is_smi = masm_->CheckSmi(answer.reg());
      destination()->false_target()->Branch(is_smi);

      // It can be an undetectable string object.
      __ movq(kScratchRegister,
              FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ testb(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
               Immediate(1 << Map::kIsUndetectable));
      destination()->false_target()->Branch(not_zero);
      __ CmpInstanceType(kScratchRegister, FIRST_NONSTRING_TYPE);
      answer.Unuse();
      destination()->Split(below);  // Unsigned byte comparison needed.

    } else if (check->Equals(Heap::boolean_symbol())) {
      __ CompareRoot(answer.reg(), Heap::kTrueValueRootIndex);
      destination()->true_target()->Branch(equal);
      __ CompareRoot(answer.reg(), Heap::kFalseValueRootIndex);
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::undefined_symbol())) {
      __ CompareRoot(answer.reg(), Heap::kUndefinedValueRootIndex);
      destination()->true_target()->Branch(equal);

      Condition is_smi = masm_->CheckSmi(answer.reg());
      destination()->false_target()->Branch(is_smi);

      // It can be an undetectable object.
      __ movq(kScratchRegister,
              FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ testb(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
               Immediate(1 << Map::kIsUndetectable));
      answer.Unuse();
      destination()->Split(not_zero);

    } else if (check->Equals(Heap::function_symbol())) {
      Condition is_smi = masm_->CheckSmi(answer.reg());
      destination()->false_target()->Branch(is_smi);
      frame_->Spill(answer.reg());
      __ CmpObjectType(answer.reg(), JS_FUNCTION_TYPE, answer.reg());
      destination()->true_target()->Branch(equal);
      // Regular expressions are callable so typeof == 'function'.
      __ CmpInstanceType(answer.reg(), JS_REGEXP_TYPE);
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::object_symbol())) {
      Condition is_smi = masm_->CheckSmi(answer.reg());
      destination()->false_target()->Branch(is_smi);
      __ CompareRoot(answer.reg(), Heap::kNullValueRootIndex);
      destination()->true_target()->Branch(equal);

      // Regular expressions are typeof == 'function', not 'object'.
      __ CmpObjectType(answer.reg(), JS_REGEXP_TYPE, kScratchRegister);
      destination()->false_target()->Branch(equal);

      // It can be an undetectable object.
      __ testb(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
               Immediate(1 << Map::kIsUndetectable));
      destination()->false_target()->Branch(not_zero);
      __ CmpInstanceType(kScratchRegister, FIRST_JS_OBJECT_TYPE);
      destination()->false_target()->Branch(below);
      __ CmpInstanceType(kScratchRegister, LAST_JS_OBJECT_TYPE);
      answer.Unuse();
      destination()->Split(below_equal);
    } else {
      // Uncommon case: typeof testing against a string literal that is
      // never returned from the typeof operator.
      answer.Unuse();
      destination()->Goto(false);
    }
    return;
  }

  Condition cc = no_condition;
  bool strict = false;
  switch (op) {
    case Token::EQ_STRICT:
      strict = true;
      // Fall through
    case Token::EQ:
      cc = equal;
      break;
    case Token::LT:
      cc = less;
      break;
    case Token::GT:
      cc = greater;
      break;
    case Token::LTE:
      cc = less_equal;
      break;
    case Token::GTE:
      cc = greater_equal;
      break;
    case Token::IN: {
      Load(left);
      Load(right);
      Result answer = frame_->InvokeBuiltin(Builtins::IN, CALL_FUNCTION, 2);
      frame_->Push(&answer);  // push the result
      return;
    }
    case Token::INSTANCEOF: {
      Load(left);
      Load(right);
      InstanceofStub stub;
      Result answer = frame_->CallStub(&stub, 2);
      answer.ToRegister();
      __ testq(answer.reg(), answer.reg());
      answer.Unuse();
      destination()->Split(zero);
      return;
    }
    default:
      UNREACHABLE();
  }

  if (left->IsTrivial()) {
    Load(right);
    Result right_result = frame_->Pop();
    frame_->Push(left);
    frame_->Push(&right_result);
  } else {
    Load(left);
    Load(right);
  }

  Comparison(node, cc, strict, destination());
}


void CodeGenerator::VisitCompareToNull(CompareToNull* node) {
  Comment cmnt(masm_, "[ CompareToNull");

  Load(node->expression());
  Result operand = frame_->Pop();
  operand.ToRegister();
  __ CompareRoot(operand.reg(), Heap::kNullValueRootIndex);
  if (node->is_strict()) {
    operand.Unuse();
    destination()->Split(equal);
  } else {
    // The 'null' value is only equal to 'undefined' if using non-strict
    // comparisons.
    destination()->true_target()->Branch(equal);
    __ CompareRoot(operand.reg(), Heap::kUndefinedValueRootIndex);
    destination()->true_target()->Branch(equal);
    Condition is_smi = masm_->CheckSmi(operand.reg());
    destination()->false_target()->Branch(is_smi);

    // It can be an undetectable object.
    // Use a scratch register in preference to spilling operand.reg().
    Result temp = allocator()->Allocate();
    ASSERT(temp.is_valid());
    __ movq(temp.reg(),
            FieldOperand(operand.reg(), HeapObject::kMapOffset));
    __ testb(FieldOperand(temp.reg(), Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    temp.Unuse();
    operand.Unuse();
    destination()->Split(not_zero);
  }
}


#ifdef DEBUG
bool CodeGenerator::HasValidEntryRegisters() {
  return (allocator()->count(rax) == (frame()->is_used(rax) ? 1 : 0))
      && (allocator()->count(rbx) == (frame()->is_used(rbx) ? 1 : 0))
      && (allocator()->count(rcx) == (frame()->is_used(rcx) ? 1 : 0))
      && (allocator()->count(rdx) == (frame()->is_used(rdx) ? 1 : 0))
      && (allocator()->count(rdi) == (frame()->is_used(rdi) ? 1 : 0))
      && (allocator()->count(r8) == (frame()->is_used(r8) ? 1 : 0))
      && (allocator()->count(r9) == (frame()->is_used(r9) ? 1 : 0))
      && (allocator()->count(r11) == (frame()->is_used(r11) ? 1 : 0))
      && (allocator()->count(r14) == (frame()->is_used(r14) ? 1 : 0))
      && (allocator()->count(r12) == (frame()->is_used(r12) ? 1 : 0));
}
#endif



// Emit a LoadIC call to get the value from receiver and leave it in
// dst.  The receiver register is restored after the call.
class DeferredReferenceGetNamedValue: public DeferredCode {
 public:
  DeferredReferenceGetNamedValue(Register dst,
                                 Register receiver,
                                 Handle<String> name)
      : dst_(dst), receiver_(receiver),  name_(name) {
    set_comment("[ DeferredReferenceGetNamedValue");
  }

  virtual void Generate();

  Label* patch_site() { return &patch_site_; }

 private:
  Label patch_site_;
  Register dst_;
  Register receiver_;
  Handle<String> name_;
};


void DeferredReferenceGetNamedValue::Generate() {
  if (!receiver_.is(rax)) {
    __ movq(rax, receiver_);
  }
  __ Move(rcx, name_);
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // The call must be followed by a test rax instruction to indicate
  // that the inobject property case was inlined.
  //
  // Store the delta to the map check instruction here in the test
  // instruction.  Use masm_-> instead of the __ macro since the
  // latter can't return a value.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  masm_->testl(rax, Immediate(-delta_to_patch_site));
  __ IncrementCounter(&Counters::named_load_inline_miss, 1);

  if (!dst_.is(rax)) __ movq(dst_, rax);
}


class DeferredReferenceGetKeyedValue: public DeferredCode {
 public:
  explicit DeferredReferenceGetKeyedValue(Register dst,
                                          Register receiver,
                                          Register key)
      : dst_(dst), receiver_(receiver), key_(key) {
    set_comment("[ DeferredReferenceGetKeyedValue");
  }

  virtual void Generate();

  Label* patch_site() { return &patch_site_; }

 private:
  Label patch_site_;
  Register dst_;
  Register receiver_;
  Register key_;
};


void DeferredReferenceGetKeyedValue::Generate() {
  if (receiver_.is(rdx)) {
    if (!key_.is(rax)) {
      __ movq(rax, key_);
    }  // else do nothing.
  } else if (receiver_.is(rax)) {
    if (key_.is(rdx)) {
      __ xchg(rax, rdx);
    } else if (key_.is(rax)) {
      __ movq(rdx, receiver_);
    } else {
      __ movq(rdx, receiver_);
      __ movq(rax, key_);
    }
  } else if (key_.is(rax)) {
    __ movq(rdx, receiver_);
  } else {
    __ movq(rax, key_);
    __ movq(rdx, receiver_);
  }
  // Calculate the delta from the IC call instruction to the map check
  // movq instruction in the inlined version.  This delta is stored in
  // a test(rax, delta) instruction after the call so that we can find
  // it in the IC initialization code and patch the movq instruction.
  // This means that we cannot allow test instructions after calls to
  // KeyedLoadIC stubs in other places.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // The delta from the start of the map-compare instruction to the
  // test instruction.  We use masm_-> directly here instead of the __
  // macro because the macro sometimes uses macro expansion to turn
  // into something that can't return a value.  This is encountered
  // when doing generated code coverage tests.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  // TODO(X64): Consider whether it's worth switching the test to a
  // 7-byte NOP with non-zero immediate (0f 1f 80 xxxxxxxx) which won't
  // be generated normally.
  masm_->testl(rax, Immediate(-delta_to_patch_site));
  __ IncrementCounter(&Counters::keyed_load_inline_miss, 1);

  if (!dst_.is(rax)) __ movq(dst_, rax);
}


class DeferredReferenceSetKeyedValue: public DeferredCode {
 public:
  DeferredReferenceSetKeyedValue(Register value,
                                 Register key,
                                 Register receiver)
      : value_(value), key_(key), receiver_(receiver) {
    set_comment("[ DeferredReferenceSetKeyedValue");
  }

  virtual void Generate();

  Label* patch_site() { return &patch_site_; }

 private:
  Register value_;
  Register key_;
  Register receiver_;
  Label patch_site_;
};


void DeferredReferenceSetKeyedValue::Generate() {
  __ IncrementCounter(&Counters::keyed_store_inline_miss, 1);
  // Move value, receiver, and key to registers rax, rdx, and rcx, as
  // the IC stub expects.
  // Move value to rax, using xchg if the receiver or key is in rax.
  if (!value_.is(rax)) {
    if (!receiver_.is(rax) && !key_.is(rax)) {
      __ movq(rax, value_);
    } else {
      __ xchg(rax, value_);
      // Update receiver_ and key_ if they are affected by the swap.
      if (receiver_.is(rax)) {
        receiver_ = value_;
      } else if (receiver_.is(value_)) {
        receiver_ = rax;
      }
      if (key_.is(rax)) {
        key_ = value_;
      } else if (key_.is(value_)) {
        key_ = rax;
      }
    }
  }
  // Value is now in rax. Its original location is remembered in value_,
  // and the value is restored to value_ before returning.
  // The variables receiver_ and key_ are not preserved.
  // Move receiver and key to rdx and rcx, swapping if necessary.
  if (receiver_.is(rdx)) {
    if (!key_.is(rcx)) {
      __ movq(rcx, key_);
    }  // Else everything is already in the right place.
  } else if (receiver_.is(rcx)) {
    if (key_.is(rdx)) {
      __ xchg(rcx, rdx);
    } else if (key_.is(rcx)) {
      __ movq(rdx, receiver_);
    } else {
      __ movq(rdx, receiver_);
      __ movq(rcx, key_);
    }
  } else if (key_.is(rcx)) {
    __ movq(rdx, receiver_);
  } else {
    __ movq(rcx, key_);
    __ movq(rdx, receiver_);
  }

  // Call the IC stub.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // The delta from the start of the map-compare instructions (initial movq)
  // to the test instruction.  We use masm_-> directly here instead of the
  // __ macro because the macro sometimes uses macro expansion to turn
  // into something that can't return a value.  This is encountered
  // when doing generated code coverage tests.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  masm_->testl(rax, Immediate(-delta_to_patch_site));
  // Restore value (returned from store IC).
  if (!value_.is(rax)) __ movq(value_, rax);
}


Result CodeGenerator::EmitNamedLoad(Handle<String> name, bool is_contextual) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Result result;
  // Do not inline the inobject property case for loads from the global
  // object.  Also do not inline for unoptimized code.  This saves time
  // in the code generator.  Unoptimized code is toplevel code or code
  // that is not in a loop.
  if (is_contextual || scope()->is_global_scope() || loop_nesting() == 0) {
    Comment cmnt(masm(), "[ Load from named Property");
    frame()->Push(name);

    RelocInfo::Mode mode = is_contextual
        ? RelocInfo::CODE_TARGET_CONTEXT
        : RelocInfo::CODE_TARGET;
    result = frame()->CallLoadIC(mode);
    // A test rax instruction following the call signals that the
    // inobject property case was inlined.  Ensure that there is not
    // a test rax instruction here.
    __ nop();
  } else {
    // Inline the inobject property case.
    Comment cmnt(masm(), "[ Inlined named property load");
    Result receiver = frame()->Pop();
    receiver.ToRegister();
    result = allocator()->Allocate();
    ASSERT(result.is_valid());

    // Cannot use r12 for receiver, because that changes
    // the distance between a call and a fixup location,
    // due to a special encoding of r12 as r/m in a ModR/M byte.
    if (receiver.reg().is(r12)) {
      frame()->Spill(receiver.reg());  // It will be overwritten with result.
      // Swap receiver and value.
      __ movq(result.reg(), receiver.reg());
      Result temp = receiver;
      receiver = result;
      result = temp;
    }

    DeferredReferenceGetNamedValue* deferred =
        new DeferredReferenceGetNamedValue(result.reg(), receiver.reg(), name);

    // Check that the receiver is a heap object.
    __ JumpIfSmi(receiver.reg(), deferred->entry_label());

    __ bind(deferred->patch_site());
    // This is the map check instruction that will be patched (so we can't
    // use the double underscore macro that may insert instructions).
    // Initially use an invalid map to force a failure.
    masm()->Move(kScratchRegister, Factory::null_value());
    masm()->cmpq(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                 kScratchRegister);
    // This branch is always a forwards branch so it's always a fixed
    // size which allows the assert below to succeed and patching to work.
    // Don't use deferred->Branch(...), since that might add coverage code.
    masm()->j(not_equal, deferred->entry_label());

    // The delta from the patch label to the load offset must be
    // statically known.
    ASSERT(masm()->SizeOfCodeGeneratedSince(deferred->patch_site()) ==
           LoadIC::kOffsetToLoadInstruction);
    // The initial (invalid) offset has to be large enough to force
    // a 32-bit instruction encoding to allow patching with an
    // arbitrary offset.  Use kMaxInt (minus kHeapObjectTag).
    int offset = kMaxInt;
    masm()->movq(result.reg(), FieldOperand(receiver.reg(), offset));

    __ IncrementCounter(&Counters::named_load_inline, 1);
    deferred->BindExit();
  }
  ASSERT(frame()->height() == original_height - 1);
  return result;
}


Result CodeGenerator::EmitNamedStore(Handle<String> name, bool is_contextual) {
#ifdef DEBUG
  int expected_height = frame()->height() - (is_contextual ? 1 : 2);
#endif

  Result result;
  if (is_contextual || scope()->is_global_scope() || loop_nesting() == 0) {
      result = frame()->CallStoreIC(name, is_contextual);
      // A test rax instruction following the call signals that the inobject
      // property case was inlined.  Ensure that there is not a test rax
      // instruction here.
      __ nop();
  } else {
    // Inline the in-object property case.
    JumpTarget slow, done;
    Label patch_site;

    // Get the value and receiver from the stack.
    Result value = frame()->Pop();
    value.ToRegister();
    Result receiver = frame()->Pop();
    receiver.ToRegister();

    // Allocate result register.
    result = allocator()->Allocate();
    ASSERT(result.is_valid() && receiver.is_valid() && value.is_valid());

    // Cannot use r12 for receiver, because that changes
    // the distance between a call and a fixup location,
    // due to a special encoding of r12 as r/m in a ModR/M byte.
    if (receiver.reg().is(r12)) {
      frame()->Spill(receiver.reg());  // It will be overwritten with result.
      // Swap receiver and value.
      __ movq(result.reg(), receiver.reg());
      Result temp = receiver;
      receiver = result;
      result = temp;
    }

    // Check that the receiver is a heap object.
    Condition is_smi = __ CheckSmi(receiver.reg());
    slow.Branch(is_smi, &value, &receiver);

    // This is the map check instruction that will be patched.
    // Initially use an invalid map to force a failure. The exact
    // instruction sequence is important because we use the
    // kOffsetToStoreInstruction constant for patching. We avoid using
    // the __ macro for the following two instructions because it
    // might introduce extra instructions.
    __ bind(&patch_site);
    masm()->Move(kScratchRegister, Factory::null_value());
    masm()->cmpq(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                 kScratchRegister);
    // This branch is always a forwards branch so it's always a fixed size
    // which allows the assert below to succeed and patching to work.
    slow.Branch(not_equal, &value, &receiver);

    // The delta from the patch label to the store offset must be
    // statically known.
    ASSERT(masm()->SizeOfCodeGeneratedSince(&patch_site) ==
           StoreIC::kOffsetToStoreInstruction);

    // The initial (invalid) offset has to be large enough to force a 32-bit
    // instruction encoding to allow patching with an arbitrary offset.  Use
    // kMaxInt (minus kHeapObjectTag).
    int offset = kMaxInt;
    __ movq(FieldOperand(receiver.reg(), offset), value.reg());
    __ movq(result.reg(), value.reg());

    // Allocate scratch register for write barrier.
    Result scratch = allocator()->Allocate();
    ASSERT(scratch.is_valid());

    // The write barrier clobbers all input registers, so spill the
    // receiver and the value.
    frame_->Spill(receiver.reg());
    frame_->Spill(value.reg());

    // If the receiver and the value share a register allocate a new
    // register for the receiver.
    if (receiver.reg().is(value.reg())) {
      receiver = allocator()->Allocate();
      ASSERT(receiver.is_valid());
      __ movq(receiver.reg(), value.reg());
    }

    // Update the write barrier. To save instructions in the inlined
    // version we do not filter smis.
    Label skip_write_barrier;
    __ InNewSpace(receiver.reg(), value.reg(), equal, &skip_write_barrier);
    int delta_to_record_write = masm_->SizeOfCodeGeneratedSince(&patch_site);
    __ lea(scratch.reg(), Operand(receiver.reg(), offset));
    __ RecordWriteHelper(receiver.reg(), scratch.reg(), value.reg());
    if (FLAG_debug_code) {
      __ movq(receiver.reg(), BitCast<int64_t>(kZapValue), RelocInfo::NONE);
      __ movq(value.reg(), BitCast<int64_t>(kZapValue), RelocInfo::NONE);
      __ movq(scratch.reg(), BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    }
    __ bind(&skip_write_barrier);
    value.Unuse();
    scratch.Unuse();
    receiver.Unuse();
    done.Jump(&result);

    slow.Bind(&value, &receiver);
    frame()->Push(&receiver);
    frame()->Push(&value);
    result = frame()->CallStoreIC(name, is_contextual);
    // Encode the offset to the map check instruction and the offset
    // to the write barrier store address computation in a test rax
    // instruction.
    int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(&patch_site);
    __ testl(rax,
             Immediate((delta_to_record_write << 16) | delta_to_patch_site));
    done.Bind(&result);
  }

  ASSERT_EQ(expected_height, frame()->height());
  return result;
}


Result CodeGenerator::EmitKeyedLoad() {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Result result;
  // Inline array load code if inside of a loop.  We do not know
  // the receiver map yet, so we initially generate the code with
  // a check against an invalid map.  In the inline cache code, we
  // patch the map check if appropriate.
  if (loop_nesting() > 0) {
    Comment cmnt(masm_, "[ Inlined load from keyed Property");

    // Use a fresh temporary to load the elements without destroying
    // the receiver which is needed for the deferred slow case.
    // Allocate the temporary early so that we use rax if it is free.
    Result elements = allocator()->Allocate();
    ASSERT(elements.is_valid());

    Result key = frame_->Pop();
    Result receiver = frame_->Pop();
    key.ToRegister();
    receiver.ToRegister();

    // If key and receiver are shared registers on the frame, their values will
    // be automatically saved and restored when going to deferred code.
    // The result is returned in elements, which is not shared.
    DeferredReferenceGetKeyedValue* deferred =
        new DeferredReferenceGetKeyedValue(elements.reg(),
                                           receiver.reg(),
                                           key.reg());

    __ JumpIfSmi(receiver.reg(), deferred->entry_label());

    // Check that the receiver has the expected map.
    // Initially, use an invalid map. The map is patched in the IC
    // initialization code.
    __ bind(deferred->patch_site());
    // Use masm-> here instead of the double underscore macro since extra
    // coverage code can interfere with the patching.  Do not use a load
    // from the root array to load null_value, since the load must be patched
    // with the expected receiver map, which is not in the root array.
    masm_->movq(kScratchRegister, Factory::null_value(),
                RelocInfo::EMBEDDED_OBJECT);
    masm_->cmpq(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                kScratchRegister);
    deferred->Branch(not_equal);

    // Check that the key is a non-negative smi.
    __ JumpIfNotPositiveSmi(key.reg(), deferred->entry_label());

    // Get the elements array from the receiver.
    __ movq(elements.reg(),
            FieldOperand(receiver.reg(), JSObject::kElementsOffset));
    __ AssertFastElements(elements.reg());

    // Check that key is within bounds.
    __ SmiCompare(key.reg(),
                  FieldOperand(elements.reg(), FixedArray::kLengthOffset));
    deferred->Branch(above_equal);

    // Load and check that the result is not the hole.  We could
    // reuse the index or elements register for the value.
    //
    // TODO(206): Consider whether it makes sense to try some
    // heuristic about which register to reuse.  For example, if
    // one is rax, the we can reuse that one because the value
    // coming from the deferred code will be in rax.
    SmiIndex index =
        masm_->SmiToIndex(kScratchRegister, key.reg(), kPointerSizeLog2);
    __ movq(elements.reg(),
            FieldOperand(elements.reg(),
                         index.reg,
                         index.scale,
                         FixedArray::kHeaderSize));
    result = elements;
    __ CompareRoot(result.reg(), Heap::kTheHoleValueRootIndex);
    deferred->Branch(equal);
    __ IncrementCounter(&Counters::keyed_load_inline, 1);

    deferred->BindExit();
  } else {
    Comment cmnt(masm_, "[ Load from keyed Property");
    result = frame_->CallKeyedLoadIC(RelocInfo::CODE_TARGET);
    // Make sure that we do not have a test instruction after the
    // call.  A test instruction after the call is used to
    // indicate that we have generated an inline version of the
    // keyed load.  The explicit nop instruction is here because
    // the push that follows might be peep-hole optimized away.
    __ nop();
  }
  ASSERT(frame()->height() == original_height - 2);
  return result;
}


Result CodeGenerator::EmitKeyedStore(StaticType* key_type) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Result result;
  // Generate inlined version of the keyed store if the code is in a loop
  // and the key is likely to be a smi.
  if (loop_nesting() > 0 && key_type->IsLikelySmi()) {
    Comment cmnt(masm(), "[ Inlined store to keyed Property");

    // Get the receiver, key and value into registers.
    result = frame()->Pop();
    Result key = frame()->Pop();
    Result receiver = frame()->Pop();

    Result tmp = allocator_->Allocate();
    ASSERT(tmp.is_valid());
    Result tmp2 = allocator_->Allocate();
    ASSERT(tmp2.is_valid());

    // Determine whether the value is a constant before putting it in a
    // register.
    bool value_is_constant = result.is_constant();

    // Make sure that value, key and receiver are in registers.
    result.ToRegister();
    key.ToRegister();
    receiver.ToRegister();

    DeferredReferenceSetKeyedValue* deferred =
        new DeferredReferenceSetKeyedValue(result.reg(),
                                           key.reg(),
                                           receiver.reg());

    // Check that the receiver is not a smi.
    __ JumpIfSmi(receiver.reg(), deferred->entry_label());

    // Check that the key is a smi.
    if (!key.is_smi()) {
      __ JumpIfNotSmi(key.reg(), deferred->entry_label());
    } else if (FLAG_debug_code) {
      __ AbortIfNotSmi(key.reg());
    }

    // Check that the receiver is a JSArray.
    __ CmpObjectType(receiver.reg(), JS_ARRAY_TYPE, kScratchRegister);
    deferred->Branch(not_equal);

    // Check that the key is within bounds.  Both the key and the length of
    // the JSArray are smis. Use unsigned comparison to handle negative keys.
    __ SmiCompare(FieldOperand(receiver.reg(), JSArray::kLengthOffset),
                  key.reg());
    deferred->Branch(below_equal);

    // Get the elements array from the receiver and check that it is not a
    // dictionary.
    __ movq(tmp.reg(),
            FieldOperand(receiver.reg(), JSArray::kElementsOffset));

    // Check whether it is possible to omit the write barrier. If the elements
    // array is in new space or the value written is a smi we can safely update
    // the elements array without write barrier.
    Label in_new_space;
    __ InNewSpace(tmp.reg(), tmp2.reg(), equal, &in_new_space);
    if (!value_is_constant) {
      __ JumpIfNotSmi(result.reg(), deferred->entry_label());
    }

    __ bind(&in_new_space);
    // Bind the deferred code patch site to be able to locate the fixed
    // array map comparison.  When debugging, we patch this comparison to
    // always fail so that we will hit the IC call in the deferred code
    // which will allow the debugger to break for fast case stores.
    __ bind(deferred->patch_site());
    // Avoid using __ to ensure the distance from patch_site
    // to the map address is always the same.
    masm()->movq(kScratchRegister, Factory::fixed_array_map(),
               RelocInfo::EMBEDDED_OBJECT);
    __ cmpq(FieldOperand(tmp.reg(), HeapObject::kMapOffset),
            kScratchRegister);
    deferred->Branch(not_equal);

    // Store the value.
    SmiIndex index =
        masm()->SmiToIndex(kScratchRegister, key.reg(), kPointerSizeLog2);
    __ movq(FieldOperand(tmp.reg(),
                         index.reg,
                         index.scale,
                         FixedArray::kHeaderSize),
            result.reg());
    __ IncrementCounter(&Counters::keyed_store_inline, 1);

    deferred->BindExit();
  } else {
    result = frame()->CallKeyedStoreIC();
    // Make sure that we do not have a test instruction after the
    // call.  A test instruction after the call is used to
    // indicate that we have generated an inline version of the
    // keyed store.
    __ nop();
  }
  ASSERT(frame()->height() == original_height - 3);
  return result;
}


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


void Reference::GetValue() {
  ASSERT(!cgen_->in_spilled_code());
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_illegal());
  MacroAssembler* masm = cgen_->masm();

  // Record the source position for the property load.
  Property* property = expression_->AsProperty();
  if (property != NULL) {
    cgen_->CodeForSourcePosition(property->position());
  }

  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Load from Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      cgen_->LoadFromSlotCheckForArguments(slot, NOT_INSIDE_TYPEOF);
      break;
    }

    case NAMED: {
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());
      if (persist_after_get_) {
        cgen_->frame()->Dup();
      }
      Result result = cgen_->EmitNamedLoad(GetName(), is_global);
      cgen_->frame()->Push(&result);
      break;
    }

    case KEYED: {
      // A load of a bare identifier (load from global) cannot be keyed.
      ASSERT(expression_->AsVariableProxy()->AsVariable() == NULL);
      if (persist_after_get_) {
        cgen_->frame()->PushElementAt(1);
        cgen_->frame()->PushElementAt(1);
      }
      Result value = cgen_->EmitKeyedLoad();
      cgen_->frame()->Push(&value);
      break;
    }

    default:
      UNREACHABLE();
  }

  if (!persist_after_get_) {
    set_unloaded();
  }
}


void Reference::TakeValue() {
  // TODO(X64): This function is completely architecture independent. Move
  // it somewhere shared.

  // For non-constant frame-allocated slots, we invalidate the value in the
  // slot.  For all others, we fall back on GetValue.
  ASSERT(!cgen_->in_spilled_code());
  ASSERT(!is_illegal());
  if (type_ != SLOT) {
    GetValue();
    return;
  }

  Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
  ASSERT(slot != NULL);
  if (slot->type() == Slot::LOOKUP ||
      slot->type() == Slot::CONTEXT ||
      slot->var()->mode() == Variable::CONST ||
      slot->is_arguments()) {
    GetValue();
    return;
  }

  // Only non-constant, frame-allocated parameters and locals can reach
  // here.  Be careful not to use the optimizations for arguments
  // object access since it may not have been initialized yet.
  ASSERT(!slot->is_arguments());
  if (slot->type() == Slot::PARAMETER) {
    cgen_->frame()->TakeParameterAt(slot->index());
  } else {
    ASSERT(slot->type() == Slot::LOCAL);
    cgen_->frame()->TakeLocalAt(slot->index());
  }

  ASSERT(persist_after_get_);
  // Do not unload the reference, because it is used in SetValue.
}


void Reference::SetValue(InitState init_state) {
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_illegal());
  MacroAssembler* masm = cgen_->masm();
  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Store to Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      cgen_->StoreToSlot(slot, init_state);
      set_unloaded();
      break;
    }

    case NAMED: {
      Comment cmnt(masm, "[ Store to named Property");
      Result answer = cgen_->EmitNamedStore(GetName(), false);
      cgen_->frame()->Push(&answer);
      set_unloaded();
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Store to keyed Property");
      Property* property = expression()->AsProperty();
      ASSERT(property != NULL);

      Result answer = cgen_->EmitKeyedStore(property->key()->type());
      cgen_->frame()->Push(&answer);
      set_unloaded();
      break;
    }

    case UNLOADED:
    case ILLEGAL:
      UNREACHABLE();
  }
}


Result CodeGenerator::GenerateGenericBinaryOpStubCall(GenericBinaryOpStub* stub,
                                                      Result* left,
                                                      Result* right) {
  if (stub->ArgsInRegistersSupported()) {
    stub->SetArgsInRegisters();
    return frame_->CallStub(stub, left, right);
  } else {
    frame_->Push(left);
    frame_->Push(right);
    return frame_->CallStub(stub, 2);
  }
}

#undef __

#define __ masm.

#ifdef _WIN64
typedef double (*ModuloFunction)(double, double);
// Define custom fmod implementation.
ModuloFunction CreateModuloFunction() {
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler masm(buffer, static_cast<int>(actual_size));
  // Generated code is put into a fixed, unmovable, buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // Windows 64 ABI passes double arguments in xmm0, xmm1 and
  // returns result in xmm0.
  // Argument backing space is allocated on the stack above
  // the return address.

  // Compute x mod y.
  // Load y and x (use argument backing store as temporary storage).
  __ movsd(Operand(rsp, kPointerSize * 2), xmm1);
  __ movsd(Operand(rsp, kPointerSize), xmm0);
  __ fld_d(Operand(rsp, kPointerSize * 2));
  __ fld_d(Operand(rsp, kPointerSize));

  // Clear exception flags before operation.
  {
    Label no_exceptions;
    __ fwait();
    __ fnstsw_ax();
    // Clear if Illegal Operand or Zero Division exceptions are set.
    __ testb(rax, Immediate(5));
    __ j(zero, &no_exceptions);
    __ fnclex();
    __ bind(&no_exceptions);
  }

  // Compute st(0) % st(1)
  {
    Label partial_remainder_loop;
    __ bind(&partial_remainder_loop);
    __ fprem();
    __ fwait();
    __ fnstsw_ax();
    __ testl(rax, Immediate(0x400 /* C2 */));
    // If C2 is set, computation only has partial result. Loop to
    // continue computation.
    __ j(not_zero, &partial_remainder_loop);
  }

  Label valid_result;
  Label return_result;
  // If Invalid Operand or Zero Division exceptions are set,
  // return NaN.
  __ testb(rax, Immediate(5));
  __ j(zero, &valid_result);
  __ fstp(0);  // Drop result in st(0).
  int64_t kNaNValue = V8_INT64_C(0x7ff8000000000000);
  __ movq(rcx, kNaNValue, RelocInfo::NONE);
  __ movq(Operand(rsp, kPointerSize), rcx);
  __ movsd(xmm0, Operand(rsp, kPointerSize));
  __ jmp(&return_result);

  // If result is valid, return that.
  __ bind(&valid_result);
  __ fstp_d(Operand(rsp, kPointerSize));
  __ movsd(xmm0, Operand(rsp, kPointerSize));

  // Clean up FPU stack and exceptions and return xmm0
  __ bind(&return_result);
  __ fstp(0);  // Unload y.

  Label clear_exceptions;
  __ testb(rax, Immediate(0x3f /* Any Exception*/));
  __ j(not_zero, &clear_exceptions);
  __ ret(0);
  __ bind(&clear_exceptions);
  __ fnclex();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(&desc);
  // Call the function from C++.
  return FUNCTION_CAST<ModuloFunction>(buffer);
}

#endif


#undef __

void RecordWriteStub::Generate(MacroAssembler* masm) {
  masm->RecordWriteHelper(object_, addr_, scratch_);
  masm->ret(0);
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
