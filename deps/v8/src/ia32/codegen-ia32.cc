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
#include "jsregexp.h"
#include "parser.h"
#include "regexp-macro-assembler.h"
#include "regexp-stack.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.

void DeferredCode::SaveRegisters() {
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    int action = registers_[i];
    if (action == kPush) {
      __ push(RegisterAllocator::ToRegister(i));
    } else if (action != kIgnore && (action & kSyncedFlag) == 0) {
      __ mov(Operand(ebp, action), RegisterAllocator::ToRegister(i));
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
      __ mov(RegisterAllocator::ToRegister(i), Operand(ebp, action));
    }
  }
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
// CodeGenerator implementation

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
// ebp: caller's frame pointer
// esp: stack pointer
// edi: called JS function
// esi: callee's context

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
  set_in_spilled_code(false);

  // Adjust for function-level loop nesting.
  loop_nesting_ += info->loop_nesting();

  JumpTarget::set_compiling_deferred_code(false);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    frame_->SpillAll();
    __ int3();
  }
#endif

  // New scope to get automatic timing calculation.
  {  // NOLINT
    HistogramTimerScope codegen_timer(&Counters::code_generation);
    CodeGenState state(this);

    // Entry:
    // Stack: receiver, arguments, return address.
    // ebp: caller's frame pointer
    // esp: stack pointer
    // edi: called JS function
    // esi: callee's context
    allocator_->Initialize();

    if (info->mode() == CompilationInfo::PRIMARY) {
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

        // Verify that the runtime call result and esi agree.
        if (FLAG_debug_code) {
          __ cmp(context.reg(), Operand(esi));
          __ Assert(equal, "Runtime::NewContext should end up in esi");
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
            __ mov(SlotOperand(slot, context.reg()), value.reg());
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
    } else {
      // When used as the secondary compiler for splitting, ebp, esi,
      // and edi have been pushed on the stack.  Adjust the virtual
      // frame to match this state.
      frame_->Adjust(3);
      allocator_->Unuse(edi);

      // Bind all the bailout labels to the beginning of the function.
      List<CompilationInfo::Bailout*>* bailouts = info->bailouts();
      for (int i = 0; i < bailouts->length(); i++) {
        __ bind(bailouts->at(i)->label());
      }
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
  loop_nesting_ -= info->loop_nesting();

  // Code generation state must be reset.
  ASSERT(state_ == NULL);
  ASSERT(loop_nesting() == 0);
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
      ASSERT(!tmp.is(esi));  // do not overwrite context register
      Register context = esi;
      int chain_length = scope()->ContextChainLength(slot->var()->scope());
      for (int i = 0; i < chain_length; i++) {
        // Load the closure.
        // (All contexts, even 'with' contexts, have a closure,
        // and it is the same for all contexts inside a function.
        // There is no need to go to the function context first.)
        __ mov(tmp, ContextOperand(context, Context::CLOSURE_INDEX));
        // Load the function context (which is the incoming, outer context).
        __ mov(tmp, FieldOperand(tmp, JSFunction::kContextOffset));
        context = tmp;
      }
      // We may have a 'with' context now. Get the function context.
      // (In fact this mov may never be the needed, since the scope analysis
      // may not permit a direct context access in this case and thus we are
      // always at a function context. However it is safe to dereference be-
      // cause the function context of a function context is itself. Before
      // deleting this mov we should try to create a counter-example first,
      // though...)
      __ mov(tmp, ContextOperand(context, Context::FCONTEXT_INDEX));
      return ContextOperand(tmp, index);
    }

    default:
      UNREACHABLE();
      return Operand(eax);
  }
}


Operand CodeGenerator::ContextSlotOperandCheckExtensions(Slot* slot,
                                                         Result tmp,
                                                         JumpTarget* slow) {
  ASSERT(slot->type() == Slot::CONTEXT);
  ASSERT(tmp.is_register());
  Register context = esi;

  for (Scope* s = scope(); s != slot->var()->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context, Context::EXTENSION_INDEX),
               Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      __ mov(tmp.reg(), ContextOperand(context, Context::CLOSURE_INDEX));
      __ mov(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp.reg();
    }
  }
  // Check that last extension is NULL.
  __ cmp(ContextOperand(context, Context::EXTENSION_INDEX), Immediate(0));
  slow->Branch(not_equal, not_taken);
  __ mov(tmp.reg(), ContextOperand(context, Context::FCONTEXT_INDEX));
  return ContextOperand(tmp.reg(), slot->index());
}


// Emit code to load the value of an expression to the top of the
// frame. If the expression is boolean-valued it may be compiled (or
// partially compiled) into control flow to the control destination.
// If force_control is true, control flow is forced.
void CodeGenerator::LoadCondition(Expression* x,
                                  ControlDestination* dest,
                                  bool force_control) {
  ASSERT(!in_spilled_code());
  int original_height = frame_->height();

  { CodeGenState new_state(this, dest);
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
    __ mov(temp.reg(), GlobalObject());
    frame_->Push(&temp);
  }
}


void CodeGenerator::LoadGlobalReceiver() {
  Result temp = allocator_->Allocate();
  Register reg = temp.reg();
  __ mov(reg, GlobalObject());
  __ mov(reg, FieldOperand(reg, GlobalObject::kGlobalReceiverOffset));
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
    Result result =
        LoadFromSlotCheckForArguments(variable->slot(), INSIDE_TYPEOF);
    frame()->Push(&result);
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
    // We have to skip storing into the arguments slot if it has already
    // been written to. This can happen if the a function has a local
    // variable named 'arguments'.
    Result probe = LoadFromSlot(arguments->slot(), NOT_INSIDE_TYPEOF);
    if (probe.is_constant()) {
      // We have to skip updating the arguments object if it has
      // been assigned a proper value.
      skip_arguments = !probe.handle()->IsTheHole();
    } else {
      __ cmp(Operand(probe.reg()), Immediate(Factory::the_hole_value()));
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
      // If eax is free, the register allocator prefers it.  Thus the code
      // generator will load the global object into eax, which is where
      // LoadIC wants it.  Most uses of Reference call LoadIC directly
      // after the reference is created.
      frame_->Spill(eax);
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
    Comment cmnt(masm_, "ONLY_NUMBER");
    // Fast case if NumberInfo indicates only numbers.
    if (FLAG_debug_code) {
      __ AbortIfNotNumber(value.reg(), "ToBoolean operand is not a number.");
    }
    // Smi => false iff zero.
    ASSERT(kSmiTag == 0);
    __ test(value.reg(), Operand(value.reg()));
    dest->false_target()->Branch(zero);
    __ test(value.reg(), Immediate(kSmiTagMask));
    dest->true_target()->Branch(zero);
    __ fldz();
    __ fld_d(FieldOperand(value.reg(), HeapNumber::kValueOffset));
    __ FCmp();
    value.Unuse();
    dest->Split(not_zero);
  } else {
    // Fast case checks.
    // 'false' => false.
    __ cmp(value.reg(), Factory::false_value());
    dest->false_target()->Branch(equal);

    // 'true' => true.
    __ cmp(value.reg(), Factory::true_value());
    dest->true_target()->Branch(equal);

    // 'undefined' => false.
    __ cmp(value.reg(), Factory::undefined_value());
    dest->false_target()->Branch(equal);

    // Smi => false iff zero.
    ASSERT(kSmiTag == 0);
    __ test(value.reg(), Operand(value.reg()));
    dest->false_target()->Branch(zero);
    __ test(value.reg(), Immediate(kSmiTagMask));
    dest->true_target()->Branch(zero);

    // Call the stub for all other cases.
    frame_->Push(&value);  // Undo the Pop() from above.
    ToBooleanStub stub;
    Result temp = frame_->CallStub(&stub, 1);
    // Convert the result to a condition code.
    __ test(temp.reg(), Operand(temp.reg()));
    temp.Unuse();
    dest->Split(not_equal);
  }
}


class FloatingPointHelper : public AllStatic {
 public:

  enum ArgLocation {
    ARGS_ON_STACK,
    ARGS_IN_REGISTERS
  };

  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in register number. Returns operand as floating point number
  // on FPU stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register number);
  // Code pattern for loading floating point values. Input values must
  // be either smi or heap number objects (fp values). Requirements:
  // operand_1 on TOS+1 or in edx, operand_2 on TOS+2 or in eax.
  // Returns operands as floating point numbers on FPU stack.
  static void LoadFloatOperands(MacroAssembler* masm,
                                Register scratch,
                                ArgLocation arg_location = ARGS_ON_STACK);

  // Similar to LoadFloatOperand but assumes that both operands are smis.
  // Expects operands in edx, eax.
  static void LoadFloatSmis(MacroAssembler* masm, Register scratch);

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);
  // Takes the operands in edx and eax and loads them as integers in eax
  // and ecx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             bool use_sse3,
                             Label* operand_conversion_failure);
  // Test if operands are smis or heap numbers and load them
  // into xmm0 and xmm1 if they are. Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm);
  // Test if operands are numbers (smi or HeapNumber objects), and load
  // them into xmm0 and xmm1 if they are.  Jump to label not_numbers if
  // either operand is not a number.  Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm, Label* not_numbers);

  // Similar to LoadSSE2Operands but assumes that both operands are smis.
  // Expects operands in edx, eax.
  static void LoadSSE2Smis(MacroAssembler* masm, Register scratch);
};


const char* GenericBinaryOpStub::GetName() {
  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "GenericBinaryOpStub_%s_%s%s_%s%s_%s",
               op_name,
               overwrite_name,
               (flags_ & NO_SMI_CODE_IN_STUB) ? "_NoSmiInStub" : "",
               args_in_registers_ ? "RegArgs" : "StackArgs",
               args_reversed_ ? "_R" : "",
               NumberInfo::ToString(operands_type_));
  return name_;
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
  if (CpuFeatures::IsSupported(SSE2) && ((op_ == Token::ADD) ||
      (op_ ==Token::SUB) ||
      (op_ == Token::MUL) ||
      (op_ == Token::DIV))) {
    CpuFeatures::Scope use_sse2(SSE2);
    Label call_runtime, after_alloc_failure;
    Label left_smi, right_smi, load_right, do_op;
    __ test(left_, Immediate(kSmiTagMask));
    __ j(zero, &left_smi);
    __ cmp(FieldOperand(left_, HeapObject::kMapOffset),
           Factory::heap_number_map());
    __ j(not_equal, &call_runtime);
    __ movdbl(xmm0, FieldOperand(left_, HeapNumber::kValueOffset));
    if (mode_ == OVERWRITE_LEFT) {
      __ mov(dst_, left_);
    }
    __ jmp(&load_right);

    __ bind(&left_smi);
    __ SmiUntag(left_);
    __ cvtsi2sd(xmm0, Operand(left_));
    __ SmiTag(left_);
    if (mode_ == OVERWRITE_LEFT) {
      Label alloc_failure;
      __ push(left_);
      __ AllocateHeapNumber(dst_, left_, no_reg, &after_alloc_failure);
      __ pop(left_);
    }

    __ bind(&load_right);
    __ test(right_, Immediate(kSmiTagMask));
    __ j(zero, &right_smi);
    __ cmp(FieldOperand(right_, HeapObject::kMapOffset),
           Factory::heap_number_map());
    __ j(not_equal, &call_runtime);
    __ movdbl(xmm1, FieldOperand(right_, HeapNumber::kValueOffset));
    if (mode_ == OVERWRITE_RIGHT) {
      __ mov(dst_, right_);
    } else if (mode_ == NO_OVERWRITE) {
      Label alloc_failure;
      __ push(left_);
      __ AllocateHeapNumber(dst_, left_, no_reg, &after_alloc_failure);
      __ pop(left_);
    }
    __ jmp(&do_op);

    __ bind(&right_smi);
    __ SmiUntag(right_);
    __ cvtsi2sd(xmm1, Operand(right_));
    __ SmiTag(right_);
    if (mode_ == OVERWRITE_RIGHT || mode_ == NO_OVERWRITE) {
      Label alloc_failure;
      __ push(left_);
      __ AllocateHeapNumber(dst_, left_, no_reg, &after_alloc_failure);
      __ pop(left_);
    }

    __ bind(&do_op);
    switch (op_) {
      case Token::ADD: __ addsd(xmm0, xmm1); break;
      case Token::SUB: __ subsd(xmm0, xmm1); break;
      case Token::MUL: __ mulsd(xmm0, xmm1); break;
      case Token::DIV: __ divsd(xmm0, xmm1); break;
      default: UNREACHABLE();
    }
    __ movdbl(FieldOperand(dst_, HeapNumber::kValueOffset), xmm0);
    __ jmp(&done);

    __ bind(&after_alloc_failure);
    __ pop(left_);
    __ bind(&call_runtime);
  }
  GenericBinaryOpStub stub(op_, mode_, NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, left_, right_);
  if (!dst_.is(eax)) __ mov(dst_, eax);
  __ bind(&done);
}


