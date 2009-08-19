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
#include "ic-inl.h"
#include "parser.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"

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
      typeof_state_(NOT_INSIDE_TYPEOF),
      destination_(NULL),
      previous_(NULL) {
  owner_->set_state(this);
}


CodeGenState::CodeGenState(CodeGenerator* owner,
                           TypeofState typeof_state,
                           ControlDestination* destination)
    : owner_(owner),
      typeof_state_(typeof_state),
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

CodeGenerator::CodeGenerator(int buffer_size,
                             Handle<Script> script,
                             bool is_eval)
    : is_eval_(is_eval),
      script_(script),
      deferred_(8),
      masm_(new MacroAssembler(NULL, buffer_size)),
      scope_(NULL),
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

void CodeGenerator::GenCode(FunctionLiteral* fun) {
  // Record the position for debugging purposes.
  CodeForFunctionPosition(fun);

  ZoneList<Statement*>* body = fun->body();

  // Initialize state.
  ASSERT(scope_ == NULL);
  scope_ = fun->scope();
  ASSERT(allocator_ == NULL);
  RegisterAllocator register_allocator(this);
  allocator_ = &register_allocator;
  ASSERT(frame_ == NULL);
  frame_ = new VirtualFrame();
  set_in_spilled_code(false);

  // Adjust for function-level loop nesting.
  loop_nesting_ += fun->loop_nesting();

  JumpTarget::set_compiling_deferred_code(false);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      fun->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
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
    frame_->Enter();

    // Allocate space for locals and initialize them.
    frame_->AllocateStackSlots();
    // Initialize the function return target after the locals are set
    // up, because it needs the expected frame height from the frame.
    function_return_.set_direction(JumpTarget::BIDIRECTIONAL);
    function_return_is_shadowed_ = false;

    // Allocate the local context if needed.
    if (scope_->num_heap_slots() > 0) {
      Comment cmnt(masm_, "[ allocate local context");
      // Allocate local context.
      // Get outer context and create a new context based on it.
      frame_->PushFunction();
      Result context = frame_->CallRuntime(Runtime::kNewContext, 1);

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
      for (int i = 0; i < scope_->num_parameters(); i++) {
        Variable* par = scope_->parameter(i);
        Slot* slot = par->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          // The use of SlotOperand below is safe in unspilled code
          // because the slot is guaranteed to be a context slot.
          //
          // There are no parameters in the global scope.
          ASSERT(!scope_->is_global_scope());
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
    CheckStack();

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
      VisitStatements(body);

      // Handle the return from the function.
      if (has_valid_frame()) {
        // If there is a valid frame, control flow can fall off the end of
        // the body.  In that case there is an implicit return statement.
        ASSERT(!function_return_is_shadowed_);
        CodeForReturnPosition(fun);
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
  loop_nesting_ -= fun->loop_nesting();

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
  scope_ = NULL;
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
                                  TypeofState typeof_state,
                                  ControlDestination* dest,
                                  bool force_control) {
  ASSERT(!in_spilled_code());
  int original_height = frame_->height();

  { CodeGenState new_state(this, typeof_state, dest);
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


void CodeGenerator::LoadAndSpill(Expression* expression,
                                 TypeofState typeof_state) {
  ASSERT(in_spilled_code());
  set_in_spilled_code(false);
  Load(expression, typeof_state);
  frame_->SpillAll();
  set_in_spilled_code(true);
}


void CodeGenerator::Load(Expression* x, TypeofState typeof_state) {
#ifdef DEBUG
  int original_height = frame_->height();
#endif
  ASSERT(!in_spilled_code());
  JumpTarget true_target;
  JumpTarget false_target;
  ControlDestination dest(&true_target, &false_target, true);
  LoadCondition(x, typeof_state, &dest, false);

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


// TODO(1241834): Get rid of this function in favor of just using Load, now
// that we have the INSIDE_TYPEOF typeof state. => Need to handle global
// variables w/o reference errors elsewhere.
void CodeGenerator::LoadTypeofExpression(Expression* x) {
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
    Load(&property);
  } else {
    Load(x, INSIDE_TYPEOF);
  }
}


ArgumentsAllocationMode CodeGenerator::ArgumentsMode() const {
  if (scope_->arguments() == NULL) return NO_ARGUMENTS_ALLOCATION;
  ASSERT(scope_->arguments_shadow() != NULL);
  // We don't want to do lazy arguments allocation for functions that
  // have heap-allocated contexts, because it interfers with the
  // uninitialized const tracking in the context objects.
  return (scope_->num_heap_slots() > 0)
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
    frame_->Push(Smi::FromInt(scope_->num_parameters()));
    Result result = frame_->CallStub(&stub, 3);
    frame_->Push(&result);
  }

  { Reference shadow_ref(this, scope_->arguments_shadow());
    Reference arguments_ref(this, scope_->arguments());
    ASSERT(shadow_ref.is_slot() && arguments_ref.is_slot());
    // Here we rely on the convenient property that references to slot
    // take up zero space in the frame (ie, it doesn't matter that the
    // stored value is actually below the reference on the frame).
    JumpTarget done;
    bool skip_arguments = false;
    if (mode == LAZY_ARGUMENTS_ALLOCATION && !initial) {
      // We have to skip storing into the arguments slot if it has
      // already been written to. This can happen if the a function
      // has a local variable named 'arguments'.
      LoadFromSlot(scope_->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);
      Result arguments = frame_->Pop();
      if (arguments.is_constant()) {
        // We have to skip updating the arguments object if it has
        // been assigned a proper value.
        skip_arguments = !arguments.handle()->IsTheHole();
      } else {
        __ cmp(Operand(arguments.reg()), Immediate(Factory::the_hole_value()));
        arguments.Unuse();
        done.Branch(not_equal);
      }
    }
    if (!skip_arguments) {
      arguments_ref.SetValue(NOT_CONST_INIT);
      if (mode == LAZY_ARGUMENTS_ALLOCATION) done.Bind();
    }
    shadow_ref.SetValue(NOT_CONST_INIT);
  }
  return frame_->Pop();
}


Reference::Reference(CodeGenerator* cgen, Expression* expression)
    : cgen_(cgen), expression_(expression), type_(ILLEGAL) {
  cgen->LoadReference(this);
}


Reference::~Reference() {
  cgen_->UnloadReference(this);
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

  in_spilled_code_ = was_in_spilled_code;
}


void CodeGenerator::UnloadReference(Reference* ref) {
  // Pop a reference from the stack while preserving TOS.
  Comment cmnt(masm_, "[ UnloadReference");
  frame_->Nip(ref->size());
}


class ToBooleanStub: public CodeStub {
 public:
  ToBooleanStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return ToBoolean; }
  int MinorKey() { return 0; }
};


// ECMA-262, section 9.2, page 30: ToBoolean(). Pop the top of stack and
// convert it to a boolean in the condition code register or jump to
// 'false_target'/'true_target' as appropriate.
void CodeGenerator::ToBoolean(ControlDestination* dest) {
  Comment cmnt(masm_, "[ ToBoolean");

  // The value to convert should be popped from the frame.
  Result value = frame_->Pop();
  value.ToRegister();
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


class FloatingPointHelper : public AllStatic {
 public:
  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in register number. Returns operand as floating point number
  // on FPU stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register number);
  // Code pattern for loading floating point values. Input values must
  // be either smi or heap number objects (fp values). Requirements:
  // operand_1 on TOS+1 , operand_2 on TOS+2; Returns operands as
  // floating point numbers on FPU stack.
  static void LoadFloatOperands(MacroAssembler* masm, Register scratch);
  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);
  // Allocate a heap number in new space with undefined value.
  // Returns tagged pointer in eax, or jumps to need_gc if new space is full.
  static void AllocateHeapNumber(MacroAssembler* masm,
                                 Label* need_gc,
                                 Register scratch1,
                                 Register scratch2,
                                 Register result);
};


const char* GenericBinaryOpStub::GetName() {
  switch (op_) {
    case Token::ADD: return "GenericBinaryOpStub_ADD";
    case Token::SUB: return "GenericBinaryOpStub_SUB";
    case Token::MUL: return "GenericBinaryOpStub_MUL";
    case Token::DIV: return "GenericBinaryOpStub_DIV";
    case Token::BIT_OR: return "GenericBinaryOpStub_BIT_OR";
    case Token::BIT_AND: return "GenericBinaryOpStub_BIT_AND";
    case Token::BIT_XOR: return "GenericBinaryOpStub_BIT_XOR";
    case Token::SAR: return "GenericBinaryOpStub_SAR";
    case Token::SHL: return "GenericBinaryOpStub_SHL";
    case Token::SHR: return "GenericBinaryOpStub_SHR";
    default:         return "GenericBinaryOpStub";
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
  __ push(left_);
  __ push(right_);
  GenericBinaryOpStub stub(op_, mode_, SMI_CODE_INLINED);
  __ CallStub(&stub);
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


void CodeGenerator::GenericBinaryOperation(Token::Value op,
                                           SmiAnalysis* type,
                                           OverwriteMode overwrite_mode) {
  Comment cmnt(masm_, "[ BinaryOperation");
  Comment cmnt_token(masm_, Token::String(op));

  if (op == Token::COMMA) {
    // Simply discard left value.
    frame_->Nip(1);
    return;
  }

  // Set the flags based on the operation, type and loop nesting level.
  GenericBinaryFlags flags;
  switch (op) {
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      // Bit operations always assume they likely operate on Smis. Still only
      // generate the inline Smi check code if this operation is part of a loop.
      flags = (loop_nesting() > 0)
              ? SMI_CODE_INLINED
              : SMI_CODE_IN_STUB;
      break;

    default:
      // By default only inline the Smi check code for likely smis if this
      // operation is part of a loop.
      flags = ((loop_nesting() > 0) && type->IsLikelySmi())
              ? SMI_CODE_INLINED
              : SMI_CODE_IN_STUB;
      break;
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

  bool left_is_smi = left.is_constant() && left.handle()->IsSmi();
  bool left_is_non_smi = left.is_constant() && !left.handle()->IsSmi();
  bool right_is_smi = right.is_constant() && right.handle()->IsSmi();
  bool right_is_non_smi = right.is_constant() && !right.handle()->IsSmi();
  bool generate_no_smi_code = false;  // No smi code at all, inline or in stub.

  if (left_is_smi && right_is_smi) {
    // Compute the constant result at compile time, and leave it on the frame.
    int left_int = Smi::cast(*left.handle())->value();
    int right_int = Smi::cast(*right.handle())->value();
    if (FoldConstantSmis(op, left_int, right_int)) return;
  }

  if (left_is_non_smi || right_is_non_smi) {
    // Set flag so that we go straight to the slow case, with no smi code.
    generate_no_smi_code = true;
  } else if (right_is_smi) {
    ConstantSmiBinaryOperation(op, &left, right.handle(),
                               type, false, overwrite_mode);
    return;
  } else if (left_is_smi) {
    ConstantSmiBinaryOperation(op, &right, left.handle(),
                               type, true, overwrite_mode);
    return;
  }

  if (flags == SMI_CODE_INLINED && !generate_no_smi_code) {
    LikelySmiBinaryOperation(op, &left, &right, overwrite_mode);
  } else {
    frame_->Push(&left);
    frame_->Push(&right);
    // If we know the arguments aren't smis, use the binary operation stub
    // that does not check for the fast smi case.
    // The same stub is used for NO_SMI_CODE and SMI_CODE_INLINED.
    if (generate_no_smi_code) {
      flags = SMI_CODE_INLINED;
    }
    GenericBinaryOpStub stub(op, overwrite_mode, flags);
    Result answer = frame_->CallStub(&stub, 2);
    frame_->Push(&answer);
  }
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
void CodeGenerator::LikelySmiBinaryOperation(Token::Value op,
                                             Result* left,
                                             Result* right,
                                             OverwriteMode overwrite_mode) {
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
      ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
      __ lea(eax, Operand(eax, eax, times_1, kSmiTag));
      deferred->BindExit();
      left->Unuse();
      right->Unuse();
      frame_->Push(&quotient);
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
      frame_->Push(&remainder);
    }
    return;
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
    Result answer = allocator_->Allocate();
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
    __ sar(answer.reg(), kSmiTagSize);
    __ sar(ecx, kSmiTagSize);
    // Perform the operation.
    switch (op) {
      case Token::SAR:
        __ sar(answer.reg());
        // No checks of result necessary
        break;
      case Token::SHR: {
        Label result_ok;
        __ shr(answer.reg());
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
        ASSERT(kSmiTag == 0);
        __ shl(ecx, kSmiTagSize);
        deferred->Jump();
        __ bind(&result_ok);
        break;
      }
      case Token::SHL: {
        Label result_ok;
        __ shl(answer.reg());
        // Check that the *signed* result fits in a smi.
        __ cmp(answer.reg(), 0xc0000000);
        __ j(positive, &result_ok);
        ASSERT(kSmiTag == 0);
        __ shl(ecx, kSmiTagSize);
        deferred->Jump();
        __ bind(&result_ok);
        break;
      }
      default:
        UNREACHABLE();
    }
    // Smi-tag the result in answer.
    ASSERT(kSmiTagSize == 1);  // Adjust code if not the case.
    __ lea(answer.reg(),
           Operand(answer.reg(), answer.reg(), times_1, kSmiTag));
    deferred->BindExit();
    left->Unuse();
    right->Unuse();
    frame_->Push(&answer);
    return;
  }

  // Handle the other binary operations.
  left->ToRegister();
  right->ToRegister();
  // A newly allocated register answer is used to hold the answer.  The
  // registers containing left and right are not modified so they don't
  // need to be spilled in the fast case.
  Result answer = allocator_->Allocate();
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
      __ add(answer.reg(), Operand(right->reg()));  // Add optimistically.
      deferred->Branch(overflow);
      break;

    case Token::SUB:
      __ sub(answer.reg(), Operand(right->reg()));  // Subtract optimistically.
      deferred->Branch(overflow);
      break;

    case Token::MUL: {
      // If the smi tag is 0 we can just leave the tag on one operand.
      ASSERT(kSmiTag == 0);  // Adjust code below if not the case.
      // Remove smi tag from the left operand (but keep sign).
      // Left-hand operand has been copied into answer.
      __ sar(answer.reg(), kSmiTagSize);
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
  frame_->Push(&answer);
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
  __ push(src_);
  __ push(Immediate(value_));
  // For mod we don't generate all the Smi code inline.
  GenericBinaryOpStub stub(
      op_,
      overwrite_mode_,
      (op_ == Token::MOD) ? SMI_CODE_IN_STUB : SMI_CODE_INLINED);
  __ CallStub(&stub);
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
  __ push(Immediate(value_));
  __ push(src_);
  GenericBinaryOpStub igostub(op_, overwrite_mode_, SMI_CODE_INLINED);
  __ CallStub(&igostub);
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
  __ push(dst_);
  __ push(Immediate(value_));
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, SMI_CODE_INLINED);
  __ CallStub(&igostub);
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
  __ push(Immediate(value_));
  __ push(dst_);
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, SMI_CODE_INLINED);
  __ CallStub(&igostub);
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
  __ push(dst_);
  __ push(Immediate(value_));
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, SMI_CODE_INLINED);
  __ CallStub(&igostub);
  if (!dst_.is(eax)) __ mov(dst_, eax);
}


void CodeGenerator::ConstantSmiBinaryOperation(Token::Value op,
                                               Result* operand,
                                               Handle<Object> value,
                                               SmiAnalysis* type,
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
      LikelySmiBinaryOperation(op, &unsafe_operand, operand,
                               overwrite_mode);
    } else {
      LikelySmiBinaryOperation(op, operand, &unsafe_operand,
                               overwrite_mode);
    }
    ASSERT(!operand->is_valid());
    return;
  }

  // Get the literal value.
  Smi* smi_value = Smi::cast(*value);
  int int_value = smi_value->value();

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
      frame_->Push(operand);
      break;
    }

    case Token::SUB: {
      DeferredCode* deferred = NULL;
      Result answer;  // Only allocate a new register if reversed.
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
      frame_->Push(&answer);
      break;
    }

    case Token::SAR:
      if (reversed) {
        Result constant_operand(value);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
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
        frame_->Push(operand);
      }
      break;

    case Token::SHR:
      if (reversed) {
        Result constant_operand(value);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        operand->ToRegister();
        Result answer = allocator()->Allocate();
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
        __ sar(answer.reg(), kSmiTagSize);
        __ shr(answer.reg(), shift_value);
        // A negative Smi shifted right two is in the positive Smi range.
        if (shift_value < 2) {
          __ test(answer.reg(), Immediate(0xc0000000));
          deferred->Branch(not_zero);
        }
        operand->Unuse();
        ASSERT(kSmiTagSize == times_2);  // Adjust the code if not true.
        __ lea(answer.reg(),
               Operand(answer.reg(), answer.reg(), times_1, kSmiTag));
        deferred->BindExit();
        frame_->Push(&answer);
      }
      break;

    case Token::SHL:
      if (reversed) {
        Result constant_operand(value);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
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
          frame_->Push(operand);
        } else {
          // Use a fresh temporary for nonzero shift values.
          Result answer = allocator()->Allocate();
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
          frame_->Push(&answer);
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
      frame_->Push(operand);
      break;
    }

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
        frame_->Push(operand);
        break;
      }
      // Fall through if we did not find a power of 2 on the right hand side!

    default: {
      Result constant_operand(value);
      if (reversed) {
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
      } else {
        LikelySmiBinaryOperation(op, operand, &constant_operand,
                                 overwrite_mode);
      }
      break;
    }
  }
  ASSERT(!operand->is_valid());
}


