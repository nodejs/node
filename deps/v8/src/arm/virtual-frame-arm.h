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

namespace v8 {
namespace internal {

// This dummy class is only used to create invalid virtual frames.
extern class InvalidVirtualFrameInitializer {}* kInvalidVirtualFrameInitializer;


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
    inline explicit RegisterAllocationScope(CodeGenerator* cgen);
    inline ~RegisterAllocationScope();

   private:
    CodeGenerator* cgen_;
    bool old_is_spilled_;

    RegisterAllocationScope() { }
  };

  // An illegal index into the virtual frame.
  static const int kIllegalIndex = -1;

  // Construct an initial virtual frame on entry to a JS function.
  inline VirtualFrame();

  // Construct an invalid virtual frame, used by JumpTargets.
  inline VirtualFrame(InvalidVirtualFrameInitializer* dummy);

  // Construct a virtual frame as a clone of an existing one.
  explicit inline VirtualFrame(VirtualFrame* original);

  inline CodeGenerator* cgen() const;
  inline MacroAssembler* masm();

  // The number of elements on the virtual frame.
  int element_count() const { return element_count_; }

  // The height of the virtual expression stack.
  inline int height() const;

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

  // Add extra in-memory elements to the top of the frame to match an actual
  // frame (eg, the frame after an exception handler is pushed).  No code is
  // emitted.
  void Adjust(int count);

  // Forget elements from the top of the frame to match an actual frame (eg,
  // the frame after a runtime call).  No code is emitted except to bring the
  // frame to a spilled state.
  void Forget(int count);

  // Spill all values from the frame to memory.
  void SpillAll();

  void AssertIsSpilled() const {
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
  void MergeTo(VirtualFrame* expected, Condition cond = al);
  void MergeTo(const VirtualFrame* expected, Condition cond = al);

  // Checks whether this frame can be branched to by the other frame.
  bool IsCompatibleWith(const VirtualFrame* other) const {
    return (tos_known_smi_map_ & (~other->tos_known_smi_map_)) == 0;
  }

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

  // Prepare for returning from the frame by elements in the virtual frame. This
  // avoids generating unnecessary merge code when jumping to the
  // shared return site. No spill code emitted. Value to return should be in r0.
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
    int adjusted_index = index - kVirtualElements[top_of_stack_state_];
    ASSERT(adjusted_index >= 0);
    return MemOperand(sp, adjusted_index * kPointerSize);
  }

  bool KnownSmiAt(int index) {
    if (index >= kTOSKnownSmiMapSize) return false;
    return (tos_known_smi_map_ & (1 << index)) != 0;
  }

  // A frame-allocated local as an assembly operand.
  inline MemOperand LocalAt(int index);

  // Push the address of the receiver slot on the frame.
  void PushReceiverSlotAddress();

  // The function frame slot.
  MemOperand Function() { return MemOperand(fp, kFunctionOffset); }

  // The context frame slot.
  MemOperand Context() { return MemOperand(fp, kContextOffset); }

  // A parameter as an assembly operand.
  inline MemOperand ParameterAt(int index);

  // The receiver frame slot.
  inline MemOperand Receiver();

  // Push a try-catch or try-finally handler on top of the virtual frame.
  void PushTryHandler(HandlerType type);

  // Call stub given the number of arguments it expects on (and
  // removes from) the stack.
  inline void CallStub(CodeStub* stub, int arg_count);

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

  // Call load IC. Receiver is on the stack and is consumed. Result is returned
  // in r0.
  void CallLoadIC(Handle<String> name, RelocInfo::Mode mode);

  // Call store IC. If the load is contextual, value is found on top of the
  // frame. If not, value and receiver are on the frame. Both are consumed.
  // Result is returned in r0.
  void CallStoreIC(Handle<String> name, bool is_contextual);

  // Call keyed load IC. Key and receiver are on the stack. Both are consumed.
  // Result is returned in r0.
  void CallKeyedLoadIC();