void CodeGenerator::GenericBinaryOperation(Token::Value op,
                                           StaticType* type,
                                           OverwriteMode overwrite_mode) {
  Comment cmnt(masm_, "[ BinaryOperation");
  Comment cmnt_token(masm_, Token::String(op));

  if (op == Token::COMMA) {
    // Simply discard left value.
    frame_->Nip(1);
    return;
  }

  Result right = frame_->Pop();
  Result left = frame_->Pop();

  if (op == Token::ADD) {
    bool left_is_string = left.is_constant() && left.handle()->IsString();
    bool right_is_string = right.is_constant() && right.handle()->IsString();
    if (left_is_string || right_is_string) {
      frame_->Push(&left);
      frame_->Push(&right);
      Result answer;
      if (left_is_string) {
        if (right_is_string) {
          // TODO(lrn): if both are constant strings
          // -- do a compile time cons, if allocation during codegen is allowed.
          answer = frame_->CallRuntime(Runtime::kStringAdd, 2);
        } else {
          answer =
            frame_->InvokeBuiltin(Builtins::STRING_ADD_LEFT, CALL_FUNCTION, 2);
        }
      } else if (right_is_string) {
        answer =
          frame_->InvokeBuiltin(Builtins::STRING_ADD_RIGHT, CALL_FUNCTION, 2);
      }
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
  NumberInfo::Type operands_type =
      NumberInfo::Combine(left.number_info(), right.number_info());

  Result answer;
  if (left_is_non_smi_constant || right_is_non_smi_constant) {
    // Go straight to the slow case, with no smi code.
    GenericBinaryOpStub stub(op,
                             overwrite_mode,
                             NO_SMI_CODE_IN_STUB,
                             operands_type);
    answer = stub.GenerateCall(masm_, frame_, &left, &right);
  } else if (right_is_smi_constant) {
    answer = ConstantSmiBinaryOperation(op, &left, right.handle(),
                                        type, false, overwrite_mode);
  } else if (left_is_smi_constant) {
    answer = ConstantSmiBinaryOperation(op, &right, left.handle(),
                                        type, true, overwrite_mode);
  } else {
    // Set the flags based on the operation, type and loop nesting level.
    // Bit operations always assume they likely operate on Smis. Still only
    // generate the inline Smi check code if this operation is part of a loop.
    // For all other operations only inline the Smi check code for likely smis
    // if the operation is part of a loop.
    if (loop_nesting() > 0 && (Token::IsBitOp(op) || type->IsLikelySmi())) {
      answer = LikelySmiBinaryOperation(op, &left, &right, overwrite_mode);
    } else {
      GenericBinaryOpStub stub(op,
                               overwrite_mode,
                               NO_GENERIC_BINARY_FLAGS,
                               operands_type);
      answer = stub.GenerateCall(masm_, frame_, &left, &right);
    }
  }

  // Set NumberInfo of result according to the operation performed.
  // Rely on the fact that smis have a 31 bit payload on ia32.
  ASSERT(kSmiValueSize == 31);
  NumberInfo::Type result_type = NumberInfo::kUnknown;
  switch (op) {
    case Token::COMMA:
      result_type = right.number_info();
      break;
    case Token::OR:
    case Token::AND:
      // Result type can be either of the two input types.
      result_type = operands_type;
      break;
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
      // Result is always a number. Smi property of inputs is preserved.
      result_type = (operands_type == NumberInfo::kSmi)
          ? NumberInfo::kSmi
          : NumberInfo::kNumber;
      break;
    case Token::SAR:
      // Result is a smi if we shift by a constant >= 1, otherwise a number.
      result_type = (right.is_constant() && right.handle()->IsSmi()
                     && Smi::cast(*right.handle())->value() >= 1)
          ? NumberInfo::kSmi
          : NumberInfo::kNumber;
      break;
    case Token::SHR:
      // Result is a smi if we shift by a constant >= 2, otherwise a number.
      result_type = (right.is_constant() && right.handle()->IsSmi()
                     && Smi::cast(*right.handle())->value() >= 2)
          ? NumberInfo::kSmi
          : NumberInfo::kNumber;
      break;
    case Token::ADD:
      // Result could be a string or a number. Check types of inputs.
      result_type = NumberInfo::IsNumber(operands_type)
          ? NumberInfo::kNumber
          : NumberInfo::kUnknown;
      break;
    case Token::SHL:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      // Result is always a number.
      result_type = NumberInfo::kNumber;
      break;
    default:
      UNREACHABLE();
  }
  answer.set_number_info(result_type);
  frame_->Push(&answer);
}


bool CodeGenerator::FoldConstantSmis(Token::Value op, int left, int right) {
  Object* answer_object = Heap::undefined_value();
  switch (op) {
    case Token::ADD:
      if (Smi::IsValid(left + right)) {
        answer_object = Smi::FromInt(left + right);
      }
      break;
    case Token::SUB:
      if (Smi::IsValid(left - right)) {
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
        ASSERT(Smi::IsValid(unsigned_left));  // Converted to signed.
        answer_object = Smi::FromInt(unsigned_left);  // Converted to signed.
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


// Implements a binary operation using a deferred code object and some
// inline code to operate on smis quickly.
Result CodeGenerator::LikelySmiBinaryOperation(Token::Value op,
                                               Result* left,
                                               Result* right,
                                               OverwriteMode overwrite_mode) {
  Result answer;
  // Special handling of div and mod because they use fixed registers.
  if (op == Token::DIV || op == Token::MOD) {
    // We need eax as the quotient register, edx as the remainder
    // register, neither left nor right in eax or edx, and left copied
    // to eax.
    Result quotient;
    Result remainder;
    bool left_is_in_eax = false;
    // Step 1: get eax for quotient.
    if ((left->is_register() && left->reg().is(eax)) ||
        (right->is_register() && right->reg().is(eax))) {
      // One or both is in eax.  Use a fresh non-edx register for
      // them.
      Result fresh = allocator_->Allocate();
      ASSERT(fresh.is_valid());
      if (fresh.reg().is(edx)) {
        remainder = fresh;
        fresh = allocator_->Allocate();
        ASSERT(fresh.is_valid());
      }
      if (left->is_register() && left->reg().is(eax)) {
        quotient = *left;
        *left = fresh;
        left_is_in_eax = true;
      }
      if (right->is_register() && right->reg().is(eax)) {
        quotient = *right;
        *right = fresh;
      }
      __ mov(fresh.reg(), eax);
    } else {
      // Neither left nor right is in eax.
      quotient = allocator_->Allocate(eax);
    }
    ASSERT(quotient.is_register() && quotient.reg().is(eax));
    ASSERT(!(left->is_register() && left->reg().is(eax)));
    ASSERT(!(right->is_register() && right->reg().is(eax)));

    // Step 2: get edx for remainder if necessary.
    if (!remainder.is_valid()) {
      if ((left->is_register() && left->reg().is(edx)) ||
          (right->is_register() && right->reg().is(edx))) {
        Result fresh = allocator_->Allocate();
        ASSERT(fresh.is_valid());
        if (left->is_register() && left->reg().is(edx)) {
          remainder = *left;
          *left = fresh;
        }
        if (right->is_register() && right->reg().is(edx)) {
          remainder = *right;
          *right = fresh;
        }
        __ mov(fresh.reg(), edx);
      } else {
        // Neither left nor right is in edx.
        remainder = allocator_->Allocate(edx);
      }
    }
    ASSERT(remainder.is_register() && remainder.reg().is(edx));
    ASSERT(!(left->is_register() && left->reg().is(edx)));
    ASSERT(!(right->is_register() && right->reg().is(edx)));

    left->ToRegister();
    right->ToRegister();
    frame_->Spill(eax);
    frame_->Spill(edx);

    // Check that left and right are smi tagged.
    DeferredInlineBinaryOperation* deferred =
        new DeferredInlineBinaryOperation(op,
                                          (op == Token::DIV) ? eax : edx,
                                          left->reg(),
                                          right->reg(),
                                          overwrite_mode);
    if (left->reg().is(right->reg())) {
      __ test(left->reg(), Immediate(kSmiTagMask));
    } else {
      // Use the quotient register as a scratch for the tag check.
      if (!left_is_in_eax) __ mov(eax, left->reg());
      left_is_in_eax = false;  // About to destroy the value in eax.
      __ or_(eax, Operand(right->reg()));
      ASSERT(kSmiTag == 0);  // Adjust test if not the case.
      __ test(eax, Immediate(kSmiTagMask));
    }
    deferred->Branch(not_zero);

    if (!left_is_in_eax) __ mov(eax, left->reg());
    // Sign extend eax into edx:eax.
    __ cdq();
    // Check for 0 divisor.
    __ test(right->reg(), Operand(right->reg()));
    deferred->Branch(zero);
    // Divide edx:eax by the right operand.
    __ idiv(right->reg());

    // Complete the operation.
    if (op == Token::DIV) {
      // Check for negative zero result.  If result is zero, and divisor
      // is negative, return a floating point negative zero.  The
      // virtual frame is unchanged in this block, so local control flow
      // can use a Label rather than a JumpTarget.
      Label non_zero_result;
      __ test(left->reg(), Operand(left->reg()));
      __ j(not_zero, &non_zero_result);
      __ test(right->reg(), Operand(right->reg()));
      deferred->Branch(negative);
      __ bind(&non_zero_result);
      // Check for the corner case of dividing the most negative smi by
      // -1. We cannot use the overflow flag, since it is not set by
      // idiv instruction.
      ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
      __ cmp(eax, 0x40000000);
      deferred->Branch(equal);
      // Check that the remainder is zero.
      __ test(edx, Operand(edx));
      deferred->Branch(not_zero);
      // Tag the result and store it in the quotient register.
      __ SmiTag(eax);
      deferred->BindExit();
      left->Unuse();
      right->Unuse();
      answer = quotient;
    } else {
      ASSERT(op == Token::MOD);
      // Check for a negative zero result.  If the result is zero, and
      // the dividend is negative, return a floating point negative
      // zero.  The frame is unchanged in this block, so local control
      // flow can use a Label rather than a JumpTarget.
      Label non_zero_result;
      __ test(edx, Operand(edx));
      __ j(not_zero, &non_zero_result, taken);
      __ test(left->reg(), Operand(left->reg()));
      deferred->Branch(negative);
      __ bind(&non_zero_result);
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
    // Move left out of ecx if necessary.
    if (left->is_register() && left->reg().is(ecx)) {
      *left = allocator_->Allocate();
      ASSERT(left->is_valid());
      __ mov(left->reg(), ecx);
    }
    right->ToRegister(ecx);
    left->ToRegister();
    ASSERT(left->is_register() && !left->reg().is(ecx));
    ASSERT(right->is_register() && right->reg().is(ecx));

    // We will modify right, it must be spilled.
    frame_->Spill(ecx);

    // Use a fresh answer register to avoid spilling the left operand.
    answer = allocator_->Allocate();
    ASSERT(answer.is_valid());
    // Check that both operands are smis using the answer register as a
    // temporary.
    DeferredInlineBinaryOperation* deferred =
        new DeferredInlineBinaryOperation(op,
                                          answer.reg(),
                                          left->reg(),
                                          ecx,
                                          overwrite_mode);
    __ mov(answer.reg(), left->reg());
    __ or_(answer.reg(), Operand(ecx));
    __ test(answer.reg(), Immediate(kSmiTagMask));
    deferred->Branch(not_zero);

    // Untag both operands.
    __ mov(answer.reg(), left->reg());
    __ SmiUntag(answer.reg());
    __ SmiUntag(ecx);
    // Perform the operation.
    switch (op) {
      case Token::SAR:
        __ sar_cl(answer.reg());
        // No checks of result necessary
        break;
      case Token::SHR: {
        Label result_ok;
        __ shr_cl(answer.reg());
        // Check that the *unsigned* result fits in a smi.  Neither of
        // the two high-order bits can be set:
        //  * 0x80000000: high bit would be lost when smi tagging.
        //  * 0x40000000: this number would convert to negative when smi
        //    tagging.
        // These two cases can only happen with shifts by 0 or 1 when
        // handed a valid smi.  If the answer cannot be represented by a
        // smi, restore the left and right arguments, and jump to slow
        // case.  The low bit of the left argument may be lost, but only
        // in a case where it is dropped anyway.
        __ test(answer.reg(), Immediate(0xc0000000));
        __ j(zero, &result_ok);
        __ SmiTag(ecx);
        deferred->Jump();
        __ bind(&result_ok);
        break;
      }
      case Token::SHL: {
        Label result_ok;
        __ shl_cl(answer.reg());
        // Check that the *signed* result fits in a smi.
        __ cmp(answer.reg(), 0xc0000000);
        __ j(positive, &result_ok);
        __ SmiTag(ecx);
        deferred->Jump();
        __ bind(&result_ok);
        break;
      }
      default:
        UNREACHABLE();
    }
    // Smi-tag the result in answer.
    __ SmiTag(answer.reg());
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
  if (left->reg().is(right->reg())) {
    __ test(left->reg(), Immediate(kSmiTagMask));
  } else {
    __ mov(answer.reg(), left->reg());
    __ or_(answer.reg(), Operand(right->reg()));
    ASSERT(kSmiTag == 0);  // Adjust test if not the case.
    __ test(answer.reg(), Immediate(kSmiTagMask));
  }
  deferred->Branch(not_zero);
  __ mov(answer.reg(), left->reg());
  switch (op) {
    case Token::ADD:
      __ add(answer.reg(), Operand(right->reg()));
      deferred->Branch(overflow);
      break;

    case Token::SUB:
      __ sub(answer.reg(), Operand(right->reg()));
      deferred->Branch(overflow);
      break;

    case Token::MUL: {
      // If the smi tag is 0 we can just leave the tag on one operand.
      ASSERT(kSmiTag == 0);  // Adjust code below if not the case.
      // Remove smi tag from the left operand (but keep sign).
      // Left-hand operand has been copied into answer.
      __ SmiUntag(answer.reg());
      // Do multiplication of smis, leaving result in answer.
      __ imul(answer.reg(), Operand(right->reg()));
      // Go slow on overflows.
      deferred->Branch(overflow);
      // Check for negative zero result.  If product is zero, and one
      // argument is negative, go to slow case.  The frame is unchanged
      // in this block, so local control flow can use a Label rather
      // than a JumpTarget.
      Label non_zero_result;
      __ test(answer.reg(), Operand(answer.reg()));
      __ j(not_zero, &non_zero_result, taken);
      __ mov(answer.reg(), left->reg());
      __ or_(answer.reg(), Operand(right->reg()));
      deferred->Branch(negative);
      __ xor_(answer.reg(), Operand(answer.reg()));  // Positive 0 is correct.
      __ bind(&non_zero_result);
      break;
    }

    case Token::BIT_OR:
      __ or_(answer.reg(), Operand(right->reg()));
      break;

    case Token::BIT_AND:
      __ and_(answer.reg(), Operand(right->reg()));
      break;

    case Token::BIT_XOR:
      __ xor_(answer.reg(), Operand(right->reg()));
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
  if (!dst_.is(eax)) __ mov(dst_, eax);
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
  GenericBinaryOpStub igostub(op_, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, value_, src_);
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


// The result of src + value is in dst.  It either overflowed or was not
// smi tagged.  Undo the speculative addition and call the appropriate
// specialized stub for add.  The result is left in dst.
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
  // Undo the optimistic add operation and call the shared stub.
  __ sub(Operand(dst_), Immediate(value_));
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(eax)) __ mov(dst_, eax);
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
  // Undo the optimistic add operation and call the shared stub.
  __ sub(Operand(dst_), Immediate(value_));
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, value_, dst_);
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


// The result of src - value is in dst.  It either overflowed or was not
// smi tagged.  Undo the speculative subtraction and call the
// appropriate specialized stub for subtract.  The result is left in
// dst.
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
  // Undo the optimistic sub operation and call the shared stub.
  __ add(Operand(dst_), Immediate(value_));
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


Result CodeGenerator::ConstantSmiBinaryOperation(Token::Value op,
                                                 Result* operand,
                                                 Handle<Object> value,
                                                 StaticType* type,
                                                 bool reversed,
                                                 OverwriteMode overwrite_mode) {
  // NOTE: This is an attempt to inline (a bit) more of the code for
  // some possible smi operations (like + and -) when (at least) one
  // of the operands is a constant smi.
  // Consumes the argument "operand".
  // TODO(199): Optimize some special cases of operations involving a
  // smi literal (multiply by 2, shift by 0, etc.).
  if (IsUnsafeSmi(value)) {
    Result unsafe_operand(value);
    if (reversed) {
      return LikelySmiBinaryOperation(op, &unsafe_operand, operand,
                                      overwrite_mode);
    } else {
      return LikelySmiBinaryOperation(op, operand, &unsafe_operand,
                                      overwrite_mode);
    }
  }

  // Get the literal value.
  Smi* smi_value = Smi::cast(*value);
  int int_value = smi_value->value();

  Result answer;
  switch (op) {
    case Token::ADD: {
      operand->ToRegister();
      frame_->Spill(operand->reg());

      // Optimistically add.  Call the specialized add stub if the
      // result is not a smi or overflows.
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
      __ add(Operand(operand->reg()), Immediate(value));
      deferred->Branch(overflow);
      __ test(operand->reg(), Immediate(kSmiTagMask));
      deferred->Branch(not_zero);
      deferred->BindExit();
      answer = *operand;
      break;
    }

    case Token::SUB: {
      DeferredCode* deferred = NULL;
      if (reversed) {
        // The reversed case is only hit when the right operand is not a
        // constant.
        ASSERT(operand->is_register());
        answer = allocator()->Allocate();
        ASSERT(answer.is_valid());
        __ Set(answer.reg(), Immediate(value));
        deferred = new DeferredInlineSmiOperationReversed(op,
                                                          answer.reg(),
                                                          smi_value,
                                                          operand->reg(),
                                                          overwrite_mode);
        __ sub(answer.reg(), Operand(operand->reg()));
      } else {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        answer = *operand;
        deferred = new DeferredInlineSmiSub(operand->reg(),
                                            smi_value,
                                            overwrite_mode);
        __ sub(Operand(operand->reg()), Immediate(value));
      }
      deferred->Branch(overflow);
      __ test(answer.reg(), Immediate(kSmiTagMask));
      deferred->Branch(not_zero);
      deferred->BindExit();
      operand->Unuse();
      break;
    }

    case Token::SAR:
      if (reversed) {
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
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
        __ test(operand->reg(), Immediate(kSmiTagMask));
        deferred->Branch(not_zero);
        if (shift_value > 0) {
          __ sar(operand->reg(), shift_value);
          __ and_(operand->reg(), ~kSmiTagMask);
        }
        deferred->BindExit();
        answer = *operand;
      }
      break;

    case Token::SHR:
      if (reversed) {
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
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
        __ test(operand->reg(), Immediate(kSmiTagMask));
        deferred->Branch(not_zero);
        __ mov(answer.reg(), operand->reg());
        __ SmiUntag(answer.reg());
        __ shr(answer.reg(), shift_value);
        // A negative Smi shifted right two is in the positive Smi range.
        if (shift_value < 2) {
          __ test(answer.reg(), Immediate(0xc0000000));
          deferred->Branch(not_zero);
        }
        operand->Unuse();
        __ SmiTag(answer.reg());
        deferred->BindExit();
      }
      break;

    case Token::SHL:
      if (reversed) {
        Result right;
        Result right_copy_in_ecx;

        // Make sure to get a copy of the right operand into ecx. This
        // allows us to modify it without having to restore it in the
        // deferred code.
        operand->ToRegister();
        if (operand->reg().is(ecx)) {
          right = allocator()->Allocate();
          __ mov(right.reg(), ecx);
          frame_->Spill(ecx);
          right_copy_in_ecx = *operand;
        } else {
          right_copy_in_ecx = allocator()->Allocate(ecx);
          __ mov(ecx, operand->reg());
          right = *operand;
        }
        operand->Unuse();

        answer = allocator()->Allocate();
        DeferredInlineSmiOperationReversed* deferred =
            new DeferredInlineSmiOperationReversed(op,
                                                   answer.reg(),
                                                   smi_value,
                                                   right.reg(),
                                                   overwrite_mode);
        __ mov(answer.reg(), Immediate(int_value));
        __ sar(ecx, kSmiTagSize);
        deferred->Branch(carry);
        __ shl_cl(answer.reg());
        __ cmp(answer.reg(), 0xc0000000);
        deferred->Branch(sign);
        __ SmiTag(answer.reg());

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
          __ test(operand->reg(), Immediate(kSmiTagMask));
          deferred->Branch(not_zero);
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
          __ test(operand->reg(), Immediate(kSmiTagMask));
          deferred->Branch(not_zero);
          __ mov(answer.reg(), operand->reg());
          ASSERT(kSmiTag == 0);  // adjust code if not the case
          // We do no shifts, only the Smi conversion, if shift_value is 1.
          if (shift_value > 1) {
            __ shl(answer.reg(), shift_value - 1);
          }
          // Convert int result to Smi, checking that it is in int range.
          ASSERT(kSmiTagSize == 1);  // adjust code if not the case
          __ add(answer.reg(), Operand(answer.reg()));
          deferred->Branch(overflow);
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
      DeferredCode* deferred = NULL;
      if (reversed) {
        deferred = new DeferredInlineSmiOperationReversed(op,
                                                          operand->reg(),
                                                          smi_value,
                                                          operand->reg(),
                                                          overwrite_mode);
      } else {
        deferred =  new DeferredInlineSmiOperation(op,
                                                   operand->reg(),
                                                   operand->reg(),
                                                   smi_value,
                                                   overwrite_mode);
      }
      __ test(operand->reg(), Immediate(kSmiTagMask));
      deferred->Branch(not_zero);
      if (op == Token::BIT_AND) {
        __ and_(Operand(operand->reg()), Immediate(value));
      } else if (op == Token::BIT_XOR) {
        if (int_value != 0) {
          __ xor_(Operand(operand->reg()), Immediate(value));
        }
      } else {
        ASSERT(op == Token::BIT_OR);
        if (int_value != 0) {
          __ or_(Operand(operand->reg()), Immediate(value));
        }
      }
      deferred->BindExit();
      answer = *operand;
      break;
    }

    case Token::DIV:
      if (!reversed && int_value == 2) {
        operand->ToRegister();
        frame_->Spill(operand->reg());

        DeferredInlineSmiOperation* deferred =
            new DeferredInlineSmiOperation(op,
                                           operand->reg(),
                                           operand->reg(),
                                           smi_value,
                                           overwrite_mode);
        // Check that lowest log2(value) bits of operand are zero, and test
        // smi tag at the same time.
        ASSERT_EQ(0, kSmiTag);
        ASSERT_EQ(1, kSmiTagSize);
        __ test(operand->reg(), Immediate(3));
        deferred->Branch(not_zero);  // Branch if non-smi or odd smi.
        __ sar(operand->reg(), 1);
        deferred->BindExit();
        answer = *operand;
      } else {
        // Cannot fall through MOD to default case, so we duplicate the
        // default case here.
        Result constant_operand(value);
        if (reversed) {
          answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
                                            overwrite_mode);
        } else {
          answer = LikelySmiBinaryOperation(op, operand, &constant_operand,
                                            overwrite_mode);
        }
      }
      break;
    // Generate inline code for mod of powers of 2 and negative powers of 2.
    case Token::MOD:
      if (!reversed &&
          int_value != 0 &&
          (IsPowerOf2(int_value) || IsPowerOf2(-int_value))) {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        DeferredCode* deferred = new DeferredInlineSmiOperation(op,
                                                                operand->reg(),
                                                                operand->reg(),
                                                                smi_value,
                                                                overwrite_mode);
        // Check for negative or non-Smi left hand side.
        __ test(operand->reg(), Immediate(kSmiTagMask | 0x80000000));
        deferred->Branch(not_zero);
        if (int_value < 0) int_value = -int_value;
        if (int_value == 1) {
          __ mov(operand->reg(), Immediate(Smi::FromInt(0)));
        } else {
          __ and_(operand->reg(), (int_value << kSmiTagSize) - 1);
        }
        deferred->BindExit();
        answer = *operand;
        break;
      }
      // Fall through if we did not find a power of 2 on the right hand side!

    default: {
      Result constant_operand(value);
      if (reversed) {
        answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        answer = LikelySmiBinaryOperation(op, operand, &constant_operand,
                                          overwrite_mode);
      }
      break;
    }
  }
  ASSERT(answer.is_valid());
  return answer;
}


static bool CouldBeNaN(const Result& result) {
  if (!result.is_constant()) return true;
  if (!result.handle()->IsHeapNumber()) return false;
  return isnan(HeapNumber::cast(*result.handle())->value());
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

  // If either side is a constant of some sort, we can probably optimize the
  // comparison.
  bool left_side_constant_smi = false;
  bool left_side_constant_null = false;
  bool left_side_constant_1_char_string = false;
  if (left_side.is_constant()) {
    left_side_constant_smi = left_side.handle()->IsSmi();
    left_side_constant_null = left_side.handle()->IsNull();
    left_side_constant_1_char_string =
        (left_side.handle()->IsString() &&
         (String::cast(*left_side.handle())->length() == 1));
  }
  bool right_side_constant_smi = false;
  bool right_side_constant_null = false;
  bool right_side_constant_1_char_string = false;
  if (right_side.is_constant()) {
    right_side_constant_smi = right_side.handle()->IsSmi();
    right_side_constant_null = right_side.handle()->IsNull();
    right_side_constant_1_char_string =
        (right_side.handle()->IsString() &&
         (String::cast(*right_side.handle())->length() == 1));
  }

  if (left_side_constant_smi || right_side_constant_smi) {
    if (left_side_constant_smi && right_side_constant_smi) {
      // Trivial case, comparing two constants.
      int left_value = Smi::cast(*left_side.handle())->value();
      int right_value = Smi::cast(*right_side.handle())->value();
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
        Result temp = left_side;
        left_side = right_side;
        right_side = temp;
        cc = ReverseCondition(cc);
        // This may reintroduce greater or less_equal as the value of cc.
        // CompareStub and the inline code both support all values of cc.
      }
      // Implement comparison against a constant Smi, inlining the case
      // where both sides are Smis.
      left_side.ToRegister();
      Register left_reg = left_side.reg();
      Handle<Object> right_val = right_side.handle();

      // Here we split control flow to the stub call and inlined cases
      // before finally splitting it to the control destination.  We use
      // a jump target and branching to duplicate the virtual frame at
      // the first split.  We manually handle the off-frame references
      // by reconstituting them on the non-fall-through path.
      JumpTarget is_smi;
      __ test(left_side.reg(), Immediate(kSmiTagMask));
      is_smi.Branch(zero, taken);

      bool is_for_loop_compare = (node->AsCompareOperation() != NULL)
          && node->AsCompareOperation()->is_for_loop_condition();
      if (!is_for_loop_compare
          && CpuFeatures::IsSupported(SSE2)
          && right_val->IsSmi()) {
        // Right side is a constant smi and left side has been checked
        // not to be a smi.
        CpuFeatures::Scope use_sse2(SSE2);
        JumpTarget not_number;
        __ cmp(FieldOperand(left_reg, HeapObject::kMapOffset),
               Immediate(Factory::heap_number_map()));
        not_number.Branch(not_equal, &left_side);
        __ movdbl(xmm1,
                  FieldOperand(left_reg, HeapNumber::kValueOffset));
        int value = Smi::cast(*right_val)->value();
        if (value == 0) {
          __ xorpd(xmm0, xmm0);
        } else {
          Result temp = allocator()->Allocate();
          __ mov(temp.reg(), Immediate(value));
          __ cvtsi2sd(xmm0, Operand(temp.reg()));
          temp.Unuse();
        }
        __ comisd(xmm1, xmm0);
        // Jump to builtin for NaN.
        not_number.Branch(parity_even, &left_side);
        left_side.Unuse();
        Condition double_cc = cc;
        switch (cc) {
          case less:          double_cc = below;       break;
          case equal:         double_cc = equal;       break;
          case less_equal:    double_cc = below_equal; break;
          case greater:       double_cc = above;       break;
          case greater_equal: double_cc = above_equal; break;
          default: UNREACHABLE();
        }
        dest->true_target()->Branch(double_cc);
        dest->false_target()->Jump();
        not_number.Bind(&left_side);
      }

      // Setup and call the compare stub.
      CompareStub stub(cc, strict, kCantBothBeNaN);
      Result result = frame_->CallStub(&stub, &left_side, &right_side);
      result.ToRegister();
      __ cmp(result.reg(), 0);
      result.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_smi.Bind();
      left_side = Result(left_reg);
      right_side = Result(right_val);
      // Test smi equality and comparison by signed int comparison.
      if (IsUnsafeSmi(right_side.handle())) {
        right_side.ToRegister();
        __ cmp(left_side.reg(), Operand(right_side.reg()));
      } else {
        __ cmp(Operand(left_side.reg()), Immediate(right_side.handle()));
      }
      left_side.Unuse();
      right_side.Unuse();
      dest->Split(cc);
    }

  } else if (cc == equal &&
             (left_side_constant_null || right_side_constant_null)) {
    // To make null checks efficient, we check if either the left side or
    // the right side is the constant 'null'.
    // If so, we optimize the code by inlining a null check instead of
    // calling the (very) general runtime routine for checking equality.
    Result operand = left_side_constant_null ? right_side : left_side;
    right_side.Unuse();
    left_side.Unuse();
    operand.ToRegister();
    __ cmp(operand.reg(), Factory::null_value());
    if (strict) {
      operand.Unuse();
      dest->Split(equal);
    } else {
      // The 'null' value is only equal to 'undefined' if using non-strict
      // comparisons.
      dest->true_target()->Branch(equal);
      __ cmp(operand.reg(), Factory::undefined_value());
      dest->true_target()->Branch(equal);
      __ test(operand.reg(), Immediate(kSmiTagMask));
      dest->false_target()->Branch(equal);

      // It can be an undetectable object.
      // Use a scratch register in preference to spilling operand.reg().
      Result temp = allocator()->Allocate();
      ASSERT(temp.is_valid());
      __ mov(temp.reg(),
             FieldOperand(operand.reg(), HeapObject::kMapOffset));
      __ movzx_b(temp.reg(),
                 FieldOperand(temp.reg(), Map::kBitFieldOffset));
      __ test(temp.reg(), Immediate(1 << Map::kIsUndetectable));
      temp.Unuse();
      operand.Unuse();
      dest->Split(not_zero);
    }
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
      __ test(left_side.reg(), Immediate(kSmiTagMask));
      is_not_string.Branch(zero, &left_side);
      Result temp = allocator_->Allocate();
      ASSERT(temp.is_valid());
      __ mov(temp.reg(),
             FieldOperand(left_side.reg(), HeapObject::kMapOffset));
      __ movzx_b(temp.reg(),
                 FieldOperand(temp.reg(), Map::kInstanceTypeOffset));
      // If we are testing for equality then make use of the symbol shortcut.
      // Check if the right left hand side has the same type as the left hand
      // side (which is always a symbol).
      if (cc == equal) {
        Label not_a_symbol;
        ASSERT(kSymbolTag != 0);
        // Ensure that no non-strings have the symbol bit set.
        ASSERT(kNotStringTag + kIsSymbolMask > LAST_TYPE);
        __ test(temp.reg(), Immediate(kIsSymbolMask));  // Test the symbol bit.
        __ j(zero, &not_a_symbol);
        // They are symbols, so do identity compare.
        __ cmp(left_side.reg(), right_side.handle());
        dest->true_target()->Branch(equal);
        dest->false_target()->Branch(not_equal);
        __ bind(&not_a_symbol);
      }
      // If the receiver is not a string of the type we handle call the stub.
      __ and_(temp.reg(),
          kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask);
      __ cmp(temp.reg(), kStringTag | kSeqStringTag | kAsciiStringTag);
      temp.Unuse();
      is_string.Branch(equal, &left_side);

      // Setup and call the compare stub.
      is_not_string.Bind(&left_side);
      CompareStub stub(cc, strict, kCantBothBeNaN);
      Result result = frame_->CallStub(&stub, &left_side, &right_side);
      result.ToRegister();
      __ cmp(result.reg(), 0);
      result.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_string.Bind(&left_side);
      // Here we know we have a sequential ASCII string.
      left_side = Result(left_reg);
      right_side = Result(right_val);
      Result temp2 = allocator_->Allocate();
      ASSERT(temp2.is_valid());
      // Test string equality and comparison.
      if (cc == equal) {
        Label comparison_done;
        __ cmp(FieldOperand(left_side.reg(), String::kLengthOffset),
               Immediate(1));
        __ j(not_equal, &comparison_done);
        uint8_t char_value =
            static_cast<uint8_t>(String::cast(*right_side.handle())->Get(0));
        __ cmpb(FieldOperand(left_side.reg(), SeqAsciiString::kHeaderSize),
                char_value);
        __ bind(&comparison_done);
      } else {
        __ mov(temp2.reg(),
               FieldOperand(left_side.reg(), String::kLengthOffset));
        __ sub(Operand(temp2.reg()), Immediate(1));
        Label comparison;
        // If the length is 0 then our subtraction gave -1 which compares less
        // than any character.
        __ j(negative, &comparison);
        // Otherwise load the first character.
        __ movzx_b(temp2.reg(),
                   FieldOperand(left_side.reg(), SeqAsciiString::kHeaderSize));
        __ bind(&comparison);
        // Compare the first character of the string with out constant
        // 1-character string.
        uint8_t char_value =
            static_cast<uint8_t>(String::cast(*right_side.handle())->Get(0));
        __ cmp(Operand(temp2.reg()), Immediate(char_value));
        Label characters_were_different;
        __ j(not_equal, &characters_were_different);
        // If the first character is the same then the long string sorts after
        // the short one.
        __ cmp(FieldOperand(left_side.reg(), String::kLengthOffset),
               Immediate(1));
        __ bind(&characters_were_different);
      }
      temp2.Unuse();
      left_side.Unuse();
      right_side.Unuse();
      dest->Split(cc);
    }
  } else {
    // Neither side is a constant Smi or null.
    // If either side is a non-smi constant, skip the smi check.
    bool known_non_smi =
        (left_side.is_constant() && !left_side.handle()->IsSmi()) ||
        (right_side.is_constant() && !right_side.handle()->IsSmi());
    NaNInformation nan_info =
        (CouldBeNaN(left_side) && CouldBeNaN(right_side)) ?
        kBothCouldBeNaN :
        kCantBothBeNaN;
    left_side.ToRegister();
    right_side.ToRegister();

    if (known_non_smi) {
      // When non-smi, call out to the compare stub.
      CompareStub stub(cc, strict, nan_info);
      Result answer = frame_->CallStub(&stub, &left_side, &right_side);
      if (cc == equal) {
        __ test(answer.reg(), Operand(answer.reg()));
      } else {
        __ cmp(answer.reg(), 0);
      }
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

      Result temp = allocator_->Allocate();
      ASSERT(temp.is_valid());
      __ mov(temp.reg(), left_side.reg());
      __ or_(temp.reg(), Operand(right_side.reg()));
      __ test(temp.reg(), Immediate(kSmiTagMask));
      temp.Unuse();
      is_smi.Branch(zero, taken);
      // When non-smi, call out to the compare stub.
      CompareStub stub(cc, strict, nan_info);
      Result answer = frame_->CallStub(&stub, &left_side, &right_side);
      if (cc == equal) {
        __ test(answer.reg(), Operand(answer.reg()));
      } else {
        __ cmp(answer.reg(), 0);
      }
      answer.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_smi.Bind();
      left_side = Result(left_reg);
      right_side = Result(right_reg);
      __ cmp(left_side.reg(), Operand(right_side.reg()));
      right_side.Unuse();
      left_side.Unuse();
      dest->Split(cc);
    }
  }
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
  Result existing_args =
      LoadFromSlot(scope()->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);
  frame()->Push(&existing_args);

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
      __ cmp(Operand(probe.reg()), Immediate(Factory::the_hole_value()));
      probe.Unuse();
      __ j(not_equal, &slow);
    }

    if (try_lazy) {
      Label build_args;
      // Get rid of the arguments object probe.
      frame_->Drop();  // Can be called on a spilled frame.
      // Stack now has 3 elements on it.
      // Contents of stack at this point:
      // esp[0]: receiver
      // esp[1]: applicand.apply
      // esp[2]: applicand.

      // Check that the receiver really is a JavaScript object.
      __ mov(eax, Operand(esp, 0));
      __ test(eax, Immediate(kSmiTagMask));
      __ j(zero, &build_args);
      // We allow all JSObjects including JSFunctions.  As long as
      // JS_FUNCTION_TYPE is the last instance type and it is right
      // after LAST_JS_OBJECT_TYPE, we do not have to check the upper
      // bound.
      ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
      __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ecx);
      __ j(below, &build_args);

      // Check that applicand.apply is Function.prototype.apply.
      __ mov(eax, Operand(esp, kPointerSize));
      __ test(eax, Immediate(kSmiTagMask));
      __ j(zero, &build_args);
      __ CmpObjectType(eax, JS_FUNCTION_TYPE, ecx);
      __ j(not_equal, &build_args);
      __ mov(ecx, FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset));
      Handle<Code> apply_code(Builtins::builtin(Builtins::FunctionApply));
      __ cmp(FieldOperand(ecx, SharedFunctionInfo::kCodeOffset),
             Immediate(apply_code));
      __ j(not_equal, &build_args);

      // Check that applicand is a function.
      __ mov(edi, Operand(esp, 2 * kPointerSize));
      __ test(edi, Immediate(kSmiTagMask));
      __ j(zero, &build_args);
      __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
      __ j(not_equal, &build_args);

      // Copy the arguments to this function possibly from the
      // adaptor frame below it.
      Label invoke, adapted;
      __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
      __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
      __ cmp(Operand(ecx),
             Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
      __ j(equal, &adapted);

      // No arguments adaptor frame. Copy fixed number of arguments.
      __ mov(eax, Immediate(scope()->num_parameters()));
      for (int i = 0; i < scope()->num_parameters(); i++) {
        __ push(frame_->ParameterAt(i));
      }
      __ jmp(&invoke);

      // Arguments adaptor frame present. Copy arguments from there, but
      // avoid copying too many arguments to avoid stack overflows.
      __ bind(&adapted);
      static const uint32_t kArgumentsLimit = 1 * KB;
      __ mov(eax, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
      __ SmiUntag(eax);
      __ mov(ecx, Operand(eax));
      __ cmp(eax, kArgumentsLimit);
      __ j(above, &build_args);

      // Loop through the arguments pushing them onto the execution
      // stack. We don't inform the virtual frame of the push, so we don't
      // have to worry about getting rid of the elements from the virtual
      // frame.
      Label loop;
      // ecx is a small non-negative integer, due to the test above.
      __ test(ecx, Operand(ecx));
      __ j(zero, &invoke);
      __ bind(&loop);
      __ push(Operand(edx, ecx, times_pointer_size, 1 * kPointerSize));
      __ dec(ecx);
      __ j(not_zero, &loop);

      // Invoke the function.
      __ bind(&invoke);
      ParameterCount actual(eax);
      __ InvokeFunction(edi, actual, CALL_FUNCTION);
      // Drop applicand.apply and applicand from the stack, and push
      // the result of the function call, but leave the spilled frame
      // unchanged, with 3 elements, so it is correct when we compile the
      // slow-case code.
      __ add(Operand(esp), Immediate(2 * kPointerSize));
      __ push(eax);
      // Stack now has 1 element:
      //   esp[0]: result
      __ jmp(&done);

      // Slow-case: Allocate the arguments object since we know it isn't
      // there, and fall-through to the slow-case where we call
      // applicand.apply.
      __ bind(&build_args);
      // Stack now has 3 elements, because we have jumped from where:
      // esp[0]: receiver
      // esp[1]: applicand.apply
      // esp[2]: applicand.

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
    __ mov(eax, Operand(esp, 3 * kPointerSize));
    __ mov(ebx, Operand(esp, 2 * kPointerSize));
    __ mov(Operand(esp, 2 * kPointerSize), eax);
    __ mov(Operand(esp, 3 * kPointerSize), ebx);

    CallFunctionStub call_function(2, NOT_IN_LOOP, NO_CALL_FUNCTION_FLAGS);
    Result res = frame_->CallStub(&call_function, 3);
    // The function and its two arguments have been dropped.
    frame_->Drop(1);  // Drop the receiver as well.
    res.ToRegister();
    frame_->EmitPush(res.reg());
    // Stack now has 1 element:
    //   esp[0]: result
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
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit();
  __ cmp(esp, Operand::StaticVariable(stack_limit));
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
  ASSERT(in_spilled_code());
  set_in_spilled_code(false);
  VisitStatements(statements);
  if (frame_ != NULL) {
    frame_->SpillAll();
  }
  set_in_spilled_code(true);
}


void CodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
  ASSERT(!in_spilled_code());
  for (int i = 0; has_valid_frame() && i < statements->length(); i++) {
    Visit(statements->at(i));
  }
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

  frame_->EmitPush(esi);  // The context is the first argument.
  frame_->EmitPush(Immediate(pairs));
  frame_->EmitPush(Immediate(Smi::FromInt(is_eval() ? 1 : 0)));
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
    frame_->EmitPush(esi);
    frame_->EmitPush(Immediate(var->name()));
    // Declaration nodes are always introduced in one of two modes.
    ASSERT(node->mode() == Variable::VAR || node->mode() == Variable::CONST);
    PropertyAttributes attr = node->mode() == Variable::VAR ? NONE : READ_ONLY;
    frame_->EmitPush(Immediate(Smi::FromInt(attr)));
    // Push initial value, if any.
    // Note: For variables we must not push an initial value (such as
    // 'undefined') because we may have a (legal) redeclaration and we
    // must not destroy the current value.
    if (node->mode() == Variable::CONST) {
      frame_->EmitPush(Immediate(Factory::the_hole_value()));
    } else if (node->fun() != NULL) {
      Load(node->fun());
    } else {
      frame_->EmitPush(Immediate(Smi::FromInt(0)));  // no initial value!
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
  // reference to eax.  This is safe because the current frame does not
  // contain a reference to eax (it is prepared for the return by spilling
  // all registers).
  if (FLAG_trace) {
    frame_->Push(return_value);
    *return_value = frame_->CallRuntime(Runtime::kTraceExit, 1);
  }
  return_value->ToRegister(eax);

  // Add a label for checking the size of the code used for returning.
  Label check_exit_codesize;
  masm_->bind(&check_exit_codesize);

  // Leave the frame and return popping the arguments and the
  // receiver.
  frame_->Exit();
  masm_->ret((scope()->num_parameters() + 1) * kPointerSize);
  DeleteFrame();

#ifdef ENABLE_DEBUGGER_SUPPORT
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

  // Verify that the runtime call result and esi agree.
  if (FLAG_debug_code) {
    __ cmp(context.reg(), Operand(esi));
    __ Assert(equal, "Runtime::NewContext should end up in esi");
  }
}


void CodeGenerator::VisitWithExitStatement(WithExitStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ WithExitStatement");
  CodeForStatementPosition(node);
  // Pop context.
  __ mov(esi, ContextOperand(esi, Context::PREVIOUS_INDEX));
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
      // If control flow can fall off the end of the body, jump back to
      // the top and bind the break target at the exit.
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
        // If we have chosen to recompile the test at the bottom, then
        // it is the continue target.
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


void CodeGenerator::VisitForStatement(ForStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ ForStatement");
  CodeForStatementPosition(node);

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

  // The break target may be already bound (by the condition), or
  // there may not be a valid frame.  Bind it only if needed.
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
  frame_->EmitPop(eax);

  // eax: value to be iterated over
  __ cmp(eax, Factory::undefined_value());
  exit.Branch(equal);
  __ cmp(eax, Factory::null_value());
  exit.Branch(equal);

  // Stack layout in body:
  // [iteration counter (smi)] <- slot 0
  // [length of array]         <- slot 1
  // [FixedArray]              <- slot 2
  // [Map or 0]                <- slot 3
  // [Object]                  <- slot 4

  // Check if enumerable is already a JSObject
  // eax: value to be iterated over
  __ test(eax, Immediate(kSmiTagMask));
  primitive.Branch(zero);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  jsobject.Branch(above_equal);

  primitive.Bind();
  frame_->EmitPush(eax);
  frame_->InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION, 1);
  // function call returns the value in eax, which is where we want it below

  jsobject.Bind();
  // Get the set of properties (as a FixedArray or Map).
  // eax: value to be iterated over
  frame_->EmitPush(eax);  // Push the object being iterated over.

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  JumpTarget call_runtime;
  JumpTarget loop(JumpTarget::BIDIRECTIONAL);
  JumpTarget check_prototype;
  JumpTarget use_cache;
  __ mov(ecx, eax);
  loop.Bind();
  // Check that there are no elements.
  __ mov(edx, FieldOperand(ecx, JSObject::kElementsOffset));
  __ cmp(Operand(edx), Immediate(Factory::empty_fixed_array()));
  call_runtime.Branch(not_equal);
  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in ebx for the subsequent
  // prototype load.
  __ mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ mov(edx, FieldOperand(ebx, Map::kInstanceDescriptorsOffset));
  __ cmp(Operand(edx), Immediate(Factory::empty_descriptor_array()));
  call_runtime.Branch(equal);
  // Check that there in an enum cache in the non-empty instance
  // descriptors.  This is the case if the next enumeration index
  // field does not contain a smi.
  __ mov(edx, FieldOperand(edx, DescriptorArray::kEnumerationIndexOffset));
  __ test(edx, Immediate(kSmiTagMask));
  call_runtime.Branch(zero);
  // For all objects but the receiver, check that the cache is empty.
  __ cmp(ecx, Operand(eax));
  check_prototype.Branch(equal);
  __ mov(edx, FieldOperand(edx, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ cmp(Operand(edx), Immediate(Factory::empty_fixed_array()));
  call_runtime.Branch(not_equal);
  check_prototype.Bind();
  // Load the prototype from the map and loop if non-null.
  __ mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  __ cmp(Operand(ecx), Immediate(Factory::null_value()));
  loop.Branch(not_equal);
  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
  use_cache.Jump();

  call_runtime.Bind();
  // Call the runtime to get the property names for the object.
  frame_->EmitPush(eax);  // push the Object (slot 4) for the runtime call
  frame_->CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  // eax: map or fixed array (result from call to
  // Runtime::kGetPropertyNamesFast)
  __ mov(edx, Operand(eax));
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ecx, Factory::meta_map());
  fixed_array.Branch(not_equal);

  use_cache.Bind();
  // Get enum cache
  // eax: map (either the result from a call to
  // Runtime::kGetPropertyNamesFast or has been fetched directly from
  // the object)
  __ mov(ecx, Operand(eax));

  __ mov(ecx, FieldOperand(ecx, Map::kInstanceDescriptorsOffset));
  // Get the bridge array held in the enumeration index field.
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumerationIndexOffset));
  // Get the cache from the bridge array.
  __ mov(edx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  frame_->EmitPush(eax);  // <- slot 3
  frame_->EmitPush(edx);  // <- slot 2
  __ mov(eax, FieldOperand(edx, FixedArray::kLengthOffset));
  __ SmiTag(eax);
  frame_->EmitPush(eax);  // <- slot 1
  frame_->EmitPush(Immediate(Smi::FromInt(0)));  // <- slot 0
  entry.Jump();

  fixed_array.Bind();
  // eax: fixed array (result from call to Runtime::kGetPropertyNamesFast)
  frame_->EmitPush(Immediate(Smi::FromInt(0)));  // <- slot 3
  frame_->EmitPush(eax);  // <- slot 2

  // Push the length of the array and the initial index onto the stack.
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ SmiTag(eax);
  frame_->EmitPush(eax);  // <- slot 1
  frame_->EmitPush(Immediate(Smi::FromInt(0)));  // <- slot 0

  // Condition.
  entry.Bind();
  // Grab the current frame's height for the break and continue
  // targets only after all the state is pushed on the frame.
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);
  node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);

  __ mov(eax, frame_->ElementAt(0));  // load the current count
  __ cmp(eax, frame_->ElementAt(1));  // compare to the array length
  node->break_target()->Branch(above_equal);

  // Get the i'th entry of the array.
  __ mov(edx, frame_->ElementAt(2));
  __ mov(ebx, Operand(edx, eax, times_2,
                      FixedArray::kHeaderSize - kHeapObjectTag));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case eax: current iteration count ebx: i'th entry
  // of the enum cache
  __ mov(edx, frame_->ElementAt(3));
  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  // eax: current iteration count
  // ebx: i'th entry of the enum cache
  // edx: expected map value
  __ mov(ecx, frame_->ElementAt(4));
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ cmp(ecx, Operand(edx));
  end_del_check.Branch(equal);

  // Convert the entry to a string (or null if it isn't a property anymore).
  frame_->EmitPush(frame_->ElementAt(4));  // push enumerable
  frame_->EmitPush(ebx);  // push entry
  frame_->InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION, 2);
  __ mov(ebx, Operand(eax));

  // If the property has been removed while iterating, we just skip it.
  __ cmp(ebx, Factory::null_value());
  node->continue_target()->Branch(equal);

  end_del_check.Bind();
  // Store the entry in the 'each' expression and take another spin in the
  // loop.  edx: i'th entry of the enum cache (or string there of)
  frame_->EmitPush(ebx);
  { Reference each(this, node->each());
    // Loading a reference may leave the frame in an unspilled state.
    frame_->SpillAll();
    if (!each.is_illegal()) {
      if (each.size() > 0) {
        frame_->EmitPush(frame_->ElementAt(each.size()));
        each.SetValue(NOT_CONST_INIT);
        frame_->Drop(2);
      } else {
        // If the reference was to a slot we rely on the convenient property
        // that it doesn't matter whether a value (eg, ebx pushed above) is
        // right on top of or right underneath a zero-sized reference.
        each.SetValue(NOT_CONST_INIT);
        frame_->Drop();
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
  frame_->EmitPop(eax);
  __ add(Operand(eax), Immediate(Smi::FromInt(1)));
  frame_->EmitPush(eax);
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
  frame_->EmitPush(eax);

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
    __ mov(eax, Operand::StaticVariable(handler_address));
    __ cmp(esp, Operand(eax));
    __ Assert(equal, "stack pointer should point to top handler");
  }

  // If we can fall off the end of the try block, unlink from try chain.
  if (has_valid_frame()) {
    // The next handler address is on top of the frame.  Unlink from
    // the handler list and drop the rest of this handler from the
    // frame.
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    frame_->EmitPop(Operand::StaticVariable(handler_address));
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
        return_value.ToRegister(eax);
      } else {
        shadows[i]->Bind();
      }
      // Because we can be jumping here (to spilled code) from
      // unspilled code, we need to reestablish a spilled frame at
      // this block.
      frame_->SpillAll();

      // Reload sp from the top handler, because some statements that we
      // break from (eg, for...in) may have left stuff on the stack.
      __ mov(esp, Operand::StaticVariable(handler_address));
      frame_->Forget(frame_->height() - handler_height);

      ASSERT(StackHandlerConstants::kNextOffset == 0);
      frame_->EmitPop(Operand::StaticVariable(handler_address));
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

  frame_->EmitPush(eax);
  // In case of thrown exceptions, this is where we continue.
  __ Set(ecx, Immediate(Smi::FromInt(THROWING)));
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
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    frame_->EmitPop(Operand::StaticVariable(handler_address));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

    // Fake a top of stack value (unneeded when FALLING) and set the
    // state in ecx, then jump around the unlink blocks if any.
    frame_->EmitPush(Immediate(Factory::undefined_value()));
    __ Set(ecx, Immediate(Smi::FromInt(FALLING)));
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
        return_value.ToRegister(eax);
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
      __ mov(esp, Operand::StaticVariable(handler_address));
      frame_->Forget(frame_->height() - handler_height);

      // Unlink this handler and drop it from the frame.
      ASSERT(StackHandlerConstants::kNextOffset == 0);
      frame_->EmitPop(Operand::StaticVariable(handler_address));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);

      if (i == kReturnShadowIndex) {
        // If this target shadowed the function return, materialize
        // the return value on the stack.
        frame_->EmitPush(eax);
      } else {
        // Fake TOS for targets that shadowed breaks and continues.
        frame_->EmitPush(Immediate(Factory::undefined_value()));
      }
      __ Set(ecx, Immediate(Smi::FromInt(JUMPING + i)));
      if (--nof_unlinks > 0) {
        // If this is not the last unlink block, jump around the next.
        finally_block.Jump();
      }
    }
  }

  // --- Finally block ---
  finally_block.Bind();

  // Push the state on the stack.
  frame_->EmitPush(ecx);

  // We keep two elements on the stack - the (possibly faked) result
  // and the state - while evaluating the finally block.
  //
  // Generate code for the statements in the finally block.
  VisitStatementsAndSpill(node->finally_block()->statements());

  if (has_valid_frame()) {
    // Restore state and return value or faked TOS.
    frame_->EmitPop(ecx);
    frame_->EmitPop(eax);
  }

  // Generate code to jump to the right destination for all used
  // formerly shadowing targets.  Deallocate each shadow target.
  for (int i = 0; i < shadows.length(); i++) {
    if (has_valid_frame() && shadows[i]->is_bound()) {
      BreakTarget* original = shadows[i]->other_target();
      __ cmp(Operand(ecx), Immediate(Smi::FromInt(JUMPING + i)));
      if (i == kReturnShadowIndex) {
        // The return value is (already) in eax.
        Result return_value = allocator_->Allocate(eax);
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
    __ cmp(Operand(ecx), Immediate(Smi::FromInt(THROWING)));
    exit.Branch(not_equal);

    // Rethrow exception.
    frame_->EmitPush(eax);  // undo pop from above
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


Result CodeGenerator::InstantiateBoilerplate(Handle<JSFunction> boilerplate) {
  ASSERT(boilerplate->IsBoilerplate());

  // The inevitable call will sync frame elements to memory anyway, so
  // we do it eagerly to allow us to push the arguments directly into
  // place.
  frame()->SyncRange(0, frame()->element_count() - 1);

  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && boilerplate->NumberOfLiterals() == 0) {
    FastNewClosureStub stub;
    frame()->EmitPush(Immediate(boilerplate));
    return frame()->CallStub(&stub, 1);
  } else {
    // Call the runtime to instantiate the function boilerplate
    // object.
    frame()->EmitPush(esi);
    frame()->EmitPush(Immediate(boilerplate));
    return frame()->CallRuntime(Runtime::kNewClosure, 2);
  }
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate =
      Compiler::BuildBoilerplate(node, script(), this);
  // Check for stack-overflow exception.
  if (HasStackOverflow()) return;
  Result result = InstantiateBoilerplate(boilerplate);
  frame()->Push(&result);
}


void CodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  Comment cmnt(masm_, "[ FunctionBoilerplateLiteral");
  Result result = InstantiateBoilerplate(node->boilerplate());
  frame()->Push(&result);
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


Result CodeGenerator::LoadFromSlot(Slot* slot, TypeofState typeof_state) {
  Result result;
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
      result = LoadFromGlobalSlotCheckExtensions(slot, typeof_state, &slow);
      // If there was no control flow to slow, we can exit early.
      if (!slow.is_linked()) return result;
      done.Jump(&result);

    } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
      Slot* potential_slot = slot->var()->local_if_not_shadowed()->slot();
      // Only generate the fast case for locals that rewrite to slots.
      // This rules out argument loads.
      if (potential_slot != NULL) {
        // Allocate a fresh register to use as a temp in
        // ContextSlotOperandCheckExtensions and to hold the result
        // value.
        result = allocator()->Allocate();
        ASSERT(result.is_valid());
        __ mov(result.reg(),
               ContextSlotOperandCheckExtensions(potential_slot,
                                                 result,
                                                 &slow));
        if (potential_slot->var()->mode() == Variable::CONST) {
          __ cmp(result.reg(), Factory::the_hole_value());
          done.Branch(not_equal, &result);
          __ mov(result.reg(), Factory::undefined_value());
        }
        // There is always control flow to slow from
        // ContextSlotOperandCheckExtensions so we have to jump around
        // it.
        done.Jump(&result);
      }
    }

    slow.Bind();
    // A runtime call is inevitable.  We eagerly sync frame elements
    // to memory so that we can push the arguments directly into place
    // on top of the frame.
    frame()->SyncRange(0, frame()->element_count() - 1);
    frame()->EmitPush(esi);
    frame()->EmitPush(Immediate(slot->var()->name()));
    if (typeof_state == INSIDE_TYPEOF) {
      result =
          frame()->CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    } else {
      result = frame()->CallRuntime(Runtime::kLoadContextSlot, 2);
    }

    done.Bind(&result);
    return result;

  } else if (slot->var()->mode() == Variable::CONST) {
    // Const slots may contain 'the hole' value (the constant hasn't been
    // initialized yet) which needs to be converted into the 'undefined'
    // value.
    //
    // We currently spill the virtual frame because constants use the
    // potentially unsafe direct-frame access of SlotOperand.
    VirtualFrame::SpilledScope spilled_scope;
    Comment cmnt(masm_, "[ Load const");
    Label exit;
    __ mov(ecx, SlotOperand(slot, ecx));
    __ cmp(ecx, Factory::the_hole_value());
    __ j(not_equal, &exit);
    __ mov(ecx, Factory::undefined_value());
    __ bind(&exit);
    return Result(ecx);

  } else if (slot->type() == Slot::PARAMETER) {
    frame()->PushParameterAt(slot->index());
    return frame()->Pop();

  } else if (slot->type() == Slot::LOCAL) {
    frame()->PushLocalAt(slot->index());
    return frame()->Pop();

  } else {
    // The other remaining slot types (LOOKUP and GLOBAL) cannot reach
    // here.
    //
    // The use of SlotOperand below is safe for an unspilled frame
    // because it will always be a context slot.
    ASSERT(slot->type() == Slot::CONTEXT);
    result = allocator()->Allocate();
    ASSERT(result.is_valid());
    __ mov(result.reg(), SlotOperand(slot, result.reg()));
    return result;
  }
}


