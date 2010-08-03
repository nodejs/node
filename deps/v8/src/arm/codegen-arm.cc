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

#if defined(V8_TARGET_ARCH_ARM)

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "ic-inl.h"
#include "jsregexp.h"
#include "jump-target-light-inl.h"
#include "parser.h"
#include "regexp-macro-assembler.h"
#include "regexp-stack.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"
#include "virtual-frame-inl.h"
#include "virtual-frame-arm-inl.h"

namespace v8 {
namespace internal {


static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cc,
                                          bool never_nan_nan);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs);
static void MultiplyByKnownInt(MacroAssembler* masm,
                               Register source,
                               Register destination,
                               int known_int);
static bool IsEasyToMultiplyBy(int x);


#define __ ACCESS_MASM(masm_)

// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.

void DeferredCode::SaveRegisters() {
  // On ARM you either have a completely spilled frame or you
  // handle it yourself, but at the moment there's no automation
  // of registers and deferred code.
}


void DeferredCode::RestoreRegisters() {
}


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void VirtualFrameRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  frame_state_->frame()->AssertIsSpilled();
}


void VirtualFrameRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
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
      previous_(owner->state()) {
  owner->set_state(this);
}


ConditionCodeGenState::ConditionCodeGenState(CodeGenerator* owner,
                                             JumpTarget* true_target,
                                             JumpTarget* false_target)
    : CodeGenState(owner),
      true_target_(true_target),
      false_target_(false_target) {
  owner->set_state(this);
}


TypeInfoCodeGenState::TypeInfoCodeGenState(CodeGenerator* owner,
                                           Slot* slot,
                                           TypeInfo type_info)
    : CodeGenState(owner),
      slot_(slot) {
  owner->set_state(this);
  old_type_info_ = owner->set_type_info(slot, type_info);
}


CodeGenState::~CodeGenState() {
  ASSERT(owner_->state() == this);
  owner_->set_state(previous_);
}


TypeInfoCodeGenState::~TypeInfoCodeGenState() {
  owner()->set_type_info(slot_, old_type_info_);
}

// -------------------------------------------------------------------------
// CodeGenerator implementation

int CodeGenerator::inlined_write_barrier_size_ = -1;

CodeGenerator::CodeGenerator(MacroAssembler* masm)
    : deferred_(8),
      masm_(masm),
      info_(NULL),
      frame_(NULL),
      allocator_(NULL),
      cc_reg_(al),
      state_(NULL),
      loop_nesting_(0),
      type_info_(NULL),
      function_return_(JumpTarget::BIDIRECTIONAL),
      function_return_is_shadowed_(false) {
}


// Calling conventions:
// fp: caller's frame pointer
// sp: stack pointer
// r1: called JS function
// cp: callee's context

void CodeGenerator::Generate(CompilationInfo* info) {
  // Record the position for debugging purposes.
  CodeForFunctionPosition(info->function());
  Comment cmnt(masm_, "[ function compiled by virtual frame code generator");

  // Initialize state.
  info_ = info;

  int slots = scope()->num_parameters() + scope()->num_stack_slots();
  ScopedVector<TypeInfo> type_info_array(slots);
  type_info_ = &type_info_array;

  ASSERT(allocator_ == NULL);
  RegisterAllocator register_allocator(this);
  allocator_ = &register_allocator;
  ASSERT(frame_ == NULL);
  frame_ = new VirtualFrame();
  cc_reg_ = al;

  // Adjust for function-level loop nesting.
  ASSERT_EQ(0, loop_nesting_);
  loop_nesting_ = info->loop_nesting();

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

#ifdef DEBUG
    if (strlen(FLAG_stop_at) > 0 &&
        info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
      frame_->SpillAll();
      __ stop("stop-at");
    }
#endif

    if (info->mode() == CompilationInfo::PRIMARY) {
      frame_->Enter();
      // tos: code slot

      // Allocate space for locals and initialize them.  This also checks
      // for stack overflow.
      frame_->AllocateStackSlots();

      frame_->AssertIsSpilled();
      int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
      if (heap_slots > 0) {
        // Allocate local context.
        // Get outer context and create a new context based on it.
        __ ldr(r0, frame_->Function());
        frame_->EmitPush(r0);
        if (heap_slots <= FastNewContextStub::kMaximumSlots) {
          FastNewContextStub stub(heap_slots);
          frame_->CallStub(&stub, 1);
        } else {
          frame_->CallRuntime(Runtime::kNewContext, 1);
        }

#ifdef DEBUG
        JumpTarget verified_true;
        __ cmp(r0, cp);
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
        frame_->AssertIsSpilled();
        for (int i = 0; i < scope()->num_parameters(); i++) {
          Variable* par = scope()->parameter(i);
          Slot* slot = par->slot();
          if (slot != NULL && slot->type() == Slot::CONTEXT) {
            ASSERT(!scope()->is_global_scope());  // No params in global scope.
            __ ldr(r1, frame_->ParameterAt(i));
            // Loads r2 with context; used below in RecordWrite.
            __ str(r1, SlotOperand(slot, r2));
            // Load the offset into r3.
            int slot_offset =
                FixedArray::kHeaderSize + slot->index() * kPointerSize;
            __ RecordWrite(r2, Operand(slot_offset), r3, r1);
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
        frame_->EmitPushRoot(Heap::kTheHoleValueRootIndex);
        StoreToSlot(scope()->function()->slot(), NOT_CONST_INIT);
      }
    } else {
      // When used as the secondary compiler for splitting, r1, cp,
      // fp, and lr have been pushed on the stack.  Adjust the virtual
      // frame to match this state.
      frame_->Adjust(4);

      // Bind all the bailout labels to the beginning of the function.
      List<CompilationInfo::Bailout*>* bailouts = info->bailouts();
      for (int i = 0; i < bailouts->length(); i++) {
        __ bind(bailouts->at(i)->label());
      }
    }

    // Initialize the function return target after the locals are set
    // up, because it needs the expected frame height from the frame.
    function_return_.SetExpectedHeight();
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
    }
  }

  // Handle the return from the function.
  if (has_valid_frame()) {
    // If there is a valid frame, control flow can fall off the end of
    // the body.  In that case there is an implicit return statement.
    ASSERT(!function_return_is_shadowed_);
    frame_->PrepareForReturn();
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    if (function_return_.is_bound()) {
      function_return_.Jump();
    } else {
      function_return_.Bind();
      GenerateReturnSequence();
    }
  } else if (function_return_.is_linked()) {
    // If the return target has dangling jumps to it, then we have not
    // yet generated the return sequence.  This can happen when (a)
    // control does not flow off the end of the body so we did not
    // compile an artificial return statement just above, and (b) there
    // are return statements in the body but (c) they are all shadowed.
    function_return_.Bind();
    GenerateReturnSequence();
  }

  // Adjust for function-level loop nesting.
  ASSERT(loop_nesting_ == info->loop_nesting());
  loop_nesting_ = 0;

  // Code generation state must be reset.
  ASSERT(!has_cc());
  ASSERT(state_ == NULL);
  ASSERT(loop_nesting() == 0);
  ASSERT(!function_return_is_shadowed_);
  function_return_.Unuse();
  DeleteFrame();

  // Process any deferred code using the register allocator.
  if (!HasStackOverflow()) {
    ProcessDeferred();
  }

  allocator_ = NULL;
  type_info_ = NULL;
}


int CodeGenerator::NumberOfSlot(Slot* slot) {
  if (slot == NULL) return kInvalidSlotNumber;
  switch (slot->type()) {
    case Slot::PARAMETER:
      return slot->index();
    case Slot::LOCAL:
      return slot->index() + scope()->num_parameters();
    default:
      break;
  }
  return kInvalidSlotNumber;
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
                                  JumpTarget* true_target,
                                  JumpTarget* false_target,
                                  bool force_cc) {
  ASSERT(!has_cc());
  int original_height = frame_->height();

  { ConditionCodeGenState new_state(this, true_target, false_target);
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


void CodeGenerator::Load(Expression* expr) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  JumpTarget true_target;
  JumpTarget false_target;
  LoadCondition(expr, &true_target, &false_target, false);

  if (has_cc()) {
    // Convert cc_reg_ into a boolean value.
    JumpTarget loaded;
    JumpTarget materialize_true;
    materialize_true.Branch(cc_reg_);
    frame_->EmitPushRoot(Heap::kFalseValueRootIndex);
    loaded.Jump();
    materialize_true.Bind();
    frame_->EmitPushRoot(Heap::kTrueValueRootIndex);
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
      frame_->EmitPushRoot(Heap::kTrueValueRootIndex);
    }
    // If both "true" and "false" need to be loaded jump across the code for
    // "false".
    if (both) {
      loaded.Jump();
    }
    // Load "false" if necessary.
    if (false_target.is_linked()) {
      false_target.Bind();
      frame_->EmitPushRoot(Heap::kFalseValueRootIndex);
    }
    // A value is loaded on all paths reaching this point.
    loaded.Bind();
  }
  ASSERT(has_valid_frame());
  ASSERT(!has_cc());
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::LoadGlobal() {
  Register reg = frame_->GetTOSRegister();
  __ ldr(reg, GlobalObject());
  frame_->EmitPush(reg);
}


void CodeGenerator::LoadGlobalReceiver(Register scratch) {
  Register reg = frame_->GetTOSRegister();
  __ ldr(reg, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(reg,
         FieldMemOperand(reg, GlobalObject::kGlobalReceiverOffset));
  frame_->EmitPush(reg);
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


void CodeGenerator::StoreArgumentsObject(bool initial) {
  ArgumentsAllocationMode mode = ArgumentsMode();
  ASSERT(mode != NO_ARGUMENTS_ALLOCATION);

  Comment cmnt(masm_, "[ store arguments object");
  if (mode == LAZY_ARGUMENTS_ALLOCATION && initial) {
    // When using lazy arguments allocation, we store the hole value
    // as a sentinel indicating that the arguments object hasn't been
    // allocated yet.
    frame_->EmitPushRoot(Heap::kTheHoleValueRootIndex);
  } else {
    frame_->SpillAll();
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    __ ldr(r2, frame_->Function());
    // The receiver is below the arguments, the return address, and the
    // frame pointer on the stack.
    const int kReceiverDisplacement = 2 + scope()->num_parameters();
    __ add(r1, fp, Operand(kReceiverDisplacement * kPointerSize));
    __ mov(r0, Operand(Smi::FromInt(scope()->num_parameters())));
    frame_->Adjust(3);
    __ Push(r2, r1, r0);
    frame_->CallStub(&stub, 3);
    frame_->EmitPush(r0);
  }

  Variable* arguments = scope()->arguments()->var();
  Variable* shadow = scope()->arguments_shadow()->var();
  ASSERT(arguments != NULL && arguments->slot() != NULL);
  ASSERT(shadow != NULL && shadow->slot() != NULL);
  JumpTarget done;
  if (mode == LAZY_ARGUMENTS_ALLOCATION && !initial) {
    // We have to skip storing into the arguments slot if it has
    // already been written to. This can happen if the a function
    // has a local variable named 'arguments'.
    LoadFromSlot(scope()->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);
    Register arguments = frame_->PopToRegister();
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(arguments, ip);
    done.Branch(ne);
  }
  StoreToSlot(arguments->slot(), NOT_CONST_INIT);
  if (mode == LAZY_ARGUMENTS_ALLOCATION) done.Bind();
  StoreToSlot(shadow->slot(), NOT_CONST_INIT);
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


void CodeGenerator::LoadReference(Reference* ref) {
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
}


void CodeGenerator::UnloadReference(Reference* ref) {
  int size = ref->size();
  ref->set_unloaded();
  if (size == 0) return;

  // Pop a reference from the stack while preserving TOS.
  VirtualFrame::RegisterAllocationScope scope(this);
  Comment cmnt(masm_, "[ UnloadReference");
  if (size > 0) {
    Register tos = frame_->PopToRegister();
    frame_->Drop(size);
    frame_->EmitPush(tos);
  }
}


// ECMA-262, section 9.2, page 30: ToBoolean(). Convert the given
// register to a boolean in the condition code register. The code
// may jump to 'false_target' in case the register converts to 'false'.
void CodeGenerator::ToBoolean(JumpTarget* true_target,
                              JumpTarget* false_target) {
  // Note: The generated code snippet does not change stack variables.
  //       Only the condition code should be set.
  bool known_smi = frame_->KnownSmiAt(0);
  Register tos = frame_->PopToRegister();

  // Fast case checks

  // Check if the value is 'false'.
  if (!known_smi) {
    __ LoadRoot(ip, Heap::kFalseValueRootIndex);
    __ cmp(tos, ip);
    false_target->Branch(eq);

    // Check if the value is 'true'.
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(tos, ip);
    true_target->Branch(eq);

    // Check if the value is 'undefined'.
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(tos, ip);
    false_target->Branch(eq);
  }

  // Check if the value is a smi.
  __ cmp(tos, Operand(Smi::FromInt(0)));

  if (!known_smi) {
    false_target->Branch(eq);
    __ tst(tos, Operand(kSmiTagMask));
    true_target->Branch(eq);

    // Slow case: call the runtime.
    frame_->EmitPush(tos);
    frame_->CallRuntime(Runtime::kToBool, 1);
    // Convert the result (r0) to a condition code.
    __ LoadRoot(ip, Heap::kFalseValueRootIndex);
    __ cmp(r0, ip);
  }

  cc_reg_ = ne;
}


void CodeGenerator::GenericBinaryOperation(Token::Value op,
                                           OverwriteMode overwrite_mode,
                                           GenerateInlineSmi inline_smi,
                                           int constant_rhs) {
  // top of virtual frame: y
  // 2nd elt. on virtual frame : x
  // result : top of virtual frame

  // Stub is entered with a call: 'return address' is in lr.
  switch (op) {
    case Token::ADD:
    case Token::SUB:
      if (inline_smi) {
        JumpTarget done;
        Register rhs = frame_->PopToRegister();
        Register lhs = frame_->PopToRegister(rhs);
        Register scratch = VirtualFrame::scratch0();
        __ orr(scratch, rhs, Operand(lhs));
        // Check they are both small and positive.
        __ tst(scratch, Operand(kSmiTagMask | 0xc0000000));
        ASSERT(rhs.is(r0) || lhs.is(r0));  // r0 is free now.
        STATIC_ASSERT(kSmiTag == 0);
        if (op == Token::ADD) {
          __ add(r0, lhs, Operand(rhs), LeaveCC, eq);
        } else {
          __ sub(r0, lhs, Operand(rhs), LeaveCC, eq);
        }
        done.Branch(eq);
        GenericBinaryOpStub stub(op, overwrite_mode, lhs, rhs, constant_rhs);
        frame_->SpillAll();
        frame_->CallStub(&stub, 0);
        done.Bind();
        frame_->EmitPush(r0);
        break;
      } else {
        // Fall through!
      }
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
      if (inline_smi) {
        bool rhs_is_smi = frame_->KnownSmiAt(0);
        bool lhs_is_smi = frame_->KnownSmiAt(1);
        Register rhs = frame_->PopToRegister();
        Register lhs = frame_->PopToRegister(rhs);
        Register smi_test_reg;
        Condition cond;
        if (!rhs_is_smi || !lhs_is_smi) {
          if (rhs_is_smi) {
            smi_test_reg = lhs;
          } else if (lhs_is_smi) {
            smi_test_reg = rhs;
          } else {
            smi_test_reg = VirtualFrame::scratch0();
            __ orr(smi_test_reg, rhs, Operand(lhs));
          }
          // Check they are both Smis.
          __ tst(smi_test_reg, Operand(kSmiTagMask));
          cond = eq;
        } else {
          cond = al;
        }
        ASSERT(rhs.is(r0) || lhs.is(r0));  // r0 is free now.
        if (op == Token::BIT_OR) {
          __ orr(r0, lhs, Operand(rhs), LeaveCC, cond);
        } else if (op == Token::BIT_AND) {
          __ and_(r0, lhs, Operand(rhs), LeaveCC, cond);
        } else {
          ASSERT(op == Token::BIT_XOR);
          STATIC_ASSERT(kSmiTag == 0);
          __ eor(r0, lhs, Operand(rhs), LeaveCC, cond);
        }
        if (cond != al) {
          JumpTarget done;
          done.Branch(cond);
          GenericBinaryOpStub stub(op, overwrite_mode, lhs, rhs, constant_rhs);
          frame_->SpillAll();
          frame_->CallStub(&stub, 0);
          done.Bind();
        }
        frame_->EmitPush(r0);
        break;
      } else {
        // Fall through!
      }
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      Register rhs = frame_->PopToRegister();
      Register lhs = frame_->PopToRegister(rhs);  // Don't pop to rhs register.
      GenericBinaryOpStub stub(op, overwrite_mode, lhs, rhs, constant_rhs);
      frame_->SpillAll();
      frame_->CallStub(&stub, 0);
      frame_->EmitPush(r0);
      break;
    }

    case Token::COMMA: {
      Register scratch = frame_->PopToRegister();
      // Simply discard left value.
      frame_->Drop();
      frame_->EmitPush(scratch);
      break;
    }

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
                             OverwriteMode overwrite_mode,
                             Register tos)
      : op_(op),
        value_(value),
        reversed_(reversed),
        overwrite_mode_(overwrite_mode),
        tos_register_(tos) {
    set_comment("[ DeferredInlinedSmiOperation");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  int value_;
  bool reversed_;
  OverwriteMode overwrite_mode_;
  Register tos_register_;
};



// On entry the non-constant side of the binary operation is in tos_register_
// and the constant smi side is nowhere.  The tos_register_ is not used by the
// virtual frame.  On exit the answer is in the tos_register_ and the virtual
// frame is unchanged.
void DeferredInlineSmiOperation::Generate() {
  VirtualFrame copied_frame(*frame_state()->frame());
  copied_frame.SpillAll();

  Register lhs = r1;
  Register rhs = r0;
  switch (op_) {
    case Token::ADD: {
      // Revert optimistic add.
      if (reversed_) {
        __ sub(r0, tos_register_, Operand(Smi::FromInt(value_)));
        __ mov(r1, Operand(Smi::FromInt(value_)));
      } else {
        __ sub(r1, tos_register_, Operand(Smi::FromInt(value_)));
        __ mov(r0, Operand(Smi::FromInt(value_)));
      }
      break;
    }

    case Token::SUB: {
      // Revert optimistic sub.
      if (reversed_) {
        __ rsb(r0, tos_register_, Operand(Smi::FromInt(value_)));
        __ mov(r1, Operand(Smi::FromInt(value_)));
      } else {
        __ add(r1, tos_register_, Operand(Smi::FromInt(value_)));
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
    case Token::BIT_AND:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      if (tos_register_.is(r1)) {
        __ mov(r0, Operand(Smi::FromInt(value_)));
      } else {
        ASSERT(tos_register_.is(r0));
        __ mov(r1, Operand(Smi::FromInt(value_)));
      }
      if (reversed_ == tos_register_.is(r1)) {
          lhs = r0;
          rhs = r1;
      }
      break;
    }

    default:
      // Other cases should have been handled before this point.
      UNREACHABLE();
      break;
  }

  GenericBinaryOpStub stub(op_, overwrite_mode_, lhs, rhs, value_);
  __ CallStub(&stub);

  // The generic stub returns its value in r0, but that's not
  // necessarily what we want.  We want whatever the inlined code
  // expected, which is that the answer is in the same register as
  // the operand was.
  __ Move(tos_register_, r0);

  // The tos register was not in use for the virtual frame that we
  // came into this function with, so we can merge back to that frame
  // without trashing it.
  copied_frame.MergeTo(frame_state()->frame());
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
  int int_value = Smi::cast(*value)->value();

  bool both_sides_are_smi = frame_->KnownSmiAt(0);

  bool something_to_inline;
  switch (op) {
    case Token::ADD:
    case Token::SUB:
    case Token::BIT_AND:
    case Token::BIT_OR:
    case Token::BIT_XOR: {
      something_to_inline = true;
      break;
    }
    case Token::SHL: {
      something_to_inline = (both_sides_are_smi || !reversed);
      break;
    }
    case Token::SHR:
    case Token::SAR: {
      if (reversed) {
        something_to_inline = false;
      } else {
        something_to_inline = true;
      }
      break;
    }
    case Token::MOD: {
      if (reversed || int_value < 2 || !IsPowerOf2(int_value)) {
        something_to_inline = false;
      } else {
        something_to_inline = true;
      }
      break;
    }
    case Token::MUL: {
      if (!IsEasyToMultiplyBy(int_value)) {
        something_to_inline = false;
      } else {
        something_to_inline = true;
      }
      break;
    }
    default: {
      something_to_inline = false;
      break;
    }
  }

  if (!something_to_inline) {
    if (!reversed) {
      // Push the rhs onto the virtual frame by putting it in a TOS register.
      Register rhs = frame_->GetTOSRegister();
      __ mov(rhs, Operand(value));
      frame_->EmitPush(rhs, TypeInfo::Smi());
      GenericBinaryOperation(op, mode, GENERATE_INLINE_SMI, int_value);
    } else {
      // Pop the rhs, then push lhs and rhs in the right order.  Only performs
      // at most one pop, the rest takes place in TOS registers.
      Register lhs = frame_->GetTOSRegister();    // Get reg for pushing.
      Register rhs = frame_->PopToRegister(lhs);  // Don't use lhs for this.
      __ mov(lhs, Operand(value));
      frame_->EmitPush(lhs, TypeInfo::Smi());
      TypeInfo t = both_sides_are_smi ? TypeInfo::Smi() : TypeInfo::Unknown();
      frame_->EmitPush(rhs, t);
      GenericBinaryOperation(op, mode, GENERATE_INLINE_SMI, kUnknownIntValue);
    }
    return;
  }

  // We move the top of stack to a register (normally no move is invoved).
  Register tos = frame_->PopToRegister();
  switch (op) {
    case Token::ADD: {
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode, tos);

      __ add(tos, tos, Operand(value), SetCC);
      deferred->Branch(vs);
      if (!both_sides_are_smi) {
        __ tst(tos, Operand(kSmiTagMask));
        deferred->Branch(ne);
      }
      deferred->BindExit();
      frame_->EmitPush(tos);
      break;
    }

    case Token::SUB: {
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode, tos);

      if (reversed) {
        __ rsb(tos, tos, Operand(value), SetCC);
      } else {
        __ sub(tos, tos, Operand(value), SetCC);
      }
      deferred->Branch(vs);
      if (!both_sides_are_smi) {
        __ tst(tos, Operand(kSmiTagMask));
        deferred->Branch(ne);
      }
      deferred->BindExit();
      frame_->EmitPush(tos);
      break;
    }


    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND: {
      if (both_sides_are_smi) {
        switch (op) {
          case Token::BIT_OR:  __ orr(tos, tos, Operand(value)); break;
          case Token::BIT_XOR: __ eor(tos, tos, Operand(value)); break;
          case Token::BIT_AND: __ And(tos, tos, Operand(value)); break;
          default: UNREACHABLE();
        }
        frame_->EmitPush(tos, TypeInfo::Smi());
      } else {
        DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode, tos);
        __ tst(tos, Operand(kSmiTagMask));
        deferred->Branch(ne);
        switch (op) {
          case Token::BIT_OR:  __ orr(tos, tos, Operand(value)); break;
          case Token::BIT_XOR: __ eor(tos, tos, Operand(value)); break;
          case Token::BIT_AND: __ And(tos, tos, Operand(value)); break;
          default: UNREACHABLE();
        }
        deferred->BindExit();
        TypeInfo result_type =
            (op == Token::BIT_AND) ? TypeInfo::Smi() : TypeInfo::Integer32();
        frame_->EmitPush(tos, result_type);
      }
      break;
    }

    case Token::SHL:
      if (reversed) {
        ASSERT(both_sides_are_smi);
        int max_shift = 0;
        int max_result = int_value == 0 ? 1 : int_value;
        while (Smi::IsValid(max_result << 1)) {
          max_shift++;
          max_result <<= 1;
        }
        DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, true, mode, tos);
        // Mask off the last 5 bits of the shift operand (rhs).  This is part
        // of the definition of shift in JS and we know we have a Smi so we
        // can safely do this.  The masked version gets passed to the
        // deferred code, but that makes no difference.
        __ and_(tos, tos, Operand(Smi::FromInt(0x1f)));
        __ cmp(tos, Operand(Smi::FromInt(max_shift)));
        deferred->Branch(ge);
        Register scratch = VirtualFrame::scratch0();
        __ mov(scratch, Operand(tos, ASR, kSmiTagSize));  // Untag.
        __ mov(tos, Operand(Smi::FromInt(int_value)));    // Load constant.
        __ mov(tos, Operand(tos, LSL, scratch));          // Shift constant.
        deferred->BindExit();
        TypeInfo result = TypeInfo::Integer32();
        frame_->EmitPush(tos, result);
        break;
      }
      // Fall through!
    case Token::SHR:
    case Token::SAR: {
      ASSERT(!reversed);
      TypeInfo result = TypeInfo::Integer32();
      Register scratch = VirtualFrame::scratch0();
      Register scratch2 = VirtualFrame::scratch1();
      int shift_value = int_value & 0x1f;  // least significant 5 bits
      DeferredCode* deferred =
        new DeferredInlineSmiOperation(op, shift_value, false, mode, tos);
      uint32_t problematic_mask = kSmiTagMask;
      // For unsigned shift by zero all negative smis are problematic.
      bool skip_smi_test = both_sides_are_smi;
      if (shift_value == 0 && op == Token::SHR) {
        problematic_mask |= 0x80000000;
        skip_smi_test = false;
      }
      if (!skip_smi_test) {
        __ tst(tos, Operand(problematic_mask));
        deferred->Branch(ne);  // Go slow for problematic input.
      }
      switch (op) {
        case Token::SHL: {
          if (shift_value != 0) {
            int adjusted_shift = shift_value - kSmiTagSize;
            ASSERT(adjusted_shift >= 0);
            if (adjusted_shift != 0) {
              __ mov(scratch, Operand(tos, LSL, adjusted_shift));
              // Check that the *signed* result fits in a smi.
              __ add(scratch2, scratch, Operand(0x40000000), SetCC);
              deferred->Branch(mi);
              __ mov(tos, Operand(scratch, LSL, kSmiTagSize));
            } else {
              // Check that the *signed* result fits in a smi.
              __ add(scratch2, tos, Operand(0x40000000), SetCC);
              deferred->Branch(mi);
              __ mov(tos, Operand(tos, LSL, kSmiTagSize));
            }
          }
          break;
        }
        case Token::SHR: {
          if (shift_value != 0) {
            __ mov(scratch, Operand(tos, ASR, kSmiTagSize));  // Remove tag.
            // LSR by immediate 0 means shifting 32 bits.
            __ mov(scratch, Operand(scratch, LSR, shift_value));
            if (shift_value == 1) {
              // check that the *unsigned* result fits in a smi
              // neither of the two high-order bits can be set:
              // - 0x80000000: high bit would be lost when smi tagging
              // - 0x40000000: this number would convert to negative when
              // smi tagging these two cases can only happen with shifts
              // by 0 or 1 when handed a valid smi
              __ tst(scratch, Operand(0xc0000000));
              deferred->Branch(ne);
            } else {
              ASSERT(shift_value >= 2);
              result = TypeInfo::Smi();  // SHR by at least 2 gives a Smi.
            }
            __ mov(tos, Operand(scratch, LSL, kSmiTagSize));
          }
          break;
        }
        case Token::SAR: {
          // In the ARM instructions set, ASR by immediate 0 means shifting 32
          // bits.
          if (shift_value != 0) {
            // Do the shift and the tag removal in one operation.  If the shift
            // is 31 bits (the highest possible value) then we emit the
            // instruction as a shift by 0 which means shift arithmetically by
            // 32.
            __ mov(tos, Operand(tos, ASR, (kSmiTagSize + shift_value) & 0x1f));
            // Put tag back.
            __ mov(tos, Operand(tos, LSL, kSmiTagSize));
            // SAR by at least 1 gives a Smi.
            result = TypeInfo::Smi();
          }
          break;
        }
        default: UNREACHABLE();
      }
      deferred->BindExit();
      frame_->EmitPush(tos, result);
      break;
    }

    case Token::MOD: {
      ASSERT(!reversed);
      ASSERT(int_value >= 2);
      ASSERT(IsPowerOf2(int_value));
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode, tos);
      unsigned mask = (0x80000000u | kSmiTagMask);
      __ tst(tos, Operand(mask));
      deferred->Branch(ne);  // Go to deferred code on non-Smis and negative.
      mask = (int_value << kSmiTagSize) - 1;
      __ and_(tos, tos, Operand(mask));
      deferred->BindExit();
      // Mod of positive power of 2 Smi gives a Smi if the lhs is an integer.
      frame_->EmitPush(
          tos,
          both_sides_are_smi ? TypeInfo::Smi() : TypeInfo::Number());
      break;
    }

    case Token::MUL: {
      ASSERT(IsEasyToMultiplyBy(int_value));
      DeferredCode* deferred =
          new DeferredInlineSmiOperation(op, int_value, reversed, mode, tos);
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
      __ tst(tos, Operand(mask));
      deferred->Branch(ne);
      MultiplyByKnownInt(masm_, tos, tos, int_value);
      deferred->BindExit();
      frame_->EmitPush(tos);
      break;
    }

    default:
      UNREACHABLE();
      break;
  }
}


void CodeGenerator::Comparison(Condition cc,
                               Expression* left,
                               Expression* right,
                               bool strict) {
  VirtualFrame::RegisterAllocationScope scope(this);

  if (left != NULL) Load(left);
  if (right != NULL) Load(right);

  // sp[0] : y
  // sp[1] : x
  // result : cc register

  // Strict only makes sense for equality comparisons.
  ASSERT(!strict || cc == eq);

  Register lhs;
  Register rhs;

  bool lhs_is_smi;
  bool rhs_is_smi;

  // We load the top two stack positions into registers chosen by the virtual
  // frame.  This should keep the register shuffling to a minimum.
  // Implement '>' and '<=' by reversal to obtain ECMA-262 conversion order.
  if (cc == gt || cc == le) {
    cc = ReverseCondition(cc);
    lhs_is_smi = frame_->KnownSmiAt(0);
    rhs_is_smi = frame_->KnownSmiAt(1);
    lhs = frame_->PopToRegister();
    rhs = frame_->PopToRegister(lhs);  // Don't pop to the same register again!
  } else {
    rhs_is_smi = frame_->KnownSmiAt(0);
    lhs_is_smi = frame_->KnownSmiAt(1);
    rhs = frame_->PopToRegister();
    lhs = frame_->PopToRegister(rhs);  // Don't pop to the same register again!
  }

  bool both_sides_are_smi = (lhs_is_smi && rhs_is_smi);

  ASSERT(rhs.is(r0) || rhs.is(r1));
  ASSERT(lhs.is(r0) || lhs.is(r1));

  JumpTarget exit;

  if (!both_sides_are_smi) {
    // Now we have the two sides in r0 and r1.  We flush any other registers
    // because the stub doesn't know about register allocation.
    frame_->SpillAll();
    Register scratch = VirtualFrame::scratch0();
    Register smi_test_reg;
    if (lhs_is_smi) {
      smi_test_reg = rhs;
    } else if (rhs_is_smi) {
      smi_test_reg = lhs;
    } else {
      __ orr(scratch, lhs, Operand(rhs));
      smi_test_reg = scratch;
    }
    __ tst(smi_test_reg, Operand(kSmiTagMask));
    JumpTarget smi;
    smi.Branch(eq);

    // Perform non-smi comparison by stub.
    // CompareStub takes arguments in r0 and r1, returns <0, >0 or 0 in r0.
    // We call with 0 args because there are 0 on the stack.
    CompareStub stub(cc, strict, kBothCouldBeNaN, true, lhs, rhs);
    frame_->CallStub(&stub, 0);
    __ cmp(r0, Operand(0));
    exit.Jump();

    smi.Bind();
  }

  // Do smi comparisons by pointer comparison.
  __ cmp(lhs, Operand(rhs));

  exit.Bind();
  cc_reg_ = cc;
}


