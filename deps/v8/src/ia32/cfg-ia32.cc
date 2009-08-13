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

#include "v8.h"

#include "cfg.h"
#include "codegen-inl.h"
#include "codegen-ia32.h"
#include "macro-assembler-ia32.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void InstructionBlock::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  {
    Comment cmt(masm, "[ InstructionBlock");
    for (int i = 0, len = instructions_.length(); i < len; i++) {
      // If the location of the current instruction is a temp, then the
      // instruction cannot be in tail position in the block.  Allocate the
      // temp based on peeking ahead to the next instruction.
      Instruction* instr = instructions_[i];
      Location* loc = instr->location();
      if (loc->is_temporary()) {
        instructions_[i+1]->FastAllocate(TempLocation::cast(loc));
      }
      instructions_[i]->Compile(masm);
    }
  }
  successor_->Compile(masm);
}


void EntryNode::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  Label deferred_enter, deferred_exit;
  {
    Comment cmnt(masm, "[ EntryNode");
    __ push(ebp);
    __ mov(ebp, esp);
    __ push(esi);
    __ push(edi);
    int count = CfgGlobals::current()->fun()->scope()->num_stack_slots();
    if (count > 0) {
      __ Set(eax, Immediate(Factory::undefined_value()));
      for (int i = 0; i < count; i++) {
        __ push(eax);
      }
    }
    if (FLAG_trace) {
      __ CallRuntime(Runtime::kTraceEnter, 0);
    }
    if (FLAG_check_stack) {
      ExternalReference stack_limit =
          ExternalReference::address_of_stack_guard_limit();
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      __ j(below, &deferred_enter);
      __ bind(&deferred_exit);
    }
  }
  successor_->Compile(masm);
  if (FLAG_check_stack) {
    Comment cmnt(masm, "[ Deferred Stack Check");
    __ bind(&deferred_enter);
    StackCheckStub stub;
    __ CallStub(&stub);
    __ jmp(&deferred_exit);
  }
}


void ExitNode::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  Comment cmnt(masm, "[ ExitNode");
  if (FLAG_trace) {
    __ push(eax);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  __ RecordJSReturn();
  __ mov(esp, ebp);
  __ pop(ebp);
  int count = CfgGlobals::current()->fun()->scope()->num_parameters();
  __ ret((count + 1) * kPointerSize);
}


void PropLoadInstr::Compile(MacroAssembler* masm) {
  // The key should not be on the stack---if it is a compiler-generated
  // temporary it is in the accumulator.
  ASSERT(!key()->is_on_stack());

  Comment cmnt(masm, "[ Load from Property");
  // If the key is known at compile-time we may be able to use a load IC.
  bool is_keyed_load = true;
  if (key()->is_constant()) {
    // Still use the keyed load IC if the key can be parsed as an integer so
    // we will get into the case that handles [] on string objects.
    Handle<Object> key_val = Constant::cast(key())->handle();
    uint32_t ignored;
    if (key_val->IsSymbol() &&
        !String::cast(*key_val)->AsArrayIndex(&ignored)) {
      is_keyed_load = false;
    }
  }

  if (!object()->is_on_stack()) object()->Push(masm);
  // A test eax instruction after the call indicates to the IC code that it
  // was inlined.  Ensure there is not one here.
  if (is_keyed_load) {
    key()->Push(masm);
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    __ pop(ebx);  // Discard key.
  } else {
    key()->Get(masm, ecx);
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
  }
  __ pop(ebx);  // Discard receiver.
  location()->Set(masm, eax);
}


void BinaryOpInstr::Compile(MacroAssembler* masm) {
  // The right-hand value should not be on the stack---if it is a
  // compiler-generated temporary it is in the accumulator.
  ASSERT(!right()->is_on_stack());

  Comment cmnt(masm, "[ BinaryOpInstr");
  // We can overwrite one of the operands if it is a temporary.
  OverwriteMode mode = NO_OVERWRITE;
  if (left()->is_temporary()) {
    mode = OVERWRITE_LEFT;
  } else if (right()->is_temporary()) {
    mode = OVERWRITE_RIGHT;
  }

  // Push both operands and call the specialized stub.
  if (!left()->is_on_stack()) left()->Push(masm);
  right()->Push(masm);
  GenericBinaryOpStub stub(op(), mode, SMI_CODE_IN_STUB);
  __ CallStub(&stub);
  location()->Set(masm, eax);
}


void ReturnInstr::Compile(MacroAssembler* masm) {
  // The location should be 'Effect'.  As a side effect, move the value to
  // the accumulator.
  Comment cmnt(masm, "[ ReturnInstr");
  value_->Get(masm, eax);
}


void Constant::Get(MacroAssembler* masm, Register reg) {
  __ mov(reg, Immediate(handle_));
}


void Constant::Push(MacroAssembler* masm) {
  __ push(Immediate(handle_));
}


static Operand ToOperand(SlotLocation* loc) {
  switch (loc->type()) {
    case Slot::PARAMETER: {
      int count = CfgGlobals::current()->fun()->scope()->num_parameters();
      return Operand(ebp, (1 + count - loc->index()) * kPointerSize);
    }
    case Slot::LOCAL: {
      const int kOffset = JavaScriptFrameConstants::kLocal0Offset;
      return Operand(ebp, kOffset - loc->index() * kPointerSize);
    }
    default:
      UNREACHABLE();
      return Operand(eax);
  }
}


void Constant::MoveToSlot(MacroAssembler* masm, SlotLocation* loc) {
  __ mov(ToOperand(loc), Immediate(handle_));
}


void SlotLocation::Get(MacroAssembler* masm, Register reg) {
  __ mov(reg, ToOperand(this));
}


void SlotLocation::Set(MacroAssembler* masm, Register reg) {
  __ mov(ToOperand(this), reg);
}


void SlotLocation::Push(MacroAssembler* masm) {
  __ push(ToOperand(this));
}


void SlotLocation::Move(MacroAssembler* masm, Value* value) {
  // We dispatch to the value because in some cases (temp or constant)
  // we can use a single instruction.
  value->MoveToSlot(masm, this);
}


void SlotLocation::MoveToSlot(MacroAssembler* masm, SlotLocation* loc) {
  // The accumulator is not live across a MoveInstr.
  __ mov(eax, ToOperand(this));
  __ mov(ToOperand(loc), eax);
}


void TempLocation::Get(MacroAssembler* masm, Register reg) {
  switch (where_) {
    case ACCUMULATOR:
      if (!reg.is(eax)) __ mov(reg, eax);
      break;
    case STACK:
      __ pop(reg);
      break;
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


void TempLocation::Set(MacroAssembler* masm, Register reg) {
  switch (where_) {
    case ACCUMULATOR:
      if (!reg.is(eax)) __ mov(eax, reg);
      break;
    case STACK:
      __ push(reg);
      break;
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


void TempLocation::Push(MacroAssembler* masm) {
  switch (where_) {
    case ACCUMULATOR:
      __ push(eax);
      break;
    case STACK:
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


void TempLocation::Move(MacroAssembler* masm, Value* value) {
  switch (where_) {
    case ACCUMULATOR:
      value->Get(masm, eax);
      break;
    case STACK:
      value->Push(masm);
      break;
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


void TempLocation::MoveToSlot(MacroAssembler* masm, SlotLocation* loc) {
  switch (where_) {
    case ACCUMULATOR:
      __ mov(ToOperand(loc), eax);
      break;
    case STACK:
      __ pop(ToOperand(loc));
      break;
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


#undef __

} }  // namespace v8::internal