  // Call keyed store IC. Value, key and receiver are on the stack. All three
  // are consumed. Result is returned in r0.
  void CallKeyedStoreIC();

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

  // Duplicate the top of stack.
  void Dup();

  // Duplicate the two elements on top of stack.
  void Dup2();

  // Flushes all registers, but it puts a copy of the top-of-stack in r0.
  void SpillAllButCopyTOSToR0();

  // Flushes all registers, but it puts a copy of the top-of-stack in r1
  // and the next value on the stack in r0.
  void SpillAllButCopyTOSToR1R0();

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
  void EmitPush(Register reg, TypeInfo type_info = TypeInfo::Unknown());
  void EmitPush(Operand operand, TypeInfo type_info = TypeInfo::Unknown());
  void EmitPush(MemOperand operand, TypeInfo type_info = TypeInfo::Unknown());
  void EmitPushRoot(Heap::RootListIndex index);

  // Overwrite the nth thing on the stack.  If the nth position is in a
  // register then this turns into a mov, otherwise an str.  Afterwards
  // you can still use the register even if it is a register that can be
  // used for TOS (r0 or r1).
  void SetElementAt(Register reg, int this_far_down);

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
  enum TopOfStack {
    NO_TOS_REGISTERS,
    R0_TOS,
    R1_TOS,
    R1_R0_TOS,
    R0_R1_TOS,
    TOS_STATES
  };

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
  static const int kTOSKnownSmiMapSize = 4;
  unsigned tos_known_smi_map_:kTOSKnownSmiMapSize;

  // The index of the element that is at the processor's stack pointer
  // (the sp register).  For now since everything is in memory it is given
  // by the number of elements on the not-very-virtual stack frame.
  int stack_pointer() { return element_count_ - 1; }

  // The number of frame-allocated locals and parameters respectively.
  inline int parameter_count() const;
  inline int local_count() const;

  // The index of the element that is at the processor's frame pointer
  // (the fp register).  The parameters, receiver, function, and context
  // are below the frame pointer.
  inline int frame_pointer() const;

  // The index of the first parameter.  The receiver lies below the first
  // parameter.
  int param0_index() { return 1; }

  // The index of the context slot in the frame.  It is immediately
  // below the frame pointer.
  inline int context_index();

  // The index of the function slot in the frame.  It is below the frame
  // pointer and context slot.
  inline int function_index();

  // The index of the first local.  Between the frame pointer and the
  // locals lies the return address.
  inline int local0_index() const;

  // The index of the base of the expression stack.
  inline int expression_base_index() const;

  // Convert a frame index into a frame pointer relative offset into the
  // actual stack.
  inline int fp_relative(int index);

  // Spill all elements in registers. Spill the top spilled_args elements
  // on the frame.  Sync all other frame elements.
  // Then drop dropped_args elements from the virtual frame, to match
  // the effect of an upcoming call that will drop them from the stack.
  void PrepareForCall(int spilled_args, int dropped_args);

  // If all top-of-stack registers are in use then the lowest one is pushed
  // onto the physical stack and made free.
  void EnsureOneFreeTOSRegister();

  // Emit instructions to get the top of stack state from where we are to where
  // we want to be.
  void MergeTOSTo(TopOfStack expected_state, Condition cond = al);

  inline bool Equals(const VirtualFrame* other);

  inline void LowerHeight(int count) {
    element_count_ -= count;
    if (count >= kTOSKnownSmiMapSize) {
      tos_known_smi_map_ = 0;
    } else {
      tos_known_smi_map_ >>= count;
    }
  }

  inline void RaiseHeight(int count, unsigned known_smi_map = 0) {
    ASSERT(known_smi_map < (1u << count));
    element_count_ += count;
    if (count >= kTOSKnownSmiMapSize) {
      tos_known_smi_map_ = known_smi_map;
    } else {
      tos_known_smi_map_ = ((tos_known_smi_map_ << count) | known_smi_map);
    }
  }

  friend class JumpTarget;
};


} }  // namespace v8::internal

#endif  // V8_ARM_VIRTUAL_FRAME_ARM_H_