// Call the function on the stack with the given arguments.
void CodeGenerator::CallWithArguments(ZoneList<Expression*>* args,
                                      CallFunctionFlags flags,
                                      int position) {
  // Push the arguments ("left-to-right") on the stack.
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
  }

  // Record the position for debugging purposes.
  CodeForSourcePosition(position);

  // Use the shared code stub to call the function.
  InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub call_function(arg_count, in_loop, flags);
  frame_->CallStub(&call_function, arg_count + 1);

  // Restore context and pop function from the stack.
  __ ldr(cp, frame_->Context());
  frame_->Drop();  // discard the TOS
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
  Handle<String> name = Factory::LookupAsciiSymbol("apply");
  frame_->Dup();
  frame_->CallLoadIC(name, RelocInfo::CODE_TARGET);
  frame_->EmitPush(r0);

  // Load the receiver and the existing arguments object onto the
  // expression stack. Avoid allocating the arguments object here.
  Load(receiver);
  LoadFromSlot(scope()->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);

  // At this point the top two stack elements are probably in registers
  // since they were just loaded.  Ensure they are in regs and get the
  // regs.
  Register receiver_reg = frame_->Peek2();
  Register arguments_reg = frame_->Peek();

  // From now on the frame is spilled.
  frame_->SpillAll();

  // Emit the source position information after having loaded the
  // receiver and the arguments.
  CodeForSourcePosition(position);
  // Contents of the stack at this point:
  //   sp[0]: arguments object of the current function or the hole.
  //   sp[1]: receiver
  //   sp[2]: applicand.apply
  //   sp[3]: applicand.

  // Check if the arguments object has been lazily allocated
  // already. If so, just use that instead of copying the arguments
  // from the stack. This also deals with cases where a local variable
  // named 'arguments' has been introduced.
  JumpTarget slow;
  Label done;
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(ip, arguments_reg);
  slow.Branch(ne);

  Label build_args;
  // Get rid of the arguments object probe.
  frame_->Drop();
  // Stack now has 3 elements on it.
  // Contents of stack at this point:
  //   sp[0]: receiver - in the receiver_reg register.
  //   sp[1]: applicand.apply
  //   sp[2]: applicand.

  // Check that the receiver really is a JavaScript object.
  __ BranchOnSmi(receiver_reg, &build_args);
  // We allow all JSObjects including JSFunctions.  As long as
  // JS_FUNCTION_TYPE is the last instance type and it is right
  // after LAST_JS_OBJECT_TYPE, we do not have to check the upper
  // bound.
  STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  STATIC_ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ CompareObjectType(receiver_reg, r2, r3, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &build_args);

  // Check that applicand.apply is Function.prototype.apply.
  __ ldr(r0, MemOperand(sp, kPointerSize));
  __ BranchOnSmi(r0, &build_args);
  __ CompareObjectType(r0, r1, r2, JS_FUNCTION_TYPE);
  __ b(ne, &build_args);
  __ ldr(r0, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  Handle<Code> apply_code(Builtins::builtin(Builtins::FunctionApply));
  __ ldr(r1, FieldMemOperand(r0, SharedFunctionInfo::kCodeOffset));
  __ cmp(r1, Operand(apply_code));
  __ b(ne, &build_args);

  // Check that applicand is a function.
  __ ldr(r1, MemOperand(sp, 2 * kPointerSize));
  __ BranchOnSmi(r1, &build_args);
  __ CompareObjectType(r1, r2, r3, JS_FUNCTION_TYPE);
  __ b(ne, &build_args);

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  Label invoke, adapted;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adapted);

  // No arguments adaptor frame. Copy fixed number of arguments.
  __ mov(r0, Operand(scope()->num_parameters()));
  for (int i = 0; i < scope()->num_parameters(); i++) {
    __ ldr(r2, frame_->ParameterAt(i));
    __ push(r2);
  }
  __ jmp(&invoke);

  // Arguments adaptor frame present. Copy arguments from there, but
  // avoid copying too many arguments to avoid stack overflows.
  __ bind(&adapted);
  static const uint32_t kArgumentsLimit = 1 * KB;
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(r0, Operand(r0, LSR, kSmiTagSize));
  __ mov(r3, r0);
  __ cmp(r0, Operand(kArgumentsLimit));
  __ b(gt, &build_args);

  // Loop through the arguments pushing them onto the execution
  // stack. We don't inform the virtual frame of the push, so we don't
  // have to worry about getting rid of the elements from the virtual
  // frame.
  Label loop;
  // r3 is a small non-negative integer, due to the test above.
  __ cmp(r3, Operand(0));
  __ b(eq, &invoke);
  // Compute the address of the first argument.
  __ add(r2, r2, Operand(r3, LSL, kPointerSizeLog2));
  __ add(r2, r2, Operand(kPointerSize));
  __ bind(&loop);
  // Post-decrement argument address by kPointerSize on each iteration.
  __ ldr(r4, MemOperand(r2, kPointerSize, NegPostIndex));
  __ push(r4);
  __ sub(r3, r3, Operand(1), SetCC);
  __ b(gt, &loop);

  // Invoke the function.
  __ bind(&invoke);
  ParameterCount actual(r0);
  __ InvokeFunction(r1, actual, CALL_FUNCTION);
  // Drop applicand.apply and applicand from the stack, and push
  // the result of the function call, but leave the spilled frame
  // unchanged, with 3 elements, so it is correct when we compile the
  // slow-case code.
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ push(r0);
  // Stack now has 1 element:
  //   sp[0]: result
  __ jmp(&done);

  // Slow-case: Allocate the arguments object since we know it isn't
  // there, and fall-through to the slow-case where we call
  // applicand.apply.
  __ bind(&build_args);
  // Stack now has 3 elements, because we have jumped from where:
  //   sp[0]: receiver
  //   sp[1]: applicand.apply
  //   sp[2]: applicand.
  StoreArgumentsObject(false);

  // Stack and frame now have 4 elements.
  slow.Bind();

  // Generic computation of x.apply(y, args) with no special optimization.
  // Flip applicand.apply and applicand on the stack, so
  // applicand looks like the receiver of the applicand.apply call.
  // Then process it as a normal function call.
  __ ldr(r0, MemOperand(sp, 3 * kPointerSize));
  __ ldr(r1, MemOperand(sp, 2 * kPointerSize));
  __ Strd(r0, r1, MemOperand(sp, 2 * kPointerSize));

  CallFunctionStub call_function(2, NOT_IN_LOOP, NO_CALL_FUNCTION_FLAGS);
  frame_->CallStub(&call_function, 3);
  // The function and its two arguments have been dropped.
  frame_->Drop();  // Drop the receiver as well.
  frame_->EmitPush(r0);
  frame_->SpillAll();  // A spilled frame is also jumping to label done.
  // Stack now has 1 element:
  //   sp[0]: result
  __ bind(&done);

  // Restore the context register after a call.
  __ ldr(cp, frame_->Context());
}


void CodeGenerator::Branch(bool if_true, JumpTarget* target) {
  ASSERT(has_cc());
  Condition cc = if_true ? cc_reg_ : NegateCondition(cc_reg_);
  target->Branch(cc);
  cc_reg_ = al;
}


void CodeGenerator::CheckStack() {
  frame_->SpillAll();
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
  for (int i = 0; frame_ != NULL && i < statements->length(); i++) {
    Visit(statements->at(i));
  }
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitBlock(Block* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Block");
  CodeForStatementPosition(node);
  node->break_target()->SetExpectedHeight();
  VisitStatements(node->statements());
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  frame_->EmitPush(cp);
  frame_->EmitPush(Operand(pairs));
  frame_->EmitPush(Operand(Smi::FromInt(is_eval() ? 1 : 0)));

  frame_->CallRuntime(Runtime::kDeclareGlobals, 3);
  // The result is discarded.
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
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
    frame_->EmitPush(Operand(var->name()));
    // Declaration nodes are always declared in only two modes.
    ASSERT(node->mode() == Variable::VAR || node->mode() == Variable::CONST);
    PropertyAttributes attr = node->mode() == Variable::VAR ? NONE : READ_ONLY;
    frame_->EmitPush(Operand(Smi::FromInt(attr)));
    // Push initial value, if any.
    // Note: For variables we must not push an initial value (such as
    // 'undefined') because we may have a (legal) redeclaration and we
    // must not destroy the current value.
    if (node->mode() == Variable::CONST) {
      frame_->EmitPushRoot(Heap::kTheHoleValueRootIndex);
    } else if (node->fun() != NULL) {
      Load(node->fun());
    } else {
      frame_->EmitPush(Operand(0));
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
    WriteBarrierCharacter wb_info =
        val->type()->IsLikelySmi() ? LIKELY_SMI : UNLIKELY_SMI;
    if (val->AsLiteral() != NULL) wb_info = NEVER_NEWSPACE;
    // Set initial value.
    Reference target(this, node->proxy());
    Load(val);
    target.SetValue(NOT_CONST_INIT, wb_info);

    // Get rid of the assigned value (declarations are statements).
    frame_->Drop();
  }
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitExpressionStatement(ExpressionStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ ExpressionStatement");
  CodeForStatementPosition(node);
  Expression* expression = node->expression();
  expression->MarkAsStatement();
  Load(expression);
  frame_->Drop();
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitEmptyStatement(EmptyStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "// EmptyStatement");
  CodeForStatementPosition(node);
  // nothing to do
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::VisitIfStatement(IfStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
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
    LoadCondition(node->condition(), &then, &else_, true);
    if (frame_ != NULL) {
      Branch(false, &else_);
    }
    // then
    if (frame_ != NULL || then.is_linked()) {
      then.Bind();
      Visit(node->then_statement());
    }
    if (frame_ != NULL) {
      exit.Jump();
    }
    // else
    if (else_.is_linked()) {
      else_.Bind();
      Visit(node->else_statement());
    }

  } else if (has_then_stm) {
    Comment cmnt(masm_, "[ IfThen");
    ASSERT(!has_else_stm);
    JumpTarget then;
    // if (cond)
    LoadCondition(node->condition(), &then, &exit, true);
    if (frame_ != NULL) {
      Branch(false, &exit);
    }
    // then
    if (frame_ != NULL || then.is_linked()) {
      then.Bind();
      Visit(node->then_statement());
    }

  } else if (has_else_stm) {
    Comment cmnt(masm_, "[ IfElse");
    ASSERT(!has_then_stm);
    JumpTarget else_;
    // if (!cond)
    LoadCondition(node->condition(), &exit, &else_, true);
    if (frame_ != NULL) {
      Branch(true, &exit);
    }
    // else
    if (frame_ != NULL || else_.is_linked()) {
      else_.Bind();
      Visit(node->else_statement());
    }

  } else {
    Comment cmnt(masm_, "[ If");
    ASSERT(!has_then_stm && !has_else_stm);
    // if (cond)
    LoadCondition(node->condition(), &exit, &exit, false);
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
  Comment cmnt(masm_, "[ ContinueStatement");
  CodeForStatementPosition(node);
  node->target()->continue_target()->Jump();
}


void CodeGenerator::VisitBreakStatement(BreakStatement* node) {
  Comment cmnt(masm_, "[ BreakStatement");
  CodeForStatementPosition(node);
  node->target()->break_target()->Jump();
}


void CodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  frame_->SpillAll();
  Comment cmnt(masm_, "[ ReturnStatement");

  CodeForStatementPosition(node);
  Load(node->expression());
  if (function_return_is_shadowed_) {
    frame_->EmitPop(r0);
    function_return_.Jump();
  } else {
    // Pop the result from the frame and prepare the frame for
    // returning thus making it easier to merge.
    frame_->PopToR0();
    frame_->PrepareForReturn();
    if (function_return_.is_bound()) {
      // If the function return label is already bound we reuse the
      // code by jumping to the return site.
      function_return_.Jump();
    } else {
      function_return_.Bind();
      GenerateReturnSequence();
    }
  }
}


void CodeGenerator::GenerateReturnSequence() {
  if (FLAG_trace) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns the parameter as it is.
    frame_->EmitPush(r0);
    frame_->CallRuntime(Runtime::kTraceExit, 1);
  }

#ifdef DEBUG
  // Add a label for checking the size of the code used for returning.
  Label check_exit_codesize;
  masm_->bind(&check_exit_codesize);
#endif
  // Make sure that the constant pool is not emitted inside of the return
  // sequence.
  { Assembler::BlockConstPoolScope block_const_pool(masm_);
    // Tear down the frame which will restore the caller's frame pointer and
    // the link register.
    frame_->Exit();

    // Here we use masm_-> instead of the __ macro to avoid the code coverage
    // tool from instrumenting as we rely on the code size here.
    int32_t sp_delta = (scope()->num_parameters() + 1) * kPointerSize;
    masm_->add(sp, sp, Operand(sp_delta));
    masm_->Jump(lr);
    DeleteFrame();

#ifdef DEBUG
    // Check that the size of the code used for returning matches what is
    // expected by the debugger. If the sp_delts above cannot be encoded in
    // the add instruction the add will generate two instructions.
    int return_sequence_length =
        masm_->InstructionsGeneratedSince(&check_exit_codesize);
    CHECK(return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions ||
          return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions + 1);
#endif
  }
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ WithEnterStatement");
  CodeForStatementPosition(node);
  Load(node->expression());
  if (node->is_catch_block()) {
    frame_->CallRuntime(Runtime::kPushCatchContext, 1);
  } else {
    frame_->CallRuntime(Runtime::kPushContext, 1);
  }
#ifdef DEBUG
  JumpTarget verified_true;
  __ cmp(r0, cp);
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
  Comment cmnt(masm_, "[ SwitchStatement");
  CodeForStatementPosition(node);
  node->break_target()->SetExpectedHeight();

  Load(node->tag());

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
    frame_->Dup();
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
    VisitStatements(clause->statements());

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
    VisitStatements(default_clause->statements());
    // If control flow can fall out of the default and there is a case after
    // it, jump to that case's body.
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
  Comment cmnt(masm_, "[ DoWhileStatement");
  CodeForStatementPosition(node);
  node->break_target()->SetExpectedHeight();
  JumpTarget body(JumpTarget::BIDIRECTIONAL);
  IncrementLoopNesting();

  // Label the top of the loop for the backward CFG edge.  If the test
  // is always true we can use the continue target, and if the test is
  // always false there is no need.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  switch (info) {
    case ALWAYS_TRUE:
      node->continue_target()->SetExpectedHeight();
      node->continue_target()->Bind();
      break;
    case ALWAYS_FALSE:
      node->continue_target()->SetExpectedHeight();
      break;
    case DONT_KNOW:
      node->continue_target()->SetExpectedHeight();
      body.Bind();
      break;
  }

  CheckStack();  // TODO(1222600): ignore if body contains calls.
  Visit(node->body());

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
        Comment cmnt(masm_, "[ DoWhileCondition");
        CodeForDoWhileConditionPosition(node);
        LoadCondition(node->cond(), &body, node->break_target(), true);
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
  DecrementLoopNesting();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitWhileStatement(WhileStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ WhileStatement");
  CodeForStatementPosition(node);

  // If the test is never true and has no side effects there is no need
  // to compile the test or body.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  node->break_target()->SetExpectedHeight();
  IncrementLoopNesting();

  // Label the top of the loop with the continue target for the backward
  // CFG edge.
  node->continue_target()->SetExpectedHeight();
  node->continue_target()->Bind();

  if (info == DONT_KNOW) {
    JumpTarget body(JumpTarget::BIDIRECTIONAL);
    LoadCondition(node->cond(), &body, node->break_target(), true);
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
    Visit(node->body());

    // If control flow can fall out of the body, jump back to the top.
    if (has_valid_frame()) {
      node->continue_target()->Jump();
    }
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  DecrementLoopNesting();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitForStatement(ForStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ ForStatement");
  CodeForStatementPosition(node);
  if (node->init() != NULL) {
    Visit(node->init());
  }

  // If the test is never true there is no need to compile the test or
  // body.
  ConditionAnalysis info = AnalyzeCondition(node->cond());
  if (info == ALWAYS_FALSE) return;

  node->break_target()->SetExpectedHeight();
  IncrementLoopNesting();

  // We know that the loop index is a smi if it is not modified in the
  // loop body and it is checked against a constant limit in the loop
  // condition.  In this case, we reset the static type information of the
  // loop index to smi before compiling the body, the update expression, and
  // the bottom check of the loop condition.
  TypeInfoCodeGenState type_info_scope(this,
                                       node->is_fast_smi_loop() ?
                                           node->loop_variable()->slot() :
                                           NULL,
                                       TypeInfo::Smi());

  // If there is no update statement, label the top of the loop with the
  // continue target, otherwise with the loop target.
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  if (node->next() == NULL) {
    node->continue_target()->SetExpectedHeight();
    node->continue_target()->Bind();
  } else {
    node->continue_target()->SetExpectedHeight();
    loop.Bind();
  }

  // If the test is always true, there is no need to compile it.
  if (info == DONT_KNOW) {
    JumpTarget body;
    LoadCondition(node->cond(), &body, node->break_target(), true);
    if (has_valid_frame()) {
      Branch(false, node->break_target());
    }
    if (has_valid_frame() || body.is_linked()) {
      body.Bind();
    }
  }

  if (has_valid_frame()) {
    CheckStack();  // TODO(1222600): ignore if body contains calls.
    Visit(node->body());

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
        Visit(node->next());
        loop.Jump();
      }
    }
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  DecrementLoopNesting();
  ASSERT(!has_valid_frame() || frame_->height() == original_height);
}


void CodeGenerator::VisitForInStatement(ForInStatement* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  VirtualFrame::SpilledScope spilled_scope(frame_);
  Comment cmnt(masm_, "[ ForInStatement");
  CodeForStatementPosition(node);

  JumpTarget primitive;
  JumpTarget jsobject;
  JumpTarget fixed_array;
  JumpTarget entry(JumpTarget::BIDIRECTIONAL);
  JumpTarget end_del_check;
  JumpTarget exit;

  // Get the object to enumerate over (converted to JSObject).
  Load(node->enumerable());

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
  frame_->InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS, 1);

  jsobject.Bind();
  // Get the set of properties (as a FixedArray or Map).
  // r0: value to be iterated over
  frame_->EmitPush(r0);  // Push the object being iterated over.

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  JumpTarget call_runtime;
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  JumpTarget check_prototype;
  JumpTarget use_cache;
  __ mov(r1, Operand(r0));
  loop.Bind();
  // Check that there are no elements.
  __ ldr(r2, FieldMemOperand(r1, JSObject::kElementsOffset));
  __ LoadRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ cmp(r2, r4);
  call_runtime.Branch(ne);
  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in r3 for the subsequent
  // prototype load.
  __ ldr(r3, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldr(r2, FieldMemOperand(r3, Map::kInstanceDescriptorsOffset));
  __ LoadRoot(ip, Heap::kEmptyDescriptorArrayRootIndex);
  __ cmp(r2, ip);
  call_runtime.Branch(eq);
  // Check that there in an enum cache in the non-empty instance
  // descriptors.  This is the case if the next enumeration index
  // field does not contain a smi.
  __ ldr(r2, FieldMemOperand(r2, DescriptorArray::kEnumerationIndexOffset));
  __ tst(r2, Operand(kSmiTagMask));
  call_runtime.Branch(eq);
  // For all objects but the receiver, check that the cache is empty.
  // r4: empty fixed array root.
  __ cmp(r1, r0);
  check_prototype.Branch(eq);
  __ ldr(r2, FieldMemOperand(r2, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ cmp(r2, r4);
  call_runtime.Branch(ne);
  check_prototype.Bind();
  // Load the prototype from the map and loop if non-null.
  __ ldr(r1, FieldMemOperand(r3, Map::kPrototypeOffset));
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r1, ip);
  loop.Branch(ne);
  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
  use_cache.Jump();

  call_runtime.Bind();
  // Call the runtime to get the property names for the object.
  frame_->EmitPush(r0);  // push the object (slot 4) for the runtime call
  frame_->CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  // r0: map or fixed array (result from call to
  // Runtime::kGetPropertyNamesFast)
  __ mov(r2, Operand(r0));
  __ ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r1, ip);
  fixed_array.Branch(ne);

  use_cache.Bind();
  // Get enum cache
  // r0: map (either the result from a call to
  // Runtime::kGetPropertyNamesFast or has been fetched directly from
  // the object)
  __ mov(r1, Operand(r0));
  __ ldr(r1, FieldMemOperand(r1, Map::kInstanceDescriptorsOffset));
  __ ldr(r1, FieldMemOperand(r1, DescriptorArray::kEnumerationIndexOffset));
  __ ldr(r2,
         FieldMemOperand(r1, DescriptorArray::kEnumCacheBridgeCacheOffset));

  frame_->EmitPush(r0);  // map
  frame_->EmitPush(r2);  // enum cache bridge cache
  __ ldr(r0, FieldMemOperand(r2, FixedArray::kLengthOffset));
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
  node->break_target()->SetExpectedHeight();
  node->continue_target()->SetExpectedHeight();

  // Load the current count to r0, load the length to r1.
  __ Ldrd(r0, r1, frame_->ElementAt(0));
  __ cmp(r0, r1);  // compare to the array length
  node->break_target()->Branch(hs);

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
  frame_->InvokeBuiltin(Builtins::FILTER_KEY, CALL_JS, 2);
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
        each.SetValue(NOT_CONST_INIT, UNLIKELY_SMI);
        frame_->Drop(2);
      } else {
        // If the reference was to a slot we rely on the convenient property
        // that it doesn't matter whether a value (eg, r3 pushed above) is
        // right on top of or right underneath a zero-sized reference.
        each.SetValue(NOT_CONST_INIT, UNLIKELY_SMI);
        frame_->Drop();
      }
    }
  }
  // Body.
  CheckStack();  // TODO(1222600): ignore if body contains calls.
  Visit(node->body());

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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  Comment cmnt(masm_, "[ TryCatchStatement");
  CodeForStatementPosition(node);

  JumpTarget try_block;
  JumpTarget exit;

  try_block.Call();
  // --- Catch block ---
  frame_->EmitPush(r0);

  // Store the caught exception in the catch variable.
  Variable* catch_var = node->catch_var()->var();
  ASSERT(catch_var != NULL && catch_var->slot() != NULL);
  StoreToSlot(catch_var->slot(), NOT_CONST_INIT);

  // Remove the exception from the stack.
  frame_->Drop();

  VisitStatements(node->catch_block()->statements());
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
  VisitStatements(node->try_block()->statements());

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
    STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
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

      STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
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
  VisitStatements(node->try_block()->statements());

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
    STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
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
      STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
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
  VisitStatements(node->finally_block()->statements());

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
  Comment cmnt(masm_, "[ DebuggerStatament");
  CodeForStatementPosition(node);
#ifdef ENABLE_DEBUGGER_SUPPORT
  frame_->DebugBreak();
#endif
  // Ignore the return value.
  ASSERT(frame_->height() == original_height);
}


void CodeGenerator::InstantiateFunction(
    Handle<SharedFunctionInfo> function_info) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && function_info->num_literals() == 0) {
    FastNewClosureStub stub;
    frame_->EmitPush(Operand(function_info));
    frame_->SpillAll();
    frame_->CallStub(&stub, 1);
    frame_->EmitPush(r0);
  } else {
    // Create a new closure.
    frame_->EmitPush(cp);
    frame_->EmitPush(Operand(function_info));
    frame_->CallRuntime(Runtime::kNewClosure, 2);
    frame_->EmitPush(r0);
  }
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function info and instantiate it.
  Handle<SharedFunctionInfo> function_info =
      Compiler::BuildFunctionInfo(node, script(), this);
  // Check for stack-overflow exception.
  if (HasStackOverflow()) {
    ASSERT(frame_->height() == original_height);
    return;
  }
  InstantiateFunction(function_info);
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ SharedFunctionInfoLiteral");
  InstantiateFunction(node->shared_function_info());
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitConditional(Conditional* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Conditional");
  JumpTarget then;
  JumpTarget else_;
  LoadCondition(node->condition(), &then, &else_, true);
  if (has_valid_frame()) {
    Branch(false, &else_);
  }
  if (has_valid_frame() || then.is_linked()) {
    then.Bind();
    Load(node->then_expression());
  }
  if (else_.is_linked()) {
    JumpTarget exit;
    if (has_valid_frame()) exit.Jump();
    else_.Bind();
    Load(node->else_expression());
    if (exit.is_linked()) exit.Bind();
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::LoadFromSlot(Slot* slot, TypeofState typeof_state) {
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    // JumpTargets do not yet support merging frames so the frame must be
    // spilled when jumping to these targets.
    JumpTarget slow;
    JumpTarget done;

    // Generate fast case for loading from slots that correspond to
    // local/global variables or arguments unless they are shadowed by
    // eval-introduced bindings.
    EmitDynamicLoadFromSlotFastCase(slot,
                                    typeof_state,
                                    &slow,
                                    &done);

    slow.Bind();
    frame_->EmitPush(cp);
    frame_->EmitPush(Operand(slot->var()->name()));

    if (typeof_state == INSIDE_TYPEOF) {
      frame_->CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    } else {
      frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    }

    done.Bind();
    frame_->EmitPush(r0);

  } else {
    Register scratch = VirtualFrame::scratch0();
    TypeInfo info = type_info(slot);
    frame_->EmitPush(SlotOperand(slot, scratch), info);

    if (slot->var()->mode() == Variable::CONST) {
      // Const slots may contain 'the hole' value (the constant hasn't been
      // initialized yet) which needs to be converted into the 'undefined'
      // value.
      Comment cmnt(masm_, "[ Unhole const");
      Register tos = frame_->PopToRegister();
      __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
      __ cmp(tos, ip);
      __ LoadRoot(tos, Heap::kUndefinedValueRootIndex, eq);
      frame_->EmitPush(tos);
    }
  }
}


void CodeGenerator::LoadFromSlotCheckForArguments(Slot* slot,
                                                  TypeofState state) {
  VirtualFrame::RegisterAllocationScope scope(this);
  LoadFromSlot(slot, state);

  // Bail out quickly if we're not using lazy arguments allocation.
  if (ArgumentsMode() != LAZY_ARGUMENTS_ALLOCATION) return;

  // ... or if the slot isn't a non-parameter arguments slot.
  if (slot->type() == Slot::PARAMETER || !slot->is_arguments()) return;

  // Load the loaded value from the stack into a register but leave it on the
  // stack.
  Register tos = frame_->Peek();

  // If the loaded value is the sentinel that indicates that we
  // haven't loaded the arguments object yet, we need to do it now.
  JumpTarget exit;
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(tos, ip);
  exit.Branch(ne);
  frame_->Drop();
  StoreArgumentsObject(false);
  exit.Bind();
}


void CodeGenerator::StoreToSlot(Slot* slot, InitState init_state) {
  ASSERT(slot != NULL);
  VirtualFrame::RegisterAllocationScope scope(this);
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    // For now, just do a runtime call.
    frame_->EmitPush(cp);
    frame_->EmitPush(Operand(slot->var()->name()));

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
      frame_->CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    } else {
      frame_->CallRuntime(Runtime::kStoreContextSlot, 3);
    }
    // Storing a variable must keep the (new) value on the expression
    // stack. This is necessary for compiling assignment expressions.
    frame_->EmitPush(r0);

  } else {
    ASSERT(!slot->var()->is_dynamic());
    Register scratch = VirtualFrame::scratch0();
    Register scratch2 = VirtualFrame::scratch1();

    // The frame must be spilled when branching to this target.
    JumpTarget exit;

    if (init_state == CONST_INIT) {
      ASSERT(slot->var()->mode() == Variable::CONST);
      // Only the first const initialization must be executed (the slot
      // still contains 'the hole' value). When the assignment is
      // executed, the code is identical to a normal store (see below).
      Comment cmnt(masm_, "[ Init const");
      __ ldr(scratch, SlotOperand(slot, scratch));
      __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
      __ cmp(scratch, ip);
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
    Register tos = frame_->Peek();
    __ str(tos, SlotOperand(slot, scratch));
    if (slot->type() == Slot::CONTEXT) {
      // Skip write barrier if the written value is a smi.
      __ tst(tos, Operand(kSmiTagMask));
      // We don't use tos any more after here.
      exit.Branch(eq);
      // scratch is loaded with context when calling SlotOperand above.
      int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
      // We need an extra register.  Until we have a way to do that in the
      // virtual frame we will cheat and ask for a free TOS register.
      Register scratch3 = frame_->GetTOSRegister();
      __ RecordWrite(scratch, Operand(offset), scratch2, scratch3);
    }
    // If we definitely did not jump over the assignment, we do not need
    // to bind the exit label.  Doing so can defeat peephole
    // optimization.
    if (init_state == CONST_INIT || slot->type() == Slot::CONTEXT) {
      exit.Bind();
    }
  }
}


void CodeGenerator::LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                                      TypeofState typeof_state,
                                                      JumpTarget* slow) {
  // Check that no extension objects have been created by calls to
  // eval from the current scope to the global scope.
  Register tmp = frame_->scratch0();
  Register tmp2 = frame_->scratch1();
  Register context = cp;
  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        frame_->SpillAll();
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
    frame_->SpillAll();
    Label next, fast;
    __ Move(tmp, context);
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

  // Load the global object.
  LoadGlobal();
  // Setup the name register and call load IC.
  frame_->CallLoadIC(slot->var()->name(),
                     typeof_state == INSIDE_TYPEOF
                         ? RelocInfo::CODE_TARGET
                         : RelocInfo::CODE_TARGET_CONTEXT);
}


void CodeGenerator::EmitDynamicLoadFromSlotFastCase(Slot* slot,
                                                    TypeofState typeof_state,
                                                    JumpTarget* slow,
                                                    JumpTarget* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  if (slot->var()->mode() == Variable::DYNAMIC_GLOBAL) {
    LoadFromGlobalSlotCheckExtensions(slot, typeof_state, slow);
    frame_->SpillAll();
    done->Jump();

  } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
    frame_->SpillAll();
    Slot* potential_slot = slot->var()->local_if_not_shadowed()->slot();
    Expression* rewrite = slot->var()->local_if_not_shadowed()->rewrite();
    if (potential_slot != NULL) {
      // Generate fast case for locals that rewrite to slots.
      __ ldr(r0,
             ContextSlotOperandCheckExtensions(potential_slot,
                                               r1,
                                               r2,
                                               slow));
      if (potential_slot->var()->mode() == Variable::CONST) {
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ cmp(r0, ip);
        __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
      }
      done->Jump();
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
          __ ldr(r0,
                 ContextSlotOperandCheckExtensions(obj_proxy->var()->slot(),
                                                   r1,
                                                   r2,
                                                   slow));
          frame_->EmitPush(r0);
          __ mov(r1, Operand(key_literal->handle()));
          frame_->EmitPush(r1);
          EmitKeyedLoad();
          done->Jump();
        }
      }
    }
  }
}


