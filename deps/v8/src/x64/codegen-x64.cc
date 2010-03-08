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
#include "regexp-macro-assembler.h"
#include "register-allocator-inl.h"
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
      __ movq(Operand(rbp, action), RegisterAllocator::ToRegister(i));
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
      __ movq(RegisterAllocator::ToRegister(i), Operand(rbp, action));
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
// Deferred code objects
//
// These subclasses of DeferredCode add pieces of code to the end of generated
// code.  They are branched to from the generated code, and
// keep some slower code out of the main body of the generated code.
// Many of them call a code stub or a runtime function.

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


class FloatingPointHelper : public AllStatic {
 public:
  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand on TOS+1. Returns operand as floating point number on FPU
  // stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register scratch);

  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in src register. Returns operand as floating point number
  // in XMM register
  static void LoadFloatOperand(MacroAssembler* masm,
                               Register src,
                               XMMRegister dst);

  // Code pattern for loading floating point values. Input values must
  // be either smi or heap number objects (fp values). Requirements:
  // operand_1 in rdx, operand_2 in rax; Returns operands as
  // floating point numbers in XMM registers.
  static void LoadFloatOperands(MacroAssembler* masm,
                                XMMRegister dst1,
                                XMMRegister dst2);

  // Similar to LoadFloatOperands, assumes that the operands are smis.
  static void LoadFloatOperandsFromSmis(MacroAssembler* masm,
                                        XMMRegister dst1,
                                        XMMRegister dst2);

  // Code pattern for loading floating point values onto the fp stack.
  // Input values must be either smi or heap number objects (fp values).
  // Requirements:
  // Register version: operands in registers lhs and rhs.
  // Stack version: operands on TOS+1 and TOS+2.
  // Returns operands as floating point numbers on fp stack.
  static void LoadFloatOperands(MacroAssembler* masm,
                                Register lhs,
                                Register rhs);

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in rax, operand_2 in rdx; falls through on float or smi
  // operands, jumps to the non_float label otherwise.
  static void CheckNumberOperands(MacroAssembler* masm,
                                  Label* non_float);

  // Takes the operands in rdx and rax and loads them as integers in rax
  // and rcx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             bool use_sse3,
                             Label* operand_conversion_failure);
};


