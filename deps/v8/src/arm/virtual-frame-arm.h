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

#ifndef V8_ARM_VIRTUAL_FRAME_ARM_H_
#define V8_ARM_VIRTUAL_FRAME_ARM_H_

#include "register-allocator.h"
#include "scopes.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// Virtual frames
//
// The virtual frame is an abstraction of the physical stack frame.  It
// encapsulates the parameters, frame-allocated locals, and the expression
// stack.  It supports push/pop operations on the expression stack, as well
// as random access to the expression stack elements, locals, and
// parameters.

class VirtualFrame : public ZoneObject {
 public:
  class RegisterAllocationScope;
  // A utility class to introduce a scope where the virtual frame is
  // expected to remain spilled.  The constructor spills the code
  // generator's current frame, and keeps it spilled.
  class SpilledScope BASE_EMBEDDED {
   public:
    explicit SpilledScope(VirtualFrame* frame)
      : old_is_spilled_(is_spilled_) {
      if (frame != NULL) {
        if (!is_spilled_) {
          frame->SpillAll();
        } else {
          frame->AssertIsSpilled();
        }
      }
      is_spilled_ = true;
    }
    ~SpilledScope() {
      is_spilled_ = old_is_spilled_;
    }
    static bool is_spilled() { return is_spilled_; }

   private:
    static bool is_spilled_;
    int old_is_spilled_;

    SpilledScope() { }

    friend class RegisterAllocationScope;
  };

  class RegisterAllocationScope BASE_EMBEDDED {
   public:
    // A utility class to introduce a scope where the virtual frame
    // is not spilled, ie. where register allocation occurs.  Eventually
    // when RegisterAllocationScope is ubiquitous it can be removed
    // along with the (by then unused) SpilledScope class.
    explicit RegisterAllocationScope(CodeGenerator* cgen)
      : cgen_(cgen),
        old_is_spilled_(SpilledScope::is_spilled_) {
      SpilledScope::is_spilled_ = false;
      if (old_is_spilled_) {
        VirtualFrame* frame = cgen->frame();
        if (frame != NULL) {
          frame->AssertIsSpilled();
        }
      }
    }
    ~RegisterAllocationScope() {
      SpilledScope::is_spilled_ = old_is_spilled_;
      if (old_is_spilled_) {
        VirtualFrame* frame = cgen_->frame();
        if (frame != NULL) {
          frame->SpillAll();
        }
      }
    }

   private:
    CodeGenerator* cgen_;
    bool old_is_spilled_;

    RegisterAllocationScope() { }
  };

  // An illegal index into the virtual frame.
  static const int kIllegalIndex = -1;

  // Construct an initial virtual frame on entry to a JS function.
  inline VirtualFrame();

  // Construct a virtual frame as a clone of an existing one.
  explicit inline VirtualFrame(VirtualFrame* original);

  CodeGenerator* cgen() { return CodeGeneratorScope::Current(); }
  MacroAssembler* masm() { return cgen()->masm(); }

  // The number of elements on the virtual frame.
  int element_count() { return element_count_; }

  // The height of the virtual expression stack.
  int height() {
    return element_count() - expression_base_index();
  }

  bool is_used(int num) {
    switch (num) {
      case 0: {  // r0.
        return kR0InUse[top_of_stack_state_];
      }
      case 1: {  // r1.
        return kR1InUse[top_of_stack_state_];
      }
      case 2:
      case 3:
      case 4:
      case 5:
      case 6: {  // r2 to r6.
        ASSERT(num - kFirstAllocatedRegister < kNumberOfAllocatedRegisters);
        ASSERT(num >= kFirstAllocatedRegister);
        if ((register_allocation_map_ &
             (1 << (num - kFirstAllocatedRegister))) == 0) {
          return false;
        } else {
          return true;
        }
      }
      default: {
        ASSERT(num < kFirstAllocatedRegister ||
               num >= kFirstAllocatedRegister + kNumberOfAllocatedRegisters);
        return false;
      }
    }
  }

  bool is_used(Register reg) {
    return is_used(RegisterAllocator::ToNumber(reg));
  }

  // Add extra in-memory elements to the top of the frame to match an actual
  // frame (eg, the frame after an exception handler is pushed).  No code is
  // emitted.
  void Adjust(int count);

  // Forget elements from the top of the frame to match an actual frame (eg,
  // the frame after a runtime call).  No code is emitted except to bring the
  // frame to a spilled state.
  void Forget(int count) {
    SpillAll();
    element_count_ -= count;
  }

  // Spill all values from the frame to memory.
  void SpillAll();

  void AssertIsSpilled() {
    ASSERT(top_of_stack_state_ == NO_TOS_REGISTERS);
    ASSERT(register_allocation_map_ == 0);
  }

  void AssertIsNotSpilled() {
    ASSERT(!SpilledScope::is_spilled());
  }

  // Spill all occurrences of a specific register from the frame.
  void Spill(Register reg) {
    UNIMPLEMENTED();
  }

