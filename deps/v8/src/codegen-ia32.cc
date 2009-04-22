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

namespace v8 { namespace internal {

#define __ masm_->

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

CodeGenerator::CodeGenerator(int buffer_size, Handle<Script> script,
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
  frame_ = new VirtualFrame(this);
  set_in_spilled_code(false);

  // Adjust for function-level loop nesting.
  loop_nesting_ += fun->loop_nesting();

  {
    CodeGenState state(this);

    // Entry:
    // Stack: receiver, arguments, return address.
    // ebp: caller's frame pointer
    // esp: stack pointer
    // edi: called JS function
    // esi: callee's context
    allocator_->Initialize();
    frame_->Enter();

#ifdef DEBUG
    if (strlen(FLAG_stop_at) > 0 &&
        fun->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
      frame_->SpillAll();
      __ int3();
    }
#endif

    // Allocate space for locals and initialize them.
    frame_->AllocateStackSlots(scope_->num_stack_slots());
    // Initialize the function return target after the locals are set
    // up, because it needs the expected frame height from the frame.
    function_return_.Initialize(this, JumpTarget::BIDIRECTIONAL);
    function_return_is_shadowed_ = false;

    // Allocate the arguments object and copy the parameters into it.
    if (scope_->arguments() != NULL) {
      ASSERT(scope_->arguments_shadow() != NULL);
      Comment cmnt(masm_, "[ Allocate arguments object");
      ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
      frame_->PushFunction();
      frame_->PushReceiverSlotAddress();
      frame_->Push(Smi::FromInt(scope_->num_parameters()));
      Result answer = frame_->CallStub(&stub, 3);
      frame_->Push(&answer);
    }

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

    // This section stores the pointer to the arguments object that
    // was allocated and copied into above. If the address was not
    // saved to TOS, we push ecx onto the stack.
    //
    // Store the arguments object.  This must happen after context
    // initialization because the arguments object may be stored in the
    // context.
    if (scope_->arguments() != NULL) {
      Comment cmnt(masm_, "[ store arguments object");
      { Reference shadow_ref(this, scope_->arguments_shadow());
        ASSERT(shadow_ref.is_slot());
        { Reference arguments_ref(this, scope_->arguments());
          ASSERT(arguments_ref.is_slot());
          // Here we rely on the convenient property that references to slot
          // take up zero space in the frame (ie, it doesn't matter that the
          // stored value is actually below the reference on the frame).
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
        Result undefined(Factory::undefined_value(), this);
        if (function_return_.is_bound()) {
          function_return_.Jump(&undefined);
        } else {
          // Though this is a (possibly) backward block, the frames
          // can only differ on their top element.
          function_return_.Bind(&undefined, 1);
          GenerateReturnSequence(&undefined);
        }
      } else if (function_return_.is_linked()) {
        // If the return target has dangling jumps to it, then we have not
        // yet generated the return sequence.  This can happen when (a)
        // control does not flow off the end of the body so we did not
        // compile an artificial return statement just above, and (b) there
        // are return statements in the body but (c) they are all shadowed.
        Result return_value(this);
        // Though this is a (possibly) backward block, the frames can
        // only differ on their top element.
        function_return_.Bind(&return_value, 1);
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
  if (HasStackOverflow()) {
    ClearDeferred();
  } else {
    ProcessDeferred();
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
  Result context(esi, this);

  for (Scope* s = scope(); s != slot->var()->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context.reg(), Context::EXTENSION_INDEX),
               Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      __ mov(tmp.reg(), ContextOperand(context.reg(), Context::CLOSURE_INDEX));
      __ mov(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp;
    }
  }
  // Check that last extension is NULL.
  __ cmp(ContextOperand(context.reg(), Context::EXTENSION_INDEX),
         Immediate(0));
  slow->Branch(not_equal, not_taken);
  __ mov(tmp.reg(), ContextOperand(context.reg(), Context::FCONTEXT_INDEX));
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
  JumpTarget true_target(this);
  JumpTarget false_target(this);
  ControlDestination dest(&true_target, &false_target, true);
  LoadCondition(x, typeof_state, &dest, false);

  if (dest.false_was_fall_through()) {
    // The false target was just bound.
    JumpTarget loaded(this);
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
    JumpTarget loaded(this);
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
      JumpTarget loaded(this);
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
                                 Register scratch2);
};


// Flag that indicates whether or not the code that handles smi arguments
// should be placed in the stub, inlined, or omitted entirely.
enum GenericBinaryFlags {
  SMI_CODE_IN_STUB,
  SMI_CODE_INLINED
};


class GenericBinaryOpStub: public CodeStub {
 public:
  GenericBinaryOpStub(Token::Value op,
                      OverwriteMode mode,
                      GenericBinaryFlags flags)
      : op_(op), mode_(mode), flags_(flags) {
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  void GenerateSmiCode(MacroAssembler* masm, Label* slow);

 private:
  Token::Value op_;
  OverwriteMode mode_;
  GenericBinaryFlags flags_;

  const char* GetName();

#ifdef DEBUG
  void Print() {
    PrintF("GenericBinaryOpStub (op %s), (mode %d, flags %d)\n",
           Token::String(op_),
           static_cast<int>(mode_),
           static_cast<int>(flags_));
  }
#endif

  // Minor key encoding in 16 bits FOOOOOOOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 13> {};
  class FlagBits: public BitField<GenericBinaryFlags, 15, 1> {};

  Major MajorKey() { return GenericBinaryOp; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | FlagBits::encode(flags_);
  }
  void Generate(MacroAssembler* masm);
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


// A deferred code class implementing binary operations on likely smis.
// This class generates both inline code and deferred code.
// The fastest path is implemented inline.  Deferred code calls
// the GenericBinaryOpStub stub for slow cases.
class DeferredInlineBinaryOperation: public DeferredCode {
 public:
  DeferredInlineBinaryOperation(CodeGenerator* generator,
                                Token::Value op,
                                OverwriteMode mode,
                                GenericBinaryFlags flags)
      : DeferredCode(generator), stub_(op, mode, flags), op_(op) {
    set_comment("[ DeferredInlineBinaryOperation");
  }

  // Consumes its arguments, left and right, leaving them invalid.
  Result GenerateInlineCode(Result* left, Result* right);

  virtual void Generate();

 private:
  GenericBinaryOpStub stub_;
  Token::Value op_;
};


void DeferredInlineBinaryOperation::Generate() {
  Result left(generator());
  Result right(generator());
  enter()->Bind(&left, &right);
  generator()->frame()->Push(&left);
  generator()->frame()->Push(&right);
  Result answer = generator()->frame()->CallStub(&stub_, 2);
  exit_.Jump(&answer);
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
    bool left_is_string = left.static_type().is_jsstring();
    bool right_is_string = right.static_type().is_jsstring();
    if (left_is_string || right_is_string) {
      frame_->Push(&left);
      frame_->Push(&right);
      Result answer(this);
      if (left_is_string) {
        if (right_is_string) {
          // TODO(lrn): if (left.is_constant() && right.is_constant())
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
      answer.set_static_type(StaticType::jsstring());
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


void CodeGenerator::LikelySmiBinaryOperation(Token::Value op,
                                             Result* left,
                                             Result* right,
                                             OverwriteMode overwrite_mode) {
  // Implements a binary operation using a deferred code object
  // and some inline code to operate on smis quickly.
  DeferredInlineBinaryOperation* deferred =
      new DeferredInlineBinaryOperation(this, op, overwrite_mode,
                                        SMI_CODE_INLINED);
  // Generate the inline code that handles some smi operations,
  // and jumps to the deferred code for everything else.
  Result answer = deferred->GenerateInlineCode(left, right);
  deferred->BindExit(&answer);
  frame_->Push(&answer);
}


class DeferredInlineSmiOperation: public DeferredCode {
 public:
  DeferredInlineSmiOperation(CodeGenerator* generator,
                             Token::Value op,
                             Smi* value,
                             OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        op_(op),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiOperation");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiOperation::Generate() {
  Result left(generator());
  enter()->Bind(&left);
  generator()->frame()->Push(&left);
  generator()->frame()->Push(value_);
  GenericBinaryOpStub igostub(op_, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
}


class DeferredInlineSmiOperationReversed: public DeferredCode {
 public:
  DeferredInlineSmiOperationReversed(CodeGenerator* generator,
                                     Token::Value op,
                                     Smi* value,
                                     OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        op_(op),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiOperationReversed");
  }

  virtual void Generate();

 private:
  Token::Value op_;
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiOperationReversed::Generate() {
  Result right(generator());
  enter()->Bind(&right);
  generator()->frame()->Push(value_);
  generator()->frame()->Push(&right);
  GenericBinaryOpStub igostub(op_, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
}


class DeferredInlineSmiAdd: public DeferredCode {
 public:
  DeferredInlineSmiAdd(CodeGenerator* generator,
                       Smi* value,
                       OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiAdd");
  }

  virtual void Generate();

 private:
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiAdd::Generate() {
  // Undo the optimistic add operation and call the shared stub.
  Result left(generator());  // Initially left + value_.
  enter()->Bind(&left);
  left.ToRegister();
  generator()->frame()->Spill(left.reg());
  __ sub(Operand(left.reg()), Immediate(value_));
  generator()->frame()->Push(&left);
  generator()->frame()->Push(value_);
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
}


class DeferredInlineSmiAddReversed: public DeferredCode {
 public:
  DeferredInlineSmiAddReversed(CodeGenerator* generator,
                               Smi* value,
                               OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiAddReversed");
  }

  virtual void Generate();

 private:
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiAddReversed::Generate() {
  // Undo the optimistic add operation and call the shared stub.
  Result right(generator());  // Initially value_ + right.
  enter()->Bind(&right);
  right.ToRegister();
  generator()->frame()->Spill(right.reg());
  __ sub(Operand(right.reg()), Immediate(value_));
  generator()->frame()->Push(value_);
  generator()->frame()->Push(&right);
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
}


class DeferredInlineSmiSub: public DeferredCode {
 public:
  DeferredInlineSmiSub(CodeGenerator* generator,
                       Smi* value,
                       OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiSub");
  }

  virtual void Generate();

 private:
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiSub::Generate() {
  // Undo the optimistic sub operation and call the shared stub.
  Result left(generator());  // Initially left - value_.
  enter()->Bind(&left);
  left.ToRegister();
  generator()->frame()->Spill(left.reg());
  __ add(Operand(left.reg()), Immediate(value_));
  generator()->frame()->Push(&left);
  generator()->frame()->Push(value_);
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
}


class DeferredInlineSmiSubReversed: public DeferredCode {
 public:
  DeferredInlineSmiSubReversed(CodeGenerator* generator,
                               Smi* value,
                               OverwriteMode overwrite_mode)
      : DeferredCode(generator),
        value_(value),
        overwrite_mode_(overwrite_mode) {
    set_comment("[ DeferredInlineSmiSubReversed");
  }

  virtual void Generate();

 private:
  Smi* value_;
  OverwriteMode overwrite_mode_;
};


void DeferredInlineSmiSubReversed::Generate() {
  // Call the shared stub.
  Result right(generator());
  enter()->Bind(&right);
  generator()->frame()->Push(value_);
  generator()->frame()->Push(&right);
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, SMI_CODE_INLINED);
  Result answer = generator()->frame()->CallStub(&igostub, 2);
  exit_.Jump(&answer);
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
    Result unsafe_operand(value, this);
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
      DeferredCode* deferred = NULL;
      if (reversed) {
        deferred = new DeferredInlineSmiAddReversed(this, smi_value,
                                                    overwrite_mode);
      } else {
        deferred = new DeferredInlineSmiAdd(this, smi_value, overwrite_mode);
      }
      operand->ToRegister();
      frame_->Spill(operand->reg());
      __ add(Operand(operand->reg()), Immediate(value));
      deferred->enter()->Branch(overflow, operand, not_taken);
      __ test(operand->reg(), Immediate(kSmiTagMask));
      deferred->enter()->Branch(not_zero, operand, not_taken);
      deferred->BindExit(operand);
      frame_->Push(operand);
      break;
    }

    case Token::SUB: {
      DeferredCode* deferred = NULL;
      Result answer(this);  // Only allocate a new register if reversed.
      if (reversed) {
        answer = allocator()->Allocate();
        ASSERT(answer.is_valid());
        deferred = new DeferredInlineSmiSubReversed(this,
                                                    smi_value,
                                                    overwrite_mode);
        __ Set(answer.reg(), Immediate(value));
        if (operand->is_register()) {
          __ sub(answer.reg(), Operand(operand->reg()));
        } else {
          ASSERT(operand->is_constant());
          __ sub(Operand(answer.reg()), Immediate(operand->handle()));
        }
      } else {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        deferred = new DeferredInlineSmiSub(this,
                                            smi_value,
                                            overwrite_mode);
        __ sub(Operand(operand->reg()), Immediate(value));
        answer = *operand;
      }
      deferred->enter()->Branch(overflow, operand, not_taken);
      __ test(answer.reg(), Immediate(kSmiTagMask));
      deferred->enter()->Branch(not_zero, operand, not_taken);
      operand->Unuse();
      deferred->BindExit(&answer);
      frame_->Push(&answer);
      break;
    }

    case Token::SAR: {
      if (reversed) {
        Result constant_operand(value, this);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        DeferredCode* deferred =
            new DeferredInlineSmiOperation(this, Token::SAR, smi_value,
                                           overwrite_mode);
        operand->ToRegister();
        __ test(operand->reg(), Immediate(kSmiTagMask));
        deferred->enter()->Branch(not_zero, operand, not_taken);
        if (shift_value > 0) {
          frame_->Spill(operand->reg());
          __ sar(operand->reg(), shift_value);
          __ and_(operand->reg(), ~kSmiTagMask);
        }
        deferred->BindExit(operand);
        frame_->Push(operand);
      }
      break;
    }

    case Token::SHR: {
      if (reversed) {
        Result constant_operand(value, this);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        DeferredCode* deferred =
            new DeferredInlineSmiOperation(this, Token::SHR, smi_value,
                                           overwrite_mode);
        operand->ToRegister();
        __ test(operand->reg(), Immediate(kSmiTagMask));
        deferred->enter()->Branch(not_zero, operand, not_taken);
        Result answer = allocator()->Allocate();
        ASSERT(answer.is_valid());
        __ mov(answer.reg(), operand->reg());
        __ sar(answer.reg(), kSmiTagSize);
        __ shr(answer.reg(), shift_value);
        // A negative Smi shifted right two is in the positive Smi range.
        if (shift_value < 2) {
          __ test(answer.reg(), Immediate(0xc0000000));
          deferred->enter()->Branch(not_zero, operand, not_taken);
        }
        operand->Unuse();
        ASSERT(kSmiTagSize == times_2);  // Adjust the code if not true.
        __ lea(answer.reg(),
               Operand(answer.reg(), answer.reg(), times_1, kSmiTag));
        deferred->BindExit(&answer);
        frame_->Push(&answer);
      }
      break;
    }

    case Token::SHL: {
      if (reversed) {
        Result constant_operand(value, this);
        LikelySmiBinaryOperation(op, &constant_operand, operand,
                                 overwrite_mode);
      } else {
        // Only the least significant 5 bits of the shift value are used.
        // In the slow case, this masking is done inside the runtime call.
        int shift_value = int_value & 0x1f;
        DeferredCode* deferred =
            new DeferredInlineSmiOperation(this, Token::SHL, smi_value,
                                           overwrite_mode);
        operand->ToRegister();
        __ test(operand->reg(), Immediate(kSmiTagMask));
        deferred->enter()->Branch(not_zero, operand, not_taken);
        Result answer = allocator()->Allocate();
        ASSERT(answer.is_valid());
        __ mov(answer.reg(), operand->reg());
        ASSERT(kSmiTag == 0);  // adjust code if not the case
        // We do no shifts, only the Smi conversion, if shift_value is 1.
        if (shift_value == 0) {
          __ sar(answer.reg(), kSmiTagSize);
        } else if (shift_value > 1) {
          __ shl(answer.reg(), shift_value - 1);
        }
        // Convert int result to Smi, checking that it is in int range.
        ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
        __ add(answer.reg(), Operand(answer.reg()));
        deferred->enter()->Branch(overflow, operand, not_taken);
        operand->Unuse();
        deferred->BindExit(&answer);
        frame_->Push(&answer);
      }
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND: {
      DeferredCode* deferred = NULL;
      if (reversed) {
        deferred = new DeferredInlineSmiOperationReversed(this, op, smi_value,
                                                          overwrite_mode);
      } else {
        deferred =  new DeferredInlineSmiOperation(this, op, smi_value,
                                                   overwrite_mode);
      }
      operand->ToRegister();
      __ test(operand->reg(), Immediate(kSmiTagMask));
      deferred->enter()->Branch(not_zero, operand, not_taken);
      frame_->Spill(operand->reg());
      if (op == Token::BIT_AND) {
        if (int_value == 0) {
          __ xor_(Operand(operand->reg()), operand->reg());
        } else {
          __ and_(Operand(operand->reg()), Immediate(value));
        }
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
      deferred->BindExit(operand);
      frame_->Push(operand);
      break;
    }

    default: {
      Result constant_operand(value, this);
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


class CompareStub: public CodeStub {
 public:
  CompareStub(Condition cc, bool strict) : cc_(cc), strict_(strict) { }

  void Generate(MacroAssembler* masm);

 private:
  Condition cc_;
  bool strict_;

  Major MajorKey() { return Compare; }

  int MinorKey() {
    // Encode the three parameters in a unique 16 bit value.
    ASSERT(static_cast<int>(cc_) < (1 << 15));
    return (static_cast<int>(cc_) << 1) | (strict_ ? 1 : 0);
  }

#ifdef DEBUG
  void Print() {
    PrintF("CompareStub (cc %d), (strict %s)\n",
           static_cast<int>(cc_),
           strict_ ? "true" : "false");
  }
#endif
};


void CodeGenerator::Comparison(Condition cc,
                               bool strict,
                               ControlDestination* dest) {
  // Strict only makes sense for equality comparisons.
  ASSERT(!strict || cc == equal);

  Result left_side(this);
  Result right_side(this);
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
      ASSERT(left_side.is_valid());
      JumpTarget is_smi(this);
      __ test(left_side.reg(), Immediate(kSmiTagMask));
      is_smi.Branch(zero, &left_side, &right_side, taken);

      // Setup and call the compare stub, which expects its arguments
      // in registers.
      CompareStub stub(cc, strict);
      Result result = frame_->CallStub(&stub, &left_side, &right_side);
      result.ToRegister();
      __ cmp(result.reg(), 0);
      result.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_smi.Bind(&left_side, &right_side);
      left_side.ToRegister();
      // Test smi equality and comparison by signed int comparison.
      if (IsUnsafeSmi(right_side.handle())) {
        right_side.ToRegister();
        ASSERT(right_side.is_valid());
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
    JumpTarget is_smi(this);
    if (!known_non_smi) {
      // Check for the smi case.
      Result temp = allocator_->Allocate();
      ASSERT(temp.is_valid());
      __ mov(temp.reg(), left_side.reg());
      __ or_(temp.reg(), Operand(right_side.reg()));
      __ test(temp.reg(), Immediate(kSmiTagMask));
      temp.Unuse();
      is_smi.Branch(zero, &left_side, &right_side, taken);
    }
    // When non-smi, call out to the compare stub, which expects its
    // arguments in registers.
    CompareStub stub(cc, strict);
    Result answer = frame_->CallStub(&stub, &left_side, &right_side);
    if (cc == equal) {
      __ test(answer.reg(), Operand(answer.reg()));
    } else {
      __ cmp(answer.reg(), 0);
    }
    answer.Unuse();
    if (known_non_smi) {
      dest->Split(cc);
    } else {
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();
      is_smi.Bind(&left_side, &right_side);
      left_side.ToRegister();
      right_side.ToRegister();
      __ cmp(left_side.reg(), Operand(right_side.reg()));
      right_side.Unuse();
      left_side.Unuse();
      dest->Split(cc);
    }
  }
}


class CallFunctionStub: public CodeStub {
 public:
  explicit CallFunctionStub(int argc) : argc_(argc) { }

  void Generate(MacroAssembler* masm);

 private:
  int argc_;

#ifdef DEBUG
  void Print() { PrintF("CallFunctionStub (args %d)\n", argc_); }
#endif

  Major MajorKey() { return CallFunction; }
  int MinorKey() { return argc_; }
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
  CallFunctionStub call_function(arg_count);
  Result answer = frame_->CallStub(&call_function, arg_count + 1);
  // Restore context and replace function on the stack with the
  // result of the stub invocation.
  frame_->RestoreContextRegister();
  frame_->SetElementAt(0, &answer);
}


class DeferredStackCheck: public DeferredCode {
 public:
  explicit DeferredStackCheck(CodeGenerator* generator)
      : DeferredCode(generator) {
    set_comment("[ DeferredStackCheck");
  }

  virtual void Generate();
};


void DeferredStackCheck::Generate() {
  enter()->Bind();
  StackCheckStub stub;
  Result ignored = generator()->frame()->CallStub(&stub, 0);
  ignored.Unuse();
  exit_.Jump();
}


void CodeGenerator::CheckStack() {
  if (FLAG_check_stack) {
    DeferredStackCheck* deferred = new DeferredStackCheck(this);
    ExternalReference stack_guard_limit =
        ExternalReference::address_of_stack_guard_limit();
    __ cmp(esp, Operand::StaticVariable(stack_guard_limit));
    deferred->enter()->Branch(below, not_taken);
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
  node->break_target()->Initialize(this);
  VisitStatements(node->statements());
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }
  node->break_target()->Unuse();
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  frame_->Push(pairs);

  // Duplicate the context register.
  Result context(esi, this);
  frame_->Push(&context);

  frame_->Push(Smi::FromInt(is_eval() ? 1 : 0));
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
    // For now, just do a runtime call.  Duplicate the context register.
    Result context(esi, this);
    frame_->Push(&context);
    frame_->Push(var->name());
    // Declaration nodes are always introduced in one of two modes.
    ASSERT(node->mode() == Variable::VAR || node->mode() == Variable::CONST);
    PropertyAttributes attr = node->mode() == Variable::VAR ? NONE : READ_ONLY;
    frame_->Push(Smi::FromInt(attr));
    // Push initial value, if any.
    // Note: For variables we must not push an initial value (such as
    // 'undefined') because we may have a (legal) redeclaration and we
    // must not destroy the current value.
    if (node->mode() == Variable::CONST) {
      frame_->Push(Factory::the_hole_value());
    } else if (node->fun() != NULL) {
      Load(node->fun());
    } else {
      frame_->Push(Smi::FromInt(0));  // no initial value!
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
  JumpTarget exit(this);
  if (has_then_stm && has_else_stm) {
    JumpTarget then(this);
    JumpTarget else_(this);
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
    JumpTarget then(this);
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
    JumpTarget else_(this);
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
      // Though this is a (possibly) backward block, the frames can
      // only differ on their top element.
      function_return_.Bind(&return_value, 1);
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
  __ bind(&check_exit_codesize);

  // Leave the frame and return popping the arguments and the
  // receiver.
  frame_->Exit();
  __ ret((scope_->num_parameters() + 1) * kPointerSize);
  DeleteFrame();

  // Check that the size of the code used for returning matches what is
  // expected by the debugger.
  ASSERT_EQ(Debug::kIa32JSReturnSequenceLength,
            __ SizeOfCodeGeneratedSince(&check_exit_codesize));
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ WithEnterStatement");
  CodeForStatementPosition(node);
  Load(node->expression());
  Result context(this);
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


int CodeGenerator::FastCaseSwitchMaxOverheadFactor() {
    return kFastSwitchMaxOverheadFactor;
}


int CodeGenerator::FastCaseSwitchMinCaseCount() {
    return kFastSwitchMinCaseCount;
}


// Generate a computed jump to a switch case.
void CodeGenerator::GenerateFastCaseSwitchJumpTable(
    SwitchStatement* node,
    int min_index,
    int range,
    Label* default_label,
    Vector<Label*> case_targets,
    Vector<Label> case_labels) {
  // Notice: Internal references, used by both the jmp instruction and
  // the table entries, need to be relocated if the buffer grows. This
  // prevents the forward use of Labels, since a displacement cannot
  // survive relocation, and it also cannot safely be distinguished
  // from a real address.  Instead we put in zero-values as
  // placeholders, and fill in the addresses after the labels have been
  // bound.

  JumpTarget setup_default(this);
  JumpTarget is_smi(this);

  // A non-null default label pointer indicates a default case among
  // the case labels.  Otherwise we use the break target as a
  // "default".
  JumpTarget* default_target =
      (default_label == NULL) ? node->break_target() : &setup_default;

  // Test whether input is a smi.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  Result switch_value = frame_->Pop();
  switch_value.ToRegister();
  __ test(switch_value.reg(), Immediate(kSmiTagMask));
  is_smi.Branch(equal, &switch_value, taken);

  // It's a heap object, not a smi or a failure.  Check if it is a
  // heap number.
  Result temp = allocator()->Allocate();
  ASSERT(temp.is_valid());
  __ CmpObjectType(switch_value.reg(), HEAP_NUMBER_TYPE, temp.reg());
  temp.Unuse();
  default_target->Branch(not_equal);

  // The switch value is a heap number.  Convert it to a smi.
  frame_->Push(&switch_value);
  Result smi_value = frame_->CallRuntime(Runtime::kNumberToSmi, 1);

  is_smi.Bind(&smi_value);
  smi_value.ToRegister();
  // Convert the switch value to a 0-based table index.
  if (min_index != 0) {
    frame_->Spill(smi_value.reg());
    __ sub(Operand(smi_value.reg()), Immediate(min_index << kSmiTagSize));
  }
  // Go to the default case if the table index is negative or not a smi.
  __ test(smi_value.reg(), Immediate(0x80000000 | kSmiTagMask));
  default_target->Branch(not_equal, not_taken);
  __ cmp(smi_value.reg(), range << kSmiTagSize);
  default_target->Branch(greater_equal, not_taken);

  // The expected frame at all the case labels is a version of the
  // current one (the bidirectional entry frame, which an arbitrary
  // frame of the correct height can be merged to).  Keep a copy to
  // restore at the start of every label.  Create a jump target and
  // bind it to set its entry frame properly.
  JumpTarget entry_target(this, JumpTarget::BIDIRECTIONAL);
  entry_target.Bind(&smi_value);
  VirtualFrame* start_frame = new VirtualFrame(frame_);

  // 0 is placeholder.
  // Jump to the address at table_address + 2 * smi_value.reg().
  // The target of the jump is read from table_address + 4 * switch_value.
  // The Smi encoding of smi_value.reg() is 2 * switch_value.
  smi_value.ToRegister();
  __ jmp(Operand(smi_value.reg(), smi_value.reg(),
                 times_1, 0x0, RelocInfo::INTERNAL_REFERENCE));
  smi_value.Unuse();
  // Calculate address to overwrite later with actual address of table.
  int32_t jump_table_ref = __ pc_offset() - sizeof(int32_t);
  __ Align(4);
  Label table_start;
  __ bind(&table_start);
  __ WriteInternalReference(jump_table_ref, table_start);

  for (int i = 0; i < range; i++) {
    // These are the table entries. 0x0 is the placeholder for case address.
    __ dd(0x0, RelocInfo::INTERNAL_REFERENCE);
  }

  GenerateFastCaseSwitchCases(node, case_labels, start_frame);

  // If there was a default case, we need to emit the code to match it.
  if (default_label != NULL) {
    if (has_valid_frame()) {
      node->break_target()->Jump();
    }
    setup_default.Bind();
    frame_->MergeTo(start_frame);
    __ jmp(default_label);
    DeleteFrame();
  }
  if (node->break_target()->is_linked()) {
    node->break_target()->Bind();
  }

  for (int i = 0, entry_pos = table_start.pos();
       i < range;
       i++, entry_pos += sizeof(uint32_t)) {
    if (case_targets[i] == NULL) {
      __ WriteInternalReference(entry_pos,
                                *node->break_target()->entry_label());
    } else {
      __ WriteInternalReference(entry_pos, *case_targets[i]);
    }
  }

  delete start_frame;
}


void CodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
  ASSERT(!in_spilled_code());
  Comment cmnt(masm_, "[ SwitchStatement");
  CodeForStatementPosition(node);
  node->break_target()->Initialize(this);

  // Compile the switch value.
  Load(node->tag());

  if (TryGenerateFastCaseSwitchStatement(node)) {
    return;
  }

  ZoneList<CaseClause*>* cases = node->cases();
  int length = cases->length();
  CaseClause* default_clause = NULL;

  JumpTarget next_test(this);
  // Compile the case label expressions and comparisons.  Exit early
  // if a comparison is unconditionally true.  The target next_test is
  // bound before the loop in order to indicate control flow to the
  // first comparison.
  next_test.Bind();
  for (int i = 0; i < length && !next_test.is_unused(); i++) {
    CaseClause* clause = cases->at(i);
    clause->body_target()->Initialize(this);
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
            JumpTarget body(this);
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
  node->break_target()->Initialize(this);

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
      JumpTarget body(this, JumpTarget::BIDIRECTIONAL);
      IncrementLoopNesting();

      // Label the top of the loop for the backward jump if necessary.
      if (info == ALWAYS_TRUE) {
        // Use the continue target.
        node->continue_target()->Initialize(this, JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else if (info == ALWAYS_FALSE) {
        // No need to label it.
        node->continue_target()->Initialize(this);
      } else {
        // Continue is the test, so use the backward body target.
        ASSERT(info == DONT_KNOW);
        node->continue_target()->Initialize(this);
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
        body.Initialize(this, JumpTarget::BIDIRECTIONAL);
      } else {
        body.Initialize(this);
      }

      // Based on the condition analysis, compile the test as necessary.
      if (info == ALWAYS_TRUE) {
        // We will not compile the test expression.  Label the top of
        // the loop with the continue target.
        node->continue_target()->Initialize(this, JumpTarget::BIDIRECTIONAL);
        node->continue_target()->Bind();
      } else {
        ASSERT(info == DONT_KNOW);  // ALWAYS_FALSE cannot reach here.
        if (test_at_bottom) {
          // Continue is the test at the bottom, no need to label the
          // test at the top.  The body is a backward target.
          node->continue_target()->Initialize(this);
        } else {
          // Label the test at the top as the continue target.  The
          // body is a forward-only target.
          node->continue_target()->Initialize(this, JumpTarget::BIDIRECTIONAL);
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
      JumpTarget loop(this, JumpTarget::BIDIRECTIONAL);

      // Target for backward edge if there is a test at the bottom,
      // otherwise used as target for test at the top.
      JumpTarget body;
      if (test_at_bottom) {
        body.Initialize(this, JumpTarget::BIDIRECTIONAL);
      } else {
        body.Initialize(this);
      }

      // Based on the condition analysis, compile the test as necessary.
      if (info == ALWAYS_TRUE) {
        // We will not compile the test expression.  Label the top of
        // the loop.
        if (node->next() == NULL) {
          // Use the continue target if there is no update expression.
          node->continue_target()->Initialize(this, JumpTarget::BIDIRECTIONAL);
          node->continue_target()->Bind();
        } else {
          // Otherwise use the backward loop target.
          node->continue_target()->Initialize(this);
          loop.Bind();
        }
      } else {
        ASSERT(info == DONT_KNOW);
        if (test_at_bottom) {
          // Continue is either the update expression or the test at
          // the bottom, no need to label the test at the top.
          node->continue_target()->Initialize(this);
        } else if (node->next() == NULL) {
          // We are not recompiling the test at the bottom and there
          // is no update expression.
          node->continue_target()->Initialize(this, JumpTarget::BIDIRECTIONAL);
          node->continue_target()->Bind();
        } else {
          // We are not recompiling the test at the bottom and there
          // is an update expression.
          node->continue_target()->Initialize(this);
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
  VirtualFrame::SpilledScope spilled_scope(this);
  Comment cmnt(masm_, "[ ForInStatement");
  CodeForStatementPosition(node);

  JumpTarget primitive(this);
  JumpTarget jsobject(this);
  JumpTarget fixed_array(this);
  JumpTarget entry(this, JumpTarget::BIDIRECTIONAL);
  JumpTarget end_del_check(this);
  JumpTarget exit(this);

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
  node->break_target()->Initialize(this);
  node->continue_target()->Initialize(this);

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
  VirtualFrame::SpilledScope spilled_scope(this);
  Comment cmnt(masm_, "[ TryCatch");
  CodeForStatementPosition(node);

  JumpTarget try_block(this);
  JumpTarget exit(this);

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
    __ lea(eax, Operand(eax, StackHandlerConstants::kAddressDisplacement));
    __ cmp(esp, Operand(eax));
    __ Assert(equal, "stack pointer should point to top handler");
  }

  // If we can fall off the end of the try block, unlink from try chain.
  if (has_valid_frame()) {
    // The next handler address is on top of the frame.  Unlink from
    // the handler list and drop the rest of this handler from the
    // frame.
    frame_->EmitPop(Operand::StaticVariable(handler_address));
    frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);
    if (has_unlinks) {
      exit.Jump();
    }
  }

  // Generate unlink code for the (formerly) shadowing targets that
  // have been jumped to.  Deallocate each shadow target.
  Result return_value(this);
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
      __ mov(edx, Operand::StaticVariable(handler_address));
      const int kNextOffset = StackHandlerConstants::kNextOffset +
          StackHandlerConstants::kAddressDisplacement;
      __ lea(esp, Operand(edx, kNextOffset));
      frame_->Forget(frame_->height() - handler_height);

      frame_->EmitPop(Operand::StaticVariable(handler_address));
      frame_->Drop(StackHandlerConstants::kSize / kPointerSize - 1);
      // next_sp popped.

      if (i == kReturnShadowIndex) {
        if (!function_return_is_shadowed_) frame_->PrepareForReturn();
        shadows[i]->other_target()->Jump(&return_value);
      } else {
        shadows[i]->other_target()->Jump();
      }
    }
    delete shadows[i];
  }

  exit.Bind();
}


void CodeGenerator::VisitTryFinally(TryFinally* node) {
  ASSERT(!in_spilled_code());
  VirtualFrame::SpilledScope spilled_scope(this);
  Comment cmnt(masm_, "[ TryFinally");
  CodeForStatementPosition(node);

  // State: Used to keep track of reason for entering the finally
  // block. Should probably be extended to hold information for
  // break/continue from within the try block.
  enum { FALLING, THROWING, JUMPING };

  JumpTarget try_block(this);
  JumpTarget finally_block(this);

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
    frame_->EmitPop(eax);
    __ mov(Operand::StaticVariable(handler_address), eax);
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
        Result return_value(this);
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
      __ mov(edx, Operand::StaticVariable(handler_address));
      const int kNextOffset = StackHandlerConstants::kNextOffset +
          StackHandlerConstants::kAddressDisplacement;
      __ lea(esp, Operand(edx, kNextOffset));
      frame_->Forget(frame_->height() - handler_height);

      // Unlink this handler and drop it from the frame.
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
          JumpTarget skip(this);
          skip.Branch(not_equal);
          frame_->PrepareForReturn();
          original->Jump(&return_value);
          skip.Bind();
        }
      } else {
        original->Branch(equal);
      }
    }
    delete shadows[i];
  }

  if (has_valid_frame()) {
    // Check if we need to rethrow the exception.
    JumpTarget exit(this);
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
  // Spill everything, even constants, to the frame.
  frame_->SpillAll();
  frame_->CallRuntime(Runtime::kDebugBreak, 0);
  // Ignore the return value.
}


void CodeGenerator::InstantiateBoilerplate(Handle<JSFunction> boilerplate) {
  ASSERT(boilerplate->IsBoilerplate());

  // Push the boilerplate on the stack.
  frame_->Push(boilerplate);

  // Create a new closure.
  frame_->Push(esi);
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
  JumpTarget then(this);
  JumpTarget else_(this);
  JumpTarget exit(this);
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

    JumpTarget slow(this);
    JumpTarget done(this);
    Result value(this);

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
    frame_->Push(esi);
    frame_->Push(slot->var()->name());
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
    VirtualFrame::SpilledScope spilled_scope(this);
    Comment cmnt(masm_, "[ Load const");
    JumpTarget exit(this);
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


Result CodeGenerator::LoadFromGlobalSlotCheckExtensions(
    Slot* slot,
    TypeofState typeof_state,
    JumpTarget* slow) {
  // Check that no extension objects have been created by calls to
  // eval from the current scope to the global scope.
  Result context(esi, this);
  Result tmp = allocator_->Allocate();
  ASSERT(tmp.is_valid());  // All non-reserved registers were available.

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context.reg(), Context::EXTENSION_INDEX),
               Immediate(0));
        slow->Branch(not_equal, not_taken);
      }
      // Load next context in chain.
      __ mov(tmp.reg(), ContextOperand(context.reg(), Context::CLOSURE_INDEX));
      __ mov(tmp.reg(), FieldOperand(tmp.reg(), JSFunction::kContextOffset));
      context = tmp;
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
    if (!context.reg().is(tmp.reg())) __ mov(tmp.reg(), context.reg());
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
  context.Unuse();
  tmp.Unuse();

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  LoadGlobal();
  frame_->Push(slot->var()->name());
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
                         ? RelocInfo::CODE_TARGET
                         : RelocInfo::CODE_TARGET_CONTEXT;
  Result answer = frame_->CallLoadIC(mode);

  // Discard the global object. The result is in answer.
  frame_->Drop();
  return answer;
}


void CodeGenerator::StoreToSlot(Slot* slot, InitState init_state) {
  if (slot->type() == Slot::LOOKUP) {
    ASSERT(slot->var()->is_dynamic());

    // For now, just do a runtime call.
    frame_->Push(esi);
    frame_->Push(slot->var()->name());

    Result value(this);
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

    JumpTarget exit(this);
    if (init_state == CONST_INIT) {
      ASSERT(slot->var()->mode() == Variable::CONST);
      // Only the first const initialization must be executed (the slot
      // still contains 'the hole' value). When the assignment is executed,
      // the code is identical to a normal store (see below).
      //
      // We spill the frame in the code below because the direct-frame
      // access of SlotOperand is potentially unsafe with an unspilled
      // frame.
      VirtualFrame::SpilledScope spilled_scope(this);
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
  LoadFromSlot(node, typeof_state());
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


class DeferredRegExpLiteral: public DeferredCode {
 public:
  DeferredRegExpLiteral(CodeGenerator* generator, RegExpLiteral* node)
      : DeferredCode(generator), node_(node) {
    set_comment("[ DeferredRegExpLiteral");
  }

  virtual void Generate();

 private:
  RegExpLiteral* node_;
};


void DeferredRegExpLiteral::Generate() {
  Result literals(generator());
  enter()->Bind(&literals);
  // Since the entry is undefined we call the runtime system to
  // compute the literal.

  VirtualFrame* frame = generator()->frame();
  // Literal array (0).
  frame->Push(&literals);
  // Literal index (1).
  frame->Push(Smi::FromInt(node_->literal_index()));
  // RegExp pattern (2).
  frame->Push(node_->pattern());
  // RegExp flags (3).
  frame->Push(node_->flags());
  Result boilerplate =
      frame->CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  exit_.Jump(&boilerplate);
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
  Comment cmnt(masm_, "[ RegExp Literal");
  DeferredRegExpLiteral* deferred = new DeferredRegExpLiteral(this, node);

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
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  Result boilerplate = allocator_->Allocate();
  ASSERT(boilerplate.is_valid());
  __ mov(boilerplate.reg(), FieldOperand(literals.reg(), literal_offset));

  // Check whether we need to materialize the RegExp object.  If so,
  // jump to the deferred code passing the literals array.
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->enter()->Branch(equal, &literals, not_taken);

  literals.Unuse();
  // The deferred code returns the boilerplate object.
  deferred->BindExit(&boilerplate);

  // Push the boilerplate object.
  frame_->Push(&boilerplate);
}


// This deferred code stub will be used for creating the boilerplate
// by calling Runtime_CreateObjectLiteral.
// Each created boilerplate is stored in the JSFunction and they are
// therefore context dependent.
class DeferredObjectLiteral: public DeferredCode {
 public:
  DeferredObjectLiteral(CodeGenerator* generator,
                        ObjectLiteral* node)
      : DeferredCode(generator), node_(node) {
    set_comment("[ DeferredObjectLiteral");
  }

  virtual void Generate();

 private:
  ObjectLiteral* node_;
};


void DeferredObjectLiteral::Generate() {
  Result literals(generator());
  enter()->Bind(&literals);
  // Since the entry is undefined we call the runtime system to
  // compute the literal.

  VirtualFrame* frame = generator()->frame();
  // Literal array (0).
  frame->Push(&literals);
  // Literal index (1).
  frame->Push(Smi::FromInt(node_->literal_index()));
  // Constant properties (2).
  frame->Push(node_->constant_properties());
  Result boilerplate =
      frame->CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  exit_.Jump(&boilerplate);
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  DeferredObjectLiteral* deferred = new DeferredObjectLiteral(this, node);

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
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  Result boilerplate = allocator_->Allocate();
  ASSERT(boilerplate.is_valid());
  __ mov(boilerplate.reg(), FieldOperand(literals.reg(), literal_offset));

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code passing the literals array.
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->enter()->Branch(equal, &literals, not_taken);

  literals.Unuse();
  // The deferred code returns the boilerplate object.
  deferred->BindExit(&boilerplate);

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


// This deferred code stub will be used for creating the boilerplate
// by calling Runtime_CreateArrayLiteralBoilerplate.
// Each created boilerplate is stored in the JSFunction and they are
// therefore context dependent.
class DeferredArrayLiteral: public DeferredCode {
 public:
  DeferredArrayLiteral(CodeGenerator* generator,
                       ArrayLiteral* node)
      : DeferredCode(generator), node_(node) {
    set_comment("[ DeferredArrayLiteral");
  }

  virtual void Generate();

 private:
  ArrayLiteral* node_;
};


void DeferredArrayLiteral::Generate() {
  Result literals(generator());
  enter()->Bind(&literals);
  // Since the entry is undefined we call the runtime system to
  // compute the literal.

  VirtualFrame* frame = generator()->frame();
  // Literal array (0).
  frame->Push(&literals);
  // Literal index (1).
  frame->Push(Smi::FromInt(node_->literal_index()));
  // Constant properties (2).
  frame->Push(node_->literals());
  Result boilerplate =
      frame->CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);
  exit_.Jump(&boilerplate);
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  Comment cmnt(masm_, "[ ArrayLiteral");
  DeferredArrayLiteral* deferred = new DeferredArrayLiteral(this, node);

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
  int literal_offset =
      FixedArray::kHeaderSize + node->literal_index() * kPointerSize;
  Result boilerplate = allocator_->Allocate();
  ASSERT(boilerplate.is_valid());
  __ mov(boilerplate.reg(), FieldOperand(literals.reg(), literal_offset));

  // Check whether we need to materialize the object literal boilerplate.
  // If so, jump to the deferred code passing the literals array.
  __ cmp(boilerplate.reg(), Factory::undefined_value());
  deferred->enter()->Branch(equal, &literals, not_taken);

  literals.Unuse();
  // The deferred code returns the boilerplate object.
  deferred->BindExit(&boilerplate);

  // Push the resulting array literal on the stack.
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
    int offset = i * kPointerSize + Array::kHeaderSize;
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
      GenericBinaryOperation(node->binary_op(), node->type());
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

    // Load the function
    frame_->Push(esi);
    frame_->Push(var->name());
    frame_->CallRuntime(Runtime::kLoadContextSlot, 2);
    // eax: slot value; edx: receiver

    // Load the receiver.
    frame_->Push(eax);
    frame_->Push(edx);

    // Call the function.
    CallWithArguments(args, node->position());

  } else if (property != NULL) {
    // Check if the key is a literal string.
    Literal* literal = property->key()->AsLiteral();

    if (literal != NULL && literal->handle()->IsSymbol()) {
      // ------------------------------------------------------------------
      // JavaScript example: 'object.foo(1, 2, 3)' or 'map["key"](1, 2, 3)'
      // ------------------------------------------------------------------

      // Push the name of the function and the receiver onto the stack.
      frame_->Push(literal->handle());
      Load(property->obj());

      // Load the arguments.
      int arg_count = args->length();
      for (int i = 0; i < arg_count; i++) {
        Load(args->at(i));
      }

      // Call the IC initialization code.
      CodeForSourcePosition(node->position());
      Result result =
          frame_->CallCallIC(RelocInfo::CODE_TARGET, arg_count, loop_nesting());
      frame_->RestoreContextRegister();
      // Replace the function on the stack with the result.
      frame_->SetElementAt(0, &result);

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
  CallFunctionStub call_function(arg_count);
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
  ASSERT(args->length() == 2);

  JumpTarget slow_case(this);
  JumpTarget end(this);
  JumpTarget not_a_flat_string(this);
  JumpTarget a_cons_string(this);
  JumpTarget try_again_with_new_string(this, JumpTarget::BIDIRECTIONAL);
  JumpTarget ascii_string(this);
  JumpTarget got_char_code(this);

  Load(args->at(0));
  Load(args->at(1));
  // Reserve register ecx, to use as shift amount later
  Result shift_amount = allocator()->Allocate(ecx);
  ASSERT(shift_amount.is_valid());
  Result index = frame_->Pop();
  index.ToRegister();
  Result object = frame_->Pop();
  object.ToRegister();
  // If the receiver is a smi return undefined.
  ASSERT(kSmiTag == 0);
  __ test(object.reg(), Immediate(kSmiTagMask));
  slow_case.Branch(zero, not_taken);

  // Check for negative or non-smi index.
  ASSERT(kSmiTag == 0);
  __ test(index.reg(), Immediate(kSmiTagMask | 0x80000000));
  slow_case.Branch(not_zero, not_taken);
  // Get rid of the smi tag on the index.
  frame_->Spill(index.reg());
  __ sar(index.reg(), kSmiTagSize);

  try_again_with_new_string.Bind(&object, &index, &shift_amount);
  // Get the type of the heap object.
  Result object_type = allocator()->Allocate();
  ASSERT(object_type.is_valid());
  __ mov(object_type.reg(), FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzx_b(object_type.reg(),
             FieldOperand(object_type.reg(), Map::kInstanceTypeOffset));
  // We don't handle non-strings.
  __ test(object_type.reg(), Immediate(kIsNotStringMask));
  slow_case.Branch(not_zero, not_taken);

  // Here we make assumptions about the tag values and the shifts needed.
  // See the comment in objects.h.
  ASSERT(kLongStringTag == 0);
  ASSERT(kMediumStringTag + String::kLongLengthShift ==
         String::kMediumLengthShift);
  ASSERT(kShortStringTag + String::kLongLengthShift ==
         String::kShortLengthShift);
  __ mov(shift_amount.reg(), Operand(object_type.reg()));
  __ and_(shift_amount.reg(), kStringSizeMask);
  __ add(Operand(shift_amount.reg()), Immediate(String::kLongLengthShift));
  // Get the length field. Temporary register now used for length.
  Result length = object_type;
  __ mov(length.reg(), FieldOperand(object.reg(), String::kLengthOffset));
  __ shr(length.reg());  // shift_amount, in ecx, is implicit operand.
  // Check for index out of range.
  __ cmp(index.reg(), Operand(length.reg()));
  slow_case.Branch(greater_equal, not_taken);
  length.Unuse();
  // Load the object type into object_type again.
  // These two instructions are duplicated from above, to save a register.
  __ mov(object_type.reg(), FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzx_b(object_type.reg(),
             FieldOperand(object_type.reg(), Map::kInstanceTypeOffset));

  // We need special handling for non-flat strings.
  ASSERT(kSeqStringTag == 0);
  __ test(object_type.reg(), Immediate(kStringRepresentationMask));
  not_a_flat_string.Branch(not_zero, &object, &index, &object_type,
                           &shift_amount, not_taken);
  shift_amount.Unuse();
  // Check for 1-byte or 2-byte string.
  __ test(object_type.reg(), Immediate(kStringEncodingMask));
  ascii_string.Branch(not_zero, &object, &index, &object_type, taken);

  // 2-byte string.
  // Load the 2-byte character code.
  __ movzx_w(object_type.reg(), FieldOperand(object.reg(),
                                             index.reg(),
                                             times_2,
                                             SeqTwoByteString::kHeaderSize));
  object.Unuse();
  index.Unuse();
  got_char_code.Jump(&object_type);

  // ASCII string.
  ascii_string.Bind(&object, &index, &object_type);
  // Load the byte.
  __ movzx_b(object_type.reg(), FieldOperand(object.reg(),
                                             index.reg(),
                                             times_1,
                                             SeqAsciiString::kHeaderSize));
  object.Unuse();
  index.Unuse();
  got_char_code.Bind(&object_type);
  ASSERT(kSmiTag == 0);
  __ shl(object_type.reg(), kSmiTagSize);
  frame_->Push(&object_type);
  end.Jump();

  // Handle non-flat strings.
  not_a_flat_string.Bind(&object, &index, &object_type, &shift_amount);
  __ and_(object_type.reg(), kStringRepresentationMask);
  __ cmp(object_type.reg(), kConsStringTag);
  a_cons_string.Branch(equal, &object, &index, &shift_amount, taken);
  __ cmp(object_type.reg(), kSlicedStringTag);
  slow_case.Branch(not_equal, not_taken);
  object_type.Unuse();

  // SlicedString.
  // Add the offset to the index.
  __ add(index.reg(), FieldOperand(object.reg(), SlicedString::kStartOffset));
  slow_case.Branch(overflow);
  // Getting the underlying string is done by running the cons string code.

  // ConsString.
  a_cons_string.Bind(&object, &index, &shift_amount);
  // Get the first of the two strings.
  frame_->Spill(object.reg());
  // Both sliced and cons strings store their source string at the same place.
  ASSERT(SlicedString::kBufferOffset == ConsString::kFirstOffset);
  __ mov(object.reg(), FieldOperand(object.reg(), ConsString::kFirstOffset));
  try_again_with_new_string.Jump(&object, &index, &shift_amount);

  // No results live at this point.
  slow_case.Bind();
  frame_->Push(Factory::undefined_value());
  end.Bind();
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


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  // ArgumentsAccessStub takes the parameter count as an input argument
  // in register eax.  Create a constant result for it.
  Result count(Handle<Smi>(Smi::FromInt(scope_->num_parameters())), this);
  // Call the shared stub to get to the arguments.length.
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_LENGTH);
  Result result = frame_->CallStub(&stub, &count);
  frame_->Push(&result);
}


void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  JumpTarget leave(this);
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
  JumpTarget leave(this);
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
  Result count(Handle<Smi>(Smi::FromInt(scope_->num_parameters())), this);
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
    // Call the JS runtime function.  Pass 0 as the loop nesting depth
    // because we do not handle runtime calls specially in loops.
    Result answer = frame_->CallCallIC(RelocInfo::CODE_TARGET, arg_count, 0);
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
        // lookup the context holding the named variable
        frame_->Push(esi);
        frame_->Push(variable->name());
        Result context = frame_->CallRuntime(Runtime::kLookupContext, 2);
        frame_->Push(&context);
        frame_->Push(variable->name());
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
      case Token::NOT:
      case Token::DELETE:
      case Token::TYPEOF:
        UNREACHABLE();  // handled above
        break;

      case Token::SUB: {
        UnarySubStub stub;
        // TODO(1222589): remove dependency of TOS being cached inside stub
        Result operand = frame_->Pop();
        Result answer = frame_->CallStub(&stub, &operand);
        frame_->Push(&answer);
        break;
      }

      case Token::BIT_NOT: {
        // Smi check.
        JumpTarget smi_label(this);
        JumpTarget continue_label(this);
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
        JumpTarget continue_label(this);
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
        UNREACHABLE();
    }
  }
}


class DeferredCountOperation: public DeferredCode {
 public:
  DeferredCountOperation(CodeGenerator* generator,
                         bool is_postfix,
                         bool is_increment,
                         int target_size)
      : DeferredCode(generator),
        is_postfix_(is_postfix),
        is_increment_(is_increment),
        target_size_(target_size) {
    set_comment("[ DeferredCountOperation");
  }

  virtual void Generate();

 private:
  bool is_postfix_;
  bool is_increment_;
  int target_size_;
};


void DeferredCountOperation::Generate() {
  CodeGenerator* cgen = generator();
  Result value(cgen);
  enter()->Bind(&value);
  VirtualFrame* frame = cgen->frame();
  // Undo the optimistic smi operation.
  value.ToRegister();
  frame->Spill(value.reg());
  if (is_increment_) {
    __ sub(Operand(value.reg()), Immediate(Smi::FromInt(1)));
  } else {
    __ add(Operand(value.reg()), Immediate(Smi::FromInt(1)));
  }
  frame->Push(&value);
  value = frame->InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION, 1);
  frame->Push(&value);
  if (is_postfix_) {  // Fix up copy of old value with ToNumber(value).
    // This is only safe because VisitCountOperation makes this frame slot
    // beneath the reference a register, which is spilled at the above call.
    // We cannot safely write to constants or copies below the water line.
    frame->StoreToElementAt(target_size_ + 1);
  }
  frame->Push(Smi::FromInt(1));
  if (is_increment_) {
    value = frame->CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    value = frame->CallRuntime(Runtime::kNumberSub, 2);
  }
  exit_.Jump(&value);
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
  Comment cmnt(masm_, "[ CountOperation");

  bool is_postfix = node->is_postfix();
  bool is_increment = node->op() == Token::INC;

  Variable* var = node->expression()->AsVariableProxy()->AsVariable();
  bool is_const = (var != NULL && var->mode() == Variable::CONST);

  // Postfix operators need a stack slot under the reference to hold
  // the old value while the new one is being stored.
  if (is_postfix) {
    frame_->Push(Smi::FromInt(0));
  }

  { Reference target(this, node->expression());
    if (target.is_illegal()) {
      // Spoof the virtual frame to have the expected height (one higher
      // than on entry).
      if (!is_postfix) {
        frame_->Push(Smi::FromInt(0));
      }
      return;
    }
    target.TakeValue(NOT_INSIDE_TYPEOF);

    DeferredCountOperation* deferred =
        new DeferredCountOperation(this, is_postfix,
                                   is_increment, target.size());

    Result value = frame_->Pop();
    value.ToRegister();

    // Postfix: Store the old value as the result.
    if (is_postfix) {
      // Explicitly back the slot for the old value with a new register.
      // This improves performance in some cases.
      Result old_value = allocator_->Allocate();
      ASSERT(old_value.is_valid());
      __ mov(old_value.reg(), value.reg());
      // SetElement must not create a constant element or a copy in this slot,
      // since we will write to it, below the waterline, in deferred code.
      frame_->SetElementAt(target.size(), &old_value);
    }

    // Perform optimistic increment/decrement.  Ensure the value is
    // writable.
    frame_->Spill(value.reg());
    ASSERT(allocator_->count(value.reg()) == 1);

    // In order to combine the overflow and the smi check, we need to
    // be able to allocate a byte register.  We attempt to do so
    // without spilling.  If we fail, we will generate separate
    // overflow and smi checks.
    //
    // We need to allocate and clear the temporary byte register
    // before performing the count operation since clearing the
    // register using xor will clear the overflow flag.
    Result tmp = allocator_->AllocateByteRegisterWithoutSpilling();
    if (tmp.is_valid()) {
      __ Set(tmp.reg(), Immediate(0));
    }

    if (is_increment) {
      __ add(Operand(value.reg()), Immediate(Smi::FromInt(1)));
    } else {
      __ sub(Operand(value.reg()), Immediate(Smi::FromInt(1)));
    }

    // If the count operation didn't overflow and the result is a
    // valid smi, we're done. Otherwise, we jump to the deferred
    // slow-case code.
    //
    // We combine the overflow and the smi check if we could
    // successfully allocate a temporary byte register.
    if (tmp.is_valid()) {
      __ setcc(overflow, tmp.reg());
      __ or_(Operand(value.reg()), tmp.reg());
      tmp.Unuse();
      __ test(value.reg(), Immediate(kSmiTagMask));
      deferred->enter()->Branch(not_zero, &value, not_taken);
    } else {  // Otherwise we test separately for overflow and smi check.
      deferred->enter()->Branch(overflow, &value, not_taken);
      __ test(value.reg(), Immediate(kSmiTagMask));
      deferred->enter()->Branch(not_zero, &value, not_taken);
    }

    // Store the new value in the target if not const.
    deferred->BindExit(&value);
    frame_->Push(&value);
    if (!is_const) {
      target.SetValue(NOT_CONST_INIT);
    }
  }

  // Postfix: Discard the new value and use the old.
  if (is_postfix) {
    frame_->Drop();
  }
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
    JumpTarget is_true(this);
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
      JumpTarget pop_and_continue(this);
      JumpTarget exit(this);

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
    JumpTarget is_false(this);
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
      JumpTarget pop_and_continue(this);
      JumpTarget exit(this);

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


class InstanceofStub: public CodeStub {
 public:
  InstanceofStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Instanceof; }
  int MinorKey() { return 0; }
};


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


class DeferredReferenceGetKeyedValue: public DeferredCode {
 public:
  DeferredReferenceGetKeyedValue(CodeGenerator* generator, bool is_global)
      : DeferredCode(generator), is_global_(is_global) {
    set_comment("[ DeferredReferenceGetKeyedValue");
  }

  virtual void Generate();

  Label* patch_site() { return &patch_site_; }

 private:
  Label patch_site_;
  bool is_global_;
};


void DeferredReferenceGetKeyedValue::Generate() {
  CodeGenerator* cgen = generator();
  Result receiver(cgen);
  Result key(cgen);
  enter()->Bind(&receiver, &key);
  cgen->frame()->Push(&receiver);  // First IC argument.
  cgen->frame()->Push(&key);       // Second IC argument.

  // Calculate the delta from the IC call instruction to the map check
  // cmp instruction in the inlined version.  This delta is stored in
  // a test(eax, delta) instruction after the call so that we can find
  // it in the IC initialization code and patch the cmp instruction.
  // This means that we cannot allow test instructions after calls to
  // KeyedLoadIC stubs in other places.
  RelocInfo::Mode mode = is_global_
                         ? RelocInfo::CODE_TARGET_CONTEXT
                         : RelocInfo::CODE_TARGET;
  Result value = cgen->frame()->CallKeyedLoadIC(mode);
  // The result needs to be specifically the eax register because the
  // offset to the patch site will be expected in a test eax
  // instruction.
  ASSERT(value.is_register() && value.reg().is(eax));
  // The delta from the start of the map-compare instruction to the
  // test eax instruction.
  int delta_to_patch_site = __ SizeOfCodeGeneratedSince(patch_site());
  __ test(value.reg(), Immediate(-delta_to_patch_site));
  __ IncrementCounter(&Counters::keyed_load_inline_miss, 1);

  // The receiver and key were spilled by the call, so their state as
  // constants or copies has been changed.  Thus, they need to be
  // "mergable" in the block at the exit label and are therefore
  // passed as return results here.
  key = cgen->frame()->Pop();
  receiver = cgen->frame()->Pop();
  exit_.Jump(&receiver, &key, &value);
}


#undef __
#define __ masm->

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
  switch (type_) {
    case SLOT: {
      Comment cmnt(masm, "[ Load from Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      cgen_->LoadFromSlot(slot, typeof_state);
      break;
    }

    case NAMED: {
      // TODO(1241834): Make sure that it is safe to ignore the
      // distinction between expressions in a typeof and not in a
      // typeof. If there is a chance that reference errors can be
      // thrown below, we must distinguish between the two kinds of
      // loads (typeof expression loads must not throw a reference
      // error).
      Comment cmnt(masm, "[ Load from named Property");
      cgen_->frame()->Push(GetName());

      Variable* var = expression_->AsVariableProxy()->AsVariable();
      ASSERT(var == NULL || var->is_global());
      RelocInfo::Mode mode = (var == NULL)
                             ? RelocInfo::CODE_TARGET
                             : RelocInfo::CODE_TARGET_CONTEXT;
      Result answer = cgen_->frame()->CallLoadIC(mode);
      cgen_->frame()->Push(&answer);
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
        Comment cmnt(masm, "[ Inlined array index load");
        DeferredReferenceGetKeyedValue* deferred =
            new DeferredReferenceGetKeyedValue(cgen_, is_global);

        Result key = cgen_->frame()->Pop();
        Result receiver = cgen_->frame()->Pop();
        key.ToRegister();
        receiver.ToRegister();

        // Check that the receiver is not a smi (only needed if this
        // is not a load from the global context) and that it has the
        // expected map.
        if (!is_global) {
          __ test(receiver.reg(), Immediate(kSmiTagMask));
          deferred->enter()->Branch(zero, &receiver, &key, not_taken);
        }

        // Initially, use an invalid map. The map is patched in the IC
        // initialization code.
        __ bind(deferred->patch_site());
        __ cmp(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
               Immediate(Factory::null_value()));
        deferred->enter()->Branch(not_equal, &receiver, &key, not_taken);

        // Check that the key is a smi.
        __ test(key.reg(), Immediate(kSmiTagMask));
        deferred->enter()->Branch(not_zero, &receiver, &key, not_taken);

        // Get the elements array from the receiver and check that it
        // is not a dictionary.
        Result elements = cgen_->allocator()->Allocate();
        ASSERT(elements.is_valid());
        __ mov(elements.reg(),
               FieldOperand(receiver.reg(), JSObject::kElementsOffset));
        __ cmp(FieldOperand(elements.reg(), HeapObject::kMapOffset),
               Immediate(Factory::hash_table_map()));
        deferred->enter()->Branch(equal, &receiver, &key, not_taken);

        // Shift the key to get the actual index value and check that
        // it is within bounds.
        Result index = cgen_->allocator()->Allocate();
        ASSERT(index.is_valid());
        __ mov(index.reg(), key.reg());
        __ sar(index.reg(), kSmiTagSize);
        __ cmp(index.reg(),
               FieldOperand(elements.reg(), Array::kLengthOffset));
        deferred->enter()->Branch(above_equal, &receiver, &key, not_taken);

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
                                    Array::kHeaderSize - kHeapObjectTag));
        elements.Unuse();
        index.Unuse();
        __ cmp(Operand(value.reg()), Immediate(Factory::the_hole_value()));
        deferred->enter()->Branch(equal, &receiver, &key, not_taken);
        __ IncrementCounter(&Counters::keyed_load_inline, 1);

        // Restore the receiver and key to the frame and push the
        // result on top of it.
        deferred->BindExit(&receiver, &key, &value);
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
      slot->var()->mode() == Variable::CONST) {
    GetValue(typeof_state);
    return;
  }

  // Only non-constant, frame-allocated parameters and locals can reach
  // here.
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
  switch (type_) {
    case SLOT: {
      Comment cmnt(cgen_->masm(), "[ Store to Slot");
      Slot* slot = expression_->AsVariableProxy()->AsVariable()->slot();
      ASSERT(slot != NULL);
      cgen_->StoreToSlot(slot, init_state);
      break;
    }

    case NAMED: {
      Comment cmnt(cgen_->masm(), "[ Store to named Property");
      cgen_->frame()->Push(GetName());
      Result answer = cgen_->frame()->CallStoreIC();
      cgen_->frame()->Push(&answer);
      break;
    }

    case KEYED: {
      Comment cmnt(cgen_->masm(), "[ Store to keyed Property");
      Result answer = cgen_->frame()->CallKeyedStoreIC();
      cgen_->frame()->Push(&answer);
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


#undef __
#define __ masm_->

Result DeferredInlineBinaryOperation::GenerateInlineCode(Result* left,
                                                         Result* right) {
  // Perform fast-case smi code for the operation (left <op> right) and
  // returns the result in a Result.
  // If any fast-case tests fail, it jumps to the slow-case deferred code,
  // which calls the binary operation stub, with the arguments (in registers)
  // on top of the frame.
  // Consumes its arguments (sets left and right to invalid and frees their
  // registers).

  left->ToRegister();
  right->ToRegister();
  // A newly allocated register answer is used to hold the answer.
  // The registers containing left and right are not modified in
  // most cases, so they usually don't need to be spilled in the fast case.
  Result answer = generator()->allocator()->Allocate();

  ASSERT(answer.is_valid());
  // Perform the smi check.
  if (left->reg().is(right->reg())) {
    __ test(left->reg(), Immediate(kSmiTagMask));
  } else {
    __ mov(answer.reg(), left->reg());
    __ or_(answer.reg(), Operand(right->reg()));
    ASSERT(kSmiTag == 0);  // adjust zero check if not the case
    __ test(answer.reg(), Immediate(kSmiTagMask));
  }
  enter()->Branch(not_zero, left, right, not_taken);

  // All operations start by copying the left argument into answer.
  __ mov(answer.reg(), left->reg());
  switch (op_) {
    case Token::ADD:
      __ add(answer.reg(), Operand(right->reg()));  // add optimistically
      enter()->Branch(overflow, left, right, not_taken);
      break;

    case Token::SUB:
      __ sub(answer.reg(), Operand(right->reg()));  // subtract optimistically
      enter()->Branch(overflow, left, right, not_taken);
      break;

    case Token::MUL: {
      // If the smi tag is 0 we can just leave the tag on one operand.
      ASSERT(kSmiTag == 0);  // adjust code below if not the case
      // Remove tag from the left operand (but keep sign).
      // Left hand operand has been copied into answer.
      __ sar(answer.reg(), kSmiTagSize);
      // Do multiplication of smis, leaving result in answer.
      __ imul(answer.reg(), Operand(right->reg()));
      // Go slow on overflows.
      enter()->Branch(overflow, left, right, not_taken);
      // Check for negative zero result.  If product is zero,
      // and one argument is negative, go to slow case.
      // The frame is unchanged in this block, so local control flow can
      // use a Label rather than a JumpTarget.
      Label non_zero_result;
      __ test(answer.reg(), Operand(answer.reg()));
      __ j(not_zero, &non_zero_result, taken);
      __ mov(answer.reg(), left->reg());
      __ or_(answer.reg(), Operand(right->reg()));
      enter()->Branch(negative, left, right, not_taken);
      __ xor_(answer.reg(), Operand(answer.reg()));  // Positive 0 is correct.
      __ bind(&non_zero_result);
      break;
    }

    case Token::DIV:  // Fall through.
    case Token::MOD: {
      // Div and mod use the registers eax and edx.  Left and right must
      // be preserved, because the original operands are needed if we switch
      // to the slow case.  Move them if either is in eax or edx.
      // The Result answer should be changed into an alias for eax.
      // Precondition:
      // The Results left and right are valid.  They may be the same register,
      // and may be unspilled.  The Result answer is valid and is distinct
      // from left and right, and is spilled.
      // The value in left is copied to answer.

      Result reg_eax = generator()->allocator()->Allocate(eax);
      Result reg_edx = generator()->allocator()->Allocate(edx);
      // These allocations may have failed, if one of left, right, or answer
      // is in register eax or edx.
      bool left_copied_to_eax = false;  // We will make sure this becomes true.

      // Part 1: Get eax
      if (answer.reg().is(eax)) {
        reg_eax = answer;
        left_copied_to_eax = true;
      } else if (right->reg().is(eax) || left->reg().is(eax)) {
        // We need a non-edx register to move one or both of left and right to.
        // We use answer if it is not edx, otherwise we allocate one.
        if (answer.reg().is(edx)) {
          reg_edx = answer;
          answer = generator()->allocator()->Allocate();
          ASSERT(answer.is_valid());
        }

        if (left->reg().is(eax)) {
          reg_eax = *left;
          left_copied_to_eax = true;
          *left = answer;
        }
        if (right->reg().is(eax)) {
          reg_eax = *right;
          *right = answer;
        }
        __ mov(answer.reg(), eax);
      }
      // End of Part 1.
      // reg_eax is valid, and neither left nor right is in eax.
      ASSERT(reg_eax.is_valid());
      ASSERT(!left->reg().is(eax));
      ASSERT(!right->reg().is(eax));

      // Part 2: Get edx
      // reg_edx is invalid if and only if either left, right,
      // or answer is in edx.  If edx is valid, then either edx
      // was free, or it was answer, but answer was reallocated.
      if (answer.reg().is(edx)) {
        reg_edx = answer;
      } else if (right->reg().is(edx) || left->reg().is(edx)) {
        // Is answer used?
        if (answer.reg().is(eax) || answer.reg().is(left->reg()) ||
            answer.reg().is(right->reg())) {
          answer = generator()->allocator()->Allocate();
          ASSERT(answer.is_valid());  // We cannot hit both Allocate() calls.
        }
        if (left->reg().is(edx)) {
          reg_edx = *left;
          *left = answer;
        }
        if (right->reg().is(edx)) {
          reg_edx = *right;
          *right = answer;
        }
        __ mov(answer.reg(), edx);
      }
      // End of Part 2
      ASSERT(reg_edx.is_valid());
      ASSERT(!left->reg().is(eax));
      ASSERT(!right->reg().is(eax));

      answer = reg_eax;  // May free answer, if it was never used.
      generator()->frame()->Spill(eax);
      if (!left_copied_to_eax) {
        __ mov(eax, left->reg());
        left_copied_to_eax = true;
      }
      generator()->frame()->Spill(edx);

      // Postcondition:
      // reg_eax, reg_edx are valid, correct, and spilled.
      // reg_eax contains the value originally in left
      // left and right are not eax or edx.  They may or may not be
      // spilled or distinct.
      // answer is an alias for reg_eax.

      // Sign extend eax into edx:eax.
      __ cdq();
      // Check for 0 divisor.
      __ test(right->reg(), Operand(right->reg()));
      enter()->Branch(zero, left, right, not_taken);
      // Divide edx:eax by the right operand.
      __ idiv(right->reg());
      if (op_ == Token::DIV) {
        // Check for negative zero result.  If result is zero, and divisor
        // is negative, return a floating point negative zero.
        // The frame is unchanged in this block, so local control flow can
        // use a Label rather than a JumpTarget.
        Label non_zero_result;
        __ test(left->reg(), Operand(left->reg()));
        __ j(not_zero, &non_zero_result, taken);
        __ test(right->reg(), Operand(right->reg()));
        enter()->Branch(negative, left, right, not_taken);
        __ bind(&non_zero_result);
        // Check for the corner case of dividing the most negative smi
        // by -1. We cannot use the overflow flag, since it is not set
        // by idiv instruction.
        ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
        __ cmp(eax, 0x40000000);
        enter()->Branch(equal, left, right, not_taken);
        // Check that the remainder is zero.
        __ test(edx, Operand(edx));
        enter()->Branch(not_zero, left, right, not_taken);
        // Tag the result and store it in register temp.
        ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
        __ lea(answer.reg(), Operand(eax, eax, times_1, kSmiTag));
      } else {
        ASSERT(op_ == Token::MOD);
        // Check for a negative zero result.  If the result is zero, and the
        // dividend is negative, return a floating point negative zero.
        // The frame is unchanged in this block, so local control flow can
        // use a Label rather than a JumpTarget.
        Label non_zero_result;
        __ test(edx, Operand(edx));
        __ j(not_zero, &non_zero_result, taken);
        __ test(left->reg(), Operand(left->reg()));
        enter()->Branch(negative, left, right, not_taken);
        __ bind(&non_zero_result);
        // The answer is in edx.
        answer = reg_edx;
      }
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

    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      // Move right into ecx.
      // Left is in two registers already, so even if left or answer is ecx,
      // we can move right to it, and use the other one.
      // Right operand must be in register cl because x86 likes it that way.
      if (right->reg().is(ecx)) {
        // Right is already in the right place.  Left may be in the
        // same register, which causes problems.  Use answer instead.
        if (left->reg().is(ecx)) {
          *left = answer;
        }
      } else if (left->reg().is(ecx)) {
        generator()->frame()->Spill(left->reg());
        __ mov(left->reg(), right->reg());
        *right = *left;
        *left = answer;  // Use copy of left in answer as left.
      } else if (answer.reg().is(ecx)) {
        __ mov(answer.reg(), right->reg());
        *right = answer;
      } else {
        Result reg_ecx = generator()->allocator()->Allocate(ecx);
        ASSERT(reg_ecx.is_valid());
        __ mov(ecx, right->reg());
        *right = reg_ecx;
      }
      ASSERT(left->reg().is_valid());
      ASSERT(!left->reg().is(ecx));
      ASSERT(right->reg().is(ecx));
      answer.Unuse();  // Answer may now be being used for left or right.
      // We will modify left and right, which we do not do in any other
      // binary operation.  The exits to slow code need to restore the
      // original values of left and right, or at least values that give
      // the same answer.

      // We are modifying left and right.  They must be spilled!
      generator()->frame()->Spill(left->reg());
      generator()->frame()->Spill(right->reg());

      // Remove tags from operands (but keep sign).
      __ sar(left->reg(), kSmiTagSize);
      __ sar(ecx, kSmiTagSize);
      // Perform the operation.
      switch (op_) {
        case Token::SAR:
          __ sar(left->reg());
          // No checks of result necessary
          break;
        case Token::SHR: {
          __ shr(left->reg());
          // Check that the *unsigned* result fits in a smi.
          // Neither of the two high-order bits can be set:
          // - 0x80000000: high bit would be lost when smi tagging.
          // - 0x40000000: this number would convert to negative when
          // Smi tagging these two cases can only happen with shifts
          // by 0 or 1 when handed a valid smi.
          // If the answer cannot be represented by a SMI, restore
          // the left and right arguments, and jump to slow case.
          // The low bit of the left argument may be lost, but only
          // in a case where it is dropped anyway.
          JumpTarget result_ok(generator());
          __ test(left->reg(), Immediate(0xc0000000));
          result_ok.Branch(zero, left, taken);
          __ shl(left->reg());
          ASSERT(kSmiTag == 0);
          __ shl(left->reg(), kSmiTagSize);
          __ shl(right->reg(), kSmiTagSize);
          enter()->Jump(left, right);
          result_ok.Bind(left);
          break;
        }
        case Token::SHL: {
          __ shl(left->reg());
          // Check that the *signed* result fits in a smi.
          //
          // TODO(207): Can reduce registers from 4 to 3 by
          // preallocating ecx.
          JumpTarget result_ok(generator());
          Result smi_test_reg = generator()->allocator()->Allocate();
          ASSERT(smi_test_reg.is_valid());
          __ lea(smi_test_reg.reg(), Operand(left->reg(), 0x40000000));
          __ test(smi_test_reg.reg(), Immediate(0x80000000));
          smi_test_reg.Unuse();
          result_ok.Branch(zero, left, taken);
          __ shr(left->reg());
          ASSERT(kSmiTag == 0);
          __ shl(left->reg(), kSmiTagSize);
          __ shl(right->reg(), kSmiTagSize);
          enter()->Jump(left, right);
          result_ok.Bind(left);
          break;
        }
        default:
          UNREACHABLE();
      }
      // Smi-tag the result, in left, and make answer an alias for left->
      answer = *left;
      answer.ToRegister();
      ASSERT(kSmiTagSize == times_2);  // adjust code if not the case
      __ lea(answer.reg(),
             Operand(answer.reg(), answer.reg(), times_1, kSmiTag));
      break;

    default:
      UNREACHABLE();
      break;
  }
  left->Unuse();
  right->Unuse();
  return answer;
}


#undef __
#define __ masm->

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
                                                  edx);
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

      bool use_sse3 = CpuFeatures::IsSupported(CpuFeatures::SSE3);
      if (use_sse3) {
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

      // Check if result is non-negative and fits in a smi.
      __ test(eax, Immediate(0xc0000000));
      __ j(not_zero, &non_smi_result);

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
                                                    ecx, edx);
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
      if (use_sse3) {
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
      if (op_ == Token::SHR) __ bind(&non_smi_result);
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
    case Token::ADD:
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
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
                                             Register scratch2) {
  ExternalReference allocation_top =
      ExternalReference::new_space_allocation_top_address();
  ExternalReference allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  __ mov(Operand(scratch1), Immediate(allocation_top));
  __ mov(eax, Operand(scratch1, 0));
  __ lea(scratch2, Operand(eax, HeapNumber::kSize));  // scratch2: new top
  __ cmp(scratch2, Operand::StaticVariable(allocation_limit));
  __ j(above, need_gc, not_taken);

  __ mov(Operand(scratch1, 0), scratch2);  // store new top
  __ mov(Operand(eax, HeapObject::kMapOffset),
         Immediate(Factory::heap_number_map()));
  // Tag old top and use as result.
  __ add(Operand(eax), Immediate(kHeapObjectTag));
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
  __ mov(edx, Operand(eax));
  // edx: operand
  FloatingPointHelper::AllocateHeapNumber(masm, &undo, ebx, ecx);
  // eax: allocated 'empty' number
  __ fld_d(FieldOperand(edx, HeapNumber::kValueOffset));
  __ fchs();
  __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));

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
      __ mov(eax, FieldOperand(edx, HeapNumber::kValueOffset + kPointerSize));
      // Test that exponent bits are all set.
      __ not_(eax);
      __ test(eax, Immediate(0x7ff00000));
      __ j(not_zero, &return_equal);
      __ not_(eax);

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ shl(eax, 12);
      // Or with all low-bits of mantissa.
      __ or_(eax, FieldOperand(edx, HeapNumber::kValueOffset));
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

  // Save the return address (and get it off the stack).
  __ pop(ecx);

  // Push arguments.
  __ push(eax);
  __ push(edx);
  __ push(ecx);

  // Inlined floating point compare.
  // Call builtin if operands are not floating point or smi.
  FloatingPointHelper::CheckFloatOperands(masm, &call_builtin, ebx);
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
  ASSERT(StackHandlerConstants::kSize == 6 * kPointerSize);  // adjust this code
  ExternalReference handler_address(Top::k_handler_address);
  __ mov(edx, Operand::StaticVariable(handler_address));
  __ mov(ecx, Operand(edx, -1 * kPointerSize));  // get next in chain
  __ mov(Operand::StaticVariable(handler_address), ecx);
  __ mov(esp, Operand(edx));
  __ pop(edi);
  __ pop(ebp);
  __ pop(edx);  // remove code pointer
  __ pop(edx);  // remove state

  // Before returning we restore the context from the frame pointer if not NULL.
  // The frame pointer is NULL in the exception handler of a JS entry frame.
  __ xor_(esi, Operand(esi));  // tentatively set context pointer to NULL
  Label skip;
  __ cmp(ebp, 0);
  __ j(equal, &skip, not_taken);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

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
  // Fetch top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ mov(edx, Operand::StaticVariable(handler_address));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kAddressDisplacement +
      StackHandlerConstants::kStateOffset;
  __ cmp(Operand(edx, kStateOffset), Immediate(StackHandler::ENTRY));
  __ j(equal, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kAddressDisplacement +
      StackHandlerConstants::kNextOffset;
  __ mov(edx, Operand(edx, kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  __ mov(eax, Operand(edx, kNextOffset));
  __ mov(Operand::StaticVariable(handler_address), eax);

  // Set external caught exception to false.
  __ mov(eax, false);
  ExternalReference external_caught(Top::k_external_caught_exception_address);
  __ mov(Operand::StaticVariable(external_caught), eax);

  // Set pending exception and eax to out of memory exception.
  __ mov(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(Operand::StaticVariable(pending_exception), eax);

  // Restore the stack to the address of the ENTRY handler
  __ mov(esp, Operand(edx));

  // Clear the context pointer;
  __ xor_(esi, Operand(esi));

  // Restore registers from handler.
  __ pop(edi);  // PP
  __ pop(ebp);  // FP
  __ pop(edx);  // Code
  __ pop(edx);  // State

  __ ret(0);
}


void CEntryStub::GenerateBody(MacroAssembler* masm, bool is_debug_break) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: caller's parameter pointer pp  (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

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

  // Call into the runtime system. Collect garbage before the call if
  // running with --gc-greedy set.
  if (FLAG_gc_greedy) {
    Failure* failure = Failure::RetryAfterGC(0);
    __ mov(eax, Immediate(reinterpret_cast<int32_t>(failure)));
  }
  GenerateCore(masm, &throw_normal_exception,
               &throw_out_of_memory_exception,
               frame_type,
               FLAG_gc_greedy,
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
  __ push(eax);  // flush TOS

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
  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));  // ebx - object map
  __ movzx_b(ecx, FieldOperand(eax, Map::kInstanceTypeOffset));  // ecx - type
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &slow, not_taken);
  __ cmp(ecx, LAST_JS_OBJECT_TYPE);
  __ j(greater, &slow, not_taken);

  // Get the prototype of the function.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // 1 ~ return address
  __ TryGetFunctionPrototype(edx, ebx, ecx, &slow);

  // Check that the function prototype is a JS object.
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


#undef __

} }  // namespace v8::internal