// -----------------------------------------------------------------------------
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
    // rbp: caller's frame pointer
    // rsp: stack pointer
    // rdi: called JS function
    // rsi: callee's context
    allocator_->Initialize();

    if (info->mode() == CompilationInfo::PRIMARY) {
      frame_->Enter();

      // Allocate space for locals and initialize them.
      frame_->AllocateStackSlots();

      // Allocate the local context if needed.
      int heap_slots = scope()->num_heap_slots();
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
    } else {
      // When used as the secondary compiler for splitting, rbp, rsi,
      // and rdi have been pushed on the stack.  Adjust the virtual
      // frame to match this state.
      frame_->Adjust(3);
      allocator_->Unuse(rdi);

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
  DeleteFrame();
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
      && (allocator()->count(r15) == (frame()->is_used(r15) ? 1 : 0))
      && (allocator()->count(r12) == (frame()->is_used(r12) ? 1 : 0));
}
#endif


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
  // movq instruction in the inlined version.  This delta is stored in
  // a test(rax, delta) instruction after the call so that we can find
  // it in the IC initialization code and patch the movq instruction.
  // This means that we cannot allow test instructions after calls to
  // KeyedLoadIC stubs in other places.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  RelocInfo::Mode mode = is_global_
                         ? RelocInfo::CODE_TARGET_CONTEXT
                         : RelocInfo::CODE_TARGET;
  __ Call(ic, mode);
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
  if (!value_.is(rax)) __ movq(rax, value_);
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
  // Restore value (returned from store IC), key and receiver
  // registers.
  if (!value_.is(rax)) __ movq(value_, rax);
  __ pop(key_);
  __ pop(receiver_);
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
      ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
      __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(below, &build_args);

      // Check that applicand.apply is Function.prototype.apply.
      __ movq(rax, Operand(rsp, kPointerSize));
      is_smi = masm_->CheckSmi(rax);
      __ j(is_smi, &build_args);
      __ CmpObjectType(rax, JS_FUNCTION_TYPE, rcx);
      __ j(not_equal, &build_args);
      __ movq(rax, FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset));
      Handle<Code> apply_code(Builtins::builtin(Builtins::FunctionApply));
      __ Cmp(FieldOperand(rax, SharedFunctionInfo::kCodeOffset), apply_code);
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
      __ movq(rax, Immediate(scope()->num_parameters()));
      for (int i = 0; i < scope()->num_parameters(); i++) {
        __ push(frame_->ParameterAt(i));
      }
      __ jmp(&invoke);

      // Arguments adaptor frame present. Copy arguments from there, but
      // avoid copying too many arguments to avoid stack overflows.
      __ bind(&adapted);
      static const uint32_t kArgumentsLimit = 1 * KB;
      __ movq(rax, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
      __ SmiToInteger32(rax, rax);
      __ movq(rcx, rax);
      __ cmpq(rax, Immediate(kArgumentsLimit));
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
  // TODO(X64): No architecture specific code. Move to shared location.
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
  // TODO(X64): This code is completely generic and should be moved somewhere
  // where it can be shared between architectures.
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
        // If we have chosen not to recompile the test at the
        // bottom, jump back to the one at the top.
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
  __ movl(rax, FieldOperand(rdx, FixedArray::kLengthOffset));
  __ Integer32ToSmi(rax, rax);
  frame_->EmitPush(rax);  // <- slot 1
  frame_->EmitPush(Smi::FromInt(0));  // <- slot 0
  entry.Jump();

  fixed_array.Bind();
  // rax: fixed array (result from call to Runtime::kGetPropertyNamesFast)
  frame_->EmitPush(Smi::FromInt(0));  // <- slot 3
  frame_->EmitPush(rax);  // <- slot 2

  // Push the length of the array and the initial index onto the stack.
  __ movl(rax, FieldOperand(rax, FixedArray::kLengthOffset));
  __ Integer32ToSmi(rax, rax);
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
  __ CompareRoot(rbx, Heap::kNullValueRootIndex);
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
    ASSERT(StackHandlerConstants::kNextOffset == 0);
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

      ASSERT(StackHandlerConstants::kNextOffset == 0);
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
    ASSERT(StackHandlerConstants::kNextOffset == 0);
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
      ASSERT(StackHandlerConstants::kNextOffset == 0);
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


void CodeGenerator::InstantiateBoilerplate(Handle<JSFunction> boilerplate) {
  ASSERT(boilerplate->IsBoilerplate());

  // The inevitable call will sync frame elements to memory anyway, so
  // we do it eagerly to allow us to push the arguments directly into
  // place.
  frame_->SyncRange(0, frame_->element_count() - 1);

  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && boilerplate->NumberOfLiterals() == 0) {
    FastNewClosureStub stub;
    frame_->Push(boilerplate);
    Result answer = frame_->CallStub(&stub, 1);
    frame_->Push(&answer);
  } else {
    // Call the runtime to instantiate the function boilerplate
    // object.
    frame_->EmitPush(rsi);
    frame_->EmitPush(boilerplate);
    Result result = frame_->CallRuntime(Runtime::kNewClosure, 2);
    frame_->Push(&result);
  }
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate =
      Compiler::BuildBoilerplate(node, script(), this);
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
  __ movq(literals.reg(),
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
          frame_->Push(key);
          Result ignored = frame_->CallStoreIC();
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


void CodeGenerator::VisitAssignment(Assignment* node) {
  Comment cmnt(masm_, "[ Assignment");

  { Reference target(this, node->target(), node->is_compound());
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
    if (node->ends_initialization_block()) {
      // Add an extra copy of the receiver to the frame, so that it can be
      // converted back to fast case after the assignment.
      ASSERT(target.type() == Reference::NAMED ||
             target.type() == Reference::KEYED);
      if (target.type() == Reference::NAMED) {
        frame_->Dup();
        // Dup target receiver on stack.
      } else {
        ASSERT(target.type() == Reference::KEYED);
        Result temp = frame_->Pop();
        frame_->Dup();
        frame_->Push(&temp);
      }
    }
    if (node->op() == Token::ASSIGN ||
        node->op() == Token::INIT_VAR ||
        node->op() == Token::INIT_CONST) {
      Load(node->value());

    } else {  // Assignment is a compound assignment.
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
        target.TakeValue();
      } else {
        target.GetValue();
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
      UnloadReference(&target);
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
        ASSERT(target.type() == Reference::UNLOADED);
        // End of initialization block. Revert to fast case.  The
        // argument to the runtime call is the extra copy of the receiver,
        // which is below the value of the assignment.
        // Swap the receiver and the value of the assignment expression.
        Result lhs = frame_->Pop();
        Result receiver = frame_->Pop();
        frame_->Push(&lhs);
        frame_->Push(&receiver);
        Result ignored = frame_->CallRuntime(Runtime::kToFastProperties, 1);
      }
    }
  }
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
    // JavaScript example: 'with (obj) foo(1, 2, 3)'  // foo is in obj
    // ----------------------------------

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
      } else {
        Reference ref(this, property, false);
        ASSERT(ref.size() == 2);
        Result key = frame_->Pop();
        frame_->Dup();  // Duplicate the receiver.
        frame_->Push(&key);
        ref.GetValue();
        // Top of frame contains function to call, with duplicate copy of
        // receiver below it.  Swap them.
        Result function = frame_->Pop();
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
    bool overwrite =
      (node->expression()->AsBinaryOperation() != NULL &&
       node->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
    Load(node->expression());
    switch (op) {
      case Token::NOT:
      case Token::DELETE:
      case Token::TYPEOF:
        UNREACHABLE();  // handled above
        break;

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
        frame_->Push(&answer);
        break;
      }

      case Token::ADD: {
        // Smi check.
        JumpTarget continue_label;
        Result operand = frame_->Pop();
        operand.ToRegister();
        Condition is_smi = masm_->CheckSmi(operand.reg());
        continue_label.Branch(is_smi, &operand);
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
  __ push(dst_);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
  __ push(rax);
  __ Push(Smi::FromInt(1));
  if (is_increment_) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
  if (!dst_.is(rax)) __ movq(dst_, rax);
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
  __ push(dst_);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);

  // Save the result of ToNumber to use as the old value.
  __ push(rax);

  // Call the runtime for the addition or subtraction.
  __ push(rax);
  __ Push(Smi::FromInt(1));
  if (is_increment_) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
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
    }
    // Ensure the new value is writable.
    frame_->Spill(new_value.reg());

    DeferredCode* deferred = NULL;
    if (is_postfix) {
      deferred = new DeferredPostfixCountOperation(new_value.reg(),
                                                   old_value.reg(),
                                                   is_increment);
    } else {
      deferred = new DeferredPrefixCountOperation(new_value.reg(),
                                                  is_increment);
    }

    __ JumpIfNotSmi(new_value.reg(), deferred->entry_label());
    if (is_increment) {
      __ SmiAddConstant(kScratchRegister,
                        new_value.reg(),
                        Smi::FromInt(1),
                        deferred->entry_label());
    } else {
      __ SmiSubConstant(kScratchRegister,
                        new_value.reg(),
                        Smi::FromInt(1),
                        deferred->entry_label());
    }
    __ movq(new_value.reg(), kScratchRegister);
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
  // TODO(X64): This code was copied verbatim from codegen-ia32.
  //     Either find a reason to change it or move it to a shared location.

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
  Load(left);
  Load(right);
  Comparison(node, cc, strict, destination());
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
  frame_->PushFunction();
}


void CodeGenerator::GenerateArgumentsAccess(ZoneList<Expression*>* args) {
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
  __ CmpInstanceType(kScratchRegister, FIRST_JS_OBJECT_TYPE);
  destination()->false_target()->Branch(less);
  __ CmpInstanceType(kScratchRegister, LAST_JS_OBJECT_TYPE);
  obj.Unuse();
  destination()->Split(less_equal);
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
  // ArgumentsAccessStub takes the parameter count as an input argument
  // in register eax.  Create a constant result for it.
  Result count(Handle<Smi>(Smi::FromInt(scope()->num_parameters())));
  // Call the shared stub to get to the arguments.length.
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_LENGTH);
  Result result = frame_->CallStub(&stub, &count);
  frame_->Push(&result);
}


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

  // Get register rcx to use as shift amount later.
  Result shift_amount;
  if (object.is_register() && object.reg().is(rcx)) {
    Result fresh = allocator_->Allocate();
    shift_amount = object;
    object = fresh;
    __ movq(object.reg(), rcx);
  }
  if (index.is_register() && index.reg().is(rcx)) {
    Result fresh = allocator_->Allocate();
    shift_amount = index;
    index = fresh;
    __ movq(index.reg(), rcx);
  }
  // There could be references to ecx in the frame. Allocating will
  // spill them, otherwise spill explicitly.
  if (shift_amount.is_valid()) {
    frame_->Spill(rcx);
  } else {
    shift_amount = allocator()->Allocate(rcx);
  }
  ASSERT(shift_amount.is_register());
  ASSERT(shift_amount.reg().is(rcx));
  ASSERT(allocator_->count(rcx) == 1);

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
  __ JumpIfSmi(object.reg(), &slow_case);

  // If the index is negative or non-smi trigger the slow case.
  __ JumpIfNotPositiveSmi(index.reg(), &slow_case);

  // Untag the index.
  __ SmiToInteger32(index.reg(), index.reg());

  __ bind(&try_again_with_new_string);
  // Fetch the instance type of the receiver into rcx.
  __ movq(rcx, FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzxbl(rcx, FieldOperand(rcx, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the slow case.
  __ testb(rcx, Immediate(kIsNotStringMask));
  __ j(not_zero, &slow_case);

  // Check for index out of range.
  __ cmpl(index.reg(), FieldOperand(object.reg(), String::kLengthOffset));
  __ j(greater_equal, &slow_case);
  // Reload the instance type (into the temp register this time)..
  __ movq(temp.reg(), FieldOperand(object.reg(), HeapObject::kMapOffset));
  __ movzxbl(temp.reg(), FieldOperand(temp.reg(), Map::kInstanceTypeOffset));

  // We need special handling for non-flat strings.
  ASSERT_EQ(0, kSeqStringTag);
  __ testb(temp.reg(), Immediate(kStringRepresentationMask));
  __ j(not_zero, &not_a_flat_string);
  // Check for 1-byte or 2-byte string.
  ASSERT_EQ(0, kTwoByteStringTag);
  __ testb(temp.reg(), Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the temp register.
  __ movzxwl(temp.reg(), FieldOperand(object.reg(),
                                      index.reg(),
                                      times_2,
                                      SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  __ bind(&ascii_string);
  // Load the byte into the temp register.
  __ movzxbl(temp.reg(), FieldOperand(object.reg(),
                                      index.reg(),
                                      times_1,
                                      SeqAsciiString::kHeaderSize));
  __ bind(&got_char_code);
  __ Integer32ToSmi(temp.reg(), temp.reg());
  __ jmp(&end);

  // Handle non-flat strings.
  __ bind(&not_a_flat_string);
  __ and_(temp.reg(), Immediate(kStringRepresentationMask));
  __ cmpb(temp.reg(), Immediate(kConsStringTag));
  __ j(not_equal, &slow_case);

  // ConsString.
  // Check that the right hand side is the empty string (ie if this is really a
  // flat string in a cons string).  If that is not the case we would rather go
  // to the runtime system now, to flatten the string.
  __ movq(temp.reg(), FieldOperand(object.reg(), ConsString::kSecondOffset));
  __ CompareRoot(temp.reg(), Heap::kEmptyStringRootIndex);
  __ j(not_equal, &slow_case);
  // Get the first of the two strings.
  __ movq(object.reg(), FieldOperand(object.reg(), ConsString::kFirstOffset));
  __ jmp(&try_again_with_new_string);

  __ bind(&slow_case);
  // Move the undefined value into the result register, which will
  // trigger the slow case.
  __ LoadRoot(temp.reg(), Heap::kUndefinedValueRootIndex);

  __ bind(&end);
  frame_->Push(&temp);
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
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  Result rbp_as_smi = allocator_->Allocate();
  ASSERT(rbp_as_smi.is_valid());
  __ movq(rbp_as_smi.reg(), rbp);
  frame_->Push(&rbp_as_smi);
}


void CodeGenerator::GenerateRandomPositiveSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);
  frame_->SpillAll();
  __ push(rsi);

  // Make sure the frame is aligned like the OS expects.
  static const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    __ movq(rbx, rsp);  // Save in AMD-64 abi callee-saved register.
    __ and_(rsp, Immediate(-kFrameAlignment));
  }

  // Call V8::RandomPositiveSmi().
  __ Call(FUNCTION_ADDR(V8::RandomPositiveSmi), RelocInfo::RUNTIME_ENTRY);

  // Restore stack pointer from callee-saved register.
  if (kFrameAlignment > 0) {
    __ movq(rsp, rbx);
  }

  __ pop(rsi);
  Result result = allocator_->Allocate(rax);
  frame_->Push(&result);
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


void CodeGenerator::GenerateNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and jump to the runtime.
  Load(args->at(0));

  Result answer = frame_->CallRuntime(Runtime::kNumberToString, 1);
  frame_->Push(&answer);
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


// -----------------------------------------------------------------------------
// CodeGenerator implementation of Expressions

void CodeGenerator::LoadAndSpill(Expression* expression) {
  // TODO(x64): No architecture specific code. Move to shared location.
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
    // TODO(X64): Make control flow to control destinations work.
    ToBoolean(dest);
  }

  ASSERT(!(force_control && !dest->is_used()));
  ASSERT(dest->is_used() || frame_->height() == original_height + 1);
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
    __ SmiCompare(value.reg(), Smi::FromInt(0));
    dest->false_target()->Branch(equal);
    Condition is_smi = masm_->CheckSmi(value.reg());
    dest->true_target()->Branch(is_smi);
    __ fldz();
    __ fld_d(FieldOperand(value.reg(), HeapNumber::kValueOffset));
    __ FCmp();
    value.Unuse();
    dest->Split(not_zero);
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


void CodeGenerator::LoadUnsafeSmi(Register target, Handle<Object> value) {
  UNIMPLEMENTED();
  // TODO(X64): Implement security policy for loads of smis.
}


bool CodeGenerator::IsUnsafeSmi(Handle<Object> value) {
  return false;
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
        __ movq(value.reg(),
               ContextSlotOperandCheckExtensions(potential_slot,
                                                 value,
                                                 &slow));
        if (potential_slot->var()->mode() == Variable::CONST) {
          __ CompareRoot(value.reg(), Heap::kTheHoleValueRootIndex);
          done.Branch(not_equal, &value);
          __ LoadRoot(value.reg(), Heap::kUndefinedValueRootIndex);
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
  // Discard the global object. The result is in answer.
  frame_->Drop();
  return answer;
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
    LoadFromSlot(scope()->arguments()->var()->slot(), NOT_INSIDE_TYPEOF);
    Result probe = frame_->Pop();
    if (probe.is_constant()) {
      // We have to skip updating the arguments object if it has been
      // assigned a proper value.
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

      Condition left_is_smi = masm_->CheckSmi(left_side.reg());
      is_smi.Branch(left_is_smi);

      bool is_for_loop_compare = (node->AsCompareOperation() != NULL)
          && node->AsCompareOperation()->is_for_loop_condition();
      if (!is_for_loop_compare && right_val->IsSmi()) {
        // Right side is a constant smi and left side has been checked
        // not to be a smi.
        JumpTarget not_number;
        __ Cmp(FieldOperand(left_reg, HeapObject::kMapOffset),
               Factory::heap_number_map());
        not_number.Branch(not_equal, &left_side);
        __ movsd(xmm1,
                 FieldOperand(left_reg, HeapNumber::kValueOffset));
        int value = Smi::cast(*right_val)->value();
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
      CompareStub stub(cc, strict);
      Result result = frame_->CallStub(&stub, &left_side, &right_side);
      result.ToRegister();
      __ testq(result.reg(), result.reg());
      result.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

      is_smi.Bind();
      left_side = Result(left_reg);
      right_side = Result(right_val);
      // Test smi equality and comparison by signed int comparison.
      // Both sides are smis, so we can use an Immediate.
      __ SmiCompare(left_side.reg(), Smi::cast(*right_side.handle()));
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
    __ CompareRoot(operand.reg(), Heap::kNullValueRootIndex);
    if (strict) {
      operand.Unuse();
      dest->Split(equal);
    } else {
      // The 'null' value is only equal to 'undefined' if using non-strict
      // comparisons.
      dest->true_target()->Branch(equal);
      __ CompareRoot(operand.reg(), Heap::kUndefinedValueRootIndex);
      dest->true_target()->Branch(equal);
      Condition is_smi = masm_->CheckSmi(operand.reg());
      dest->false_target()->Branch(is_smi);

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
      // The result is a Smi, which is negative, zero, or positive.
      __ SmiTest(answer.reg());  // Sets both zero and sign flag.
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

      Condition both_smi = masm_->CheckBothSmi(left_reg, right_reg);
      is_smi.Branch(both_smi);
      // When non-smi, call out to the compare stub.
      CompareStub stub(cc, strict);
      Result answer = frame_->CallStub(&stub, &left_side, &right_side);
      __ SmiTest(answer.reg());  // Sets both zero and sign flags.
      answer.Unuse();
      dest->true_target()->Branch(cc);
      dest->false_target()->Jump();

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
  GenericBinaryOpStub stub(op_, mode_, NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, left_, right_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
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
  // We rely on the fact that smis have a 32 bit payload on x64.
  ASSERT(kSmiValueSize == 32);
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
      // Result is always a smi.
      result_type = NumberInfo::kSmi;
      break;
    case Token::SAR:
    case Token::SHL:
      // Result is always a smi.
      result_type = NumberInfo::kSmi;
      break;
    case Token::SHR:
      // Result of x >>> y is always a smi if y >= 1, otherwise a number.
      result_type = (right.is_constant() && right.handle()->IsSmi()
                     && Smi::cast(*right.handle())->value() >= 1)
          ? NumberInfo::kSmi
          : NumberInfo::kNumber;
      break;
    case Token::ADD:
      // Result could be a string or a number. Check types of inputs.
      result_type = NumberInfo::IsNumber(operands_type)
          ? NumberInfo::kNumber
          : NumberInfo::kUnknown;
      break;
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
  __ pop(receiver_);
}


void DeferredInlineSmiAdd::Generate() {
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


void DeferredInlineSmiAddReversed::Generate() {
  GenericBinaryOpStub igostub(Token::ADD, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, value_, dst_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


void DeferredInlineSmiSub::Generate() {
  GenericBinaryOpStub igostub(Token::SUB, overwrite_mode_, NO_SMI_CODE_IN_STUB);
  igostub.GenerateCall(masm_, dst_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
}


void DeferredInlineSmiOperation::Generate() {
  // For mod we don't generate all the Smi code inline.
  GenericBinaryOpStub stub(
      op_,
      overwrite_mode_,
      (op_ == Token::MOD) ? NO_GENERIC_BINARY_FLAGS : NO_SMI_CODE_IN_STUB);
  stub.GenerateCall(masm_, src_, value_);
  if (!dst_.is(rax)) __ movq(dst_, rax);
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
      __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
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
        answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
                                          overwrite_mode);
      } else {
        operand->ToRegister();
        frame_->Spill(operand->reg());
        DeferredCode* deferred = new DeferredInlineSmiSub(operand->reg(),
                                                          smi_value,
                                                          overwrite_mode);
        __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
        // A smi currently fits in a 32-bit Immediate.
        __ SmiSubConstant(operand->reg(),
                          operand->reg(),
                          smi_value,
                          deferred->entry_label());
        deferred->BindExit();
        answer = *operand;
      }
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
        __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
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
        __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
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
        Result constant_operand(value);
        answer = LikelySmiBinaryOperation(op, &constant_operand, operand,
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
          __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
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
          __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
          __ SmiShiftLeftConstant(answer.reg(),
                                  operand->reg(),
                                  shift_value,
                                  deferred->entry_label());
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
      __ JumpIfNotSmi(operand->reg(), deferred->entry_label());
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

Result CodeGenerator::LikelySmiBinaryOperation(Token::Value op,
                                               Result* left,
                                               Result* right,
                                               OverwriteMode overwrite_mode) {
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
    __ JumpIfNotBothSmi(left->reg(), right->reg(), deferred->entry_label());

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
    __ movq(answer.reg(), left->reg());
    __ or_(answer.reg(), rcx);
    __ JumpIfNotSmi(answer.reg(), deferred->entry_label());

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
                        rcx,
                        deferred->entry_label());
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
  __ JumpIfNotBothSmi(left->reg(), right->reg(), deferred->entry_label());

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


Result CodeGenerator::EmitKeyedLoad(bool is_global) {
  Comment cmnt(masm_, "[ Load from keyed Property");
  // Inline array load code if inside of a loop.  We do not know
  // the receiver map yet, so we initially generate the code with
  // a check against an invalid map.  In the inline cache code, we
  // patch the map check if appropriate.
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
    Result index = allocator()->Allocate();
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
      __ JumpIfSmi(receiver.reg(), deferred->entry_label());
    }

    // Initially, use an invalid map. The map is patched in the IC
    // initialization code.
    __ bind(deferred->patch_site());
    // Use masm-> here instead of the double underscore macro since extra
    // coverage code can interfere with the patching.  Do not use
    // root array to load null_value, since it must be patched with
    // the expected receiver map.
    masm_->movq(kScratchRegister, Factory::null_value(),
                RelocInfo::EMBEDDED_OBJECT);
    masm_->cmpq(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                kScratchRegister);
    deferred->Branch(not_equal);

    // Check that the key is a non-negative smi.
    __ JumpIfNotPositiveSmi(key.reg(), deferred->entry_label());

    // Get the elements array from the receiver and check that it
    // is not a dictionary.
    __ movq(elements.reg(),
            FieldOperand(receiver.reg(), JSObject::kElementsOffset));
    __ Cmp(FieldOperand(elements.reg(), HeapObject::kMapOffset),
           Factory::fixed_array_map());
    deferred->Branch(not_equal);

    // Shift the key to get the actual index value and check that
    // it is within bounds.
    __ SmiToInteger32(index.reg(), key.reg());
    __ cmpl(index.reg(),
            FieldOperand(elements.reg(), FixedArray::kLengthOffset));
    deferred->Branch(above_equal);

    // The index register holds the un-smi-tagged key. It has been
    // zero-extended to 64-bits, so it can be used directly as index in the
    // operand below.
    // Load and check that the result is not the hole.  We could
    // reuse the index or elements register for the value.
    //
    // TODO(206): Consider whether it makes sense to try some
    // heuristic about which register to reuse.  For example, if
    // one is rax, the we can reuse that one because the value
    // coming from the deferred code will be in rax.
    Result value = index;
    __ movq(value.reg(),
            Operand(elements.reg(),
                    index.reg(),
                    times_pointer_size,
                    FixedArray::kHeaderSize - kHeapObjectTag));
    elements.Unuse();
    index.Unuse();
    __ CompareRoot(value.reg(), Heap::kTheHoleValueRootIndex);
    deferred->Branch(equal);
    __ IncrementCounter(&Counters::keyed_load_inline, 1);

    deferred->BindExit();
    // Restore the receiver and key to the frame and push the
    // result on top of it.
    frame_->Push(&receiver);
    frame_->Push(&key);
    return value;

  } else {
    Comment cmnt(masm_, "[ Load from keyed Property");
    RelocInfo::Mode mode = is_global
        ? RelocInfo::CODE_TARGET_CONTEXT
        : RelocInfo::CODE_TARGET;
    Result answer = frame_->CallKeyedLoadIC(mode);
    // Make sure that we do not have a test instruction after the
    // call.  A test instruction after the call is used to
    // indicate that we have generated an inline version of the
    // keyed load.  The explicit nop instruction is here because
    // the push that follows might be peep-hole optimized away.
    __ nop();
    return answer;
  }
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
        // A test rax instruction following the call signals that the
        // inobject property case was inlined.  Ensure that there is not
        // a test rax instruction here.
        __ nop();
        cgen_->frame()->Push(&answer);
      } else {
        // Inline the inobject property case.
        Comment cmnt(masm, "[ Inlined named property load");
        Result receiver = cgen_->frame()->Pop();
        receiver.ToRegister();
        Result value = cgen_->allocator()->Allocate();
        ASSERT(value.is_valid());
        // Cannot use r12 for receiver, because that changes
        // the distance between a call and a fixup location,
        // due to a special encoding of r12 as r/m in a ModR/M byte.
        if (receiver.reg().is(r12)) {
          // Swap receiver and value.
          __ movq(value.reg(), receiver.reg());
          Result temp = receiver;
          receiver = value;
          value = temp;
          cgen_->frame()->Spill(value.reg());  // r12 may have been shared.
        }

        DeferredReferenceGetNamedValue* deferred =
            new DeferredReferenceGetNamedValue(value.reg(),
                                               receiver.reg(),
                                               GetName());

        // Check that the receiver is a heap object.
        __ JumpIfSmi(receiver.reg(), deferred->entry_label());

        __ bind(deferred->patch_site());
        // This is the map check instruction that will be patched (so we can't
        // use the double underscore macro that may insert instructions).
        // Initially use an invalid map to force a failure.
        masm->Move(kScratchRegister, Factory::null_value());
        masm->cmpq(FieldOperand(receiver.reg(), HeapObject::kMapOffset),
                   kScratchRegister);
        // This branch is always a forwards branch so it's always a fixed
        // size which allows the assert below to succeed and patching to work.
        // Don't use deferred->Branch(...), since that might add coverage code.
        masm->j(not_equal, deferred->entry_label());

        // The delta from the patch label to the load offset must be
        // statically known.
        ASSERT(masm->SizeOfCodeGeneratedSince(deferred->patch_site()) ==
               LoadIC::kOffsetToLoadInstruction);
        // The initial (invalid) offset has to be large enough to force
        // a 32-bit instruction encoding to allow patching with an
        // arbitrary offset.  Use kMaxInt (minus kHeapObjectTag).
        int offset = kMaxInt;
        masm->movq(value.reg(), FieldOperand(receiver.reg(), offset));

        __ IncrementCounter(&Counters::named_load_inline, 1);
        deferred->BindExit();
        cgen_->frame()->Push(&receiver);
        cgen_->frame()->Push(&value);
      }
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Load from keyed Property");
      Variable* var = expression_->AsVariableProxy()->AsVariable();
      bool is_global = var != NULL;
      ASSERT(!is_global || var->is_global());

      Result value = cgen_->EmitKeyedLoad(is_global);
      cgen_->frame()->Push(&value);
      break;
    }

    default:
      UNREACHABLE();
  }

  if (!persist_after_get_) {
    cgen_->UnloadReference(this);
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
      cgen_->UnloadReference(this);
      break;
    }

    case NAMED: {
      Comment cmnt(masm, "[ Store to named Property");
      cgen_->frame()->Push(GetName());
      Result answer = cgen_->frame()->CallStoreIC();
      cgen_->frame()->Push(&answer);
      set_unloaded();
      break;
    }

    case KEYED: {
      Comment cmnt(masm, "[ Store to keyed Property");

      // Generate inlined version of the keyed store if the code is in
      // a loop and the key is likely to be a smi.
      Property* property = expression()->AsProperty();
      ASSERT(property != NULL);
      StaticType* key_smi_analysis = property->key()->type();

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

        // Check that the value is a smi if it is not a constant.
        // We can skip the write barrier for smis and constants.
        if (!value_is_constant) {
          __ JumpIfNotSmi(value.reg(), deferred->entry_label());
        }

        // Check that the key is a non-negative smi.
        __ JumpIfNotPositiveSmi(key.reg(), deferred->entry_label());

        // Check that the receiver is not a smi.
        __ JumpIfSmi(receiver.reg(), deferred->entry_label());

        // Check that the receiver is a JSArray.
        __ CmpObjectType(receiver.reg(), JS_ARRAY_TYPE, kScratchRegister);
        deferred->Branch(not_equal);

        // Check that the key is within bounds.  Both the key and the
        // length of the JSArray are smis.
        __ SmiCompare(FieldOperand(receiver.reg(), JSArray::kLengthOffset),
                      key.reg());
        deferred->Branch(less_equal);

        // Get the elements array from the receiver and check that it
        // is a flat array (not a dictionary).
        __ movq(tmp.reg(),
                FieldOperand(receiver.reg(), JSObject::kElementsOffset));
        // Bind the deferred code patch site to be able to locate the
        // fixed array map comparison.  When debugging, we patch this
        // comparison to always fail so that we will hit the IC call
        // in the deferred code which will allow the debugger to
        // break for fast case stores.
        __ bind(deferred->patch_site());
        // Avoid using __ to ensure the distance from patch_site
        // to the map address is always the same.
        masm->movq(kScratchRegister, Factory::fixed_array_map(),
                   RelocInfo::EMBEDDED_OBJECT);
        __ cmpq(FieldOperand(tmp.reg(), HeapObject::kMapOffset),
                kScratchRegister);
        deferred->Branch(not_equal);

        // Store the value.
        SmiIndex index =
            masm->SmiToIndex(kScratchRegister, key.reg(), kPointerSizeLog2);
              __ movq(Operand(tmp.reg(),
                        index.reg,
                        index.scale,
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
        masm->nop();
        cgen_->frame()->Push(&answer);
      }
      cgen_->UnloadReference(this);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Clone the boilerplate in new space. Set the context to the
  // current context in rsi.
  Label gc;
  __ AllocateInNewSpace(JSFunction::kSize, rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the boilerplate function from the stack.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ movq(rcx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalContextOffset));
  __ movq(rcx, Operand(rcx, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ movq(FieldOperand(rax, JSObject::kMapOffset), rcx);

  // Clone the rest of the boilerplate fields. We don't have to update
  // the write barrier because the allocated object is in new space.
  for (int offset = kPointerSize;
       offset < JSFunction::kSize;
       offset += kPointerSize) {
    if (offset == JSFunction::kContextOffset) {
      __ movq(FieldOperand(rax, offset), rsi);
    } else {
      __ movq(rbx, FieldOperand(rdx, offset));
      __ movq(FieldOperand(rax, offset), rbx);
    }
  }

  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(rcx);  // Temporarily remove return address.
  __ pop(rdx);
  __ push(rsi);
  __ push(rdx);
  __ push(rcx);  // Restore return address.
  __ TailCallRuntime(ExternalReference(Runtime::kNewClosure), 2, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace((length * kPointerSize) + FixedArray::kHeaderSize,
                        rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));

  // Setup the object header.
  __ LoadRoot(kScratchRegister, Heap::kContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ movl(FieldOperand(rax, Array::kLengthOffset), Immediate(length));

  // Setup the fixed slots.
  __ xor_(rbx, rbx);  // Set to NULL.
  __ movq(Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)), rcx);
  __ movq(Operand(rax, Context::SlotOffset(Context::FCONTEXT_INDEX)), rax);
  __ movq(Operand(rax, Context::SlotOffset(Context::PREVIOUS_INDEX)), rbx);
  __ movq(Operand(rax, Context::SlotOffset(Context::EXTENSION_INDEX)), rbx);

  // Copy the global object from the surrounding context.
  __ movq(rbx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(Operand(rax, Context::SlotOffset(Context::GLOBAL_INDEX)), rbx);

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ movq(Operand(rax, Context::SlotOffset(i)), rbx);
  }

  // Return and remove the on-stack parameter.
  __ movq(rsi, rax);
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(ExternalReference(Runtime::kNewContext), 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + kPointerSize]: constant elements.
  // [rsp + (2 * kPointerSize)]: literal index.
  // [rsp + (3 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into rcx and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ movq(rcx, Operand(rsp, 3 * kPointerSize));
  __ movq(rax, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rcx,
          FieldOperand(rcx, index.reg, index.scale, FixedArray::kHeaderSize));
  __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
  __ j(equal, &slow_case);

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size, rax, rbx, rdx, &slow_case, TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ movq(rbx, FieldOperand(rcx, i));
      __ movq(FieldOperand(rax, i), rbx);
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ movq(rcx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ lea(rdx, Operand(rax, JSArray::kSize));
    __ movq(FieldOperand(rax, JSArray::kElementsOffset), rdx);

    // Copy the elements array.
    for (int i = 0; i < elements_size; i += kPointerSize) {
      __ movq(rbx, FieldOperand(rcx, i));
      __ movq(FieldOperand(rdx, i), rbx);
    }
  }

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  __ bind(&slow_case);
  ExternalReference runtime(Runtime::kCreateArrayLiteralShallow);
  __ TailCallRuntime(runtime, 3, 1);
}


void ToBooleanStub::Generate(MacroAssembler* masm) {
  Label false_result, true_result, not_string;
  __ movq(rax, Operand(rsp, 1 * kPointerSize));

  // 'null' => false.
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, &false_result);

  // Get the map and type of the heap object.
  // We don't use CmpObjectType because we manipulate the type field.
  __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbq(rcx, FieldOperand(rdx, Map::kInstanceTypeOffset));

  // Undetectable => false.
  __ movzxbq(rbx, FieldOperand(rdx, Map::kBitFieldOffset));
  __ and_(rbx, Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, &false_result);

  // JavaScript object => true.
  __ cmpq(rcx, Immediate(FIRST_JS_OBJECT_TYPE));
  __ j(above_equal, &true_result);

  // String value => false iff empty.
  __ cmpq(rcx, Immediate(FIRST_NONSTRING_TYPE));
  __ j(above_equal, &not_string);
  __ movl(rdx, FieldOperand(rax, String::kLengthOffset));
  __ testl(rdx, rdx);
  __ j(zero, &false_result);
  __ jmp(&true_result);

  __ bind(&not_string);
  // HeapNumber => false iff +0, -0, or NaN.
  // These three cases set C3 when compared to zero in the FPU.
  __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &true_result);
  __ fldz();  // Load zero onto fp stack
  // Load heap-number double value onto fp stack
  __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
  __ FCmp();
  __ j(zero, &false_result);
  // Fall through to |true_result|.

  // Return 1/0 for true/false in rax.
  __ bind(&true_result);
  __ movq(rax, Immediate(1));
  __ ret(1 * kPointerSize);
  __ bind(&false_result);
  __ xor_(rax, rax);
  __ ret(1 * kPointerSize);
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
          if (answer != 0 || (left + right) >= 0) {
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


// End of CodeGenerator implementation.

// Get the integer part of a heap number.  Surprisingly, all this bit twiddling
// is faster than using the built-in instructions on floating point registers.
// Trashes rdi and rbx.  Dest is rcx.  Source cannot be rcx or one of the
// trashed registers.
void IntegerConvert(MacroAssembler* masm,
                    Register source,
                    bool use_sse3,
                    Label* conversion_failure) {
  ASSERT(!source.is(rcx) && !source.is(rdi) && !source.is(rbx));
  Label done, right_exponent, normal_exponent;
  Register scratch = rbx;
  Register scratch2 = rdi;
  // Get exponent word.
  __ movl(scratch, FieldOperand(source, HeapNumber::kExponentOffset));
  // Get exponent alone in scratch2.
  __ movl(scratch2, scratch);
  __ and_(scratch2, Immediate(HeapNumber::kExponentMask));
  if (use_sse3) {
    CpuFeatures::Scope scope(SSE3);
    // Check whether the exponent is too big for a 64 bit signed integer.
    static const uint32_t kTooBigExponent =
        (HeapNumber::kExponentBias + 63) << HeapNumber::kExponentShift;
    __ cmpl(scratch2, Immediate(kTooBigExponent));
    __ j(greater_equal, conversion_failure);
    // Load x87 register with heap number.
    __ fld_d(FieldOperand(source, HeapNumber::kValueOffset));
    // Reserve space for 64 bit answer.
    __ subq(rsp, Immediate(sizeof(uint64_t)));  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(rsp, 0));
    __ movl(rcx, Operand(rsp, 0));  // Load low word of answer into rcx.
    __ addq(rsp, Immediate(sizeof(uint64_t)));  // Nolint.
  } else {
    // Load rcx with zero.  We use this either for the final shift or
    // for the answer.
    __ xor_(rcx, rcx);
    // Check whether the exponent matches a 32 bit signed int that cannot be
    // represented by a Smi.  A non-smi 32 bit integer is 1.xxx * 2^30 so the
    // exponent is 30 (biased).  This is the exponent that we are fastest at and
    // also the highest exponent we can handle here.
    const uint32_t non_smi_exponent =
        (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
    __ cmpl(scratch2, Immediate(non_smi_exponent));
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
      __ cmpl(scratch2, Immediate(big_non_smi_exponent));
      __ j(not_equal, conversion_failure);
      // We have the big exponent, typically from >>>.  This means the number is
      // in the range 2^31 to 2^32 - 1.  Get the top bits of the mantissa.
      __ movl(scratch2, scratch);
      __ and_(scratch2, Immediate(HeapNumber::kMantissaMask));
      // Put back the implicit 1.
      __ or_(scratch2, Immediate(1 << HeapNumber::kExponentShift));
      // Shift up the mantissa bits to take up the space the exponent used to
      // take. We just orred in the implicit bit so that took care of one and
      // we want to use the full unsigned range so we subtract 1 bit from the
      // shift distance.
      const int big_shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 1;
      __ shl(scratch2, Immediate(big_shift_distance));
      // Get the second half of the double.
      __ movl(rcx, FieldOperand(source, HeapNumber::kMantissaOffset));
      // Shift down 21 bits to get the most significant 11 bits or the low
      // mantissa word.
      __ shr(rcx, Immediate(32 - big_shift_distance));
      __ or_(rcx, scratch2);
      // We have the answer in rcx, but we may need to negate it.
      __ testl(scratch, scratch);
      __ j(positive, &done);
      __ neg(rcx);
      __ jmp(&done);
    }

    __ bind(&normal_exponent);
    // Exponent word in scratch, exponent part of exponent word in scratch2.
    // Zero in rcx.
    // We know the exponent is smaller than 30 (biased).  If it is less than
    // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
    // it rounds to zero.
    const uint32_t zero_exponent =
        (HeapNumber::kExponentBias + 0) << HeapNumber::kExponentShift;
    __ subl(scratch2, Immediate(zero_exponent));
    // rcx already has a Smi zero.
    __ j(less, &done);

    // We have a shifted exponent between 0 and 30 in scratch2.
    __ shr(scratch2, Immediate(HeapNumber::kExponentShift));
    __ movl(rcx, Immediate(30));
    __ subl(rcx, scratch2);

    __ bind(&right_exponent);
    // Here rcx is the shift, scratch is the exponent word.
    // Get the top bits of the mantissa.
    __ and_(scratch, Immediate(HeapNumber::kMantissaMask));
    // Put back the implicit 1.
    __ or_(scratch, Immediate(1 << HeapNumber::kExponentShift));
    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We have kExponentShift + 1 significant bits int he low end of the
    // word.  Shift them to the top bits.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    __ shl(scratch, Immediate(shift_distance));
    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    __ movl(scratch2, FieldOperand(source, HeapNumber::kMantissaOffset));
    // Shift down 22 bits to get the most significant 10 bits or the low
    // mantissa word.
    __ shr(scratch2, Immediate(32 - shift_distance));
    __ or_(scratch2, scratch);
    // Move down according to the exponent.
    __ shr_cl(scratch2);
    // Now the unsigned answer is in scratch2.  We need to move it to rcx and
    // we may need to fix the sign.
    Label negative;
    __ xor_(rcx, rcx);
    __ cmpl(rcx, FieldOperand(source, HeapNumber::kExponentOffset));
    __ j(greater, &negative);
    __ movl(rcx, scratch2);
    __ jmp(&done);
    __ bind(&negative);
    __ subl(rcx, scratch2);
    __ bind(&done);
  }
}


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done;

  if (op_ == Token::SUB) {
    // Check whether the value is a smi.
    Label try_float;
    __ JumpIfNotSmi(rax, &try_float);

    // Enter runtime system if the value of the smi is zero
    // to make sure that we switch between 0 and -0.
    // Also enter it if the value of the smi is Smi::kMinValue.
    __ SmiNeg(rax, rax, &done);

    // Either zero or Smi::kMinValue, neither of which become a smi when
    // negated.
    __ SmiCompare(rax, Smi::FromInt(0));
    __ j(not_equal, &slow);
    __ Move(rax, Factory::minus_zero_value());
    __ jmp(&done);

    // Try floating point case.
    __ bind(&try_float);
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &slow);
    // Operand is a float, negate its value by flipping sign bit.
    __ movq(rdx, FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(kScratchRegister, Immediate(0x01));
    __ shl(kScratchRegister, Immediate(63));
    __ xor_(rdx, kScratchRegister);  // Flip sign.
    // rdx is value to store.
    if (overwrite_) {
      __ movq(FieldOperand(rax, HeapNumber::kValueOffset), rdx);
    } else {
      __ AllocateHeapNumber(rcx, rbx, &slow);
      // rcx: allocated 'empty' number
      __ movq(FieldOperand(rcx, HeapNumber::kValueOffset), rdx);
      __ movq(rax, rcx);
    }
  } else if (op_ == Token::BIT_NOT) {
    // Check if the operand is a heap number.
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &slow);

    // Convert the heap number in rax to an untagged integer in rcx.
    IntegerConvert(masm, rax, CpuFeatures::IsSupported(SSE3), &slow);

    // Do the bitwise operation and check if the result fits in a smi.
    Label try_float;
    __ not_(rcx);
    // Tag the result as a smi and we're done.
    ASSERT(kSmiTagSize == 1);
    __ Integer32ToSmi(rax, rcx);
  }

  // Return from the stub.
  __ bind(&done);
  __ StubReturn(1);

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
  __ pop(rcx);  // pop return address
  __ push(rax);
  __ push(rcx);  // push return address
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
  //  esp[8]: last_match_info (expected JSArray)
  //  esp[16]: previous index
  //  esp[24]: subject string
  //  esp[32]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ movq(kScratchRegister, Operand(kScratchRegister, 0));
  __ testq(kScratchRegister, kScratchRegister);
  __ j(zero, &runtime);


  // Check that the first argument is a JSRegExp object.
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the RegExp has been compiled (data contains a fixed array).
  __ movq(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    Condition is_smi = masm->CheckSmi(rcx);
    __ Check(NegateCondition(is_smi),
        "Unexpected type for RegExp data, FixedArray expected");
    __ CmpObjectType(rcx, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Check(equal, "Unexpected type for RegExp data, FixedArray expected");
  }

  // rcx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ movq(rbx, FieldOperand(rcx, JSRegExp::kDataTagOffset));
  __ SmiCompare(rbx, Smi::FromInt(JSRegExp::IRREGEXP));
  __ j(not_equal, &runtime);

  // rcx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ movq(rdx, FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ PositiveSmiTimesPowerOfTwoToInteger64(rdx, rdx, 1);
  __ addq(rdx, Immediate(2));  // rdx was number_of_captures * 2.
  // Check that the static offsets vector buffer is large enough.
  __ cmpq(rdx, Immediate(OffsetsVector::kStaticOffsetsVectorSize));
  __ j(above, &runtime);

  // rcx: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the second argument is a string.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ JumpIfSmi(rax, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);
  // Get the length of the string to rbx.
  __ movl(rbx, FieldOperand(rax, String::kLengthOffset));

  // rbx: Length of subject string
  // rcx: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the third argument is a positive smi less than the string
  // length. A negative value will be greater (usigned comparison).
  __ movq(rax, Operand(rsp, kPreviousIndexOffset));
  __ SmiToInteger32(rax, rax);
  __ cmpl(rax, rbx);
  __ j(above, &runtime);

  // rcx: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_ARRAY_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ movq(rbx, FieldOperand(rax, JSArray::kElementsOffset));
  __ movq(rax, FieldOperand(rbx, HeapObject::kMapOffset));
  __ Cmp(rax, Factory::fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information. Ensure no overflow in add.
  ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
  __ movl(rax, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ addl(rdx, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmpl(rdx, rax);
  __ j(greater, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_string, seq_two_byte_string, check_code;
  const int kStringRepresentationEncodingMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ andb(rbx, Immediate(kStringRepresentationEncodingMask));
  // First check for sequential string.
  ASSERT_EQ(0, kStringTag);
  ASSERT_EQ(0, kSeqStringTag);
  __ testb(rbx, Immediate(kIsNotStringMask | kStringRepresentationMask));
  __ j(zero, &seq_string);

  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  __ movl(rdx, rbx);
  __ andb(rdx, Immediate(kStringRepresentationMask));
  __ cmpb(rdx, Immediate(kConsStringTag));
  __ j(not_equal, &runtime);
  __ movq(rdx, FieldOperand(rax, ConsString::kSecondOffset));
  __ Cmp(rdx, Factory::empty_string());
  __ j(not_equal, &runtime);
  __ movq(rax, FieldOperand(rax, ConsString::kFirstOffset));
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  ASSERT_EQ(0, kSeqStringTag);
  __ testb(rbx, Immediate(kStringRepresentationMask));
  __ j(not_zero, &runtime);
  __ andb(rbx, Immediate(kStringRepresentationEncodingMask));

  __ bind(&seq_string);
  // rax: subject string (sequential either ascii to two byte)
  // rbx: suject string type & kStringRepresentationEncodingMask
  // rcx: RegExp data (FixedArray)
  // Check that the irregexp code has been generated for an ascii string. If
  // it has, the field contains a code object otherwise it contains the hole.
  __ cmpb(rbx, Immediate(kStringTag | kSeqStringTag | kTwoByteStringTag));
  __ j(equal, &seq_two_byte_string);
  if (FLAG_debug_code) {
    __ cmpb(rbx, Immediate(kStringTag | kSeqStringTag | kAsciiStringTag));
    __ Check(equal, "Expected sequential ascii string");
  }
  __ movq(r12, FieldOperand(rcx, JSRegExp::kDataAsciiCodeOffset));
  __ Set(rdi, 1);  // Type is ascii.
  __ jmp(&check_code);

  __ bind(&seq_two_byte_string);
  // rax: subject string
  // rcx: RegExp data (FixedArray)
  __ movq(r12, FieldOperand(rcx, JSRegExp::kDataUC16CodeOffset));
  __ Set(rdi, 0);  // Type is two byte.

  __ bind(&check_code);
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CmpObjectType(r12, CODE_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);

  // rax: subject string
  // rdi: encoding of subject string (1 if ascii, 0 if two_byte);
  // r12: code
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ movq(rbx, Operand(rsp, kPreviousIndexOffset));
  __ SmiToInteger64(rbx, rbx);  // Previous index from smi.

  // rax: subject string
  // rbx: previous index
  // rdi: encoding of subject string (1 if ascii 0 if two_byte);
  // r12: code
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1);

  // rsi is caller save on Windows and used to pass parameter on Linux.
  __ push(rsi);

  static const int kRegExpExecuteArguments = 7;
  __ PrepareCallCFunction(kRegExpExecuteArguments);
  int argument_slots_on_stack =
      masm->ArgumentStackSlotsForCFunctionCall(kRegExpExecuteArguments);

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kPointerSize),
          Immediate(1));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ movq(kScratchRegister, address_of_regexp_stack_memory_address);
  __ movq(r9, Operand(kScratchRegister, 0));
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ addq(r9, Operand(kScratchRegister, 0));
  // Argument 6 passed in r9 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 2) * kPointerSize), r9);
#endif

  // Argument 5: static offsets vector buffer.
  __ movq(r8, ExternalReference::address_of_static_offsets_vector());
  // Argument 5 passed in r8 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 3) * kPointerSize), r8);
#endif

  // First four arguments are passed in registers on both Linux and Windows.
#ifdef _WIN64
  Register arg4 = r9;
  Register arg3 = r8;
  Register arg2 = rdx;
  Register arg1 = rcx;
#else
  Register arg4 = rcx;
  Register arg3 = rdx;
  Register arg2 = rsi;
  Register arg1 = rdi;
#endif

  // Keep track on aliasing between argX defined above and the registers used.
  // rax: subject string
  // rbx: previous index
  // rdi: encoding of subject string (1 if ascii 0 if two_byte);
  // r12: code

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label setup_two_byte, setup_rest;
  __ testb(rdi, rdi);
  __ movl(rdi, FieldOperand(rax, String::kLengthOffset));
  __ j(zero, &setup_two_byte);
  __ lea(arg4, FieldOperand(rax, rdi, times_1, SeqAsciiString::kHeaderSize));
  __ lea(arg3, FieldOperand(rax, rbx, times_1, SeqAsciiString::kHeaderSize));
  __ jmp(&setup_rest);
  __ bind(&setup_two_byte);
  __ lea(arg4, FieldOperand(rax, rdi, times_2, SeqTwoByteString::kHeaderSize));
  __ lea(arg3, FieldOperand(rax, rbx, times_2, SeqTwoByteString::kHeaderSize));

  __ bind(&setup_rest);
  // Argument 2: Previous index.
  __ movq(arg2, rbx);

  // Argument 1: Subject string.
  __ movq(arg1, rax);

  // Locate the code entry and call it.
  __ addq(r12, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ CallCFunction(r12, kRegExpExecuteArguments);

  // rsi is caller save, as it is used to pass parameter.
  __ pop(rsi);

  // Check the result.
  Label success;
  __ cmpq(rax, Immediate(NativeRegExpMacroAssembler::SUCCESS));
  __ j(equal, &success);
  Label failure;
  __ cmpq(rax, Immediate(NativeRegExpMacroAssembler::FAILURE));
  __ j(equal, &failure);
  __ cmpq(rax, Immediate(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592) Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ movq(kScratchRegister, pending_exception_address);
  __ Cmp(kScratchRegister, Factory::the_hole_value());
  __ j(equal, &runtime);
  __ bind(&failure);
  // For failure and exception return null.
  __ Move(rax, Factory::null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ movq(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  __ movq(rdx, FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ PositiveSmiTimesPowerOfTwoToInteger64(rdx, rdx, 1);
  __ addq(rdx, Immediate(2));  // rdx was number_of_captures * 2.

  // rdx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ movq(rbx, FieldOperand(rax, JSArray::kElementsOffset));

  // rbx: last_match_info backing store (FixedArray)
  // rdx: number of capture registers
  // Store the capture count.
  __ Integer32ToSmi(kScratchRegister, rdx);
  __ movq(FieldOperand(rbx, RegExpImpl::kLastCaptureCountOffset),
          kScratchRegister);
  // Store last subject and last input.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastSubjectOffset), rax);
  __ movq(rcx, rbx);
  __ RecordWrite(rcx, RegExpImpl::kLastSubjectOffset, rax, rdi);
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastInputOffset), rax);
  __ movq(rcx, rbx);
  __ RecordWrite(rcx, RegExpImpl::kLastInputOffset, rax, rdi);

  // Get the static offsets vector filled by the native regexp code.
  __ movq(rcx, ExternalReference::address_of_static_offsets_vector());

  // rbx: last_match_info backing store (FixedArray)
  // rcx: offsets vector
  // rdx: number of capture registers
  Label next_capture, done;
  __ movq(rax, Operand(rsp, kPreviousIndexOffset));
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ subq(rdx, Immediate(1));
  __ j(negative, &done);
  // Read the value from the static offsets vector buffer and make it a smi.
  __ movl(rdi, Operand(rcx, rdx, times_int_size, 0));
  __ Integer32ToSmi(rdi, rdi, &runtime);
  // Add previous index (from its stack slot) if value is not negative.
  Label capture_negative;
  // Negative flag set by smi convertion above.
  __ j(negative, &capture_negative);
  __ SmiAdd(rdi, rdi, rax, &runtime);  // Add previous index.
  __ bind(&capture_negative);
  // Store the smi value in the last match info.
  __ movq(FieldOperand(rbx,
                       rdx,
                       times_pointer_size,
                       RegExpImpl::kFirstCaptureOffset),
                       rdi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kRegExpExec), 4, 1);
#endif  // V8_NATIVE_REGEXP
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
      __ cmpq(rax, rdx);
      __ j(not_equal, &not_identical);
      // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
      // so we do the second best thing - test it ourselves.

      if (never_nan_nan_) {
        __ xor_(rax, rax);
        __ ret(0);
      } else {
        Label return_equal;
        Label heap_number;
        // If it's not a heap number, then return equal.
        __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
               Factory::heap_number_map());
        __ j(equal, &heap_number);
        __ bind(&return_equal);
        __ xor_(rax, rax);
        __ ret(0);

        __ bind(&heap_number);
        // It is a heap number, so return non-equal if it's NaN and equal if
        // it's not NaN.
        // The representation of NaN values has all exponent bits (52..62) set,
        // and not all mantissa bits (0..51) clear.
        // We only allow QNaNs, which have bit 51 set (which also rules out
        // the value being Infinity).

        // Value is a QNaN if value & kQuietNaNMask == kQuietNaNMask, i.e.,
        // all bits in the mask are set. We only need to check the word
        // that contains the exponent and high bit of the mantissa.
        ASSERT_NE(0, (kQuietNaNHighBitsMask << 1) & 0x80000000u);
        __ movl(rdx, FieldOperand(rdx, HeapNumber::kExponentOffset));
        __ xorl(rax, rax);
        __ addl(rdx, rdx);  // Shift value and mask so mask applies to top bits.
        __ cmpl(rdx, Immediate(kQuietNaNHighBitsMask << 1));
        __ setcc(above_equal, rax);
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
        __ SelectNonSmi(rbx, rax, rdx, &not_smis);

        // Check if the non-smi operand is a heap number.
        __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
               Factory::heap_number_map());
        // If heap number, handle it in the slow case.
        __ j(equal, &slow);
        // Return non-equal.  ebx (the lower half of rbx) is not zero.
        __ movq(rax, rbx);
        __ ret(0);

        __ bind(&not_smis);
      }

      // If either operand is a JSObject or an oddball value, then they are not
      // equal since their pointers are different
      // There is no test for undetectability in strict equality.

      // If the first object is a JS object, we have done pointer comparison.
      ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      Label first_non_object;
      __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(below, &first_non_object);
      // Return non-zero (eax (not rax) is not zero)
      Label return_not_equal;
      ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ CmpObjectType(rdx, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(above_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Push arguments below the return address to prepare jump to builtin.
  __ pop(rcx);
  __ push(rax);
  __ push(rdx);
  __ push(rcx);

  // Inlined floating point compare.
  // Call builtin if operands are not floating point or smi.
  Label check_for_symbols;
  // Push arguments on stack, for helper functions.
  FloatingPointHelper::CheckNumberOperands(masm, &check_for_symbols);
  FloatingPointHelper::LoadFloatOperands(masm, rax, rdx);
  __ FCmp();

  // Jump to builtin for NaN.
  __ j(parity_even, &call_builtin);

  // TODO(1243847): Use cmov below once CpuFeatures are properly hooked up.
  Label below_lbl, above_lbl;
  // use rdx, rax to convert unsigned to signed comparison
  __ j(below, &below_lbl);
  __ j(above, &above_lbl);

  __ xor_(rax, rax);  // equal
  __ ret(2 * kPointerSize);

  __ bind(&below_lbl);
  __ movq(rax, Immediate(-1));
  __ ret(2 * kPointerSize);

  __ bind(&above_lbl);
  __ movq(rax, Immediate(1));
  __ ret(2 * kPointerSize);  // rax, rdx were pushed

  // Fast negative check for symbol-to-symbol equality.
  __ bind(&check_for_symbols);
  Label check_for_strings;
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &check_for_strings, rax, kScratchRegister);
    BranchIfNonSymbol(masm, &check_for_strings, rdx, kScratchRegister);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax (not rax) already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(2 * kPointerSize);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(rdx, rax, rcx, rbx, &call_builtin);

  // Inline comparison of ascii strings.
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     rdx,
                                                     rax,
                                                     rcx,
                                                     rbx,
                                                     rdi,
                                                     r8);

#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&call_builtin);
  // must swap argument order
  __ pop(rcx);
  __ pop(rdx);
  __ pop(rax);
  __ push(rdx);
  __ push(rax);

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
    __ Push(Smi::FromInt(ncr));
  }

  // Restore return address on the stack.
  __ push(rcx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);
}


void CompareStub::BranchIfNonSymbol(MacroAssembler* masm,
                                    Label* label,
                                    Register object,
                                    Register scratch) {
  __ JumpIfSmi(object, label);
  __ movq(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzxbq(scratch,
             FieldOperand(scratch, Map::kInstanceTypeOffset));
  // Ensure that no non-strings have the symbol bit set.
  ASSERT(kNotStringTag + kIsSymbolMask > LAST_TYPE);
  ASSERT(kSymbolTag != 0);
  __ testb(scratch, Immediate(kIsSymbolMask));
  __ j(zero, label);
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


void InstanceofStub::Generate(MacroAssembler* masm) {
  // Implements "value instanceof function" operator.
  // Expected input state:
  //   rsp[0] : return address
  //   rsp[1] : function pointer
  //   rsp[2] : value

  // Get the object - go slow case if it's a smi.
  Label slow;
  __ movq(rax, Operand(rsp, 2 * kPointerSize));
  __ JumpIfSmi(rax, &slow);

  // Check that the left hand is a JS object. Leave its map in rax.
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rax);
  __ j(below, &slow);
  __ CmpInstanceType(rax, LAST_JS_OBJECT_TYPE);
  __ j(above, &slow);

  // Get the prototype of the function.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));
  __ TryGetFunctionPrototype(rdx, rbx, &slow);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(rbx, &slow);
  __ CmpObjectType(rbx, FIRST_JS_OBJECT_TYPE, kScratchRegister);
  __ j(below, &slow);
  __ CmpInstanceType(kScratchRegister, LAST_JS_OBJECT_TYPE);
  __ j(above, &slow);

  // Register mapping: rax is object map and rbx is function prototype.
  __ movq(rcx, FieldOperand(rax, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  Label loop, is_instance, is_not_instance;
  __ LoadRoot(kScratchRegister, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmpq(rcx, rbx);
  __ j(equal, &is_instance);
  __ cmpq(rcx, kScratchRegister);
  __ j(equal, &is_not_instance);
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rcx, FieldOperand(rcx, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  __ xorl(rax, rax);
  __ ret(2 * kPointerSize);

  __ bind(&is_not_instance);
  __ movl(rax, Immediate(1));
  __ ret(2 * kPointerSize);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // rsp[0] : return address
  // rsp[8] : number of parameters
  // rsp[16] : receiver displacement
  // rsp[24] : function

  // The displacement is used for skipping the return address and the
  // frame pointer on the stack. It is the offset of the last
  // parameter (if any) relative to the frame pointer.
  static const int kDisplacement = 2 * kPointerSize;

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rdx, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ movq(rcx, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ movq(Operand(rsp, 1 * kPointerSize), rcx);
  // Do not clobber the length index for the indexing operation since
  // it is used compute the size for allocation later.
  SmiIndex index = masm->SmiToIndex(rbx, rcx, kPointerSizeLog2);
  __ lea(rdx, Operand(rdx, index.reg, index.scale, kDisplacement));
  __ movq(Operand(rsp, 2 * kPointerSize), rdx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ testq(rcx, rcx);
  __ j(zero, &add_arguments_object);
  index = masm->SmiToIndex(rcx, rcx, kPointerSizeLog2);
  __ lea(rcx, Operand(index.reg, index.scale, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ addq(rcx, Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(rcx, rax, rdx, rbx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kGlobalContextOffset));
  __ movq(rdi, Operand(rdi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ movq(kScratchRegister, FieldOperand(rdi, i));
    __ movq(FieldOperand(rax, i), kScratchRegister);
  }

  // Setup the callee in-object property.
  ASSERT(Heap::arguments_callee_index == 0);
  __ movq(kScratchRegister, Operand(rsp, 3 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize), kScratchRegister);

  // Get the length (smi tagged) and set that as an in-object property too.
  ASSERT(Heap::arguments_length_index == 1);
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize + kPointerSize), rcx);

  // If there are no actual arguments, we're done.
  Label done;
  __ testq(rcx, rcx);
  __ j(zero, &done);

  // Get the parameters pointer from the stack and untag the length.
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));
  __ SmiToInteger32(rcx, rcx);

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ movq(kScratchRegister, Operand(rdx, -1 * kPointerSize));  // Skip receiver.
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize), kScratchRegister);
  __ addq(rdi, Immediate(kPointerSize));
  __ subq(rdx, Immediate(kPointerSize));
  __ decq(rcx);
  __ j(not_zero, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kNewArgumentsFast), 3, 1);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in rdx and the parameter count is in rax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(rdx, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rbx, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register rax. Use unsigned comparison to get negative
  // check for free.
  __ cmpq(rdx, rax);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ lea(rbx, Operand(rbp, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ movq(rcx, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmpq(rdx, rcx);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  index = masm->SmiToIndex(rax, rcx, kPointerSizeLog2);
  __ lea(rbx, Operand(rbx, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ pop(rbx);  // Return address.
  __ push(rdx);
  __ push(rbx);
  Runtime::Function* f =
      Runtime::FunctionForId(Runtime::kGetArgumentsProperty);
  __ TailCallRuntime(ExternalReference(f), 1, f->result_size);
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rdx, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame and return it.
  // Otherwise nothing to do: The number of formal parameters has already been
  // passed in register eax by calling function. Just return it.
  __ cmovq(equal, rax,
           Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ ret(0);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // Check that stack should contain next handler, frame pointer, state and
  // return address in that order.
  ASSERT_EQ(StackHandlerConstants::kFPOffset + kPointerSize,
            StackHandlerConstants::kStateOffset);
  ASSERT_EQ(StackHandlerConstants::kStateOffset + kPointerSize,
            StackHandlerConstants::kPCOffset);

  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rsp, Operand(kScratchRegister, 0));
  // get next in chain
  __ pop(rcx);
  __ movq(Operand(kScratchRegister, 0), rcx);
  __ pop(rbp);  // pop frame pointer
  __ pop(rdx);  // remove state

  // Before returning we restore the context from the frame pointer if not NULL.
  // The frame pointer is NULL in the exception handler of a JS entry frame.
  __ xor_(rsi, rsi);  // tentatively set context pointer to NULL
  Label skip;
  __ cmpq(rbp, Immediate(0));
  __ j(equal, &skip);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);
  __ ret(0);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope) {
  // rax: result parameter for PerformGC, if any.
  // rbx: pointer to C function  (C callee-saved).
  // rbp: frame pointer  (restored after C call).
  // rsp: stack pointer  (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: pointer to the first argument (C callee-saved).
  //      This pointer is reused in LeaveExitFrame(), so it is stored in a
  //      callee-saved register.

  // Simple results returned in rax (both AMD64 and Win64 calling conventions).
  // Complex results must be written to address passed as first argument.
  // AMD64 calling convention: a struct of two pointers in rax+rdx

  if (do_gc) {
    // Pass failure code returned from last attempt as first argument to GC.
#ifdef _WIN64
    __ movq(rcx, rax);
#else  // ! defined(_WIN64)
    __ movq(rdi, rax);
#endif
    __ movq(kScratchRegister,
            FUNCTION_ADDR(Runtime::PerformGC),
            RelocInfo::RUNTIME_ENTRY);
    __ call(kScratchRegister);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ incl(Operand(kScratchRegister, 0));
  }

  // Call C function.
#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9
  // Store Arguments object on stack, below the 4 WIN64 ABI parameter slots.
  __ movq(Operand(rsp, 4 * kPointerSize), r14);  // argc.
  __ movq(Operand(rsp, 5 * kPointerSize), r15);  // argv.
  if (result_size_ < 2) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax).
    __ lea(rcx, Operand(rsp, 4 * kPointerSize));
  } else {
    ASSERT_EQ(2, result_size_);
    // Pass a pointer to the result location as the first argument.
    __ lea(rcx, Operand(rsp, 6 * kPointerSize));
    // Pass a pointer to the Arguments object as the second argument.
    __ lea(rdx, Operand(rsp, 4 * kPointerSize));
  }

#else  // ! defined(_WIN64)
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9.
  __ movq(rdi, r14);  // argc.
  __ movq(rsi, r15);  // argv.
#endif
  __ call(rbx);
  // Result is in rax - do not destroy this register!

  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ decl(Operand(kScratchRegister, 0));
  }

  // Check for failure result.
  Label failure_returned;
  ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
#ifdef _WIN64
  // If return value is on the stack, pop it to registers.
  if (result_size_ > 1) {
    ASSERT_EQ(2, result_size_);
    // Read result values stored on stack. Result is stored
    // above the four argument mirror slots and the two
    // Arguments object slots.
    __ movq(rax, Operand(rsp, 6 * kPointerSize));
    __ movq(rdx, Operand(rsp, 7 * kPointerSize));
  }
#endif
  __ lea(rcx, Operand(rax, 1));
  // Lower 2 bits of rcx are 0 iff rax has failure tag.
  __ testl(rcx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned);

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(mode_, result_size_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ testl(rax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry);

  // Special handling of out of memory exceptions.
  __ movq(kScratchRegister, Failure::OutOfMemoryException(), RelocInfo::NONE);
  __ cmpq(rax, kScratchRegister);
  __ j(equal, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ movq(kScratchRegister, pending_exception_address);
  __ movq(rax, Operand(kScratchRegister, 0));
  __ movq(rdx, ExternalReference::the_hole_value_location());
  __ movq(rdx, Operand(rdx, 0));
  __ movq(Operand(kScratchRegister, 0), rdx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  // Fetch top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rsp, Operand(kScratchRegister, 0));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  __ cmpq(Operand(rsp, kStateOffset), Immediate(StackHandler::ENTRY));
  __ j(equal, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  __ movq(rsp, Operand(rsp, kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  __ movq(kScratchRegister, handler_address);
  __ pop(Operand(kScratchRegister, 0));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(Top::k_external_caught_exception_address);
    __ movq(rax, Immediate(false));
    __ store_rax(external_caught);

    // Set pending exception and rax to out of memory exception.
    ExternalReference pending_exception(Top::k_pending_exception_address);
    __ movq(rax, Failure::OutOfMemoryException(), RelocInfo::NONE);
    __ store_rax(pending_exception);
  }

  // Clear the context pointer.
  __ xor_(rsi, rsi);

  // Restore registers from handler.
  ASSERT_EQ(StackHandlerConstants::kNextOffset + kPointerSize,
            StackHandlerConstants::kFPOffset);
  __ pop(rbp);  // FP
  ASSERT_EQ(StackHandlerConstants::kFPOffset + kPointerSize,
            StackHandlerConstants::kStateOffset);
  __ pop(rdx);  // State

  ASSERT_EQ(StackHandlerConstants::kStateOffset + kPointerSize,
            StackHandlerConstants::kPCOffset);
  __ ret(0);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // +1 ~ return address
    Label receiver_is_value, receiver_is_js_object;
    __ movq(rax, Operand(rsp, (argc_ + 1) * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ JumpIfSmi(rax, &receiver_is_value);

    // Check if the receiver is a valid JS object.
    __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rdi);
    __ j(above_equal, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(rax);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ LeaveInternalFrame();
    __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rax);

    __ bind(&receiver_is_js_object);
  }

  // Get the function to call from the stack.
  // +2 ~ receiver, return address
  __ movq(rdi, Operand(rsp, (argc_ + 2) * kPointerSize));

  // Check that the function really is a JavaScript function.
  __ JumpIfSmi(rdi, &slow);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rdi);
  __ Set(rax, argc_);
  __ Set(rbx, 0);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
  __ Jump(adaptor, RelocInfo::CODE_TARGET);
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(mode_, result_size_);

  // rax: Holds the context at this point, but should not be used.
  //      On entry to code generated by GenerateCore, it must hold
  //      a failure result if the collect_garbage argument to GenerateCore
  //      is true.  This failure result can be the result of code
  //      generated by a previous call to GenerateCore.  The value
  //      of rax is then passed to Runtime::PerformGC.
  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: argv pointer (C callee-saved).

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
  __ movq(rax, failure, RelocInfo::NONE);
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


void ApiGetterEntryStub::Generate(MacroAssembler* masm) {
  UNREACHABLE();
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, exit;
#ifdef ENABLE_LOGGING_AND_PROFILING
  Label not_outermost_js, not_outermost_js_2;
#endif

  // Setup frame.
  __ push(rbp);
  __ movq(rbp, rsp);

  // Push the stack frame type marker twice.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ Push(Smi::FromInt(marker));  // context slot
  __ Push(Smi::FromInt(marker));  // function slot
  // Save callee-saved registers (X64 calling conventions).
  __ push(r12);
  __ push(r13);
  __ push(r14);
  __ push(r15);
  __ push(rdi);
  __ push(rsi);
  __ push(rbx);
  // TODO(X64): Push XMM6-XMM15 (low 64 bits) as well, or make them
  // callee-save in JS code as well.

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Top::k_c_entry_fp_address);
  __ load_rax(c_entry_fp);
  __ push(rax);

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Top::k_js_entry_sp_address);
  __ load_rax(js_entry_sp);
  __ testq(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ movq(rax, rbp);
  __ store_rax(js_entry_sp);
  __ bind(&not_outermost_js);
#endif

  // Call a faked try-block that does the invoke.
  __ call(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ store_rax(pending_exception);
  __ movq(rax, Failure::Exception(), RelocInfo::NONE);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);

  // Clear any pending exceptions.
  __ load_rax(ExternalReference::the_hole_value_location());
  __ store_rax(pending_exception);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline
  // builtin and pop the faked function when we return. We load the address
  // from an external reference instead of inlining the call target address
  // directly in the code, because the builtin stubs may not have been
  // generated yet at the time this code is generated.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ load_rax(construct_entry);
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ load_rax(entry);
  }
  __ lea(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  __ pop(Operand(kScratchRegister, 0));
  // Pop next_sp.
  __ addq(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If current EBP value is the same as js_entry_sp value, it means that
  // the current function is the outermost.
  __ movq(kScratchRegister, js_entry_sp);
  __ cmpq(rbp, Operand(kScratchRegister, 0));
  __ j(not_equal, &not_outermost_js_2);
  __ movq(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);
#endif

  // Restore the top frame descriptor from the stack.
  __ bind(&exit);
  __ movq(kScratchRegister, ExternalReference(Top::k_c_entry_fp_address));
  __ pop(Operand(kScratchRegister, 0));

  // Restore callee-saved registers (X64 conventions).
  __ pop(rbx);
  __ pop(rsi);
  __ pop(rdi);
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ addq(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(rbp);
  __ ret(0);
}


// -----------------------------------------------------------------------------
// Implementation of stubs.

//  Stub classes have public member named masm, not masm_.

void StackCheckStub::Generate(MacroAssembler* masm) {
  // Because builtins always remove the receiver from the stack, we
  // have to fake one to avoid underflowing the stack. The receiver
  // must be inserted below the return address on the stack so we
  // temporarily store that in a register.
  __ pop(rax);
  __ Push(Smi::FromInt(0));
  __ push(rax);

  // Do tail-call to runtime routine.
  Runtime::Function* f = Runtime::FunctionForId(Runtime::kStackGuard);
  __ TailCallRuntime(ExternalReference(f), 1, f->result_size);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ JumpIfSmi(number, &load_smi);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi);
  __ SmiToInteger32(number, number);
  __ push(number);
  __ fild_s(Operand(rsp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register src,
                                           XMMRegister dst) {
  Label load_smi, done;

  __ JumpIfSmi(src, &load_smi);
  __ movsd(dst, FieldOperand(src, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi);
  __ SmiToInteger32(src, src);
  __ cvtlsi2sd(dst, src);

  __ bind(&done);
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            XMMRegister dst1,
                                            XMMRegister dst2) {
  __ movq(kScratchRegister, rdx);
  LoadFloatOperand(masm, kScratchRegister, dst1);
  __ movq(kScratchRegister, rax);
  LoadFloatOperand(masm, kScratchRegister, dst2);
}


void FloatingPointHelper::LoadFloatOperandsFromSmis(MacroAssembler* masm,
                                                    XMMRegister dst1,
                                                    XMMRegister dst2) {
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(dst1, kScratchRegister);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(dst2, kScratchRegister);
}


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         bool use_sse3,
                                         Label* conversion_failure) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  __ JumpIfNotSmi(rdx, &arg1_is_object);
  __ SmiToInteger32(rdx, rdx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ movl(rdx, Immediate(0));
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ CompareRoot(rbx, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &check_undefined_arg1);
  // Get the untagged integer version of the edx heap number in rcx.
  IntegerConvert(masm, rdx, use_sse3, conversion_failure);
  __ movl(rdx, rcx);

  // Here edx has the untagged integer, eax has a Smi or a heap number.
  __ bind(&load_arg2);
  // Test if arg2 is a Smi.
  __ JumpIfNotSmi(rax, &arg2_is_object);
  __ SmiToInteger32(rax, rax);
  __ movl(rcx, rax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ movl(rcx, Immediate(0));
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ CompareRoot(rbx, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the eax heap number in ecx.
  IntegerConvert(masm, rax, use_sse3, conversion_failure);
  __ bind(&done);
  __ movl(rax, rdx);
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            Register lhs,
                                            Register rhs) {
  Label load_smi_lhs, load_smi_rhs, done_load_lhs, done;
  __ JumpIfSmi(lhs, &load_smi_lhs);
  __ fld_d(FieldOperand(lhs, HeapNumber::kValueOffset));
  __ bind(&done_load_lhs);

  __ JumpIfSmi(rhs, &load_smi_rhs);
  __ fld_d(FieldOperand(rhs, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_lhs);
  __ SmiToInteger64(kScratchRegister, lhs);
  __ push(kScratchRegister);
  __ fild_d(Operand(rsp, 0));
  __ pop(kScratchRegister);
  __ jmp(&done_load_lhs);

  __ bind(&load_smi_rhs);
  __ SmiToInteger64(kScratchRegister, rhs);
  __ push(kScratchRegister);
  __ fild_d(Operand(rsp, 0));
  __ pop(kScratchRegister);

  __ bind(&done);
}


void FloatingPointHelper::CheckNumberOperands(MacroAssembler* masm,
                                              Label* non_float) {
  Label test_other, done;
  // Test if both operands are numbers (heap_numbers or smis).
  // If not, jump to label non_float.
  __ JumpIfSmi(rdx, &test_other);  // argument in rdx is OK
  __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(not_equal, non_float);  // The argument in rdx is not a number.

  __ bind(&test_other);
  __ JumpIfSmi(rax, &done);  // argument in rax is OK
  __ Cmp(FieldOperand(rax, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(not_equal, non_float);  // The argument in rax is not a number.

  // Fall-through: Both operands are numbers.
  __ bind(&done);
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
               "GenericBinaryOpStub_%s_%s%s_%s%s_%s%s",
               op_name,
               overwrite_name,
               (flags_ & NO_SMI_CODE_IN_STUB) ? "_NoSmiInStub" : "",
               args_in_registers_ ? "RegArgs" : "StackArgs",
               args_reversed_ ? "_R" : "",
               use_sse3_ ? "SSE3" : "SSE2",
               NumberInfo::ToString(operands_type_));
  return name_;
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
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (!(left.is(left_arg) && right.is(right_arg))) {
      if (left.is(right_arg) && right.is(left_arg)) {
        if (IsOperationCommutative()) {
          SetArgsReversed();
        } else {
          __ xchg(left, right);
        }
      } else if (left.is(left_arg)) {
        __ movq(right_arg, right);
      } else if (right.is(right_arg)) {
        __ movq(left_arg, left);
      } else if (left.is(right_arg)) {
        if (IsOperationCommutative()) {
          __ movq(left_arg, right);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying left argument.
          __ movq(left_arg, left);
          __ movq(right_arg, right);
        }
      } else if (right.is(left_arg)) {
        if (IsOperationCommutative()) {
          __ movq(right_arg, left);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying right argument.
          __ movq(right_arg, right);
          __ movq(left_arg, left);
        }
      } else {
        // Order of moves is not important.
        __ movq(left_arg, left);
        __ movq(right_arg, right);
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
    __ Push(right);
  } else {
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (left.is(left_arg)) {
      __ Move(right_arg, right);
    } else if (left.is(right_arg) && IsOperationCommutative()) {
      __ Move(left_arg, right);
      SetArgsReversed();
    } else {
      // For non-commutative operations, left and right_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite left before moving
      // it to left_arg.
      __ movq(left_arg, left);
      __ Move(right_arg, right);
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
    __ Push(left);
    __ push(right);
  } else {
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (right.is(right_arg)) {
      __ Move(left_arg, left);
    } else if (right.is(left_arg) && IsOperationCommutative()) {
      __ Move(right_arg, left);
      SetArgsReversed();
    } else {
      // For non-commutative operations, right and left_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite right before moving
      // it to right_arg.
      __ movq(right_arg, right);
      __ Move(left_arg, left);
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
  Register left = rdx;
  Register right = rax;
  if (op_ == Token::DIV || op_ == Token::MOD) {
    left = rax;
    right = rbx;
    if (HasArgsInRegisters()) {
      __ movq(rbx, rax);
      __ movq(rax, rdx);
    }
  }
  if (!HasArgsInRegisters()) {
    __ movq(right, Operand(rsp, 1 * kPointerSize));
    __ movq(left, Operand(rsp, 2 * kPointerSize));
  }

  // 2. Smi check both operands. Skip the check for OR as it is better combined
  // with the actual operation.
  Label not_smis;
  if (op_ != Token::BIT_OR) {
    Comment smi_check_comment(masm, "-- Smi check arguments");
    __ JumpIfNotBothSmi(left, right, &not_smis);
  }

  // 3. Operands are both smis (except for OR), perform the operation leaving
  // the result in rax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op_) {
    case Token::ADD: {
      ASSERT(right.is(rax));
      __ SmiAdd(right, right, left, &use_fp_on_smis);  // ADD is commutative.
      break;
    }

    case Token::SUB: {
      __ SmiSub(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;
    }

    case Token::MUL:
      ASSERT(right.is(rax));
      __ SmiMul(right, right, left, &use_fp_on_smis);  // MUL is commutative.
      break;

    case Token::DIV:
      ASSERT(left.is(rax));
      __ SmiDiv(left, left, right, &use_fp_on_smis);
      break;

    case Token::MOD:
      ASSERT(left.is(rax));
      __ SmiMod(left, left, right, slow);
      break;

    case Token::BIT_OR:
      ASSERT(right.is(rax));
      __ movq(rcx, right);  // Save the right operand.
      __ SmiOr(right, right, left);  // BIT_OR is commutative.
      __ testb(right, Immediate(kSmiTagMask));
      __ j(not_zero, &not_smis);
      break;

    case Token::BIT_AND:
      ASSERT(right.is(rax));
      __ SmiAnd(right, right, left);  // BIT_AND is commutative.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(rax));
      __ SmiXor(right, right, left);  // BIT_XOR is commutative.
      break;

    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      switch (op_) {
        case Token::SAR:
          __ SmiShiftArithmeticRight(left, left, right);
          break;
        case Token::SHR:
          __ SmiShiftLogicalRight(left, left, right, slow);
          break;
        case Token::SHL:
          __ SmiShiftLeft(left, left, right, slow);
          break;
        default:
          UNREACHABLE();
      }
      __ movq(rax, left);
      break;

    default:
      UNREACHABLE();
      break;
  }

  // 4. Emit return of result in eax.
  GenerateReturn(masm);

  // 5. For some operations emit inline code to perform floating point
  // operations on known smis (e.g., if the result of the operation
  // overflowed the smi range).
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      __ bind(&use_fp_on_smis);
      if (op_ == Token::DIV) {
        __ movq(rdx, rax);
        __ movq(rax, rbx);
      }
      // left is rdx, right is rax.
      __ AllocateHeapNumber(rbx, rcx, slow);
      FloatingPointHelper::LoadFloatOperandsFromSmis(masm, xmm4, xmm5);
      switch (op_) {
        case Token::ADD: __ addsd(xmm4, xmm5); break;
        case Token::SUB: __ subsd(xmm4, xmm5); break;
        case Token::MUL: __ mulsd(xmm4, xmm5); break;
        case Token::DIV: __ divsd(xmm4, xmm5); break;
        default: UNREACHABLE();
      }
      __ movsd(FieldOperand(rbx, HeapNumber::kValueOffset), xmm4);
      __ movq(rax, rbx);
      GenerateReturn(masm);
    }
    default:
      break;
  }

  // 6. Non-smi operands, fall out to the non-smi code with the operands in
  // rdx and rax.
  Comment done_comment(masm, "-- Enter non-smi code");
  __ bind(&not_smis);

  switch (op_) {
    case Token::DIV:
    case Token::MOD:
      // Operands are in rax, rbx at this point.
      __ movq(rdx, rax);
      __ movq(rax, rbx);
      break;

    case Token::BIT_OR:
      // Right operand is saved in rcx and rax was destroyed by the smi
      // operation.
      __ movq(rax, rcx);
      break;

    default:
      break;
  }
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  Label call_runtime;
  if (HasSmiCodeInStub()) {
    GenerateSmiCode(masm, &call_runtime);
  } else if (op_ != Token::MOD) {
    GenerateLoadArguments(masm);
  }
  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      // rax: y
      // rdx: x
      if (NumberInfo::IsNumber(operands_type_)) {
        if (FLAG_debug_code) {
          // Assert at runtime that inputs are only numbers.
          __ AbortIfNotNumber(rdx, "GenericBinaryOpStub operand not a number.");
          __ AbortIfNotNumber(rax, "GenericBinaryOpStub operand not a number.");
        }
      } else {
        FloatingPointHelper::CheckNumberOperands(masm, &call_runtime);
      }
      // Fast-case: Both operands are numbers.
      // xmm4 and xmm5 are volatile XMM registers.
      FloatingPointHelper::LoadFloatOperands(masm, xmm4, xmm5);

      switch (op_) {
        case Token::ADD: __ addsd(xmm4, xmm5); break;
        case Token::SUB: __ subsd(xmm4, xmm5); break;
        case Token::MUL: __ mulsd(xmm4, xmm5); break;
        case Token::DIV: __ divsd(xmm4, xmm5); break;
        default: UNREACHABLE();
      }
      // Allocate a heap number, if needed.
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
        case OVERWRITE_LEFT:
          __ JumpIfNotSmi(rdx, &skip_allocation);
          __ AllocateHeapNumber(rbx, rcx, &call_runtime);
          __ movq(rdx, rbx);
          __ bind(&skip_allocation);
          __ movq(rax, rdx);
          break;
        case OVERWRITE_RIGHT:
          // If the argument in rax is already an object, we skip the
          // allocation of a heap number.
          __ JumpIfNotSmi(rax, &skip_allocation);
          // Fall through!
        case NO_OVERWRITE:
          // Allocate a heap number for the result. Keep rax and rdx intact
          // for the possible runtime call.
          __ AllocateHeapNumber(rbx, rcx, &call_runtime);
          __ movq(rax, rbx);
          __ bind(&skip_allocation);
          break;
        default: UNREACHABLE();
      }
      __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm4);
      GenerateReturn(masm);
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
      Label skip_allocation, non_smi_result;
      FloatingPointHelper::LoadAsIntegers(masm, use_sse3_, &call_runtime);
      switch (op_) {
        case Token::BIT_OR:  __ orl(rax, rcx); break;
        case Token::BIT_AND: __ andl(rax, rcx); break;
        case Token::BIT_XOR: __ xorl(rax, rcx); break;
        case Token::SAR: __ sarl_cl(rax); break;
        case Token::SHL: __ shll_cl(rax); break;
        case Token::SHR: __ shrl_cl(rax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative. This can only happen for a shift
        // by zero, which also doesn't update the sign flag.
        __ testl(rax, rax);
        __ j(negative, &non_smi_result);
      }
      __ JumpIfNotValidSmiValue(rax, &non_smi_result);
      // Tag smi result, if possible, and return.
      __ Integer32ToSmi(rax, rax);
      GenerateReturn(masm);

      // All ops except SHR return a signed int32 that we load in a HeapNumber.
      if (op_ != Token::SHR && non_smi_result.is_linked()) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ movsxlq(rbx, rax);  // rbx: sign extended 32-bit result
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ movq(rax, Operand(rsp, mode_ == OVERWRITE_RIGHT ?
                                 1 * kPointerSize : 2 * kPointerSize));
            __ JumpIfNotSmi(rax, &skip_allocation);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(rax, rcx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        __ movq(Operand(rsp, 1 * kPointerSize), rbx);
        __ fild_s(Operand(rsp, 1 * kPointerSize));
        __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
        GenerateReturn(masm);
      }

      // SHR should return uint32 - go to runtime for non-smi/negative result.
      if (op_ == Token::SHR) {
        __ bind(&non_smi_result);
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
    __ pop(rcx);
    if (HasArgsReversed()) {
      __ push(rax);
      __ push(rdx);
    } else {
      __ push(rdx);
      __ push(rax);
    }
    __ push(rcx);
  }
  switch (op_) {
    case Token::ADD: {
      // Test for string arguments before calling runtime.
      Label not_strings, both_strings, not_string1, string1;
      Condition is_smi;
      Result answer;
      is_smi = masm->CheckSmi(rdx);
      __ j(is_smi, &not_string1);
      __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, rdx);
      __ j(above_equal, &not_string1);

      // First argument is a a string, test second.
      is_smi = masm->CheckSmi(rax);
      __ j(is_smi, &string1);
      __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rax);
      __ j(above_equal, &string1);

      // First and second argument are strings.
      StringAddStub stub(NO_STRING_CHECK_IN_STUB);
      __ TailCallStub(&stub);

      // Only first argument is a string.
      __ bind(&string1);
      __ InvokeBuiltin(
          HasArgsReversed() ?
              Builtins::STRING_ADD_RIGHT :
              Builtins::STRING_ADD_LEFT,
          JUMP_FUNCTION);

      // First argument was not a string, test second.
      __ bind(&not_string1);
      is_smi = masm->CheckSmi(rax);
      __ j(is_smi, &not_strings);
      __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rax);
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


void GenericBinaryOpStub::GenerateLoadArguments(MacroAssembler* masm) {
  // If arguments are not passed in registers read them from the stack.
  if (!HasArgsInRegisters()) {
    __ movq(rax, Operand(rsp, 1 * kPointerSize));
    __ movq(rdx, Operand(rsp, 2 * kPointerSize));
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


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 14));
  int nnn_value = (never_nan_nan_ ? 2 : 0);
  if (cc_ != equal) nnn_value = 0;  // Avoid duplicate stubs.
  return (static_cast<unsigned>(cc_) << 2) | nnn_value | (strict_ ? 1 : 0);
}


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


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime;

  // Load the two arguments.
  __ movq(rax, Operand(rsp, 2 * kPointerSize));  // First argument.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (string_check_) {
    Condition is_smi;
    is_smi = masm->CheckSmi(rax);
    __ j(is_smi, &string_add_runtime);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, r8);
    __ j(above_equal, &string_add_runtime);

    // First argument is a a string, test second.
    is_smi = masm->CheckSmi(rdx);
    __ j(is_smi, &string_add_runtime);
    __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, r9);
    __ j(above_equal, &string_add_runtime);
  }

  // Both arguments are strings.
  // rax: first string
  // rdx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ movl(rcx, FieldOperand(rdx, String::kLengthOffset));
  __ testl(rcx, rcx);
  __ j(not_zero, &second_not_zero_length);
  // Second string is empty, result is first string which is already in rax.
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ movl(rbx, FieldOperand(rax, String::kLengthOffset));
  __ testl(rbx, rbx);
  __ j(not_zero, &both_not_zero_length);
  // First string is empty, result is second string which is in rdx.
  __ movq(rax, rdx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // rax: first string
  // rbx: length of first string
  // rcx: length of second string
  // rdx: second string
  // r8: instance type of first string if string check was performed above
  // r9: instance type of first string if string check was performed above
  Label string_add_flat_result;
  __ bind(&both_not_zero_length);
  // Look at the length of the result of adding the two strings.
  __ addl(rbx, rcx);
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ cmpl(rbx, Immediate(2));
  __ j(equal, &string_add_runtime);
  // If arguments where known to be strings, maps are not loaded to r8 and r9
  // by the code above.
  if (!string_check_) {
    __ movq(r8, FieldOperand(rax, HeapObject::kMapOffset));
    __ movq(r9, FieldOperand(rdx, HeapObject::kMapOffset));
  }
  // Get the instance types of the two strings as they will be needed soon.
  __ movzxbl(r8, FieldOperand(r8, Map::kInstanceTypeOffset));
  __ movzxbl(r9, FieldOperand(r9, Map::kInstanceTypeOffset));
  // Check if resulting string will be flat.
  __ cmpl(rbx, Immediate(String::kMinNonFlatLength));
  __ j(below, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  ASSERT((String::kMaxLength & 0x80000000) == 0);
  __ cmpl(rbx, Immediate(String::kMaxLength));
  __ j(above, &string_add_runtime);

  // If result is not supposed to be flat, allocate a cons string object. If
  // both strings are ascii the result is an ascii cons string.
  // rax: first string
  // ebx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii, allocated;
  __ movl(rcx, r8);
  __ and_(rcx, r9);
  ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ testl(rcx, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii);
  // Allocate an acsii cons string.
  __ AllocateAsciiConsString(rcx, rdi, no_reg, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ movl(FieldOperand(rcx, ConsString::kLengthOffset), rbx);
  __ movl(FieldOperand(rcx, ConsString::kHashFieldOffset),
          Immediate(String::kEmptyHashField));
  __ movq(FieldOperand(rcx, ConsString::kFirstOffset), rax);
  __ movq(FieldOperand(rcx, ConsString::kSecondOffset), rdx);
  __ movq(rax, rcx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // Allocate a two byte cons string.
  __ AllocateConsString(rcx, rdi, no_reg, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are not
  // external strings.
  // rax: first string
  // ebx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  __ bind(&string_add_flat_result);
  __ movl(rcx, r8);
  __ and_(rcx, Immediate(kStringRepresentationMask));
  __ cmpl(rcx, Immediate(kExternalStringTag));
  __ j(equal, &string_add_runtime);
  __ movl(rcx, r9);
  __ and_(rcx, Immediate(kStringRepresentationMask));
  __ cmpl(rcx, Immediate(kExternalStringTag));
  __ j(equal, &string_add_runtime);
  // Now check if both strings are ascii strings.
  // rax: first string
  // ebx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii_string_add_flat_result;
  ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ testl(r8, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii_string_add_flat_result);
  __ testl(r9, Immediate(kAsciiStringTag));
  __ j(zero, &string_add_runtime);
  // Both strings are ascii strings. As they are short they are both flat.
  __ AllocateAsciiString(rcx, rbx, rdi, r14, r15, &string_add_runtime);
  // rcx: result string
  __ movq(rbx, rcx);
  // Locate first character of result.
  __ addq(rcx, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument
  __ movl(rdi, FieldOperand(rax, String::kLengthOffset));
  __ addq(rax, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // rax: first char of first argument
  // rbx: result string
  // rcx: first character of result
  // rdx: second string
  // rdi: length of first argument
  GenerateCopyCharacters(masm, rcx, rax, rdi, true);
  // Locate first character of second argument.
  __ movl(rdi, FieldOperand(rdx, String::kLengthOffset));
  __ addq(rdx, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // rbx: result string
  // rcx: next character of result
  // rdx: first char of second argument
  // rdi: length of second argument
  GenerateCopyCharacters(masm, rcx, rdx, rdi, true);
  __ movq(rax, rbx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Handle creating a flat two byte result.
  // rax: first string - known to be two byte
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  __ bind(&non_ascii_string_add_flat_result);
  __ and_(r9, Immediate(kAsciiStringTag));
  __ j(not_zero, &string_add_runtime);
  // Both strings are two byte strings. As they are short they are both
  // flat.
  __ AllocateTwoByteString(rcx, rbx, rdi, r14, r15, &string_add_runtime);
  // rcx: result string
  __ movq(rbx, rcx);
  // Locate first character of result.
  __ addq(rcx, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ movl(rdi, FieldOperand(rax, String::kLengthOffset));
  __ addq(rax, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // rax: first char of first argument
  // rbx: result string
  // rcx: first character of result
  // rdx: second argument
  // rdi: length of first argument
  GenerateCopyCharacters(masm, rcx, rax, rdi, false);
  // Locate first character of second argument.
  __ movl(rdi, FieldOperand(rdx, String::kLengthOffset));
  __ addq(rdx, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // rbx: result string
  // rcx: next character of result
  // rdx: first char of second argument
  // rdi: length of second argument
  GenerateCopyCharacters(masm, rcx, rdx, rdi, false);
  __ movq(rax, rbx);
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
                                            bool ascii) {
  Label loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ movb(kScratchRegister, Operand(src, 0));
    __ movb(Operand(dest, 0), kScratchRegister);
    __ addq(src, Immediate(1));
    __ addq(dest, Immediate(1));
  } else {
    __ movzxwl(kScratchRegister, Operand(src, 0));
    __ movw(Operand(dest, 0), kScratchRegister);
    __ addq(src, Immediate(2));
    __ addq(dest, Immediate(2));
  }
  __ subl(count, Immediate(1));
  __ j(not_zero, &loop);
}


void StringStubBase::GenerateCopyCharactersREP(MacroAssembler* masm,
                                               Register dest,
                                               Register src,
                                               Register count,
                                               bool ascii) {
  // Copy characters using rep movs of doublewords. Align destination on 4 byte
  // boundary before starting rep movs. Copy remaining characters after running
  // rep movs.
  ASSERT(dest.is(rdi));  // rep movs destination
  ASSERT(src.is(rsi));  // rep movs source
  ASSERT(count.is(rcx));  // rep movs count

  // Nothing to do for zero characters.
  Label done;
  __ testq(count, count);
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (!ascii) {
    ASSERT_EQ(2, sizeof(uc16));  // NOLINT
    __ addq(count, count);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ testq(count, Immediate(~7));
  __ j(zero, &last_bytes);

  // Copy from edi to esi using rep movs instruction.
  __ movq(kScratchRegister, count);
  __ sar(count, Immediate(3));  // Number of doublewords to copy.
  __ repmovsq();

  // Find number of bytes left.
  __ movq(count, kScratchRegister);
  __ and_(count, Immediate(7));

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ testq(count, count);
  __ j(zero, &done);

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ movb(kScratchRegister, Operand(src, 0));
  __ movb(Operand(dest, 0), kScratchRegister);
  __ addq(src, Immediate(1));
  __ addq(dest, Immediate(1));
  __ subq(count, Immediate(1));
  __ j(not_zero, &loop);

  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: to
  //  rsp[16]: from
  //  rsp[24]: string

  const int kToOffset = 1 * kPointerSize;
  const int kFromOffset = kToOffset + kPointerSize;
  const int kStringOffset = kFromOffset + kPointerSize;
  const int kArgumentsSize = (kStringOffset + kPointerSize) - kToOffset;

  // Make sure first argument is a string.
  __ movq(rax, Operand(rsp, kStringOffset));
  ASSERT_EQ(0, kSmiTag);
  __ testl(rax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rax: string
  // rbx: instance type
  // Calculate length of sub string using the smi values.
  __ movq(rcx, Operand(rsp, kToOffset));
  __ movq(rdx, Operand(rsp, kFromOffset));
  __ JumpIfNotBothPositiveSmi(rcx, rdx, &runtime);

  __ SmiSub(rcx, rcx, rdx, NULL);  // Overflow doesn't happen.
  __ j(negative, &runtime);
  // Handle sub-strings of length 2 and less in the runtime system.
  __ SmiToInteger32(rcx, rcx);
  __ cmpl(rcx, Immediate(2));
  __ j(below_equal, &runtime);

  // rax: string
  // rbx: instance type
  // rcx: result string length
  // Check for flat ascii string
  Label non_ascii_flat;
  __ and_(rbx, Immediate(kStringRepresentationMask | kStringEncodingMask));
  __ cmpb(rbx, Immediate(kSeqStringTag | kAsciiStringTag));
  __ j(not_equal, &non_ascii_flat);

  // Allocate the result.
  __ AllocateAsciiString(rax, rcx, rbx, rdx, rdi, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(rdx, rsi);  // esi used by following code.
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  // Load string argument and locate character of sub string start.
  __ movq(rsi, Operand(rsp, kStringOffset));
  __ movq(rbx, Operand(rsp, kFromOffset));
  {
    SmiIndex smi_as_index = masm->SmiToIndex(rbx, rbx, times_1);
    __ lea(rsi, Operand(rsi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }

  // rax: result string
  // rcx: result length
  // rdx: original value of rsi
  // rdi: first character of result
  // rsi: character of sub string start
  GenerateCopyCharactersREP(masm, rdi, rsi, rcx, true);
  __ movq(rsi, rdx);  // Restore rsi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(kArgumentsSize);

  __ bind(&non_ascii_flat);
  // rax: string
  // rbx: instance type & kStringRepresentationMask | kStringEncodingMask
  // rcx: result string length
  // Check for sequential two byte string
  __ cmpb(rbx, Immediate(kSeqStringTag | kTwoByteStringTag));
  __ j(not_equal, &runtime);

  // Allocate the result.
  __ AllocateTwoByteString(rax, rcx, rbx, rdx, rdi, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(rdx, rsi);  // esi used by following code.
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqTwoByteString::kHeaderSize));
  // Load string argument and locate character of sub string start.
  __ movq(rsi, Operand(rsp, kStringOffset));
  __ movq(rbx, Operand(rsp, kFromOffset));
  {
    SmiIndex smi_as_index = masm->SmiToIndex(rbx, rbx, times_2);
    __ lea(rsi, Operand(rsi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }

  // rax: result string
  // rcx: result length
  // rdx: original value of rsi
  // rdi: first character of result
  // rsi: character of sub string start
  GenerateCopyCharactersREP(masm, rdi, rsi, rcx, false);
  __ movq(rsi, rdx);  // Restore esi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(kArgumentsSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kSubString), 3, 1);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  // Ensure that you can always subtract a string length from a non-negative
  // number (e.g. another length).
  ASSERT(String::kMaxLength < 0x7fffffff);

  // Find minimum length and length difference.
  __ movl(scratch1, FieldOperand(left, String::kLengthOffset));
  __ movl(scratch4, scratch1);
  __ subl(scratch4, FieldOperand(right, String::kLengthOffset));
  // Register scratch4 now holds left.length - right.length.
  const Register length_difference = scratch4;
  Label left_shorter;
  __ j(less, &left_shorter);
  // The right string isn't longer that the left one.
  // Get the right string's length by subtracting the (non-negative) difference
  // from the left string's length.
  __ subl(scratch1, length_difference);
  __ bind(&left_shorter);
  // Register scratch1 now holds Min(left.length, right.length).
  const Register min_length = scratch1;

  Label compare_lengths;
  // If min-length is zero, go directly to comparing lengths.
  __ testl(min_length, min_length);
  __ j(zero, &compare_lengths);

  // Registers scratch2 and scratch3 are free.
  Label result_not_equal;
  Label loop;
  {
    // Check characters 0 .. min_length - 1 in a loop.
    // Use scratch3 as loop index, min_length as limit and scratch2
    // for computation.
    const Register index = scratch3;
    __ movl(index, Immediate(0));  // Index into strings.
    __ bind(&loop);
    // Compare characters.
    // TODO(lrn): Could we load more than one character at a time?
    __ movb(scratch2, FieldOperand(left,
                                   index,
                                   times_1,
                                   SeqAsciiString::kHeaderSize));
    // Increment index and use -1 modifier on next load to give
    // the previous load extra time to complete.
    __ addl(index, Immediate(1));
    __ cmpb(scratch2, FieldOperand(right,
                                   index,
                                   times_1,
                                   SeqAsciiString::kHeaderSize - 1));
    __ j(not_equal, &result_not_equal);
    __ cmpl(index, min_length);
    __ j(not_equal, &loop);
  }
  // Completed loop without finding different characters.
  // Compare lengths (precomputed).
  __ bind(&compare_lengths);
  __ testl(length_difference, length_difference);
  __ j(not_zero, &result_not_equal);

  // Result is EQUAL.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(2 * kPointerSize);

  Label result_greater;
  __ bind(&result_not_equal);
  // Unequal comparison of left to right, either character or length.
  __ j(greater, &result_greater);

  // Result is LESS.
  __ Move(rax, Smi::FromInt(LESS));
  __ ret(2 * kPointerSize);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(rax, Smi::FromInt(GREATER));
  __ ret(2 * kPointerSize);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: right string
  //  rsp[16]: left string

  __ movq(rdx, Operand(rsp, 2 * kPointerSize));  // left
  __ movq(rax, Operand(rsp, 1 * kPointerSize));  // right

  // Check for identity.
  Label not_same;
  __ cmpq(rdx, rax);
  __ j(not_equal, &not_same);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ IncrementCounter(&Counters::string_compare_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(rdx, rax, rcx, rbx, &runtime);

  // Inline comparison of ascii strings.
  __ IncrementCounter(&Counters::string_compare_native, 1);
  GenerateCompareFlatAsciiStrings(masm, rdx, rax, rcx, rbx, rdi, r8);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(ExternalReference(Runtime::kStringCompare), 2, 1);
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

} }  // namespace v8::internal