Result CodeGenerator::LoadFromSlotCheckForArguments(Slot* slot,
                                                    TypeofState state) {
  Result result = LoadFromSlot(slot, state);

  // Bail out quickly if we're not using lazy arguments allocation.
  if (ArgumentsMode() != LAZY_ARGUMENTS_ALLOCATION) return result;

  // ... or if the slot isn't a non-parameter arguments slot.
  if (slot->type() == Slot::PARAMETER || !slot->is_arguments()) return result;

  // If the loaded value is a constant, we know if the arguments
  // object has been lazily loaded yet.
  if (result.is_constant()) {
    if (result.handle()->IsTheHole()) {
      result.Unuse();
      return StoreArgumentsObject(false);
    } else {
      return result;
    }
  }

  // The loaded value is in a register. If it is the sentinel that
  // indicates that we haven't loaded the arguments object yet, we
  // need to do it now.
  JumpTarget exit;
  __ cmp(Operand(result.reg()), Immediate(Factory::the_hole_value()));
  exit.Branch(not_equal, &result);

  result.Unuse();
  result = StoreArgumentsObject(false);
  exit.Bind(&result);
  return result;
}


Result CodeGenerator::LoadFromGlobalSlotCheckExtensions(
    Slot* slot,
    TypeofState typeof_state,
    JumpTarget* slow) {
  // Check that no extension objects have been created by calls to
  // eval from the current scope to the global scope.
  Register context = esi;
  Result tmp = allocator_->Allocate();
  ASSERT(tmp.is_valid());  // All non-reserved registers were available.

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context, Context::EXTENSION_INDEX),
               Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      // Load next context in chain.
      __ mov(tmp.reg(), ContextOperand(context, Context::CLOSURE_INDEX));
      __ mov(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp.reg();
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.  If we have reached an eval scope, we check
    // all extensions from this point.
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s != NULL && s->is_eval_scope()) {
    // Loop up the context chain.  There is no frame effect so it is
    // safe to use raw labels here.
    Label next, fast;
    if (!context.is(tmp.reg())) {
      __ mov(tmp.reg(), context);
    }
    __ bind(&next);
    // Terminate at global context.
    __ cmp(FieldOperand(tmp.reg(), HeapObject::kMapOffset),
           Immediate(Factory::global_context_map()));
    __ j(equal, &fast);
    // Check that extension is NULL.
    __ cmp(ContextOperand(tmp.reg(), Context::EXTENSION_INDEX), Immediate(0));
    slow->Branch(not_equal, not_taken);
    // Load next context in chain.
    __ mov(tmp.reg(), ContextOperand(tmp.reg(), Context::CLOSURE_INDEX));
    __ mov(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
    __ jmp(&next);
    __ bind(&fast);
  }
  tmp.Unuse();

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  // The register allocator prefers eax if it is free, so the code generator
  // will load the global object directly into eax, which is where the LoadIC
  // expects it.
  frame_->Spill(eax);
  LoadGlobal();
  frame_->Push(slot->var()->name());
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
                         ? RelocInfo::CODE_TARGET
                         : RelocInfo::CODE_TARGET_CONTEXT;
  Result answer = frame_->CallLoadIC(mode);
  // A test eax instruction following the call signals that the inobject
  // property case was inlined.  Ensure that there is not a test eax
  // instruction here.
  __ nop();
  return answer;
}