  // Spill all occurrences of an arbitrary register if possible.  Return the
  // register spilled or no_reg if it was not possible to free any register
  // (ie, they all have frame-external references).  Unimplemented.
  Register SpillAnyRegister();

  // Make this virtual frame have a state identical to an expected virtual
  // frame.  As a side effect, code may be emitted to make this frame match
  // the expected one.
  void MergeTo(VirtualFrame* expected);

  // Detach a frame from its code generator, perhaps temporarily.  This
  // tells the register allocator that it is free to use frame-internal
  // registers.  Used when the code generator's frame is switched from this
  // one to NULL by an unconditional jump.
  void DetachFromCodeGenerator() {
    AssertIsSpilled();
  }

  // (Re)attach a frame to its code generator.  This informs the register
  // allocator that the frame-internal register references are active again.
  // Used when a code generator's frame is switched from NULL to this one by
  // binding a label.
  void AttachToCodeGenerator() {
    AssertIsSpilled();
  }

  // Emit code for the physical JS entry and exit frame sequences.  After
  // calling Enter, the virtual frame is ready for use; and after calling
  // Exit it should not be used.  Note that Enter does not allocate space in
  // the physical frame for storing frame-allocated locals.
  void Enter();
  void Exit();

  // Prepare for returning from the frame by spilling locals and
  // dropping all non-locals elements in the virtual frame.  This
  // avoids generating unnecessary merge code when jumping to the
  // shared return site.  Emits code for spills.
  inline void PrepareForReturn();

  // Number of local variables after when we use a loop for allocating.
  static const int kLocalVarBound = 5;

  // Allocate and initialize the frame-allocated locals.
  void AllocateStackSlots();

  // The current top of the expression stack as an assembly operand.
  MemOperand Top() {
    AssertIsSpilled();
    return MemOperand(sp, 0);
  }

  // An element of the expression stack as an assembly operand.
  MemOperand ElementAt(int index) {
    AssertIsSpilled();
    return MemOperand(sp, index * kPointerSize);
  }

  // A frame-allocated local as an assembly operand.
  MemOperand LocalAt(int index) {
    ASSERT(0 <= index);
    ASSERT(index < local_count());
    return MemOperand(fp, kLocal0Offset - index * kPointerSize);
  }

  // Push the address of the receiver slot on the frame.
  void PushReceiverSlotAddress();

  // The function frame slot.
  MemOperand Function() { return MemOperand(fp, kFunctionOffset); }

  // The context frame slot.
  MemOperand Context() { return MemOperand(fp, kContextOffset); }

  // A parameter as an assembly operand.
  MemOperand ParameterAt(int index) {
    // Index -1 corresponds to the receiver.
    ASSERT(-1 <= index);  // -1 is the receiver.
    ASSERT(index <= parameter_count());
    return MemOperand(fp, (1 + parameter_count() - index) * kPointerSize);
  }

  // The receiver frame slot.
  MemOperand Receiver() { return ParameterAt(-1); }

  // Push a try-catch or try-finally handler on top of the virtual frame.
  void PushTryHandler(HandlerType type);

  // Call stub given the number of arguments it expects on (and
  // removes from) the stack.
  void CallStub(CodeStub* stub, int arg_count) {
    if (arg_count != 0) Forget(arg_count);
    ASSERT(cgen()->HasValidEntryRegisters());
    masm()->CallStub(stub);
  }

  // Call JS function from top of the stack with arguments
  // taken from the stack.
  void CallJSFunction(int arg_count);

  // Call runtime given the number of arguments expected on (and
  // removed from) the stack.
  void CallRuntime(Runtime::Function* f, int arg_count);
  void CallRuntime(Runtime::FunctionId id, int arg_count);

#ifdef ENABLE_DEBUGGER_SUPPORT
  void DebugBreak();
#endif

  // Invoke builtin given the number of arguments it expects on (and
  // removes from) the stack.
  void InvokeBuiltin(Builtins::JavaScript id,
                     InvokeJSFlags flag,
                     int arg_count);

  // Call into an IC stub given the number of arguments it removes
  // from the stack.  Register arguments to the IC stub are implicit,
  // and depend on the type of IC stub.
  void CallCodeObject(Handle<Code> ic,
                      RelocInfo::Mode rmode,
                      int dropped_args);

  // Drop a number of elements from the top of the expression stack.  May
  // emit code to affect the physical frame.  Does not clobber any registers
  // excepting possibly the stack pointer.
  void Drop(int count);

  // Drop one element.
  void Drop() { Drop(1); }

  // Pop an element from the top of the expression stack.  Discards
  // the result.
  void Pop();

  // Pop an element from the top of the expression stack.  The register
  // will be one normally used for the top of stack register allocation
  // so you can't hold on to it if you push on the stack.
  Register PopToRegister(Register but_not_to_this_one = no_reg);