void CodeGenerator::VisitSlot(Slot* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Slot");
  LoadFromSlotCheckForArguments(node, NOT_INSIDE_TYPEOF);
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitVariableProxy(VariableProxy* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
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
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitLiteral(Literal* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Literal");
  Register reg = frame_->GetTOSRegister();
  bool is_smi = node->handle()->IsSmi();
  __ mov(reg, Operand(node->handle()));
  frame_->EmitPush(reg, is_smi ? TypeInfo::Smi() : TypeInfo::Unknown());
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ RexExp Literal");

  Register tmp = VirtualFrame::scratch0();
  // Free up a TOS register that can be used to push the literal.
  Register literal = frame_->GetTOSRegister();

  // Retrieve the literal array and check the allocated entry.

  // Load the function of this activation.
  __ ldr(tmp, frame_->Function());

  // Load the literals array of the function.
  __ ldr(tmp, FieldMemOperand(tmp, JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ ldr(literal, FieldMemOperand(tmp, literal_offset));

  JumpTarget done;
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(literal, ip);
  // This branch locks the virtual frame at the done label to match the
  // one we have here, where the literal register is not on the stack and
  // nothing is spilled.
  done.Branch(ne);

  // If the entry is undefined we call the runtime system to compute
  // the literal.
  // literal array  (0)
  frame_->EmitPush(tmp);
  // literal index  (1)
  frame_->EmitPush(Operand(Smi::FromInt(node->literal_index())));
  // RegExp pattern (2)
  frame_->EmitPush(Operand(node->pattern()));
  // RegExp flags   (3)
  frame_->EmitPush(Operand(node->flags()));
  frame_->CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ Move(literal, r0);

  // This call to bind will get us back to the virtual frame we had before
  // where things are not spilled and the literal register is not on the stack.
  done.Bind();
  // Push the literal.
  frame_->EmitPush(literal);
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ ObjectLiteral");

  Register literal = frame_->GetTOSRegister();
  // Load the function of this activation.
  __ ldr(literal, frame_->Function());
  // Literal array.
  __ ldr(literal, FieldMemOperand(literal, JSFunction::kLiteralsOffset));
  frame_->EmitPush(literal);
  // Literal index.
  frame_->EmitPush(Operand(Smi::FromInt(node->literal_index())));
  // Constant properties.
  frame_->EmitPush(Operand(node->constant_properties()));
  // Should the object literal have fast elements?
  frame_->EmitPush(Operand(Smi::FromInt(node->fast_elements() ? 1 : 0)));
  if (node->depth() > 1) {
    frame_->CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    frame_->CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }
  frame_->EmitPush(r0);  // save the result
  for (int i = 0; i < node->properties()->length(); i++) {
    // At the start of each iteration, the top of stack contains
    // the newly created object literal.
    ObjectLiteral::Property* property = node->properties()->at(i);
    Literal* key = property->key();
    Expression* value = property->value();
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        break;
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        if (CompileTimeValue::IsCompileTimeValue(property->value())) break;
        // else fall through
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          Load(value);
          frame_->PopToR0();
          // Fetch the object literal.
          frame_->SpillAllButCopyTOSToR1();
          __ mov(r2, Operand(key->handle()));
          frame_->CallCodeObject(ic, RelocInfo::CODE_TARGET, 0);
          break;
        }
        // else fall through
      case ObjectLiteral::Property::PROTOTYPE: {
        frame_->Dup();
        Load(key);
        Load(value);
        frame_->CallRuntime(Runtime::kSetProperty, 3);
        break;
      }
      case ObjectLiteral::Property::SETTER: {
        frame_->Dup();
        Load(key);
        frame_->EmitPush(Operand(Smi::FromInt(1)));
        Load(value);
        frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        break;
      }
      case ObjectLiteral::Property::GETTER: {
        frame_->Dup();
        Load(key);
        frame_->EmitPush(Operand(Smi::FromInt(0)));
        Load(value);
        frame_->CallRuntime(Runtime::kDefineAccessor, 4);
        break;
      }
    }
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ ArrayLiteral");

  Register tos = frame_->GetTOSRegister();
  // Load the function of this activation.
  __ ldr(tos, frame_->Function());
  // Load the literals array of the function.
  __ ldr(tos, FieldMemOperand(tos, JSFunction::kLiteralsOffset));
  frame_->EmitPush(tos);
  frame_->EmitPush(Operand(Smi::FromInt(node->literal_index())));
  frame_->EmitPush(Operand(node->constant_elements()));
  int length = node->values()->length();
  if (node->depth() > 1) {
    frame_->CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumLength) {
    frame_->CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(length);
    frame_->CallStub(&stub, 3);
  }
  frame_->EmitPush(r0);  // save the result
  // r0: created object literal

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
    Load(value);
    frame_->PopToR0();
    // Fetch the object literal.
    frame_->SpillAllButCopyTOSToR1();

    // Get the elements array.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));

    // Write to the indexed properties array.
    int offset = i * kPointerSize + FixedArray::kHeaderSize;
    __ str(r0, FieldMemOperand(r1, offset));

    // Update the write barrier for the array address.
    __ RecordWrite(r1, Operand(offset), r3, r2);
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  // Call runtime routine to allocate the catch extension object and
  // assign the exception value to the catch variable.
  Comment cmnt(masm_, "[ CatchExtensionObject");
  Load(node->key());
  Load(node->value());
  frame_->CallRuntime(Runtime::kCreateCatchExtensionObject, 2);
  frame_->EmitPush(r0);
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::EmitSlotAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame_->height();
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

    // Perform the binary operation.
    Literal* literal = node->value()->AsLiteral();
    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    if (literal != NULL && literal->handle()->IsSmi()) {
      SmiOperation(node->binary_op(),
                   literal->handle(),
                   false,
                   overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
    } else {
      GenerateInlineSmi inline_smi =
          loop_nesting() > 0 ? GENERATE_INLINE_SMI : DONT_GENERATE_INLINE_SMI;
      if (literal != NULL) {
        ASSERT(!literal->handle()->IsSmi());
        inline_smi = DONT_GENERATE_INLINE_SMI;
      }
      Load(node->value());
      GenericBinaryOperation(node->binary_op(),
                             overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE,
                             inline_smi);
    }
  } else {
    Load(node->value());
  }

  // Perform the assignment.
  if (var->mode() != Variable::CONST || node->op() == Token::INIT_CONST) {
    CodeForSourcePosition(node->position());
    StoreToSlot(slot,
                node->op() == Token::INIT_CONST ? CONST_INIT : NOT_CONST_INIT);
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::EmitNamedPropertyAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame_->height();
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
      Load(prop->obj());
    } else {
      frame_->Dup();
    }
    frame_->CallRuntime(Runtime::kToSlowProperties, 1);
  }

  // Change to fast case at the end of an initialization block. To prepare for
  // that add an extra copy of the receiver to the frame, so that it can be
  // converted back to fast case after the assignment.
  if (node->ends_initialization_block() && !is_trivial_receiver) {
    frame_->Dup();
  }

  // Stack layout:
  // [tos]   : receiver (only materialized if non-trivial)
  // [tos+1] : receiver if at the end of an initialization block

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // For a compound assignment the right-hand side is a binary operation
    // between the current property value and the actual right-hand side.
    if (is_trivial_receiver) {
      Load(prop->obj());
    } else if (var != NULL) {
      LoadGlobal();
    } else {
      frame_->Dup();
    }
    EmitNamedLoad(name, var != NULL);

    // Perform the binary operation.
    Literal* literal = node->value()->AsLiteral();
    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    if (literal != NULL && literal->handle()->IsSmi()) {
      SmiOperation(node->binary_op(),
                   literal->handle(),
                   false,
                   overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
    } else {
      GenerateInlineSmi inline_smi =
          loop_nesting() > 0 ? GENERATE_INLINE_SMI : DONT_GENERATE_INLINE_SMI;
      if (literal != NULL) {
        ASSERT(!literal->handle()->IsSmi());
        inline_smi = DONT_GENERATE_INLINE_SMI;
      }
      Load(node->value());
      GenericBinaryOperation(node->binary_op(),
                             overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE,
                             inline_smi);
    }
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
    // Load the receiver and swap with the value.
    Load(prop->obj());
    Register t0 = frame_->PopToRegister();
    Register t1 = frame_->PopToRegister(t0);
    frame_->EmitPush(t0);
    frame_->EmitPush(t1);
  }
  CodeForSourcePosition(node->position());
  bool is_contextual = (var != NULL);
  EmitNamedStore(name, is_contextual);
  frame_->EmitPush(r0);

  // Change to fast case at the end of an initialization block.
  if (node->ends_initialization_block()) {
    ASSERT_EQ(NULL, var);
    // The argument to the runtime call is the receiver.
    if (is_trivial_receiver) {
      Load(prop->obj());
    } else {
      // A copy of the receiver is below the value of the assignment. Swap
      // the receiver and the value of the assignment expression.
      Register t0 = frame_->PopToRegister();
      Register t1 = frame_->PopToRegister(t0);
      frame_->EmitPush(t0);
      frame_->EmitPush(t1);
    }
    frame_->CallRuntime(Runtime::kToFastProperties, 1);
  }

  // Stack layout:
  // [tos]   : result

  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::EmitKeyedPropertyAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Keyed Property Assignment");
  Property* prop = node->target()->AsProperty();
  ASSERT_NOT_NULL(prop);

  // Evaluate the receiver subexpression.
  Load(prop->obj());

  WriteBarrierCharacter wb_info;

  // Change to slow case in the beginning of an initialization block to
  // avoid the quadratic behavior of repeatedly adding fast properties.
  if (node->starts_initialization_block()) {
    frame_->Dup();
    frame_->CallRuntime(Runtime::kToSlowProperties, 1);
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
  //
  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // For a compound assignment the right-hand side is a binary operation
    // between the current property value and the actual right-hand side.
    // Duplicate receiver and key for loading the current property value.
    frame_->Dup2();
    EmitKeyedLoad();
    frame_->EmitPush(r0);

    // Perform the binary operation.
    Literal* literal = node->value()->AsLiteral();
    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    if (literal != NULL && literal->handle()->IsSmi()) {
      SmiOperation(node->binary_op(),
                   literal->handle(),
                   false,
                   overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
    } else {
      GenerateInlineSmi inline_smi =
          loop_nesting() > 0 ? GENERATE_INLINE_SMI : DONT_GENERATE_INLINE_SMI;
      if (literal != NULL) {
        ASSERT(!literal->handle()->IsSmi());
        inline_smi = DONT_GENERATE_INLINE_SMI;
      }
      Load(node->value());
      GenericBinaryOperation(node->binary_op(),
                             overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE,
                             inline_smi);
    }
    wb_info = node->type()->IsLikelySmi() ? LIKELY_SMI : UNLIKELY_SMI;
  } else {
    // For non-compound assignment just load the right-hand side.
    Load(node->value());
    wb_info = node->value()->AsLiteral() != NULL ?
        NEVER_NEWSPACE :
        (node->value()->type()->IsLikelySmi() ? LIKELY_SMI : UNLIKELY_SMI);
  }

  // Stack layout:
  // [tos]   : value
  // [tos+1] : key
  // [tos+2] : receiver
  // [tos+3] : receiver if at the end of an initialization block

  // Perform the assignment.  It is safe to ignore constants here.
  ASSERT(node->op() != Token::INIT_CONST);
  CodeForSourcePosition(node->position());
  EmitKeyedStore(prop->key()->type(), wb_info);
  frame_->EmitPush(r0);

  // Stack layout:
  // [tos]   : result
  // [tos+1] : receiver if at the end of an initialization block

  // Change to fast case at the end of an initialization block.
  if (node->ends_initialization_block()) {
    // The argument to the runtime call is the extra copy of the receiver,
    // which is below the value of the assignment.  Swap the receiver and
    // the value of the assignment expression.
    Register t0 = frame_->PopToRegister();
    Register t1 = frame_->PopToRegister(t0);
    frame_->EmitPush(t1);
    frame_->EmitPush(t0);
    frame_->CallRuntime(Runtime::kToFastProperties, 1);
  }

  // Stack layout:
  // [tos]   : result

  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitAssignment(Assignment* node) {
  VirtualFrame::RegisterAllocationScope scope(this);
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Assignment");

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
    frame_->CallRuntime(Runtime::kThrowReferenceError, 1);
    // The runtime call doesn't actually return but the code generator will
    // still generate code and expects a certain frame height.
    frame_->EmitPush(r0);
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitThrow(Throw* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Throw");

  Load(node->exception());
  CodeForSourcePosition(node->position());
  frame_->CallRuntime(Runtime::kThrow, 1);
  frame_->EmitPush(r0);
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitProperty(Property* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ Property");

  { Reference property(this, node);
    property.GetValue();
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitCall(Call* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
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
    Load(function);

    // Allocate a frame slot for the receiver.
    frame_->EmitPushRoot(Heap::kUndefinedValueRootIndex);

    // Load the arguments.
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      Load(args->at(i));
    }

    VirtualFrame::SpilledScope spilled_scope(frame_);

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
      LoadFromGlobalSlotCheckExtensions(var->slot(),
                                        NOT_INSIDE_TYPEOF,
                                        &slow);
      frame_->EmitPush(r0);
      if (arg_count > 0) {
        __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));
        frame_->EmitPush(r1);
      } else {
        frame_->EmitPush(r2);
      }
      __ ldr(r1, frame_->Receiver());
      frame_->EmitPush(r1);

      frame_->CallRuntime(Runtime::kResolvePossiblyDirectEvalNoLookup, 3);

      done.Jump();
      slow.Bind();
    }

    // Prepare the stack for the call to ResolvePossiblyDirectEval by
    // pushing the loaded function, the first argument to the eval
    // call and the receiver.
    __ ldr(r1, MemOperand(sp, arg_count * kPointerSize + kPointerSize));
    frame_->EmitPush(r1);
    if (arg_count > 0) {
      __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));
      frame_->EmitPush(r1);
    } else {
      frame_->EmitPush(r2);
    }
    __ ldr(r1, frame_->Receiver());
    frame_->EmitPush(r1);

    // Resolve the call.
    frame_->CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // If we generated fast-case code bind the jump-target where fast
    // and slow case merge.
    if (done.is_linked()) done.Bind();

    // Touch up stack with the right values for the function and the receiver.
    __ str(r0, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ str(r1, MemOperand(sp, arg_count * kPointerSize));

    // Call the function.
    CodeForSourcePosition(node->position());

    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub call_function(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    frame_->CallStub(&call_function, arg_count + 1);

    __ ldr(cp, frame_->Context());
    // Remove the function from the stack.
    frame_->Drop();
    frame_->EmitPush(r0);

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
    }

    VirtualFrame::SpilledScope spilled_scope(frame_);
    // Setup the name register and call the IC initialization code.
    __ mov(r2, Operand(var->name()));
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
    CodeForSourcePosition(node->position());
    frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET_CONTEXT,
                           arg_count + 1);
    __ ldr(cp, frame_->Context());
    frame_->EmitPush(r0);

  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    VirtualFrame::SpilledScope spilled_scope(frame_);
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

    // JumpTargets do not yet support merging frames so the frame must be
    // spilled when jumping to these targets.
    JumpTarget slow, done;

    // Generate fast case for loading functions from slots that
    // correspond to local/global variables or arguments unless they
    // are shadowed by eval-introduced bindings.
    EmitDynamicLoadFromSlotFastCase(var->slot(),
                                    NOT_INSIDE_TYPEOF,
                                    &slow,
                                    &done);

    slow.Bind();
    // Load the function
    frame_->EmitPush(cp);
    __ mov(r0, Operand(var->name()));
    frame_->EmitPush(r0);
    frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    // r0: slot value; r1: receiver

    // Load the receiver.
    frame_->EmitPush(r0);  // function
    frame_->EmitPush(r1);  // receiver

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      JumpTarget call;
      call.Jump();
      done.Bind();
      frame_->EmitPush(r0);  // function
      LoadGlobalReceiver(r1);  // receiver
      call.Bind();
    }

    // Call the function. At this point, everything is spilled but the
    // function and receiver are in r0 and r1.
    CallWithArguments(args, NO_CALL_FUNCTION_FLAGS, node->position());
    frame_->EmitPush(r0);

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
        Load(property->obj());  // Receiver.
        // Load the arguments.
        int arg_count = args->length();
        for (int i = 0; i < arg_count; i++) {
          Load(args->at(i));
        }

        VirtualFrame::SpilledScope spilled_scope(frame_);
        // Set the name register and call the IC initialization code.
        __ mov(r2, Operand(name));
        InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
        Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
        CodeForSourcePosition(node->position());
        frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET, arg_count + 1);
        __ ldr(cp, frame_->Context());
        frame_->EmitPush(r0);
      }

    } else {
      // -------------------------------------------
      // JavaScript example: 'array[index](1, 2, 3)'
      // -------------------------------------------
      VirtualFrame::SpilledScope spilled_scope(frame_);

      Load(property->obj());
      if (property->is_synthetic()) {
        Load(property->key());
        EmitKeyedLoad();
        // Put the function below the receiver.
        // Use the global receiver.
        frame_->EmitPush(r0);  // Function.
        LoadGlobalReceiver(r0);
        // Call the function.
        CallWithArguments(args, RECEIVER_MIGHT_BE_VALUE, node->position());
        frame_->EmitPush(r0);
      } else {
        // Load the arguments.
        int arg_count = args->length();
        for (int i = 0; i < arg_count; i++) {
          Load(args->at(i));
        }

        // Set the name register and call the IC initialization code.
        Load(property->key());
        frame_->EmitPop(r2);  // Function name.

        InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
        Handle<Code> stub = ComputeKeyedCallInitialize(arg_count, in_loop);
        CodeForSourcePosition(node->position());
        frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET, arg_count + 1);
        __ ldr(cp, frame_->Context());
        frame_->EmitPush(r0);
      }
    }

  } else {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is not global
    // ----------------------------------

    // Load the function.
    Load(function);

    VirtualFrame::SpilledScope spilled_scope(frame_);

    // Pass the global proxy as the receiver.
    LoadGlobalReceiver(r0);

    // Call the function.
    CallWithArguments(args, NO_CALL_FUNCTION_FLAGS, node->position());
    frame_->EmitPush(r0);
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitCallNew(CallNew* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ CallNew");

  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments. This is different from ordinary calls, where the
  // actual function to call is resolved after the arguments have been
  // evaluated.

  // Compute function to call and use the global object as the
  // receiver. There is no need to use the global proxy here because
  // it will always be replaced with a newly allocated object.
  Load(node->expression());
  LoadGlobal();

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = node->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
  }

  VirtualFrame::SpilledScope spilled_scope(frame_);

  // r0: the number of arguments.
  __ mov(r0, Operand(arg_count));
  // Load the function into r1 as per calling convention.
  __ ldr(r1, frame_->ElementAt(arg_count + 1));

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  CodeForSourcePosition(node->position());
  Handle<Code> ic(Builtins::builtin(Builtins::JSConstructCall));
  frame_->CallCodeObject(ic, RelocInfo::CONSTRUCT_CALL, arg_count + 1);

  // Discard old TOS value and push r0 on the stack (same as Pop(), push(r0)).
  __ str(r0, frame_->Top());
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::GenerateClassOf(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope(frame_);
  ASSERT(args->length() == 1);
  JumpTarget leave, null, function, non_function_constructor;

  // Load the object into r0.
  Load(args->at(0));
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
  STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  STATIC_ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  ASSERT(args->length() == 1);
  JumpTarget leave;
  Load(args->at(0));
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  ASSERT(args->length() == 2);
  JumpTarget leave;
  Load(args->at(0));    // Load the object.
  Load(args->at(1));    // Load the value.
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
  __ RecordWrite(r1, Operand(JSValue::kValueOffset - kHeapObjectTag), r2, r3);
  // Leave.
  leave.Bind();
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register reg = frame_->PopToRegister();
  __ tst(reg, Operand(kSmiTagMask));
  cc_reg_ = eq;
}


void CodeGenerator::GenerateLog(ZoneList<Expression*>* args) {
  // See comment in CodeGenerator::GenerateLog in codegen-ia32.cc.
  ASSERT_EQ(args->length(), 3);
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (ShouldGenerateLog(args->at(0))) {
    Load(args->at(1));
    Load(args->at(2));
    frame_->CallRuntime(Runtime::kLog, 2);
  }
#endif
  frame_->EmitPushRoot(Heap::kUndefinedValueRootIndex);
}


void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register reg = frame_->PopToRegister();
  __ tst(reg, Operand(kSmiTagMask | 0x80000000u));
  cc_reg_ = eq;
}


// Generates the Math.pow method.
void CodeGenerator::GenerateMathPow(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);
  Load(args->at(0));
  Load(args->at(1));

  if (!CpuFeatures::IsSupported(VFP3)) {
    frame_->CallRuntime(Runtime::kMath_pow, 2);
    frame_->EmitPush(r0);
  } else {
    CpuFeatures::Scope scope(VFP3);
    JumpTarget runtime, done;
    Label exponent_nonsmi, base_nonsmi, powi, not_minus_half, allocate_return;

    Register scratch1 = VirtualFrame::scratch0();
    Register scratch2 = VirtualFrame::scratch1();

    // Get base and exponent to registers.
    Register exponent = frame_->PopToRegister();
    Register base = frame_->PopToRegister(exponent);
    Register heap_number_map = no_reg;

    // Set the frame for the runtime jump target. The code below jumps to the
    // jump target label so the frame needs to be established before that.
    ASSERT(runtime.entry_frame() == NULL);
    runtime.set_entry_frame(frame_);

    __ BranchOnNotSmi(exponent, &exponent_nonsmi);
    __ BranchOnNotSmi(base, &base_nonsmi);

    heap_number_map = r6;
    __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

    // Exponent is a smi and base is a smi. Get the smi value into vfp register
    // d1.
    __ SmiToDoubleVFPRegister(base, d1, scratch1, s0);
    __ b(&powi);

    __ bind(&base_nonsmi);
    // Exponent is smi and base is non smi. Get the double value from the base
    // into vfp register d1.
    __ ObjectToDoubleVFPRegister(base, d1,
                                 scratch1, scratch2, heap_number_map, s0,
                                 runtime.entry_label());

    __ bind(&powi);

    // Load 1.0 into d0.
    __ vmov(d0, 1.0);

    // Get the absolute untagged value of the exponent and use that for the
    // calculation.
    __ mov(scratch1, Operand(exponent, ASR, kSmiTagSize), SetCC);
    __ rsb(scratch1, scratch1, Operand(0), LeaveCC, mi);  // Negate if negative.
    __ vmov(d2, d0, mi);  // 1.0 needed in d2 later if exponent is negative.

    // Run through all the bits in the exponent. The result is calculated in d0
    // and d1 holds base^(bit^2).
    Label more_bits;
    __ bind(&more_bits);
    __ mov(scratch1, Operand(scratch1, LSR, 1), SetCC);
    __ vmul(d0, d0, d1, cs);  // Multiply with base^(bit^2) if bit is set.
    __ vmul(d1, d1, d1, ne);  // Don't bother calculating next d1 if done.
    __ b(ne, &more_bits);

    // If exponent is positive we are done.
    __ cmp(exponent, Operand(0));
    __ b(ge, &allocate_return);

    // If exponent is negative result is 1/result (d2 already holds 1.0 in that
    // case). However if d0 has reached infinity this will not provide the
    // correct result, so call runtime if that is the case.
    __ mov(scratch2, Operand(0x7FF00000));
    __ mov(scratch1, Operand(0));
    __ vmov(d1, scratch1, scratch2);  // Load infinity into d1.
    __ vcmp(d0, d1);
    __ vmrs(pc);
    runtime.Branch(eq);  // d0 reached infinity.
    __ vdiv(d0, d2, d0);
    __ b(&allocate_return);

    __ bind(&exponent_nonsmi);
    // Special handling of raising to the power of -0.5 and 0.5. First check
    // that the value is a heap number and that the lower bits (which for both
    // values are zero).
    heap_number_map = r6;
    __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
    __ ldr(scratch1, FieldMemOperand(exponent, HeapObject::kMapOffset));
    __ ldr(scratch2, FieldMemOperand(exponent, HeapNumber::kMantissaOffset));
    __ cmp(scratch1, heap_number_map);
    runtime.Branch(ne);
    __ tst(scratch2, scratch2);
    runtime.Branch(ne);

    // Load the higher bits (which contains the floating point exponent).
    __ ldr(scratch1, FieldMemOperand(exponent, HeapNumber::kExponentOffset));

    // Compare exponent with -0.5.
    __ cmp(scratch1, Operand(0xbfe00000));
    __ b(ne, &not_minus_half);

    // Get the double value from the base into vfp register d0.
    __ ObjectToDoubleVFPRegister(base, d0,
                                 scratch1, scratch2, heap_number_map, s0,
                                 runtime.entry_label(),
                                 AVOID_NANS_AND_INFINITIES);

    // Load 1.0 into d2.
    __ vmov(d2, 1.0);

    // Calculate the reciprocal of the square root. 1/sqrt(x) = sqrt(1/x).
    __ vdiv(d0, d2, d0);
    __ vsqrt(d0, d0);

    __ b(&allocate_return);

    __ bind(&not_minus_half);
    // Compare exponent with 0.5.
    __ cmp(scratch1, Operand(0x3fe00000));
    runtime.Branch(ne);

      // Get the double value from the base into vfp register d0.
    __ ObjectToDoubleVFPRegister(base, d0,
                                 scratch1, scratch2, heap_number_map, s0,
                                 runtime.entry_label(),
                                 AVOID_NANS_AND_INFINITIES);
    __ vsqrt(d0, d0);

    __ bind(&allocate_return);
    Register scratch3 = r5;
    __ AllocateHeapNumberWithValue(scratch3, d0, scratch1, scratch2,
                                   heap_number_map, runtime.entry_label());
    __ mov(base, scratch3);
    done.Jump();

    runtime.Bind();

    // Push back the arguments again for the runtime call.
    frame_->EmitPush(base);
    frame_->EmitPush(exponent);
    frame_->CallRuntime(Runtime::kMath_pow, 2);
    __ Move(base, r0);

    done.Bind();
    frame_->EmitPush(base);
  }
}


// Generates the Math.sqrt method.
void CodeGenerator::GenerateMathSqrt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));

  if (!CpuFeatures::IsSupported(VFP3)) {
    frame_->CallRuntime(Runtime::kMath_sqrt, 1);
    frame_->EmitPush(r0);
  } else {
    CpuFeatures::Scope scope(VFP3);
    JumpTarget runtime, done;

    Register scratch1 = VirtualFrame::scratch0();
    Register scratch2 = VirtualFrame::scratch1();

    // Get the value from the frame.
    Register tos = frame_->PopToRegister();

    // Set the frame for the runtime jump target. The code below jumps to the
    // jump target label so the frame needs to be established before that.
    ASSERT(runtime.entry_frame() == NULL);
    runtime.set_entry_frame(frame_);

    Register heap_number_map = r6;
    __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

    // Get the double value from the heap number into vfp register d0.
    __ ObjectToDoubleVFPRegister(tos, d0,
                                 scratch1, scratch2, heap_number_map, s0,
                                 runtime.entry_label());

    // Calculate the square root of d0 and place result in a heap number object.
    __ vsqrt(d0, d0);
    __ AllocateHeapNumberWithValue(
        tos, d0, scratch1, scratch2, heap_number_map, runtime.entry_label());
    done.Jump();

    runtime.Bind();
    // Push back the argument again for the runtime call.
    frame_->EmitPush(tos);
    frame_->CallRuntime(Runtime::kMath_sqrt, 1);
    __ Move(tos, r0);

    done.Bind();
    frame_->EmitPush(tos);
  }
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  Comment(masm_, "[ GenerateStringCharCodeAt");
  ASSERT(args->length() == 2);

  Load(args->at(0));
  Load(args->at(1));

  Register index = r1;
  Register object = r2;

  frame_->EmitPop(r1);
  frame_->EmitPop(r2);

  // We need two extra registers.
  Register scratch = r3;
  Register result = r0;

  DeferredStringCharCodeAt* deferred =
      new DeferredStringCharCodeAt(object,
                                   index,
                                   scratch,
                                   result);
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->EmitPush(result);
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  Comment(masm_, "[ GenerateStringCharFromCode");
  ASSERT(args->length() == 1);

  Load(args->at(0));

  Register code = r1;
  Register result = r0;

  frame_->EmitPop(code);

  DeferredStringCharFromCode* deferred = new DeferredStringCharFromCode(
      code, result);
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->EmitPush(result);
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
    __ mov(result_, Operand(Smi::FromInt(0)));
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
  VirtualFrame::SpilledScope spilled_scope(frame_);
  Comment(masm_, "[ GenerateStringCharAt");
  ASSERT(args->length() == 2);

  Load(args->at(0));
  Load(args->at(1));

  Register index = r1;
  Register object = r2;

  frame_->EmitPop(r1);
  frame_->EmitPop(r2);

  // We need three extra registers.
  Register scratch1 = r3;
  Register scratch2 = r4;
  Register result = r0;

  DeferredStringCharAt* deferred =
      new DeferredStringCharAt(object,
                               index,
                               scratch1,
                               scratch2,
                               result);
  deferred->fast_case_generator()->GenerateFast(masm_);
  deferred->BindExit();
  frame_->EmitPush(result);
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  JumpTarget answer;
  // We need the CC bits to come out as not_equal in the case where the
  // object is a smi.  This can't be done with the usual test opcode so
  // we use XOR to get the right CC bits.
  Register possible_array = frame_->PopToRegister();
  Register scratch = VirtualFrame::scratch0();
  __ and_(scratch, possible_array, Operand(kSmiTagMask));
  __ eor(scratch, scratch, Operand(kSmiTagMask), SetCC);
  answer.Branch(ne);
  // It is a heap object - get the map. Check if the object is a JS array.
  __ CompareObjectType(possible_array, scratch, scratch, JS_ARRAY_TYPE);
  answer.Bind();
  cc_reg_ = eq;
}


void CodeGenerator::GenerateIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  JumpTarget answer;
  // We need the CC bits to come out as not_equal in the case where the
  // object is a smi.  This can't be done with the usual test opcode so
  // we use XOR to get the right CC bits.
  Register possible_regexp = frame_->PopToRegister();
  Register scratch = VirtualFrame::scratch0();
  __ and_(scratch, possible_regexp, Operand(kSmiTagMask));
  __ eor(scratch, scratch, Operand(kSmiTagMask), SetCC);
  answer.Branch(ne);
  // It is a heap object - get the map. Check if the object is a regexp.
  __ CompareObjectType(possible_regexp, scratch, scratch, JS_REGEXP_TYPE);
  answer.Bind();
  cc_reg_ = eq;
}