void CodeGenerator::StoreToSlot(Slot* slot, InitState init_state) {
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    // For now, just do a runtime call.  Since the call is inevitable,
    // we eagerly sync the virtual frame so we can directly push the
    // arguments into place.
    frame_->SyncRange(0, frame_->element_count() - 1);

    frame_->EmitPush(esi);
    frame_->EmitPush(Immediate(slot->var()->name()));

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
      __ mov(ecx, SlotOperand(slot, ecx));
      __ cmp(ecx, Factory::the_hole_value());
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
      __ mov(SlotOperand(slot, start.reg()), value.reg());
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
  Result result = LoadFromSlotCheckForArguments(node, NOT_INSIDE_TYPEOF);
  frame()->Push(&result);
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


void CodeGenerator::PushUnsafeSmi(Handle<Object> value) {
  ASSERT(value->IsSmi());
  int bits = reinterpret_cast<int>(*value);
  __ push(Immediate(bits & 0x0000FFFF));
  __ or_(Operand(esp, 0), Immediate(bits & 0xFFFF0000));
}


void CodeGenerator::StoreUnsafeSmiToLocal(int offset, Handle<Object> value) {
  ASSERT(value->IsSmi());
  int bits = reinterpret_cast<int>(*value);
  __ mov(Operand(ebp, offset), Immediate(bits & 0x0000FFFF));
  __ or_(Operand(ebp, offset), Immediate(bits & 0xFFFF0000));
}


void CodeGenerator::MoveUnsafeSmi(Register target, Handle<Object> value) {
  ASSERT(target.is_valid());
  ASSERT(value->IsSmi());
  int bits = reinterpret_cast<int>(*value);
  __ Set(target, Immediate(bits & 0x0000FFFF));
  __ or_(target, bits & 0xFFFF0000);
}


bool CodeGenerator::IsUnsafeSmi(Handle<Object> value) {
  if (!value->IsSmi()) return false;
  int int_value = Smi::cast(*value)->value();
  return !is_intn(int_value, kMaxSmiInlinedBits);
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
  __ push(Immediate(Smi::FromInt(node_->literal_index())));
  // RegExp pattern (2).
  __ push(Immediate(node_->pattern()));
  // RegExp flags (3).
  __ push(Immediate(node_->flags()));
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  if (!boilerplate_.is(eax)) __ mov(boilerplate_, eax);
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
  __ mov(literals.reg(),
         FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));

  // Load the literal at the ast saved index.
  Result boilerplate = allocator_->Allocate();
  ASSERT(boilerplate.is_valid());
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  __ mov(boilerplate.reg(), FieldOperand(literals.reg(), literal_offset));

  // Check whether we need to materialize the RegExp object.  If so,
  // jump to the deferred code passing the literals array.
  DeferredRegExpLiteral* deferred =
      new DeferredRegExpLiteral(boilerplate.reg(), literals.reg(), node);
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->Branch(equal);
  deferred->BindExit();
  literals.Unuse();

  // Push the boilerplate object.
  frame_->Push(&boilerplate);
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
  __ mov(literals.reg(),
         FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));
  // Literal array.
  frame_->Push(&literals);
  // Literal index.
  frame_->Push(Smi::FromInt(node->literal_index()));
  // Constant properties.
  frame_->Push(node->constant_properties());
  Result clone;
  if (node->depth() > 1) {
    clone = frame_->CallRuntime(Runtime::kCreateObjectLiteral, 3);
  } else {
    clone = frame_->CallRuntime(Runtime::kCreateObjectLiteralShallow, 3);
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
          Result dummy = frame_->CallStoreIC(Handle<String>::cast(key), false);
          dummy.Unuse();
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
  __ mov(literals.reg(),
         FieldOperand(literals.reg(), JSFunction::kLiteralsOffset));

  frame_->Push(&literals);
  frame_->Push(Smi::FromInt(node->literal_index()));
  frame_->Push(node->constant_elements());
  int length = node->values()->length();
  Result clone;
  if (node->depth() > 1) {
    clone = frame_->CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumLength) {
    clone = frame_->CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(length);
    clone = frame_->CallStub(&stub, 3);
  }
  frame_->Push(&clone);

  // Generate code to set the elements in the array that are not
  // literals.
  for (int i = 0; i < length; i++) {
    Expression* value = node->values()->at(i);

    // If value is a literal the property value is already set in the
    // boilerplate object.
    if (value->AsLiteral() != NULL) continue;
    // If value is a materialized literal the property value is already set
    // in the boilerplate object if it is simple.
    if (CompileTimeValue::IsCompileTimeValue(value)) continue;

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
    // Get the elements array.
    __ mov(elements.reg(),
           FieldOperand(elements.reg(), JSObject::kElementsOffset));

    // Write to the indexed properties array.
    int offset = i * kPointerSize + FixedArray::kHeaderSize;
    __ mov(FieldOperand(elements.reg(), offset), prop_value.reg());

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
    Result result = LoadFromSlotCheckForArguments(slot, NOT_INSIDE_TYPEOF);
    frame()->Push(&result);
    Load(node->value());

    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    GenericBinaryOperation(node->binary_op(),
                           node->type(),
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
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

  // Initialize name and evaluate the receiver subexpression if necessary.
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

  if (node->starts_initialization_block()) {
    ASSERT_EQ(NULL, var);
    // Change to slow case in the beginning of an initialization block to
    // avoid the quadratic behavior of repeatedly adding fast properties.
    if (is_trivial_receiver) {
      frame()->Push(prop->obj());
    } else {
      frame()->Dup();
    }
    Result ignored = frame()->CallRuntime(Runtime::kToSlowProperties, 1);
  }

  if (node->ends_initialization_block() && !is_trivial_receiver) {
    // Add an extra copy of the receiver to the frame, so that it can be
    // converted back to fast case after the assignment.
    frame()->Dup();
  }

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    if (is_trivial_receiver) {
      frame()->Push(prop->obj());
    } else if (var != NULL) {
      // The LoadIC stub expects the object in eax.
      // Freeing eax causes the code generator to load the global into it.
      frame_->Spill(eax);
      LoadGlobal();
    } else {
      frame()->Dup();
    }
    Result value = EmitNamedLoad(name, var != NULL);
    frame()->Push(&value);
    Load(node->value());

    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    GenericBinaryOperation(node->binary_op(),
                           node->type(),
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
    Load(node->value());
  }

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

  ASSERT_EQ(frame()->height(), original_height + 1);
}


void CodeGenerator::EmitKeyedPropertyAssignment(Assignment* node) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Comment cmnt(masm_, "[ Named Property Assignment");
  Property* prop = node->target()->AsProperty();
  ASSERT_NOT_NULL(prop);

  // Evaluate the receiver subexpression.
  Load(prop->obj());

  if (node->starts_initialization_block()) {
    // Change to slow case in the beginning of an initialization block to
    // avoid the quadratic behavior of repeatedly adding fast properties.
    frame_->Dup();
    Result ignored = frame_->CallRuntime(Runtime::kToSlowProperties, 1);
  }

  if (node->ends_initialization_block()) {
    // Add an extra copy of the receiver to the frame, so that it can be
    // converted back to fast case after the assignment.
    frame_->Dup();
  }

  // Evaluate the key subexpression.
  Load(prop->key());

  // Evaluate the right-hand side.
  if (node->is_compound()) {
    // Duplicate receiver and key.
    frame()->PushElementAt(1);
    frame()->PushElementAt(1);
    Result value = EmitKeyedLoad();
    frame()->Push(&value);
    Load(node->value());

    bool overwrite_value =
        (node->value()->AsBinaryOperation() != NULL &&
         node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
    GenericBinaryOperation(node->binary_op(),
                           node->type(),
                           overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
  } else {
    Load(node->value());
  }

  // Perform the assignment.  It is safe to ignore constants here.
  ASSERT(node->op() != Token::INIT_CONST);
  CodeForSourcePosition(node->position());
  Result answer = EmitKeyedStore(prop->key()->type());
  frame()->Push(&answer);

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

  Expression* function = node->expression();
  ZoneList<Expression*>* args = node->arguments();

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

    // Prepare the stack for the call to the resolved function.
    Load(function);

    // Allocate a frame slot for the receiver.
    frame_->Push(Factory::undefined_value());
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      Load(args->at(i));
    }

    // Prepare the stack for the call to ResolvePossiblyDirectEval.
    frame_->PushElementAt(arg_count + 1);
    if (arg_count > 0) {
      frame_->PushElementAt(arg_count);
    } else {
      frame_->Push(Factory::undefined_value());
    }

    // Push the receiver.
    frame_->PushParameterAt(-1);

    // Resolve the call.
    Result result =
        frame_->CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // The runtime call returns a pair of values in eax (function) and
    // edx (receiver). Touch up the stack with the right values.
    Result receiver = allocator_->Allocate(edx);
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
    }

    // Push the name of the function onto the frame.
    frame_->Push(var->name());

    // Call the IC initialization code.
    CodeForSourcePosition(node->position());
    Result result = frame_->CallCallIC(RelocInfo::CODE_TARGET_CONTEXT,
                                       arg_count,
                                       loop_nesting());
    frame_->RestoreContextRegister();
    frame_->Push(&result);

  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // ----------------------------------
    // JavaScript example: 'with (obj) foo(1, 2, 3)'  // foo is in obj
    // ----------------------------------

    // Load the function from the context.  Sync the frame so we can
    // push the arguments directly into place.
    frame_->SyncRange(0, frame_->element_count() - 1);
    frame_->EmitPush(esi);
    frame_->EmitPush(Immediate(var->name()));
    frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    // The runtime call returns a pair of values in eax and edx.  The
    // looked-up function is in eax and the receiver is in edx.  These
    // register references are not ref counted here.  We spill them
    // eagerly since they are arguments to an inevitable call (and are
    // not sharable by the arguments).
    ASSERT(!allocator()->is_used(eax));
    frame_->EmitPush(eax);

    // Load the receiver.
    ASSERT(!allocator()->is_used(edx));
    frame_->EmitPush(edx);

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
        }

        // Push the name of the function onto the frame.
        frame_->Push(name);

        // Call the IC initialization code.
        CodeForSourcePosition(node->position());
        Result result =
            frame_->CallCallIC(RelocInfo::CODE_TARGET, arg_count,
                               loop_nesting());
        frame_->RestoreContextRegister();
        frame_->Push(&result);
      }

    } else {
      // -------------------------------------------
      // JavaScript example: 'array[index](1, 2, 3)'
      // -------------------------------------------

      // Load the function to call from the property through a reference.

      // Pass receiver to called function.
      if (property->is_synthetic()) {
        Reference ref(this, property);
        ref.GetValue();
        // Use global object as receiver.
        LoadGlobalReceiver();
      } else {
        Load(property->obj());
        frame()->Dup();
        Load(property->key());
        Result function = EmitKeyedLoad();
        Result receiver = frame_->Pop();
        frame_->Push(&function);
        frame_->Push(&receiver);
      }

      // Call the function.
      CallWithArguments(args, RECEIVER_MIGHT_BE_VALUE, node->position());
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

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  CodeForSourcePosition(node->position());
  Result result = frame_->CallConstructor(arg_count);
  // Replace the function on the stack with the result.
  frame_->SetElementAt(0, &result);
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  __ test(value.reg(), Immediate(kSmiTagMask));
  value.Unuse();
  destination()->Split(zero);
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
  __ test(value.reg(), Immediate(kSmiTagMask | 0x80000000));
  value.Unuse();
  destination()->Split(zero);
}


