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

#ifndef V8_IA32_VIRTUAL_FRAME_IA32_H_
#define V8_IA32_VIRTUAL_FRAME_IA32_H_

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
  // A utility class to introduce a scope where the virtual frame is
  // expected to remain spilled.  The constructor spills the code
  // generator's current frame, but no attempt is made to require it
  // to stay spilled.  It is intended as documentation while the code
  // generator is being transformed.
  class SpilledScope BASE_EMBEDDED {
   public:
    SpilledScope() : previous_state_(cgen()->in_spilled_code()) {
      ASSERT(cgen()->has_valid_frame());
      cgen()->frame()->SpillAll();
      cgen()->set_in_spilled_code(true);
    }

    ~SpilledScope() {
      cgen()->set_in_spilled_code(previous_state_);
    }

   private:
    bool previous_state_;

    CodeGenerator* cgen() { return CodeGeneratorScope::Current(); }
  };

  // An illegal index into the virtual frame.
  static const int kIllegalIndex = -1;

  // Construct an initial virtual frame on entry to a JS function.
  VirtualFrame();

  // Construct a virtual frame as a clone of an existing one.
  explicit VirtualFrame(VirtualFrame* original);

  CodeGenerator* cgen() { return CodeGeneratorScope::Current(); }
  MacroAssembler* masm() { return cgen()->masm(); }

  // Create a duplicate of an existing valid frame element.
  FrameElement CopyElementAt(int index);

  // The number of elements on the virtual frame.
  int element_count() { return elements_.length(); }

  // The height of the virtual expression stack.
  int height() {
    return element_count() - expression_base_index();
  }

  int register_location(int num) {
    ASSERT(num >= 0 && num < RegisterAllocator::kNumRegisters);
    return register_locations_[num];
  }

  int register_location(Register reg) {
    return register_locations_[RegisterAllocator::ToNumber(reg)];
  }

  void set_register_location(Register reg, int index) {
    register_locations_[RegisterAllocator::ToNumber(reg)] = index;
  }

  bool is_used(int num) {
    ASSERT(num >= 0 && num < RegisterAllocator::kNumRegisters);
    return register_locations_[num] != kIllegalIndex;
  }

  bool is_used(Register reg) {
    return register_locations_[RegisterAllocator::ToNumber(reg)]
        != kIllegalIndex;
  }

  // Add extra in-memory elements to the top of the frame to match an actual
  // frame (eg, the frame after an exception handler is pushed).  No code is
  // emitted.
  void Adjust(int count);

  // Forget count elements from the top of the frame all in-memory
  // (including synced) and adjust the stack pointer downward, to
  // match an external frame effect (examples include a call removing
  // its arguments, and exiting a try/catch removing an exception
  // handler).  No code will be emitted.
  void Forget(int count) {
    ASSERT(count >= 0);
    ASSERT(stack_pointer_ == element_count() - 1);
    stack_pointer_ -= count;
    ForgetElements(count);
  }

  // Forget count elements from the top of the frame without adjusting
  // the stack pointer downward.  This is used, for example, before
  // merging frames at break, continue, and return targets.
  void ForgetElements(int count);

  // Spill all values from the frame to memory.
  void SpillAll();

  // Spill all occurrences of a specific register from the frame.
  void Spill(Register reg) {
    if (is_used(reg)) SpillElementAt(register_location(reg));
  }

  // Spill all occurrences of an arbitrary register if possible.  Return the
  // register spilled or no_reg if it was not possible to free any register
  // (ie, they all have frame-external references).
  Register SpillAnyRegister();

  // Sync the range of elements in [begin, end] with memory.
  void SyncRange(int begin, int end);

  // Make this frame so that an arbitrary frame of the same height can
  // be merged to it.  Copies and constants are removed from the frame.
  void MakeMergable();

  // Prepare this virtual frame for merging to an expected frame by
  // performing some state changes that do not require generating
  // code.  It is guaranteed that no code will be generated.
  void PrepareMergeTo(VirtualFrame* expected);

  // Make this virtual frame have a state identical to an expected virtual
  // frame.  As a side effect, code may be emitted to make this frame match
  // the expected one.
  void MergeTo(VirtualFrame* expected);

  // Detach a frame from its code generator, perhaps temporarily.  This
  // tells the register allocator that it is free to use frame-internal
  // registers.  Used when the code generator's frame is switched from this
  // one to NULL by an unconditional jump.
  void DetachFromCodeGenerator() {
    RegisterAllocator* cgen_allocator = cgen()->allocator();
    for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
      if (is_used(i)) cgen_allocator->Unuse(i);
    }
  }

  // (Re)attach a frame to its code generator.  This informs the register
  // allocator that the frame-internal register references are active again.
  // Used when a code generator's frame is switched from NULL to this one by
  // binding a label.
  void AttachToCodeGenerator() {
    RegisterAllocator* cgen_allocator = cgen()->allocator();
    for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
      if (is_used(i)) cgen_allocator->Use(i);
    }
  }

  // Emit code for the physical JS entry and exit frame sequences.  After
  // calling Enter, the virtual frame is ready for use; and after calling
  // Exit it should not be used.  Note that Enter does not allocate space in
  // the physical frame for storing frame-allocated locals.
  void Enter();
  void Exit();

  // Prepare for returning from the frame by spilling locals.  This
  // avoids generating unnecessary merge code when jumping to the
  // shared return site.  Emits code for spills.
  void PrepareForReturn();

  // Allocate and initialize the frame-allocated locals.
  void AllocateStackSlots();

  // An element of the expression stack as an assembly operand.
  Operand ElementAt(int index) const {
    return Operand(esp, index * kPointerSize);
  }

  // Random-access store to a frame-top relative frame element.  The result
  // becomes owned by the frame and is invalidated.
  void SetElementAt(int index, Result* value);

  // Set a frame element to a constant.  The index is frame-top relative.
  void SetElementAt(int index, Handle<Object> value) {
    Result temp(value);
    SetElementAt(index, &temp);
  }

  void PushElementAt(int index) {
    PushFrameSlotAt(element_count() - index - 1);
  }

  void StoreToElementAt(int index) {
    StoreToFrameSlotAt(element_count() - index - 1);
  }

  // A frame-allocated local as an assembly operand.
  Operand LocalAt(int index) {
    ASSERT(0 <= index);
    ASSERT(index < local_count());
    return Operand(ebp, kLocal0Offset - index * kPointerSize);
  }

  // Push a copy of the value of a local frame slot on top of the frame.
  void PushLocalAt(int index) {
    PushFrameSlotAt(local0_index() + index);
  }

  // Push the value of a local frame slot on top of the frame and invalidate
  // the local slot.  The slot should be written to before trying to read
  // from it again.
  void TakeLocalAt(int index) {
    TakeFrameSlotAt(local0_index() + index);
  }

  // Store the top value on the virtual frame into a local frame slot.  The
  // value is left in place on top of the frame.
  void StoreToLocalAt(int index) {
    StoreToFrameSlotAt(local0_index() + index);
  }

  // Push the address of the receiver slot on the frame.
  void PushReceiverSlotAddress();

  // Push the function on top of the frame.
  void PushFunction() { PushFrameSlotAt(function_index()); }

  // Save the value of the esi register to the context frame slot.
  void SaveContextRegister();

  // Restore the esi register from the value of the context frame
  // slot.
  void RestoreContextRegister();

  // A parameter as an assembly operand.
  Operand ParameterAt(int index) {
    ASSERT(-1 <= index);  // -1 is the receiver.
    ASSERT(index < parameter_count());
    return Operand(ebp, (1 + parameter_count() - index) * kPointerSize);
  }

  // Push a copy of the value of a parameter frame slot on top of the frame.
  void PushParameterAt(int index) {
    PushFrameSlotAt(param0_index() + index);
  }

  // Push the value of a paramter frame slot on top of the frame and
  // invalidate the parameter slot.  The slot should be written to before
  // trying to read from it again.
  void TakeParameterAt(int index) {
    TakeFrameSlotAt(param0_index() + index);
  }

  // Store the top value on the virtual frame into a parameter frame slot.
  // The value is left in place on top of the frame.
  void StoreToParameterAt(int index) {
    StoreToFrameSlotAt(param0_index() + index);
  }

  // The receiver frame slot.
  Operand Receiver() { return ParameterAt(-1); }

  // Push a try-catch or try-finally handler on top of the virtual frame.
  void PushTryHandler(HandlerType type);

  // Call stub given the number of arguments it expects on (and
  // removes from) the stack.
  Result CallStub(CodeStub* stub, int arg_count) {
    PrepareForCall(arg_count, arg_count);
    return RawCallStub(stub);
  }

  // Call stub that takes a single argument passed in eax.  The
  // argument is given as a result which does not have to be eax or
  // even a register.  The argument is consumed by the call.
  Result CallStub(CodeStub* stub, Result* arg);

  // Call stub that takes a pair of arguments passed in edx (arg0) and
  // eax (arg1).  The arguments are given as results which do not have
  // to be in the proper registers or even in registers.  The
  // arguments are consumed by the call.
  Result CallStub(CodeStub* stub, Result* arg0, Result* arg1);

  // Call runtime given the number of arguments expected on (and
  // removed from) the stack.
  Result CallRuntime(Runtime::Function* f, int arg_count);
  Result CallRuntime(Runtime::FunctionId id, int arg_count);

  // Invoke builtin given the number of arguments it expects on (and
  // removes from) the stack.
  Result InvokeBuiltin(Builtins::JavaScript id,
                       InvokeFlag flag,
                       int arg_count);

  // Call load IC.  Name and receiver are found on top of the frame.
  // Receiver is not dropped.
  Result CallLoadIC(RelocInfo::Mode mode);

  // Call keyed load IC.  Key and receiver are found on top of the
  // frame.  They are not dropped.
  Result CallKeyedLoadIC(RelocInfo::Mode mode);

  // Call store IC.  Name, value, and receiver are found on top of the
  // frame.  Receiver is not dropped.
  Result CallStoreIC();

  // Call keyed store IC.  Value, key, and receiver are found on top
  // of the frame.  Key and receiver are not dropped.
  Result CallKeyedStoreIC();

  // Call call IC.  Arguments, reciever, and function name are found
  // on top of the frame.  Function name slot is not dropped.  The
  // argument count does not include the receiver.
  Result CallCallIC(RelocInfo::Mode mode, int arg_count, int loop_nesting);

  // Allocate and call JS function as constructor.  Arguments,
  // receiver (global object), and function are found on top of the
  // frame.  Function is not dropped.  The argument count does not
  // include the receiver.
  Result CallConstructor(int arg_count);

  // Drop a number of elements from the top of the expression stack.  May
  // emit code to affect the physical frame.  Does not clobber any registers
  // excepting possibly the stack pointer.
  void Drop(int count);

  // Drop one element.
  void Drop() { Drop(1); }

  // Duplicate the top element of the frame.
  void Dup() { PushFrameSlotAt(element_count() - 1); }

  // Pop an element from the top of the expression stack.  Returns a
  // Result, which may be a constant or a register.
  Result Pop();

  // Pop and save an element from the top of the expression stack and
  // emit a corresponding pop instruction.
  void EmitPop(Register reg);
  void EmitPop(Operand operand);

  // Push an element on top of the expression stack and emit a
  // corresponding push instruction.
  void EmitPush(Register reg);
  void EmitPush(Operand operand);
  void EmitPush(Immediate immediate);

  // Push an element on the virtual frame.
  void Push(Register reg, StaticType static_type = StaticType());
  void Push(Handle<Object> value);
  void Push(Smi* value) { Push(Handle<Object>(value)); }

  // Pushing a result invalidates it (its contents become owned by the
  // frame).
  void Push(Result* result) {
    if (result->is_register()) {
      Push(result->reg(), result->static_type());
    } else {
      ASSERT(result->is_constant());
      Push(result->handle());
    }
    result->Unuse();
  }

  // Nip removes zero or more elements from immediately below the top
  // of the frame, leaving the previous top-of-frame value on top of
  // the frame.  Nip(k) is equivalent to x = Pop(), Drop(k), Push(x).
  void Nip(int num_dropped);

 private:
  static const int kLocal0Offset = JavaScriptFrameConstants::kLocal0Offset;
  static const int kFunctionOffset = JavaScriptFrameConstants::kFunctionOffset;
  static const int kContextOffset = StandardFrameConstants::kContextOffset;

  static const int kHandlerSize = StackHandlerConstants::kSize / kPointerSize;
  static const int kPreallocatedElements = 5 + 8;  // 8 expression stack slots.

  ZoneList<FrameElement> elements_;

  // The index of the element that is at the processor's stack pointer
  // (the esp register).
  int stack_pointer_;

  // The index of the register frame element using each register, or
  // kIllegalIndex if a register is not on the frame.
  int register_locations_[RegisterAllocator::kNumRegisters];

  // The number of frame-allocated locals and parameters respectively.
  int parameter_count() { return cgen()->scope()->num_parameters(); }
  int local_count() { return cgen()->scope()->num_stack_slots(); }

  // The index of the element that is at the processor's frame pointer
  // (the ebp register).  The parameters, receiver, and return address
  // are below the frame pointer.
  int frame_pointer() { return parameter_count() + 2; }

  // The index of the first parameter.  The receiver lies below the first
  // parameter.
  int param0_index() { return 1; }

  // The index of the context slot in the frame.  It is immediately
  // above the frame pointer.
  int context_index() { return frame_pointer() + 1; }

  // The index of the function slot in the frame.  It is above the frame
  // pointer and the context slot.
  int function_index() { return frame_pointer() + 2; }

  // The index of the first local.  Between the frame pointer and the
  // locals lie the context and the function.
  int local0_index() { return frame_pointer() + 3; }

  // The index of the base of the expression stack.
  int expression_base_index() { return local0_index() + local_count(); }

  // Convert a frame index into a frame pointer relative offset into the
  // actual stack.
  int fp_relative(int index) {
    ASSERT(index < element_count());
    ASSERT(frame_pointer() < element_count());  // FP is on the frame.
    return (frame_pointer() - index) * kPointerSize;
  }

  // Record an occurrence of a register in the virtual frame.  This has the
  // effect of incrementing the register's external reference count and
  // of updating the index of the register's location in the frame.
  void Use(Register reg, int index) {
    ASSERT(!is_used(reg));
    set_register_location(reg, index);
    cgen()->allocator()->Use(reg);
  }

  // Record that a register reference has been dropped from the frame.  This
  // decrements the register's external reference count and invalidates the
  // index of the register's location in the frame.
  void Unuse(Register reg) {
    ASSERT(is_used(reg));
    set_register_location(reg, kIllegalIndex);
    cgen()->allocator()->Unuse(reg);
  }

  // Spill the element at a particular index---write it to memory if
  // necessary, free any associated register, and forget its value if
  // constant.
  void SpillElementAt(int index);

  // Sync the element at a particular index.  If it is a register or
  // constant that disagrees with the value on the stack, write it to memory.
  // Keep the element type as register or constant, and clear the dirty bit.
  void SyncElementAt(int index);

  // Sync a single unsynced element that lies beneath or at the stack pointer.
  void SyncElementBelowStackPointer(int index);

  // Sync a single unsynced element that lies just above the stack pointer.
  void SyncElementByPushing(int index);

  // Push a copy of a frame slot (typically a local or parameter) on top of
  // the frame.
  void PushFrameSlotAt(int index);

  // Push a the value of a frame slot (typically a local or parameter) on
  // top of the frame and invalidate the slot.
  void TakeFrameSlotAt(int index);

  // Store the value on top of the frame to a frame slot (typically a local
  // or parameter).
  void StoreToFrameSlotAt(int index);

  // Spill all elements in registers. Spill the top spilled_args elements
  // on the frame.  Sync all other frame elements.
  // Then drop dropped_args elements from the virtual frame, to match
  // the effect of an upcoming call that will drop them from the stack.
  void PrepareForCall(int spilled_args, int dropped_args);

  // Move frame elements currently in registers or constants, that
  // should be in memory in the expected frame, to memory.
  void MergeMoveRegistersToMemory(VirtualFrame* expected);

  // Make the register-to-register moves necessary to
  // merge this frame with the expected frame.
  // Register to memory moves must already have been made,
  // and memory to register moves must follow this call.
  // This is because some new memory-to-register moves are
  // created in order to break cycles of register moves.
  // Used in the implementation of MergeTo().
  void MergeMoveRegistersToRegisters(VirtualFrame* expected);

  // Make the memory-to-register and constant-to-register moves
  // needed to make this frame equal the expected frame.
  // Called after all register-to-memory and register-to-register
  // moves have been made.  After this function returns, the frames
  // should be equal.
  void MergeMoveMemoryToRegisters(VirtualFrame* expected);

  // Invalidates a frame slot (puts an invalid frame element in it).
  // Copies on the frame are correctly handled, and if this slot was
  // the backing store of copies, the index of the new backing store
  // is returned.  Otherwise, returns kIllegalIndex.
  // Register counts are correctly updated.
  int InvalidateFrameSlotAt(int index);

  // Call a code stub that has already been prepared for calling (via
  // PrepareForCall).
  Result RawCallStub(CodeStub* stub);

  // Calls a code object which has already been prepared for calling
  // (via PrepareForCall).
  Result RawCallCodeObject(Handle<Code> code, RelocInfo::Mode rmode);

  bool Equals(VirtualFrame* other);

  // Classes that need raw access to the elements_ array.
  friend class DeferredCode;
  friend class JumpTarget;
};


} }  // namespace v8::internal

#endif  // V8_IA32_VIRTUAL_FRAME_IA32_H_
