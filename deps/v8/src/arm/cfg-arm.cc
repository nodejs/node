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
#include "codegen-arm.h"  // Include after codegen-inl.h.
#include "macro-assembler-arm.h"

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
  {
    Comment cmnt(masm, "[ EntryNode");
    __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
    __ add(fp, sp, Operand(2 * kPointerSize));
    int count = CfgGlobals::current()->fun()->scope()->num_stack_slots();
    if (count > 0) {
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      for (int i = 0; i < count; i++) {
        __ push(ip);
      }
    }
    if (FLAG_trace) {
      __ CallRuntime(Runtime::kTraceEnter, 0);
    }
    if (FLAG_check_stack) {
      StackCheckStub stub;
      __ CallStub(&stub);
    }
  }
  successor_->Compile(masm);
}


void ExitNode::Compile(MacroAssembler* masm) {
  ASSERT(!is_marked());
  is_marked_ = true;
  Comment cmnt(masm, "[ ExitNode");
  if (FLAG_trace) {
    __ push(r0);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  int count = CfgGlobals::current()->fun()->scope()->num_parameters();
  __ add(sp, sp, Operand((count + 1) * kPointerSize));
  __ Jump(lr);
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

  if (is_keyed_load) {
    key()->Push(masm);
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Discard key and receiver.
    __ add(sp, sp, Operand(2 * kPointerSize));
  } else {
    key()->Get(masm, r2);
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    __ pop();  // Discard receiver.
  }
  location()->Set(masm, r0);
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

  // Move left to r1 and right to r0.
  left()->Get(masm, r1);
  right()->Get(masm, r0);
  GenericBinaryOpStub stub(op(), mode);
  __ CallStub(&stub);
  location()->Set(masm, r0);
}


void ReturnInstr::Compile(MacroAssembler* masm) {
  // The location should be 'Effect'.  As a side effect, move the value to
  // the accumulator.
  Comment cmnt(masm, "[ ReturnInstr");
  value()->Get(masm, r0);
}


void Constant::Get(MacroAssembler* masm, Register reg) {
  __ mov(reg, Operand(handle_));
}


void Constant::Push(MacroAssembler* masm) {
  __ mov(ip, Operand(handle_));
  __ push(ip);
}


static MemOperand ToMemOperand(SlotLocation* loc) {
  switch (loc->type()) {
    case Slot::PARAMETER: {
      int count = CfgGlobals::current()->fun()->scope()->num_parameters();
      return MemOperand(fp, (1 + count - loc->index()) * kPointerSize);
    }
    case Slot::LOCAL: {
      const int kOffset = JavaScriptFrameConstants::kLocal0Offset;
      return MemOperand(fp, kOffset - loc->index() * kPointerSize);
    }
    default:
      UNREACHABLE();
      return MemOperand(r0);
  }
}


void Constant::MoveToSlot(MacroAssembler* masm, SlotLocation* loc) {
  __ mov(ip, Operand(handle_));
  __ str(ip, ToMemOperand(loc));
}


void SlotLocation::Get(MacroAssembler* masm, Register reg) {
  __ ldr(reg, ToMemOperand(this));
}


void SlotLocation::Set(MacroAssembler* masm, Register reg) {
  __ str(reg, ToMemOperand(this));
}


void SlotLocation::Push(MacroAssembler* masm) {
  __ ldr(ip, ToMemOperand(this));
  __ push(ip);  // Push will not destroy ip.
}


void SlotLocation::Move(MacroAssembler* masm, Value* value) {
  // Double dispatch.
  value->MoveToSlot(masm, this);
}


void SlotLocation::MoveToSlot(MacroAssembler* masm, SlotLocation* loc) {
  __ ldr(ip, ToMemOperand(this));
  __ str(ip, ToMemOperand(loc));
}


void TempLocation::Get(MacroAssembler* masm, Register reg) {
  switch (where_) {
    case ACCUMULATOR:
      if (!reg.is(r0)) __ mov(reg, r0);
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
      if (!reg.is(r0)) __ mov(r0, reg);
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
      __ push(r0);
      break;
    case STACK:
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}


void TempLocation::Move(MacroAssembler* masm, Value* value) {
  switch (where_) {
    case ACCUMULATOR:
      value->Get(masm, r0);
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
      __ str(r0, ToMemOperand(loc));
    case STACK:
      __ pop(ip);
      __ str(ip, ToMemOperand(loc));
      break;
    case NOT_ALLOCATED:
      UNREACHABLE();
  }
}

#undef __

} }  // namespace v8::internal