// This generates code that performs a charCodeAt() call or returns
// undefined in order to trigger the slow case, Runtime_StringCharCodeAt.
// It can handle flat, 8 and 16 bit characters and cons strings where the
// answer is found in the left hand branch of the cons.  The slow case will
// flatten the string, which will ensure that the answer is in the left hand
// side the next time around.
void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* args) {
  Comment(masm_, "[ GenerateFastCharCodeAt");
  ASSERT(args->length() == 2);

  Label slow_case;
  Label end;
  Label not_a_flat_string;
  Label try_again_with_new_string;
  Label ascii_string;
  Label got_char_code;

  Load(args->at(0));
  Load(args->at(1));
  Result index = frame_->Pop();
  Result object = frame_->Pop();

  // Get register ecx to use as shift amount later.
  Result shift_amount;
  if (object.is_register() && object.reg().is(ecx)) {
    Result fresh = allocator_->Allocate();
    shift_amount = object;
    object = fresh;
    __ mov(object.reg(), ecx);
  }
  if (index.is_register() && index.reg().is(ecx)) {
    Result fresh = allocator_->Allocate();
    shift_amount = index;
    index = fresh;
    __ mov(index.reg(), ecx);
  }
  // There could be references to ecx in the frame. Allocating will
  // spill them, otherwise spill explicitly.
  if (shift_amount.is_valid()) {
    frame_->Spill(ecx);
  } else {
    shift_amount = allocator()->Allocate(ecx);
  }
  ASSERT(shift_amount.is_register());
  ASSERT(shift_amount.reg().is(ecx));
  ASSERT(allocator_->count(ecx) == 1);

  // We will mutate the index register and possibly the object register.
  // The case where they are somehow the same register is handled
  // because we only mutate them in the case where the receiver is a
  // heap object and the index is not.
  object.ToRegister();
  index.ToRegister();
  frame_->Spill(object.reg());
  frame_->Spill(index.reg());

  // We need a single extra temporary register.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());

  // There is no virtual frame effect from here up to the final result
  // push.

  // If the receiver is a smi trigger the slow case.
  ASSERT(kSmiTag == 0);
  __ test(object.reg(), Immediate(kSmiTagMask));
  __ j(zero, &slow_case);

  // If the index is negative or non-smi trigger the slow case.
  ASSERT(kSmiTag == 0);
  __ test(index.reg(), Immediate(kSmiTagMask | 0x80000000));
  __ j(not_zero, &slow_case);
  // Untag the index.
  __ SmiUntag(index.reg());

  __ bind(&try_again_with_new_string);
  // Fetch the instance type of the receiver into ecx.
  __ mov(ecx, FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the slow case.
  __ test(ecx, Immediate(kIsNotStringMask));
  __ j(not_zero, &slow_case);

  // Fetch the length field into the temporary register.
  __ mov(temp.reg(), FieldOperand(object.reg(), String::kLengthOffset));
  // Check for index out of range.
  __ cmp(index.reg(), Operand(temp.reg()));
  __ j(greater_equal, &slow_case);
  // Reload the instance type (into the temp register this time)..
  __ mov(temp.reg(), FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzx_b(temp.reg(), FieldOperand(temp.reg(), Map::kInstanceTypeOffset));

  // We need special handling for non-flat strings.
  ASSERT(kSeqStringTag == 0);
  __ test(temp.reg(), Immediate(kStringRepresentationMask));
  __ j(not_zero, &not_a_flat_string);
  // Check for 1-byte or 2-byte string.
  __ test(temp.reg(), Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the temp register.
  __ movzx_w(temp.reg(), FieldOperand(object.reg(),
                                      index.reg(),
                                      times_2,
                                      SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  __ bind(&ascii_string);
  // Load the byte into the temp register.
  __ movzx_b(temp.reg(), FieldOperand(object.reg(),
                                      index.reg(),
                                      times_1,
                                      SeqAsciiString::kHeaderSize));
  __ bind(&got_char_code);
  __ SmiTag(temp.reg());
  __ jmp(&end);

  // Handle non-flat strings.
  __ bind(&not_a_flat_string);
  __ and_(temp.reg(), kStringRepresentationMask);
  __ cmp(temp.reg(), kConsStringTag);
  __ j(not_equal, &slow_case);

  // ConsString.
  // Check that the right hand side is the empty string (ie if this is really a
  // flat string in a cons string).  If that is not the case we would rather go
  // to the runtime system now, to flatten the string.
  __ mov(temp.reg(), FieldOperand(object.reg(), ConsString::kSecondOffset));
  __ cmp(Operand(temp.reg()), Factory::empty_string());
  __ j(not_equal, &slow_case);
  // Get the first of the two strings.
  __ mov(object.reg(), FieldOperand(object.reg(), ConsString::kFirstOffset));
  __ jmp(&try_again_with_new_string);

  __ bind(&slow_case);
  // Move the undefined value into the result register, which will
  // trigger the slow case.
  __ Set(temp.reg(), Immediate(Factory::undefined_value()));

  __ bind(&end);
  frame_->Push(&temp);
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  __ test(value.reg(), Immediate(kSmiTagMask));
  destination()->false_target()->Branch(equal);
  // It is a heap object - get map.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  // Check if the object is a JS array or not.
  __ CmpObjectType(value.reg(), JS_ARRAY_TYPE, temp.reg());
  value.Unuse();
  temp.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result value = frame_->Pop();
  value.ToRegister();
  ASSERT(value.is_valid());
  __ test(value.reg(), Immediate(kSmiTagMask));
  destination()->false_target()->Branch(equal);
  // It is a heap object - get map.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  // Check if the object is a regexp.
  __ CmpObjectType(value.reg(), JS_REGEXP_TYPE, temp.reg());
  value.Unuse();
  temp.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsObject(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (typeof(arg) === 'object' || %_ClassOf(arg) == 'RegExp')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();

  __ test(obj.reg(), Immediate(kSmiTagMask));
  destination()->false_target()->Branch(zero);
  __ cmp(obj.reg(), Factory::null_value());
  destination()->true_target()->Branch(equal);

  Result map = allocator()->Allocate();
  ASSERT(map.is_valid());
  __ mov(map.reg(), FieldOperand(obj.reg(), HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ movzx_b(map.reg(), FieldOperand(map.reg(), Map::kBitFieldOffset));
  __ test(map.reg(), Immediate(1 << Map::kIsUndetectable));
  destination()->false_target()->Branch(not_zero);
  __ mov(map.reg(), FieldOperand(obj.reg(), HeapObject::kMapOffset));
  __ movzx_b(map.reg(), FieldOperand(map.reg(), Map::kInstanceTypeOffset));
  __ cmp(map.reg(), FIRST_JS_OBJECT_TYPE);
  destination()->false_target()->Branch(less);
  __ cmp(map.reg(), LAST_JS_OBJECT_TYPE);
  obj.Unuse();
  map.Unuse();
  destination()->Split(less_equal);
}


void CodeGenerator::GenerateIsFunction(ZoneList<Expression*>* args) {
  // This generates a fast version of:
  // (%_ClassOf(arg) === 'Function')
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();
  __ test(obj.reg(), Immediate(kSmiTagMask));
  destination()->false_target()->Branch(zero);
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  __ CmpObjectType(obj.reg(), JS_FUNCTION_TYPE, temp.reg());
  obj.Unuse();
  temp.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Load(args->at(0));
  Result obj = frame_->Pop();
  obj.ToRegister();
  __ test(obj.reg(), Immediate(kSmiTagMask));
  destination()->false_target()->Branch(zero);
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  __ mov(temp.reg(),
         FieldOperand(obj.reg(), HeapObject::kMapOffset));
  __ movzx_b(temp.reg(),
             FieldOperand(temp.reg(), Map::kBitFieldOffset));
  __ test(temp.reg(), Immediate(1 << Map::kIsUndetectable));
  obj.Unuse();
  temp.Unuse();
  destination()->Split(not_zero);
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  // Get the frame pointer for the calling frame.
  Result fp = allocator()->Allocate();
  __ mov(fp.reg(), Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ cmp(Operand(fp.reg(), StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &check_frame_marker);
  __ mov(fp.reg(), Operand(fp.reg(), StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ cmp(Operand(fp.reg(), StandardFrameConstants::kMarkerOffset),
         Immediate(Smi::FromInt(StackFrame::CONSTRUCT)));
  fp.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  // ArgumentsAccessStub takes the parameter count as an input argument
  // in register eax.  Create a constant result for it.
  Result count(Handle<Smi>(Smi::FromInt(scope()->num_parameters())));
  // Call the shared stub to get to the arguments.length.
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_LENGTH);
  Result result = frame_->CallStub(&stub, &count);
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
  __ test(obj.reg(), Immediate(kSmiTagMask));
  null.Branch(zero);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  { Result tmp = allocator()->Allocate();
    __ mov(obj.reg(), FieldOperand(obj.reg(), HeapObject::kMapOffset));
    __ movzx_b(tmp.reg(), FieldOperand(obj.reg(), Map::kInstanceTypeOffset));
    __ cmp(tmp.reg(), FIRST_JS_OBJECT_TYPE);
    null.Branch(less);

    // As long as JS_FUNCTION_TYPE is the last instance type and it is
    // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
    // LAST_JS_OBJECT_TYPE.
    ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
    ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
    __ cmp(tmp.reg(), JS_FUNCTION_TYPE);
    function.Branch(equal);
  }

  // Check if the constructor in the map is a function.
  { Result tmp = allocator()->Allocate();
    __ mov(obj.reg(), FieldOperand(obj.reg(), Map::kConstructorOffset));
    __ CmpObjectType(obj.reg(), JS_FUNCTION_TYPE, tmp.reg());
    non_function_constructor.Branch(not_equal);
  }

  // The map register now contains the constructor function. Grab the
  // instance class name from there.
  __ mov(obj.reg(),
         FieldOperand(obj.reg(), JSFunction::kSharedFunctionInfoOffset));
  __ mov(obj.reg(),
         FieldOperand(obj.reg(), SharedFunctionInfo::kInstanceClassNameOffset));
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
  __ test(object.reg(), Immediate(kSmiTagMask));
  leave.Branch(zero, taken);
  // It is a heap object - get map.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  // if (!object->IsJSValue()) return object.
  __ CmpObjectType(object.reg(), JS_VALUE_TYPE, temp.reg());
  leave.Branch(not_equal, not_taken);
  __ mov(temp.reg(), FieldOperand(object.reg(), JSValue::kValueOffset));
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
  __ test(object.reg(), Immediate(kSmiTagMask));
  leave.Branch(zero, &value, taken);

  // It is a heap object - get its map.
  Result scratch = allocator_->Allocate();
  ASSERT(scratch.is_valid());
  // if (!object->IsJSValue()) return value.
  __ CmpObjectType(object.reg(), JS_VALUE_TYPE, scratch.reg());
  leave.Branch(not_equal, &value, not_taken);

  // Store the value.
  __ mov(FieldOperand(object.reg(), JSValue::kValueOffset), value.reg());
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  Result duplicate_value = allocator_->Allocate();
  ASSERT(duplicate_value.is_valid());
  __ mov(duplicate_value.reg(), value.reg());
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


void CodeGenerator::GenerateArgumentsAccess(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
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
  __ cmp(right.reg(), Operand(left.reg()));
  right.Unuse();
  left.Unuse();
  destination()->Split(equal);
}


void CodeGenerator::GenerateGetFramePointer(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  ASSERT(kSmiTag == 0);  // EBP value is aligned, so it should look like Smi.
  Result ebp_as_smi = allocator_->Allocate();
  ASSERT(ebp_as_smi.is_valid());
  __ mov(ebp_as_smi.reg(), Operand(ebp));
  frame_->Push(&ebp_as_smi);
}


void CodeGenerator::GenerateRandomPositiveSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  frame_->SpillAll();

  // Make sure the frame is aligned like the OS expects.
  static const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    __ mov(edi, Operand(esp));  // Save in callee-saved register.
    __ and_(esp, -kFrameAlignment);
  }

  // Call V8::RandomPositiveSmi().
  __ call(FUNCTION_ADDR(V8::RandomPositiveSmi), RelocInfo::RUNTIME_ENTRY);

  // Restore stack pointer from callee-saved register edi.
  if (kFrameAlignment > 0) {
    __ mov(esp, Operand(edi));
  }

  Result result = allocator_->Allocate(eax);
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

  // Load the arguments on the stack and call the stub.
  Load(args->at(0));
  Load(args->at(1));
  Load(args->at(2));
  Load(args->at(3));
  RegExpExecStub stub;
  Result result = frame_->CallStub(&stub, 4);
  frame_->Push(&result);
}


void CodeGenerator::GenerateNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  Load(args->at(0));
  NumberToStringStub stub;
  Result result = frame_->CallStub(&stub, 1);
  frame_->Push(&result);
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
    __ mov(temp.reg(), GlobalObject());
    __ mov(temp.reg(), FieldOperand(temp.reg(), GlobalObject::kBuiltinsOffset));
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
        frame_->EmitPush(esi);
        frame_->EmitPush(Immediate(variable->name()));
        Result context = frame_->CallRuntime(Runtime::kLookupContext, 2);
        ASSERT(context.is_register());
        frame_->EmitPush(context.reg());
        context.Unuse();
        frame_->EmitPush(Immediate(variable->name()));
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
    Load(node->expression());
    bool overwrite =
        (node->expression()->AsBinaryOperation() != NULL &&
         node->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
    switch (op) {
      case Token::SUB: {
        GenericUnaryOpStub stub(Token::SUB, overwrite);
        Result operand = frame_->Pop();
        Result answer = frame_->CallStub(&stub, &operand);
        frame_->Push(&answer);
        break;
      }

      case Token::BIT_NOT: {
        // Smi check.
        JumpTarget smi_label;
        JumpTarget continue_label;
        Result operand = frame_->Pop();
        operand.ToRegister();
        __ test(operand.reg(), Immediate(kSmiTagMask));
        smi_label.Branch(zero, &operand, taken);

        GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
        Result answer = frame_->CallStub(&stub, &operand);
        continue_label.Jump(&answer);

        smi_label.Bind(&answer);
        answer.ToRegister();
        frame_->Spill(answer.reg());
        __ not_(answer.reg());
        __ and_(answer.reg(), ~kSmiTagMask);  // Remove inverted smi-tag.

        continue_label.Bind(&answer);
        frame_->Push(&answer);
        break;
      }

      case Token::ADD: {
        // Smi check.
        JumpTarget continue_label;
        Result operand = frame_->Pop();
        operand.ToRegister();
        __ test(operand.reg(), Immediate(kSmiTagMask));
        continue_label.Branch(zero, &operand, taken);

        frame_->Push(&operand);
        Result answer = frame_->InvokeBuiltin(Builtins::TO_NUMBER,
                                              CALL_FUNCTION, 1);

        continue_label.Bind(&answer);
        frame_->Push(&answer);
        break;
      }

      default:
        // NOT, DELETE, TYPEOF, and VOID are handled outside the
        // switch.
        UNREACHABLE();
    }
  }
}


// The value in dst was optimistically incremented or decremented.  The
// result overflowed or was not smi tagged.  Undo the operation, call
// into the runtime to convert the argument to a number, and call the
// specialized add or subtract stub.  The result is left in dst.
class DeferredPrefixCountOperation: public DeferredCode {
 public:
  DeferredPrefixCountOperation(Register dst, bool is_increment)
      : dst_(dst), is_increment_(is_increment) {
    set_comment("[ DeferredCountOperation");
  }

  virtual void Generate();

 private:
  Register dst_;
  bool is_increment_;
};


void DeferredPrefixCountOperation::Generate() {
  // Undo the optimistic smi operation.
  if (is_increment_) {
    __ sub(Operand(dst_), Immediate(Smi::FromInt(1)));
  } else {
    __ add(Operand(dst_), Immediate(Smi::FromInt(1)));
  }
  __ push(dst_);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
  __ push(eax);
  __ push(Immediate(Smi::FromInt(1)));
  if (is_increment_) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


// The value in dst was optimistically incremented or decremented.  The
// result overflowed or was not smi tagged.  Undo the operation and call
// into the runtime to convert the argument to a number.  Update the
// original value in old.  Call the specialized add or subtract stub.
// The result is left in dst.
class DeferredPostfixCountOperation: public DeferredCode {
 public:
  DeferredPostfixCountOperation(Register dst, Register old, bool is_increment)
      : dst_(dst), old_(old), is_increment_(is_increment) {
    set_comment("[ DeferredCountOperation");
  }

  virtual void Generate();

 private:
  Register dst_;
  Register old_;
  bool is_increment_;
};


void DeferredPostfixCountOperation::Generate() {
  // Undo the optimistic smi operation.
  if (is_increment_) {
    __ sub(Operand(dst_), Immediate(Smi::FromInt(1)));
  } else {
    __ add(Operand(dst_), Immediate(Smi::FromInt(1)));
  }
  __ push(dst_);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);

  // Save the result of ToNumber to use as the old value.
  __ push(eax);

  // Call the runtime for the addition or subtraction.
  __ push(eax);
  __ push(Immediate(Smi::FromInt(1)));
  if (is_increment_) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
  if (!dst_.is(eax)) __ mov(dst_, eax);
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

  // A constant reference is not saved to, so a constant reference is not a
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
      __ mov(old_value.reg(), new_value.reg());
    }
    // Ensure the new value is writable.
    frame_->Spill(new_value.reg());

    // In order to combine the overflow and the smi tag check, we need
    // to be able to allocate a byte register.  We attempt to do so
    // without spilling.  If we fail, we will generate separate overflow
    // and smi tag checks.
    //
    // We allocate and clear the temporary byte register before
    // performing the count operation since clearing the register using
    // xor will clear the overflow flag.
    Result tmp = allocator_->AllocateByteRegisterWithoutSpilling();
    if (tmp.is_valid()) {
      __ Set(tmp.reg(), Immediate(0));
    }

    DeferredCode* deferred = NULL;
    if (is_postfix) {
      deferred = new DeferredPostfixCountOperation(new_value.reg(),
                                                   old_value.reg(),
                                                   is_increment);
    } else {
      deferred = new DeferredPrefixCountOperation(new_value.reg(),
                                                  is_increment);
    }

    if (is_increment) {
      __ add(Operand(new_value.reg()), Immediate(Smi::FromInt(1)));
    } else {
      __ sub(Operand(new_value.reg()), Immediate(Smi::FromInt(1)));
    }

    // If the count operation didn't overflow and the result is a valid
    // smi, we're done. Otherwise, we jump to the deferred slow-case
    // code.
    if (tmp.is_valid()) {
      // We combine the overflow and the smi tag check if we could
      // successfully allocate a temporary byte register.
      __ setcc(overflow, tmp.reg());
      __ or_(Operand(tmp.reg()), new_value.reg());
      __ test(tmp.reg(), Immediate(kSmiTagMask));
      tmp.Unuse();
      deferred->Branch(not_zero);
    } else {
      // Otherwise we test separately for overflow and smi tag.
      deferred->Branch(overflow);
      __ test(new_value.reg(), Immediate(kSmiTagMask));
      deferred->Branch(not_zero);
    }
    deferred->BindExit();

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


void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
  Comment cmnt(masm_, "[ BinaryOperation");
  Token::Value op = node->op();

  // According to ECMA-262 section 11.11, page 58, the binary logical
  // operators must yield the result of one of the two expressions
  // before any ToBoolean() conversions. This means that the value
  // produced by a && or || operator is not necessarily a boolean.

  // NOTE: If the left hand side produces a materialized value (not
  // control flow), we force the right hand side to do the same. This
  // is necessary because we assume that if we get control flow on the
  // last path out of an expression we got it on all paths.
  if (op == Token::AND) {
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

  } else if (op == Token::OR) {
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

  } else {
    // NOTE: The code below assumes that the slow cases (calls to runtime)
    // never return a constant/immutable object.
    OverwriteMode overwrite_mode = NO_OVERWRITE;
    if (node->left()->AsBinaryOperation() != NULL &&
        node->left()->AsBinaryOperation()->ResultOverwriteAllowed()) {
      overwrite_mode = OVERWRITE_LEFT;
    } else if (node->right()->AsBinaryOperation() != NULL &&
               node->right()->AsBinaryOperation()->ResultOverwriteAllowed()) {
      overwrite_mode = OVERWRITE_RIGHT;
    }

    Load(node->left());
    Load(node->right());
    GenericBinaryOperation(node->op(), node->type(), overwrite_mode);
  }
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
  frame_->PushFunction();
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
  Comment cmnt(masm_, "[ CompareOperation");

  bool left_already_loaded = false;

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
    Handle<String> check(String::cast(*right->AsLiteral()->handle()));

    // Load the operand and move it to a register.
    LoadTypeofExpression(operation->expression());
    Result answer = frame_->Pop();
    answer.ToRegister();

    if (check->Equals(Heap::number_symbol())) {
      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->true_target()->Branch(zero);
      frame_->Spill(answer.reg());
      __ mov(answer.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ cmp(answer.reg(), Factory::heap_number_map());
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::string_symbol())) {
      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->false_target()->Branch(zero);

      // It can be an undetectable string object.
      Result temp = allocator()->Allocate();
      ASSERT(temp.is_valid());
      __ mov(temp.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ movzx_b(temp.reg(), FieldOperand(temp.reg(), Map::kBitFieldOffset));
      __ test(temp.reg(), Immediate(1 << Map::kIsUndetectable));
      destination()->false_target()->Branch(not_zero);
      __ CmpObjectType(answer.reg(), FIRST_NONSTRING_TYPE, temp.reg());
      temp.Unuse();
      answer.Unuse();
      destination()->Split(below);

    } else if (check->Equals(Heap::boolean_symbol())) {
      __ cmp(answer.reg(), Factory::true_value());
      destination()->true_target()->Branch(equal);
      __ cmp(answer.reg(), Factory::false_value());
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::undefined_symbol())) {
      __ cmp(answer.reg(), Factory::undefined_value());
      destination()->true_target()->Branch(equal);

      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->false_target()->Branch(zero);

      // It can be an undetectable object.
      frame_->Spill(answer.reg());
      __ mov(answer.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ movzx_b(answer.reg(),
                 FieldOperand(answer.reg(), Map::kBitFieldOffset));
      __ test(answer.reg(), Immediate(1 << Map::kIsUndetectable));
      answer.Unuse();
      destination()->Split(not_zero);

    } else if (check->Equals(Heap::function_symbol())) {
      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->false_target()->Branch(zero);
      frame_->Spill(answer.reg());
      __ CmpObjectType(answer.reg(), JS_FUNCTION_TYPE, answer.reg());
      destination()->true_target()->Branch(equal);
      // Regular expressions are callable so typeof == 'function'.
      __ CmpInstanceType(answer.reg(), JS_REGEXP_TYPE);
      answer.Unuse();
      destination()->Split(equal);
    } else if (check->Equals(Heap::object_symbol())) {
      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->false_target()->Branch(zero);
      __ cmp(answer.reg(), Factory::null_value());
      destination()->true_target()->Branch(equal);

      Result map = allocator()->Allocate();
      ASSERT(map.is_valid());
      // Regular expressions are typeof == 'function', not 'object'.
      __ CmpObjectType(answer.reg(), JS_REGEXP_TYPE, map.reg());
      destination()->false_target()->Branch(equal);

      // It can be an undetectable object.
      __ movzx_b(map.reg(), FieldOperand(map.reg(), Map::kBitFieldOffset));
      __ test(map.reg(), Immediate(1 << Map::kIsUndetectable));
      destination()->false_target()->Branch(not_zero);
      __ mov(map.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ movzx_b(map.reg(), FieldOperand(map.reg(), Map::kInstanceTypeOffset));
      __ cmp(map.reg(), FIRST_JS_OBJECT_TYPE);
      destination()->false_target()->Branch(less);
      __ cmp(map.reg(), LAST_JS_OBJECT_TYPE);
      answer.Unuse();
      map.Unuse();
      destination()->Split(less_equal);
    } else {
      // Uncommon case: typeof testing against a string literal that is
      // never returned from the typeof operator.
      answer.Unuse();
      destination()->Goto(false);
    }
    return;
  } else if (op == Token::LT &&
             right->AsLiteral() != NULL &&
             right->AsLiteral()->handle()->IsHeapNumber()) {
    Handle<HeapNumber> check(HeapNumber::cast(*right->AsLiteral()->handle()));
    if (check->value() == 2147483648.0) {  // 0x80000000.
      Load(left);
      left_already_loaded = true;
      Result lhs = frame_->Pop();
      lhs.ToRegister();
      __ test(lhs.reg(), Immediate(kSmiTagMask));
      destination()->true_target()->Branch(zero);  // All Smis are less.
      Result scratch = allocator()->Allocate();
      ASSERT(scratch.is_valid());
      __ mov(scratch.reg(), FieldOperand(lhs.reg(), HeapObject::kMapOffset));
      __ cmp(scratch.reg(), Factory::heap_number_map());
      JumpTarget not_a_number;
      not_a_number.Branch(not_equal, &lhs);
      __ mov(scratch.reg(),
             FieldOperand(lhs.reg(), HeapNumber::kExponentOffset));
      __ cmp(Operand(scratch.reg()), Immediate(0xfff00000));
      not_a_number.Branch(above_equal, &lhs);  // It's a negative NaN or -Inf.
      const uint32_t borderline_exponent =
          (HeapNumber::kExponentBias + 31) << HeapNumber::kExponentShift;
      __ cmp(Operand(scratch.reg()), Immediate(borderline_exponent));
      scratch.Unuse();
      lhs.Unuse();
      destination()->true_target()->Branch(less);
      destination()->false_target()->Jump();

      not_a_number.Bind(&lhs);
      frame_->Push(&lhs);
    }
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
      if (!left_already_loaded) Load(left);
      Load(right);
      Result answer = frame_->InvokeBuiltin(Builtins::IN, CALL_FUNCTION, 2);
      frame_->Push(&answer);  // push the result
      return;
    }
    case Token::INSTANCEOF: {
      if (!left_already_loaded) Load(left);
      Load(right);
      InstanceofStub stub;
      Result answer = frame_->CallStub(&stub, 2);
      answer.ToRegister();
      __ test(answer.reg(), Operand(answer.reg()));
      answer.Unuse();
      destination()->Split(zero);
      return;
    }
    default:
      UNREACHABLE();
  }
  if (!left_already_loaded) Load(left);
  Load(right);
  Comparison(node, cc, strict, destination());
}


#ifdef DEBUG
bool CodeGenerator::HasValidEntryRegisters() {
  return (allocator()->count(eax) == (frame()->is_used(eax) ? 1 : 0))
      && (allocator()->count(ebx) == (frame()->is_used(ebx) ? 1 : 0))
      && (allocator()->count(ecx) == (frame()->is_used(ecx) ? 1 : 0))
      && (allocator()->count(edx) == (frame()->is_used(edx) ? 1 : 0))
      && (allocator()->count(edi) == (frame()->is_used(edi) ? 1 : 0));
}
#endif


// Emit a LoadIC call to get the value from receiver and leave it in
// dst.
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
  if (!receiver_.is(eax)) {
    __ mov(eax, receiver_);
  }
  __ Set(ecx, Immediate(name_));
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // The call must be followed by a test eax instruction to indicate
  // that the inobject property case was inlined.
  //
  // Store the delta to the map check instruction here in the test
  // instruction.  Use masm_-> instead of the __ macro since the
  // latter can't return a value.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  masm_->test(eax, Immediate(-delta_to_patch_site));
  __ IncrementCounter(&Counters::named_load_inline_miss, 1);

  if (!dst_.is(eax)) __ mov(dst_, eax);
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
  if (!receiver_.is(eax)) {
    // Register eax is available for key.
    if (!key_.is(eax)) {
      __ mov(eax, key_);
    }
    if (!receiver_.is(edx)) {
      __ mov(edx, receiver_);
    }
  } else if (!key_.is(edx)) {
    // Register edx is available for receiver.
    if (!receiver_.is(edx)) {
      __ mov(edx, receiver_);
    }
    if (!key_.is(eax)) {
      __ mov(eax, key_);
    }
  } else {
    __ xchg(edx, eax);
  }
  // Calculate the delta from the IC call instruction to the map check
  // cmp instruction in the inlined version.  This delta is stored in
  // a test(eax, delta) instruction after the call so that we can find
  // it in the IC initialization code and patch the cmp instruction.
  // This means that we cannot allow test instructions after calls to
  // KeyedLoadIC stubs in other places.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // The delta from the start of the map-compare instruction to the
  // test instruction.  We use masm_-> directly here instead of the __
  // macro because the macro sometimes uses macro expansion to turn
  // into something that can't return a value.  This is encountered
  // when doing generated code coverage tests.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  masm_->test(eax, Immediate(-delta_to_patch_site));
  __ IncrementCounter(&Counters::keyed_load_inline_miss, 1);

  if (!dst_.is(eax)) __ mov(dst_, eax);
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
  // Push receiver and key arguments on the stack.
  __ push(receiver_);
  __ push(key_);
  // Move value argument to eax as expected by the IC stub.
  if (!value_.is(eax)) __ mov(eax, value_);
  // Call the IC stub.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // The delta from the start of the map-compare instruction to the
  // test instruction.  We use masm_-> directly here instead of the
  // __ macro because the macro sometimes uses macro expansion to turn
  // into something that can't return a value.  This is encountered
  // when doing generated code coverage tests.
  int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(patch_site());
  // Here we use masm_-> instead of the __ macro because this is the
  // instruction that gets patched and coverage code gets in the way.
  masm_->test(eax, Immediate(-delta_to_patch_site));
  // Restore value (returned from store IC), key and receiver
  // registers.
  if (!value_.is(eax)) __ mov(value_, eax);
  __ pop(key_);
  __ pop(receiver_);
}


Result CodeGenerator::EmitNamedLoad(Handle<String> name, bool is_contextual) {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Result result;
  // Do not inline the inobject property case for loads from the global
  // object.  Also do not inline for unoptimized code.  This saves time in
  // the code generator.  Unoptimized code is toplevel code or code that is
  // not in a loop.
  if (is_contextual || scope()->is_global_scope() || loop_nesting() == 0) {
    Comment cmnt(masm(), "[ Load from named Property");
    frame()->Push(name);

    RelocInfo::Mode mode = is_contextual
        ? RelocInfo::CODE_TARGET_CONTEXT
        : RelocInfo::CODE_TARGET;
    result = frame()->CallLoadIC(mode);
    // A test eax instruction following the call signals that the inobject
    // property case was inlined.  Ensure that there is not a test eax
    // instruction here.
    __ nop();
  } else {
    // Inline the inobject property case.
    Comment cmnt(masm(), "[ Inlined named property load");
    Result receiver = frame()->Pop();
    receiver.ToRegister();

    result = allocator()->Allocate();
    ASSERT(result.is_valid());
    DeferredReferenceGetNamedValue* deferred =
        new DeferredReferenceGetNamedValue(result.reg(), receiver.reg(), name);

    // Check that the receiver is a heap object.
    __ test(receiver.reg(), Immediate(kSmiTagMask));
    deferred->Branch(zero);

    __ bind(deferred->patch_site());
    // This is the map check instruction that will be patched (so we can't
    // use the double underscore macro that may insert instructions).
    // Initially use an invalid map to force a failure.
    masm()->cmp(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                Immediate(Factory::null_value()));
    // This branch is always a forwards branch so it's always a fixed size
    // which allows the assert below to succeed and patching to work.
    deferred->Branch(not_equal);

    // The delta from the patch label to the load offset must be statically
    // known.
    ASSERT(masm()->SizeOfCodeGeneratedSince(deferred->patch_site()) ==
           LoadIC::kOffsetToLoadInstruction);
    // The initial (invalid) offset has to be large enough to force a 32-bit
    // instruction encoding to allow patching with an arbitrary offset.  Use
    // kMaxInt (minus kHeapObjectTag).
    int offset = kMaxInt;
    masm()->mov(result.reg(), FieldOperand(receiver.reg(), offset));

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
  Result result = frame()->CallStoreIC(name, is_contextual);

  ASSERT_EQ(expected_height, frame()->height());
  return result;
}


Result CodeGenerator::EmitKeyedLoad() {
#ifdef DEBUG
  int original_height = frame()->height();
#endif
  Result result;
  // Inline array load code if inside of a loop.  We do not know the
  // receiver map yet, so we initially generate the code with a check
  // against an invalid map.  In the inline cache code, we patch the map
  // check if appropriate.
  if (loop_nesting() > 0) {
    Comment cmnt(masm_, "[ Inlined load from keyed Property");

    Result key = frame_->Pop();
    Result receiver = frame_->Pop();
    key.ToRegister();
    receiver.ToRegister();

    // Use a fresh temporary to load the elements without destroying
    // the receiver which is needed for the deferred slow case.
    Result elements = allocator()->Allocate();
    ASSERT(elements.is_valid());

    // Use a fresh temporary for the index and later the loaded
    // value.
    result = allocator()->Allocate();
    ASSERT(result.is_valid());

    DeferredReferenceGetKeyedValue* deferred =
        new DeferredReferenceGetKeyedValue(result.reg(),
                                           receiver.reg(),
                                           key.reg());

    __ test(receiver.reg(), Immediate(kSmiTagMask));
    deferred->Branch(zero);

    // Initially, use an invalid map. The map is patched in the IC
    // initialization code.
    __ bind(deferred->patch_site());
    // Use masm-> here instead of the double underscore macro since extra
    // coverage code can interfere with the patching.
    masm_->cmp(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
              Immediate(Factory::null_value()));
    deferred->Branch(not_equal);

    // Check that the key is a smi.
    __ test(key.reg(), Immediate(kSmiTagMask));
    deferred->Branch(not_zero);

    // Get the elements array from the receiver and check that it
    // is not a dictionary.
    __ mov(elements.reg(),
           FieldOperand(receiver.reg(), JSObject::kElementsOffset));
    __ cmp(FieldOperand(elements.reg(), HeapObject::kMapOffset),
           Immediate(Factory::fixed_array_map()));
    deferred->Branch(not_equal);

    // Shift the key to get the actual index value and check that
    // it is within bounds.
    __ mov(result.reg(), key.reg());
    __ SmiUntag(result.reg());
    __ cmp(result.reg(),
           FieldOperand(elements.reg(), FixedArray::kLengthOffset));
    deferred->Branch(above_equal);

    // Load and check that the result is not the hole.
    __ mov(result.reg(), Operand(elements.reg(),
                                 result.reg(),
                                 times_4,
                                 FixedArray::kHeaderSize - kHeapObjectTag));
    elements.Unuse();
    __ cmp(Operand(result.reg()), Immediate(Factory::the_hole_value()));
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

    // Check that the value is a smi if it is not a constant.  We can skip
    // the write barrier for smis and constants.
    if (!value_is_constant) {
      __ test(result.reg(), Immediate(kSmiTagMask));
      deferred->Branch(not_zero);
    }

    // Check that the key is a non-negative smi.
    __ test(key.reg(), Immediate(kSmiTagMask | 0x80000000));
    deferred->Branch(not_zero);

    // Check that the receiver is not a smi.
    __ test(receiver.reg(), Immediate(kSmiTagMask));
    deferred->Branch(zero);

    // Check that the receiver is a JSArray.
    __ mov(tmp.reg(),
           FieldOperand(receiver.reg(), HeapObject::kMapOffset));
    __ movzx_b(tmp.reg(),
               FieldOperand(tmp.reg(), Map::kInstanceTypeOffset));
    __ cmp(tmp.reg(), JS_ARRAY_TYPE);
    deferred->Branch(not_equal);

    // Check that the key is within bounds.  Both the key and the length of
    // the JSArray are smis.
    __ cmp(key.reg(),
           FieldOperand(receiver.reg(), JSArray::kLengthOffset));
    deferred->Branch(greater_equal);

    // Get the elements array from the receiver and check that it is not a
    // dictionary.
    __ mov(tmp.reg(),
           FieldOperand(receiver.reg(), JSObject::kElementsOffset));
    // Bind the deferred code patch site to be able to locate the fixed
    // array map comparison.  When debugging, we patch this comparison to
    // always fail so that we will hit the IC call in the deferred code
    // which will allow the debugger to break for fast case stores.
    __ bind(deferred->patch_site());
    __ cmp(FieldOperand(tmp.reg(), HeapObject::kMapOffset),
           Immediate(Factory::fixed_array_map()));
    deferred->Branch(not_equal);

    // Store the value.
    __ mov(Operand(tmp.reg(),
                   key.reg(),
                   times_2,
                   FixedArray::kHeaderSize - kHeapObjectTag),
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
    frame()->Drop(2);
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
    return Handle<String>::cast(raw_name->handle());
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
      Result result =
          cgen_->LoadFromSlotCheckForArguments(slot, NOT_INSIDE_TYPEOF);
      if (!persist_after_get_) set_unloaded();
      cgen_->frame()->Push(&result);
      break;
    }

    case NAMED: {
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());
      if (persist_after_get_) cgen_->frame()->Dup();
      Result result = cgen_->EmitNamedLoad(GetName(), is_global);
      if (!persist_after_get_) set_unloaded();
      cgen_->frame()->Push(&result);
      break;
    }

    case KEYED: {
      if (persist_after_get_) {
        cgen_->frame()->PushElementAt(1);
        cgen_->frame()->PushElementAt(1);
      }
      Result value = cgen_->EmitKeyedLoad();
      cgen_->frame()->Push(&value);
      if (!persist_after_get_) set_unloaded();
      break;
    }

    default:
      UNREACHABLE();
  }
}


void Reference::TakeValue() {
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

  // Only non-constant, frame-allocated parameters and locals can
  // reach here. Be careful not to use the optimizations for arguments
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


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Clone the boilerplate in new space. Set the context to the
  // current context in esi.
  Label gc;
  __ AllocateInNewSpace(JSFunction::kSize, eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the boilerplate function from the stack.
  __ mov(edx, Operand(esp, 1 * kPointerSize));

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ mov(ecx, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(ecx, FieldOperand(ecx, GlobalObject::kGlobalContextOffset));
  __ mov(ecx, Operand(ecx, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ mov(FieldOperand(eax, JSObject::kMapOffset), ecx);

  // Clone the rest of the boilerplate fields. We don't have to update
  // the write barrier because the allocated object is in new space.
  for (int offset = kPointerSize;
       offset < JSFunction::kSize;
       offset += kPointerSize) {
    if (offset == JSFunction::kContextOffset) {
      __ mov(FieldOperand(eax, offset), esi);
    } else {
      __ mov(ebx, FieldOperand(edx, offset));
      __ mov(FieldOperand(eax, offset), ebx);
    }
  }

  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(ecx);  // Temporarily remove return address.
  __ pop(edx);
  __ push(esi);
  __ push(edx);
  __ push(ecx);  // Restore return address.
  __ TailCallRuntime(ExternalReference(Runtime::kNewClosure), 2, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace((length * kPointerSize) + FixedArray::kHeaderSize,
                        eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));

  // Setup the object header.
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), Factory::context_map());
  __ mov(FieldOperand(eax, Array::kLengthOffset), Immediate(length));

  // Setup the fixed slots.
  __ xor_(ebx, Operand(ebx));  // Set to NULL.
  __ mov(Operand(eax, Context::SlotOffset(Context::CLOSURE_INDEX)), ecx);
  __ mov(Operand(eax, Context::SlotOffset(Context::FCONTEXT_INDEX)), eax);
  __ mov(Operand(eax, Context::SlotOffset(Context::PREVIOUS_INDEX)), ebx);
  __ mov(Operand(eax, Context::SlotOffset(Context::EXTENSION_INDEX)), ebx);

  // Copy the global object from the surrounding context. We go through the
  // context in the function (ecx) to match the allocation behavior we have
  // in the runtime system (see Heap::AllocateFunctionContext).
  __ mov(ebx, FieldOperand(ecx, JSFunction::kContextOffset));
  __ mov(ebx, Operand(ebx, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(Operand(eax, Context::SlotOffset(Context::GLOBAL_INDEX)), ebx);

  // Initialize the rest of the slots to undefined.
  __ mov(ebx, Factory::undefined_value());
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ mov(Operand(eax, Context::SlotOffset(i)), ebx);
  }

  // Return and remove the on-stack parameter.
  __ mov(esi, Operand(eax));
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(ExternalReference(Runtime::kNewContext), 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [esp + kPointerSize]: constant elements.
  // [esp + (2 * kPointerSize)]: literal index.
  // [esp + (3 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into ecx and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ mov(ecx, Operand(esp, 3 * kPointerSize));
  __ mov(eax, Operand(esp, 2 * kPointerSize));
  ASSERT((kPointerSize == 4) && (kSmiTagSize == 1) && (kSmiTag == 0));
  __ mov(ecx, FieldOperand(ecx, eax, times_2, FixedArray::kHeaderSize));
  __ cmp(ecx, Factory::undefined_value());
  __ j(equal, &slow_case);

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size, eax, ebx, edx, &slow_case, TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ mov(ebx, FieldOperand(ecx, i));
      __ mov(FieldOperand(eax, i), ebx);
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ mov(ecx, FieldOperand(ecx, JSArray::kElementsOffset));
    __ lea(edx, Operand(eax, JSArray::kSize));
    __ mov(FieldOperand(eax, JSArray::kElementsOffset), edx);

    // Copy the elements array.
    for (int i = 0; i < elements_size; i += kPointerSize) {
      __ mov(ebx, FieldOperand(ecx, i));
      __ mov(FieldOperand(edx, i), ebx);
    }
  }

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  __ bind(&slow_case);
  ExternalReference runtime(Runtime::kCreateArrayLiteralShallow);
  __ TailCallRuntime(runtime, 3, 1);
}


// NOTE: The stub does not handle the inlined cases (Smis, Booleans, undefined).
void ToBooleanStub::Generate(MacroAssembler* masm) {
  Label false_result, true_result, not_string;
  __ mov(eax, Operand(esp, 1 * kPointerSize));

  // 'null' => false.
  __ cmp(eax, Factory::null_value());
  __ j(equal, &false_result);

  // Get the map and type of the heap object.
  __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(edx, Map::kInstanceTypeOffset));

  // Undetectable => false.
  __ movzx_b(ebx, FieldOperand(edx, Map::kBitFieldOffset));
  __ and_(ebx, 1 << Map::kIsUndetectable);
  __ j(not_zero, &false_result);

  // JavaScript object => true.
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(above_equal, &true_result);

  // String value => false iff empty.
  __ cmp(ecx, FIRST_NONSTRING_TYPE);
  __ j(above_equal, &not_string);
  __ mov(edx, FieldOperand(eax, String::kLengthOffset));
  __ test(edx, Operand(edx));
  __ j(zero, &false_result);
  __ jmp(&true_result);

  __ bind(&not_string);
  // HeapNumber => false iff +0, -0, or NaN.
  __ cmp(edx, Factory::heap_number_map());
  __ j(not_equal, &true_result);
  __ fldz();
  __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
  __ FCmp();
  __ j(zero, &false_result);
  // Fall through to |true_result|.

  // Return 1/0 for true/false in eax.
  __ bind(&true_result);
  __ mov(eax, 1);
  __ ret(1 * kPointerSize);
  __ bind(&false_result);
  __ mov(eax, 0);
  __ ret(1 * kPointerSize);
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Register left,
    Register right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ push(left);
    __ push(right);
  } else {
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (!(left.is(left_arg) && right.is(right_arg))) {
      if (left.is(right_arg) && right.is(left_arg)) {
        if (IsOperationCommutative()) {
          SetArgsReversed();
        } else {
          __ xchg(left, right);
        }
      } else if (left.is(left_arg)) {
        __ mov(right_arg, right);
      } else if (right.is(right_arg)) {
        __ mov(left_arg, left);
      } else if (left.is(right_arg)) {
        if (IsOperationCommutative()) {
          __ mov(left_arg, right);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying left argument.
          __ mov(left_arg, left);
          __ mov(right_arg, right);
        }
      } else if (right.is(left_arg)) {
        if (IsOperationCommutative()) {
          __ mov(right_arg, left);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying right argument.
          __ mov(right_arg, right);
          __ mov(left_arg, left);
        }
      } else {
        // Order of moves is not important.
        __ mov(left_arg, left);
        __ mov(right_arg, right);
      }
    }

    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Register left,
    Smi* right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ push(left);
    __ push(Immediate(right));
  } else {
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (left.is(left_arg)) {
      __ mov(right_arg, Immediate(right));
    } else if (left.is(right_arg) && IsOperationCommutative()) {
      __ mov(left_arg, Immediate(right));
      SetArgsReversed();
    } else {
      // For non-commutative operations, left and right_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite left before moving
      // it to left_arg.
      __ mov(left_arg, left);
      __ mov(right_arg, Immediate(right));
    }

    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Smi* left,
    Register right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ push(Immediate(left));
    __ push(right);
  } else {
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (right.is(right_arg)) {
      __ mov(left_arg, Immediate(left));
    } else if (right.is(left_arg) && IsOperationCommutative()) {
      __ mov(right_arg, Immediate(left));
      SetArgsReversed();
    } else {
      // For non-commutative operations, right and left_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite right before moving
      // it to right_arg.
      __ mov(right_arg, right);
      __ mov(left_arg, Immediate(left));
    }
    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


Result GenericBinaryOpStub::GenerateCall(MacroAssembler* masm,
                                         VirtualFrame* frame,
                                         Result* left,
                                         Result* right) {
  if (ArgsInRegistersSupported()) {
    SetArgsInRegisters();
    return frame->CallStub(this, left, right);
  } else {
    frame->Push(left);
    frame->Push(right);
    return frame->CallStub(this, 2);
  }
}


void GenericBinaryOpStub::GenerateSmiCode(MacroAssembler* masm, Label* slow) {
  // 1. Move arguments into edx, eax except for DIV and MOD, which need the
  // dividend in eax and edx free for the division.  Use eax, ebx for those.
  Comment load_comment(masm, "-- Load arguments");
  Register left = edx;
  Register right = eax;
  if (op_ == Token::DIV || op_ == Token::MOD) {
    left = eax;
    right = ebx;
    if (HasArgsInRegisters()) {
      __ mov(ebx, eax);
      __ mov(eax, edx);
    }
  }
  if (!HasArgsInRegisters()) {
    __ mov(right, Operand(esp, 1 * kPointerSize));
    __ mov(left, Operand(esp, 2 * kPointerSize));
  }

  // 2. Prepare the smi check of both operands by oring them together.
  Comment smi_check_comment(masm, "-- Smi check arguments");
  Label not_smis;
  Register combined = ecx;
  ASSERT(!left.is(combined) && !right.is(combined));
  switch (op_) {
    case Token::BIT_OR:
      // Perform the operation into eax and smi check the result.  Preserve
      // eax in case the result is not a smi.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));  // Bitwise or is commutative.
      combined = right;
      break;

    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      __ mov(combined, right);
      __ or_(combined, Operand(left));
      break;

    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Move the right operand into ecx for the shift operation, use eax
      // for the smi check register.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));
      combined = right;
      break;

    default:
      break;
  }

  // 3. Perform the smi check of the operands.
  ASSERT(kSmiTag == 0);  // Adjust zero check if not the case.
  __ test(combined, Immediate(kSmiTagMask));
  __ j(not_zero, &not_smis, not_taken);

  // 4. Operands are both smis, perform the operation leaving the result in
  // eax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op_) {
    case Token::BIT_OR:
      // Nothing to do.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(eax));
      __ xor_(right, Operand(left));  // Bitwise xor is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(eax));
      __ and_(right, Operand(left));  // Bitwise and is commutative.
      break;

    case Token::SHL:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shl_cl(left);
      // Check that the *signed* result fits in a smi.
      __ cmp(left, 0xc0000000);
      __ j(sign, &use_fp_on_smis, not_taken);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::SAR:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ sar_cl(left);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::SHR:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shr_cl(left);
      // Check that the *unsigned* result fits in a smi.
      // Neither of the two high-order bits can be set:
      // - 0x80000000: high bit would be lost when smi tagging.
      // - 0x40000000: this number would convert to negative when
      // Smi tagging these two cases can only happen with shifts
      // by 0 or 1 when handed a valid smi.
      __ test(left, Immediate(0xc0000000));
      __ j(not_zero, slow, not_taken);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::ADD:
      ASSERT(right.is(eax));
      __ add(right, Operand(left));  // Addition is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      break;

    case Token::SUB:
      __ sub(left, Operand(right));
      __ j(overflow, &use_fp_on_smis, not_taken);
      __ mov(eax, left);
      break;

    case Token::MUL:
      // If the smi tag is 0 we can just leave the tag on one operand.
      ASSERT(kSmiTag == 0);  // Adjust code below if not the case.
      // We can't revert the multiplication if the result is not a smi
      // so save the right operand.
      __ mov(ebx, right);
      // Remove tag from one of the operands (but keep sign).
      __ SmiUntag(right);
      // Do multiplication.
      __ imul(right, Operand(left));  // Multiplication is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(right, combined, &use_fp_on_smis);
      break;

    case Token::DIV:
      // We can't revert the division if the result is not a smi so
      // save the left operand.
      __ mov(edi, left);
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &use_fp_on_smis, not_taken);
      // Sign extend left into edx:eax.
      ASSERT(left.is(eax));
      __ cdq();
      // Divide edx:eax by right.
      __ idiv(right);
      // Check for the corner case of dividing the most negative smi by
      // -1. We cannot use the overflow flag, since it is not set by idiv
      // instruction.
      ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
      __ cmp(eax, 0x40000000);
      __ j(equal, &use_fp_on_smis);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(eax, combined, &use_fp_on_smis);
      // Check that the remainder is zero.
      __ test(edx, Operand(edx));
      __ j(not_zero, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(eax);
      break;

    case Token::MOD:
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &not_smis, not_taken);

      // Sign extend left into edx:eax.
      ASSERT(left.is(eax));
      __ cdq();
      // Divide edx:eax by right.
      __ idiv(right);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(edx, combined, slow);
      // Move remainder to register eax.
      __ mov(eax, edx);
      break;

    default:
      UNREACHABLE();
  }

  // 5. Emit return of result in eax.
  GenerateReturn(masm);

  // 6. For some operations emit inline code to perform floating point
  // operations on known smis (e.g., if the result of the operation
  // overflowed the smi range).
  switch (op_) {
    case Token::SHL: {
      Comment perform_float(masm, "-- Perform float operation on smis");
      __ bind(&use_fp_on_smis);
      // Result we want is in left == edx, so we can put the allocated heap
      // number in eax.
      __ AllocateHeapNumber(eax, ecx, ebx, slow);
      // Store the result in the HeapNumber and return.
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        __ cvtsi2sd(xmm0, Operand(left));
        __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
      } else {
        // It's OK to overwrite the right argument on the stack because we
        // are about to return.
        __ mov(Operand(esp, 1 * kPointerSize), left);
        __ fild_s(Operand(esp, 1 * kPointerSize));
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
      }
      GenerateReturn(masm);
      break;
    }

    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Comment perform_float(masm, "-- Perform float operation on smis");
      __ bind(&use_fp_on_smis);
      // Restore arguments to edx, eax.
      switch (op_) {
        case Token::ADD:
          // Revert right = right + left.
          __ sub(right, Operand(left));
          break;
        case Token::SUB:
          // Revert left = left - right.
          __ add(left, Operand(right));
          break;
        case Token::MUL:
          // Right was clobbered but a copy is in ebx.
          __ mov(right, ebx);
          break;
        case Token::DIV:
          // Left was clobbered but a copy is in edi.  Right is in ebx for
          // division.
          __ mov(edx, edi);
          __ mov(eax, right);
          break;
        default: UNREACHABLE();
          break;
      }
      __ AllocateHeapNumber(ecx, ebx, no_reg, slow);
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        FloatingPointHelper::LoadSSE2Smis(masm, ebx);
        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm0);
      } else {  // SSE2 not available, use FPU.
        FloatingPointHelper::LoadFloatSmis(masm, ebx);
        switch (op_) {
          case Token::ADD: __ faddp(1); break;
          case Token::SUB: __ fsubp(1); break;
          case Token::MUL: __ fmulp(1); break;
          case Token::DIV: __ fdivp(1); break;
          default: UNREACHABLE();
        }
        __ fstp_d(FieldOperand(ecx, HeapNumber::kValueOffset));
      }
      __ mov(eax, ecx);
      GenerateReturn(masm);
      break;
    }

    default:
      break;
  }

  // 7. Non-smi operands, fall out to the non-smi code with the operands in
  // edx and eax.
  Comment done_comment(masm, "-- Enter non-smi code");
  __ bind(&not_smis);
  switch (op_) {
    case Token::BIT_OR:
    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Right operand is saved in ecx and eax was destroyed by the smi
      // check.
      __ mov(eax, ecx);
      break;

    case Token::DIV:
    case Token::MOD:
      // Operands are in eax, ebx at this point.
      __ mov(edx, eax);
      __ mov(eax, ebx);
      break;

    default:
      break;
  }
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  Label call_runtime;

  __ IncrementCounter(&Counters::generic_binary_stub_calls, 1);

  // Generate fast case smi code if requested. This flag is set when the fast
  // case smi code is not generated by the caller. Generating it here will speed
  // up common operations.
  if (HasSmiCodeInStub()) {
    GenerateSmiCode(masm, &call_runtime);
  } else if (op_ != Token::MOD) {  // MOD goes straight to runtime.
    GenerateLoadArguments(masm);
  }

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        if (NumberInfo::IsNumber(operands_type_)) {
          if (FLAG_debug_code) {
            // Assert at runtime that inputs are only numbers.
            __ AbortIfNotNumber(edx,
                                "GenericBinaryOpStub operand not a number.");
            __ AbortIfNotNumber(eax,
                                "GenericBinaryOpStub operand not a number.");
          }
          FloatingPointHelper::LoadSSE2Operands(masm);
        } else {
          FloatingPointHelper::LoadSSE2Operands(masm, &call_runtime);
        }

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        GenerateHeapResultAllocation(masm, &call_runtime);
        __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        GenerateReturn(masm);
      } else {  // SSE2 not available, use FPU.
        if (NumberInfo::IsNumber(operands_type_)) {
          if (FLAG_debug_code) {
            // Assert at runtime that inputs are only numbers.
            __ AbortIfNotNumber(edx,
                                "GenericBinaryOpStub operand not a number.");
            __ AbortIfNotNumber(eax,
                                "GenericBinaryOpStub operand not a number.");
          }
        } else {
          FloatingPointHelper::CheckFloatOperands(masm, &call_runtime, ebx);
        }
        FloatingPointHelper::LoadFloatOperands(
            masm,
            ecx,
            FloatingPointHelper::ARGS_IN_REGISTERS);
        switch (op_) {
          case Token::ADD: __ faddp(1); break;
          case Token::SUB: __ fsubp(1); break;
          case Token::MUL: __ fmulp(1); break;
          case Token::DIV: __ fdivp(1); break;
          default: UNREACHABLE();
        }
        Label after_alloc_failure;
        GenerateHeapResultAllocation(masm, &after_alloc_failure);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        GenerateReturn(masm);
        __ bind(&after_alloc_failure);
        __ ffree();
        __ jmp(&call_runtime);
      }
    }
    case Token::MOD: {
      // For MOD we go directly to runtime in the non-smi case.
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      Label non_smi_result;
      FloatingPointHelper::LoadAsIntegers(masm, use_sse3_, &call_runtime);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
        case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
        case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
        case Token::SAR: __ sar_cl(eax); break;
        case Token::SHL: __ shl_cl(eax); break;
        case Token::SHR: __ shr_cl(eax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative and fits in a smi.
        __ test(eax, Immediate(0xc0000000));
        __ j(not_zero, &call_runtime);
      } else {
        // Check if result fits in a smi.
        __ cmp(eax, 0xc0000000);
        __ j(negative, &non_smi_result);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      GenerateReturn(masm);

      // All ops except SHR return a signed int32 that we load in a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, Operand(eax));  // ebx: result
        Label skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ test(eax, Immediate(kSmiTagMask));
            __ j(not_zero, &skip_allocation, not_taken);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(ebx));
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          __ mov(Operand(esp, 1 * kPointerSize), ebx);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        }
        GenerateReturn(masm);
      }
      break;
    }
    default: UNREACHABLE(); break;
  }

  // If all else fails, use the runtime system to get the correct
  // result. If arguments was passed in registers now place them on the
  // stack in the correct order below the return address.
  __ bind(&call_runtime);
  if (HasArgsInRegisters()) {
    __ pop(ecx);
    if (HasArgsReversed()) {
      __ push(eax);
      __ push(edx);
    } else {
      __ push(edx);
      __ push(eax);
    }
    __ push(ecx);
  }
  switch (op_) {
    case Token::ADD: {
      // Test for string arguments before calling runtime.
      Label not_strings, not_string1, string1, string1_smi2;
      Result answer;
      __ test(edx, Immediate(kSmiTagMask));
      __ j(zero, &not_string1);
      __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, ecx);
      __ j(above_equal, &not_string1);

      // First argument is a string, test second.
      __ test(eax, Immediate(kSmiTagMask));
      __ j(zero, &string1_smi2);
      __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ecx);
      __ j(above_equal, &string1);

      // First and second argument are strings. Jump to the string add stub.
      StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
      __ TailCallStub(&string_add_stub);

      __ bind(&string1_smi2);
      // First argument is a string, second is a smi. Try to lookup the number
      // string for the smi in the number string cache.
      NumberToStringStub::GenerateLookupNumberStringCache(
          masm, eax, edi, ebx, ecx, true, &string1);

      // Call the string add stub to make the result.
      __ EnterInternalFrame();
      __ push(edx);  // Original first argument.
      __ push(edi);  // Number to string result for second argument.
      __ CallStub(&string_add_stub);
      __ LeaveInternalFrame();
      __ ret(2 * kPointerSize);

      __ bind(&string1);
      __ InvokeBuiltin(
          HasArgsReversed() ?
              Builtins::STRING_ADD_RIGHT :
              Builtins::STRING_ADD_LEFT,
          JUMP_FUNCTION);

      // First argument was not a string, test second.
      __ bind(&not_string1);
      __ test(eax, Immediate(kSmiTagMask));
      __ j(zero, &not_strings);
      __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ecx);
      __ j(above_equal, &not_strings);

      // Only second argument is a string.
      __ InvokeBuiltin(
          HasArgsReversed() ?
              Builtins::STRING_ADD_LEFT :
              Builtins::STRING_ADD_RIGHT,
          JUMP_FUNCTION);

      __ bind(&not_strings);
      // Neither argument is a string.
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    }
    case Token::SUB:
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
      break;
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_FUNCTION);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_FUNCTION);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_FUNCTION);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_FUNCTION);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_FUNCTION);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void GenericBinaryOpStub::GenerateHeapResultAllocation(MacroAssembler* masm,
                                                       Label* alloc_failure) {
  Label skip_allocation;
  OverwriteMode mode = mode_;
  if (HasArgsReversed()) {
    if (mode == OVERWRITE_RIGHT) {
      mode = OVERWRITE_LEFT;
    } else if (mode == OVERWRITE_LEFT) {
      mode = OVERWRITE_RIGHT;
    }
  }
  switch (mode) {
    case OVERWRITE_LEFT: {
      // If the argument in edx is already an object, we skip the
      // allocation of a heap number.
      __ test(edx, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now edx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(edx, Operand(ebx));
      __ bind(&skip_allocation);
      // Use object in edx as a result holder
      __ mov(eax, Operand(edx));
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in eax is already an object, we skip the
      // allocation of a heap number.
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
      // Fall through!
    case NO_OVERWRITE:
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now eax can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(eax, ebx);
      __ bind(&skip_allocation);
      break;
    default: UNREACHABLE();
  }
}


void GenericBinaryOpStub::GenerateLoadArguments(MacroAssembler* masm) {
  // If arguments are not passed in registers read them from the stack.
  if (!HasArgsInRegisters()) {
    __ mov(eax, Operand(esp, 1 * kPointerSize));
    __ mov(edx, Operand(esp, 2 * kPointerSize));
  }
}


void GenericBinaryOpStub::GenerateReturn(MacroAssembler* masm) {
  // If arguments are not passed in registers remove them from the stack before
  // returning.
  if (!HasArgsInRegisters()) {
    __ ret(2 * kPointerSize);  // Remove both operands
  } else {
    __ ret(0);
  }
}


// Get the integer part of a heap number.  Surprisingly, all this bit twiddling
// is faster than using the built-in instructions on floating point registers.
// Trashes edi and ebx.  Dest is ecx.  Source cannot be ecx or one of the
// trashed registers.
void IntegerConvert(MacroAssembler* masm,
                    Register source,
                    bool use_sse3,
                    Label* conversion_failure) {
  ASSERT(!source.is(ecx) && !source.is(edi) && !source.is(ebx));
  Label done, right_exponent, normal_exponent;
  Register scratch = ebx;
  Register scratch2 = edi;
  // Get exponent word.
  __ mov(scratch, FieldOperand(source, HeapNumber::kExponentOffset));
  // Get exponent alone in scratch2.
  __ mov(scratch2, scratch);
  __ and_(scratch2, HeapNumber::kExponentMask);
  if (use_sse3) {
    CpuFeatures::Scope scope(SSE3);
    // Check whether the exponent is too big for a 64 bit signed integer.
    static const uint32_t kTooBigExponent =
        (HeapNumber::kExponentBias + 63) << HeapNumber::kExponentShift;
    __ cmp(Operand(scratch2), Immediate(kTooBigExponent));
    __ j(greater_equal, conversion_failure);
    // Load x87 register with heap number.
    __ fld_d(FieldOperand(source, HeapNumber::kValueOffset));
    // Reserve space for 64 bit answer.
    __ sub(Operand(esp), Immediate(sizeof(uint64_t)));  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(ecx, Operand(esp, 0));  // Load low word of answer into ecx.
    __ add(Operand(esp), Immediate(sizeof(uint64_t)));  // Nolint.
  } else {
    // Load ecx with zero.  We use this either for the final shift or
    // for the answer.
    __ xor_(ecx, Operand(ecx));
    // Check whether the exponent matches a 32 bit signed int that cannot be
    // represented by a Smi.  A non-smi 32 bit integer is 1.xxx * 2^30 so the
    // exponent is 30 (biased).  This is the exponent that we are fastest at and
    // also the highest exponent we can handle here.
    const uint32_t non_smi_exponent =
        (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
    __ cmp(Operand(scratch2), Immediate(non_smi_exponent));
    // If we have a match of the int32-but-not-Smi exponent then skip some
    // logic.
    __ j(equal, &right_exponent);
    // If the exponent is higher than that then go to slow case.  This catches
    // numbers that don't fit in a signed int32, infinities and NaNs.
    __ j(less, &normal_exponent);

    {
      // Handle a big exponent.  The only reason we have this code is that the
      // >>> operator has a tendency to generate numbers with an exponent of 31.
      const uint32_t big_non_smi_exponent =
          (HeapNumber::kExponentBias + 31) << HeapNumber::kExponentShift;
      __ cmp(Operand(scratch2), Immediate(big_non_smi_exponent));
      __ j(not_equal, conversion_failure);
      // We have the big exponent, typically from >>>.  This means the number is
      // in the range 2^31 to 2^32 - 1.  Get the top bits of the mantissa.
      __ mov(scratch2, scratch);
      __ and_(scratch2, HeapNumber::kMantissaMask);
      // Put back the implicit 1.
      __ or_(scratch2, 1 << HeapNumber::kExponentShift);
      // Shift up the mantissa bits to take up the space the exponent used to
      // take. We just orred in the implicit bit so that took care of one and
      // we want to use the full unsigned range so we subtract 1 bit from the
      // shift distance.
      const int big_shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 1;
      __ shl(scratch2, big_shift_distance);
      // Get the second half of the double.
      __ mov(ecx, FieldOperand(source, HeapNumber::kMantissaOffset));
      // Shift down 21 bits to get the most significant 11 bits or the low
      // mantissa word.
      __ shr(ecx, 32 - big_shift_distance);
      __ or_(ecx, Operand(scratch2));
      // We have the answer in ecx, but we may need to negate it.
      __ test(scratch, Operand(scratch));
      __ j(positive, &done);
      __ neg(ecx);
      __ jmp(&done);
    }

    __ bind(&normal_exponent);
    // Exponent word in scratch, exponent part of exponent word in scratch2.
    // Zero in ecx.
    // We know the exponent is smaller than 30 (biased).  If it is less than
    // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
    // it rounds to zero.
    const uint32_t zero_exponent =
        (HeapNumber::kExponentBias + 0) << HeapNumber::kExponentShift;
    __ sub(Operand(scratch2), Immediate(zero_exponent));
    // ecx already has a Smi zero.
    __ j(less, &done);

    // We have a shifted exponent between 0 and 30 in scratch2.
    __ shr(scratch2, HeapNumber::kExponentShift);
    __ mov(ecx, Immediate(30));
    __ sub(ecx, Operand(scratch2));

    __ bind(&right_exponent);
    // Here ecx is the shift, scratch is the exponent word.
    // Get the top bits of the mantissa.
    __ and_(scratch, HeapNumber::kMantissaMask);
    // Put back the implicit 1.
    __ or_(scratch, 1 << HeapNumber::kExponentShift);
    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We have kExponentShift + 1 significant bits int he low end of the
    // word.  Shift them to the top bits.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    __ shl(scratch, shift_distance);
    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    __ mov(scratch2, FieldOperand(source, HeapNumber::kMantissaOffset));
    // Shift down 22 bits to get the most significant 10 bits or the low
    // mantissa word.
    __ shr(scratch2, 32 - shift_distance);
    __ or_(scratch2, Operand(scratch));
    // Move down according to the exponent.
    __ shr_cl(scratch2);
    // Now the unsigned answer is in scratch2.  We need to move it to ecx and
    // we may need to fix the sign.
    Label negative;
    __ xor_(ecx, Operand(ecx));
    __ cmp(ecx, FieldOperand(source, HeapNumber::kExponentOffset));
    __ j(greater, &negative);
    __ mov(ecx, scratch2);
    __ jmp(&done);
    __ bind(&negative);
    __ sub(ecx, Operand(scratch2));
    __ bind(&done);
  }
}


// Input: edx, eax are the left and right objects of a bit op.
// Output: eax, ecx are left and right integers for a bit op.
void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         bool use_sse3,
                                         Label* conversion_failure) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &arg1_is_object);
  __ SmiUntag(edx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ cmp(edx, Factory::undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(edx, Immediate(0));
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ebx, Factory::heap_number_map());
  __ j(not_equal, &check_undefined_arg1);
  // Get the untagged integer version of the edx heap number in ecx.
  IntegerConvert(masm, edx, use_sse3, conversion_failure);
  __ mov(edx, ecx);

  // Here edx has the untagged integer, eax has a Smi or a heap number.
  __ bind(&load_arg2);
  // Test if arg2 is a Smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &arg2_is_object);
  __ SmiUntag(eax);
  __ mov(ecx, eax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(ecx, Immediate(0));
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(ebx, Factory::heap_number_map());
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the eax heap number in ecx.
  IntegerConvert(masm, eax, use_sse3, conversion_failure);
  __ bind(&done);
  __ mov(eax, edx);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ test(number, Immediate(kSmiTagMask));
  __ j(zero, &load_smi, not_taken);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi);
  __ SmiUntag(number);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm) {
  Label load_smi_edx, load_eax, load_smi_eax, done;
  // Load operand in edx into xmm0.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_edx, not_taken);  // Argument in edx is a smi.
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));

  __ bind(&load_eax);
  // Load operand in eax into xmm1.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_eax, not_taken);  // Argument in eax is a smi.
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, Operand(edx));
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);

  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, Operand(eax));
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm,
                                           Label* not_numbers) {
  Label load_smi_edx, load_eax, load_smi_eax, load_float_eax, done;
  // Load operand in edx into xmm0, or branch to not_numbers.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_edx, not_taken);  // Argument in edx is a smi.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(not_equal, not_numbers);  // Argument in edx is not a number.
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ bind(&load_eax);
  // Load operand in eax into xmm1, or branch to not_numbers.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_eax, not_taken);  // Argument in eax is a smi.
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(equal, &load_float_eax);
  __ jmp(not_numbers);  // Argument in eax is not a number.
  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, Operand(edx));
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);
  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, Operand(eax));
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.
  __ jmp(&done);
  __ bind(&load_float_eax);
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Smis(MacroAssembler* masm,
                                       Register scratch) {
  const Register left = edx;
  const Register right = eax;
  __ mov(scratch, left);
  ASSERT(!scratch.is(right));  // We're about to clobber scratch.
  __ SmiUntag(scratch);
  __ cvtsi2sd(xmm0, Operand(scratch));

  __ mov(scratch, right);
  __ SmiUntag(scratch);
  __ cvtsi2sd(xmm1, Operand(scratch));
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            Register scratch,
                                            ArgLocation arg_location) {
  Label load_smi_1, load_smi_2, done_load_1, done;
  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, edx);
  } else {
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
  }
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_1, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ bind(&done_load_1);

  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, eax);
  } else {
    __ mov(scratch, Operand(esp, 1 * kPointerSize));
  }
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_2, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_1);
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);
  __ jmp(&done_load_1);

  __ bind(&load_smi_2);
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);

  __ bind(&done);
}