void CodeGenerator::GenerateIsObject(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (typeof(arg) === 'object' || %_ClassOf(arg) == 'RegExp')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register possible_object = frame_->PopToRegister();
  __ tst(possible_object, Operand(kSmiTagMask));
  false_target()->Branch(eq);

  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(possible_object, ip);
  true_target()->Branch(eq);

  Register map_reg = VirtualFrame::scratch0();
  __ ldr(map_reg, FieldMemOperand(possible_object, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ ldrb(possible_object, FieldMemOperand(map_reg, Map::kBitFieldOffset));
  __ tst(possible_object, Operand(1 << Map::kIsUndetectable));
  false_target()->Branch(ne);

  __ ldrb(possible_object, FieldMemOperand(map_reg, Map::kInstanceTypeOffset));
  __ cmp(possible_object, Operand(FIRST_JS_OBJECT_TYPE));
  false_target()->Branch(lt);
  __ cmp(possible_object, Operand(LAST_JS_OBJECT_TYPE));
  cc_reg_ = le;
}


void CodeGenerator::GenerateIsSpecObject(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (typeof(arg) === 'object' || %_ClassOf(arg) == 'RegExp' ||
  // typeof(arg) == function).
  // It includes undetectable objects (as opposed to IsObject).
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register value = frame_->PopToRegister();
  __ tst(value, Operand(kSmiTagMask));
  false_target()->Branch(eq);
  // Check that this is an object.
  __ ldr(value, FieldMemOperand(value, HeapObject::kMapOffset));
  __ ldrb(value, FieldMemOperand(value, Map::kInstanceTypeOffset));
  __ cmp(value, Operand(FIRST_JS_OBJECT_TYPE));
  cc_reg_ = ge;
}


void CodeGenerator::GenerateIsFunction(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (%_ClassOf(arg) === 'Function')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register possible_function = frame_->PopToRegister();
  __ tst(possible_function, Operand(kSmiTagMask));
  false_target()->Branch(eq);
  Register map_reg = VirtualFrame::scratch0();
  Register scratch = VirtualFrame::scratch1();
  __ CompareObjectType(possible_function, map_reg, scratch, JS_FUNCTION_TYPE);
  cc_reg_ = eq;
}


void CodeGenerator::GenerateIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Register possible_undetectable = frame_->PopToRegister();
  __ tst(possible_undetectable, Operand(kSmiTagMask));
  false_target()->Branch(eq);
  Register scratch = VirtualFrame::scratch0();
  __ ldr(scratch,
         FieldMemOperand(possible_undetectable, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
  __ tst(scratch, Operand(1 << Map::kIsUndetectable));
  cc_reg_ = ne;
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Register scratch0 = VirtualFrame::scratch0();
  Register scratch1 = VirtualFrame::scratch1();
  // Get the frame pointer for the calling frame.
  __ ldr(scratch0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  __ ldr(scratch1,
         MemOperand(scratch0, StandardFrameConstants::kContextOffset));
  __ cmp(scratch1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ ldr(scratch0,
         MemOperand(scratch0, StandardFrameConstants::kCallerFPOffset), eq);

  // Check the marker in the calling frame.
  __ ldr(scratch1,
         MemOperand(scratch0, StandardFrameConstants::kMarkerOffset));
  __ cmp(scratch1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
  cc_reg_ = eq;
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Register tos = frame_->GetTOSRegister();
  Register scratch0 = VirtualFrame::scratch0();
  Register scratch1 = VirtualFrame::scratch1();

  // Check if the calling frame is an arguments adaptor frame.
  __ ldr(scratch0,
         MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(scratch1,
         MemOperand(scratch0, StandardFrameConstants::kContextOffset));
  __ cmp(scratch1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Get the number of formal parameters.
  __ mov(tos, Operand(Smi::FromInt(scope()->num_parameters())), LeaveCC, ne);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ ldr(tos,
         MemOperand(scratch0, ArgumentsAdaptorFrameConstants::kLengthOffset),
         eq);

  frame_->EmitPush(tos);
}


void CodeGenerator::GenerateArguments(ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope(frame_);
  ASSERT(args->length() == 1);

  // Satisfy contract with ArgumentsAccessStub:
  // Load the key into r1 and the formal parameters count into r0.
  Load(args->at(0));
  frame_->EmitPop(r1);
  __ mov(r0, Operand(Smi::FromInt(scope()->num_parameters())));

  // Call the shared stub to get to arguments[key].
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  frame_->CallStub(&stub, 0);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateRandomHeapNumber(
    ZoneList<Expression*>* args) {
  VirtualFrame::SpilledScope spilled_scope(frame_);
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(r4, r1, r2, r6, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(r4, Operand(r0));

  __ bind(&heapnumber_allocated);

  // Convert 32 random bits in r0 to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  if (CpuFeatures::IsSupported(VFP3)) {
    __ PrepareCallCFunction(0, r1);
    __ CallCFunction(ExternalReference::random_uint32_function(), 0);

    CpuFeatures::Scope scope(VFP3);
    // 0x41300000 is the top half of 1.0 x 2^20 as a double.
    // Create this constant using mov/orr to avoid PC relative load.
    __ mov(r1, Operand(0x41000000));
    __ orr(r1, r1, Operand(0x300000));
    // Move 0x41300000xxxxxxxx (x = random bits) to VFP.
    __ vmov(d7, r0, r1);
    // Move 0x4130000000000000 to VFP.
    __ mov(r0, Operand(0));
    __ vmov(d8, r0, r1);
    // Subtract and store the result in the heap number.
    __ vsub(d7, d7, d8);
    __ sub(r0, r4, Operand(kHeapObjectTag));
    __ vstr(d7, r0, HeapNumber::kValueOffset);
    frame_->EmitPush(r4);
  } else {
    __ mov(r0, Operand(r4));
    __ PrepareCallCFunction(1, r1);
    __ CallCFunction(
        ExternalReference::fill_heap_number_with_random_function(), 1);
    frame_->EmitPush(r0);
  }
}


void CodeGenerator::GenerateStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Load(args->at(0));
  Load(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  frame_->SpillAll();
  frame_->CallStub(&stub, 2);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateSubString(ZoneList<Expression*>* args) {
  ASSERT_EQ(3, args->length());

  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));

  SubStringStub stub;
  frame_->SpillAll();
  frame_->CallStub(&stub, 3);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Load(args->at(0));
  Load(args->at(1));

  StringCompareStub stub;
  frame_->SpillAll();
  frame_->CallStub(&stub, 2);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateRegExpExec(ZoneList<Expression*>* args) {
  ASSERT_EQ(4, args->length());

  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));
  Load(args->at(3));
  RegExpExecStub stub;
  frame_->SpillAll();
  frame_->CallStub(&stub, 4);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateRegExpConstructResult(ZoneList<Expression*>* args) {
  // No stub. This code only occurs a few times in regexp.js.
  const int kMaxInlineLength = 100;
  ASSERT_EQ(3, args->length());
  Load(args->at(0));  // Size of array, smi.
  Load(args->at(1));  // "index" property value.
  Load(args->at(2));  // "input" property value.
  {
    VirtualFrame::SpilledScope spilled_scope(frame_);
    Label slowcase;
    Label done;
    __ ldr(r1, MemOperand(sp, kPointerSize * 2));
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    __ tst(r1, Operand(kSmiTagMask));
    __ b(ne, &slowcase);
    __ cmp(r1, Operand(Smi::FromInt(kMaxInlineLength)));
    __ b(hi, &slowcase);
    // Smi-tagging is equivalent to multiplying by 2.
    // Allocate RegExpResult followed by FixedArray with size in ebx.
    // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
    // Elements:  [Map][Length][..elements..]
    // Size of JSArray with two in-object properties and the header of a
    // FixedArray.
    int objects_size =
        (JSRegExpResult::kSize + FixedArray::kHeaderSize) / kPointerSize;
    __ mov(r5, Operand(r1, LSR, kSmiTagSize + kSmiShiftSize));
    __ add(r2, r5, Operand(objects_size));
    __ AllocateInNewSpace(
        r2,  // In: Size, in words.
        r0,  // Out: Start of allocation (tagged).
        r3,  // Scratch register.
        r4,  // Scratch register.
        &slowcase,
        static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));
    // r0: Start of allocated area, object-tagged.
    // r1: Number of elements in array, as smi.
    // r5: Number of elements, untagged.

    // Set JSArray map to global.regexp_result_map().
    // Set empty properties FixedArray.
    // Set elements to point to FixedArray allocated right after the JSArray.
    // Interleave operations for better latency.
    __ ldr(r2, ContextOperand(cp, Context::GLOBAL_INDEX));
    __ add(r3, r0, Operand(JSRegExpResult::kSize));
    __ mov(r4, Operand(Factory::empty_fixed_array()));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalContextOffset));
    __ str(r3, FieldMemOperand(r0, JSObject::kElementsOffset));
    __ ldr(r2, ContextOperand(r2, Context::REGEXP_RESULT_MAP_INDEX));
    __ str(r4, FieldMemOperand(r0, JSObject::kPropertiesOffset));
    __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));

    // Set input, index and length fields from arguments.
    __ ldm(ia_w, sp, static_cast<RegList>(r2.bit() | r4.bit()));
    __ str(r1, FieldMemOperand(r0, JSArray::kLengthOffset));
    __ add(sp, sp, Operand(kPointerSize));
    __ str(r4, FieldMemOperand(r0, JSRegExpResult::kIndexOffset));
    __ str(r2, FieldMemOperand(r0, JSRegExpResult::kInputOffset));

    // Fill out the elements FixedArray.
    // r0: JSArray, tagged.
    // r3: FixedArray, tagged.
    // r5: Number of elements in array, untagged.

    // Set map.
    __ mov(r2, Operand(Factory::fixed_array_map()));
    __ str(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
    // Set FixedArray length.
    __ mov(r6, Operand(r5, LSL, kSmiTagSize));
    __ str(r6, FieldMemOperand(r3, FixedArray::kLengthOffset));
    // Fill contents of fixed-array with the-hole.
    __ mov(r2, Operand(Factory::the_hole_value()));
    __ add(r3, r3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    // Fill fixed array elements with hole.
    // r0: JSArray, tagged.
    // r2: the hole.
    // r3: Start of elements in FixedArray.
    // r5: Number of elements to fill.
    Label loop;
    __ tst(r5, Operand(r5));
    __ bind(&loop);
    __ b(le, &done);  // Jump if r1 is negative or zero.
    __ sub(r5, r5, Operand(1), SetCC);
    __ str(r2, MemOperand(r3, r5, LSL, kPointerSizeLog2));
    __ jmp(&loop);

    __ bind(&slowcase);
    __ CallRuntime(Runtime::kRegExpConstructResult, 3);

    __ bind(&done);
  }
  frame_->Forget(3);
  frame_->EmitPush(r0);
}


class DeferredSearchCache: public DeferredCode {
 public:
  DeferredSearchCache(Register dst, Register cache, Register key)
      : dst_(dst), cache_(cache), key_(key) {
    set_comment("[ DeferredSearchCache");
  }

  virtual void Generate();

 private:
  Register dst_, cache_, key_;
};


void DeferredSearchCache::Generate() {
  __ Push(cache_, key_);
  __ CallRuntime(Runtime::kGetFromCache, 2);
  if (!dst_.is(r0)) {
    __ mov(dst_, r0);
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
    frame_->EmitPushRoot(Heap::kUndefinedValueRootIndex);
    return;
  }

  Load(args->at(1));

  VirtualFrame::SpilledScope spilled_scope(frame_);

  frame_->EmitPop(r2);

  __ ldr(r1, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalContextOffset));
  __ ldr(r1, ContextOperand(r1, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ ldr(r1, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(cache_id)));

  DeferredSearchCache* deferred = new DeferredSearchCache(r0, r1, r2);

  const int kFingerOffset =
      FixedArray::OffsetOfElementAt(JSFunctionResultCache::kFingerIndex);
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ ldr(r0, FieldMemOperand(r1, kFingerOffset));
  // r0 now holds finger offset as a smi.
  __ add(r3, r1, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // r3 now points to the start of fixed array elements.
  __ ldr(r0, MemOperand(r3, r0, LSL, kPointerSizeLog2 - kSmiTagSize, PreIndex));
  // Note side effect of PreIndex: r3 now points to the key of the pair.
  __ cmp(r2, r0);
  deferred->Branch(ne);

  __ ldr(r0, MemOperand(r3, kPointerSize));

  deferred->BindExit();
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and jump to the runtime.
  Load(args->at(0));

  NumberToStringStub stub;
  frame_->SpillAll();
  frame_->CallStub(&stub, 1);
  frame_->EmitPush(r0);
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

  VirtualFrame::SpilledScope spilled_scope(frame_);

  Register index2 = r2;
  Register index1 = r1;
  Register object = r0;
  Register tmp1 = r3;
  Register tmp2 = r4;

  frame_->EmitPop(index2);
  frame_->EmitPop(index1);
  frame_->EmitPop(object);

  DeferredSwapElements* deferred =
      new DeferredSwapElements(object, index1, index2);

  // Fetch the map and check if array is in fast case.
  // Check that object doesn't require security checks and
  // has no indexed interceptor.
  __ CompareObjectType(object, tmp1, tmp2, FIRST_JS_OBJECT_TYPE);
  deferred->Branch(lt);
  __ ldrb(tmp2, FieldMemOperand(tmp1, Map::kBitFieldOffset));
  __ tst(tmp2, Operand(KeyedLoadIC::kSlowCaseBitFieldMask));
  deferred->Branch(nz);

  // Check the object's elements are in fast case.
  __ ldr(tmp1, FieldMemOperand(object, JSObject::kElementsOffset));
  __ ldr(tmp2, FieldMemOperand(tmp1, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(tmp2, ip);
  deferred->Branch(ne);

  // Smi-tagging is equivalent to multiplying by 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  // Check that both indices are smis.
  __ mov(tmp2, index1);
  __ orr(tmp2, tmp2, index2);
  __ tst(tmp2, Operand(kSmiTagMask));
  deferred->Branch(nz);

  // Bring the offsets into the fixed array in tmp1 into index1 and
  // index2.
  __ mov(tmp2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(index1, tmp2, Operand(index1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(index2, tmp2, Operand(index2, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Swap elements.
  Register tmp3 = object;
  object = no_reg;
  __ ldr(tmp3, MemOperand(tmp1, index1));
  __ ldr(tmp2, MemOperand(tmp1, index2));
  __ str(tmp3, MemOperand(tmp1, index2));
  __ str(tmp2, MemOperand(tmp1, index1));

  Label done;
  __ InNewSpace(tmp1, tmp2, eq, &done);
  // Possible optimization: do a check that both values are Smis
  // (or them and test against Smi mask.)

  __ mov(tmp2, tmp1);
  RecordWriteStub recordWrite1(tmp1, index1, tmp3);
  __ CallStub(&recordWrite1);

  RecordWriteStub recordWrite2(tmp2, index2, tmp3);
  __ CallStub(&recordWrite2);

  __ bind(&done);

  deferred->BindExit();
  __ LoadRoot(tmp1, Heap::kUndefinedValueRootIndex);
  frame_->EmitPush(tmp1);
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
  frame_->CallJSFunction(n_args);
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateMathSin(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);
  Load(args->at(0));
  if (CpuFeatures::IsSupported(VFP3)) {
    TranscendentalCacheStub stub(TranscendentalCache::SIN);
    frame_->SpillAllButCopyTOSToR0();
    frame_->CallStub(&stub, 1);
  } else {
    frame_->CallRuntime(Runtime::kMath_sin, 1);
  }
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateMathCos(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);
  Load(args->at(0));
  if (CpuFeatures::IsSupported(VFP3)) {
    TranscendentalCacheStub stub(TranscendentalCache::COS);
    frame_->SpillAllButCopyTOSToR0();
    frame_->CallStub(&stub, 1);
  } else {
    frame_->CallRuntime(Runtime::kMath_cos, 1);
  }
  frame_->EmitPush(r0);
}


void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  Load(args->at(0));
  Load(args->at(1));
  Register lhs = frame_->PopToRegister();
  Register rhs = frame_->PopToRegister(lhs);
  __ cmp(lhs, rhs);
  cc_reg_ = eq;
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
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
    // Push the builtins object found in the current global object.
    Register scratch = VirtualFrame::scratch0();
    __ ldr(scratch, GlobalObject());
    Register builtins = frame_->GetTOSRegister();
    __ ldr(builtins, FieldMemOperand(scratch, GlobalObject::kBuiltinsOffset));
    frame_->EmitPush(builtins);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Load(args->at(i));
  }

  VirtualFrame::SpilledScope spilled_scope(frame_);

  if (function == NULL) {
    // Call the JS runtime function.
    __ mov(r2, Operand(node->name()));
    InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> stub = ComputeCallInitialize(arg_count, in_loop);
    frame_->CallCodeObject(stub, RelocInfo::CODE_TARGET, arg_count + 1);
    __ ldr(cp, frame_->Context());
    frame_->EmitPush(r0);
  } else {
    // Call the C runtime function.
    frame_->CallRuntime(function, arg_count);
    frame_->EmitPush(r0);
  }
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ UnaryOperation");

  Token::Value op = node->op();

  if (op == Token::NOT) {
    LoadCondition(node->expression(), false_target(), true_target(), true);
    // LoadCondition may (and usually does) leave a test and branch to
    // be emitted by the caller.  In that case, negate the condition.
    if (has_cc()) cc_reg_ = NegateCondition(cc_reg_);

  } else if (op == Token::DELETE) {
    Property* property = node->expression()->AsProperty();
    Variable* variable = node->expression()->AsVariableProxy()->AsVariable();
    if (property != NULL) {
      Load(property->obj());
      Load(property->key());
      frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, 2);
      frame_->EmitPush(r0);

    } else if (variable != NULL) {
      Slot* slot = variable->slot();
      if (variable->is_global()) {
        LoadGlobal();
        frame_->EmitPush(Operand(variable->name()));
        frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, 2);
        frame_->EmitPush(r0);

      } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
        // lookup the context holding the named variable
        frame_->EmitPush(cp);
        frame_->EmitPush(Operand(variable->name()));
        frame_->CallRuntime(Runtime::kLookupContext, 2);
        // r0: context
        frame_->EmitPush(r0);
        frame_->EmitPush(Operand(variable->name()));
        frame_->InvokeBuiltin(Builtins::DELETE, CALL_JS, 2);
        frame_->EmitPush(r0);

      } else {
        // Default: Result of deleting non-global, not dynamically
        // introduced variables is false.
        frame_->EmitPushRoot(Heap::kFalseValueRootIndex);
      }

    } else {
      // Default: Result of deleting expressions is true.
      Load(node->expression());  // may have side-effects
      frame_->Drop();
      frame_->EmitPushRoot(Heap::kTrueValueRootIndex);
    }

  } else if (op == Token::TYPEOF) {
    // Special case for loading the typeof expression; see comment on
    // LoadTypeofExpression().
    LoadTypeofExpression(node->expression());
    frame_->CallRuntime(Runtime::kTypeof, 1);
    frame_->EmitPush(r0);  // r0 has result

  } else {
    bool can_overwrite =
        (node->expression()->AsBinaryOperation() != NULL &&
         node->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
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
        frame_->PopToR0();
        GenericUnaryOpStub stub(
            Token::SUB,
            overwrite,
            no_negative_zero ? kIgnoreNegativeZero : kStrictNegativeZero);
        frame_->CallStub(&stub, 0);
        frame_->EmitPush(r0);  // r0 has result
        break;
      }

      case Token::BIT_NOT: {
        Register tos = frame_->PopToRegister();
        JumpTarget not_smi_label;
        JumpTarget continue_label;
        // Smi check.
        __ tst(tos, Operand(kSmiTagMask));
        not_smi_label.Branch(ne);

        __ mvn(tos, Operand(tos));
        __ bic(tos, tos, Operand(kSmiTagMask));  // Bit-clear inverted smi-tag.
        frame_->EmitPush(tos);
        // The fast case is the first to jump to the continue label, so it gets
        // to decide the virtual frame layout.
        continue_label.Jump();

        not_smi_label.Bind();
        frame_->SpillAll();
        __ Move(r0, tos);
        GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
        frame_->CallStub(&stub, 0);
        frame_->EmitPush(r0);

        continue_label.Bind();
        break;
      }

      case Token::VOID:
        frame_->Drop();
        frame_->EmitPushRoot(Heap::kUndefinedValueRootIndex);
        break;

      case Token::ADD: {
        Register tos = frame_->Peek();
        // Smi check.
        JumpTarget continue_label;
        __ tst(tos, Operand(kSmiTagMask));
        continue_label.Branch(eq);

        frame_->InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS, 1);
        frame_->EmitPush(r0);

        continue_label.Bind();
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ CountOperation");
  VirtualFrame::RegisterAllocationScope scope(this);

  bool is_postfix = node->is_postfix();
  bool is_increment = node->op() == Token::INC;

  Variable* var = node->expression()->AsVariableProxy()->AsVariable();
  bool is_const = (var != NULL && var->mode() == Variable::CONST);
  bool is_slot = (var != NULL && var->mode() == Variable::VAR);

  if (!is_const && is_slot && type_info(var->slot()).IsSmi()) {
    // The type info declares that this variable is always a Smi.  That
    // means it is a Smi both before and after the increment/decrement.
    // Lets make use of that to make a very minimal count.
    Reference target(this, node->expression(), !is_const);
    ASSERT(!target.is_illegal());
    target.GetValue();  // Pushes the value.
    Register value = frame_->PopToRegister();
    if (is_postfix) frame_->EmitPush(value);
    if (is_increment) {
      __ add(value, value, Operand(Smi::FromInt(1)));
    } else {
      __ sub(value, value, Operand(Smi::FromInt(1)));
    }
    frame_->EmitPush(value);
    target.SetValue(NOT_CONST_INIT, LIKELY_SMI);
    if (is_postfix) frame_->Pop();
    ASSERT_EQ(original_height + 1, frame_->height());
    return;
  }

  // If it's a postfix expression and its result is not ignored and the
  // reference is non-trivial, then push a placeholder on the stack now
  // to hold the result of the expression.
  bool placeholder_pushed = false;
  if (!is_slot && is_postfix) {
    frame_->EmitPush(Operand(Smi::FromInt(0)));
    placeholder_pushed = true;
  }

  // A constant reference is not saved to, so a constant reference is not a
  // compound assignment reference.
  { Reference target(this, node->expression(), !is_const);
    if (target.is_illegal()) {
      // Spoof the virtual frame to have the expected height (one higher
      // than on entry).
      if (!placeholder_pushed) frame_->EmitPush(Operand(Smi::FromInt(0)));
      ASSERT_EQ(original_height + 1, frame_->height());
      return;
    }

    // This pushes 0, 1 or 2 words on the object to be used later when updating
    // the target.  It also pushes the current value of the target.
    target.GetValue();

    JumpTarget slow;
    JumpTarget exit;

    Register value = frame_->PopToRegister();

    // Postfix: Store the old value as the result.
    if (placeholder_pushed) {
      frame_->SetElementAt(value, target.size());
    } else if (is_postfix) {
      frame_->EmitPush(value);
      __ mov(VirtualFrame::scratch0(), value);
      value = VirtualFrame::scratch0();
    }

    // Check for smi operand.
    __ tst(value, Operand(kSmiTagMask));
    slow.Branch(ne);

    // Perform optimistic increment/decrement.
    if (is_increment) {
      __ add(value, value, Operand(Smi::FromInt(1)), SetCC);
    } else {
      __ sub(value, value, Operand(Smi::FromInt(1)), SetCC);
    }

    // If the increment/decrement didn't overflow, we're done.
    exit.Branch(vc);

    // Revert optimistic increment/decrement.
    if (is_increment) {
      __ sub(value, value, Operand(Smi::FromInt(1)));
    } else {
      __ add(value, value, Operand(Smi::FromInt(1)));
    }

    // Slow case: Convert to number.  At this point the
    // value to be incremented is in the value register..
    slow.Bind();

    // Convert the operand to a number.
    frame_->EmitPush(value);

    {
      VirtualFrame::SpilledScope spilled(frame_);
      frame_->InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS, 1);

      if (is_postfix) {
        // Postfix: store to result (on the stack).
        __ str(r0, frame_->ElementAt(target.size()));
      }

      // Compute the new value.
      frame_->EmitPush(r0);
      frame_->EmitPush(Operand(Smi::FromInt(1)));
      if (is_increment) {
        frame_->CallRuntime(Runtime::kNumberAdd, 2);
      } else {
        frame_->CallRuntime(Runtime::kNumberSub, 2);
      }
    }

    __ Move(value, r0);
    // Store the new value in the target if not const.
    // At this point the answer is in the value register.
    exit.Bind();
    frame_->EmitPush(value);
    // Set the target with the result, leaving the result on
    // top of the stack.  Removes the target from the stack if
    // it has a non-zero size.
    if (!is_const) target.SetValue(NOT_CONST_INIT, LIKELY_SMI);
  }

  // Postfix: Discard the new value and use the old.
  if (is_postfix) frame_->Pop();
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::GenerateLogicalBooleanOperation(BinaryOperation* node) {
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
  if (node->op() == Token::AND) {
    JumpTarget is_true;
    LoadCondition(node->left(), &is_true, false_target(), false);
    if (has_valid_frame() && !has_cc()) {
      // The left-hand side result is on top of the virtual frame.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      frame_->Dup();
      // Avoid popping the result if it converts to 'false' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      ToBoolean(&pop_and_continue, &exit);
      Branch(false, &exit);

      // Pop the result of evaluating the first part.
      pop_and_continue.Bind();
      frame_->Pop();

      // Evaluate right side expression.
      is_true.Bind();
      Load(node->right());

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
      LoadCondition(node->right(), true_target(), false_target(), false);
    } else {
      // Nothing to do.
      ASSERT(!has_valid_frame() && !has_cc() && !is_true.is_linked());
    }

  } else {
    ASSERT(node->op() == Token::OR);
    JumpTarget is_false;
    LoadCondition(node->left(), true_target(), &is_false, false);
    if (has_valid_frame() && !has_cc()) {
      // The left-hand side result is on top of the virtual frame.
      JumpTarget pop_and_continue;
      JumpTarget exit;

      frame_->Dup();
      // Avoid popping the result if it converts to 'true' using the
      // standard ToBoolean() conversion as described in ECMA-262,
      // section 9.2, page 30.
      ToBoolean(&exit, &pop_and_continue);
      Branch(true, &exit);

      // Pop the result of evaluating the first part.
      pop_and_continue.Bind();
      frame_->Pop();

      // Evaluate right side expression.
      is_false.Bind();
      Load(node->right());

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
      LoadCondition(node->right(), true_target(), false_target(), false);
    } else {
      // Nothing to do.
      ASSERT(!has_valid_frame() && !has_cc() && !is_false.is_linked());
    }
  }
}


void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ BinaryOperation");

  if (node->op() == Token::AND || node->op() == Token::OR) {
    GenerateLogicalBooleanOperation(node);
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
      VirtualFrame::RegisterAllocationScope scope(this);
      Load(node->left());
      if (frame_->KnownSmiAt(0)) overwrite_left = false;
      SmiOperation(node->op(),
                   rliteral->handle(),
                   false,
                   overwrite_left ? OVERWRITE_LEFT : NO_OVERWRITE);
    } else if (lliteral != NULL && lliteral->handle()->IsSmi()) {
      VirtualFrame::RegisterAllocationScope scope(this);
      Load(node->right());
      if (frame_->KnownSmiAt(0)) overwrite_right = false;
      SmiOperation(node->op(),
                   lliteral->handle(),
                   true,
                   overwrite_right ? OVERWRITE_RIGHT : NO_OVERWRITE);
    } else {
      GenerateInlineSmi inline_smi =
          loop_nesting() > 0 ? GENERATE_INLINE_SMI : DONT_GENERATE_INLINE_SMI;
      if (lliteral != NULL) {
        ASSERT(!lliteral->handle()->IsSmi());
        inline_smi = DONT_GENERATE_INLINE_SMI;
      }
      if (rliteral != NULL) {
        ASSERT(!rliteral->handle()->IsSmi());
        inline_smi = DONT_GENERATE_INLINE_SMI;
      }
      VirtualFrame::RegisterAllocationScope scope(this);
      OverwriteMode overwrite_mode = NO_OVERWRITE;
      if (overwrite_left) {
        overwrite_mode = OVERWRITE_LEFT;
      } else if (overwrite_right) {
        overwrite_mode = OVERWRITE_RIGHT;
      }
      Load(node->left());
      Load(node->right());
      GenericBinaryOperation(node->op(), overwrite_mode, inline_smi);
    }
  }
  ASSERT(!has_valid_frame() ||
         (has_cc() && frame_->height() == original_height) ||
         (!has_cc() && frame_->height() == original_height + 1));
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  frame_->EmitPush(MemOperand(frame_->Function()));
  ASSERT_EQ(original_height + 1, frame_->height());
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  Comment cmnt(masm_, "[ CompareOperation");

  VirtualFrame::RegisterAllocationScope nonspilled_scope(this);

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
      Load(left_is_null ? right : left);
      Register tos = frame_->PopToRegister();
      __ LoadRoot(ip, Heap::kNullValueRootIndex);
      __ cmp(tos, ip);

      // The 'null' value is only equal to 'undefined' if using non-strict
      // comparisons.
      if (op != Token::EQ_STRICT) {
        true_target()->Branch(eq);

        __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
        __ cmp(tos, Operand(ip));
        true_target()->Branch(eq);

        __ tst(tos, Operand(kSmiTagMask));
        false_target()->Branch(eq);

        // It can be an undetectable object.
        __ ldr(tos, FieldMemOperand(tos, HeapObject::kMapOffset));
        __ ldrb(tos, FieldMemOperand(tos, Map::kBitFieldOffset));
        __ and_(tos, tos, Operand(1 << Map::kIsUndetectable));
        __ cmp(tos, Operand(1 << Map::kIsUndetectable));
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

    // Load the operand, move it to a register.
    LoadTypeofExpression(operation->expression());
    Register tos = frame_->PopToRegister();

    Register scratch = VirtualFrame::scratch0();

    if (check->Equals(Heap::number_symbol())) {
      __ tst(tos, Operand(kSmiTagMask));
      true_target()->Branch(eq);
      __ ldr(tos, FieldMemOperand(tos, HeapObject::kMapOffset));
      __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
      __ cmp(tos, ip);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::string_symbol())) {
      __ tst(tos, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      __ ldr(tos, FieldMemOperand(tos, HeapObject::kMapOffset));

      // It can be an undetectable string object.
      __ ldrb(scratch, FieldMemOperand(tos, Map::kBitFieldOffset));
      __ and_(scratch, scratch, Operand(1 << Map::kIsUndetectable));
      __ cmp(scratch, Operand(1 << Map::kIsUndetectable));
      false_target()->Branch(eq);

      __ ldrb(scratch, FieldMemOperand(tos, Map::kInstanceTypeOffset));
      __ cmp(scratch, Operand(FIRST_NONSTRING_TYPE));
      cc_reg_ = lt;

    } else if (check->Equals(Heap::boolean_symbol())) {
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(tos, ip);
      true_target()->Branch(eq);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ cmp(tos, ip);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::undefined_symbol())) {
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(tos, ip);
      true_target()->Branch(eq);

      __ tst(tos, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      // It can be an undetectable object.
      __ ldr(tos, FieldMemOperand(tos, HeapObject::kMapOffset));
      __ ldrb(scratch, FieldMemOperand(tos, Map::kBitFieldOffset));
      __ and_(scratch, scratch, Operand(1 << Map::kIsUndetectable));
      __ cmp(scratch, Operand(1 << Map::kIsUndetectable));

      cc_reg_ = eq;

    } else if (check->Equals(Heap::function_symbol())) {
      __ tst(tos, Operand(kSmiTagMask));
      false_target()->Branch(eq);
      Register map_reg = scratch;
      __ CompareObjectType(tos, map_reg, tos, JS_FUNCTION_TYPE);
      true_target()->Branch(eq);
      // Regular expressions are callable so typeof == 'function'.
      __ CompareInstanceType(map_reg, tos, JS_REGEXP_TYPE);
      cc_reg_ = eq;

    } else if (check->Equals(Heap::object_symbol())) {
      __ tst(tos, Operand(kSmiTagMask));
      false_target()->Branch(eq);

      __ LoadRoot(ip, Heap::kNullValueRootIndex);
      __ cmp(tos, ip);
      true_target()->Branch(eq);

      Register map_reg = scratch;
      __ CompareObjectType(tos, map_reg, tos, JS_REGEXP_TYPE);
      false_target()->Branch(eq);

      // It can be an undetectable object.
      __ ldrb(tos, FieldMemOperand(map_reg, Map::kBitFieldOffset));
      __ and_(tos, tos, Operand(1 << Map::kIsUndetectable));
      __ cmp(tos, Operand(1 << Map::kIsUndetectable));
      false_target()->Branch(eq);

      __ ldrb(tos, FieldMemOperand(map_reg, Map::kInstanceTypeOffset));
      __ cmp(tos, Operand(FIRST_JS_OBJECT_TYPE));
      false_target()->Branch(lt);
      __ cmp(tos, Operand(LAST_JS_OBJECT_TYPE));
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
      Load(left);
      Load(right);
      frame_->InvokeBuiltin(Builtins::IN, CALL_JS, 2);
      frame_->EmitPush(r0);
      break;
    }

    case Token::INSTANCEOF: {
      Load(left);
      Load(right);
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


class DeferredReferenceGetNamedValue: public DeferredCode {
 public:
  explicit DeferredReferenceGetNamedValue(Register receiver,
                                          Handle<String> name)
      : receiver_(receiver), name_(name) {
    set_comment("[ DeferredReferenceGetNamedValue");
  }

  virtual void Generate();

 private:
  Register receiver_;
  Handle<String> name_;
};


// Convention for this is that on entry the receiver is in a register that
// is not used by the stack.  On exit the answer is found in that same
// register and the stack has the same height.
void DeferredReferenceGetNamedValue::Generate() {
#ifdef DEBUG
  int expected_height = frame_state()->frame()->height();
#endif
  VirtualFrame copied_frame(*frame_state()->frame());
  copied_frame.SpillAll();

  Register scratch1 = VirtualFrame::scratch0();
  Register scratch2 = VirtualFrame::scratch1();
  ASSERT(!receiver_.is(scratch1) && !receiver_.is(scratch2));
  __ DecrementCounter(&Counters::named_load_inline, 1, scratch1, scratch2);
  __ IncrementCounter(&Counters::named_load_inline_miss, 1, scratch1, scratch2);

  // Ensure receiver in r0 and name in r2 to match load ic calling convention.
  __ Move(r0, receiver_);
  __ mov(r2, Operand(name_));

  // The rest of the instructions in the deferred code must be together.
  { Assembler::BlockConstPoolScope block_const_pool(masm_);
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // The call must be followed by a nop(1) instruction to indicate that the
    // in-object has been inlined.
    __ nop(PROPERTY_ACCESS_INLINED);

    // At this point the answer is in r0.  We move it to the expected register
    // if necessary.
    __ Move(receiver_, r0);

    // Now go back to the frame that we entered with.  This will not overwrite
    // the receiver register since that register was not in use when we came
    // in.  The instructions emitted by this merge are skipped over by the
    // inline load patching mechanism when looking for the branch instruction
    // that tells it where the code to patch is.
    copied_frame.MergeTo(frame_state()->frame());

    // Block the constant pool for one more instruction after leaving this
    // constant pool block scope to include the branch instruction ending the
    // deferred code.
    __ BlockConstPoolFor(1);
  }
  ASSERT_EQ(expected_height, frame_state()->frame()->height());
}


class DeferredReferenceGetKeyedValue: public DeferredCode {
 public:
  DeferredReferenceGetKeyedValue(Register key, Register receiver)
      : key_(key), receiver_(receiver) {
    set_comment("[ DeferredReferenceGetKeyedValue");
  }

  virtual void Generate();

 private:
  Register key_;
  Register receiver_;
};


// Takes key and register in r0 and r1 or vice versa.  Returns result
// in r0.
void DeferredReferenceGetKeyedValue::Generate() {
  ASSERT((key_.is(r0) && receiver_.is(r1)) ||
         (key_.is(r1) && receiver_.is(r0)));

  VirtualFrame copied_frame(*frame_state()->frame());
  copied_frame.SpillAll();

  Register scratch1 = VirtualFrame::scratch0();
  Register scratch2 = VirtualFrame::scratch1();
  __ DecrementCounter(&Counters::keyed_load_inline, 1, scratch1, scratch2);
  __ IncrementCounter(&Counters::keyed_load_inline_miss, 1, scratch1, scratch2);

  // Ensure key in r0 and receiver in r1 to match keyed load ic calling
  // convention.
  if (key_.is(r1)) {
    __ Swap(r0, r1, ip);
  }

  // The rest of the instructions in the deferred code must be together.
  { Assembler::BlockConstPoolScope block_const_pool(masm_);
    // Call keyed load IC. It has the arguments key and receiver in r0 and r1.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // The call must be followed by a nop instruction to indicate that the
    // keyed load has been inlined.
    __ nop(PROPERTY_ACCESS_INLINED);

    // Now go back to the frame that we entered with.  This will not overwrite
    // the receiver or key registers since they were not in use when we came
    // in.  The instructions emitted by this merge are skipped over by the
    // inline load patching mechanism when looking for the branch instruction
    // that tells it where the code to patch is.
    copied_frame.MergeTo(frame_state()->frame());

    // Block the constant pool for one more instruction after leaving this
    // constant pool block scope to include the branch instruction ending the
    // deferred code.
    __ BlockConstPoolFor(1);
  }
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

 private:
  Register value_;
  Register key_;
  Register receiver_;
};


void DeferredReferenceSetKeyedValue::Generate() {
  Register scratch1 = VirtualFrame::scratch0();
  Register scratch2 = VirtualFrame::scratch1();
  __ DecrementCounter(&Counters::keyed_store_inline, 1, scratch1, scratch2);
  __ IncrementCounter(
      &Counters::keyed_store_inline_miss, 1, scratch1, scratch2);

  // Ensure value in r0, key in r1 and receiver in r2 to match keyed store ic
  // calling convention.
  if (value_.is(r1)) {
    __ Swap(r0, r1, ip);
  }
  ASSERT(receiver_.is(r2));

  // The rest of the instructions in the deferred code must be together.
  { Assembler::BlockConstPoolScope block_const_pool(masm_);
    // Call keyed store IC. It has the arguments value, key and receiver in r0,
    // r1 and r2.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // The call must be followed by a nop instruction to indicate that the
    // keyed store has been inlined.
    __ nop(PROPERTY_ACCESS_INLINED);

    // Block the constant pool for one more instruction after leaving this
    // constant pool block scope to include the branch instruction ending the
    // deferred code.
    __ BlockConstPoolFor(1);
  }
}


class DeferredReferenceSetNamedValue: public DeferredCode {
 public:
  DeferredReferenceSetNamedValue(Register value,
                                 Register receiver,
                                 Handle<String> name)
      : value_(value), receiver_(receiver), name_(name) {
    set_comment("[ DeferredReferenceSetNamedValue");
  }

  virtual void Generate();

 private:
  Register value_;
  Register receiver_;
  Handle<String> name_;
};


// Takes value in r0, receiver in r1 and returns the result (the
// value) in r0.
void DeferredReferenceSetNamedValue::Generate() {
  // Record the entry frame and spill.
  VirtualFrame copied_frame(*frame_state()->frame());
  copied_frame.SpillAll();

  // Ensure value in r0, receiver in r1 to match store ic calling
  // convention.
  ASSERT(value_.is(r0) && receiver_.is(r1));
  __ mov(r2, Operand(name_));

  // The rest of the instructions in the deferred code must be together.
  { Assembler::BlockConstPoolScope block_const_pool(masm_);
    // Call keyed store IC. It has the arguments value, key and receiver in r0,
    // r1 and r2.
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // The call must be followed by a nop instruction to indicate that the
    // named store has been inlined.
    __ nop(PROPERTY_ACCESS_INLINED);

    // Go back to the frame we entered with. The instructions
    // generated by this merge are skipped over by the inline store
    // patching mechanism when looking for the branch instruction that
    // tells it where the code to patch is.
    copied_frame.MergeTo(frame_state()->frame());

    // Block the constant pool for one more instruction after leaving this
    // constant pool block scope to include the branch instruction ending the
    // deferred code.
    __ BlockConstPoolFor(1);
  }
}


// Consumes the top of stack (the receiver) and pushes the result instead.
void CodeGenerator::EmitNamedLoad(Handle<String> name, bool is_contextual) {
  if (is_contextual || scope()->is_global_scope() || loop_nesting() == 0) {
    Comment cmnt(masm(), "[ Load from named Property");
    // Setup the name register and call load IC.
    frame_->CallLoadIC(name,
                       is_contextual
                           ? RelocInfo::CODE_TARGET_CONTEXT
                           : RelocInfo::CODE_TARGET);
    frame_->EmitPush(r0);  // Push answer.
  } else {
    // Inline the in-object property case.
    Comment cmnt(masm(), "[ Inlined named property load");

    // Counter will be decremented in the deferred code. Placed here to avoid
    // having it in the instruction stream below where patching will occur.
    __ IncrementCounter(&Counters::named_load_inline, 1,
                        frame_->scratch0(), frame_->scratch1());

    // The following instructions are the inlined load of an in-object property.
    // Parts of this code is patched, so the exact instructions generated needs
    // to be fixed. Therefore the instruction pool is blocked when generating
    // this code

    // Load the receiver from the stack.
    Register receiver = frame_->PopToRegister();

    DeferredReferenceGetNamedValue* deferred =
        new DeferredReferenceGetNamedValue(receiver, name);

#ifdef DEBUG
    int kInlinedNamedLoadInstructions = 7;
    Label check_inlined_codesize;
    masm_->bind(&check_inlined_codesize);
#endif

    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      // Check that the receiver is a heap object.
      __ tst(receiver, Operand(kSmiTagMask));
      deferred->Branch(eq);

      Register scratch = VirtualFrame::scratch0();
      Register scratch2 = VirtualFrame::scratch1();

      // Check the map. The null map used below is patched by the inline cache
      // code.  Therefore we can't use a LoadRoot call.
      __ ldr(scratch, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ mov(scratch2, Operand(Factory::null_value()));
      __ cmp(scratch, scratch2);
      deferred->Branch(ne);

      // Initially use an invalid index. The index will be patched by the
      // inline cache code.
      __ ldr(receiver, MemOperand(receiver, 0));

      // Make sure that the expected number of instructions are generated.
      ASSERT_EQ(kInlinedNamedLoadInstructions,
                masm_->InstructionsGeneratedSince(&check_inlined_codesize));
    }

    deferred->BindExit();
    // At this point the receiver register has the result, either from the
    // deferred code or from the inlined code.
    frame_->EmitPush(receiver);
  }
}


void CodeGenerator::EmitNamedStore(Handle<String> name, bool is_contextual) {
#ifdef DEBUG
  int expected_height = frame()->height() - (is_contextual ? 1 : 2);
#endif

  Result result;
  if (is_contextual || scope()->is_global_scope() || loop_nesting() == 0) {
    frame()->CallStoreIC(name, is_contextual);
  } else {
    // Inline the in-object property case.
    JumpTarget slow, done;

    // Get the value and receiver from the stack.
    frame()->PopToR0();
    Register value = r0;
    frame()->PopToR1();
    Register receiver = r1;

    DeferredReferenceSetNamedValue* deferred =
        new DeferredReferenceSetNamedValue(value, receiver, name);

    // Check that the receiver is a heap object.
    __ tst(receiver, Operand(kSmiTagMask));
    deferred->Branch(eq);

    // The following instructions are the part of the inlined
    // in-object property store code which can be patched. Therefore
    // the exact number of instructions generated must be fixed, so
    // the constant pool is blocked while generating this code.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      Register scratch0 = VirtualFrame::scratch0();
      Register scratch1 = VirtualFrame::scratch1();

      // Check the map. Initially use an invalid map to force a
      // failure. The map check will be patched in the runtime system.
      __ ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));

#ifdef DEBUG
      Label check_inlined_codesize;
      masm_->bind(&check_inlined_codesize);
#endif
      __ mov(scratch0, Operand(Factory::null_value()));
      __ cmp(scratch0, scratch1);
      deferred->Branch(ne);

      int offset = 0;
      __ str(value, MemOperand(receiver, offset));

      // Update the write barrier and record its size. We do not use
      // the RecordWrite macro here because we want the offset
      // addition instruction first to make it easy to patch.
      Label record_write_start, record_write_done;
      __ bind(&record_write_start);
      // Add offset into the object.
      __ add(scratch0, receiver, Operand(offset));
      // Test that the object is not in the new space.  We cannot set
      // region marks for new space pages.
      __ InNewSpace(receiver, scratch1, eq, &record_write_done);
      // Record the actual write.
      __ RecordWriteHelper(receiver, scratch0, scratch1);
      __ bind(&record_write_done);
      // Clobber all input registers when running with the debug-code flag
      // turned on to provoke errors.
      if (FLAG_debug_code) {
        __ mov(receiver, Operand(BitCast<int32_t>(kZapValue)));
        __ mov(scratch0, Operand(BitCast<int32_t>(kZapValue)));
        __ mov(scratch1, Operand(BitCast<int32_t>(kZapValue)));
      }
      // Check that this is the first inlined write barrier or that
      // this inlined write barrier has the same size as all the other
      // inlined write barriers.
      ASSERT((inlined_write_barrier_size_ == -1) ||
             (inlined_write_barrier_size_ ==
              masm()->InstructionsGeneratedSince(&record_write_start)));
      inlined_write_barrier_size_ =
          masm()->InstructionsGeneratedSince(&record_write_start);

      // Make sure that the expected number of instructions are generated.
      ASSERT_EQ(GetInlinedNamedStoreInstructionsAfterPatch(),
                masm()->InstructionsGeneratedSince(&check_inlined_codesize));
    }
    deferred->BindExit();
  }
  ASSERT_EQ(expected_height, frame()->height());
}


void CodeGenerator::EmitKeyedLoad() {
  if (loop_nesting() == 0) {
    Comment cmnt(masm_, "[ Load from keyed property");
    frame_->CallKeyedLoadIC();
  } else {
    // Inline the keyed load.
    Comment cmnt(masm_, "[ Inlined load from keyed property");

    // Counter will be decremented in the deferred code. Placed here to avoid
    // having it in the instruction stream below where patching will occur.
    __ IncrementCounter(&Counters::keyed_load_inline, 1,
                        frame_->scratch0(), frame_->scratch1());

    // Load the key and receiver from the stack.
    bool key_is_known_smi = frame_->KnownSmiAt(0);
    Register key = frame_->PopToRegister();
    Register receiver = frame_->PopToRegister(key);

    // The deferred code expects key and receiver in registers.
    DeferredReferenceGetKeyedValue* deferred =
        new DeferredReferenceGetKeyedValue(key, receiver);

    // Check that the receiver is a heap object.
    __ tst(receiver, Operand(kSmiTagMask));
    deferred->Branch(eq);

    // The following instructions are the part of the inlined load keyed
    // property code which can be patched. Therefore the exact number of
    // instructions generated need to be fixed, so the constant pool is blocked
    // while generating this code.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      Register scratch1 = VirtualFrame::scratch0();
      Register scratch2 = VirtualFrame::scratch1();
      // Check the map. The null map used below is patched by the inline cache
      // code.
      __ ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));

      // Check that the key is a smi.
      if (!key_is_known_smi) {
        __ tst(key, Operand(kSmiTagMask));
        deferred->Branch(ne);
      }

#ifdef DEBUG
      Label check_inlined_codesize;
      masm_->bind(&check_inlined_codesize);
#endif
      __ mov(scratch2, Operand(Factory::null_value()));
      __ cmp(scratch1, scratch2);
      deferred->Branch(ne);

      // Get the elements array from the receiver and check that it
      // is not a dictionary.
      __ ldr(scratch1, FieldMemOperand(receiver, JSObject::kElementsOffset));
      if (FLAG_debug_code) {
        __ ldr(scratch2, FieldMemOperand(scratch1, JSObject::kMapOffset));
        __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
        __ cmp(scratch2, ip);
        __ Assert(eq, "JSObject with fast elements map has slow elements");
      }

      // Check that key is within bounds. Use unsigned comparison to handle
      // negative keys.
      __ ldr(scratch2, FieldMemOperand(scratch1, FixedArray::kLengthOffset));
      __ cmp(scratch2, key);
      deferred->Branch(ls);  // Unsigned less equal.

      // Load and check that the result is not the hole (key is a smi).
      __ LoadRoot(scratch2, Heap::kTheHoleValueRootIndex);
      __ add(scratch1,
             scratch1,
             Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ ldr(scratch1,
             MemOperand(scratch1, key, LSL,
                        kPointerSizeLog2 - (kSmiTagSize + kSmiShiftSize)));
      __ cmp(scratch1, scratch2);
      deferred->Branch(eq);

      __ mov(r0, scratch1);
      // Make sure that the expected number of instructions are generated.
      ASSERT_EQ(GetInlinedKeyedLoadInstructionsAfterPatch(),
                masm_->InstructionsGeneratedSince(&check_inlined_codesize));
    }

    deferred->BindExit();
  }
}


void CodeGenerator::EmitKeyedStore(StaticType* key_type,
                                   WriteBarrierCharacter wb_info) {
  // Generate inlined version of the keyed store if the code is in a loop
  // and the key is likely to be a smi.
  if (loop_nesting() > 0 && key_type->IsLikelySmi()) {
    // Inline the keyed store.
    Comment cmnt(masm_, "[ Inlined store to keyed property");

    Register scratch1 = VirtualFrame::scratch0();
    Register scratch2 = VirtualFrame::scratch1();
    Register scratch3 = r3;

    // Counter will be decremented in the deferred code. Placed here to avoid
    // having it in the instruction stream below where patching will occur.
    __ IncrementCounter(&Counters::keyed_store_inline, 1,
                        scratch1, scratch2);



    // Load the value, key and receiver from the stack.
    bool value_is_harmless = frame_->KnownSmiAt(0);
    if (wb_info == NEVER_NEWSPACE) value_is_harmless = true;
    bool key_is_smi = frame_->KnownSmiAt(1);
    Register value = frame_->PopToRegister();
    Register key = frame_->PopToRegister(value);
    VirtualFrame::SpilledScope spilled(frame_);
    Register receiver = r2;
    frame_->EmitPop(receiver);

#ifdef DEBUG
    bool we_remembered_the_write_barrier = value_is_harmless;
#endif

    // The deferred code expects value, key and receiver in registers.
    DeferredReferenceSetKeyedValue* deferred =
        new DeferredReferenceSetKeyedValue(value, key, receiver);

    // Check that the value is a smi. As this inlined code does not set the
    // write barrier it is only possible to store smi values.
    if (!value_is_harmless) {
      // If the value is not likely to be a Smi then let's test the fixed array
      // for new space instead.  See below.
      if (wb_info == LIKELY_SMI) {
        __ tst(value, Operand(kSmiTagMask));
        deferred->Branch(ne);
#ifdef DEBUG
        we_remembered_the_write_barrier = true;
#endif
      }
    }

    if (!key_is_smi) {
      // Check that the key is a smi.
      __ tst(key, Operand(kSmiTagMask));
      deferred->Branch(ne);
    }

    // Check that the receiver is a heap object.
    __ tst(receiver, Operand(kSmiTagMask));
    deferred->Branch(eq);

    // Check that the receiver is a JSArray.
    __ CompareObjectType(receiver, scratch1, scratch1, JS_ARRAY_TYPE);
    deferred->Branch(ne);

    // Check that the key is within bounds. Both the key and the length of
    // the JSArray are smis. Use unsigned comparison to handle negative keys.
    __ ldr(scratch1, FieldMemOperand(receiver, JSArray::kLengthOffset));
    __ cmp(scratch1, key);
    deferred->Branch(ls);  // Unsigned less equal.

    // Get the elements array from the receiver.
    __ ldr(scratch1, FieldMemOperand(receiver, JSObject::kElementsOffset));
    if (!value_is_harmless && wb_info != LIKELY_SMI) {
      Label ok;
      __ and_(scratch2, scratch1, Operand(ExternalReference::new_space_mask()));
      __ cmp(scratch2, Operand(ExternalReference::new_space_start()));
      __ tst(value, Operand(kSmiTagMask), ne);
      deferred->Branch(ne);
#ifdef DEBUG
      we_remembered_the_write_barrier = true;
#endif
    }
    // Check that the elements array is not a dictionary.
    __ ldr(scratch2, FieldMemOperand(scratch1, JSObject::kMapOffset));
    // The following instructions are the part of the inlined store keyed
    // property code which can be patched. Therefore the exact number of
    // instructions generated need to be fixed, so the constant pool is blocked
    // while generating this code.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
#ifdef DEBUG
      Label check_inlined_codesize;
      masm_->bind(&check_inlined_codesize);
#endif

      // Read the fixed array map from the constant pool (not from the root
      // array) so that the value can be patched.  When debugging, we patch this
      // comparison to always fail so that we will hit the IC call in the
      // deferred code which will allow the debugger to break for fast case
      // stores.
      __ mov(scratch3, Operand(Factory::fixed_array_map()));
      __ cmp(scratch2, scratch3);
      deferred->Branch(ne);

      // Store the value.
      __ add(scratch1, scratch1,
             Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ str(value,
             MemOperand(scratch1, key, LSL,
                        kPointerSizeLog2 - (kSmiTagSize + kSmiShiftSize)));

      // Make sure that the expected number of instructions are generated.
      ASSERT_EQ(kInlinedKeyedStoreInstructionsAfterPatch,
                masm_->InstructionsGeneratedSince(&check_inlined_codesize));
    }

    ASSERT(we_remembered_the_write_barrier);

    deferred->BindExit();
  } else {
    frame()->CallKeyedStoreIC();
  }
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


void Reference::DupIfPersist() {
  if (persist_after_get_) {
    switch (type_) {
      case KEYED:
        cgen_->frame()->Dup2();
        break;
      case NAMED:
        cgen_->frame()->Dup();
        // Fall through.
      case UNLOADED:
      case ILLEGAL:
      case SLOT:
        // Do nothing.
        ;
    }
  } else {
    set_unloaded();
  }
}


void Reference::GetValue() {
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
      DupIfPersist();
      cgen_->LoadFromSlotCheckForArguments(slot, NOT_INSIDE_TYPEOF);
      break;
    }

    case NAMED: {
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());
      Handle<String> name = GetName();
      DupIfPersist();
      cgen_->EmitNamedLoad(name, is_global);
      break;
    }

    case KEYED: {
      ASSERT(property != NULL);
      DupIfPersist();
      cgen_->EmitKeyedLoad();
      cgen_->frame()->EmitPush(r0);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void Reference::SetValue(InitState init_state, WriteBarrierCharacter wb_info) {
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
      cgen_->StoreToSlot(slot, init_state);
      set_unloaded();
      break;
    }

    case NAMED: {
      Comment cmnt(masm, "[ Store to named Property");
      cgen_->EmitNamedStore(GetName(), false);
      frame->EmitPush(r0);
      set_unloaded();
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Store to keyed Property");
      Property* property = expression_->AsProperty();
      ASSERT(property != NULL);
      cgen_->CodeForSourcePosition(property->position());
      cgen_->EmitKeyedStore(property->key()->type(), wb_info);
      frame->EmitPush(r0);
      set_unloaded();
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in cp.
  Label gc;

  // Pop the function info from the stack.
  __ pop(r3);

  // Attempt to allocate new JSFunction in new space.
  __ AllocateInNewSpace(JSFunction::kSize,
                        r0,
                        r1,
                        r2,
                        &gc,
                        TAG_OBJECT);

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ ldr(r2, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalContextOffset));
  __ ldr(r2, MemOperand(r2, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
  __ LoadRoot(r2, Heap::kTheHoleValueRootIndex);
  __ str(r1, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r1, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ str(r2, FieldMemOperand(r0, JSFunction::kPrototypeOrInitialMapOffset));
  __ str(r3, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  __ str(cp, FieldMemOperand(r0, JSFunction::kContextOffset));
  __ str(r1, FieldMemOperand(r0, JSFunction::kLiteralsOffset));

  // Return result. The argument function info has been popped already.
  __ Ret();

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ Push(cp, r3);
  __ TailCallRuntime(Runtime::kNewClosure, 2, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;

  // Attempt to allocate the context in new space.
  __ AllocateInNewSpace(FixedArray::SizeFor(length),
                        r0,
                        r1,
                        r2,
                        &gc,
                        TAG_OBJECT);

  // Load the function from the stack.
  __ ldr(r3, MemOperand(sp, 0));

  // Setup the object header.
  __ LoadRoot(r2, Heap::kContextMapRootIndex);
  __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ mov(r2, Operand(Smi::FromInt(length)));
  __ str(r2, FieldMemOperand(r0, FixedArray::kLengthOffset));

  // Setup the fixed slots.
  __ mov(r1, Operand(Smi::FromInt(0)));
  __ str(r3, MemOperand(r0, Context::SlotOffset(Context::CLOSURE_INDEX)));
  __ str(r0, MemOperand(r0, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::PREVIOUS_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::EXTENSION_INDEX)));

  // Copy the global object from the surrounding context.
  __ ldr(r1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::GLOBAL_INDEX)));

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ str(r1, MemOperand(r0, Context::SlotOffset(i)));
  }

  // Remove the on-stack argument and return.
  __ mov(cp, r0);
  __ pop();
  __ Ret();

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewContext, 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [sp]: constant elements.
  // [sp + kPointerSize]: literal index.
  // [sp + (2 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into r3 and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ ldr(r3, MemOperand(sp, 2 * kPointerSize));
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));
  __ add(r3, r3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r3, MemOperand(r3, r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r3, ip);
  __ b(eq, &slow_case);

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size,
                        r0,
                        r1,
                        r2,
                        &slow_case,
                        TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ ldr(r1, FieldMemOperand(r3, i));
      __ str(r1, FieldMemOperand(r0, i));
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ ldr(r3, FieldMemOperand(r3, JSArray::kElementsOffset));
    __ add(r2, r0, Operand(JSArray::kSize));
    __ str(r2, FieldMemOperand(r0, JSArray::kElementsOffset));

    // Copy the elements array.
    for (int i = 0; i < elements_size; i += kPointerSize) {
      __ ldr(r1, FieldMemOperand(r3, i));
      __ str(r1, FieldMemOperand(r2, i));
    }
  }

  // Return and remove the on-stack parameters.
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateArrayLiteralShallow, 3, 1);
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
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ and_(exponent, source_, Operand(HeapNumber::kSignMask), SetCC);
  // Subtract from 0 if source was negative.
  __ rsb(source_, source_, Operand(0), LeaveCC, ne);

  // We have -1, 0 or 1, which we treat specially. Register source_ contains
  // absolute value: it is either equal to 1 (special case of -1 and 1),
  // greater than 1 (not a special case) or less than 1 (special case of 0).
  __ cmp(source_, Operand(1));
  __ b(gt, &not_special);

  // For 1 or -1 we need to or in the 0 exponent (biased to 1023).
  static const uint32_t exponent_word_for_1 =
      HeapNumber::kExponentBias << HeapNumber::kExponentShift;
  __ orr(exponent, exponent, Operand(exponent_word_for_1), LeaveCC, eq);
  // 1, 0 and -1 all have 0 for the second word.
  __ mov(mantissa, Operand(0));
  __ Ret();

  __ bind(&not_special);
  // Count leading zeros.  Uses mantissa for a scratch register on pre-ARM5.
  // Gets the wrong answer for 0, but we already checked for that case above.
  __ CountLeadingZeros(zeros_, source_, mantissa);
  // Compute exponent and or it into the exponent register.
  // We use mantissa as a scratch register here.  Use a fudge factor to
  // divide the constant 31 + HeapNumber::kExponentBias, 0x41d, into two parts
  // that fit in the ARM's constant field.
  int fudge = 0x400;
  __ rsb(mantissa, zeros_, Operand(31 + HeapNumber::kExponentBias - fudge));
  __ add(mantissa, mantissa, Operand(fudge));
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


// See comment for class.
void WriteInt32ToHeapNumberStub::Generate(MacroAssembler* masm) {
  Label max_negative_int;
  // the_int_ has the answer which is a signed int32 but not a Smi.
  // We test for the special value that has a different exponent.  This test
  // has the neat side effect of setting the flags according to the sign.
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
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
                                          Condition cc,
                                          bool never_nan_nan) {
  Label not_identical;
  Label heap_number, return_equal;
  __ cmp(r0, r1);
  __ b(ne, &not_identical);

  // The two objects are identical.  If we know that one of them isn't NaN then
  // we now know they test equal.
  if (cc != eq || !never_nan_nan) {
    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
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
        // Normally here we fall through to return_equal, but undefined is
        // special: (undefined == undefined) == true, but
        // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
        if (cc == le || cc == ge) {
          __ cmp(r4, Operand(ODDBALL_TYPE));
          __ b(ne, &return_equal);
          __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
          __ cmp(r0, r2);
          __ b(ne, &return_equal);
          if (cc == le) {
            // undefined <= undefined should fail.
            __ mov(r0, Operand(GREATER));
          } else  {
            // undefined >= undefined should fail.
            __ mov(r0, Operand(LESS));
          }
          __ Ret();
        }
      }
    }
  }

  __ bind(&return_equal);
  if (cc == lt) {
    __ mov(r0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cc == gt) {
    __ mov(r0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(r0, Operand(EQUAL));    // Things are <=, >=, ==, === themselves.
  }
  __ Ret();

  if (cc != eq || !never_nan_nan) {
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
      __ Sbfx(r3, r2, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
      // NaNs have all-one exponents so they sign extend to -1.
      __ cmp(r3, Operand(-1));
      __ b(ne, &return_equal);

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ mov(r2, Operand(r2, LSL, HeapNumber::kNonMantissaBitsInTopWord));
      // Or with all low-bits of mantissa.
      __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
      __ orr(r0, r3, Operand(r2), SetCC);
      // For equal we already have the right value in r0:  Return zero (equal)
      // if all bits in mantissa are zero (it's an Infinity) and non-zero if
      // not (it's a NaN).  For <= and >= we need to load r0 with the failing
      // value if it's a NaN.
      if (cc != eq) {
        // All-zero means Infinity means equal.
        __ Ret(eq);
        if (cc == le) {
          __ mov(r0, Operand(GREATER));  // NaN <= NaN should fail.
        } else {
          __ mov(r0, Operand(LESS));     // NaN >= NaN should fail.
        }
      }
      __ Ret();
    }
    // No fall through here.
  }

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  Label rhs_is_smi;
  __ tst(rhs, Operand(kSmiTagMask));
  __ b(eq, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r0 then there is already a non zero value in it.
    if (!rhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Lhs is a smi, rhs is a number.
  if (CpuFeatures::IsSupported(VFP3)) {
    // Convert lhs to a double in d7.
    CpuFeatures::Scope scope(VFP3);
    __ SmiToDoubleVFPRegister(lhs, d7, r7, s15);
    // Load the double from rhs, tagged HeapNumber r0, to d6.
    __ sub(r7, rhs, Operand(kHeapObjectTag));
    __ vldr(d6, r7, HeapNumber::kValueOffset);
  } else {
    __ push(lr);
    // Convert lhs to a double in r2, r3.
    __ mov(r7, Operand(lhs));
    ConvertToDoubleStub stub1(r3, r2, r7, r6);
    __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
    // Load rhs to a double in r0, r1.
    __ Ldrd(r0, r1, FieldMemOperand(rhs, HeapNumber::kValueOffset));
    __ pop(lr);
  }

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ jmp(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r0 then there is already a non zero value in it.
    if (!lhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Rhs is a smi, lhs is a heap number.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    // Load the double from lhs, tagged HeapNumber r1, to d7.
    __ sub(r7, lhs, Operand(kHeapObjectTag));
    __ vldr(d7, r7, HeapNumber::kValueOffset);
    // Convert rhs to a double in d6              .
    __ SmiToDoubleVFPRegister(rhs, d6, r7, s13);
  } else {
    __ push(lr);
    // Load lhs to a double in r2, r3.
    __ Ldrd(r2, r3, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    // Convert rhs to a double in r0, r1.
    __ mov(r7, Operand(rhs));
    ConvertToDoubleStub stub2(r1, r0, r7, r6);
    __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
    __ pop(lr);
  }
  // Fall through to both_loaded_as_doubles.
}


void EmitNanCheck(MacroAssembler* masm, Label* lhs_not_nan, Condition cc) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register rhs_exponent = exp_first ? r0 : r1;
  Register lhs_exponent = exp_first ? r2 : r3;
  Register rhs_mantissa = exp_first ? r1 : r0;
  Register lhs_mantissa = exp_first ? r3 : r2;
  Label one_is_nan, neither_is_nan;

  __ Sbfx(r4,
          lhs_exponent,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // NaNs have all-one exponents so they sign extend to -1.
  __ cmp(r4, Operand(-1));
  __ b(ne, lhs_not_nan);
  __ mov(r4,
         Operand(lhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(lhs_mantissa, Operand(0));
  __ b(ne, &one_is_nan);

  __ bind(lhs_not_nan);
  __ Sbfx(r4,
          rhs_exponent,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // NaNs have all-one exponents so they sign extend to -1.
  __ cmp(r4, Operand(-1));
  __ b(ne, &neither_is_nan);
  __ mov(r4,
         Operand(rhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(rhs_mantissa, Operand(0));
  __ b(eq, &neither_is_nan);

  __ bind(&one_is_nan);
  // NaN comparisons always fail.
  // Load whatever we need in r0 to make the comparison fail.
  if (cc == lt || cc == le) {
    __ mov(r0, Operand(GREATER));
  } else {
    __ mov(r0, Operand(LESS));
  }
  __ Ret();

  __ bind(&neither_is_nan);
}


// See comment at call site.
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register rhs_exponent = exp_first ? r0 : r1;
  Register lhs_exponent = exp_first ? r2 : r3;
  Register rhs_mantissa = exp_first ? r1 : r0;
  Register lhs_mantissa = exp_first ? r3 : r2;

  // r0, r1, r2, r3 have the two doubles.  Neither is a NaN.
  if (cc == eq) {
    // Doubles are not equal unless they have the same bit pattern.
    // Exception: 0 and -0.
    __ cmp(rhs_mantissa, Operand(lhs_mantissa));
    __ orr(r0, rhs_mantissa, Operand(lhs_mantissa), LeaveCC, ne);
    // Return non-zero if the numbers are unequal.
    __ Ret(ne);

    __ sub(r0, rhs_exponent, Operand(lhs_exponent), SetCC);
    // If exponents are equal then return 0.
    __ Ret(eq);

    // Exponents are unequal.  The only way we can return that the numbers
    // are equal is if one is -0 and the other is 0.  We already dealt
    // with the case where both are -0 or both are 0.
    // We start by seeing if the mantissas (that are equal) or the bottom
    // 31 bits of the rhs exponent are non-zero.  If so we return not
    // equal.
    __ orr(r4, lhs_mantissa, Operand(lhs_exponent, LSL, kSmiTagSize), SetCC);
    __ mov(r0, Operand(r4), LeaveCC, ne);
    __ Ret(ne);
    // Now they are equal if and only if the lhs exponent is zero in its
    // low 31 bits.
    __ mov(r0, Operand(rhs_exponent, LSL, kSmiTagSize));
    __ Ret();
  } else {
    // Call a native function to do a comparison between two non-NaNs.
    // Call C routine that may not cause GC or other trouble.
    __ push(lr);
    __ PrepareCallCFunction(4, r5);  // Two doubles count as 4 arguments.
    __ CallCFunction(ExternalReference::compare_doubles(), 4);
    __ pop(pc);  // Return.
  }
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    ASSERT((lhs.is(r0) && rhs.is(r1)) ||
           (lhs.is(r1) && rhs.is(r0)));

    // If either operand is a JSObject or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
    Label first_non_object;
    // Get the type of the first operand into r2 and compare it with
    // FIRST_JS_OBJECT_TYPE.
    __ CompareObjectType(rhs, r2, r2, FIRST_JS_OBJECT_TYPE);
    __ b(lt, &first_non_object);

    // Return non-zero (r0 is not zero)
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ Ret();

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ cmp(r2, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    __ CompareObjectType(lhs, r3, r3, FIRST_JS_OBJECT_TYPE);
    __ b(ge, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ cmp(r3, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    // Now that we have the types we might as well check for symbol-symbol.
    // Ensure that no non-strings have the symbol bit set.
    STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
    STATIC_ASSERT(kSymbolTag != 0);
    __ and_(r2, r2, Operand(r3));
    __ tst(r2, Operand(kIsSymbolMask));
    __ b(ne, &return_not_equal);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  __ CompareObjectType(rhs, r3, r2, HEAP_NUMBER_TYPE);
  __ b(ne, not_heap_numbers);
  __ ldr(r2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ cmp(r2, r3);
  __ b(ne, slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    __ sub(r7, rhs, Operand(kHeapObjectTag));
    __ vldr(d6, r7, HeapNumber::kValueOffset);
    __ sub(r7, lhs, Operand(kHeapObjectTag));
    __ vldr(d7, r7, HeapNumber::kValueOffset);
  } else {
    __ Ldrd(r2, r3, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    __ Ldrd(r0, r1, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  }
  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for symbol-to-symbol equality.
static void EmitCheckForSymbolsOrObjects(MacroAssembler* masm,
                                         Register lhs,
                                         Register rhs,
                                         Label* possible_strings,
                                         Label* not_both_strings) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  // r2 is object type of rhs.
  // Ensure that no non-strings have the symbol bit set.
  Label object_test;
  STATIC_ASSERT(kSymbolTag != 0);
  __ tst(r2, Operand(kIsNotStringMask));
  __ b(ne, &object_test);
  __ tst(r2, Operand(kIsSymbolMask));
  __ b(eq, possible_strings);
  __ CompareObjectType(lhs, r3, r3, FIRST_NONSTRING_TYPE);
  __ b(ge, not_both_strings);
  __ tst(r3, Operand(kIsSymbolMask));
  __ b(eq, possible_strings);

  // Both are symbols.  We already checked they weren't the same pointer
  // so they are not equal.
  __ mov(r0, Operand(NOT_EQUAL));
  __ Ret();

  __ bind(&object_test);
  __ cmp(r2, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, not_both_strings);
  __ CompareObjectType(lhs, r2, r3, FIRST_JS_OBJECT_TYPE);
  __ b(lt, not_both_strings);
  // If both objects are undetectable, they are equal. Otherwise, they
  // are not equal, since they are different objects and an object is not
  // equal to undefined.
  __ ldr(r3, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ ldrb(r2, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ ldrb(r3, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ and_(r0, r2, Operand(r3));
  __ and_(r0, r0, Operand(1 << Map::kIsUndetectable));
  __ eor(r0, r0, Operand(1 << Map::kIsUndetectable));
  __ Ret();
}


void NumberToStringStub::GenerateLookupNumberStringCache(MacroAssembler* masm,
                                                         Register object,
                                                         Register result,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Register scratch3,
                                                         bool object_is_smi,
                                                         Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  __ LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ ldr(mask, FieldMemOperand(number_string_cache, FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  __ mov(mask, Operand(mask, ASR, kSmiTagSize + 1));
  __ sub(mask, mask, Operand(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  if (!object_is_smi) {
    __ BranchOnSmi(object, &is_smi);
    if (CpuFeatures::IsSupported(VFP3)) {
      CpuFeatures::Scope scope(VFP3);
      __ CheckMap(object,
                  scratch1,
                  Heap::kHeapNumberMapRootIndex,
                  not_found,
                  true);

      STATIC_ASSERT(8 == kDoubleSize);
      __ add(scratch1,
             object,
             Operand(HeapNumber::kValueOffset - kHeapObjectTag));
      __ ldm(ia, scratch1, scratch1.bit() | scratch2.bit());
      __ eor(scratch1, scratch1, Operand(scratch2));
      __ and_(scratch1, scratch1, Operand(mask));

      // Calculate address of entry in string cache: each entry consists
      // of two pointer sized fields.
      __ add(scratch1,
             number_string_cache,
             Operand(scratch1, LSL, kPointerSizeLog2 + 1));

      Register probe = mask;
      __ ldr(probe,
             FieldMemOperand(scratch1, FixedArray::kHeaderSize));
      __ BranchOnSmi(probe, not_found);
      __ sub(scratch2, object, Operand(kHeapObjectTag));
      __ vldr(d0, scratch2, HeapNumber::kValueOffset);
      __ sub(probe, probe, Operand(kHeapObjectTag));
      __ vldr(d1, probe, HeapNumber::kValueOffset);
      __ vcmp(d0, d1);
      __ vmrs(pc);
      __ b(ne, not_found);  // The cache did not contain this value.
      __ b(&load_result_from_cache);
    } else {
      __ b(not_found);
    }
  }

  __ bind(&is_smi);
  Register scratch = scratch1;
  __ and_(scratch, mask, Operand(object, ASR, 1));
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  __ add(scratch,
         number_string_cache,
         Operand(scratch, LSL, kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  Register probe = mask;
  __ ldr(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  __ cmp(object, probe);
  __ b(ne, not_found);

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ ldr(result,
         FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  __ IncrementCounter(&Counters::number_to_string_native,
                      1,
                      scratch1,
                      scratch2);
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ ldr(r1, MemOperand(sp, 0));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, r1, r0, r2, r3, r4, false, &runtime);
  __ add(sp, sp, Operand(1 * kPointerSize));
  __ Ret();

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(Runtime::kNumberToStringSkipCache, 1, 1);
}


void RecordWriteStub::Generate(MacroAssembler* masm) {
  __ add(offset_, object_, Operand(offset_));
  __ RecordWriteHelper(object_, offset_, scratch_);
  __ Ret();
}


// On entry lhs_ and rhs_ are the values to be compared.
// On exit r0 is 0, positive or negative to indicate the result of
// the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc_, never_nan_nan_);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(0, Smi::FromInt(0));
  __ and_(r2, lhs_, Operand(rhs_));
  __ tst(r2, Operand(kSmiTagMask));
  __ b(ne, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to lhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison.  If VFP3 is supported the double values of the numbers have
  // been loaded into d7 and d6.  Otherwise, the double values have been loaded
  // into r0, r1, r2, and r3.
  EmitSmiNonsmiComparison(masm, lhs_, rhs_, &lhs_not_nan, &slow, strict_);

  __ bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in d6 and d7, if
  // VFP3 is supported, or in r0, r1, r2, and r3.
  if (CpuFeatures::IsSupported(VFP3)) {
    __ bind(&lhs_not_nan);
    CpuFeatures::Scope scope(VFP3);
    Label no_nan;
    // ARMv7 VFP3 instructions to implement double precision comparison.
    __ vcmp(d7, d6);
    __ vmrs(pc);  // Move vector status bits to normal status bits.
    Label nan;
    __ b(vs, &nan);
    __ mov(r0, Operand(EQUAL), LeaveCC, eq);
    __ mov(r0, Operand(LESS), LeaveCC, lt);
    __ mov(r0, Operand(GREATER), LeaveCC, gt);
    __ Ret();

    __ bind(&nan);
    // If one of the sides was a NaN then the v flag is set.  Load r0 with
    // whatever it takes to make the comparison fail, since comparisons with NaN
    // always fail.
    if (cc_ == lt || cc_ == le) {
      __ mov(r0, Operand(GREATER));
    } else {
      __ mov(r0, Operand(LESS));
    }
    __ Ret();
  } else {
    // Checks for NaN in the doubles we have loaded.  Can return the answer or
    // fall through if neither is a NaN.  Also binds lhs_not_nan.
    EmitNanCheck(masm, &lhs_not_nan, cc_);
    // Compares two doubles in r0, r1, r2, r3 that are not NaNs.  Returns the
    // answer.  Never falls through.
    EmitTwoNonNanDoubleComparison(masm, cc_);
  }

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi.  The objects are in rhs_ and lhs_.
  if (strict_) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs_, rhs_);
  }

  Label check_for_symbols;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison.  Can jump to slow case,
  // or load both doubles into r0, r1, r2, r3 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to check_for_symbols.
  // In this case r2 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs_,
                             rhs_,
                             &both_loaded_as_doubles,
                             &check_for_symbols,
                             &flat_string_check);

  __ bind(&check_for_symbols);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // symbols.
  if (cc_ == eq && !strict_) {
    // Returns an answer for two symbols or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r2 is the type of rhs_ on entry.
    EmitCheckForSymbolsOrObjects(masm, lhs_, rhs_, &flat_string_check, &slow);
  }

  // Check for both being sequential ASCII strings, and inline if that is the
  // case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialAsciiStrings(lhs_, rhs_, r2, r3, &slow);

  __ IncrementCounter(&Counters::string_compare_native, 1, r2, r3);
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     lhs_,
                                                     rhs_,
                                                     r2,
                                                     r3,
                                                     r4,
                                                     r5);
  // Never falls through to here.

  __ bind(&slow);

  __ Push(lhs_, rhs_);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
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
    __ mov(r0, Operand(Smi::FromInt(ncr)));
    __ push(r0);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_JS);
}


// We fall into this code if the operands were Smis, but the result was
// not (eg. overflow).  We branch into this code (to the not_smi label) if
// the operands were not both Smi.  The operands are in r0 and r1.  In order
// to call the C-implemented binary fp operation routines we need to end up
// with the double precision floating point operands in r0 and r1 (for the
// value in r1) and r2 and r3 (for the value in r0).
void GenericBinaryOpStub::HandleBinaryOpSlowCases(
    MacroAssembler* masm,
    Label* not_smi,
    Register lhs,
    Register rhs,
    const Builtins::JavaScript& builtin) {
  Label slow, slow_reverse, do_the_call;
  bool use_fp_registers = CpuFeatures::IsSupported(VFP3) && Token::MOD != op_;

  ASSERT((lhs.is(r0) && rhs.is(r1)) || (lhs.is(r1) && rhs.is(r0)));
  Register heap_number_map = r6;

  if (ShouldGenerateSmiCode()) {
    __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

    // Smi-smi case (overflow).
    // Since both are Smis there is no heap number to overwrite, so allocate.
    // The new heap number is in r5.  r3 and r7 are scratch.
    __ AllocateHeapNumber(
        r5, r3, r7, heap_number_map, lhs.is(r0) ? &slow_reverse : &slow);

    // If we have floating point hardware, inline ADD, SUB, MUL, and DIV,
    // using registers d7 and d6 for the double values.
    if (CpuFeatures::IsSupported(VFP3)) {
      CpuFeatures::Scope scope(VFP3);
      __ mov(r7, Operand(rhs, ASR, kSmiTagSize));
      __ vmov(s15, r7);
      __ vcvt_f64_s32(d7, s15);
      __ mov(r7, Operand(lhs, ASR, kSmiTagSize));
      __ vmov(s13, r7);
      __ vcvt_f64_s32(d6, s13);
      if (!use_fp_registers) {
        __ vmov(r2, r3, d7);
        __ vmov(r0, r1, d6);
      }
    } else {
      // Write Smi from rhs to r3 and r2 in double format.  r9 is scratch.
      __ mov(r7, Operand(rhs));
      ConvertToDoubleStub stub1(r3, r2, r7, r9);
      __ push(lr);
      __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
      // Write Smi from lhs to r1 and r0 in double format.  r9 is scratch.
      __ mov(r7, Operand(lhs));
      ConvertToDoubleStub stub2(r1, r0, r7, r9);
      __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
      __ pop(lr);
    }
    __ jmp(&do_the_call);  // Tail call.  No return.
  }

  // We branch here if at least one of r0 and r1 is not a Smi.
  __ bind(not_smi);
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  // After this point we have the left hand side in r1 and the right hand side
  // in r0.
  if (lhs.is(r0)) {
    __ Swap(r0, r1, ip);
  }

  // The type transition also calculates the answer.
  bool generate_code_to_calculate_answer = true;

  if (ShouldGenerateFPCode()) {
    if (runtime_operands_type_ == BinaryOpIC::DEFAULT) {
      switch (op_) {
        case Token::ADD:
        case Token::SUB:
        case Token::MUL:
        case Token::DIV:
          GenerateTypeTransition(masm);  // Tail call.
          generate_code_to_calculate_answer = false;
          break;

        default:
          break;
      }
    }

    if (generate_code_to_calculate_answer) {
      Label r0_is_smi, r1_is_smi, finished_loading_r0, finished_loading_r1;
      if (mode_ == NO_OVERWRITE) {
        // In the case where there is no chance of an overwritable float we may
        // as well do the allocation immediately while r0 and r1 are untouched.
        __ AllocateHeapNumber(r5, r3, r7, heap_number_map, &slow);
      }

      // Move r0 to a double in r2-r3.
      __ tst(r0, Operand(kSmiTagMask));
      __ b(eq, &r0_is_smi);  // It's a Smi so don't check it's a heap number.
      __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
      __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      __ cmp(r4, heap_number_map);
      __ b(ne, &slow);
      if (mode_ == OVERWRITE_RIGHT) {
        __ mov(r5, Operand(r0));  // Overwrite this heap number.
      }
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // Load the double from tagged HeapNumber r0 to d7.
        __ sub(r7, r0, Operand(kHeapObjectTag));
        __ vldr(d7, r7, HeapNumber::kValueOffset);
      } else {
        // Calling convention says that second double is in r2 and r3.
        __ Ldrd(r2, r3, FieldMemOperand(r0, HeapNumber::kValueOffset));
      }
      __ jmp(&finished_loading_r0);
      __ bind(&r0_is_smi);
      if (mode_ == OVERWRITE_RIGHT) {
        // We can't overwrite a Smi so get address of new heap number into r5.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
      }

      if (CpuFeatures::IsSupported(VFP3)) {
        CpuFeatures::Scope scope(VFP3);
        // Convert smi in r0 to double in d7.
        __ mov(r7, Operand(r0, ASR, kSmiTagSize));
        __ vmov(s15, r7);
        __ vcvt_f64_s32(d7, s15);
        if (!use_fp_registers) {
          __ vmov(r2, r3, d7);
        }
      } else {
        // Write Smi from r0 to r3 and r2 in double format.
        __ mov(r7, Operand(r0));
        ConvertToDoubleStub stub3(r3, r2, r7, r4);
        __ push(lr);
        __ Call(stub3.GetCode(), RelocInfo::CODE_TARGET);
        __ pop(lr);
      }

      // HEAP_NUMBERS stub is slower than GENERIC on a pair of smis.
      // r0 is known to be a smi. If r1 is also a smi then switch to GENERIC.
      Label r1_is_not_smi;
      if (runtime_operands_type_ == BinaryOpIC::HEAP_NUMBERS) {
        __ tst(r1, Operand(kSmiTagMask));
        __ b(ne, &r1_is_not_smi);
        GenerateTypeTransition(masm);  // Tail call.
      }

      __ bind(&finished_loading_r0);

      // Move r1 to a double in r0-r1.
      __ tst(r1, Operand(kSmiTagMask));
      __ b(eq, &r1_is_smi);  // It's a Smi so don't check it's a heap number.
      __ bind(&r1_is_not_smi);
      __ ldr(r4, FieldMemOperand(r1, HeapNumber::kMapOffset));
      __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      __ cmp(r4, heap_number_map);
      __ b(ne, &slow);
      if (mode_ == OVERWRITE_LEFT) {
        __ mov(r5, Operand(r1));  // Overwrite this heap number.
      }
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // Load the double from tagged HeapNumber r1 to d6.
        __ sub(r7, r1, Operand(kHeapObjectTag));
        __ vldr(d6, r7, HeapNumber::kValueOffset);
      } else {
        // Calling convention says that first double is in r0 and r1.
        __ Ldrd(r0, r1, FieldMemOperand(r1, HeapNumber::kValueOffset));
      }
      __ jmp(&finished_loading_r1);
      __ bind(&r1_is_smi);
      if (mode_ == OVERWRITE_LEFT) {
        // We can't overwrite a Smi so get address of new heap number into r5.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
      }

      if (CpuFeatures::IsSupported(VFP3)) {
        CpuFeatures::Scope scope(VFP3);
        // Convert smi in r1 to double in d6.
        __ mov(r7, Operand(r1, ASR, kSmiTagSize));
        __ vmov(s13, r7);
        __ vcvt_f64_s32(d6, s13);
        if (!use_fp_registers) {
          __ vmov(r0, r1, d6);
        }
      } else {
        // Write Smi from r1 to r1 and r0 in double format.
        __ mov(r7, Operand(r1));
        ConvertToDoubleStub stub4(r1, r0, r7, r9);
        __ push(lr);
        __ Call(stub4.GetCode(), RelocInfo::CODE_TARGET);
        __ pop(lr);
      }

      __ bind(&finished_loading_r1);
    }

    if (generate_code_to_calculate_answer || do_the_call.is_linked()) {
      __ bind(&do_the_call);
      // If we are inlining the operation using VFP3 instructions for
      // add, subtract, multiply, or divide, the arguments are in d6 and d7.
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // ARMv7 VFP3 instructions to implement
        // double precision, add, subtract, multiply, divide.

        if (Token::MUL == op_) {
          __ vmul(d5, d6, d7);
        } else if (Token::DIV == op_) {
          __ vdiv(d5, d6, d7);
        } else if (Token::ADD == op_) {
          __ vadd(d5, d6, d7);
        } else if (Token::SUB == op_) {
          __ vsub(d5, d6, d7);
        } else {
          UNREACHABLE();
        }
        __ sub(r0, r5, Operand(kHeapObjectTag));
        __ vstr(d5, r0, HeapNumber::kValueOffset);
        __ add(r0, r0, Operand(kHeapObjectTag));
        __ mov(pc, lr);
      } else {
        // If we did not inline the operation, then the arguments are in:
        // r0: Left value (least significant part of mantissa).
        // r1: Left value (sign, exponent, top of mantissa).
        // r2: Right value (least significant part of mantissa).
        // r3: Right value (sign, exponent, top of mantissa).
        // r5: Address of heap number for result.

        __ push(lr);   // For later.
        __ PrepareCallCFunction(4, r4);  // Two doubles count as 4 arguments.
        // Call C routine that may not cause GC or other trouble. r5 is callee
        // save.
        __ CallCFunction(ExternalReference::double_fp_operation(op_), 4);
        // Store answer in the overwritable heap number.
    #if !defined(USE_ARM_EABI)
        // Double returned in fp coprocessor register 0 and 1, encoded as
        // register cr8.  Offsets must be divisible by 4 for coprocessor so we
        // need to substract the tag from r5.
        __ sub(r4, r5, Operand(kHeapObjectTag));
        __ stc(p1, cr8, MemOperand(r4, HeapNumber::kValueOffset));
    #else
        // Double returned in registers 0 and 1.
        __ Strd(r0, r1, FieldMemOperand(r5, HeapNumber::kValueOffset));
    #endif
        __ mov(r0, Operand(r5));
        // And we are done.
        __ pop(pc);
      }
    }
  }

  if (!generate_code_to_calculate_answer &&
      !slow_reverse.is_linked() &&
      !slow.is_linked()) {
    return;
  }

  if (lhs.is(r0)) {
    __ b(&slow);
    __ bind(&slow_reverse);
    __ Swap(r0, r1, ip);
  }

  heap_number_map = no_reg;  // Don't use this any more from here on.

  // We jump to here if something goes wrong (one param is not a number of any
  // sort or new-space allocation fails).
  __ bind(&slow);

  // Push arguments to the stack
  __ Push(r1, r0);

  if (Token::ADD == op_) {
    // Test for string arguments before calling runtime.
    // r1 : first argument
    // r0 : second argument
    // sp[0] : second argument
    // sp[4] : first argument

    Label not_strings, not_string1, string1, string1_smi2;
    __ tst(r1, Operand(kSmiTagMask));
    __ b(eq, &not_string1);
    __ CompareObjectType(r1, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &not_string1);

    // First argument is a a string, test second.
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &string1_smi2);
    __ CompareObjectType(r0, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &string1);

    // First and second argument are strings.
    StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
    __ TailCallStub(&string_add_stub);

    __ bind(&string1_smi2);
    // First argument is a string, second is a smi. Try to lookup the number
    // string for the smi in the number string cache.
    NumberToStringStub::GenerateLookupNumberStringCache(
        masm, r0, r2, r4, r5, r6, true, &string1);

    // Replace second argument on stack and tailcall string add stub to make
    // the result.
    __ str(r2, MemOperand(sp, 0));
    __ TailCallStub(&string_add_stub);

    // Only first argument is a string.
    __ bind(&string1);
    __ InvokeBuiltin(Builtins::STRING_ADD_LEFT, JUMP_JS);

    // First argument was not a string, test second.
    __ bind(&not_string1);
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &not_strings);
    __ CompareObjectType(r0, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &not_strings);

    // Only second argument is a string.
    __ InvokeBuiltin(Builtins::STRING_ADD_RIGHT, JUMP_JS);

    __ bind(&not_strings);
  }

  __ InvokeBuiltin(builtin, JUMP_JS);  // Tail call.  No return.
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
  __ Ubfx(scratch2,
          scratch,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // Load dest with zero.  We use this either for the final shift or
  // for the answer.
  __ mov(dest, Operand(0));
  // Check whether the exponent matches a 32 bit signed int that is not a Smi.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).  This is
  // the exponent that we are fastest at and also the highest exponent we can
  // handle here.
  const uint32_t non_smi_exponent = HeapNumber::kExponentBias + 30;
  // The non_smi_exponent, 0x41d, is too big for ARM's immediate field so we
  // split it up to avoid a constant pool entry.  You can't do that in general
  // for cmp because of the overflow flag, but we know the exponent is in the
  // range 0-2047 so there is no overflow.
  int fudge_factor = 0x400;
  __ sub(scratch2, scratch2, Operand(fudge_factor));
  __ cmp(scratch2, Operand(non_smi_exponent - fudge_factor));
  // If we have a match of the int32-but-not-Smi exponent then skip some logic.
  __ b(eq, &right_exponent);
  // If the exponent is higher than that then go to slow case.  This catches
  // numbers that don't fit in a signed int32, infinities and NaNs.
  __ b(gt, slow);

  // We know the exponent is smaller than 30 (biased).  If it is less than
  // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
  // it rounds to zero.
  const uint32_t zero_exponent = HeapNumber::kExponentBias + 0;
  __ sub(scratch2, scratch2, Operand(zero_exponent - fudge_factor), SetCC);
  // Dest already has a Smi zero.
  __ b(lt, &done);
  if (!CpuFeatures::IsSupported(VFP3)) {
    // We have an exponent between 0 and 30 in scratch2.  Subtract from 30 to
    // get how much to shift down.
    __ rsb(dest, scratch2, Operand(30));
  }
  __ bind(&right_exponent);
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    // ARMv7 VFP3 instructions implementing double precision to integer
    // conversion using round to zero.
    __ ldr(scratch2, FieldMemOperand(source, HeapNumber::kMantissaOffset));
    __ vmov(d7, scratch2, scratch);
    __ vcvt_s32_f64(s15, d7);
    __ vmov(dest, s15);
  } else {
    // Get the top bits of the mantissa.
    __ and_(scratch2, scratch, Operand(HeapNumber::kMantissaMask));
    // Put back the implicit 1.
    __ orr(scratch2, scratch2, Operand(1 << HeapNumber::kExponentShift));
    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We just orred in the implicit bit so that took care of one and
    // we want to leave the sign bit 0 so we subtract 2 bits from the shift
    // distance.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    __ mov(scratch2, Operand(scratch2, LSL, shift_distance));
    // Put sign in zero flag.
    __ tst(scratch, Operand(HeapNumber::kSignMask));
    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    __ ldr(scratch, FieldMemOperand(source, HeapNumber::kMantissaOffset));
    // Shift down 22 bits to get the last 10 bits.
    __ orr(scratch, scratch2, Operand(scratch, LSR, 32 - shift_distance));
    // Move down according to the exponent.
    __ mov(dest, Operand(scratch, LSR, dest));
    // Fix sign if sign bit was set.
    __ rsb(dest, dest, Operand(0), LeaveCC, ne);
  }
  __ bind(&done);
}

// For bitwise ops where the inputs are not both Smis we here try to determine
// whether both inputs are either Smis or at least heap numbers that can be
// represented by a 32 bit signed value.  We truncate towards zero as required
// by the ES spec.  If this is the case we do the bitwise op and see if the
// result is a Smi.  If so, great, otherwise we try to find a heap number to
// write the answer into (either by allocating or by overwriting).
// On entry the operands are in lhs and rhs.  On exit the answer is in r0.
void GenericBinaryOpStub::HandleNonSmiBitwiseOp(MacroAssembler* masm,
                                                Register lhs,
                                                Register rhs) {
  Label slow, result_not_a_smi;
  Label rhs_is_smi, lhs_is_smi;
  Label done_checking_rhs, done_checking_lhs;

  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  __ tst(lhs, Operand(kSmiTagMask));
  __ b(eq, &lhs_is_smi);  // It's a Smi so don't check it's a heap number.
  __ ldr(r4, FieldMemOperand(lhs, HeapNumber::kMapOffset));
  __ cmp(r4, heap_number_map);
  __ b(ne, &slow);
  GetInt32(masm, lhs, r3, r5, r4, &slow);
  __ jmp(&done_checking_lhs);
  __ bind(&lhs_is_smi);
  __ mov(r3, Operand(lhs, ASR, 1));
  __ bind(&done_checking_lhs);

  __ tst(rhs, Operand(kSmiTagMask));
  __ b(eq, &rhs_is_smi);  // It's a Smi so don't check it's a heap number.
  __ ldr(r4, FieldMemOperand(rhs, HeapNumber::kMapOffset));
  __ cmp(r4, heap_number_map);
  __ b(ne, &slow);
  GetInt32(masm, rhs, r2, r5, r4, &slow);
  __ jmp(&done_checking_rhs);
  __ bind(&rhs_is_smi);
  __ mov(r2, Operand(rhs, ASR, 1));
  __ bind(&done_checking_rhs);

  ASSERT(((lhs.is(r0) && rhs.is(r1)) || (lhs.is(r1) && rhs.is(r0))));

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
      if (CpuFeatures::IsSupported(VFP3)) {
        __ b(mi, &result_not_a_smi);
      } else {
        __ b(mi, &slow);
      }
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
      __ tst(rhs, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(rhs));
      break;
    }
    case OVERWRITE_LEFT: {
      __ tst(lhs, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(lhs));
      break;
    }
    case NO_OVERWRITE: {
      // Get a new heap number in r5.  r4 and r7 are scratch.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
    }
    default: break;
  }
  __ bind(&got_a_heap_number);
  // r2: Answer as signed int32.
  // r5: Heap number to write answer into.

  // Nothing can go wrong now, so move the heap number to r0, which is the
  // result.
  __ mov(r0, Operand(r5));

  if (CpuFeatures::IsSupported(VFP3)) {
    // Convert the int32 in r2 to the heap number in r0. r3 is corrupted.
    CpuFeatures::Scope scope(VFP3);
    __ vmov(s0, r2);
    if (op_ == Token::SHR) {
      __ vcvt_f64_u32(d0, s0);
    } else {
      __ vcvt_f64_s32(d0, s0);
    }
    __ sub(r3, r0, Operand(kHeapObjectTag));
    __ vstr(d0, r3, HeapNumber::kValueOffset);
    __ Ret();
  } else {
    // Tail call that writes the int32 in r2 to the heap number in r0, using
    // r3 as scratch.  r0 is preserved and returned.
    WriteInt32ToHeapNumberStub stub(r2, r0, r3);
    __ TailCallStub(&stub);
  }

  if (mode_ != NO_OVERWRITE) {
    __ bind(&have_to_allocate);
    // Get a new heap number in r5.  r4 and r7 are scratch.
    __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
    __ jmp(&got_a_heap_number);
  }

  // If all else failed then we go to the runtime system.
  __ bind(&slow);
  __ Push(lhs, rhs);  // Restore stack.
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


// This uses versions of the sum-of-digits-to-see-if-a-number-is-divisible-by-3
// trick.  See http://en.wikipedia.org/wiki/Divisibility_rule
// Takes the sum of the digits base (mask + 1) repeatedly until we have a
// number from 0 to mask.  On exit the 'eq' condition flags are set if the
// answer is exactly the mask.
void IntegerModStub::DigitSum(MacroAssembler* masm,
                              Register lhs,
                              int mask,
                              int shift,
                              Label* entry) {
  ASSERT(mask > 0);
  ASSERT(mask <= 0xff);  // This ensures we don't need ip to use it.
  Label loop;
  __ bind(&loop);
  __ and_(ip, lhs, Operand(mask));
  __ add(lhs, ip, Operand(lhs, LSR, shift));
  __ bind(entry);
  __ cmp(lhs, Operand(mask));
  __ b(gt, &loop);
}


void IntegerModStub::DigitSum(MacroAssembler* masm,
                              Register lhs,
                              Register scratch,
                              int mask,
                              int shift1,
                              int shift2,
                              Label* entry) {
  ASSERT(mask > 0);
  ASSERT(mask <= 0xff);  // This ensures we don't need ip to use it.
  Label loop;
  __ bind(&loop);
  __ bic(scratch, lhs, Operand(mask));
  __ and_(ip, lhs, Operand(mask));
  __ add(lhs, ip, Operand(lhs, LSR, shift1));
  __ add(lhs, lhs, Operand(scratch, LSR, shift2));
  __ bind(entry);
  __ cmp(lhs, Operand(mask));
  __ b(gt, &loop);
}


// Splits the number into two halves (bottom half has shift bits).  The top
// half is subtracted from the bottom half.  If the result is negative then
// rhs is added.
void IntegerModStub::ModGetInRangeBySubtraction(MacroAssembler* masm,
                                                Register lhs,
                                                int shift,
                                                int rhs) {
  int mask = (1 << shift) - 1;
  __ and_(ip, lhs, Operand(mask));
  __ sub(lhs, ip, Operand(lhs, LSR, shift), SetCC);
  __ add(lhs, lhs, Operand(rhs), LeaveCC, mi);
}


void IntegerModStub::ModReduce(MacroAssembler* masm,
                               Register lhs,
                               int max,
                               int denominator) {
  int limit = denominator;
  while (limit * 2 <= max) limit *= 2;
  while (limit >= denominator) {
    __ cmp(lhs, Operand(limit));
    __ sub(lhs, lhs, Operand(limit), LeaveCC, ge);
    limit >>= 1;
  }
}


void IntegerModStub::ModAnswer(MacroAssembler* masm,
                               Register result,
                               Register shift_distance,
                               Register mask_bits,
                               Register sum_of_digits) {
  __ add(result, mask_bits, Operand(sum_of_digits, LSL, shift_distance));
  __ Ret();
}


// See comment for class.
void IntegerModStub::Generate(MacroAssembler* masm) {
  __ mov(lhs_, Operand(lhs_, LSR, shift_distance_));
  __ bic(odd_number_, odd_number_, Operand(1));
  __ mov(odd_number_, Operand(odd_number_, LSL, 1));
  // We now have (odd_number_ - 1) * 2 in the register.
  // Build a switch out of branches instead of data because it avoids
  // having to teach the assembler about intra-code-object pointers
  // that are not in relative branch instructions.
  Label mod3, mod5, mod7, mod9, mod11, mod13, mod15, mod17, mod19;
  Label mod21, mod23, mod25;
  { Assembler::BlockConstPoolScope block_const_pool(masm);
    __ add(pc, pc, Operand(odd_number_));
    // When you read pc it is always 8 ahead, but when you write it you always
    // write the actual value.  So we put in two nops to take up the slack.
    __ nop();
    __ nop();
    __ b(&mod3);
    __ b(&mod5);
    __ b(&mod7);
    __ b(&mod9);
    __ b(&mod11);
    __ b(&mod13);
    __ b(&mod15);
    __ b(&mod17);
    __ b(&mod19);
    __ b(&mod21);
    __ b(&mod23);
    __ b(&mod25);
  }

  // For each denominator we find a multiple that is almost only ones
  // when expressed in binary.  Then we do the sum-of-digits trick for
  // that number.  If the multiple is not 1 then we have to do a little
  // more work afterwards to get the answer into the 0-denominator-1
  // range.
  DigitSum(masm, lhs_, 3, 2, &mod3);  // 3 = b11.
  __ sub(lhs_, lhs_, Operand(3), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xf, 4, &mod5);  // 5 * 3 = b1111.
  ModGetInRangeBySubtraction(masm, lhs_, 2, 5);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 7, 3, &mod7);  // 7 = b111.
  __ sub(lhs_, lhs_, Operand(7), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0x3f, 6, &mod9);  // 7 * 9 = b111111.
  ModGetInRangeBySubtraction(masm, lhs_, 3, 9);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0x3f, 6, 3, &mod11);  // 5 * 11 = b110111.
  ModReduce(masm, lhs_, 0x3f, 11);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 5, &mod13);  // 19 * 13 = b11110111.
  ModReduce(masm, lhs_, 0xff, 13);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xf, 4, &mod15);  // 15 = b1111.
  __ sub(lhs_, lhs_, Operand(15), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xff, 8, &mod17);  // 15 * 17 = b11111111.
  ModGetInRangeBySubtraction(masm, lhs_, 4, 17);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 5, &mod19);  // 13 * 19 = b11110111.
  ModReduce(masm, lhs_, 0xff, 19);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0x3f, 6, &mod21);  // 3 * 21 = b111111.
  ModReduce(masm, lhs_, 0x3f, 21);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 7, &mod23);  // 11 * 23 = b11111101.
  ModReduce(masm, lhs_, 0xff, 23);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0x7f, 7, 6, &mod25);  // 5 * 25 = b1111101.
  ModReduce(masm, lhs_, 0x7f, 25);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);
}


const char* GenericBinaryOpStub::GetName() {
  if (name_ != NULL) return name_;
  const int len = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(len);
  if (name_ == NULL) return "OOM";
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }

  OS::SNPrintF(Vector<char>(name_, len),
               "GenericBinaryOpStub_%s_%s%s_%s",
               op_name,
               overwrite_name,
               specialized_on_rhs_ ? "_ConstantRhs" : "",
               BinaryOpIC::GetName(runtime_operands_type_));
  return name_;
}



void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  // lhs_ : x
  // rhs_ : y
  // r0   : result

  Register result = r0;
  Register lhs = lhs_;
  Register rhs = rhs_;

  // This code can't cope with other register allocations yet.
  ASSERT(result.is(r0) &&
         ((lhs.is(r0) && rhs.is(r1)) ||
          (lhs.is(r1) && rhs.is(r0))));

  Register smi_test_reg = VirtualFrame::scratch0();
  Register scratch = VirtualFrame::scratch1();

  // All ops need to know whether we are dealing with two Smis.  Set up
  // smi_test_reg to tell us that.
  if (ShouldGenerateSmiCode()) {
    __ orr(smi_test_reg, lhs, Operand(rhs));
  }

  switch (op_) {
    case Token::ADD: {
      Label not_smi;
      // Fast path.
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        __ b(ne, &not_smi);
        __ add(r0, r1, Operand(r0), SetCC);  // Add y optimistically.
        // Return if no overflow.
        __ Ret(vc);
        __ sub(r0, r0, Operand(r1));  // Revert optimistic add.
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::ADD);
      break;
    }

    case Token::SUB: {
      Label not_smi;
      // Fast path.
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        __ b(ne, &not_smi);
        if (lhs.is(r1)) {
          __ sub(r0, r1, Operand(r0), SetCC);  // Subtract y optimistically.
          // Return if no overflow.
          __ Ret(vc);
          __ sub(r0, r1, Operand(r0));  // Revert optimistic subtract.
        } else {
          __ sub(r0, r0, Operand(r1), SetCC);  // Subtract y optimistically.
          // Return if no overflow.
          __ Ret(vc);
          __ add(r0, r0, Operand(r1));  // Revert optimistic subtract.
        }
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::SUB);
      break;
    }

    case Token::MUL: {
      Label not_smi, slow;
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // adjust code below
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        Register scratch2 = smi_test_reg;
        smi_test_reg = no_reg;
        __ b(ne, &not_smi);
        // Remove tag from one operand (but keep sign), so that result is Smi.
        __ mov(ip, Operand(rhs, ASR, kSmiTagSize));
        // Do multiplication
        // scratch = lower 32 bits of ip * lhs.
        __ smull(scratch, scratch2, lhs, ip);
        // Go slow on overflows (overflow bit is not set).
        __ mov(ip, Operand(scratch, ASR, 31));
        // No overflow if higher 33 bits are identical.
        __ cmp(ip, Operand(scratch2));
        __ b(ne, &slow);
        // Go slow on zero result to handle -0.
        __ tst(scratch, Operand(scratch));
        __ mov(result, Operand(scratch), LeaveCC, ne);
        __ Ret(ne);
        // We need -0 if we were multiplying a negative number with 0 to get 0.
        // We know one of them was zero.
        __ add(scratch2, rhs, Operand(lhs), SetCC);
        __ mov(result, Operand(Smi::FromInt(0)), LeaveCC, pl);
        __ Ret(pl);  // Return Smi 0 if the non-zero one was positive.
        // Slow case.  We fall through here if we multiplied a negative number
        // with 0, because that would mean we should produce -0.
        __ bind(&slow);
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::MUL);
      break;
    }

    case Token::DIV:
    case Token::MOD: {
      Label not_smi;
      if (ShouldGenerateSmiCode() && specialized_on_rhs_) {
        Label lhs_is_unsuitable;
        __ BranchOnNotSmi(lhs, &not_smi);
        if (IsPowerOf2(constant_rhs_)) {
          if (op_ == Token::MOD) {
            __ and_(rhs,
                    lhs,
                    Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)),
                    SetCC);
            // We now have the answer, but if the input was negative we also
            // have the sign bit.  Our work is done if the result is
            // positive or zero:
            if (!rhs.is(r0)) {
              __ mov(r0, rhs, LeaveCC, pl);
            }
            __ Ret(pl);
            // A mod of a negative left hand side must return a negative number.
            // Unfortunately if the answer is 0 then we must return -0.  And we
            // already optimistically trashed rhs so we may need to restore it.
            __ eor(rhs, rhs, Operand(0x80000000u), SetCC);
            // Next two instructions are conditional on the answer being -0.
            __ mov(rhs, Operand(Smi::FromInt(constant_rhs_)), LeaveCC, eq);
            __ b(eq, &lhs_is_unsuitable);
            // We need to subtract the dividend.  Eg. -3 % 4 == -3.
            __ sub(result, rhs, Operand(Smi::FromInt(constant_rhs_)));
          } else {
            ASSERT(op_ == Token::DIV);
            __ tst(lhs,
                   Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)));
            __ b(ne, &lhs_is_unsuitable);  // Go slow on negative or remainder.
            int shift = 0;
            int d = constant_rhs_;
            while ((d & 1) == 0) {
              d >>= 1;
              shift++;
            }
            __ mov(r0, Operand(lhs, LSR, shift));
            __ bic(r0, r0, Operand(kSmiTagMask));
          }
        } else {
          // Not a power of 2.
          __ tst(lhs, Operand(0x80000000u));
          __ b(ne, &lhs_is_unsuitable);
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
          Register scratch2 = smi_test_reg;
          smi_test_reg = no_reg;
          __ mov(scratch2, Operand(mul));
          __ umull(scratch, scratch2, scratch2, lhs);
          __ mov(scratch2, Operand(scratch2, LSR, shift - 31));
          // scratch2 is lhs / rhs.  scratch2 is not Smi tagged.
          // rhs is still the known rhs.  rhs is Smi tagged.
          // lhs is still the unkown lhs.  lhs is Smi tagged.
          int required_scratch_shift = 0;  // Including the Smi tag shift of 1.
          // scratch = scratch2 * rhs.
          MultiplyByKnownInt2(masm,
                              scratch,
                              scratch2,
                              rhs,
                              constant_rhs_,
                              &required_scratch_shift);
          // scratch << required_scratch_shift is now the Smi tagged rhs *
          // (lhs / rhs) where / indicates integer division.
          if (op_ == Token::DIV) {
            __ cmp(lhs, Operand(scratch, LSL, required_scratch_shift));
            __ b(ne, &lhs_is_unsuitable);  // There was a remainder.
            __ mov(result, Operand(scratch2, LSL, kSmiTagSize));
          } else {
            ASSERT(op_ == Token::MOD);
            __ sub(result, lhs, Operand(scratch, LSL, required_scratch_shift));
          }
        }
        __ Ret();
        __ bind(&lhs_is_unsuitable);
      } else if (op_ == Token::MOD &&
                 runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
                 runtime_operands_type_ != BinaryOpIC::STRINGS) {
        // Do generate a bit of smi code for modulus even though the default for
        // modulus is not to do it, but as the ARM processor has no coprocessor
        // support for modulus checking for smis makes sense.  We can handle
        // 1 to 25 times any power of 2.  This covers over half the numbers from
        // 1 to 100 including all of the first 25.  (Actually the constants < 10
        // are handled above by reciprocal multiplication.  We only get here for
        // those cases if the right hand side is not a constant or for cases
        // like 192 which is 3*2^6 and ends up in the 3 case in the integer mod
        // stub.)
        Label slow;
        Label not_power_of_2;
        ASSERT(!ShouldGenerateSmiCode());
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        // Check for two positive smis.
        __ orr(smi_test_reg, lhs, Operand(rhs));
        __ tst(smi_test_reg, Operand(0x80000000u | kSmiTagMask));
        __ b(ne, &slow);
        // Check that rhs is a power of two and not zero.
        Register mask_bits = r3;
        __ sub(scratch, rhs, Operand(1), SetCC);
        __ b(mi, &slow);
        __ and_(mask_bits, rhs, Operand(scratch), SetCC);
        __ b(ne, &not_power_of_2);
        // Calculate power of two modulus.
        __ and_(result, lhs, Operand(scratch));
        __ Ret();

        __ bind(&not_power_of_2);
        __ eor(scratch, scratch, Operand(mask_bits));
        // At least two bits are set in the modulus.  The high one(s) are in
        // mask_bits and the low one is scratch + 1.
        __ and_(mask_bits, scratch, Operand(lhs));
        Register shift_distance = scratch;
        scratch = no_reg;

        // The rhs consists of a power of 2 multiplied by some odd number.
        // The power-of-2 part we handle by putting the corresponding bits
        // from the lhs in the mask_bits register, and the power in the
        // shift_distance register.  Shift distance is never 0 due to Smi
        // tagging.
        __ CountLeadingZeros(r4, shift_distance, shift_distance);
        __ rsb(shift_distance, r4, Operand(32));

        // Now we need to find out what the odd number is. The last bit is
        // always 1.
        Register odd_number = r4;
        __ mov(odd_number, Operand(rhs, LSR, shift_distance));
        __ cmp(odd_number, Operand(25));
        __ b(gt, &slow);

        IntegerModStub stub(
            result, shift_distance, odd_number, mask_bits, lhs, r5);
        __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);  // Tail call.

        __ bind(&slow);
      }
      HandleBinaryOpSlowCases(
          masm,
          &not_smi,
          lhs,
          rhs,
          op_ == Token::MOD ? Builtins::MOD : Builtins::DIV);
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHR:
    case Token::SHL: {
      Label slow;
      STATIC_ASSERT(kSmiTag == 0);  // adjust code below
      __ tst(smi_test_reg, Operand(kSmiTagMask));
      __ b(ne, &slow);
      Register scratch2 = smi_test_reg;
      smi_test_reg = no_reg;
      switch (op_) {
        case Token::BIT_OR:  __ orr(result, rhs, Operand(lhs)); break;
        case Token::BIT_AND: __ and_(result, rhs, Operand(lhs)); break;
        case Token::BIT_XOR: __ eor(result, rhs, Operand(lhs)); break;
        case Token::SAR:
          // Remove tags from right operand.
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(result, Operand(lhs, ASR, scratch2));
          // Smi tag result.
          __ bic(result, result, Operand(kSmiTagMask));
          break;
        case Token::SHR:
          // Remove tags from operands.  We can't do this on a 31 bit number
          // because then the 0s get shifted into bit 30 instead of bit 31.
          __ mov(scratch, Operand(lhs, ASR, kSmiTagSize));  // x
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(scratch, Operand(scratch, LSR, scratch2));
          // Unsigned shift is not allowed to produce a negative number, so
          // check the sign bit and the sign bit after Smi tagging.
          __ tst(scratch, Operand(0xc0000000));
          __ b(ne, &slow);
          // Smi tag result.
          __ mov(result, Operand(scratch, LSL, kSmiTagSize));
          break;
        case Token::SHL:
          // Remove tags from operands.
          __ mov(scratch, Operand(lhs, ASR, kSmiTagSize));  // x
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(scratch, Operand(scratch, LSL, scratch2));
          // Check that the signed result fits in a Smi.
          __ add(scratch2, scratch, Operand(0x40000000), SetCC);
          __ b(mi, &slow);
          __ mov(result, Operand(scratch, LSL, kSmiTagSize));
          break;
        default: UNREACHABLE();
      }
      __ Ret();
      __ bind(&slow);
      HandleNonSmiBitwiseOp(masm, lhs, rhs);
      break;
    }

    default: UNREACHABLE();
  }
  // This code should be unreachable.
  __ stop("Unreachable");

  // Generate an unreachable reference to the DEFAULT stub so that it can be
  // found at the end of this stub when clearing ICs at GC.
  // TODO(kaznacheev): Check performance impact and get rid of this.
  if (runtime_operands_type_ != BinaryOpIC::DEFAULT) {
    GenericBinaryOpStub uninit(MinorKey(), BinaryOpIC::DEFAULT);
    __ CallStub(&uninit);
  }
}


void GenericBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  Label get_result;

  __ Push(r1, r0);

  __ mov(r2, Operand(Smi::FromInt(MinorKey())));
  __ mov(r1, Operand(Smi::FromInt(op_)));
  __ mov(r0, Operand(Smi::FromInt(runtime_operands_type_)));
  __ Push(r2, r1, r0);

  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch)),
      5,
      1);
}


Handle<Code> GetBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info) {
  GenericBinaryOpStub stub(key, type_info);
  return stub.GetCode();
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // Argument is a number and is on stack and in r0.
  Label runtime_call;
  Label input_not_smi;
  Label loaded;

  if (CpuFeatures::IsSupported(VFP3)) {
    // Load argument and check if it is a smi.
    __ BranchOnNotSmi(r0, &input_not_smi);

    CpuFeatures::Scope scope(VFP3);
    // Input is a smi. Convert to double and load the low and high words
    // of the double into r2, r3.
    __ IntegerToDoubleConversionWithVFP3(r0, r3, r2);
    __ b(&loaded);

    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ CheckMap(r0,
                r1,
                Heap::kHeapNumberMapRootIndex,
                &runtime_call,
                true);
    // Input is a HeapNumber. Load it to a double register and store the
    // low and high words into r2, r3.
    __ Ldrd(r2, r3, FieldMemOperand(r0, HeapNumber::kValueOffset));

    __ bind(&loaded);
    // r2 = low 32 bits of double value
    // r3 = high 32 bits of double value
    // Compute hash (the shifts are arithmetic):
    //   h = (low ^ high); h ^= h >> 16; h ^= h >> 8; h = h & (cacheSize - 1);
    __ eor(r1, r2, Operand(r3));
    __ eor(r1, r1, Operand(r1, ASR, 16));
    __ eor(r1, r1, Operand(r1, ASR, 8));
    ASSERT(IsPowerOf2(TranscendentalCache::kCacheSize));
    __ And(r1, r1, Operand(TranscendentalCache::kCacheSize - 1));

    // r2 = low 32 bits of double value.
    // r3 = high 32 bits of double value.
    // r1 = TranscendentalCache::hash(double value).
    __ mov(r0,
           Operand(ExternalReference::transcendental_cache_array_address()));
    // r0 points to cache array.
    __ ldr(r0, MemOperand(r0, type_ * sizeof(TranscendentalCache::caches_[0])));
    // r0 points to the cache for the type type_.
    // If NULL, the cache hasn't been initialized yet, so go through runtime.
    __ cmp(r0, Operand(0));
    __ b(eq, &runtime_call);

#ifdef DEBUG
    // Check that the layout of cache elements match expectations.
    { TranscendentalCache::Element test_elem[2];
      char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
      char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
      char* elem_in0 = reinterpret_cast<char*>(&(test_elem[0].in[0]));
      char* elem_in1 = reinterpret_cast<char*>(&(test_elem[0].in[1]));
      char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
      CHECK_EQ(12, elem2_start - elem_start);  // Two uint_32's and a pointer.
      CHECK_EQ(0, elem_in0 - elem_start);
      CHECK_EQ(kIntSize, elem_in1 - elem_start);
      CHECK_EQ(2 * kIntSize, elem_out - elem_start);
    }
#endif

    // Find the address of the r1'st entry in the cache, i.e., &r0[r1*12].
    __ add(r1, r1, Operand(r1, LSL, 1));
    __ add(r0, r0, Operand(r1, LSL, 2));
    // Check if cache matches: Double value is stored in uint32_t[2] array.
    __ ldm(ia, r0, r4.bit()| r5.bit() | r6.bit());
    __ cmp(r2, r4);
    __ b(ne, &runtime_call);
    __ cmp(r3, r5);
    __ b(ne, &runtime_call);
    // Cache hit. Load result, pop argument and return.
    __ mov(r0, Operand(r6));
    __ pop();
    __ Ret();
  }

  __ bind(&runtime_call);
  __ TailCallExternalReference(ExternalReference(RuntimeFunction()), 1, 1);
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    // Add more cases when necessary.
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  // Do tail-call to runtime routine.  Runtime routines expect at least one
  // argument, so give it a Smi.
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ push(r0);
  __ TailCallRuntime(Runtime::kStackGuard, 1, 1);

  __ StubReturn(1);
}


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done;

  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  if (op_ == Token::SUB) {
    // Check whether the value is a smi.
    Label try_float;
    __ tst(r0, Operand(kSmiTagMask));
    __ b(ne, &try_float);

    // Go slow case if the value of the expression is zero
    // to make sure that we switch between 0 and -0.
    if (negative_zero_ == kStrictNegativeZero) {
      // If we have to check for zero, then we can check for the max negative
      // smi while we are at it.
      __ bic(ip, r0, Operand(0x80000000), SetCC);
      __ b(eq, &slow);
      __ rsb(r0, r0, Operand(0));
      __ StubReturn(1);
    } else {
      // The value of the expression is a smi and 0 is OK for -0.  Try
      // optimistic subtraction '0 - value'.
      __ rsb(r0, r0, Operand(0), SetCC);
      __ StubReturn(1, vc);
      // We don't have to reverse the optimistic neg since the only case
      // where we fall through is the minimum negative Smi, which is the case
      // where the neg leaves the register unchanged.
      __ jmp(&slow);  // Go slow on max negative Smi.
    }

    __ bind(&try_float);
    __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
    __ cmp(r1, heap_number_map);
    __ b(ne, &slow);
    // r0 is a heap number.  Get a new heap number in r1.
    if (overwrite_ == UNARY_OVERWRITE) {
      __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
      __ str(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    } else {
      __ AllocateHeapNumber(r1, r2, r3, r6, &slow);
      __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
      __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      __ str(r3, FieldMemOperand(r1, HeapNumber::kMantissaOffset));
      __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
      __ str(r2, FieldMemOperand(r1, HeapNumber::kExponentOffset));
      __ mov(r0, Operand(r1));
    }
  } else if (op_ == Token::BIT_NOT) {
    // Check if the operand is a heap number.
    __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
    __ cmp(r1, heap_number_map);
    __ b(ne, &slow);

    // Convert the heap number is r0 to an untagged integer in r1.
    GetInt32(masm, r0, r1, r2, r3, &slow);

    // Do the bitwise operation (move negated) and check if the result
    // fits in a smi.
    Label try_float;
    __ mvn(r1, Operand(r1));
    __ add(r2, r1, Operand(0x40000000), SetCC);
    __ b(mi, &try_float);
    __ mov(r0, Operand(r1, LSL, kSmiTagSize));
    __ b(&done);

    __ bind(&try_float);
    if (!overwrite_ == UNARY_OVERWRITE) {
      // Allocate a fresh heap number, but don't overwrite r0 until
      // we're sure we can do it without going through the slow case
      // that needs the value in r0.
      __ AllocateHeapNumber(r2, r3, r4, r6, &slow);
      __ mov(r0, Operand(r2));
    }

    if (CpuFeatures::IsSupported(VFP3)) {
      // Convert the int32 in r1 to the heap number in r0. r2 is corrupted.
      CpuFeatures::Scope scope(VFP3);
      __ vmov(s0, r1);
      __ vcvt_f64_s32(d0, s0);
      __ sub(r2, r0, Operand(kHeapObjectTag));
      __ vstr(d0, r2, HeapNumber::kValueOffset);
    } else {
      // WriteInt32ToHeapNumberStub does not trigger GC, so we do not
      // have to set up a frame.
      WriteInt32ToHeapNumberStub stub(r1, r0, r2);
      __ push(lr);
      __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
      __ pop(lr);
    }
  } else {
    UNIMPLEMENTED();
  }

  __ bind(&done);
  __ StubReturn(1);

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
  __ push(r0);
  switch (op_) {
    case Token::SUB:
      __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_JS);
      break;
    case Token::BIT_NOT:
      __ InvokeBuiltin(Builtins::BIT_NOT, JUMP_JS);
      break;
    default:
      UNREACHABLE();
  }
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // r0 holds the exception.

  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // Drop the sp to the top of the handler.
  __ mov(r3, Operand(ExternalReference(Top::k_handler_address)));
  __ ldr(sp, MemOperand(r3));

  // Restore the next handler and frame pointer, discard handler state.
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  __ pop(r2);
  __ str(r2, MemOperand(r3));
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 2 * kPointerSize);
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
  STATIC_ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ pop(pc);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

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
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
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
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 2 * kPointerSize);
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
  STATIC_ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ pop(pc);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate,
                              int frame_alignment_skew) {
  // r0: result parameter for PerformGC, if any
  // r4: number of arguments including receiver  (C callee-saved)
  // r5: pointer to builtin function  (C callee-saved)
  // r6: pointer to the first argument (C callee-saved)

  if (do_gc) {
    // Passing r0.
    __ PrepareCallCFunction(1, r1);
    __ CallCFunction(ExternalReference::perform_gc_function(), 1);
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

  int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
#if defined(V8_HOST_ARCH_ARM)
  if (FLAG_debug_code) {
    if (frame_alignment > kPointerSize) {
      Label alignment_as_expected;
      ASSERT(IsPowerOf2(frame_alignment));
      __ sub(r2, sp, Operand(frame_alignment_skew));
      __ tst(r2, Operand(frame_alignment_mask));
      __ b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      __ stop("Unexpected alignment");
      __ bind(&alignment_as_expected);
    }
  }
#endif

  // Just before the call (jump) below lr is pushed, so the actual alignment is
  // adding one to the current skew.
  int alignment_before_call =
      (frame_alignment_skew + kPointerSize) & frame_alignment_mask;
  if (alignment_before_call > 0) {
    // Push until the alignment before the call is met.
    __ mov(r2, Operand(0));
    for (int i = alignment_before_call;
        (i & frame_alignment_mask) != 0;
        i += kPointerSize) {
      __ push(r2);
    }
  }

  // TODO(1242173): To let the GC traverse the return address of the exit
  // frames, we need to know where the return address is. Right now,
  // we push it on the stack to be able to find it again, but we never
  // restore from it in case of changes, which makes it impossible to
  // support moving the C entry code stub. This should be fixed, but currently
  // this is OK because the CEntryStub gets generated so early in the V8 boot
  // sequence that it is not moving ever.
  masm->add(lr, pc, Operand(4));  // Compute return address: (pc + 8) + 4
  masm->push(lr);
  masm->Jump(r5);

  // Restore sp back to before aligning the stack.
  if (alignment_before_call > 0) {
    __ add(sp, sp, Operand(alignment_before_call));
  }

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
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  // Lower 2 bits of r2 are 0 iff r0 has failure tag.
  __ add(r2, r0, Operand(1));
  __ tst(r2, Operand(kFailureTagMask));
  __ b(eq, &failure_returned);

  // Exit C frame and return.
  // r0:r1: result
  // sp: stack pointer
  // fp: frame pointer
  __ LeaveExitFrame(mode_);

  // check if we should retry or throw exception
  Label retry;
  __ bind(&failure_returned);
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
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


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // r0: number of arguments including receiver
  // r1: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  // Result returned in r0 or r0+r1 by default.

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(mode_);

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
               false,
               false,
               -kPointerSize);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               false,
               0);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ mov(r0, Operand(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true,
               kPointerSize);

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
  __ ldr(r4, MemOperand(sp, (kNumCalleeSaved + 1) * kPointerSize));  // argv

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
  __ Push(r8, r7, r6, r5);

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
  __ ldr(r1, MemOperand(sp, 0));
  // r1 is function, r3 is map.

  // Look up the function and the map in the instanceof cache.
  Label miss;
  __ LoadRoot(ip, Heap::kInstanceofCacheFunctionRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &miss);
  __ LoadRoot(ip, Heap::kInstanceofCacheMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &miss);
  __ LoadRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
  __ pop();
  __ pop();
  __ mov(pc, Operand(lr));

  __ bind(&miss);
  __ TryGetFunctionPrototype(r1, r4, r2, &slow);

  // Check that the function prototype is a JS object.
  __ BranchOnSmi(r4, &slow);
  __ CompareObjectType(r4, r5, r5, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &slow);
  __ cmp(r5, Operand(LAST_JS_OBJECT_TYPE));
  __ b(gt, &slow);

  __ StoreRoot(r1, Heap::kInstanceofCacheFunctionRootIndex);
  __ StoreRoot(r3, Heap::kInstanceofCacheMapRootIndex);

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
  __ StoreRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
  __ pop();
  __ pop();
  __ mov(pc, Operand(lr));  // Return.

  __ bind(&is_not_instance);
  __ mov(r0, Operand(Smi::FromInt(1)));
  __ StoreRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
  __ pop();
  __ pop();
  __ mov(pc, Operand(lr));  // Return.

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_JS);
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
  // through register r0. Use unsigned comparison to get negative
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
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor_frame);

  // Get the length from the frame.
  __ ldr(r1, MemOperand(sp, 0));
  __ b(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ ldr(r1, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ str(r1, MemOperand(sp, 0));
  __ add(r3, r2, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(r3, r3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ str(r3, MemOperand(sp, 1 * kPointerSize));

  // Try the new space allocation. Start out with computing the size
  // of the arguments object and the elements array in words.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ cmp(r1, Operand(0));
  __ b(eq, &add_arguments_object);
  __ mov(r1, Operand(r1, LSR, kSmiTagSize));
  __ add(r1, r1, Operand(FixedArray::kHeaderSize / kPointerSize));
  __ bind(&add_arguments_object);
  __ add(r1, r1, Operand(Heap::kArgumentsObjectSize / kPointerSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(
      r1,
      r0,
      r2,
      r3,
      &runtime,
      static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ ldr(r4, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ ldr(r4, FieldMemOperand(r4, GlobalObject::kGlobalContextOffset));
  __ ldr(r4, MemOperand(r4, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ ldr(r3, FieldMemOperand(r4, i));
    __ str(r3, FieldMemOperand(r0, i));
  }

  // Setup the callee in-object property.
  STATIC_ASSERT(Heap::arguments_callee_index == 0);
  __ ldr(r3, MemOperand(sp, 2 * kPointerSize));
  __ str(r3, FieldMemOperand(r0, JSObject::kHeaderSize));

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::arguments_length_index == 1);
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));
  __ str(r1, FieldMemOperand(r0, JSObject::kHeaderSize + kPointerSize));

  // If there are no actual arguments, we're done.
  Label done;
  __ cmp(r1, Operand(0));
  __ b(eq, &done);

  // Get the parameters pointer from the stack.
  __ ldr(r2, MemOperand(sp, 1 * kPointerSize));

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ add(r4, r0, Operand(Heap::kArgumentsObjectSize));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ LoadRoot(r3, Heap::kFixedArrayMapRootIndex);
  __ str(r3, FieldMemOperand(r4, FixedArray::kMapOffset));
  __ str(r1, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ mov(r1, Operand(r1, LSR, kSmiTagSize));  // Untag the length for the loop.

  // Copy the fixed array slots.
  Label loop;
  // Setup r4 to point to the first array slot.
  __ add(r4, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ bind(&loop);
  // Pre-decrement r2 with kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ ldr(r3, MemOperand(r2, kPointerSize, NegPreIndex));
  // Post-increment r4 with kPointerSize on each iteration.
  __ str(r3, MemOperand(r4, kPointerSize, PostIndex));
  __ sub(r1, r1, Operand(1));
  __ cmp(r1, Operand(0));
  __ b(ne, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#else  // V8_INTERPRETED_REGEXP
  if (!FLAG_regexp_entry_native) {
    __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
    return;
  }

  // Stack frame on entry.
  //  sp[0]: last_match_info (expected JSArray)
  //  sp[4]: previous index
  //  sp[8]: subject string
  //  sp[12]: JSRegExp object

  static const int kLastMatchInfoOffset = 0 * kPointerSize;
  static const int kPreviousIndexOffset = 1 * kPointerSize;
  static const int kSubjectOffset = 2 * kPointerSize;
  static const int kJSRegExpOffset = 3 * kPointerSize;

  Label runtime, invoke_regexp;

  // Allocation of registers for this function. These are in callee save
  // registers and will be preserved by the call to the native RegExp code, as
  // this code is called using the normal C calling convention. When calling
  // directly from generated code the native RegExp code will not do a GC and
  // therefore the content of these registers are safe to use after the call.
  Register subject = r4;
  Register regexp_data = r5;
  Register last_match_info_elements = r6;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ mov(r0, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r0, MemOperand(r0, 0));
  __ tst(r0, Operand(r0));
  __ b(eq, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ ldr(r0, MemOperand(sp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  __ b(ne, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ ldr(regexp_data, FieldMemOperand(r0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ tst(regexp_data, Operand(kSmiTagMask));
    __ Check(nz, "Unexpected type for RegExp data, FixedArray expected");
    __ CompareObjectType(regexp_data, r0, r0, FIXED_ARRAY_TYPE);
    __ Check(eq, "Unexpected type for RegExp data, FixedArray expected");
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ ldr(r0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ cmp(r0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ b(ne, &runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ ldr(r2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2. This
  // uses the asumption that smis are 2 * their untagged value.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(r2, r2, Operand(2));  // r2 was a smi.
  // Check that the static offsets vector buffer is large enough.
  __ cmp(r2, Operand(OffsetsVector::kStaticOffsetsVectorSize));
  __ b(hi, &runtime);

  // r2: Number of capture registers
  // regexp_data: RegExp data (FixedArray)
  // Check that the second argument is a string.
  __ ldr(subject, MemOperand(sp, kSubjectOffset));
  __ tst(subject, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  Condition is_string = masm->IsObjectStringType(subject, r0);
  __ b(NegateCondition(is_string), &runtime);
  // Get the length of the string to r3.
  __ ldr(r3, FieldMemOperand(subject, String::kLengthOffset));

  // r2: Number of capture registers
  // r3: Length of subject string as a smi
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (unsigned comparison).
  __ ldr(r0, MemOperand(sp, kPreviousIndexOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(ne, &runtime);
  __ cmp(r3, Operand(r0));
  __ b(ls, &runtime);

  // r2: Number of capture registers
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the fourth object is a JSArray object.
  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  __ b(ne, &runtime);
  // Check that the JSArray is in fast case.
  __ ldr(last_match_info_elements,
         FieldMemOperand(r0, JSArray::kElementsOffset));
  __ ldr(r0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(r0, ip);
  __ b(ne, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ ldr(r0,
         FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ add(r2, r2, Operand(RegExpImpl::kLastMatchOverhead));
  __ cmp(r2, Operand(r0, ASR, kSmiTagSize));
  __ b(gt, &runtime);

  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_string;
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  // First check for flat string.
  __ tst(r0, Operand(kIsNotStringMask | kStringRepresentationMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ b(eq, &seq_string);

  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  STATIC_ASSERT(kExternalStringTag !=0);
  STATIC_ASSERT((kConsStringTag & kExternalStringTag) == 0);
  __ tst(r0, Operand(kIsNotStringMask | kExternalStringTag));
  __ b(ne, &runtime);
  __ ldr(r0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ LoadRoot(r1, Heap::kEmptyStringRootIndex);
  __ cmp(r0, r1);
  __ b(ne, &runtime);
  __ ldr(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  // Is first part a flat string?
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(r0, Operand(kStringRepresentationMask));
  __ b(nz, &runtime);

  __ bind(&seq_string);
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // r0: Instance type of subject string
  STATIC_ASSERT(4 == kAsciiStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // Find the code object based on the assumptions above.
  __ and_(r0, r0, Operand(kStringEncodingMask));
  __ mov(r3, Operand(r0, ASR, 2), SetCC);
  __ ldr(r7, FieldMemOperand(regexp_data, JSRegExp::kDataAsciiCodeOffset), ne);
  __ ldr(r7, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset), eq);

  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CompareObjectType(r7, r0, r0, CODE_TYPE);
  __ b(ne, &runtime);

  // r3: encoding of subject string (1 if ascii, 0 if two_byte);
  // r7: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ ldr(r1, MemOperand(sp, kPreviousIndexOffset));
  __ mov(r1, Operand(r1, ASR, kSmiTagSize));

  // r1: previous index
  // r3: encoding of subject string (1 if ascii, 0 if two_byte);
  // r7: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1, r0, r2);

  static const int kRegExpExecuteArguments = 7;
  __ push(lr);
  __ PrepareCallCFunction(kRegExpExecuteArguments, r0);

  // Argument 7 (sp[8]): Indicate that this is a direct call from JavaScript.
  __ mov(r0, Operand(1));
  __ str(r0, MemOperand(sp, 2 * kPointerSize));

  // Argument 6 (sp[4]): Start (high end) of backtracking stack memory area.
  __ mov(r0, Operand(address_of_regexp_stack_memory_address));
  __ ldr(r0, MemOperand(r0, 0));
  __ mov(r2, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r2, MemOperand(r2, 0));
  __ add(r0, r0, Operand(r2));
  __ str(r0, MemOperand(sp, 1 * kPointerSize));

  // Argument 5 (sp[0]): static offsets vector buffer.
  __ mov(r0, Operand(ExternalReference::address_of_static_offsets_vector()));
  __ str(r0, MemOperand(sp, 0 * kPointerSize));

  // For arguments 4 and 3 get string length, calculate start of string data and
  // calculate the shift of the index (0 for ASCII and 1 for two byte).
  __ ldr(r0, FieldMemOperand(subject, String::kLengthOffset));
  __ mov(r0, Operand(r0, ASR, kSmiTagSize));
  STATIC_ASSERT(SeqAsciiString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ add(r9, subject, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ eor(r3, r3, Operand(1));
  // Argument 4 (r3): End of string data
  // Argument 3 (r2): Start of string data
  __ add(r2, r9, Operand(r1, LSL, r3));
  __ add(r3, r9, Operand(r0, LSL, r3));

  // Argument 2 (r1): Previous index.
  // Already there

  // Argument 1 (r0): Subject string.
  __ mov(r0, subject);

  // Locate the code entry and call it.
  __ add(r7, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ CallCFunction(r7, kRegExpExecuteArguments);
  __ pop(lr);

  // r0: result
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)

  // Check the result.
  Label success;
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::SUCCESS));
  __ b(eq, &success);
  Label failure;
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ b(eq, &failure);
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ b(ne, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r0, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r0, MemOperand(r0, 0));
  __ mov(r1, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ ldr(r1, MemOperand(r1, 0));
  __ cmp(r0, r1);
  __ b(eq, &runtime);
  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r0, Operand(Factory::null_value()));
  __ add(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ ldr(r1,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(r1, r1, Operand(2));  // r1 was a smi.

  // r1: number of capture registers
  // r4: subject string
  // Store the capture count.
  __ mov(r2, Operand(r1, LSL, kSmiTagSize + kSmiShiftSize));  // To smi.
  __ str(r2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ mov(r3, last_match_info_elements);  // Moved up to reduce latency.
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ RecordWrite(r3, Operand(RegExpImpl::kLastSubjectOffset), r2, r7);
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ mov(r3, last_match_info_elements);
  __ RecordWrite(r3, Operand(RegExpImpl::kLastInputOffset), r2, r7);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector();
  __ mov(r2, Operand(address_of_static_offsets_vector));

  // r1: number of capture registers
  // r2: offsets vector
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ add(r0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ sub(r1, r1, Operand(1), SetCC);
  __ b(mi, &done);
  // Read the value from the static offsets vector buffer.
  __ ldr(r3, MemOperand(r2, kPointerSize, PostIndex));
  // Store the smi value in the last match info.
  __ mov(r3, Operand(r3, LSL, kSmiTagSize));
  __ str(r3, MemOperand(r0, kPointerSize, PostIndex));
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ add(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#endif  // V8_INTERPRETED_REGEXP
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // function, receiver [, arguments]
    Label receiver_is_value, receiver_is_js_object;
    __ ldr(r1, MemOperand(sp, argc_ * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ BranchOnSmi(r1, &receiver_is_value);

    // Check if the receiver is a valid JS object.
    __ CompareObjectType(r1, r2, r2, FIRST_JS_OBJECT_TYPE);
    __ b(ge, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(r1);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
    __ LeaveInternalFrame();
    __ str(r0, MemOperand(sp, argc_ * kPointerSize));

    __ bind(&receiver_is_js_object);
  }

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
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ str(r1, MemOperand(sp, argc_ * kPointerSize));
  __ mov(r0, Operand(argc_));  // Setup the number of arguments.
  __ mov(r2, Operand(0));
  __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
  __ Jump(Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline)),
          RelocInfo::CODE_TARGET);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
const char* CompareStub::GetName() {
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));

  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";

  const char* cc_name;
  switch (cc_) {
    case lt: cc_name = "LT"; break;
    case gt: cc_name = "GT"; break;
    case le: cc_name = "LE"; break;
    case ge: cc_name = "GE"; break;
    case eq: cc_name = "EQ"; break;
    case ne: cc_name = "NE"; break;
    default: cc_name = "UnknownCondition"; break;
  }

  const char* lhs_name = lhs_.is(r0) ? "_r0" : "_r1";
  const char* rhs_name = rhs_.is(r0) ? "_r0" : "_r1";

  const char* strict_name = "";
  if (strict_ && (cc_ == eq || cc_ == ne)) {
    strict_name = "_STRICT";
  }

  const char* never_nan_nan_name = "";
  if (never_nan_nan_ && (cc_ == eq || cc_ == ne)) {
    never_nan_nan_name = "_NO_NAN";
  }

  const char* include_number_compare_name = "";
  if (!include_number_compare_) {
    include_number_compare_name = "_NO_NUMBER";
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "CompareStub_%s%s%s%s%s%s",
               cc_name,
               lhs_name,
               rhs_name,
               strict_name,
               never_nan_nan_name,
               include_number_compare_name);
  return name_;
}


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value. To avoid duplicate
  // stubs the never NaN NaN condition is only taken into account if the
  // condition is equals.
  ASSERT((static_cast<unsigned>(cc_) >> 28) < (1 << 12));
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));
  return ConditionField::encode(static_cast<unsigned>(cc_) >> 28)
         | RegisterField::encode(lhs_.is(r0))
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == eq ? never_nan_nan_ : false)
         | IncludeNumberCompareField::encode(include_number_compare_);
}


// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;

  // If the receiver is a smi trigger the non-string case.
  __ BranchOnSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ tst(result_, Operand(kIsNotStringMask));
  __ b(ne, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  __ BranchOnNotSmi(index_, &index_not_smi_);

  // Put smi-tagged index into scratch register.
  __ mov(scratch_, index_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ ldr(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ cmp(ip, Operand(scratch_));
  __ b(ls, index_out_of_range_);

  // We need special handling for non-flat strings.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result_, Operand(kStringRepresentationMask));
  __ b(eq, &flat_string);

  // Handle non-flat strings.
  __ tst(result_, Operand(kIsConsStringMask));
  __ b(eq, &call_runtime_);

  // ConsString.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ ldr(result_, FieldMemOperand(object_, ConsString::kSecondOffset));
  __ LoadRoot(ip, Heap::kEmptyStringRootIndex);
  __ cmp(result_, Operand(ip));
  __ b(ne, &call_runtime_);
  // Get the first of the two strings and load its instance type.
  __ ldr(object_, FieldMemOperand(object_, ConsString::kFirstOffset));
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the first cons component is also non-flat, then go to runtime.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result_, Operand(kStringRepresentationMask));
  __ b(nz, &call_runtime_);

  // Check for 1-byte or 2-byte string.
  __ bind(&flat_string);
  STATIC_ASSERT(kAsciiStringTag != 0);
  __ tst(result_, Operand(kStringEncodingMask));
  __ b(nz, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the result register. We can
  // add without shifting since the smi tag size is the log2 of the
  // number of bytes in a two-byte character.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1 && kSmiShiftSize == 0);
  __ add(scratch_, object_, Operand(scratch_));
  __ ldrh(result_, FieldMemOperand(scratch_, SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);
  __ add(scratch_, object_, Operand(scratch_, LSR, kSmiTagSize));
  __ ldrb(result_, FieldMemOperand(scratch_, SeqAsciiString::kHeaderSize));

  __ bind(&got_char_code);
  __ mov(result_, Operand(result_, LSL, kSmiTagSize));
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharCodeAt slow case");

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              scratch_,
              Heap::kHeapNumberMapRootIndex,
              index_not_number_,
              true);
  call_helper.BeforeCall(masm);
  __ Push(object_, index_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  if (!scratch_.is(r0)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ mov(scratch_, r0);
  }
  __ pop(index_);
  __ pop(object_);
  // Reload the instance type.
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ BranchOnNotSmi(scratch_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  if (!result_.is(r0)) {
    __ mov(result_, r0);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  ASSERT(IsPowerOf2(String::kMaxAsciiCharCode + 1));
  __ tst(code_,
         Operand(kSmiTagMask |
                 ((~String::kMaxAsciiCharCode) << kSmiTagSize)));
  __ b(nz, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged ascii char code.
  STATIC_ASSERT(kSmiTag == 0);
  __ add(result_, result_, Operand(code_, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result_, Operand(ip));
  __ b(eq, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  if (!result_.is(r0)) {
    __ mov(result_, r0);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharFromCode slow case");
}


// -------------------------------------------------------------------------
// StringCharAtGenerator

void StringCharAtGenerator::GenerateFast(MacroAssembler* masm) {
  char_code_at_generator_.GenerateFast(masm);
  char_from_code_generator_.GenerateFast(masm);
}


void StringCharAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  char_code_at_generator_.GenerateSlow(masm, call_helper);
  char_from_code_generator_.GenerateSlow(masm, call_helper);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          bool ascii) {
  Label loop;
  Label done;
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (!ascii) {
    __ add(count, count, Operand(count), SetCC);
  } else {
    __ cmp(count, Operand(0));
  }
  __ b(eq, &done);

  __ bind(&loop);
  __ ldrb(scratch, MemOperand(src, 1, PostIndex));
  // Perform sub between load and dependent store to get the load time to
  // complete.
  __ sub(count, count, Operand(1), SetCC);
  __ strb(scratch, MemOperand(dest, 1, PostIndex));
  // last iteration.
  __ b(gt, &loop);

  __ bind(&done);
}


enum CopyCharactersFlags {
  COPY_ASCII = 1,
  DEST_ALWAYS_ALIGNED = 2
};


void StringHelper::GenerateCopyCharactersLong(MacroAssembler* masm,
                                              Register dest,
                                              Register src,
                                              Register count,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4,
                                              Register scratch5,
                                              int flags) {
  bool ascii = (flags & COPY_ASCII) != 0;
  bool dest_always_aligned = (flags & DEST_ALWAYS_ALIGNED) != 0;

  if (dest_always_aligned && FLAG_debug_code) {
    // Check that destination is actually word aligned if the flag says
    // that it is.
    __ tst(dest, Operand(kPointerAlignmentMask));
    __ Check(eq, "Destination of copy not aligned.");
  }

  const int kReadAlignment = 4;
  const int kReadAlignmentMask = kReadAlignment - 1;
  // Ensure that reading an entire aligned word containing the last character
  // of a string will not read outside the allocated area (because we pad up
  // to kObjectAlignment).
  STATIC_ASSERT(kObjectAlignment >= kReadAlignment);
  // Assumes word reads and writes are little endian.
  // Nothing to do for zero characters.
  Label done;
  if (!ascii) {
    __ add(count, count, Operand(count), SetCC);
  } else {
    __ cmp(count, Operand(0));
  }
  __ b(eq, &done);

  // Assume that you cannot read (or write) unaligned.
  Label byte_loop;
  // Must copy at least eight bytes, otherwise just do it one byte at a time.
  __ cmp(count, Operand(8));
  __ add(count, dest, Operand(count));
  Register limit = count;  // Read until src equals this.
  __ b(lt, &byte_loop);

  if (!dest_always_aligned) {
    // Align dest by byte copying. Copies between zero and three bytes.
    __ and_(scratch4, dest, Operand(kReadAlignmentMask), SetCC);
    Label dest_aligned;
    __ b(eq, &dest_aligned);
    __ cmp(scratch4, Operand(2));
    __ ldrb(scratch1, MemOperand(src, 1, PostIndex));
    __ ldrb(scratch2, MemOperand(src, 1, PostIndex), le);
    __ ldrb(scratch3, MemOperand(src, 1, PostIndex), lt);
    __ strb(scratch1, MemOperand(dest, 1, PostIndex));
    __ strb(scratch2, MemOperand(dest, 1, PostIndex), le);
    __ strb(scratch3, MemOperand(dest, 1, PostIndex), lt);
    __ bind(&dest_aligned);
  }

  Label simple_loop;

  __ sub(scratch4, dest, Operand(src));
  __ and_(scratch4, scratch4, Operand(0x03), SetCC);
  __ b(eq, &simple_loop);
  // Shift register is number of bits in a source word that
  // must be combined with bits in the next source word in order
  // to create a destination word.

  // Complex loop for src/dst that are not aligned the same way.
  {
    Label loop;
    __ mov(scratch4, Operand(scratch4, LSL, 3));
    Register left_shift = scratch4;
    __ and_(src, src, Operand(~3));  // Round down to load previous word.
    __ ldr(scratch1, MemOperand(src, 4, PostIndex));
    // Store the "shift" most significant bits of scratch in the least
    // signficant bits (i.e., shift down by (32-shift)).
    __ rsb(scratch2, left_shift, Operand(32));
    Register right_shift = scratch2;
    __ mov(scratch1, Operand(scratch1, LSR, right_shift));

    __ bind(&loop);
    __ ldr(scratch3, MemOperand(src, 4, PostIndex));
    __ sub(scratch5, limit, Operand(dest));
    __ orr(scratch1, scratch1, Operand(scratch3, LSL, left_shift));
    __ str(scratch1, MemOperand(dest, 4, PostIndex));
    __ mov(scratch1, Operand(scratch3, LSR, right_shift));
    // Loop if four or more bytes left to copy.
    // Compare to eight, because we did the subtract before increasing dst.
    __ sub(scratch5, scratch5, Operand(8), SetCC);
    __ b(ge, &loop);
  }
  // There is now between zero and three bytes left to copy (negative that
  // number is in scratch5), and between one and three bytes already read into
  // scratch1 (eight times that number in scratch4). We may have read past
  // the end of the string, but because objects are aligned, we have not read
  // past the end of the object.
  // Find the minimum of remaining characters to move and preloaded characters
  // and write those as bytes.
  __ add(scratch5, scratch5, Operand(4), SetCC);
  __ b(eq, &done);
  __ cmp(scratch4, Operand(scratch5, LSL, 3), ne);
  // Move minimum of bytes read and bytes left to copy to scratch4.
  __ mov(scratch5, Operand(scratch4, LSR, 3), LeaveCC, lt);
  // Between one and three (value in scratch5) characters already read into
  // scratch ready to write.
  __ cmp(scratch5, Operand(2));
  __ strb(scratch1, MemOperand(dest, 1, PostIndex));
  __ mov(scratch1, Operand(scratch1, LSR, 8), LeaveCC, ge);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex), ge);
  __ mov(scratch1, Operand(scratch1, LSR, 8), LeaveCC, gt);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex), gt);
  // Copy any remaining bytes.
  __ b(&byte_loop);

  // Simple loop.
  // Copy words from src to dst, until less than four bytes left.
  // Both src and dest are word aligned.
  __ bind(&simple_loop);
  {
    Label loop;
    __ bind(&loop);
    __ ldr(scratch1, MemOperand(src, 4, PostIndex));
    __ sub(scratch3, limit, Operand(dest));
    __ str(scratch1, MemOperand(dest, 4, PostIndex));
    // Compare to 8, not 4, because we do the substraction before increasing
    // dest.
    __ cmp(scratch3, Operand(8));
    __ b(ge, &loop);
  }

  // Copy bytes from src to dst until dst hits limit.
  __ bind(&byte_loop);
  __ cmp(dest, Operand(limit));
  __ ldrb(scratch1, MemOperand(src, 1, PostIndex), lt);
  __ b(ge, &done);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex));
  __ b(&byte_loop);

  __ bind(&done);
}


void StringHelper::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                        Register c1,
                                                        Register c2,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4,
                                                        Register scratch5,
                                                        Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the symbol table.
  Label not_array_index;
  __ sub(scratch, c1, Operand(static_cast<int>('0')));
  __ cmp(scratch, Operand(static_cast<int>('9' - '0')));
  __ b(hi, &not_array_index);
  __ sub(scratch, c2, Operand(static_cast<int>('0')));
  __ cmp(scratch, Operand(static_cast<int>('9' - '0')));

  // If check failed combine both characters into single halfword.
  // This is required by the contract of the method: code at the
  // not_found branch expects this combination in c1 register
  __ orr(c1, c1, Operand(c2, LSL, kBitsPerByte), LeaveCC, ls);
  __ b(ls, not_found);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  StringHelper::GenerateHashInit(masm, hash, c1);
  StringHelper::GenerateHashAddCharacter(masm, hash, c2);
  StringHelper::GenerateHashGetHash(masm, hash);

  // Collect the two characters in a register.
  Register chars = c1;
  __ orr(chars, chars, Operand(c2, LSL, kBitsPerByte));

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load symbol table
  // Load address of first element of the symbol table.
  Register symbol_table = c2;
  __ LoadRoot(symbol_table, Heap::kSymbolTableRootIndex);

  // Load undefined value
  Register undefined = scratch4;
  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ ldr(mask, FieldMemOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ mov(mask, Operand(mask, ASR, 1));
  __ sub(mask, mask, Operand(1));

  // Calculate untagged address of the first element of the symbol table.
  Register first_symbol_table_element = symbol_table;
  __ add(first_symbol_table_element, symbol_table,
         Operand(SymbolTable::kElementsStartOffset - kHeapObjectTag));

  // Registers
  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string
  // mask:  capacity mask
  // first_symbol_table_element: address of the first element of
  //                             the symbol table
  // scratch: -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes];
  for (int i = 0; i < kProbes; i++) {
    Register candidate = scratch5;  // Scratch register contains candidate.

    // Calculate entry in symbol table.
    if (i > 0) {
      __ add(candidate, hash, Operand(SymbolTable::GetProbeOffset(i)));
    } else {
      __ mov(candidate, hash);
    }

    __ and_(candidate, candidate, Operand(mask));

    // Load the entry from the symble table.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ ldr(candidate,
           MemOperand(first_symbol_table_element,
                      candidate,
                      LSL,
                      kPointerSizeLog2));

    // If entry is undefined no string with this hash can be found.
    __ cmp(candidate, undefined);
    __ b(eq, not_found);

    // If length is not 2 the string is not a candidate.
    __ ldr(scratch, FieldMemOperand(candidate, String::kLengthOffset));
    __ cmp(scratch, Operand(Smi::FromInt(2)));
    __ b(ne, &next_probe[i]);

    // Check that the candidate is a non-external ascii string.
    __ ldr(scratch, FieldMemOperand(candidate, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(scratch, scratch,
                                              &next_probe[i]);

    // Check if the two characters match.
    // Assumes that word load is little endian.
    __ ldrh(scratch, FieldMemOperand(candidate, SeqAsciiString::kHeaderSize));
    __ cmp(chars, scratch);
    __ b(eq, &found_in_symbol_table);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = scratch;
  __ bind(&found_in_symbol_table);
  __ Move(r0, result);
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character) {
  // hash = character + (character << 10);
  __ add(hash, character, Operand(character, LSL, 10));
  // hash ^= hash >> 6;
  __ eor(hash, hash, Operand(hash, ASR, 6));
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character) {
  // hash += character;
  __ add(hash, hash, Operand(character));
  // hash += hash << 10;
  __ add(hash, hash, Operand(hash, LSL, 10));
  // hash ^= hash >> 6;
  __ eor(hash, hash, Operand(hash, ASR, 6));
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash) {
  // hash += hash << 3;
  __ add(hash, hash, Operand(hash, LSL, 3));
  // hash ^= hash >> 11;
  __ eor(hash, hash, Operand(hash, ASR, 11));
  // hash += hash << 15;
  __ add(hash, hash, Operand(hash, LSL, 15), SetCC);

  // if (hash == 0) hash = 27;
  __ mov(hash, Operand(27), LeaveCC, nz);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  lr: return address
  //  sp[0]: to
  //  sp[4]: from
  //  sp[8]: string

  // This stub is called from the native-call %_SubString(...), so
  // nothing can be assumed about the arguments. It is tested that:
  //  "string" is a sequential string,
  //  both "from" and "to" are smis, and
  //  0 <= from <= to <= string.length.
  // If any of these assumptions fail, we call the runtime system.

  static const int kToOffset = 0 * kPointerSize;
  static const int kFromOffset = 1 * kPointerSize;
  static const int kStringOffset = 2 * kPointerSize;


  // Check bounds and smi-ness.
  __ ldr(r7, MemOperand(sp, kToOffset));
  __ ldr(r6, MemOperand(sp, kFromOffset));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  // I.e., arithmetic shift right by one un-smi-tags.
  __ mov(r2, Operand(r7, ASR, 1), SetCC);
  __ mov(r3, Operand(r6, ASR, 1), SetCC, cc);
  // If either r2 or r6 had the smi tag bit set, then carry is set now.
  __ b(cs, &runtime);  // Either "from" or "to" is not a smi.
  __ b(mi, &runtime);  // From is negative.

  __ sub(r2, r2, Operand(r3), SetCC);
  __ b(mi, &runtime);  // Fail if from > to.
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ cmp(r2, Operand(2));
  __ b(lt, &runtime);

  // r2: length
  // r3: from index (untaged smi)
  // r6: from (smi)
  // r7: to (smi)

  // Make sure first argument is a sequential (or flat) string.
  __ ldr(r5, MemOperand(sp, kStringOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r5, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  Condition is_string = masm->IsObjectStringType(r5, r1);
  __ b(NegateCondition(is_string), &runtime);

  // r1: instance type
  // r2: length
  // r3: from index (untaged smi)
  // r5: string
  // r6: from (smi)
  // r7: to (smi)
  Label seq_string;
  __ and_(r4, r1, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kSeqStringTag < kConsStringTag);
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  __ cmp(r4, Operand(kConsStringTag));
  __ b(gt, &runtime);  // External strings go to runtime.
  __ b(lt, &seq_string);  // Sequential strings are handled directly.

  // Cons string. Try to recurse (once) on the first substring.
  // (This adds a little more generality than necessary to handle flattened
  // cons strings, but not much).
  __ ldr(r5, FieldMemOperand(r5, ConsString::kFirstOffset));
  __ ldr(r4, FieldMemOperand(r5, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ tst(r1, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kSeqStringTag == 0);
  __ b(ne, &runtime);  // Cons and External strings go to runtime.

  // Definitly a sequential string.
  __ bind(&seq_string);

  // r1: instance type.
  // r2: length
  // r3: from index (untaged smi)
  // r5: string
  // r6: from (smi)
  // r7: to (smi)
  __ ldr(r4, FieldMemOperand(r5, String::kLengthOffset));
  __ cmp(r4, Operand(r7));
  __ b(lt, &runtime);  // Fail if to > length.

  // r1: instance type.
  // r2: result string length.
  // r3: from index (untaged smi)
  // r5: string.
  // r6: from offset (smi)
  // Check for flat ascii string.
  Label non_ascii_flat;
  __ tst(r1, Operand(kStringEncodingMask));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ b(eq, &non_ascii_flat);

  Label result_longer_than_two;
  __ cmp(r2, Operand(2));
  __ b(gt, &result_longer_than_two);

  // Sub string of length 2 requested.
  // Get the two characters forming the sub string.
  __ add(r5, r5, Operand(r3));
  __ ldrb(r3, FieldMemOperand(r5, SeqAsciiString::kHeaderSize));
  __ ldrb(r4, FieldMemOperand(r5, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, r3, r4, r1, r5, r6, r7, r9, &make_two_character_string);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // r2: result string length.
  // r3: two characters combined into halfword in little endian byte order.
  __ bind(&make_two_character_string);
  __ AllocateAsciiString(r0, r2, r4, r5, r9, &runtime);
  __ strh(r3, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&result_longer_than_two);

  // Allocate the result.
  __ AllocateAsciiString(r0, r2, r3, r4, r1, &runtime);

  // r0: result string.
  // r2: result string length.
  // r5: string.
  // r6: from offset (smi)
  // Locate first character of result.
  __ add(r1, r0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate 'from' character of string.
  __ add(r5, r5, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ add(r5, r5, Operand(r6, ASR, 1));

  // r0: result string.
  // r1: first character of result string.
  // r2: result string length.
  // r5: first character of sub string to copy.
  STATIC_ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(masm, r1, r5, r2, r3, r4, r6, r7, r9,
                                           COPY_ASCII | DEST_ALWAYS_ALIGNED);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii_flat);
  // r2: result string length.
  // r5: string.
  // r6: from offset (smi)
  // Check for flat two byte string.

  // Allocate the result.
  __ AllocateTwoByteString(r0, r2, r1, r3, r4, &runtime);

  // r0: result string.
  // r2: result string length.
  // r5: string.
  // Locate first character of result.
  __ add(r1, r0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate 'from' character of string.
    __ add(r5, r5, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // As "from" is a smi it is 2 times the value which matches the size of a two
  // byte character.
  __ add(r5, r5, Operand(r6));

  // r0: result string.
  // r1: first character of result.
  // r2: result length.
  // r5: first character of string to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(masm, r1, r5, r2, r3, r4, r6, r7, r9,
                                           DEST_ALWAYS_ALIGNED);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  Label compare_lengths;
  // Find minimum length and length difference.
  __ ldr(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ ldr(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ sub(scratch3, scratch1, Operand(scratch2), SetCC);
  Register length_delta = scratch3;
  __ mov(scratch1, scratch2, LeaveCC, gt);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(min_length, Operand(min_length));
  __ b(eq, &compare_lengths);

  // Untag smi.
  __ mov(min_length, Operand(min_length, ASR, kSmiTagSize));

  // Setup registers so that we only need to increment one register
  // in the loop.
  __ add(scratch2, min_length,
         Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ add(left, left, Operand(scratch2));
  __ add(right, right, Operand(scratch2));
  // Registers left and right points to the min_length character of strings.
  __ rsb(min_length, min_length, Operand(-1));
  Register index = min_length;
  // Index starts at -min_length.

  {
    // Compare loop.
    Label loop;
    __ bind(&loop);
    // Compare characters.
    __ add(index, index, Operand(1), SetCC);
    __ ldrb(scratch2, MemOperand(left, index), ne);
    __ ldrb(scratch4, MemOperand(right, index), ne);
    // Skip to compare lengths with eq condition true.
    __ b(eq, &compare_lengths);
    __ cmp(scratch2, scratch4);
    __ b(eq, &loop);
    // Fallthrough with eq condition false.
  }
  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  ASSERT(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use zero length_delta as result.
  __ mov(r0, Operand(length_delta), SetCC, eq);
  // Fall through to here if characters compare not-equal.
  __ mov(r0, Operand(Smi::FromInt(GREATER)), LeaveCC, gt);
  __ mov(r0, Operand(Smi::FromInt(LESS)), LeaveCC, lt);
  __ Ret();
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[4]: left string
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));  // left
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));  // right

  Label not_same;
  __ cmp(r0, r1);
  __ b(ne, &not_same);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)));
  __ IncrementCounter(&Counters::string_compare_native, 1, r1, r2);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(r0, r1, r2, r3, &runtime);

  // Compare flat ascii strings natively. Remove arguments from stack first.
  __ IncrementCounter(&Counters::string_compare_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  GenerateCompareFlatAsciiStrings(masm, r0, r1, r2, r3, r4, r5);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime;
  // Stack on entry:
  // sp[0]: second argument.
  // sp[4]: first argument.

  // Load the two arguments.
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));  // First argument.
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (string_check_) {
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfEitherSmi(r0, r1, &string_add_runtime);
    // Load instance types.
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
    STATIC_ASSERT(kStringTag == 0);
    // If either is not a string, go to runtime.
    __ tst(r4, Operand(kIsNotStringMask));
    __ tst(r5, Operand(kIsNotStringMask), eq);
    __ b(ne, &string_add_runtime);
  }

  // Both arguments are strings.
  // r0: first string
  // r1: second string
  // r4: first string instance type (if string_check_)
  // r5: second string instance type (if string_check_)
  {
    Label strings_not_empty;
    // Check if either of the strings are empty. In that case return the other.
    __ ldr(r2, FieldMemOperand(r0, String::kLengthOffset));
    __ ldr(r3, FieldMemOperand(r1, String::kLengthOffset));
    STATIC_ASSERT(kSmiTag == 0);
    __ cmp(r2, Operand(Smi::FromInt(0)));  // Test if first string is empty.
    __ mov(r0, Operand(r1), LeaveCC, eq);  // If first is empty, return second.
    STATIC_ASSERT(kSmiTag == 0);
     // Else test if second string is empty.
    __ cmp(r3, Operand(Smi::FromInt(0)), ne);
    __ b(ne, &strings_not_empty);  // If either string was empty, return r0.

    __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
    __ add(sp, sp, Operand(2 * kPointerSize));
    __ Ret();

    __ bind(&strings_not_empty);
  }

  __ mov(r2, Operand(r2, ASR, kSmiTagSize));
  __ mov(r3, Operand(r3, ASR, kSmiTagSize));
  // Both strings are non-empty.
  // r0: first string
  // r1: second string
  // r2: length of first string
  // r3: length of second string
  // r4: first string instance type (if string_check_)
  // r5: second string instance type (if string_check_)
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  // Adding two lengths can't overflow.
  STATIC_ASSERT(String::kMaxLength < String::kMaxLength * 2);
  __ add(r6, r2, Operand(r3));
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ cmp(r6, Operand(2));
  __ b(ne, &longer_than_two);

  // Check that both strings are non-external ascii strings.
  if (!string_check_) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(r4, r5, r6, r7,
                                                  &string_add_runtime);

  // Get the two characters forming the sub string.
  __ ldrb(r2, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ ldrb(r3, FieldMemOperand(r1, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, r2, r3, r6, r7, r4, r5, r9, &make_two_character_string);
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&make_two_character_string);
  // Resulting string has length 2 and first chars of two strings
  // are combined into single halfword in r2 register.
  // So we can fill resulting string without two loops by a single
  // halfword store instruction (which assumes that processor is
  // in a little endian mode)
  __ mov(r6, Operand(2));
  __ AllocateAsciiString(r0, r6, r4, r5, r9, &string_add_runtime);
  __ strh(r2, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ cmp(r6, Operand(String::kMinNonFlatLength));
  __ b(lt, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  ASSERT(IsPowerOf2(String::kMaxLength + 1));
  // kMaxLength + 1 is representable as shifted literal, kMaxLength is not.
  __ cmp(r6, Operand(String::kMaxLength + 1));
  __ b(hs, &string_add_runtime);

  // If result is not supposed to be flat, allocate a cons string object.
  // If both strings are ascii the result is an ascii cons string.
  if (!string_check_) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  Label non_ascii, allocated, ascii_data;
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ tst(r4, Operand(kStringEncodingMask));
  __ tst(r5, Operand(kStringEncodingMask), ne);
  __ b(eq, &non_ascii);

  // Allocate an ASCII cons string.
  __ bind(&ascii_data);
  __ AllocateAsciiConsString(r7, r6, r4, r5, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ str(r0, FieldMemOperand(r7, ConsString::kFirstOffset));
  __ str(r1, FieldMemOperand(r7, ConsString::kSecondOffset));
  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ascii characters.
  // r4: first instance type.
  // r5: second instance type.
  __ tst(r4, Operand(kAsciiDataHintMask));
  __ tst(r5, Operand(kAsciiDataHintMask), ne);
  __ b(ne, &ascii_data);
  __ eor(r4, r4, Operand(r5));
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ and_(r4, r4, Operand(kAsciiStringTag | kAsciiDataHintTag));
  __ cmp(r4, Operand(kAsciiStringTag | kAsciiDataHintTag));
  __ b(eq, &ascii_data);

  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(r7, r6, r4, r5, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are
  // sequential and that they have the same encoding.
  // r0: first string
  // r1: second string
  // r2: length of first string
  // r3: length of second string
  // r4: first string instance type (if string_check_)
  // r5: second string instance type (if string_check_)
  // r6: sum of lengths.
  __ bind(&string_add_flat_result);
  if (!string_check_) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  // Check that both strings are sequential.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(r4, Operand(kStringRepresentationMask));
  __ tst(r5, Operand(kStringRepresentationMask), eq);
  __ b(ne, &string_add_runtime);
  // Now check if both strings have the same encoding (ASCII/Two-byte).
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: sum of lengths..
  Label non_ascii_string_add_flat_result;
  ASSERT(IsPowerOf2(kStringEncodingMask));  // Just one bit to test.
  __ eor(r7, r4, Operand(r5));
  __ tst(r7, Operand(kStringEncodingMask));
  __ b(ne, &string_add_runtime);
  // And see if it's ASCII or two-byte.
  __ tst(r4, Operand(kStringEncodingMask));
  __ b(eq, &non_ascii_string_add_flat_result);

  // Both strings are sequential ASCII strings. We also know that they are
  // short (since the sum of the lengths is less than kMinNonFlatLength).
  // r6: length of resulting flat string
  __ AllocateAsciiString(r7, r6, r4, r5, r9, &string_add_runtime);
  // Locate first character of result.
  __ add(r6, r7, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ add(r0, r0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // r0: first character of first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: first character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r0, r2, r4, true);

  // Load second argument and locate first character.
  __ add(r1, r1, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // r1: first character of second string.
  // r3: length of second string.
  // r6: next character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r1, r3, r4, true);
  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are sequential two byte strings.
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: sum of length of strings.
  __ AllocateTwoByteString(r7, r6, r4, r5, r9, &string_add_runtime);
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r7: result string.

  // Locate first character of result.
  __ add(r6, r7, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ add(r0, r0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r0: first character of first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: first character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r0, r2, r4, false);

  // Locate first character of second argument.
  __ add(r1, r1, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r1: first character of second string.
  // r3: length of second string.
  // r6: next character of result (after copy of first string).
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r1, r3, r4, false);

  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
  __ TailCallRuntime(Runtime::kStringAdd, 2, 1);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