void CodeGenerator::Comparison(Condition cc,
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
  bool left_side_constant_smi =
      left_side.is_constant() && left_side.handle()->IsSmi();
  bool right_side_constant_smi =
      right_side.is_constant() && right_side.handle()->IsSmi();
  bool left_side_constant_null =
      left_side.is_constant() && left_side.handle()->IsNull();
  bool right_side_constant_null =
      right_side.is_constant() && right_side.handle()->IsNull();

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
    } else {  // Only one side is a constant Smi.
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

      // Here we split control flow to the stub call and inlined cases
      // before finally splitting it to the control destination.  We use
      // a jump target and branching to duplicate the virtual frame at
      // the first split.  We manually handle the off-frame references
      // by reconstituting them on the non-fall-through path.
      JumpTarget is_smi;
      Register left_reg = left_side.reg();
      Handle<Object> right_val = right_side.handle();
      __ test(left_side.reg(), Immediate(kSmiTagMask));
      is_smi.Branch(zero, taken);

      // Setup and call the compare stub.
      CompareStub stub(cc, strict);
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
  } else {  // Neither side is a constant Smi or null.
    // If either side is a non-smi constant, skip the smi check.
    bool known_non_smi =
        (left_side.is_constant() && !left_side.handle()->IsSmi()) ||
        (right_side.is_constant() && !right_side.handle()->IsSmi());
    left_side.ToRegister();
    right_side.ToRegister();

    if (known_non_smi) {
      // When non-smi, call out to the compare stub.
      CompareStub stub(cc, strict);
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
      CompareStub stub(cc, strict);
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


class CallFunctionStub: public CodeStub {
 public:
  CallFunctionStub(int argc, InLoopFlag in_loop)
      : argc_(argc), in_loop_(in_loop) { }

  void Generate(MacroAssembler* masm);

 private:
  int argc_;
  InLoopFlag in_loop_;

#ifdef DEBUG
  void Print() { PrintF("CallFunctionStub (args %d)\n", argc_); }
#endif

  Major MajorKey() { return CallFunction; }
  int MinorKey() { return argc_; }
  InLoopFlag InLoop() { return in_loop_; }
};


// Call the function just below TOS on the stack with the given
// arguments. The receiver is the TOS.
void CodeGenerator::CallWithArguments(ZoneList<Expression*>* args,
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
  CallFunctionStub call_function(arg_count, in_loop);
  Result answer = frame_->CallStub(&call_function, arg_count + 1);
  // Restore context and replace function on the stack with the
  // result of the stub invocation.
  frame_->RestoreContextRegister();
  frame_->SetElementAt(0, &answer);
}


void CodeGenerator::CallApplyLazy(Property* apply,
                                  Expression* receiver,
                                  VariableProxy* arguments,
                                  int position) {
  ASSERT(ArgumentsMode() == LAZY_ARGUMENTS_ALLOCATION);
  ASSERT(arguments->IsArguments());

  JumpTarget slow, done;

  // Load the apply function onto the stack. This will usually
  // give us a megamorphic load site. Not super, but it works.
  Reference ref(this, apply);
  ref.GetValue(NOT_INSIDE_TYPEOF);
  ASSERT(ref.type() == Reference::NAMED);

  // Load the receiver and the existing arguments object onto the
  // expression stack. Avoid allocating the arguments object here.
  Load(receiver);
  LoadFromSlot(scope_->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);

  // Emit the source position information after having loaded the
  // receiver and the arguments.
  CodeForSourcePosition(position);

  // Check if the arguments object has been lazily allocated
  // already. If so, just use that instead of copying the arguments
  // from the stack. This also deals with cases where a local variable
  // named 'arguments' has been introduced.
  frame_->Dup();
  Result probe = frame_->Pop();
  bool try_lazy = true;
  if (probe.is_constant()) {
    try_lazy = probe.handle()->IsTheHole();
  } else {
    __ cmp(Operand(probe.reg()), Immediate(Factory::the_hole_value()));
    probe.Unuse();
    slow.Branch(not_equal);
  }

  if (try_lazy) {
    JumpTarget build_args;

    // Get rid of the arguments object probe.
    frame_->Drop();

    // Before messing with the execution stack, we sync all
    // elements. This is bound to happen anyway because we're
    // about to call a function.
    frame_->SyncRange(0, frame_->element_count() - 1);

    // Check that the receiver really is a JavaScript object.
    { frame_->PushElementAt(0);
      Result receiver = frame_->Pop();
      receiver.ToRegister();
      __ test(receiver.reg(), Immediate(kSmiTagMask));
      build_args.Branch(zero);
      Result tmp = allocator_->Allocate();
      // We allow all JSObjects including JSFunctions.  As long as
      // JS_FUNCTION_TYPE is the last instance type and it is right
      // after LAST_JS_OBJECT_TYPE, we do not have to check the upper
      // bound.
      ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
      __ CmpObjectType(receiver.reg(), FIRST_JS_OBJECT_TYPE, tmp.reg());
      build_args.Branch(less);
    }

    // Verify that we're invoking Function.prototype.apply.
    { frame_->PushElementAt(1);
      Result apply = frame_->Pop();
      apply.ToRegister();
      __ test(apply.reg(), Immediate(kSmiTagMask));
      build_args.Branch(zero);
      Result tmp = allocator_->Allocate();
      __ CmpObjectType(apply.reg(), JS_FUNCTION_TYPE, tmp.reg());
      build_args.Branch(not_equal);
      __ mov(tmp.reg(),
             FieldOperand(apply.reg(), JSFunction::kSharedFunctionInfoOffset));
      Handle<Code> apply_code(Builtins::builtin(Builtins::FunctionApply));
      __ cmp(FieldOperand(tmp.reg(), SharedFunctionInfo::kCodeOffset),
             Immediate(apply_code));
      build_args.Branch(not_equal);
    }

    // Get the function receiver from the stack. Check that it
    // really is a function.
    __ mov(edi, Operand(esp, 2 * kPointerSize));
    __ test(edi, Immediate(kSmiTagMask));
    build_args.Branch(zero);
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
    build_args.Branch(not_equal);

    // Copy the arguments to this function possibly from the
    // adaptor frame below it.
    Label invoke, adapted;
    __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
    __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
    __ cmp(ecx, ArgumentsAdaptorFrame::SENTINEL);
    __ j(equal, &adapted);

    // No arguments adaptor frame. Copy fixed number of arguments.
    __ mov(eax, Immediate(scope_->num_parameters()));
    for (int i = 0; i < scope_->num_parameters(); i++) {
      __ push(frame_->ParameterAt(i));
    }
    __ jmp(&invoke);

    // Arguments adaptor frame present. Copy arguments from there, but
    // avoid copying too many arguments to avoid stack overflows.
    __ bind(&adapted);
    static const uint32_t kArgumentsLimit = 1 * KB;
    __ mov(eax, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ shr(eax, kSmiTagSize);
    __ mov(ecx, Operand(eax));
    __ cmp(eax, kArgumentsLimit);
    build_args.Branch(above);

    // Loop through the arguments pushing them onto the execution
    // stack. We don't inform the virtual frame of the push, so we don't
    // have to worry about getting rid of the elements from the virtual
    // frame.
    Label loop;
    __ bind(&loop);
    __ test(ecx, Operand(ecx));
    __ j(zero, &invoke);
    __ push(Operand(edx, ecx, times_4, 1 * kPointerSize));
    __ dec(ecx);
    __ jmp(&loop);

    // Invoke the function. The virtual frame knows about the receiver
    // so make sure to forget that explicitly.
    __ bind(&invoke);
    ParameterCount actual(eax);
    __ InvokeFunction(edi, actual, CALL_FUNCTION);
    frame_->Forget(1);
    Result result = allocator()->Allocate(eax);
    frame_->SetElementAt(0, &result);
    done.Jump();

    // Slow-case: Allocate the arguments object since we know it isn't
    // there, and fall-through to the slow-case where we call
    // Function.prototype.apply.
    build_args.Bind();
    Result arguments_object = StoreArgumentsObject(false);
    frame_->Push(&arguments_object);
    slow.Bind();
  }

  // Flip the apply function and the function to call on the stack, so
  // the function looks like the receiver of the apply call. This way,
  // the generic Function.prototype.apply implementation can deal with
  // the call like it usually does.
  Result a2 = frame_->Pop();
  Result a1 = frame_->Pop();
  Result ap = frame_->Pop();
  Result fn = frame_->Pop();
  frame_->Push(&ap);
  frame_->Push(&fn);
  frame_->Push(&a1);
  frame_->Push(&a2);
  CallFunctionStub call_function(2, NOT_IN_LOOP);
  Result res = frame_->CallStub(&call_function, 3);
  frame_->Push(&res);

  // All done. Restore context register after call.
  if (try_lazy) done.Bind();
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
  if (FLAG_check_stack) {
    DeferredStackCheck* deferred = new DeferredStackCheck;
    ExternalReference stack_guard_limit =
        ExternalReference::address_of_stack_guard_limit();
    __ cmp(esp, Operand::StaticVariable(stack_guard_limit));
    deferred->Branch(below);
    deferred->BindExit();
  }
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

  frame_->EmitPush(Immediate(pairs));
  frame_->EmitPush(esi);  // The context is the second argument.
  frame_->EmitPush(Immediate(Smi::FromInt(is_eval() ? 1 : 0)));
  Result ignored = frame_->CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
  Comment cmnt(masm_, "[ Declaration");
  CodeForStatementPosition(node);
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
    LoadCondition(node->condition(), NOT_INSIDE_TYPEOF, &dest, true);

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
    LoadCondition(node->condition(), NOT_INSIDE_TYPEOF, &dest, true);

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
    LoadCondition(node->condition(), NOT_INSIDE_TYPEOF, &dest, true);

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
    LoadCondition(node->condition(), NOT_INSIDE_TYPEOF, &dest, false);
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
  masm_->ret((scope_->num_parameters() + 1) * kPointerSize);
  DeleteFrame();

  // Check that the size of the code used for returning matches what is
  // expected by the debugger.
  ASSERT_EQ(Debug::kIa32JSReturnSequenceLength,
            masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
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
    Comparison(equal, true, &dest);

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


void CodeGenerator::VisitLoopStatement(LoopStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ LoopStatement");
  CodeForStatementPosition(node);
  node->break_target()->set_direction(JumpTarget::FORWARD_ONLY);

  // Simple condition analysis.  ALWAYS_TRUE and ALWAYS_FALSE represent a
  // known result for the test expression, with no side effects.
  enum { ALWAYS_TRUE, ALWAYS_FALSE, DONT_KNOW } info = DONT_KNOW;
  if (node->cond() == NULL) {
    ASSERT(node->type() == LoopStatement::FOR_LOOP);
    info = ALWAYS_TRUE;
  } else {
    Literal* lit = node->cond()->AsLiteral();
    if (lit != NULL) {
      if (lit->IsTrue()) {
        info = ALWAYS_TRUE;
      } else if (lit->IsFalse()) {
        info = ALWAYS_FALSE;
      }
    }
  }

  switch (node->type()) {
    case LoopStatement::DO_LOOP: {
      JumpTarget body(JumpTarget::BIDIRECTIONAL);
      IncrementLoopNesting();

      // Label the top of the loop for the backward jump if necessary.
      if (info == ALWAYS_TRUE) {
        // Use the continue target.
        node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else if (info == ALWAYS_FALSE) {
        // No need to label it.
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
      } else {
        // Continue is the test, so use the backward body target.
        ASSERT(info == DONT_KNOW);
        node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
        body.Bind();
      }

      CheckStack();  // TODO(1222600): ignore if body contains calls.
      Visit(node->body());

      // Compile the test.
      if (info == ALWAYS_TRUE) {
        // If control flow can fall off the end of the body, jump back
        // to the top and bind the break target at the exit.
        if (has_valid_frame()) {
          node->continue_target()->Jump();
        }
        if (node->break_target()->is_linked()) {
          node->break_target()->Bind();
        }

      } else if (info == ALWAYS_FALSE) {
        // We may have had continues or breaks in the body.
        if (node->continue_target()->is_linked()) {
          node->continue_target()->Bind();
        }
        if (node->break_target()->is_linked()) {
          node->break_target()->Bind();
        }

      } else {
        ASSERT(info == DONT_KNOW);
        // We have to compile the test expression if it can be reached by
        // control flow falling out of the body or via continue.
        if (node->continue_target()->is_linked()) {
          node->continue_target()->Bind();
        }
        if (has_valid_frame()) {
          ControlDestination dest(&body, node->break_target(), false);
          LoadCondition(node->cond(), NOT_INSIDE_TYPEOF, &dest, true);
        }
        if (node->break_target()->is_linked()) {
          node->break_target()->Bind();
        }
      }
      break;
    }

    case LoopStatement::WHILE_LOOP: {
      // Do not duplicate conditions that may have function literal
      // subexpressions.  This can cause us to compile the function
      // literal twice.
      bool test_at_bottom = !node->may_have_function_literal();

      IncrementLoopNesting();

      // If the condition is always false and has no side effects, we
      // do not need to compile anything.
      if (info == ALWAYS_FALSE) break;

      JumpTarget body;
      if (test_at_bottom) {
        body.set_direction(JumpTarget::BIDIRECTIONAL);
      }

      // Based on the condition analysis, compile the test as necessary.
      if (info == ALWAYS_TRUE) {
        // We will not compile the test expression.  Label the top of
        // the loop with the continue target.
        node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else {
        ASSERT(info == DONT_KNOW);  // ALWAYS_FALSE cannot reach here.
        if (test_at_bottom) {
          // Continue is the test at the bottom, no need to label the
          // test at the top.  The body is a backward target.
          node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
        } else {
          // Label the test at the top as the continue target.  The
          // body is a forward-only target.
          node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
          node->continue_target()->Bind();
        }
        // Compile the test with the body as the true target and
        // preferred fall-through and with the break target as the
        // false target.
        ControlDestination dest(&body, node->break_target(), true);
        LoadCondition(node->cond(), NOT_INSIDE_TYPEOF, &dest, true);

        if (dest.false_was_fall_through()) {
          // If we got the break target as fall-through, the test may
          // have been unconditionally false (if there are no jumps to
          // the body).
          if (!body.is_linked()) break;

          // Otherwise, jump around the body on the fall through and
          // then bind the body target.
          node->break_target()->Unuse();
          node->break_target()->Jump();
          body.Bind();
        }
      }

      CheckStack();  // TODO(1222600): ignore if body contains calls.
      Visit(node->body());

      // Based on the condition analysis, compile the backward jump as
      // necessary.
      if (info == ALWAYS_TRUE) {
        // The loop body has been labeled with the continue target.
        if (has_valid_frame()) {
          node->continue_target()->Jump();
        }
      } else {
        ASSERT(info == DONT_KNOW);  // ALWAYS_FALSE cannot reach here.
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
            LoadCondition(node->cond(), NOT_INSIDE_TYPEOF, &dest, true);
          }
        } else {
          // If we have chosen not to recompile the test at the
          // bottom, jump back to the one at the top.
          if (has_valid_frame()) {
            node->continue_target()->Jump();
          }
        }
      }

      // The break target may be already bound (by the condition), or
      // there may not be a valid frame.  Bind it only if needed.
      if (node->break_target()->is_linked()) {
        node->break_target()->Bind();
      }
      break;
    }

    case LoopStatement::FOR_LOOP: {
      // Do not duplicate conditions that may have function literal
      // subexpressions.  This can cause us to compile the function
      // literal twice.
      bool test_at_bottom = !node->may_have_function_literal();

      // Compile the init expression if present.
      if (node->init() != NULL) {
        Visit(node->init());
      }

      IncrementLoopNesting();

      // If the condition is always false and has no side effects, we
      // do not need to compile anything else.
      if (info == ALWAYS_FALSE) break;

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
      if (info == ALWAYS_TRUE) {
        // We will not compile the test expression.  Label the top of
        // the loop.
        if (node->next() == NULL) {
          // Use the continue target if there is no update expression.
          node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
          node->continue_target()->Bind();
        } else {
          // Otherwise use the backward loop target.
          node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
          loop.Bind();
        }
      } else {
        ASSERT(info == DONT_KNOW);
        if (test_at_bottom) {
          // Continue is either the update expression or the test at
          // the bottom, no need to label the test at the top.
          node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
        } else if (node->next() == NULL) {
          // We are not recompiling the test at the bottom and there
          // is no update expression.
          node->continue_target()->set_direction(JumpTarget::BIDIRECTIONAL);
          node->continue_target()->Bind();
        } else {
          // We are not recompiling the test at the bottom and there
          // is an update expression.
          node->continue_target()->set_direction(JumpTarget::FORWARD_ONLY);
          loop.Bind();
        }

        // Compile the test with the body as the true target and
        // preferred fall-through and with the break target as the
        // false target.
        ControlDestination dest(&body, node->break_target(), true);
        LoadCondition(node->cond(), NOT_INSIDE_TYPEOF, &dest, true);

        if (dest.false_was_fall_through()) {
          // If we got the break target as fall-through, the test may
          // have been unconditionally false (if there are no jumps to
          // the body).
          if (!body.is_linked()) break;

          // Otherwise, jump around the body on the fall through and
          // then bind the body target.
          node->break_target()->Unuse();
          node->break_target()->Jump();
          body.Bind();
        }
      }

      CheckStack();  // TODO(1222600): ignore if body contains calls.
      Visit(node->body());

      // If there is an update expression, compile it if necessary.
      if (node->next() != NULL) {
        if (node->continue_target()->is_linked()) {
          node->continue_target()->Bind();
        }

        // Control can reach the update by falling out of the body or
        // by a continue.
        if (has_valid_frame()) {
          // Record the source position of the statement as this code
          // which is after the code for the body actually belongs to
          // the loop statement and not the body.
          CodeForStatementPosition(node);
          Visit(node->next());
        }
      }

      // Based on the condition analysis, compile the backward jump as
      // necessary.
      if (info == ALWAYS_TRUE) {
        if (has_valid_frame()) {
          if (node->next() == NULL) {
            node->continue_target()->Jump();
          } else {
            loop.Jump();
          }
        }
      } else {
        ASSERT(info == DONT_KNOW);  // ALWAYS_FALSE cannot reach here.
        if (test_at_bottom) {
          if (node->continue_target()->is_linked()) {
            // We can have dangling jumps to the continue target if
            // there was no update expression.
            node->continue_target()->Bind();
          }
          // Control can reach the test at the bottom by falling out
          // of the body, by a continue in the body, or from the
          // update expression.
          if (has_valid_frame()) {
            // The break target is the fall-through (body is a
            // backward jump from here).
            ControlDestination dest(&body, node->break_target(), false);
            LoadCondition(node->cond(), NOT_INSIDE_TYPEOF, &dest, true);
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
      }

      // The break target may be already bound (by the condition), or
      // there may not be a valid frame.  Bind it only if needed.
      if (node->break_target()->is_linked()) {
        node->break_target()->Bind();
      }
      break;
    }
  }

  DecrementLoopNesting();
  node->continue_target()->Unuse();
  node->break_target()->Unuse();
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
  frame_->EmitPush(eax);  // push the object being iterated over (slot 4)

  frame_->EmitPush(eax);  // push the Object (slot 4) for the runtime call
  frame_->CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a Map, we can do a fast modification check.
  // Otherwise, we got a FixedArray, and we have to do a slow check.
  // eax: map or fixed array (result from call to
  // Runtime::kGetPropertyNamesFast)
  __ mov(edx, Operand(eax));
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ecx, Factory::meta_map());
  fixed_array.Branch(not_equal);

  // Get enum cache
  // eax: map (result from call to Runtime::kGetPropertyNamesFast)
  __ mov(ecx, Operand(eax));
  __ mov(ecx, FieldOperand(ecx, Map::kInstanceDescriptorsOffset));
  // Get the bridge array held in the enumeration index field.
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumerationIndexOffset));
  // Get the cache from the bridge array.
  __ mov(edx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  frame_->EmitPush(eax);  // <- slot 3
  frame_->EmitPush(edx);  // <- slot 2
  __ mov(eax, FieldOperand(edx, FixedArray::kLengthOffset));
  __ shl(eax, kSmiTagSize);
  frame_->EmitPush(eax);  // <- slot 1
  frame_->EmitPush(Immediate(Smi::FromInt(0)));  // <- slot 0
  entry.Jump();

  fixed_array.Bind();
  // eax: fixed array (result from call to Runtime::kGetPropertyNamesFast)
  frame_->EmitPush(Immediate(Smi::FromInt(0)));  // <- slot 3
  frame_->EmitPush(eax);  // <- slot 2

  // Push the length of the array and the initial index onto the stack.
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ shl(eax, kSmiTagSize);
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
      }
      // If the reference was to a slot we rely on the convenient property
      // that it doesn't matter whether a value (eg, ebx pushed above) is
      // right on top of or right underneath a zero-sized reference.
      each.SetValue(NOT_CONST_INIT);
      if (each.size() > 0) {
        // It's safe to pop the value lying on top of the reference before
        // unloading the reference itself (which preserves the top of stack,
        // ie, now the topmost value of the non-zero sized reference), since
        // we will discard the top of stack after unloading the reference
        // anyway.
        frame_->Drop();
      }
    }
  }
  // Unloading a reference may leave the frame in an unspilled state.
  frame_->SpillAll();

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


void CodeGenerator::VisitTryCatch(TryCatch* node) {
  ASSERT(!in_spilled_code());
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ TryCatch");
  CodeForStatementPosition(node);

  JumpTarget try_block;
  JumpTarget exit;

  try_block.Call();
  // --- Catch block ---
  frame_->EmitPush(eax);

  // Store the caught exception in the catch variable.
  { Reference ref(this, node->catch_var());
    ASSERT(ref.is_slot());
    // Load the exception to the top of the stack.  Here we make use of the
    // convenient property that it doesn't matter whether a value is
    // immediately on top of or underneath a zero-sized reference.
    ref.SetValue(NOT_CONST_INIT);
  }

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


void CodeGenerator::VisitTryFinally(TryFinally* node) {
  ASSERT(!in_spilled_code());
  VirtualFrame::SpilledScope spilled_scope;
  Comment cmnt(masm_, "[ TryFinally");
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
  frame_->CallRuntime(Runtime::kDebugBreak, 0);
  // Ignore the return value.
#endif
}


void CodeGenerator::InstantiateBoilerplate(Handle<JSFunction> boilerplate) {
  // Call the runtime to instantiate the function boilerplate object.
  // The inevitable call will sync frame elements to memory anyway, so
  // we do it eagerly to allow us to push the arguments directly into
  // place.
  ASSERT(boilerplate->IsBoilerplate());
  frame_->SyncRange(0, frame_->element_count() - 1);

  // Push the boilerplate on the stack.
  frame_->EmitPush(Immediate(boilerplate));

  // Create a new closure.
  frame_->EmitPush(esi);
  Result result = frame_->CallRuntime(Runtime::kNewClosure, 2);
  frame_->Push(&result);
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate = BuildBoilerplate(node);
  // Check for stack-overflow exception.
  if (HasStackOverflow()) return;
  InstantiateBoilerplate(boilerplate);
}


void CodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  Comment cmnt(masm_, "[ FunctionBoilerplateLiteral");
  InstantiateBoilerplate(node->boilerplate());
}


void CodeGenerator::VisitConditional(Conditional* node) {
  Comment cmnt(masm_, "[ Conditional");
  JumpTarget then;
  JumpTarget else_;
  JumpTarget exit;
  ControlDestination dest(&then, &else_, true);
  LoadCondition(node->condition(), NOT_INSIDE_TYPEOF, &dest, true);

  if (dest.false_was_fall_through()) {
    // The else target was bound, so we compile the else part first.
    Load(node->else_expression(), typeof_state());

    if (then.is_linked()) {
      exit.Jump();
      then.Bind();
      Load(node->then_expression(), typeof_state());
    }
  } else {
    // The then target was bound, so we compile the then part first.
    Load(node->then_expression(), typeof_state());

    if (else_.is_linked()) {
      exit.Jump();
      else_.Bind();
      Load(node->else_expression(), typeof_state());
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

    // Generate fast-case code for variables that might be shadowed by
    // eval-introduced variables.  Eval is used a lot without
    // introducing variables.  In those cases, we do not want to
    // perform a runtime call for all variables in the scope
    // containing the eval.
    if (slot->var()->mode() == Variable::DYNAMIC_GLOBAL) {
      value = LoadFromGlobalSlotCheckExtensions(slot, typeof_state, &slow);
      // If there was no control flow to slow, we can exit early.
      if (!slow.is_linked()) {
        frame_->Push(&value);
        return;
      }

      done.Jump(&value);

    } else if (slot->var()->mode() == Variable::DYNAMIC_LOCAL) {
      Slot* potential_slot = slot->var()->local_if_not_shadowed()->slot();
      // Only generate the fast case for locals that rewrite to slots.
      // This rules out argument loads.
      if (potential_slot != NULL) {
        // Allocate a fresh register to use as a temp in
        // ContextSlotOperandCheckExtensions and to hold the result
        // value.
        value = allocator_->Allocate();
        ASSERT(value.is_valid());
        __ mov(value.reg(),
               ContextSlotOperandCheckExtensions(potential_slot,
                                                 value,
                                                 &slow));
        if (potential_slot->var()->mode() == Variable::CONST) {
          __ cmp(value.reg(), Factory::the_hole_value());
          done.Branch(not_equal, &value);
          __ mov(value.reg(), Factory::undefined_value());
        }
        // There is always control flow to slow from
        // ContextSlotOperandCheckExtensions so we have to jump around
        // it.
        done.Jump(&value);
      }
    }

    slow.Bind();
    // A runtime call is inevitable.  We eagerly sync frame elements
    // to memory so that we can push the arguments directly into place
    // on top of the frame.
    frame_->SyncRange(0, frame_->element_count() - 1);
    frame_->EmitPush(esi);
    frame_->EmitPush(Immediate(slot->var()->name()));
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
    __ mov(ecx, SlotOperand(slot, ecx));
    __ cmp(ecx, Factory::the_hole_value());
    exit.Branch(not_equal);
    __ mov(ecx, Factory::undefined_value());
    exit.Bind();
    frame_->EmitPush(ecx);

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
    __ mov(temp.reg(), SlotOperand(slot, temp.reg()));
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
  __ cmp(Operand(value.reg()), Immediate(Factory::the_hole_value()));
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
  // Discard the global object. The result is in answer.
  frame_->Drop();
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
  LoadFromSlotCheckForArguments(node, typeof_state());
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
    ref.GetValue(typeof_state());
  }
}


void CodeGenerator::VisitLiteral(Literal* node) {
  Comment cmnt(masm_, "[ Literal");
  frame_->Push(node->handle());
}


void CodeGenerator::LoadUnsafeSmi(Register target, Handle<Object> value) {
  ASSERT(target.is_valid());
  ASSERT(value->IsSmi());
  int bits = reinterpret_cast<int>(*value);
  __ Set(target, Immediate(bits & 0x0000FFFF));
  __ xor_(target, bits & 0xFFFF0000);
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


// Materialize the object literal 'node' in the literals array
// 'literals' of the function.  Leave the object boilerplate in
// 'boilerplate'.
class DeferredObjectLiteral: public DeferredCode {
 public:
  DeferredObjectLiteral(Register boilerplate,
                        Register literals,
                        ObjectLiteral* node)
      : boilerplate_(boilerplate), literals_(literals), node_(node) {
    set_comment("[ DeferredObjectLiteral");
  }

  void Generate();

 private:
  Register boilerplate_;
  Register literals_;
  ObjectLiteral* node_;
};


void DeferredObjectLiteral::Generate() {
  // Since the entry is undefined we call the runtime system to
  // compute the literal.
  // Literal array (0).
  __ push(literals_);
  // Literal index (1).
  __ push(Immediate(Smi::FromInt(node_->literal_index())));
  // Constant properties (2).
  __ push(Immediate(node_->constant_properties()));
  __ CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  if (!boilerplate_.is(eax)) __ mov(boilerplate_, eax);
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  Comment cmnt(masm_, "[ ObjectLiteral");

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

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code passing the literals array.
  DeferredObjectLiteral* deferred =
      new DeferredObjectLiteral(boilerplate.reg(), literals.reg(), node);
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->Branch(equal);
  deferred->BindExit();
  literals.Unuse();

  // Push the boilerplate object.
  frame_->Push(&boilerplate);
  // Clone the boilerplate object.
  Runtime::FunctionId clone_function_id = Runtime::kCloneLiteralBoilerplate;
  if (node->depth() == 1) {
    clone_function_id = Runtime::kCloneShallowLiteralBoilerplate;
  }
  Result clone = frame_->CallRuntime(clone_function_id, 1);
  // Push the newly cloned literal object as the result.
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
          frame_->Push(key);
          Result ignored = frame_->CallStoreIC();
          // Drop the duplicated receiver and ignore the result.
          frame_->Drop();
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


// Materialize the array literal 'node' in the literals array 'literals'
// of the function.  Leave the array boilerplate in 'boilerplate'.
class DeferredArrayLiteral: public DeferredCode {
 public:
  DeferredArrayLiteral(Register boilerplate,
                       Register literals,
                       ArrayLiteral* node)
      : boilerplate_(boilerplate), literals_(literals), node_(node) {
    set_comment("[ DeferredArrayLiteral");
  }

  void Generate();

 private:
  Register boilerplate_;
  Register literals_;
  ArrayLiteral* node_;
};


void DeferredArrayLiteral::Generate() {
  // Since the entry is undefined we call the runtime system to
  // compute the literal.
  // Literal array (0).
  __ push(literals_);
  // Literal index (1).
  __ push(Immediate(Smi::FromInt(node_->literal_index())));
  // Constant properties (2).
  __ push(Immediate(node_->literals()));
  __ CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);
  if (!boilerplate_.is(eax)) __ mov(boilerplate_, eax);
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  Comment cmnt(masm_, "[ ArrayLiteral");

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

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code passing the literals array.
  DeferredArrayLiteral* deferred =
      new DeferredArrayLiteral(boilerplate.reg(), literals.reg(), node);
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->Branch(equal);
  deferred->BindExit();
  literals.Unuse();

  // Push the resulting array literal boilerplate on the stack.
  frame_->Push(&boilerplate);
  // Clone the boilerplate object.
  Runtime::FunctionId clone_function_id = Runtime::kCloneLiteralBoilerplate;
  if (node->depth() == 1) {
    clone_function_id = Runtime::kCloneShallowLiteralBoilerplate;
  }
  Result clone = frame_->CallRuntime(clone_function_id, 1);
  // Push the newly cloned literal object as the result.
  frame_->Push(&clone);

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


void CodeGenerator::VisitAssignment(Assignment* node) {
  Comment cmnt(masm_, "[ Assignment");
  CodeForStatementPosition(node);

  { Reference target(this, node->target());
    if (target.is_illegal()) {
      // Fool the virtual frame into thinking that we left the assignment's
      // value on the frame.
      frame_->Push(Smi::FromInt(0));
      return;
    }
    Variable* var = node->target()->AsVariableProxy()->AsVariable();

    if (node->starts_initialization_block()) {
      ASSERT(target.type() == Reference::NAMED ||
             target.type() == Reference::KEYED);
      // Change to slow case in the beginning of an initialization
      // block to avoid the quadratic behavior of repeatedly adding
      // fast properties.

      // The receiver is the argument to the runtime call.  It is the
      // first value pushed when the reference was loaded to the
      // frame.
      frame_->PushElementAt(target.size() - 1);
      Result ignored = frame_->CallRuntime(Runtime::kToSlowProperties, 1);
    }
    if (node->op() == Token::ASSIGN ||
        node->op() == Token::INIT_VAR ||
        node->op() == Token::INIT_CONST) {
      Load(node->value());

    } else {
      Literal* literal = node->value()->AsLiteral();
      bool overwrite_value =
          (node->value()->AsBinaryOperation() != NULL &&
           node->value()->AsBinaryOperation()->ResultOverwriteAllowed());
      Variable* right_var = node->value()->AsVariableProxy()->AsVariable();
      // There are two cases where the target is not read in the right hand
      // side, that are easy to test for: the right hand side is a literal,
      // or the right hand side is a different variable.  TakeValue invalidates
      // the target, with an implicit promise that it will be written to again
      // before it is read.
      if (literal != NULL || (right_var != NULL && right_var != var)) {
        target.TakeValue(NOT_INSIDE_TYPEOF);
      } else {
        target.GetValue(NOT_INSIDE_TYPEOF);
      }
      Load(node->value());
      GenericBinaryOperation(node->binary_op(),
                             node->type(),
                             overwrite_value ? OVERWRITE_RIGHT : NO_OVERWRITE);
    }

    if (var != NULL &&
        var->mode() == Variable::CONST &&
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
      if (node->ends_initialization_block()) {
        ASSERT(target.type() == Reference::NAMED ||
               target.type() == Reference::KEYED);
        // End of initialization block. Revert to fast case.  The
        // argument to the runtime call is the receiver, which is the
        // first value pushed as part of the reference, which is below
        // the lhs value.
        frame_->PushElementAt(target.size());
        Result ignored = frame_->CallRuntime(Runtime::kToFastProperties, 1);
      }
    }
  }
}


void CodeGenerator::VisitThrow(Throw* node) {
  Comment cmnt(masm_, "[ Throw");
  CodeForStatementPosition(node);

  Load(node->exception());
  Result result = frame_->CallRuntime(Runtime::kThrow, 1);
  frame_->Push(&result);
}


void CodeGenerator::VisitProperty(Property* node) {
  Comment cmnt(masm_, "[ Property");
  Reference property(this, node);
  property.GetValue(typeof_state());
}


void CodeGenerator::VisitCall(Call* node) {
  Comment cmnt(masm_, "[ Call");

  ZoneList<Expression*>* args = node->arguments();

  CodeForStatementPosition(node);

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

  if (var != NULL && !var->is_this() && var->is_global()) {
    // ----------------------------------
    // JavaScript example: 'foo(1, 2, 3)'  // foo is global
    // ----------------------------------

    // Push the name of the function and the receiver onto the stack.
    frame_->Push(var->name());

    // Pass the global object as the receiver and let the IC stub
    // patch the stack to use the global proxy as 'this' in the
    // invoked function.
    LoadGlobal();

    // Load the arguments.
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      Load(args->at(i));
    }

    // Call the IC initialization code.
    CodeForSourcePosition(node->position());
    Result result = frame_->CallCallIC(RelocInfo::CODE_TARGET_CONTEXT,
                                       arg_count,
                                       loop_nesting());
    frame_->RestoreContextRegister();
    // Replace the function on the stack with the result.
    frame_->SetElementAt(0, &result);

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
    CallWithArguments(args, node->position());

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
        CallApplyLazy(property,
                      args->at(0),
                      args->at(1)->AsVariableProxy(),
                      node->position());

      } else {
        // Push the name of the function and the receiver onto the stack.
        frame_->Push(name);
        Load(property->obj());

        // Load the arguments.
        int arg_count = args->length();
        for (int i = 0; i < arg_count; i++) {
          Load(args->at(i));
        }

        // Call the IC initialization code.
        CodeForSourcePosition(node->position());
        Result result =
            frame_->CallCallIC(RelocInfo::CODE_TARGET, arg_count,
                               loop_nesting());
        frame_->RestoreContextRegister();
        // Replace the function on the stack with the result.
        frame_->SetElementAt(0, &result);
      }

    } else {
      // -------------------------------------------
      // JavaScript example: 'array[index](1, 2, 3)'
      // -------------------------------------------

      // Load the function to call from the property through a reference.
      Reference ref(this, property);
      ref.GetValue(NOT_INSIDE_TYPEOF);

      // Pass receiver to called function.
      if (property->is_synthetic()) {
        // Use global object as receiver.
        LoadGlobalReceiver();
      } else {
        // The reference's size is non-negative.
        frame_->PushElementAt(ref.size());
      }

      // Call the function.
      CallWithArguments(args, node->position());
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
    CallWithArguments(args, node->position());
  }
}


void CodeGenerator::VisitCallNew(CallNew* node) {
  Comment cmnt(masm_, "[ CallNew");
  CodeForStatementPosition(node);

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


void CodeGenerator::VisitCallEval(CallEval* node) {
  Comment cmnt(masm_, "[ CallEval");

  // In a call to eval, we first call %ResolvePossiblyDirectEval to resolve
  // the function we need to call and the receiver of the call.
  // Then we call the resolved function using the given arguments.

  ZoneList<Expression*>* args = node->arguments();
  Expression* function = node->expression();

  CodeForStatementPosition(node);

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

  // Resolve the call.
  Result result =
      frame_->CallRuntime(Runtime::kResolvePossiblyDirectEval, 2);

  // Touch up the stack with the right values for the function and the
  // receiver.  Use a scratch register to avoid destroying the result.
  Result scratch = allocator_->Allocate();
  ASSERT(scratch.is_valid());
  __ mov(scratch.reg(), FieldOperand(result.reg(), FixedArray::kHeaderSize));
  frame_->SetElementAt(arg_count + 1, &scratch);

  // We can reuse the result register now.
  frame_->Spill(result.reg());
  __ mov(result.reg(),
         FieldOperand(result.reg(), FixedArray::kHeaderSize + kPointerSize));
  frame_->SetElementAt(arg_count, &result);

  // Call the function.
  CodeForSourcePosition(node->position());
  InLoopFlag in_loop = loop_nesting() > 0 ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub call_function(arg_count, in_loop);
  result = frame_->CallStub(&call_function, arg_count + 1);

  // Restore the context and overwrite the function on the stack with
  // the result.
  frame_->RestoreContextRegister();
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
// It can handle flat and sliced strings, 8 and 16 bit characters and
// cons strings where the answer is found in the left hand branch of the
// cons.  The slow case will flatten the string, which will ensure that
// the answer is in the left hand side the next time around.
void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* args) {
  Comment(masm_, "[ GenerateFastCharCodeAt");
  ASSERT(args->length() == 2);

  Label slow_case;
  Label end;
  Label not_a_flat_string;
  Label a_cons_string;
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
  __ sar(index.reg(), kSmiTagSize);

  __ bind(&try_again_with_new_string);
  // Fetch the instance type of the receiver into ecx.
  __ mov(ecx, FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the slow case.
  __ test(ecx, Immediate(kIsNotStringMask));
  __ j(not_zero, &slow_case);

  // Here we make assumptions about the tag values and the shifts needed.
  // See the comment in objects.h.
  ASSERT(kLongStringTag == 0);
  ASSERT(kMediumStringTag + String::kLongLengthShift ==
         String::kMediumLengthShift);
  ASSERT(kShortStringTag + String::kLongLengthShift ==
         String::kShortLengthShift);
  __ and_(ecx, kStringSizeMask);
  __ add(Operand(ecx), Immediate(String::kLongLengthShift));
  // Fetch the length field into the temporary register.
  __ mov(temp.reg(), FieldOperand(object.reg(), String::kLengthOffset));
  __ shr(temp.reg());  // The shift amount in ecx is implicit operand.
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
  ASSERT(kSmiTag == 0);
  __ shl(temp.reg(), kSmiTagSize);
  __ jmp(&end);

  // Handle non-flat strings.
  __ bind(&not_a_flat_string);
  __ and_(temp.reg(), kStringRepresentationMask);
  __ cmp(temp.reg(), kConsStringTag);
  __ j(equal, &a_cons_string);
  __ cmp(temp.reg(), kSlicedStringTag);
  __ j(not_equal, &slow_case);

  // SlicedString.
  // Add the offset to the index and trigger the slow case on overflow.
  __ add(index.reg(), FieldOperand(object.reg(), SlicedString::kStartOffset));
  __ j(overflow, &slow_case);
  // Getting the underlying string is done by running the cons string code.

  // ConsString.
  __ bind(&a_cons_string);
  // Get the first of the two strings.  Both sliced and cons strings
  // store their source string at the same offset.
  ASSERT(SlicedString::kBufferOffset == ConsString::kFirstOffset);
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


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  // Get the frame pointer for the calling frame.
  Result fp = allocator()->Allocate();
  __ mov(fp.reg(), Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ cmp(Operand(fp.reg(), StandardFrameConstants::kContextOffset),
         Immediate(ArgumentsAdaptorFrame::SENTINEL));
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
  Result count(Handle<Smi>(Smi::FromInt(scope_->num_parameters())));
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
  Result count(Handle<Smi>(Smi::FromInt(scope_->num_parameters())));
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


void CodeGenerator::GenerateFastMathOp(MathOp op, ZoneList<Expression*>* args) {
  JumpTarget done;
  JumpTarget call_runtime;
  ASSERT(args->length() == 1);

  // Load number and duplicate it.
  Load(args->at(0));
  frame_->Dup();

  // Get the number into an unaliased register and load it onto the
  // floating point stack still leaving one copy on the frame.
  Result number = frame_->Pop();
  number.ToRegister();
  frame_->Spill(number.reg());
  FloatingPointHelper::LoadFloatOperand(masm_, number.reg());
  number.Unuse();

  // Perform the operation on the number.
  switch (op) {
    case SIN:
      __ fsin();
      break;
    case COS:
      __ fcos();
      break;
  }

  // Go slow case if argument to operation is out of range.
  Result eax_reg = allocator_->Allocate(eax);
  ASSERT(eax_reg.is_valid());
  __ fnstsw_ax();
  __ sahf();
  eax_reg.Unuse();
  call_runtime.Branch(parity_even, not_taken);

  // Allocate heap number for result if possible.
  Result scratch1 = allocator()->Allocate();
  Result scratch2 = allocator()->Allocate();
  Result heap_number = allocator()->Allocate();
  FloatingPointHelper::AllocateHeapNumber(masm_,
                                          call_runtime.entry_label(),
                                          scratch1.reg(),
                                          scratch2.reg(),
                                          heap_number.reg());
  scratch1.Unuse();
  scratch2.Unuse();

  // Store the result in the allocated heap number.
  __ fstp_d(FieldOperand(heap_number.reg(), HeapNumber::kValueOffset));
  // Replace the extra copy of the argument with the result.
  frame_->SetElementAt(0, &heap_number);
  done.Jump();

  call_runtime.Bind();
  // Free ST(0) which was not popped before calling into the runtime.
  __ ffree(0);
  Result answer;
  switch (op) {
    case SIN:
      answer = frame_->CallRuntime(Runtime::kMath_sin, 1);
      break;
    case COS:
      answer = frame_->CallRuntime(Runtime::kMath_cos, 1);
      break;
  }
  frame_->Push(&answer);
  done.Bind();
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
  if (CheckForInlineRuntimeCall(node)) {
    return;
  }

  ZoneList<Expression*>* args = node->arguments();
  Comment cmnt(masm_, "[ CallRuntime");
  Runtime::Function* function = node->function();

  if (function == NULL) {
    // Prepare stack for calling JS runtime function.
    frame_->Push(node->name());
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
    Result answer = frame_->CallCallIC(RelocInfo::CODE_TARGET,
                                       arg_count,
                                       loop_nesting_);
    frame_->RestoreContextRegister();
    frame_->SetElementAt(0, &answer);
  } else {
    // Call the C runtime function.
    Result answer = frame_->CallRuntime(function, arg_count);
    frame_->Push(&answer);
  }
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
  // Note that because of NOT and an optimization in comparison of a typeof
  // expression to a literal string, this function can fail to leave a value
  // on top of the frame or in the cc register.
  Comment cmnt(masm_, "[ UnaryOperation");

  Token::Value op = node->op();

  if (op == Token::NOT) {
    // Swap the true and false targets but keep the same actual label
    // as the fall through.
    destination()->Invert();
    LoadCondition(node->expression(), NOT_INSIDE_TYPEOF, destination(), true);
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
    switch (op) {
      case Token::SUB: {
        bool overwrite =
            (node->AsBinaryOperation() != NULL &&
             node->AsBinaryOperation()->ResultOverwriteAllowed());
        UnarySubStub stub(overwrite);
        // TODO(1222589): remove dependency of TOS being cached inside stub
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

        frame_->Push(&operand);  // undo popping of TOS
        Result answer = frame_->InvokeBuiltin(Builtins::BIT_NOT,
                                              CALL_FUNCTION, 1);

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

  { Reference target(this, node->expression());
    if (target.is_illegal()) {
      // Spoof the virtual frame to have the expected height (one higher
      // than on entry).
      if (!is_postfix) frame_->Push(Smi::FromInt(0));
      return;
    }
    target.TakeValue(NOT_INSIDE_TYPEOF);

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
  // Note that due to an optimization in comparison operations (typeof
  // compared to a string literal), we can evaluate a binary expression such
  // as AND or OR and not leave a value on the frame or in the cc register.
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
    LoadCondition(node->left(), NOT_INSIDE_TYPEOF, &dest, false);

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
        LoadCondition(node->right(), NOT_INSIDE_TYPEOF, destination(), false);
      } else {
        // We have actually just jumped to or bound the current false
        // target but the current control destination is not marked as
        // used.
        destination()->Use(false);
      }

    } else if (dest.is_used()) {
      // The left subexpression compiled to control flow (and is_true
      // was just bound), so the right is free to do so as well.
      LoadCondition(node->right(), NOT_INSIDE_TYPEOF, destination(), false);

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
    LoadCondition(node->left(), NOT_INSIDE_TYPEOF, &dest, false);

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
        LoadCondition(node->right(), NOT_INSIDE_TYPEOF, destination(), false);
      } else {
        // We have just jumped to or bound the current true target but
        // the current control destination is not marked as used.
        destination()->Use(true);
      }

    } else if (dest.is_used()) {
      // The left subexpression compiled to control flow (and is_false
      // was just bound), so the right is free to do so as well.
      LoadCondition(node->right(), NOT_INSIDE_TYPEOF, destination(), false);

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
      __ mov(temp.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
      __ movzx_b(temp.reg(),
                 FieldOperand(temp.reg(), Map::kInstanceTypeOffset));
      __ cmp(temp.reg(), FIRST_NONSTRING_TYPE);
      temp.Unuse();
      answer.Unuse();
      destination()->Split(less);

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
      answer.Unuse();
      destination()->Split(equal);

    } else if (check->Equals(Heap::object_symbol())) {
      __ test(answer.reg(), Immediate(kSmiTagMask));
      destination()->false_target()->Branch(zero);
      __ cmp(answer.reg(), Factory::null_value());
      destination()->true_target()->Branch(equal);

      // It can be an undetectable object.
      Result map = allocator()->Allocate();
      ASSERT(map.is_valid());
      __ mov(map.reg(), FieldOperand(answer.reg(), HeapObject::kMapOffset));
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
      __ test(answer.reg(), Operand(answer.reg()));
      answer.Unuse();
      destination()->Split(zero);
      return;
    }
    default:
      UNREACHABLE();
  }
  Load(left);
  Load(right);
  Comparison(cc, strict, destination());
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
  __ push(receiver_);
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
  __ pop(receiver_);
}


class DeferredReferenceGetKeyedValue: public DeferredCode {
 public:
  explicit DeferredReferenceGetKeyedValue(Register dst,
                                          Register receiver,
                                          Register key,
                                          bool is_global)
      : dst_(dst), receiver_(receiver), key_(key), is_global_(is_global) {
    set_comment("[ DeferredReferenceGetKeyedValue");
  }

  virtual void Generate();

  Label* patch_site() { return &patch_site_; }

 private:
  Label patch_site_;
  Register dst_;
  Register receiver_;
  Register key_;
  bool is_global_;
};


void DeferredReferenceGetKeyedValue::Generate() {
  __ push(receiver_);  // First IC argument.
  __ push(key_);       // Second IC argument.

  // Calculate the delta from the IC call instruction to the map check
  // cmp instruction in the inlined version.  This delta is stored in
  // a test(eax, delta) instruction after the call so that we can find
  // it in the IC initialization code and patch the cmp instruction.
  // This means that we cannot allow test instructions after calls to
  // KeyedLoadIC stubs in other places.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  RelocInfo::Mode mode = is_global_
                         ? RelocInfo::CODE_TARGET_CONTEXT
                         : RelocInfo::CODE_TARGET;
  __ call(ic, mode);
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
  __ pop(key_);
  __ pop(receiver_);
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
      cgen_->LoadFromSlotCheckForArguments(slot, typeof_state);
      break;
    }

    case NAMED: {
      // TODO(1241834): Make sure that it is safe to ignore the
      // distinction between expressions in a typeof and not in a
      // typeof. If there is a chance that reference errors can be
      // thrown below, we must distinguish between the two kinds of
      // loads (typeof expression loads must not throw a reference
      // error).
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());

      // Do not inline the inobject property case for loads from the global
      // object.  Also do not inline for unoptimized code.  This saves time
      // in the code generator.  Unoptimized code is toplevel code or code
      // that is not in a loop.
      if (is_global ||
          cgen_->scope()->is_global_scope() ||
          cgen_->loop_nesting() == 0) {
        Comment cmnt(masm, "[ Load from named Property");
        cgen_->frame()->Push(GetName());

        RelocInfo::Mode mode = is_global
                               ? RelocInfo::CODE_TARGET_CONTEXT
                               : RelocInfo::CODE_TARGET;
        Result answer = cgen_->frame()->CallLoadIC(mode);
        // A test eax instruction following the call signals that the
        // inobject property case was inlined.  Ensure that there is not
        // a test eax instruction here.
        __ nop();
        cgen_->frame()->Push(&answer);
      } else {
        // Inline the inobject property case.
        Comment cmnt(masm, "[ Inlined named property load");
        Result receiver = cgen_->frame()->Pop();
        receiver.ToRegister();

        Result value = cgen_->allocator()->Allocate();
        ASSERT(value.is_valid());
        DeferredReferenceGetNamedValue* deferred =
            new DeferredReferenceGetNamedValue(value.reg(),
                                               receiver.reg(),
                                               GetName());

        // Check that the receiver is a heap object.
        __ test(receiver.reg(), Immediate(kSmiTagMask));
        deferred->Branch(zero);

        __ bind(deferred->patch_site());
        // This is the map check instruction that will be patched (so we can't
        // use the double underscore macro that may insert instructions).
        // Initially use an invalid map to force a failure.
        masm->cmp(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                  Immediate(Factory::null_value()));
        // This branch is always a forwards branch so it's always a fixed
        // size which allows the assert below to succeed and patching to work.
        deferred->Branch(not_equal);

        // The delta from the patch label to the load offset must be
        // statically known.
        ASSERT(masm->SizeOfCodeGeneratedSince(deferred->patch_site()) ==
               LoadIC::kOffsetToLoadInstruction);
        // The initial (invalid) offset has to be large enough to force
        // a 32-bit instruction encoding to allow patching with an
        // arbitrary offset.  Use kMaxInt (minus kHeapObjectTag).
        int offset = kMaxInt;
        masm->mov(value.reg(), FieldOperand(receiver.reg(), offset));

        __ IncrementCounter(&Counters::named_load_inline, 1);
        deferred->BindExit();
        cgen_->frame()->Push(&receiver);
        cgen_->frame()->Push(&value);
      }
      break;
    }

    case KEYED: {
      // TODO(1241834): Make sure that this it is safe to ignore the
      // distinction between expressions in a typeof and not in a typeof.
      Comment cmnt(masm, "[ Load from keyed Property");
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());

      // Inline array load code if inside of a loop.  We do not know
      // the receiver map yet, so we initially generate the code with
      // a check against an invalid map.  In the inline cache code, we
      // patch the map check if appropriate.
      if (cgen_->loop_nesting() > 0) {
        Comment cmnt(masm, "[ Inlined load from keyed Property");

        Result key = cgen_->frame()->Pop();
        Result receiver = cgen_->frame()->Pop();
        key.ToRegister();
        receiver.ToRegister();

        // Use a fresh temporary to load the elements without destroying
        // the receiver which is needed for the deferred slow case.
        Result elements = cgen_->allocator()->Allocate();
        ASSERT(elements.is_valid());

        // Use a fresh temporary for the index and later the loaded
        // value.
        Result index = cgen_->allocator()->Allocate();
        ASSERT(index.is_valid());

        DeferredReferenceGetKeyedValue* deferred =
            new DeferredReferenceGetKeyedValue(index.reg(),
                                               receiver.reg(),
                                               key.reg(),
                                               is_global);

        // Check that the receiver is not a smi (only needed if this
        // is not a load from the global context) and that it has the
        // expected map.
        if (!is_global) {
          __ test(receiver.reg(), Immediate(kSmiTagMask));
          deferred->Branch(zero);
        }

        // Initially, use an invalid map. The map is patched in the IC
        // initialization code.
        __ bind(deferred->patch_site());
        // Use masm-> here instead of the double underscore macro since extra
        // coverage code can interfere with the patching.
        masm->cmp(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
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
        __ mov(index.reg(), key.reg());
        __ sar(index.reg(), kSmiTagSize);
        __ cmp(index.reg(),
               FieldOperand(elements.reg(), FixedArray::kLengthOffset));
        deferred->Branch(above_equal);

        // Load and check that the result is not the hole.  We could
        // reuse the index or elements register for the value.
        //
        // TODO(206): Consider whether it makes sense to try some
        // heuristic about which register to reuse.  For example, if
        // one is eax, the we can reuse that one because the value
        // coming from the deferred code will be in eax.
        Result value = index;
        __ mov(value.reg(), Operand(elements.reg(),
                                    index.reg(),
                                    times_4,
                                    FixedArray::kHeaderSize - kHeapObjectTag));
        elements.Unuse();
        index.Unuse();
        __ cmp(Operand(value.reg()), Immediate(Factory::the_hole_value()));
        deferred->Branch(equal);
        __ IncrementCounter(&Counters::keyed_load_inline, 1);

        deferred->BindExit();
        // Restore the receiver and key to the frame and push the
        // result on top of it.
        cgen_->frame()->Push(&receiver);
        cgen_->frame()->Push(&key);
        cgen_->frame()->Push(&value);

      } else {
        Comment cmnt(masm, "[ Load from keyed Property");
        RelocInfo::Mode mode = is_global
                               ? RelocInfo::CODE_TARGET_CONTEXT
                               : RelocInfo::CODE_TARGET;
        Result answer = cgen_->frame()->CallKeyedLoadIC(mode);
        // Make sure that we do not have a test instruction after the
        // call.  A test instruction after the call is used to
        // indicate that we have generated an inline version of the
        // keyed load.  The explicit nop instruction is here because
        // the push that follows might be peep-hole optimized away.
        __ nop();
        cgen_->frame()->Push(&answer);
      }
      break;
    }

    default:
      UNREACHABLE();
  }
}


void Reference::TakeValue(TypeofState typeof_state) {
  // For non-constant frame-allocated slots, we invalidate the value in the
  // slot.  For all others, we fall back on GetValue.
  ASSERT(!cgen_->in_spilled_code());
  ASSERT(!is_illegal());
  if (type_ != SLOT) {
    GetValue(typeof_state);
    return;
  }

  Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
  ASSERT(slot != NULL);
  if (slot->type() == Slot::LOOKUP ||
      slot->type() == Slot::CONTEXT ||
      slot->var()->mode() == Variable::CONST ||
      slot->is_arguments()) {
    GetValue(typeof_state);
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
      break;
    }

    case NAMED: {
      Comment cmnt(masm, "[ Store to named Property");
      cgen_->frame()->Push(GetName());
      Result answer = cgen_->frame()->CallStoreIC();
      cgen_->frame()->Push(&answer);
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Store to keyed Property");

      // Generate inlined version of the keyed store if the code is in
      // a loop and the key is likely to be a smi.
      Property* property = expression()->AsProperty();
      ASSERT(property != NULL);
      SmiAnalysis* key_smi_analysis = property->key()->type();

      if (cgen_->loop_nesting() > 0 && key_smi_analysis->IsLikelySmi()) {
        Comment cmnt(masm, "[ Inlined store to keyed Property");

        // Get the receiver, key and value into registers.
        Result value = cgen_->frame()->Pop();
        Result key = cgen_->frame()->Pop();
        Result receiver = cgen_->frame()->Pop();

        Result tmp = cgen_->allocator_->Allocate();
        ASSERT(tmp.is_valid());

        // Determine whether the value is a constant before putting it
        // in a register.
        bool value_is_constant = value.is_constant();

        // Make sure that value, key and receiver are in registers.
        value.ToRegister();
        key.ToRegister();
        receiver.ToRegister();

        DeferredReferenceSetKeyedValue* deferred =
            new DeferredReferenceSetKeyedValue(value.reg(),
                                               key.reg(),
                                               receiver.reg());

        // Check that the value is a smi if it is not a constant.  We
        // can skip the write barrier for smis and constants.
        if (!value_is_constant) {
          __ test(value.reg(), Immediate(kSmiTagMask));
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

        // Check that the key is within bounds.  Both the key and the
        // length of the JSArray are smis.
        __ cmp(key.reg(),
               FieldOperand(receiver.reg(), JSArray::kLengthOffset));
        deferred->Branch(greater_equal);

        // Get the elements array from the receiver and check that it
        // is not a dictionary.
        __ mov(tmp.reg(),
               FieldOperand(receiver.reg(), JSObject::kElementsOffset));
        // Bind the deferred code patch site to be able to locate the
        // fixed array map comparison.  When debugging, we patch this
        // comparison to always fail so that we will hit the IC call
        // in the deferred code which will allow the debugger to
        // break for fast case stores.
        __ bind(deferred->patch_site());
        __ cmp(FieldOperand(tmp.reg(), HeapObject::kMapOffset),
               Immediate(Factory::fixed_array_map()));
        deferred->Branch(not_equal);

        // Store the value.
        __ mov(Operand(tmp.reg(),
                       key.reg(),
                       times_2,
                       FixedArray::kHeaderSize - kHeapObjectTag),
               value.reg());
        __ IncrementCounter(&Counters::keyed_store_inline, 1);

        deferred->BindExit();

        cgen_->frame()->Push(&receiver);
        cgen_->frame()->Push(&key);
        cgen_->frame()->Push(&value);
      } else {
        Result answer = cgen_->frame()->CallKeyedStoreIC();
        // Make sure that we do not have a test instruction after the
        // call.  A test instruction after the call is used to
        // indicate that we have generated an inline version of the
        // keyed store.
        __ nop();
        cgen_->frame()->Push(&answer);
      }
      break;
    }

    default:
      UNREACHABLE();
  }
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
  __ and_(ecx, kStringSizeMask);
  __ cmp(ecx, kShortStringTag);
  __ j(not_equal, &true_result);  // Empty string is always short.
  __ mov(edx, FieldOperand(eax, String::kLengthOffset));
  __ shr(edx, String::kShortLengthShift);
  __ j(zero, &false_result);
  __ jmp(&true_result);

  __ bind(&not_string);
  // HeapNumber => false iff +0, -0, or NaN.
  __ cmp(edx, Factory::heap_number_map());
  __ j(not_equal, &true_result);
  __ fldz();
  __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
  __ fucompp();
  __ push(eax);
  __ fnstsw_ax();
  __ sahf();
  __ pop(eax);
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


void GenericBinaryOpStub::GenerateSmiCode(MacroAssembler* masm, Label* slow) {
  // Perform fast-case smi code for the operation (eax <op> ebx) and
  // leave result in register eax.

  // Prepare the smi check of both operands by or'ing them together
  // before checking against the smi mask.
  __ mov(ecx, Operand(ebx));
  __ or_(ecx, Operand(eax));

  switch (op_) {
    case Token::ADD:
      __ add(eax, Operand(ebx));  // add optimistically
      __ j(overflow, slow, not_taken);
      break;

    case Token::SUB:
      __ sub(eax, Operand(ebx));  // subtract optimistically
      __ j(overflow, slow, not_taken);
      break;

    case Token::DIV:
    case Token::MOD:
      // Sign extend eax into edx:eax.
      __ cdq();
      // Check for 0 divisor.
      __ test(ebx, Operand(ebx));
      __ j(zero, slow, not_taken);
      break;

    default:
      // Fall-through to smi check.
      break;
  }

  // Perform the actual smi check.
  ASSERT(kSmiTag == 0);  // adjust zero check if not the case
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, slow, not_taken);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
      // Do nothing here.
      break;

    case Token::MUL:
      // If the smi tag is 0 we can just leave the tag on one operand.
      ASSERT(kSmiTag == 0);  // adjust code below if not the case
      // Remove tag from one of the operands (but keep sign).
      __ sar(eax, kSmiTagSize);
      // Do multiplication.
      __ imul(eax, Operand(ebx));  // multiplication of smis; result in eax
      // Go slow on overflows.
      __ j(overflow, slow, not_taken);
      // Check for negative zero result.
      __ NegativeZeroTest(eax, ecx, slow);  // use ecx = x | y
      break;

    case Token::DIV:
      // Divide edx:eax by ebx.
      __ idiv(ebx);
      // Check for the corner case of dividing the most negative smi
      // by -1. We cannot use the overflow flag, since it is not set
      // by idiv instruction.
      ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
      __ cmp(eax, 0x40000000);
      __ j(equal, slow);
      // Check for negative zero result.
      __ NegativeZeroTest(eax, ecx, slow);  // use ecx = x | y
      // Check that the remainder is zero.
      __ test(edx, Operand(edx));
      __ j(not_zero, slow);
      // Tag the result and store it in register eax.
      ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
      __ lea(eax, Operand(eax, eax, times_1, kSmiTag));
      break;

    case Token::MOD:
      // Divide edx:eax by ebx.
      __ idiv(ebx);
      // Check for negative zero result.
      __ NegativeZeroTest(edx, ecx, slow);  // use ecx = x | y
      // Move remainder to register eax.
      __ mov(eax, Operand(edx));
      break;

    case Token::BIT_OR:
      __ or_(eax, Operand(ebx));
      break;

    case Token::BIT_AND:
      __ and_(eax, Operand(ebx));
      break;

    case Token::BIT_XOR:
      __ xor_(eax, Operand(ebx));
      break;

    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      // Move the second operand into register ecx.
      __ mov(ecx, Operand(ebx));
      // Remove tags from operands (but keep sign).
      __ sar(eax, kSmiTagSize);
      __ sar(ecx, kSmiTagSize);
      // Perform the operation.
      switch (op_) {
        case Token::SAR:
          __ sar(eax);
          // No checks of result necessary
          break;
        case Token::SHR:
          __ shr(eax);
          // Check that the *unsigned* result fits in a smi.
          // Neither of the two high-order bits can be set:
          // - 0x80000000: high bit would be lost when smi tagging.
          // - 0x40000000: this number would convert to negative when
          // Smi tagging these two cases can only happen with shifts
          // by 0 or 1 when handed a valid smi.
          __ test(eax, Immediate(0xc0000000));
          __ j(not_zero, slow, not_taken);
          break;
        case Token::SHL:
          __ shl(eax);
          // Check that the *signed* result fits in a smi.
          __ cmp(eax, 0xc0000000);
          __ j(sign, slow, not_taken);
          break;
        default:
          UNREACHABLE();
      }
      // Tag the result and store it in register eax.
      ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
      __ lea(eax, Operand(eax, eax, times_1, kSmiTag));
      break;

    default:
      UNREACHABLE();
      break;
  }
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  Label call_runtime;

  if (flags_ == SMI_CODE_IN_STUB) {
    // The fast case smi code wasn't inlined in the stub caller
    // code. Generate it here to speed up common operations.
    Label slow;
    __ mov(ebx, Operand(esp, 1 * kPointerSize));  // get y
    __ mov(eax, Operand(esp, 2 * kPointerSize));  // get x
    GenerateSmiCode(masm, &slow);
    __ ret(2 * kPointerSize);  // remove both operands

    // Too bad. The fast case smi code didn't succeed.
    __ bind(&slow);
  }

  // Setup registers.
  __ mov(eax, Operand(esp, 1 * kPointerSize));  // get y
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // get x

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      // eax: y
      // edx: x
      FloatingPointHelper::CheckFloatOperands(masm, &call_runtime, ebx);
      // Fast-case: Both operands are numbers.
      // Allocate a heap number, if needed.
      Label skip_allocation;
      switch (mode_) {
        case OVERWRITE_LEFT:
          __ mov(eax, Operand(edx));
          // Fall through!
        case OVERWRITE_RIGHT:
          // If the argument in eax is already an object, we skip the
          // allocation of a heap number.
          __ test(eax, Immediate(kSmiTagMask));
          __ j(not_zero, &skip_allocation, not_taken);
          // Fall through!
        case NO_OVERWRITE:
          FloatingPointHelper::AllocateHeapNumber(masm,
                                                  &call_runtime,
                                                  ecx,
                                                  edx,
                                                  eax);
          __ bind(&skip_allocation);
          break;
        default: UNREACHABLE();
      }
      FloatingPointHelper::LoadFloatOperands(masm, ecx);

      switch (op_) {
        case Token::ADD: __ faddp(1); break;
        case Token::SUB: __ fsubp(1); break;
        case Token::MUL: __ fmulp(1); break;
        case Token::DIV: __ fdivp(1); break;
        default: UNREACHABLE();
      }
      __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
      __ ret(2 * kPointerSize);
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
      FloatingPointHelper::CheckFloatOperands(masm, &call_runtime, ebx);
      FloatingPointHelper::LoadFloatOperands(masm, ecx);

      Label skip_allocation, non_smi_result, operand_conversion_failure;

      // Reserve space for converted numbers.
      __ sub(Operand(esp), Immediate(2 * kPointerSize));

      if (use_sse3_) {
        // Truncate the operands to 32-bit integers and check for
        // exceptions in doing so.
        CpuFeatures::Scope scope(CpuFeatures::SSE3);
        __ fisttp_s(Operand(esp, 0 * kPointerSize));
        __ fisttp_s(Operand(esp, 1 * kPointerSize));
        __ fnstsw_ax();
        __ test(eax, Immediate(1));
        __ j(not_zero, &operand_conversion_failure);
      } else {
        // Check if right operand is int32.
        __ fist_s(Operand(esp, 0 * kPointerSize));
        __ fild_s(Operand(esp, 0 * kPointerSize));
        __ fucompp();
        __ fnstsw_ax();
        __ sahf();
        __ j(not_zero, &operand_conversion_failure);
        __ j(parity_even, &operand_conversion_failure);

        // Check if left operand is int32.
        __ fist_s(Operand(esp, 1 * kPointerSize));
        __ fild_s(Operand(esp, 1 * kPointerSize));
        __ fucompp();
        __ fnstsw_ax();
        __ sahf();
        __ j(not_zero, &operand_conversion_failure);
        __ j(parity_even, &operand_conversion_failure);
      }

      // Get int32 operands and perform bitop.
      __ pop(ecx);
      __ pop(eax);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
        case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
        case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
        case Token::SAR: __ sar(eax); break;
        case Token::SHL: __ shl(eax); break;
        case Token::SHR: __ shr(eax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative and fits in a smi.
        __ test(eax, Immediate(0xc0000000));
        __ j(not_zero, &non_smi_result);
      } else {
        // Check if result fits in a smi.
        __ cmp(eax, 0xc0000000);
        __ j(negative, &non_smi_result);
      }
      // Tag smi result and return.
      ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
      __ lea(eax, Operand(eax, eax, times_1, kSmiTag));
      __ ret(2 * kPointerSize);

      // All ops except SHR return a signed int32 that we load in a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, Operand(eax));  // ebx: result
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
            FloatingPointHelper::AllocateHeapNumber(masm, &call_runtime,
                                                    ecx, edx, eax);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        __ mov(Operand(esp, 1 * kPointerSize), ebx);
        __ fild_s(Operand(esp, 1 * kPointerSize));
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(2 * kPointerSize);
      }

      // Clear the FPU exception flag and reset the stack before calling
      // the runtime system.
      __ bind(&operand_conversion_failure);
      __ add(Operand(esp), Immediate(2 * kPointerSize));
      if (use_sse3_) {
        // If we've used the SSE3 instructions for truncating the
        // floating point values to integers and it failed, we have a
        // pending #IA exception. Clear it.
        __ fnclex();
      } else {
        // The non-SSE3 variant does early bailout if the right
        // operand isn't a 32-bit integer, so we may have a single
        // value on the FPU stack we need to get rid of.
        __ ffree(0);
      }

      // SHR should return uint32 - go to runtime for non-smi/negative result.
      if (op_ == Token::SHR) {
        __ bind(&non_smi_result);
      }
      __ mov(eax, Operand(esp, 1 * kPointerSize));
      __ mov(edx, Operand(esp, 2 * kPointerSize));
      break;
    }
    default: UNREACHABLE(); break;
  }

  // If all else fails, use the runtime system to get the correct
  // result.
  __ bind(&call_runtime);
  switch (op_) {
    case Token::ADD: {
      // Test for string arguments before calling runtime.
      Label not_strings, both_strings, not_string1, string1;
      Result answer;
      __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
      __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.
      __ test(eax, Immediate(kSmiTagMask));
      __ j(zero, &not_string1);
      __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, eax);
      __ j(above_equal, &not_string1);

      // First argument is a a string, test second.
      __ test(edx, Immediate(kSmiTagMask));
      __ j(zero, &string1);
      __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, edx);
      __ j(above_equal, &string1);

      // First and second argument are strings.
      __ TailCallRuntime(ExternalReference(Runtime::kStringAdd), 2);

      // Only first argument is a string.
      __ bind(&string1);
      __ InvokeBuiltin(Builtins::STRING_ADD_LEFT, JUMP_FUNCTION);

      // First argument was not a string, test second.
      __ bind(&not_string1);
      __ test(edx, Immediate(kSmiTagMask));
      __ j(zero, &not_strings);
      __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, edx);
      __ j(above_equal, &not_strings);

      // Only second argument is a string.
      __ InvokeBuiltin(Builtins::STRING_ADD_RIGHT, JUMP_FUNCTION);

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


void FloatingPointHelper::AllocateHeapNumber(MacroAssembler* masm,
                                             Label* need_gc,
                                             Register scratch1,
                                             Register scratch2,
                                             Register result) {
  ExternalReference allocation_top =
      ExternalReference::new_space_allocation_top_address();
  ExternalReference allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  __ mov(Operand(scratch1), Immediate(allocation_top));
  __ mov(result, Operand(scratch1, 0));
  __ lea(scratch2, Operand(result, HeapNumber::kSize));  // scratch2: new top
  __ cmp(scratch2, Operand::StaticVariable(allocation_limit));
  __ j(above, need_gc, not_taken);

  __ mov(Operand(scratch1, 0), scratch2);  // store new top
  __ mov(Operand(result, HeapObject::kMapOffset),
         Immediate(Factory::heap_number_map()));
  // Tag old top and use as result.
  __ add(Operand(result), Immediate(kHeapObjectTag));
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ test(number, Immediate(kSmiTagMask));
  __ j(zero, &load_smi, not_taken);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi);
  __ sar(number, kSmiTagSize);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            Register scratch) {
  Label load_smi_1, load_smi_2, done_load_1, done;
  __ mov(scratch, Operand(esp, 2 * kPointerSize));
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_1, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ bind(&done_load_1);

  __ mov(scratch, Operand(esp, 1 * kPointerSize));
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_2, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_1);
  __ sar(scratch, kSmiTagSize);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);
  __ jmp(&done_load_1);

  __ bind(&load_smi_2);
  __ sar(scratch, kSmiTagSize);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);

  __ bind(&done);
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