void FloatingPointHelper::LoadFloatSmis(MacroAssembler* masm,
                                        Register scratch) {
  const Register left = edx;
  const Register right = eax;
  __ mov(scratch, left);
  ASSERT(!scratch.is(right));  // We're about to clobber scratch.
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));

  __ mov(scratch, right);
  __ SmiUntag(scratch);
  __ mov(Operand(esp, 0), scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);
}


void FloatingPointHelper::CheckFloatOperands(MacroAssembler* masm,
                                             Label* non_float,
                                             Register scratch) {
  Label test_other, done;
  // Test if both operands are floats or smi -> scratch=k_is_float;
  // Otherwise scratch = k_not_float.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &test_other, not_taken);  // argument in edx is OK
  __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(scratch, Factory::heap_number_map());
  __ j(not_equal, non_float);  // argument in edx is not a number -> NaN

  __ bind(&test_other);
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &done);  // argument in eax is OK
  __ mov(scratch, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(scratch, Factory::heap_number_map());
  __ j(not_equal, non_float);  // argument in eax is not a number -> NaN

  // Fall-through: Both operands are numbers.
  __ bind(&done);
}


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done;

  if (op_ == Token::SUB) {
    // Check whether the value is a smi.
    Label try_float;
    __ test(eax, Immediate(kSmiTagMask));
    __ j(not_zero, &try_float, not_taken);

    // Go slow case if the value of the expression is zero
    // to make sure that we switch between 0 and -0.
    __ test(eax, Operand(eax));
    __ j(zero, &slow, not_taken);

    // The value of the expression is a smi that is not zero.  Try
    // optimistic subtraction '0 - value'.
    Label undo;
    __ mov(edx, Operand(eax));
    __ Set(eax, Immediate(0));
    __ sub(eax, Operand(edx));
    __ j(overflow, &undo, not_taken);

    // If result is a smi we are done.
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &done, taken);

    // Restore eax and go slow case.
    __ bind(&undo);
    __ mov(eax, Operand(edx));
    __ jmp(&slow);

    // Try floating point case.
    __ bind(&try_float);
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ cmp(edx, Factory::heap_number_map());
    __ j(not_equal, &slow);
    if (overwrite_) {
      __ mov(edx, FieldOperand(eax, HeapNumber::kExponentOffset));
      __ xor_(edx, HeapNumber::kSignMask);  // Flip sign.
      __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), edx);
    } else {
      __ mov(edx, Operand(eax));
      // edx: operand
      __ AllocateHeapNumber(eax, ebx, ecx, &undo);
      // eax: allocated 'empty' number
      __ mov(ecx, FieldOperand(edx, HeapNumber::kExponentOffset));
      __ xor_(ecx, HeapNumber::kSignMask);  // Flip sign.
      __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), ecx);
      __ mov(ecx, FieldOperand(edx, HeapNumber::kMantissaOffset));
      __ mov(FieldOperand(eax, HeapNumber::kMantissaOffset), ecx);
    }
  } else if (op_ == Token::BIT_NOT) {
    // Check if the operand is a heap number.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ cmp(edx, Factory::heap_number_map());
    __ j(not_equal, &slow, not_taken);

    // Convert the heap number in eax to an untagged integer in ecx.
    IntegerConvert(masm, eax, CpuFeatures::IsSupported(SSE3), &slow);

    // Do the bitwise operation and check if the result fits in a smi.
    Label try_float;
    __ not_(ecx);
    __ cmp(ecx, 0xc0000000);
    __ j(sign, &try_float, not_taken);

    // Tag the result as a smi and we're done.
    ASSERT(kSmiTagSize == 1);
    __ lea(eax, Operand(ecx, times_2, kSmiTag));
    __ jmp(&done);

    // Try to store the result in a heap number.
    __ bind(&try_float);
    if (!overwrite_) {
      // Allocate a fresh heap number, but don't overwrite eax until
      // we're sure we can do it without going through the slow case
      // that needs the value in eax.
      __ AllocateHeapNumber(ebx, edx, edi, &slow);
      __ mov(eax, Operand(ebx));
    }
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatures::Scope use_sse2(SSE2);
      __ cvtsi2sd(xmm0, Operand(ecx));
      __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
    } else {
      __ push(ecx);
      __ fild_s(Operand(esp, 0));
      __ pop(ecx);
      __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    }
  } else {
    UNIMPLEMENTED();
  }

  // Return from the stub.
  __ bind(&done);
  __ StubReturn(1);

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
  __ pop(ecx);  // pop return address.
  __ push(eax);
  __ push(ecx);  // push return address
  switch (op_) {
    case Token::SUB:
      __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_FUNCTION);
      break;
    case Token::BIT_NOT:
      __ InvokeBuiltin(Builtins::BIT_NOT, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  // Check if the calling frame is an arguments adaptor frame.
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(ecx), Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame and return it.
  // Otherwise nothing to do: The number of formal parameters has already been
  // passed in register eax by calling function. Just return it.
  if (CpuFeatures::IsSupported(CMOV)) {
    CpuFeatures::Scope use_cmov(CMOV);
    __ cmov(equal, eax,
            Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  } else {
    Label exit;
    __ j(not_equal, &exit);
    __ mov(eax, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ bind(&exit);
  }
  __ ret(0);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in edx and the parameter count is in eax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(ebx, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(ecx), Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register eax. Use unsigned comparison to get negative
  // check for free.
  __ cmp(edx, Operand(eax));
  __ j(above_equal, &slow, not_taken);

  // Read the argument from the stack and return it.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);  // shifting code depends on this
  __ lea(ebx, Operand(ebp, eax, times_2, 0));
  __ neg(edx);
  __ mov(eax, Operand(ebx, edx, times_2, kDisplacement));
  __ ret(0);

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ mov(ecx, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmp(edx, Operand(ecx));
  __ j(above_equal, &slow, not_taken);

  // Read the argument from the stack and return it.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);  // shifting code depends on this
  __ lea(ebx, Operand(ebx, ecx, times_2, 0));
  __ neg(edx);
  __ mov(eax, Operand(ebx, edx, times_2, kDisplacement));
  __ ret(0);

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ pop(ebx);  // Return address.
  __ push(edx);
  __ push(ebx);
  __ TailCallRuntime(ExternalReference(Runtime::kGetArgumentsProperty), 1, 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[4] : number of parameters
  // esp[8] : receiver displacement
  // esp[16] : function

  // The displacement is used for skipping the return address and the
  // frame pointer on the stack. It is the offset of the last
  // parameter (if any) relative to the frame pointer.
  static const int kDisplacement = 2 * kPointerSize;

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(ecx), Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(Operand(esp, 1 * kPointerSize), ecx);
  __ lea(edx, Operand(edx, ecx, times_2, kDisplacement));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ test(ecx, Operand(ecx));
  __ j(zero, &add_arguments_object);
  __ lea(ecx, Operand(ecx, times_2, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ add(Operand(ecx), Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(ecx, eax, edx, ebx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ mov(edi, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(edi, FieldOperand(edi, GlobalObject::kGlobalContextOffset));
  __ mov(edi, Operand(edi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ mov(ebx, FieldOperand(edi, i));
    __ mov(FieldOperand(eax, i), ebx);
  }

  // Setup the callee in-object property.
  ASSERT(Heap::arguments_callee_index == 0);
  __ mov(ebx, Operand(esp, 3 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize), ebx);

  // Get the length (smi tagged) and set that as an in-object property too.
  ASSERT(Heap::arguments_length_index == 1);
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize + kPointerSize), ecx);

  // If there are no actual arguments, we're done.
  Label done;
  __ test(ecx, Operand(ecx));
  __ j(zero, &done);

  // Get the parameters pointer from the stack and untag the length.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ SmiUntag(ecx);

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(edi, Operand(eax, Heap::kArgumentsObjectSize));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(Factory::fixed_array_map()));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ mov(ebx, Operand(edx, -1 * kPointerSize));  // Skip receiver.
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize), ebx);
  __ add(Operand(edi), Immediate(kPointerSize));
  __ sub(Operand(edx), Immediate(kPointerSize));
  __ dec(ecx);
  __ j(not_zero, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kNewArgumentsFast), 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifndef V8_NATIVE_REGEXP
  __ TailCallRuntime(ExternalReference(Runtime::kRegExpExec), 4, 1);
#else  // V8_NATIVE_REGEXP
  if (!FLAG_regexp_entry_native) {
    __ TailCallRuntime(ExternalReference(Runtime::kRegExpExec), 4, 1);
    return;
  }

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: last_match_info (expected JSArray)
  //  esp[8]: previous index
  //  esp[12]: subject string
  //  esp[16]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime, invoke_regexp;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ mov(ebx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ test(ebx, Operand(ebx));
  __ j(zero, &runtime, not_taken);

  // Check that the first argument is a JSRegExp object.
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  ASSERT_EQ(0, kSmiTag);
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ecx);
  __ j(not_equal, &runtime);
  // Check that the RegExp has been compiled (data contains a fixed array).
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Check(not_zero, "Unexpected type for RegExp data, FixedArray expected");
    __ CmpObjectType(ecx, FIXED_ARRAY_TYPE, ebx);
    __ Check(equal, "Unexpected type for RegExp data, FixedArray expected");
  }

  // ecx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ mov(ebx, FieldOperand(ecx, JSRegExp::kDataTagOffset));
  __ cmp(Operand(ebx), Immediate(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ j(not_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2. This
  // uses the asumption that smis are 2 * their untagged value.
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ add(Operand(edx), Immediate(2));  // edx was a smi.
  // Check that the static offsets vector buffer is large enough.
  __ cmp(edx, OffsetsVector::kStaticOffsetsVectorSize);
  __ j(above, &runtime);

  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the second argument is a string.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);
  // Get the length of the string to ebx.
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));

  // ebx: Length of subject string
  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the third argument is a positive smi.
  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (usigned comparison).
  __ mov(eax, Operand(esp, kPreviousIndexOffset));
  __ SmiUntag(eax);
  __ cmp(eax, Operand(ebx));
  __ j(above, &runtime);

  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));
  __ mov(eax, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(eax, Factory::fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ mov(eax, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ add(Operand(edx), Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmp(edx, Operand(eax));
  __ j(greater, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_string, seq_two_byte_string, check_code;
  const int kStringRepresentationEncodingMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ and_(ebx, kStringRepresentationEncodingMask);
  // First check for sequential string.
  ASSERT_EQ(0, kStringTag);
  ASSERT_EQ(0, kSeqStringTag);
  __ test(Operand(ebx),
          Immediate(kIsNotStringMask | kStringRepresentationMask));
  __ j(zero, &seq_string);

  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  __ mov(edx, ebx);
  __ and_(edx, kStringRepresentationMask);
  __ cmp(edx, kConsStringTag);
  __ j(not_equal, &runtime);
  __ mov(edx, FieldOperand(eax, ConsString::kSecondOffset));
  __ cmp(Operand(edx), Factory::empty_string());
  __ j(not_equal, &runtime);
  __ mov(eax, FieldOperand(eax, ConsString::kFirstOffset));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  ASSERT_EQ(0, kSeqStringTag);
  __ test(ebx, Immediate(kStringRepresentationMask));
  __ j(not_zero, &runtime);
  __ and_(ebx, kStringRepresentationEncodingMask);

  __ bind(&seq_string);
  // eax: subject string (sequential either ascii to two byte)
  // ebx: suject string type & kStringRepresentationEncodingMask
  // ecx: RegExp data (FixedArray)
  // Check that the irregexp code has been generated for an ascii string. If
  // it has, the field contains a code object otherwise it contains the hole.
  __ cmp(ebx, kStringTag | kSeqStringTag | kTwoByteStringTag);
  __ j(equal, &seq_two_byte_string);
  if (FLAG_debug_code) {
    __ cmp(ebx, kStringTag | kSeqStringTag | kAsciiStringTag);
    __ Check(equal, "Expected sequential ascii string");
  }
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataAsciiCodeOffset));
  __ Set(edi, Immediate(1));  // Type is ascii.
  __ jmp(&check_code);

  __ bind(&seq_two_byte_string);
  // eax: subject string
  // ecx: RegExp data (FixedArray)
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataUC16CodeOffset));
  __ Set(edi, Immediate(0));  // Type is two byte.

  __ bind(&check_code);
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CmpObjectType(edx, CODE_TYPE, ebx);
  __ j(not_equal, &runtime);

  // eax: subject string
  // edx: code
  // edi: encoding of subject string (1 if ascii, 0 if two_byte);
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ SmiUntag(ebx);  // Previous index from smi.

  // eax: subject string
  // ebx: previous index
  // edx: code
  // edi: encoding of subject string (1 if ascii 0 if two_byte);
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1);

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ push(Immediate(1));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ mov(ecx, Operand::StaticVariable(address_of_regexp_stack_memory_address));
  __ add(ecx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ push(ecx);

  // Argument 5: static offsets vector buffer.
  __ push(Immediate(ExternalReference::address_of_static_offsets_vector()));

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label push_two_byte, push_rest;
  __ test(edi, Operand(edi));
  __ mov(edi, FieldOperand(eax, String::kLengthOffset));
  __ j(zero, &push_two_byte);
  __ lea(ecx, FieldOperand(eax, edi, times_1, SeqAsciiString::kHeaderSize));
  __ push(ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_1, SeqAsciiString::kHeaderSize));
  __ push(ecx);  // Argument 3.
  __ jmp(&push_rest);

  __ bind(&push_two_byte);
  __ lea(ecx, FieldOperand(eax, edi, times_2, SeqTwoByteString::kHeaderSize));
  __ push(ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_2, SeqTwoByteString::kHeaderSize));
  __ push(ecx);  // Argument 3.

  __ bind(&push_rest);

  // Argument 2: Previous index.
  __ push(ebx);

  // Argument 1: Subject string.
  __ push(eax);

  // Locate the code entry and call it.
  __ add(Operand(edx), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(Operand(edx));
  // Remove arguments.
  __ add(Operand(esp), Immediate(7 * kPointerSize));

  // Check the result.
  Label success;
  __ cmp(eax, NativeRegExpMacroAssembler::SUCCESS);
  __ j(equal, &success, taken);
  Label failure;
  __ cmp(eax, NativeRegExpMacroAssembler::FAILURE);
  __ j(equal, &failure, taken);
  __ cmp(eax, NativeRegExpMacroAssembler::EXCEPTION);
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592) Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(eax,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ cmp(eax, Operand::StaticVariable(pending_exception));
  __ j(equal, &runtime);
  __ bind(&failure);
  // For failure and exception return null.
  __ mov(Operand(eax), Factory::null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ add(Operand(edx), Immediate(2));  // edx was a smi.

  // edx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));

  // ebx: last_match_info backing store (FixedArray)
  // edx: number of capture registers
  // Store the capture count.
  __ SmiTag(edx);  // Number of capture registers to smi.
  __ mov(FieldOperand(ebx, RegExpImpl::kLastCaptureCountOffset), edx);
  __ SmiUntag(edx);  // Number of capture registers back from smi.
  // Store last subject and last input.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(FieldOperand(ebx, RegExpImpl::kLastSubjectOffset), eax);
  __ mov(ecx, ebx);
  __ RecordWrite(ecx, RegExpImpl::kLastSubjectOffset, eax, edi);
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(FieldOperand(ebx, RegExpImpl::kLastInputOffset), eax);
  __ mov(ecx, ebx);
  __ RecordWrite(ecx, RegExpImpl::kLastInputOffset, eax, edi);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector();
  __ mov(ecx, Immediate(address_of_static_offsets_vector));

  // ebx: last_match_info backing store (FixedArray)
  // ecx: offsets vector
  // edx: number of capture registers
  Label next_capture, done;
  __ mov(eax, Operand(esp, kPreviousIndexOffset));
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ sub(Operand(edx), Immediate(1));
  __ j(negative, &done);
  // Read the value from the static offsets vector buffer.
  __ mov(edi, Operand(ecx, edx, times_int_size, 0));
  // Perform explicit shift
  ASSERT_EQ(0, kSmiTag);
  __ shl(edi, kSmiTagSize);
  // Add previous index (from its stack slot) if value is not negative.
  Label capture_negative;
  // Carry flag set by shift above.
  __ j(negative, &capture_negative, not_taken);
  __ add(edi, Operand(eax));  // Add previous index (adding smi to smi).
  __ bind(&capture_negative);
  // Store the smi value in the last match info.
  __ mov(FieldOperand(ebx,
                      edx,
                      times_pointer_size,
                      RegExpImpl::kFirstCaptureOffset),
                      edi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kRegExpExec), 4, 1);
#endif  // V8_NATIVE_REGEXP
}


void NumberToStringStub::GenerateLookupNumberStringCache(MacroAssembler* masm,
                                                         Register object,
                                                         Register result,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         bool object_is_smi,
                                                         Label* not_found) {
  // Currently only lookup for smis. Check for smi if object is not known to be
  // a smi.
  if (!object_is_smi) {
    ASSERT(kSmiTag == 0);
    __ test(object, Immediate(kSmiTagMask));
    __ j(not_zero, not_found);
  }

  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch1;
  Register scratch = scratch2;

  // Load the number string cache.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(scratch, Immediate(Heap::kNumberStringCacheRootIndex));
  __ mov(number_string_cache,
         Operand::StaticArray(scratch, times_pointer_size, roots_address));
  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ mov(mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  __ shr(mask, 1);  // Divide length by two (length is not a smi).
  __ sub(Operand(mask), Immediate(1));  // Make mask.
  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value.
  __ mov(scratch, object);
  __ SmiUntag(scratch);
  __ and_(scratch, Operand(mask));
  // Check if the entry is the smi we are looking for.
  __ cmp(object,
         FieldOperand(number_string_cache,
                      scratch,
                      times_twice_pointer_size,
                      FixedArray::kHeaderSize));
  __ j(not_equal, not_found);

  // Get the result from the cache.
  __ mov(result,
         FieldOperand(number_string_cache,
                      scratch,
                      times_twice_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize));
  __ IncrementCounter(&Counters::number_to_string_native, 1);
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ mov(ebx, Operand(esp, kPointerSize));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, ebx, eax, ecx, edx, false, &runtime);
  __ ret(1 * kPointerSize);

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(ExternalReference(Runtime::kNumberToString), 1, 1);
}


void CompareStub::Generate(MacroAssembler* masm) {
  Label call_builtin, done;

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  if (cc_ == equal) {  // Both strict and non-strict.
    Label slow;  // Fallthrough label.
    // Equality is almost reflexive (everything but NaN), so start by testing
    // for "identity and not NaN".
    {
      Label not_identical;
      __ cmp(eax, Operand(edx));
      __ j(not_equal, &not_identical);
      // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
      // so we do the second best thing - test it ourselves.

      if (never_nan_nan_) {
        __ Set(eax, Immediate(0));
        __ ret(0);
      } else {
        Label return_equal;
        Label heap_number;
        // If it's not a heap number, then return equal.
        __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
               Immediate(Factory::heap_number_map()));
        __ j(equal, &heap_number);
        __ bind(&return_equal);
        __ Set(eax, Immediate(0));
        __ ret(0);

        __ bind(&heap_number);
        // It is a heap number, so return non-equal if it's NaN and equal if
        // it's not NaN.
        // The representation of NaN values has all exponent bits (52..62) set,
        // and not all mantissa bits (0..51) clear.
        // We only accept QNaNs, which have bit 51 set.
        // Read top bits of double representation (second word of value).

        // Value is a QNaN if value & kQuietNaNMask == kQuietNaNMask, i.e.,
        // all bits in the mask are set. We only need to check the word
        // that contains the exponent and high bit of the mantissa.
        ASSERT_NE(0, (kQuietNaNHighBitsMask << 1) & 0x80000000u);
        __ mov(edx, FieldOperand(edx, HeapNumber::kExponentOffset));
        __ xor_(eax, Operand(eax));
        // Shift value and mask so kQuietNaNHighBitsMask applies to topmost
        // bits.
        __ add(edx, Operand(edx));
        __ cmp(edx, kQuietNaNHighBitsMask << 1);
        __ setcc(above_equal, eax);
        __ ret(0);
      }

      __ bind(&not_identical);
    }

    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    if (strict_) {
      // If either is a Smi (we know that not both are), then they can only
      // be equal if the other is a HeapNumber. If so, use the slow case.
      {
        Label not_smis;
        ASSERT_EQ(0, kSmiTag);
        ASSERT_EQ(0, Smi::FromInt(0));
        __ mov(ecx, Immediate(kSmiTagMask));
        __ and_(ecx, Operand(eax));
        __ test(ecx, Operand(edx));
        __ j(not_zero, &not_smis);
        // One operand is a smi.

        // Check whether the non-smi is a heap number.
        ASSERT_EQ(1, kSmiTagMask);
        // ecx still holds eax & kSmiTag, which is either zero or one.
        __ sub(Operand(ecx), Immediate(0x01));
        __ mov(ebx, edx);
        __ xor_(ebx, Operand(eax));
        __ and_(ebx, Operand(ecx));  // ebx holds either 0 or eax ^ edx.
        __ xor_(ebx, Operand(eax));
        // if eax was smi, ebx is now edx, else eax.

        // Check if the non-smi operand is a heap number.
        __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
               Immediate(Factory::heap_number_map()));
        // If heap number, handle it in the slow case.
        __ j(equal, &slow);
        // Return non-equal (ebx is not zero)
        __ mov(eax, ebx);
        __ ret(0);

        __ bind(&not_smis);
      }

      // If either operand is a JSObject or an oddball value, then they are not
      // equal since their pointers are different
      // There is no test for undetectability in strict equality.

      // Get the type of the first operand.
      __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
      __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));

      // If the first object is a JS object, we have done pointer comparison.
      ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      Label first_non_object;
      __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
      __ j(less, &first_non_object);

      // Return non-zero (eax is not zero)
      Label return_not_equal;
      ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ cmp(ecx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
      __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));

      __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
      __ j(greater_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ cmp(ecx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Push arguments below the return address.
  __ pop(ecx);
  __ push(eax);
  __ push(edx);
  __ push(ecx);

  // Inlined floating point compare.
  // Call builtin if operands are not floating point or smi.
  Label check_for_symbols;
  Label unordered;
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    CpuFeatures::Scope use_cmov(CMOV);

    FloatingPointHelper::LoadSSE2Operands(masm, &check_for_symbols);
    __ comisd(xmm0, xmm1);

    // Jump to builtin for NaN.
    __ j(parity_even, &unordered, not_taken);
    __ mov(eax, 0);  // equal
    __ mov(ecx, Immediate(Smi::FromInt(1)));
    __ cmov(above, eax, Operand(ecx));
    __ mov(ecx, Immediate(Smi::FromInt(-1)));
    __ cmov(below, eax, Operand(ecx));
    __ ret(2 * kPointerSize);
  } else {
    FloatingPointHelper::CheckFloatOperands(masm, &check_for_symbols, ebx);
    FloatingPointHelper::LoadFloatOperands(masm, ecx);
    __ FCmp();

    // Jump to builtin for NaN.
    __ j(parity_even, &unordered, not_taken);

    Label below_lbl, above_lbl;
    // Return a result of -1, 0, or 1, to indicate result of comparison.
    __ j(below, &below_lbl, not_taken);
    __ j(above, &above_lbl, not_taken);

    __ xor_(eax, Operand(eax));  // equal
    // Both arguments were pushed in case a runtime call was needed.
    __ ret(2 * kPointerSize);

    __ bind(&below_lbl);
    __ mov(eax, Immediate(Smi::FromInt(-1)));
    __ ret(2 * kPointerSize);

    __ bind(&above_lbl);
    __ mov(eax, Immediate(Smi::FromInt(1)));
    __ ret(2 * kPointerSize);  // eax, edx were pushed
  }
  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  ASSERT(cc_ != not_equal);
  if (cc_ == less || cc_ == less_equal) {
    __ mov(eax, Immediate(Smi::FromInt(1)));
  } else {
    __ mov(eax, Immediate(Smi::FromInt(-1)));
  }
  __ ret(2 * kPointerSize);  // eax, edx were pushed

  // Fast negative check for symbol-to-symbol equality.
  __ bind(&check_for_symbols);
  Label check_for_strings;
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &check_for_strings, eax, ecx);
    BranchIfNonSymbol(masm, &check_for_strings, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(2 * kPointerSize);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx, &call_builtin);

  // Inline comparison of ascii strings.
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     edx,
                                                     eax,
                                                     ecx,
                                                     ebx,
                                                     edi);