  // Look at the top of the stack.  The register returned is aliased and
  // must be copied to a scratch register before modification.
  Register Peek();

  // Pop and save an element from the top of the expression stack and
  // emit a corresponding pop instruction.
  void EmitPop(Register reg);

  // Takes the top two elements and puts them in r0 (top element) and r1
  // (second element).
  void PopToR1R0();

  // Takes the top element and puts it in r1.
  void PopToR1();

  // Takes the top element and puts it in r0.
  void PopToR0();

  // Push an element on top of the expression stack and emit a
  // corresponding push instruction.
  void EmitPush(Register reg);
  void EmitPush(MemOperand operand);

  // Get a register which is free and which must be immediately used to
  // push on the top of the stack.
  Register GetTOSRegister();

  // Push multiple registers on the stack and the virtual frame
  // Register are selected by setting bit in src_regs and
  // are pushed in decreasing order: r15 .. r0.
  void EmitPushMultiple(int count, int src_regs);

  static Register scratch0() { return r7; }
  static Register scratch1() { return r9; }

 private:
  static const int kLocal0Offset = JavaScriptFrameConstants::kLocal0Offset;
  static const int kFunctionOffset = JavaScriptFrameConstants::kFunctionOffset;
  static const int kContextOffset = StandardFrameConstants::kContextOffset;

  static const int kHandlerSize = StackHandlerConstants::kSize / kPointerSize;
  static const int kPreallocatedElements = 5 + 8;  // 8 expression stack slots.

  // 5 states for the top of stack, which can be in memory or in r0 and r1.
  enum TopOfStack { NO_TOS_REGISTERS, R0_TOS, R1_TOS, R1_R0_TOS, R0_R1_TOS,
                    TOS_STATES};
  static const int kMaxTOSRegisters = 2;

  static const bool kR0InUse[TOS_STATES];
  static const bool kR1InUse[TOS_STATES];
  static const int kVirtualElements[TOS_STATES];
  static const TopOfStack kStateAfterPop[TOS_STATES];
  static const TopOfStack kStateAfterPush[TOS_STATES];
  static const Register kTopRegister[TOS_STATES];
  static const Register kBottomRegister[TOS_STATES];

  // We allocate up to 5 locals in registers.
  static const int kNumberOfAllocatedRegisters = 5;
  // r2 to r6 are allocated to locals.
  static const int kFirstAllocatedRegister = 2;

  static const Register kAllocatedRegisters[kNumberOfAllocatedRegisters];

  static Register AllocatedRegister(int r) {
    ASSERT(r >= 0 && r < kNumberOfAllocatedRegisters);
    return kAllocatedRegisters[r];
  }

  // The number of elements on the stack frame.
  int element_count_;
  TopOfStack top_of_stack_state_:3;
  int register_allocation_map_:kNumberOfAllocatedRegisters;

  // The index of the element that is at the processor's stack pointer
  // (the sp register).  For now since everything is in memory it is given
  // by the number of elements on the not-very-virtual stack frame.
  int stack_pointer() { return element_count_ - 1; }

  // The number of frame-allocated locals and parameters respectively.
  int parameter_count() { return cgen()->scope()->num_parameters(); }
  int local_count() { return cgen()->scope()->num_stack_slots(); }

  // The index of the element that is at the processor's frame pointer
  // (the fp register).  The parameters, receiver, function, and context
  // are below the frame pointer.
  int frame_pointer() { return parameter_count() + 3; }

  // The index of the first parameter.  The receiver lies below the first
  // parameter.
  int param0_index() { return 1; }

  // The index of the context slot in the frame.  It is immediately
  // below the frame pointer.
  int context_index() { return frame_pointer() - 1; }

  // The index of the function slot in the frame.  It is below the frame
  // pointer and context slot.
  int function_index() { return frame_pointer() - 2; }

  // The index of the first local.  Between the frame pointer and the
  // locals lies the return address.
  int local0_index() { return frame_pointer() + 2; }

  // The index of the base of the expression stack.
  int expression_base_index() { return local0_index() + local_count(); }

  // Convert a frame index into a frame pointer relative offset into the
  // actual stack.
  int fp_relative(int index) {
    ASSERT(index < element_count());
    ASSERT(frame_pointer() < element_count());  // FP is on the frame.
    return (frame_pointer() - index) * kPointerSize;
  }

  // Spill all elements in registers. Spill the top spilled_args elements
  // on the frame.  Sync all other frame elements.
  // Then drop dropped_args elements from the virtual frame, to match
  // the effect of an upcoming call that will drop them from the stack.
  void PrepareForCall(int spilled_args, int dropped_args);

  // If all top-of-stack registers are in use then the lowest one is pushed
  // onto the physical stack and made free.
  void EnsureOneFreeTOSRegister();

  inline bool Equals(VirtualFrame* other);

  friend class JumpTarget;
  friend class DeferredCode;
};


} }  // namespace v8::internal

#endif  // V8_ARM_VIRTUAL_FRAME_ARM_H_