void UnarySubStub::Generate(MacroAssembler* masm) {
  Label undo;
  Label slow;
  Label done;
  Label try_float;

  // Check whether the value is a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &try_float, not_taken);

  // Enter runtime system if the value of the expression is zero
  // to make sure that we switch between 0 and -0.
  __ test(eax, Operand(eax));
  __ j(zero, &slow, not_taken);

  // The value of the expression is a smi that is not zero.  Try
  // optimistic subtraction '0 - value'.
  __ mov(edx, Operand(eax));
  __ Set(eax, Immediate(0));
  __ sub(eax, Operand(edx));
  __ j(overflow, &undo, not_taken);

  // If result is a smi we are done.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &done, taken);

  // Restore eax and enter runtime system.
  __ bind(&undo);
  __ mov(eax, Operand(edx));

  // Enter runtime system.
  __ bind(&slow);
  __ pop(ecx);  // pop return address
  __ push(eax);
  __ push(ecx);  // push return address
  __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_FUNCTION);

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
    FloatingPointHelper::AllocateHeapNumber(masm, &undo, ebx, ecx, eax);
    // eax: allocated 'empty' number
    __ mov(ecx, FieldOperand(edx, HeapNumber::kExponentOffset));
    __ xor_(ecx, HeapNumber::kSignMask);  // Flip sign.
    __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), ecx);
    __ mov(ecx, FieldOperand(edx, HeapNumber::kMantissaOffset));
    __ mov(FieldOperand(eax, HeapNumber::kMantissaOffset), ecx);
  }

  __ bind(&done);

  __ StubReturn(1);
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, ArgumentsAdaptorFrame::SENTINEL);
  __ j(equal, &adaptor);

  // Nothing to do: The formal number of parameters has already been
  // passed in register eax by calling function. Just return it.
  __ ret(0);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame and return it.
  __ bind(&adaptor);
  __ mov(eax, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
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
  __ cmp(ecx, ArgumentsAdaptorFrame::SENTINEL);
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
  __ TailCallRuntime(ExternalReference(Runtime::kGetArgumentsProperty), 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // The displacement is used for skipping the return address and the
  // frame pointer on the stack. It is the offset of the last
  // parameter (if any) relative to the frame pointer.
  static const int kDisplacement = 2 * kPointerSize;

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, ArgumentsAdaptorFrame::SENTINEL);
  __ j(not_equal, &runtime);

  // Patch the arguments.length and the parameters pointer.
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(Operand(esp, 1 * kPointerSize), ecx);
  __ lea(edx, Operand(edx, ecx, times_2, kDisplacement));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kNewArgumentsFast), 3);
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
      // It is a heap number, so return non-equal if it's NaN and equal if it's
      // not NaN.
      // The representation of NaN values has all exponent bits (52..62) set,
      // and not all mantissa bits (0..51) clear.
      // Read top bits of double representation (second word of value).
      __ mov(eax, FieldOperand(edx, HeapNumber::kExponentOffset));
      // Test that exponent bits are all set.
      __ not_(eax);
      __ test(eax, Immediate(0x7ff00000));
      __ j(not_zero, &return_equal);
      __ not_(eax);

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ shl(eax, 12);
      // Or with all low-bits of mantissa.
      __ or_(eax, FieldOperand(edx, HeapNumber::kMantissaOffset));
      // Return zero equal if all bits in mantissa is zero (it's an Infinity)
      // and non-zero if not (it's a NaN).
      __ ret(0);

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
  FloatingPointHelper::CheckFloatOperands(masm, &check_for_symbols, ebx);
  FloatingPointHelper::LoadFloatOperands(masm, ecx);
  __ FCmp();

  // Jump to builtin for NaN.
  __ j(parity_even, &call_builtin, not_taken);

  // TODO(1243847): Use cmov below once CpuFeatures are properly hooked up.
  Label below_lbl, above_lbl;
  // use edx, eax to convert unsigned to signed comparison
  __ j(below, &below_lbl, not_taken);
  __ j(above, &above_lbl, not_taken);

  __ xor_(eax, Operand(eax));  // equal
  __ ret(2 * kPointerSize);

  __ bind(&below_lbl);
  __ mov(eax, -1);
  __ ret(2 * kPointerSize);

  __ bind(&above_lbl);
  __ mov(eax, 1);
  __ ret(2 * kPointerSize);  // eax, edx were pushed

  // Fast negative check for symbol-to-symbol equality.
  __ bind(&check_for_symbols);
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &call_builtin, eax, ecx);
    BranchIfNonSymbol(masm, &call_builtin, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(2 * kPointerSize);
  }

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
  __ TailCallRuntime(ExternalReference(Runtime::kStackGuard), 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

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


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_out_of_memory_exception,
                              StackFrame::Type frame_type,
                              bool do_gc,
                              bool always_allocate_scope) {
  // eax: result parameter for PerformGC, if any
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

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
  __ LeaveExitFrame(frame_type);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ test(eax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, taken);

  Label continue_exception;
  // If the returned failure is EXCEPTION then promote Top::pending_exception().
  __ cmp(eax, reinterpret_cast<int32_t>(Failure::Exception()));
  __ j(not_equal, &continue_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ mov(eax, Operand::StaticVariable(pending_exception_address));
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(Operand::StaticVariable(pending_exception_address), edx);

  __ bind(&continue_exception);
  // Special handling of out of memory exception.
  __ cmp(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
  __ j(equal, throw_out_of_memory_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowOutOfMemory(MacroAssembler* masm) {
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

  // Set external caught exception to false.
  ExternalReference external_caught(Top::k_external_caught_exception_address);
  __ mov(eax, false);
  __ mov(Operand::StaticVariable(external_caught), eax);

  // Set pending exception and eax to out of memory exception.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
  __ mov(Operand::StaticVariable(pending_exception), eax);

  // Clear the context pointer;
  __ xor_(esi, Operand(esi));

  // Restore fp from handler and discard handler state.
  ASSERT(StackHandlerConstants::kFPOffset == 1 * kPointerSize);
  __ pop(ebp);
  __ pop(edx);  // State.

  ASSERT(StackHandlerConstants::kPCOffset == 3 * kPointerSize);
  __ ret(0);
}


void CEntryStub::GenerateBody(MacroAssembler* masm, bool is_debug_break) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects instead
  // of a proper result. The builtin entry handles this by performing
  // a garbage collection and retrying the builtin (twice).

  StackFrame::Type frame_type = is_debug_break ?
      StackFrame::EXIT_DEBUG :
      StackFrame::EXIT;

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(frame_type);

  // eax: result parameter for PerformGC, if any (setup below)
  // ebx: pointer to builtin function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver (C callee-saved)
  // esi: argv pointer (C callee-saved)

  Label throw_out_of_memory_exception;
  Label throw_normal_exception;

  // Call into the runtime system.
  GenerateCore(masm, &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               false,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ mov(eax, Immediate(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  GenerateThrowOutOfMemory(masm);
  // control flow for generated will not return.

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

  // Save callee-saved registers (C calling conventions).
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  // Push something that is not an arguments adaptor.
  __ push(Immediate(~ArgumentsAdaptorFrame::SENTINEL));
  __ push(Immediate(Smi::FromInt(marker)));  // @ function offset
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


int CompareStub::MinorKey() {
  // Encode the two parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 15));
  return (static_cast<unsigned>(cc_) << 1) | (strict_ ? 1 : 0);
}

#undef __

} }  // namespace v8::internal