#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&call_builtin);
  // must swap argument order
  __ pop(ecx);
  __ pop(edx);
  __ pop(eax);
  __ push(edx);
  __ push(eax);

  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript builtin;
  if (cc_ == equal) {
    builtin = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    int ncr;  // NaN compare result
    if (cc_ == less || cc_ == less_equal) {
      ncr = GREATER;
    } else {
      ASSERT(cc_ == greater || cc_ == greater_equal);  // remaining cases
      ncr = LESS;
    }
    __ push(Immediate(Smi::FromInt(ncr)));
  }

  // Restore return address on the stack.
  __ push(ecx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);
}


void CompareStub::BranchIfNonSymbol(MacroAssembler* masm,
                                    Label* label,
                                    Register object,
                                    Register scratch) {
  __ test(object, Immediate(kSmiTagMask));
  __ j(zero, label);
  __ mov(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, kIsSymbolMask | kIsNotStringMask);
  __ cmp(scratch, kSymbolTag | kStringTag);
  __ j(not_equal, label);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  // Because builtins always remove the receiver from the stack, we
  // have to fake one to avoid underflowing the stack. The receiver
  // must be inserted below the return address on the stack so we
  // temporarily store that in a register.
  __ pop(eax);
  __ push(Immediate(Smi::FromInt(0)));
  __ push(eax);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(ExternalReference(Runtime::kStackGuard), 1, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // +1 ~ return address
    Label receiver_is_value, receiver_is_js_object;
    __ mov(eax, Operand(esp, (argc_ + 1) * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &receiver_is_value, not_taken);

    // Check if the receiver is a valid JS object.
    __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, edi);
    __ j(above_equal, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(eax);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ LeaveInternalFrame();
    __ mov(Operand(esp, (argc_ + 1) * kPointerSize), eax);

    __ bind(&receiver_is_js_object);
  }

  // Get the function to call from the stack.
  // +2 ~ receiver, return address
  __ mov(edi, Operand(esp, (argc_ + 2) * kPointerSize));

  // Check that the function really is a JavaScript function.
  __ test(edi, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &slow, not_taken);

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ mov(Operand(esp, (argc_ + 1) * kPointerSize), edi);
  __ Set(eax, Immediate(argc_));
  __ Set(ebx, Immediate(0));
  __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
  __ jmp(adaptor, RelocInfo::CODE_TARGET);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // eax holds the exception.

  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // Drop the sp to the top of the handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ mov(esp, Operand::StaticVariable(handler_address));

  // Restore next handler and frame pointer, discard handler state.
  ASSERT(StackHandlerConstants::kNextOffset == 0);
  __ pop(Operand::StaticVariable(handler_address));
  ASSERT(StackHandlerConstants::kFPOffset == 1 * kPointerSize);
  __ pop(ebp);
  __ pop(edx);  // Remove state.

  // Before returning we restore the context from the frame pointer if
  // not NULL.  The frame pointer is NULL in the exception handler of
  // a JS entry frame.
  __ xor_(esi, Operand(esi));  // Tentatively set context pointer to NULL.
  Label skip;
  __ cmp(ebp, 0);
  __ j(equal, &skip, not_taken);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ ret(0);
}


// If true, a Handle<T> passed by value is passed and returned by
// using the location_ field directly.  If false, it is passed and
// returned as a pointer to a handle.
#ifdef USING_MAC_ABI
static const bool kPassHandlesDirectly = true;
#else
static const bool kPassHandlesDirectly = false;
#endif


void ApiGetterEntryStub::Generate(MacroAssembler* masm) {
  Label get_result;
  Label prologue;
  Label promote_scheduled_exception;
  __ EnterApiExitFrame(ExitFrame::MODE_NORMAL, kStackSpace, kArgc);
  ASSERT_EQ(kArgc, 4);
  if (kPassHandlesDirectly) {
    // When handles as passed directly we don't have to allocate extra
    // space for and pass an out parameter.
    __ mov(Operand(esp, 0 * kPointerSize), ebx);  // name.
    __ mov(Operand(esp, 1 * kPointerSize), eax);  // arguments pointer.
  } else {
    // The function expects three arguments to be passed but we allocate
    // four to get space for the output cell.  The argument slots are filled
    // as follows:
    //
    //   3: output cell
    //   2: arguments pointer
    //   1: name
    //   0: pointer to the output cell
    //
    // Note that this is one more "argument" than the function expects
    // so the out cell will have to be popped explicitly after returning
    // from the function.
    __ mov(Operand(esp, 1 * kPointerSize), ebx);  // name.
    __ mov(Operand(esp, 2 * kPointerSize), eax);  // arguments pointer.
    __ mov(ebx, esp);
    __ add(Operand(ebx), Immediate(3 * kPointerSize));
    __ mov(Operand(esp, 0 * kPointerSize), ebx);  // output
    __ mov(Operand(esp, 3 * kPointerSize), Immediate(0));  // out cell.
  }
  // Call the api function!
  __ call(fun()->address(), RelocInfo::RUNTIME_ENTRY);
  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address();
  __ cmp(Operand::StaticVariable(scheduled_exception_address),
         Immediate(Factory::the_hole_value()));
  __ j(not_equal, &promote_scheduled_exception, not_taken);
  if (!kPassHandlesDirectly) {
    // The returned value is a pointer to the handle holding the result.
    // Dereference this to get to the location.
    __ mov(eax, Operand(eax, 0));
  }
  // Check if the result handle holds 0
  __ test(eax, Operand(eax));
  __ j(not_zero, &get_result, taken);
  // It was zero; the result is undefined.
  __ mov(eax, Factory::undefined_value());
  __ jmp(&prologue);
  // It was non-zero.  Dereference to get the result value.
  __ bind(&get_result);
  __ mov(eax, Operand(eax, 0));
  __ bind(&prologue);
  __ LeaveExitFrame(ExitFrame::MODE_NORMAL);
  __ ret(0);
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(ExternalReference(Runtime::kPromoteScheduledException),
                     0,
                     1);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope) {
  // eax: result parameter for PerformGC, if any
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result_size_ is 2.

  if (do_gc) {
    __ mov(Operand(esp, 0 * kPointerSize), eax);  // Result.
    __ call(FUNCTION_ADDR(Runtime::PerformGC), RelocInfo::RUNTIME_ENTRY);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate_scope) {
    __ inc(Operand::StaticVariable(scope_depth));
  }

  // Call C function.
  __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
  __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
  __ call(Operand(ebx));
  // Result is in eax or edx:eax - do not destroy these registers!

  if (always_allocate_scope) {
    __ dec(Operand::StaticVariable(scope_depth));
  }

  // Make sure we're not trying to return 'the hole' from the runtime
  // call as this may lead to crashes in the IC code later.
  if (FLAG_debug_code) {
    Label okay;
    __ cmp(eax, Factory::the_hole_value());
    __ j(not_equal, &okay);
    __ int3();
    __ bind(&okay);
  }

  // Check for failure result.
  Label failure_returned;
  ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ lea(ecx, Operand(eax, 1));
  // Lower 2 bits of ecx are 0 iff eax has failure tag.
  __ test(ecx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned, not_taken);

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(mode_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ test(eax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, taken);

  // Special handling of out of memory exceptions.
  __ cmp(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
  __ j(equal, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ mov(eax, Operand::StaticVariable(pending_exception_address));
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(Operand::StaticVariable(pending_exception_address), edx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(eax, Factory::termination_exception());
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // Drop sp to the top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ mov(esp, Operand::StaticVariable(handler_address));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  __ cmp(Operand(esp, kStateOffset), Immediate(StackHandler::ENTRY));
  __ j(equal, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  __ mov(esp, Operand(esp, kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  ASSERT(StackHandlerConstants::kNextOffset == 0);
  __ pop(Operand::StaticVariable(handler_address));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(Top::k_external_caught_exception_address);
    __ mov(eax, false);
    __ mov(Operand::StaticVariable(external_caught), eax);

    // Set pending exception and eax to out of memory exception.
    ExternalReference pending_exception(Top::k_pending_exception_address);
    __ mov(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
    __ mov(Operand::StaticVariable(pending_exception), eax);
  }

  // Clear the context pointer.
  __ xor_(esi, Operand(esi));

  // Restore fp from handler and discard handler state.
  ASSERT(StackHandlerConstants::kFPOffset == 1 * kPointerSize);
  __ pop(ebp);
  __ pop(edx);  // State.

  ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ ret(0);
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects instead
  // of a proper result. The builtin entry handles this by performing
  // a garbage collection and retrying the builtin (twice).

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(mode_);

  // eax: result parameter for PerformGC, if any (setup below)
  // ebx: pointer to builtin function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver (C callee-saved)
  // esi: argv pointer (C callee-saved)

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
  __ mov(eax, Immediate(reinterpret_cast<int32_t>(failure)));
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
#ifdef ENABLE_LOGGING_AND_PROFILING
  Label not_outermost_js, not_outermost_js_2;
#endif

  // Setup frame.
  __ push(ebp);
  __ mov(ebp, Operand(esp));

  // Push marker in two places.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ push(Immediate(Smi::FromInt(marker)));  // context slot
  __ push(Immediate(Smi::FromInt(marker)));  // function slot
  // Save callee-saved registers (C calling conventions).
  __ push(edi);
  __ push(esi);
  __ push(ebx);

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Top::k_c_entry_fp_address);
  __ push(Operand::StaticVariable(c_entry_fp));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Top::k_js_entry_sp_address);
  __ cmp(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ j(not_equal, &not_outermost_js);
  __ mov(Operand::StaticVariable(js_entry_sp), ebp);
  __ bind(&not_outermost_js);
#endif

  // Call a faked try-block that does the invoke.
  __ call(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(Operand::StaticVariable(pending_exception), eax);
  __ mov(eax, reinterpret_cast<int32_t>(Failure::Exception()));
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);

  // Clear any pending exceptions.
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline
  // builtin and pop the faked function when we return. Notice that we
  // cannot store a reference to the trampoline code directly in this
  // stub, because the builtin stubs may not have been generated yet.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ mov(edx, Immediate(construct_entry));
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ mov(edx, Immediate(entry));
  }
  __ mov(edx, Operand(edx, 0));  // deref address
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ call(Operand(edx));

  // Unlink this frame from the handler chain.
  __ pop(Operand::StaticVariable(ExternalReference(Top::k_handler_address)));
  // Pop next_sp.
  __ add(Operand(esp), Immediate(StackHandlerConstants::kSize - kPointerSize));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If current EBP value is the same as js_entry_sp value, it means that
  // the current function is the outermost.
  __ cmp(ebp, Operand::StaticVariable(js_entry_sp));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ bind(&not_outermost_js_2);
#endif

  // Restore the top frame descriptor from the stack.
  __ bind(&exit);
  __ pop(Operand::StaticVariable(ExternalReference(Top::k_c_entry_fp_address)));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(Operand(esp), Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(ebp);
  __ ret(0);
}


void InstanceofStub::Generate(MacroAssembler* masm) {
  // Get the object - go slow case if it's a smi.
  Label slow;
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // 2 ~ return address, function
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);

  // Check that the left hand is a JS object.
  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));  // eax - object map
  __ movzx_b(ecx, FieldOperand(eax, Map::kInstanceTypeOffset));  // ecx - type
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &slow, not_taken);
  __ cmp(ecx, LAST_JS_OBJECT_TYPE);
  __ j(greater, &slow, not_taken);

  // Get the prototype of the function.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // 1 ~ return address
  __ TryGetFunctionPrototype(edx, ebx, ecx, &slow);

  // Check that the function prototype is a JS object.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  __ mov(ecx, FieldOperand(ebx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &slow, not_taken);
  __ cmp(ecx, LAST_JS_OBJECT_TYPE);
  __ j(greater, &slow, not_taken);

  // Register mapping: eax is object map and ebx is function prototype.
  __ mov(ecx, FieldOperand(eax, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  Label loop, is_instance, is_not_instance;
  __ bind(&loop);
  __ cmp(ecx, Operand(ebx));
  __ j(equal, &is_instance);
  __ cmp(Operand(ecx), Immediate(Factory::null_value()));
  __ j(equal, &is_not_instance);
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ mov(ecx, FieldOperand(ecx, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  __ Set(eax, Immediate(0));
  __ ret(2 * kPointerSize);

  __ bind(&is_not_instance);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret(2 * kPointerSize);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
const char* CompareStub::GetName() {
  switch (cc_) {
    case less: return "CompareStub_LT";
    case greater: return "CompareStub_GT";
    case less_equal: return "CompareStub_LE";
    case greater_equal: return "CompareStub_GE";
    case not_equal: {
      if (strict_) {
        if (never_nan_nan_) {
          return "CompareStub_NE_STRICT_NO_NAN";
        } else {
          return "CompareStub_NE_STRICT";
        }
      } else {
        if (never_nan_nan_) {
          return "CompareStub_NE_NO_NAN";
        } else {
          return "CompareStub_NE";
        }
      }
    }
    case equal: {
      if (strict_) {
        if (never_nan_nan_) {
          return "CompareStub_EQ_STRICT_NO_NAN";
        } else {
          return "CompareStub_EQ_STRICT";
        }
      } else {
        if (never_nan_nan_) {
          return "CompareStub_EQ_NO_NAN";
        } else {
          return "CompareStub_EQ";
        }
      }
    }
    default: return "CompareStub";
  }
}


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 14));
  int nnn_value = (never_nan_nan_ ? 2 : 0);
  if (cc_ != equal) nnn_value = 0;  // Avoid duplicate stubs.
  return (static_cast<unsigned>(cc_) << 2) | nnn_value | (strict_ ? 1 : 0);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime;

  // Load the two arguments.
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (string_check_) {
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &string_add_runtime);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &string_add_runtime);

    // First argument is a a string, test second.
    __ test(edx, Immediate(kSmiTagMask));
    __ j(zero, &string_add_runtime);
    __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &string_add_runtime);
  }

  // Both arguments are strings.
  // eax: first string
  // edx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ mov(ecx, FieldOperand(edx, String::kLengthOffset));
  __ test(ecx, Operand(ecx));
  __ j(not_zero, &second_not_zero_length);
  // Second string is empty, result is first string which is already in eax.
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));
  __ test(ebx, Operand(ebx));
  __ j(not_zero, &both_not_zero_length);
  // First string is empty, result is second string which is in edx.
  __ mov(eax, edx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // eax: first string
  // ebx: length of first string
  // ecx: length of second string
  // edx: second string
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);
  __ add(ebx, Operand(ecx));
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ cmp(ebx, 2);
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(eax, edx, ebx, ecx,
                                         &string_add_runtime);

  // Get the two characters forming the sub string.
  __ movzx_b(ebx, FieldOperand(eax, SeqAsciiString::kHeaderSize));
  __ movzx_b(ecx, FieldOperand(edx, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_flat_ascii_string;
  GenerateTwoCharacterSymbolTableProbe(masm, ebx, ecx, eax, edx, edi,
                                       &make_two_character_string);
  __ ret(2 * kPointerSize);

  __ bind(&make_two_character_string);
  __ Set(ebx, Immediate(2));
  __ jmp(&make_flat_ascii_string);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ cmp(ebx, String::kMinNonFlatLength);
  __ j(below, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  ASSERT((String::kMaxLength & 0x80000000) == 0);
  __ cmp(ebx, String::kMaxLength);
  __ j(above, &string_add_runtime);

  // If result is not supposed to be flat allocate a cons string object. If both
  // strings are ascii the result is an ascii cons string.
  Label non_ascii, allocated;
  __ mov(edi, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(edi, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ and_(ecx, Operand(edi));
  ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ test(ecx, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii);
  // Allocate an acsii cons string.
  __ AllocateAsciiConsString(ecx, edi, no_reg, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ mov(FieldOperand(ecx, ConsString::kLengthOffset), ebx);
  __ mov(FieldOperand(ecx, ConsString::kHashFieldOffset),
         Immediate(String::kEmptyHashField));
  __ mov(FieldOperand(ecx, ConsString::kFirstOffset), eax);
  __ mov(FieldOperand(ecx, ConsString::kSecondOffset), edx);
  __ mov(eax, ecx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // Allocate a two byte cons string.
  __ AllocateConsString(ecx, edi, no_reg, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are not
  // external strings.
  // eax: first string
  // ebx: length of resulting flat string
  // edx: second string
  __ bind(&string_add_flat_result);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ and_(ecx, kStringRepresentationMask);
  __ cmp(ecx, kExternalStringTag);
  __ j(equal, &string_add_runtime);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ and_(ecx, kStringRepresentationMask);
  __ cmp(ecx, kExternalStringTag);
  __ j(equal, &string_add_runtime);
  // Now check if both strings are ascii strings.
  // eax: first string
  // ebx: length of resulting flat string
  // edx: second string
  Label non_ascii_string_add_flat_result;
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ test(ecx, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii_string_add_flat_result);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ test(ecx, Immediate(kAsciiStringTag));
  __ j(zero, &string_add_runtime);

  __ bind(&make_flat_ascii_string);
  // Both strings are ascii strings. As they are short they are both flat.
  // ebx: length of resulting flat string
  __ AllocateAsciiString(eax, ebx, ecx, edx, edi, &string_add_runtime);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(Operand(ecx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Load first argument and locate first character.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  // Load second argument and locate first character.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Handle creating a flat two byte result.
  // eax: first string - known to be two byte
  // ebx: length of resulting flat string
  // edx: second string
  __ bind(&non_ascii_string_add_flat_result);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ and_(ecx, kAsciiStringTag);
  __ j(not_zero, &string_add_runtime);
  // Both strings are two byte strings. As they are short they are both
  // flat.
  __ AllocateTwoByteString(eax, ebx, ecx, edx, edi, &string_add_runtime);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(Operand(ecx),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load first argument and locate first character.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ add(Operand(edx),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  // Load second argument and locate first character.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kStringAdd), 2, 1);
}


void StringStubBase::GenerateCopyCharacters(MacroAssembler* masm,
                                            Register dest,
                                            Register src,
                                            Register count,
                                            Register scratch,
                                            bool ascii) {
  Label loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ mov_b(scratch, Operand(src, 0));
    __ mov_b(Operand(dest, 0), scratch);
    __ add(Operand(src), Immediate(1));
    __ add(Operand(dest), Immediate(1));
  } else {
    __ mov_w(scratch, Operand(src, 0));
    __ mov_w(Operand(dest, 0), scratch);
    __ add(Operand(src), Immediate(2));
    __ add(Operand(dest), Immediate(2));
  }
  __ sub(Operand(count), Immediate(1));
  __ j(not_zero, &loop);
}


void StringStubBase::GenerateCopyCharactersREP(MacroAssembler* masm,
                                               Register dest,
                                               Register src,
                                               Register count,
                                               Register scratch,
                                               bool ascii) {
  // Copy characters using rep movs of doublewords. Align destination on 4 byte
  // boundary before starting rep movs. Copy remaining characters after running
  // rep movs.
  ASSERT(dest.is(edi));  // rep movs destination
  ASSERT(src.is(esi));  // rep movs source
  ASSERT(count.is(ecx));  // rep movs count
  ASSERT(!scratch.is(dest));
  ASSERT(!scratch.is(src));
  ASSERT(!scratch.is(count));

  // Nothing to do for zero characters.
  Label done;
  __ test(count, Operand(count));
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (!ascii) {
    __ shl(count, 1);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ test(count, Immediate(~3));
  __ j(zero, &last_bytes);

  // Copy from edi to esi using rep movs instruction.
  __ mov(scratch, count);
  __ sar(count, 2);  // Number of doublewords to copy.
  __ rep_movs();

  // Find number of bytes left.
  __ mov(count, scratch);
  __ and_(count, 3);

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ test(count, Operand(count));
  __ j(zero, &done);

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(src, 0));
  __ mov_b(Operand(dest, 0), scratch);
  __ add(Operand(src), Immediate(1));
  __ add(Operand(dest), Immediate(1));
  __ sub(Operand(count), Immediate(1));
  __ j(not_zero, &loop);

  __ bind(&done);
}


void StringStubBase::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                          Register c1,
                                                          Register c2,
                                                          Register scratch1,
                                                          Register scratch2,
                                                          Register scratch3,
                                                          Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the symbol table.
  Label not_array_index;
  __ mov(scratch, c1);
  __ sub(Operand(scratch), Immediate(static_cast<int>('0')));
  __ cmp(Operand(scratch), Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index);
  __ mov(scratch, c2);
  __ sub(Operand(scratch), Immediate(static_cast<int>('0')));
  __ cmp(Operand(scratch), Immediate(static_cast<int>('9' - '0')));
  __ j(below_equal, not_found);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  GenerateHashInit(masm, hash, c1, scratch);
  GenerateHashAddCharacter(masm, hash, c2, scratch);
  GenerateHashGetHash(masm, hash, scratch);

  // Collect the two characters in a register.
  Register chars = c1;
  __ shl(c2, kBitsPerByte);
  __ or_(chars, Operand(c2));

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load the symbol table.
  Register symbol_table = c2;
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(scratch, Immediate(Heap::kSymbolTableRootIndex));
  __ mov(symbol_table,
         Operand::StaticArray(scratch, times_pointer_size, roots_address));

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  static const int kCapacityOffset =
      FixedArray::kHeaderSize +
      SymbolTable::kCapacityIndex * kPointerSize;
  __ mov(mask, FieldOperand(symbol_table, kCapacityOffset));
  __ SmiUntag(mask);
  __ sub(Operand(mask), Immediate(1));

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string
  // symbol_table: symbol table
  // mask:         capacity mask
  // scratch:      -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes], next_probe_pop_mask[kProbes];
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in symbol table.
    __ mov(scratch, hash);
    if (i > 0) {
      __ add(Operand(scratch), Immediate(SymbolTable::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(mask));

    // Load the entry from the symble table.
    Register candidate = scratch;  // Scratch register contains candidate.
    ASSERT_EQ(1, SymbolTableShape::kEntrySize);
    static const int kFirstElementOffset =
        FixedArray::kHeaderSize +
        SymbolTable::kPrefixStartIndex * kPointerSize +
        SymbolTableShape::kPrefixSize * kPointerSize;
    __ mov(candidate,
           FieldOperand(symbol_table,
                        scratch,
                        times_pointer_size,
                        kFirstElementOffset));

    // If entry is undefined no string with this hash can be found.
    __ cmp(candidate, Factory::undefined_value());
    __ j(equal, not_found);

    // If length is not 2 the string is not a candidate.
    __ cmp(FieldOperand(candidate, String::kLengthOffset), Immediate(2));
    __ j(not_equal, &next_probe[i]);

    // As we are out of registers save the mask on the stack and use that
    // register as a temporary.
    __ push(mask);
    Register temp = mask;

    // Check that the candidate is a non-external ascii string.
    __ mov(temp, FieldOperand(candidate, HeapObject::kMapOffset));
    __ movzx_b(temp, FieldOperand(temp, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe_pop_mask[i]);

    // Check if the two characters match.
    __ mov(temp, FieldOperand(candidate, SeqAsciiString::kHeaderSize));
    __ and_(temp, 0x0000ffff);
    __ cmp(chars, Operand(temp));
    __ j(equal, &found_in_symbol_table);
    __ bind(&next_probe_pop_mask[i]);
    __ pop(mask);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = scratch;
  __ bind(&found_in_symbol_table);
  __ pop(mask);  // Pop temporally saved mask from the stack.
  if (!result.is(eax)) {
    __ mov(eax, result);
  }
}


void StringStubBase::GenerateHashInit(MacroAssembler* masm,
                                      Register hash,
                                      Register character,
                                      Register scratch) {
  // hash = character + (character << 10);
  __ mov(hash, character);
  __ shl(hash, 10);
  __ add(hash, Operand(character));
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ sar(scratch, 6);
  __ xor_(hash, Operand(scratch));
}


void StringStubBase::GenerateHashAddCharacter(MacroAssembler* masm,
                                              Register hash,
                                              Register character,
                                              Register scratch) {
  // hash += character;
  __ add(hash, Operand(character));
  // hash += hash << 10;
  __ mov(scratch, hash);
  __ shl(scratch, 10);
  __ add(hash, Operand(scratch));
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ sar(scratch, 6);
  __ xor_(hash, Operand(scratch));
}


void StringStubBase::GenerateHashGetHash(MacroAssembler* masm,
                                         Register hash,
                                         Register scratch) {
  // hash += hash << 3;
  __ mov(scratch, hash);
  __ shl(scratch, 3);
  __ add(hash, Operand(scratch));
  // hash ^= hash >> 11;
  __ mov(scratch, hash);
  __ sar(scratch, 11);
  __ xor_(hash, Operand(scratch));
  // hash += hash << 15;
  __ mov(scratch, hash);
  __ shl(scratch, 15);
  __ add(hash, Operand(scratch));

  // if (hash == 0) hash = 27;
  Label hash_not_zero;
  __ test(hash, Operand(hash));
  __ j(not_zero, &hash_not_zero);
  __ mov(hash, Immediate(27));
  __ bind(&hash_not_zero);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: to
  //  esp[8]: from
  //  esp[12]: string

  // Make sure first argument is a string.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  ASSERT_EQ(0, kSmiTag);
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);

  // eax: string
  // ebx: instance type
  // Calculate length of sub string using the smi values.
  Label result_longer_than_two;
  __ mov(ecx, Operand(esp, 1 * kPointerSize));  // To index.
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, &runtime);
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // From index.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &runtime);
  __ sub(ecx, Operand(edx));
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ SmiUntag(ecx);  // Result length is no longer smi.
  __ cmp(ecx, 2);
  __ j(greater, &result_longer_than_two);
  __ j(less, &runtime);

  // Sub string of length 2 requested.
  // eax: string
  // ebx: instance type
  // ecx: sub string length (value is 2)
  // edx: from index (smi)
  __ JumpIfInstanceTypeIsNotSequentialAscii(ebx, ebx, &runtime);

  // Get the two characters forming the sub string.
  __ SmiUntag(edx);  // From index is no longer smi.
  __ movzx_b(ebx, FieldOperand(eax, edx, times_1, SeqAsciiString::kHeaderSize));
  __ movzx_b(ecx,
             FieldOperand(eax, edx, times_1, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  GenerateTwoCharacterSymbolTableProbe(masm, ebx, ecx, eax, edx, edi,
                                     &make_two_character_string);
  __ ret(2 * kPointerSize);

  __ bind(&make_two_character_string);
  // Setup registers for allocating the two character string.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ Set(ecx, Immediate(2));

  __ bind(&result_longer_than_two);
  // eax: string
  // ebx: instance type
  // ecx: result string length
  // Check for flat ascii string
  Label non_ascii_flat;
  __ JumpIfInstanceTypeIsNotSequentialAscii(ebx, ebx, &non_ascii_flat);

  // Allocate the result.
  __ AllocateAsciiString(eax, ecx, ebx, edx, edi, &runtime);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(Operand(edi), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ mov(esi, Operand(esp, 3 * kPointerSize));
  __ add(Operand(esi), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ mov(ebx, Operand(esp, 2 * kPointerSize));  // from
  __ SmiUntag(ebx);
  __ add(esi, Operand(ebx));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, true);
  __ mov(esi, edx);  // Restore esi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(3 * kPointerSize);

  __ bind(&non_ascii_flat);
  // eax: string
  // ebx: instance type & kStringRepresentationMask | kStringEncodingMask
  // ecx: result string length
  // Check for flat two byte string
  __ cmp(ebx, kSeqStringTag | kTwoByteStringTag);
  __ j(not_equal, &runtime);

  // Allocate the result.
  __ AllocateTwoByteString(eax, ecx, ebx, edx, edi, &runtime);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(Operand(edi),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ mov(esi, Operand(esp, 3 * kPointerSize));
  __ add(Operand(esi),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ mov(ebx, Operand(esp, 2 * kPointerSize));  // from
  // As from is a smi it is 2 times the value which matches the size of a two
  // byte character.
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ add(esi, Operand(ebx));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, false);
  __ mov(esi, edx);  // Restore esi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(3 * kPointerSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kSubString), 3, 1);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3) {
  Label result_not_equal;
  Label result_greater;
  Label compare_lengths;
  // Find minimum length.
  Label left_shorter;
  __ mov(scratch1, FieldOperand(left, String::kLengthOffset));
  __ mov(scratch3, scratch1);
  __ sub(scratch3, FieldOperand(right, String::kLengthOffset));

  Register length_delta = scratch3;

  __ j(less_equal, &left_shorter);
  // Right string is shorter. Change scratch1 to be length of right string.
  __ sub(scratch1, Operand(length_delta));
  __ bind(&left_shorter);

  Register min_length = scratch1;

  // If either length is zero, just compare lengths.
  __ test(min_length, Operand(min_length));
  __ j(zero, &compare_lengths);

  // Change index to run from -min_length to -1 by adding min_length
  // to string start. This means that loop ends when index reaches zero,
  // which doesn't need an additional compare.
  __ lea(left,
         FieldOperand(left,
                      min_length, times_1,
                      SeqAsciiString::kHeaderSize));
  __ lea(right,
         FieldOperand(right,
                      min_length, times_1,
                      SeqAsciiString::kHeaderSize));
  __ neg(min_length);

  Register index = min_length;  // index = -min_length;

  {
    // Compare loop.
    Label loop;
    __ bind(&loop);
    // Compare characters.
    __ mov_b(scratch2, Operand(left, index, times_1, 0));
    __ cmpb(scratch2, Operand(right, index, times_1, 0));
    __ j(not_equal, &result_not_equal);
    __ add(Operand(index), Immediate(1));
    __ j(not_zero, &loop);
  }

  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  __ test(length_delta, Operand(length_delta));
  __ j(not_zero, &result_not_equal);

  // Result is EQUAL.
  ASSERT_EQ(0, EQUAL);
  ASSERT_EQ(0, kSmiTag);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(2 * kPointerSize);

  __ bind(&result_not_equal);
  __ j(greater, &result_greater);

  // Result is LESS.
  __ Set(eax, Immediate(Smi::FromInt(LESS)));
  __ ret(2 * kPointerSize);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Set(eax, Immediate(Smi::FromInt(GREATER)));
  __ ret(2 * kPointerSize);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: right string
  //  esp[8]: left string

  __ mov(edx, Operand(esp, 2 * kPointerSize));  // left
  __ mov(eax, Operand(esp, 1 * kPointerSize));  // right

  Label not_same;
  __ cmp(edx, Operand(eax));
  __ j(not_equal, &not_same);
  ASSERT_EQ(0, EQUAL);
  ASSERT_EQ(0, kSmiTag);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ IncrementCounter(&Counters::string_compare_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both objects are sequential ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx, &runtime);

  // Compare flat ascii strings.
  __ IncrementCounter(&Counters::string_compare_native, 1);
  GenerateCompareFlatAsciiStrings(masm, edx, eax, ecx, ebx, edi);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kStringCompare), 2, 1);
}

#undef __

} }  // namespace v8::internal
