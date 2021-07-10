// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_
#define V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_

#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler.h"
#include "src/heap/memory-chunk.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/simd-shuffle.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

inline constexpr Condition ToCondition(LiftoffCondition liftoff_cond) {
  switch (liftoff_cond) {
    case kEqual:
      return eq;
    case kUnequal:
      return ne;
    case kSignedLessThan:
    case kUnsignedLessThan:
      return lt;
    case kSignedLessEqual:
    case kUnsignedLessEqual:
      return le;
    case kSignedGreaterEqual:
    case kUnsignedGreaterEqual:
      return ge;
    case kSignedGreaterThan:
    case kUnsignedGreaterThan:
      return gt;
  }
}

inline constexpr bool UseSignedOp(LiftoffCondition liftoff_cond) {
  switch (liftoff_cond) {
    case kEqual:
    case kUnequal:
    case kSignedLessThan:
    case kSignedLessEqual:
    case kSignedGreaterThan:
    case kSignedGreaterEqual:
      return true;
    case kUnsignedLessThan:
    case kUnsignedLessEqual:
    case kUnsignedGreaterThan:
    case kUnsignedGreaterEqual:
      return false;
    default:
      UNREACHABLE();
  }
  return false;
}

//  half
//  slot        Frame
//  -----+--------------------+---------------------------
//  n+3  |   parameter n      |
//  ...  |       ...          |
//   4   |   parameter 1      | or parameter 2
//   3   |   parameter 0      | or parameter 1
//   2   |  (result address)  | or parameter 0
//  -----+--------------------+---------------------------
//   1   | return addr (lr)   |
//   0   | previous frame (fp)|
//  -----+--------------------+  <-- frame ptr (fp)
//  -1   | 0xa: WASM          |
//  -2   |     instance       |
//  -----+--------------------+---------------------------
//  -3   |    slot 0 (high)   |   ^
//  -4   |    slot 0 (low)    |   |
//  -5   |    slot 1 (high)   | Frame slots
//  -6   |    slot 1 (low)    |   |
//       |                    |   v
//  -----+--------------------+  <-- stack ptr (sp)
//
constexpr int32_t kInstanceOffset = 2 * kSystemPointerSize;

inline MemOperand GetStackSlot(uint32_t offset) {
  return MemOperand(fp, -offset);
}

inline MemOperand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }


}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  lay(sp, MemOperand(sp));
  return offset;
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  Register scratch = r1;
  // Push the return address and frame pointer to complete the stack frame.
  lay(sp, MemOperand(sp, -2 * kSystemPointerSize));
  LoadU64(scratch, MemOperand(fp, kSystemPointerSize));
  StoreU64(scratch, MemOperand(sp, kSystemPointerSize));
  LoadU64(scratch, MemOperand(fp));
  StoreU64(scratch, MemOperand(sp));

  // Shift the whole frame upwards.
  int slot_count = num_callee_stack_params + 2;
  for (int i = slot_count - 1; i >= 0; --i) {
    LoadU64(scratch, MemOperand(sp, i * kSystemPointerSize));
    StoreU64(scratch,
             MemOperand(fp, (i - stack_param_delta) * kSystemPointerSize));
  }

  // Set the new stack and frame pointer.
  lay(sp, MemOperand(fp, -stack_param_delta * kSystemPointerSize));
  Pop(r14, fp);
}

void LiftoffAssembler::AlignFrameSize() {}

void LiftoffAssembler::PatchPrepareStackFrame(int offset) {
  int frame_size = GetTotalFrameSize() - kSystemPointerSize;

  constexpr int LayInstrSize = 6;

#ifdef USE_SIMULATOR
  // When using the simulator, deal with Liftoff which allocates the stack
  // before checking it.
  // TODO(arm): Remove this when the stack check mechanism will be updated.
  if (frame_size > KB / 2) {
    bailout(kOtherReason,
            "Stack limited to 512 bytes to avoid a bug in StackCheck");
    return;
  }
#endif
  Assembler patching_assembler(
      AssemblerOptions{},
      ExternalAssemblerBuffer(buffer_start_ + offset, LayInstrSize + kGap));
  patching_assembler.lay(sp, MemOperand(sp, -frame_size));
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() { AbortedCodeGeneration(); }

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return liftoff::kInstanceOffset;
}

int LiftoffAssembler::SlotSizeForType(ValueKind kind) {
  switch (kind) {
    case kS128:
      return element_size_bytes(kind);
    default:
      return kStackSlotSize;
  }
}

bool LiftoffAssembler::NeedsAlignment(ValueKind kind) {
  return (kind == kS128 || is_reference(kind));
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type().kind()) {
    case kI32:
      mov(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kI64:
      mov(reg.gp(), Operand(value.to_i64(), rmode));
      break;
    case kF32: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadF32(reg.fp(), value.to_f32_boxed().get_scalar(), scratch);
      break;
    }
    case kF64: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadF64(reg.fp(), value.to_f64_boxed().get_bits(), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceFromFrame(Register dst) {
  LoadU64(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int offset, int size) {
  DCHECK_LE(0, offset);
  switch (size) {
    case 1:
      LoadU8(dst, MemOperand(instance, offset));
      break;
    case 4:
      LoadU32(dst, MemOperand(instance, offset));
      break;
    case 8:
      LoadU64(dst, MemOperand(instance, offset));
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     Register instance,
                                                     int offset) {
  DCHECK_LE(0, offset);
  LoadTaggedPointerField(dst, MemOperand(instance, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  StoreU64(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  LoadU64(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         LiftoffRegList pinned) {
  CHECK(is_int20(offset_imm));
  LoadTaggedPointerField(
      dst,
      MemOperand(src_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  UseScratchRegisterScope temps(this);
  LoadU64(dst, MemOperand(src_addr, offset_imm), r1);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm,
                                          LiftoffRegister src,
                                          LiftoffRegList pinned,
                                          SkipWriteBarrier skip_write_barrier) {
  MemOperand dst_op =
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  StoreTaggedField(src.gp(), dst_op);

  if (skip_write_barrier) return;

  Label write_barrier;
  Label exit;
  CheckPageFlag(dst_addr, r1, MemoryChunk::kPointersFromHereAreInterestingMask,
                ne, &write_barrier);
  b(&exit);
  bind(&write_barrier);
  JumpIfSmi(src.gp(), &exit);
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(src.gp(), src.gp());
  }
  CheckPageFlag(src.gp(), r1, MemoryChunk::kPointersToHereAreInterestingMask,
                eq, &exit);
  lay(r1, dst_op);
  CallRecordWriteStub(dst_addr, r1, EMIT_REMEMBERED_SET, kSaveFPRegs,
                      wasm::WasmCode::kRecordWrite);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem,
                            bool i64_offset) {
  UseScratchRegisterScope temps(this);
  if (!is_int20(offset_imm)) {
    mov(ip, Operand(offset_imm));
    if (offset_reg != no_reg) {
      if (!i64_offset) {
        // Clear the upper 32 bits of the 64 bit offset register.
        llgfr(r0, offset_reg);
        offset_reg = r0;
      }
      AddS64(ip, offset_reg);
    }
    offset_reg = ip;
    offset_imm = 0;
  }
  MemOperand src_op =
      MemOperand(src_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      LoadU8(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
    case LoadType::kI64Load8S:
      LoadS8(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      if (is_load_mem) {
        LoadU16LE(dst.gp(), src_op);
      } else {
        LoadU16(dst.gp(), src_op);
      }
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      if (is_load_mem) {
        LoadS16LE(dst.gp(), src_op);
      } else {
        LoadS16(dst.gp(), src_op);
      }
      break;
    case LoadType::kI64Load32U:
      if (is_load_mem) {
        LoadU32LE(dst.gp(), src_op);
      } else {
        LoadU32(dst.gp(), src_op);
      }
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      if (is_load_mem) {
        LoadS32LE(dst.gp(), src_op);
      } else {
        LoadS32(dst.gp(), src_op);
      }
      break;
    case LoadType::kI64Load:
      if (is_load_mem) {
        LoadU64LE(dst.gp(), src_op);
      } else {
        LoadU64(dst.gp(), src_op);
      }
      break;
    case LoadType::kF32Load:
      if (is_load_mem) {
        LoadF32LE(dst.fp(), src_op, r0);
      } else {
        LoadF32(dst.fp(), src_op);
      }
      break;
    case LoadType::kF64Load:
      if (is_load_mem) {
        LoadF64LE(dst.fp(), src_op, r0);
      } else {
        LoadF64(dst.fp(), src_op);
      }
      break;
    case LoadType::kS128Load:
      if (is_load_mem) {
        LoadV128LE(dst.fp(), src_op, r0, r1);
      } else {
        LoadV128(dst.fp(), src_op, r0);
      }
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  if (!is_int20(offset_imm)) {
    mov(ip, Operand(offset_imm));
    if (offset_reg != no_reg) {
      AddS64(ip, offset_reg);
    }
    offset_reg = ip;
    offset_imm = 0;
  }
  MemOperand dst_op =
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      StoreU8(src.gp(), dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      if (is_store_mem) {
        StoreU16LE(src.gp(), dst_op, r1);
      } else {
        StoreU16(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      if (is_store_mem) {
        StoreU32LE(src.gp(), dst_op, r1);
      } else {
        StoreU32(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kI64Store:
      if (is_store_mem) {
        StoreU64LE(src.gp(), dst_op, r1);
      } else {
        StoreU64(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kF32Store:
      if (is_store_mem) {
        StoreF32LE(src.fp(), dst_op, r1);
      } else {
        StoreF32(src.fp(), dst_op);
      }
      break;
    case StoreType::kF64Store:
      if (is_store_mem) {
        StoreF64LE(src.fp(), dst_op, r1);
      } else {
        StoreF64(src.fp(), dst_op);
      }
      break;
    case StoreType::kS128Store: {
      if (is_store_mem) {
        StoreV128LE(src.fp(), dst_op, r0, r1);
      } else {
        StoreV128(src.fp(), dst_op, r1);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  Load(dst, src_addr, offset_reg, offset_imm, type, pinned, nullptr, true);
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uintptr_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicExchangeU8(ip, src.gp(), r1, r0);
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, src.gp());
      ShiftRightU32(r1, r1, Operand(16));
#else
      LoadU16(r1, src.gp());
#endif
      Push(r2);
      AtomicExchangeU16(ip, r1, r2, r0);
      Pop(r2);
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, src.gp());
#else
      LoadU32(r1, src.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      cs(r0, r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
      break;
    }
    case StoreType::kI64Store: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(r1, src.gp());
#else
      mov(r1, src.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      csg(r0, r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  Register tmp1 =
      GetUnusedRegister(
          kGpReg, LiftoffRegList::ForRegs(dst_addr, offset_reg, value, result))
          .gp();
  Register tmp2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst_addr, offset_reg,
                                                        value, result, tmp1))
          .gp();

  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label doadd;
      bind(&doadd);
      LoadU8(tmp1, MemOperand(ip));
      AddS32(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &doadd);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label doadd;
      bind(&doadd);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      AddS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      AddS32(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &doadd);
      LoadU16(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label doadd;
      bind(&doadd);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      AddS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      AddS32(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &doadd);
      LoadU32(result.gp(), tmp1);
      break;
    }
    case StoreType::kI64Store: {
      Label doadd;
      bind(&doadd);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      AddS64(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      AddS64(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &doadd);
      mov(result.gp(), tmp1);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  Register tmp1 =
      GetUnusedRegister(
          kGpReg, LiftoffRegList::ForRegs(dst_addr, offset_reg, value, result))
          .gp();
  Register tmp2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(dst_addr, offset_reg,
                                                        value, result, tmp1))
          .gp();

  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label do_again;
      bind(&do_again);
      LoadU8(tmp1, MemOperand(ip));
      SubS32(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label do_again;
      bind(&do_again);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      SubS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      SubS32(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU16(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label do_again;
      bind(&do_again);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      SubS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      SubS32(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      LoadU32(result.gp(), tmp1);
      break;
    }
    case StoreType::kI64Store: {
      Label do_again;
      bind(&do_again);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      SubS64(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      SubS64(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      mov(result.gp(), tmp1);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicAnd");
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uintptr_t offset_imm, LiftoffRegister value,
                                LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicOr");
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicXor");
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uintptr_t offset_imm,
                                      LiftoffRegister value,
                                      LiftoffRegister result, StoreType type) {
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicExchangeU8(ip, value.gp(), result.gp(), r0);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, value.gp());
      ShiftRightU32(r1, r1, Operand(16));
#else
      LoadU16(r1, value.gp());
#endif
      AtomicExchangeU16(ip, r1, result.gp(), r0);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#else
      LoadU16(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, value.gp());
#else
      LoadU32(r1, value.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      cs(result.gp(), r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      LoadU32(result.gp(), result.gp());
      break;
    }
    case StoreType::kI64Store: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(r1, value.gp());
#else
      mov(r1, value.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      csg(result.gp(), r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicCmpExchangeU8(ip, result.gp(), expected.gp(), new_value.gp(), r0,
                          r1);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Push(r2, r3);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r2, expected.gp());
      lrvr(r3, new_value.gp());
      ShiftRightU32(r2, r2, Operand(16));
      ShiftRightU32(r3, r3, Operand(16));
#else
      LoadU16(r2, expected.gp());
      LoadU16(r3, new_value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), r2, r3, r0, r1);
      LoadU16(result.gp(), result.gp());
      Pop(r2, r3);
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Push(r2, r3);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r2, expected.gp());
      lrvr(r3, new_value.gp());
#else
      LoadU32(r2, expected.gp());
      LoadU32(r3, new_value.gp());
#endif
      CmpAndSwap(r2, r3, MemOperand(ip));
      LoadU32(result.gp(), r2);
      Pop(r2, r3);
      break;
    }
    case StoreType::kI64Store: {
      Push(r2, r3);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(r2, expected.gp());
      lrvgr(r3, new_value.gp());
#else
      mov(r2, expected.gp());
      mov(r3, new_value.gp());
#endif
      CmpAndSwap64(r2, r3, MemOperand(ip));
      mov(result.gp(), r2);
      Pop(r2, r3);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicFence() { bailout(kAtomics, "AtomicFence"); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  int32_t offset = (caller_slot_idx + 1) * 8;
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      LoadS32(dst.gp(), MemOperand(fp, offset + 4));
      break;
#else
      LoadS32(dst.gp(), MemOperand(fp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kOptRef:
    case kRttWithDepth:
    case kI64: {
      LoadU64(dst.gp(), MemOperand(fp, offset));
      break;
    }
    case kF32: {
      LoadF32(dst.fp(), MemOperand(fp, offset));
      break;
    }
    case kF64: {
      LoadF64(dst.fp(), MemOperand(fp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(dst.fp(), MemOperand(fp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind) {
  int32_t offset = (caller_slot_idx + 1) * 8;
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      StoreU32(src.gp(), MemOperand(fp, offset + 4));
      break;
#else
      StoreU32(src.gp(), MemOperand(fp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kOptRef:
    case kI64: {
      StoreU64(src.gp(), MemOperand(fp, offset));
      break;
    }
    case kF32: {
      StoreF32(src.fp(), MemOperand(fp, offset));
      break;
    }
    case kF64: {
      StoreF64(src.fp(), MemOperand(fp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      StoreV128(src.fp(), MemOperand(fp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      LoadS32(dst.gp(), MemOperand(sp, offset + 4));
      break;
#else
      LoadS32(dst.gp(), MemOperand(sp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kOptRef:
    case kRttWithDepth:
    case kI64: {
      LoadU64(dst.gp(), MemOperand(sp, offset));
      break;
    }
    case kF32: {
      LoadF32(dst.fp(), MemOperand(sp, offset));
      break;
    }
    case kF64: {
      LoadF64(dst.fp(), MemOperand(sp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(dst.fp(), MemOperand(sp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

#ifdef V8_TARGET_BIG_ENDIAN
constexpr int stack_bias = -4;
#else
constexpr int stack_bias = 0;
#endif

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  DCHECK_NE(dst_offset, src_offset);
  int length = 0;
  switch (kind) {
    case kI32:
    case kF32:
      length = 4;
      break;
    case kI64:
    case kOptRef:
    case kRef:
    case kRtt:
    case kF64:
      length = 8;
      break;
    case kS128:
      length = 16;
      break;
    default:
      UNREACHABLE();
  }

  dst_offset += (length == 4 ? stack_bias : 0);
  src_offset += (length == 4 ? stack_bias : 0);

  if (is_int20(dst_offset)) {
    lay(ip, liftoff::GetStackSlot(dst_offset));
  } else {
    mov(ip, Operand(-dst_offset));
    lay(ip, MemOperand(fp, ip));
  }

  if (is_int20(src_offset)) {
    lay(r1, liftoff::GetStackSlot(src_offset));
  } else {
    mov(r1, Operand(-src_offset));
    lay(r1, MemOperand(fp, r1));
  }

  MoveChar(MemOperand(ip), MemOperand(r1), Operand(length));
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  if (kind == kF32) {
    ler(dst, src);
  } else if (kind == kF64) {
    ldr(dst, src);
  } else {
    DCHECK_EQ(kS128, kind);
    vlr(dst, src, Condition(0), Condition(0), Condition(0));
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  DCHECK_LT(0, offset);
  RecordUsedSpillOffset(offset);

  switch (kind) {
    case kI32:
      StoreU32(reg.gp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kI64:
    case kOptRef:
    case kRef:
    case kRtt:
    case kRttWithDepth:
      StoreU64(reg.gp(), liftoff::GetStackSlot(offset));
      break;
    case kF32:
      StoreF32(reg.fp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kF64:
      StoreF64(reg.fp(), liftoff::GetStackSlot(offset));
      break;
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      StoreV128(reg.fp(), liftoff::GetStackSlot(offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  UseScratchRegisterScope temps(this);
  Register src = no_reg;
  src = ip;
  switch (value.type().kind()) {
    case kI32: {
      mov(src, Operand(value.to_i32()));
      StoreU32(src, liftoff::GetStackSlot(offset + stack_bias));
      break;
    }
    case kI64: {
      mov(src, Operand(value.to_i64()));
      StoreU64(src, liftoff::GetStackSlot(offset));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueKind kind) {
  switch (kind) {
    case kI32:
      LoadS32(reg.gp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kI64:
    case kRef:
    case kOptRef:
    case kRtt:
    case kRttWithDepth:
      LoadU64(reg.gp(), liftoff::GetStackSlot(offset));
      break;
    case kF32:
      LoadF32(reg.fp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kF64:
      LoadF64(reg.fp(), liftoff::GetStackSlot(offset));
      break;
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(reg.fp(), liftoff::GetStackSlot(offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register, int offset, RegPairHalf) {
  UNREACHABLE();
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  DCHECK_LT(0, size);
  DCHECK_EQ(0, size % 4);
  RecordUsedSpillOffset(start + size);

  // We need a zero reg. Always use r0 for that, and push it before to restore
  // its value afterwards.
  push(r0);
  mov(r0, Operand(0));

  if (size <= 5 * kStackSlotSize) {
    // Special straight-line code for up to five slots. Generates two
    // instructions per slot.
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      StoreU64(r0, liftoff::GetStackSlot(start + remainder));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      StoreU32(r0, liftoff::GetStackSlot(start + remainder));
    }
  } else {
    // General case for bigger counts (9 instructions).
    // Use r3 for start address (inclusive), r4 for end address (exclusive).
    push(r3);
    push(r4);

    lay(r3, MemOperand(fp, -start - size));
    lay(r4, MemOperand(fp, -start));

    Label loop;
    bind(&loop);
    StoreU64(r0, MemOperand(r3));
    lay(r3, MemOperand(r3, kSystemPointerSize));
    CmpU64(r3, r4);
    bne(&loop);
    pop(r4);
    pop(r3);
  }

  pop(r0);
}

#define SIGN_EXT(r) lgfr(r, r)
#define INT32_AND_WITH_1F(x) Operand(x & 0x1f)
#define REGISTER_AND_WITH_1F    \
  ([&](Register rhs) {          \
    AndP(r1, rhs, Operand(31)); \
    return r1;                  \
  })

#define LFR_TO_REG(reg) reg.gp()

// V(name, instr, dtype, stype, dcast, scast, rcast, return_val, return_type)
#define UNOP_LIST(V)                                                           \
  V(i32_popcnt, Popcnt32, Register, Register, , , USE, true, bool)             \
  V(i64_popcnt, Popcnt64, LiftoffRegister, LiftoffRegister, LFR_TO_REG,        \
    LFR_TO_REG, USE, true, bool)                                               \
  V(u32_to_intptr, LoadU32, Register, Register, , , USE, , void)               \
  V(i32_signextend_i8, lbr, Register, Register, , , USE, , void)               \
  V(i32_signextend_i16, lhr, Register, Register, , , USE, , void)              \
  V(i64_signextend_i8, lgbr, LiftoffRegister, LiftoffRegister, LFR_TO_REG,     \
    LFR_TO_REG, USE, , void)                                                   \
  V(i64_signextend_i16, lghr, LiftoffRegister, LiftoffRegister, LFR_TO_REG,    \
    LFR_TO_REG, USE, , void)                                                   \
  V(i64_signextend_i32, LoadS32, LiftoffRegister, LiftoffRegister, LFR_TO_REG, \
    LFR_TO_REG, USE, , void)                                                   \
  V(i32_clz, CountLeadingZerosU32, Register, Register, , , USE, , void)        \
  V(i32_ctz, CountTrailingZerosU32, Register, Register, , , USE, , void)       \
  V(i64_clz, CountLeadingZerosU64, LiftoffRegister, LiftoffRegister,           \
    LFR_TO_REG, LFR_TO_REG, USE, , void)                                       \
  V(i64_ctz, CountTrailingZerosU64, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, USE, , void)                                       \
  V(f32_abs, lpebr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f32_neg, lcebr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f32_sqrt, sqebr, DoubleRegister, DoubleRegister, , , USE, , void)          \
  V(f64_abs, lpdbr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f64_neg, lcdbr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f64_sqrt, sqdbr, DoubleRegister, DoubleRegister, , , USE, , void)

#define EMIT_UNOP_FUNCTION(name, instr, dtype, stype, dcast, scast, rcast, \
                           ret, return_type)                               \
  return_type LiftoffAssembler::emit_##name(dtype dst, stype src) {        \
    auto _dst = dcast(dst);                                                \
    auto _src = scast(src);                                                \
    instr(_dst, _src);                                                     \
    rcast(_dst);                                                           \
    return ret;                                                            \
  }
UNOP_LIST(EMIT_UNOP_FUNCTION)
#undef EMIT_UNOP_FUNCTION
#undef UNOP_LIST

// V(name, instr, dtype, stype1, stype2, dcast, scast1, scast2, rcast,
// return_val, return_type)
#define BINOP_LIST(V)                                                          \
  V(f64_add, AddF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_sub, SubF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_mul, MulF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_div, DivF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_add, AddF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_sub, SubF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_mul, MulF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_div, DivF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(i32_shli, ShiftLeftU32, Register, Register, int32_t, , ,                   \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_sari, ShiftRightS32, Register, Register, int32_t, , ,                  \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_shri, ShiftRightU32, Register, Register, int32_t, , ,                  \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_shl, ShiftLeftU32, Register, Register, Register, , ,                   \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_sar, ShiftRightS32, Register, Register, Register, , ,                  \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_shr, ShiftRightU32, Register, Register, Register, , ,                  \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_addi, AddS32, Register, Register, int32_t, , , Operand, SIGN_EXT, ,    \
    void)                                                                      \
  V(i32_subi, SubS32, Register, Register, int32_t, , , Operand, SIGN_EXT, ,    \
    void)                                                                      \
  V(i32_andi, And, Register, Register, int32_t, , , Operand, SIGN_EXT, , void) \
  V(i32_ori, Or, Register, Register, int32_t, , , Operand, SIGN_EXT, , void)   \
  V(i32_xori, Xor, Register, Register, int32_t, , , Operand, SIGN_EXT, , void) \
  V(i32_add, AddS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i32_sub, SubS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i32_and, And, Register, Register, Register, , , , SIGN_EXT, , void)        \
  V(i32_or, Or, Register, Register, Register, , , , SIGN_EXT, , void)          \
  V(i32_xor, Xor, Register, Register, Register, , , , SIGN_EXT, , void)        \
  V(i32_mul, MulS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i64_add, AddS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_sub, SubS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_mul, MulS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_and, AndP, LiftoffRegister, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_or, OrP, LiftoffRegister, LiftoffRegister, LiftoffRegister,            \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_xor, XorP, LiftoffRegister, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_shl, ShiftLeftU64, LiftoffRegister, LiftoffRegister, Register,         \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_sar, ShiftRightS64, LiftoffRegister, LiftoffRegister, Register,        \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_shr, ShiftRightU64, LiftoffRegister, LiftoffRegister, Register,        \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_addi, AddS64, LiftoffRegister, LiftoffRegister, int64_t, LFR_TO_REG,   \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_andi, AndP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,     \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_ori, OrP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,       \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_xori, XorP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,     \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_shli, ShiftLeftU64, LiftoffRegister, LiftoffRegister, int32_t,         \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)                              \
  V(i64_sari, ShiftRightS64, LiftoffRegister, LiftoffRegister, int32_t,        \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)                              \
  V(i64_shri, ShiftRightU64, LiftoffRegister, LiftoffRegister, int32_t,        \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)

#define EMIT_BINOP_FUNCTION(name, instr, dtype, stype1, stype2, dcast, scast1, \
                            scast2, rcast, ret, return_type)                   \
  return_type LiftoffAssembler::emit_##name(dtype dst, stype1 lhs,             \
                                            stype2 rhs) {                      \
    auto _dst = dcast(dst);                                                    \
    auto _lhs = scast1(lhs);                                                   \
    auto _rhs = scast2(rhs);                                                   \
    instr(_dst, _lhs, _rhs);                                                   \
    rcast(_dst);                                                               \
    return ret;                                                                \
  }

BINOP_LIST(EMIT_BINOP_FUNCTION)
#undef BINOP_LIST
#undef EMIT_BINOP_FUNCTION
#undef SIGN_EXT
#undef INT32_AND_WITH_1F
#undef REGISTER_AND_WITH_1F
#undef LFR_TO_REG

bool LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  fiebra(ROUND_TOWARD_POS_INF, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  fiebra(ROUND_TOWARD_NEG_INF, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  fiebra(ROUND_TOWARD_0, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  fiebra(ROUND_TO_NEAREST_TO_EVEN, dst, src);
  return true;
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(VECTOR_ENHANCE_FACILITY_1)) {
    vfmin(dst, lhs, rhs, Condition(1), Condition(8), Condition(3));
    return;
  }
  DoubleMin(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(VECTOR_ENHANCE_FACILITY_1)) {
    vfmin(dst, lhs, rhs, Condition(1), Condition(8), Condition(2));
    return;
  }
  FloatMin(dst, lhs, rhs);
}

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  fidbra(ROUND_TOWARD_POS_INF, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  fidbra(ROUND_TOWARD_NEG_INF, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  fidbra(ROUND_TOWARD_0, dst, src);
  return true;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  fidbra(ROUND_TO_NEAREST_TO_EVEN, dst, src);
  return true;
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(VECTOR_ENHANCE_FACILITY_1)) {
    vfmax(dst, lhs, rhs, Condition(1), Condition(8), Condition(3));
    return;
  }
  DoubleMax(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(VECTOR_ENHANCE_FACILITY_1)) {
    vfmax(dst, lhs, rhs, Condition(1), Condition(8), Condition(2));
    return;
  }
  FloatMax(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  Label cont;

  // Check for division by zero.
  ltr(r0, rhs);
  b(eq, trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS32(rhs, Operand(-1));
  bne(&cont);
  CmpS32(lhs, Operand(kMinInt));
  b(eq, trap_div_unrepresentable);

  bind(&cont);
  DivS32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);
  DivU32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  Label cont;
  Label done;
  Label trap_div_unrepresentable;
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);

  // Check kMinInt/-1 case.
  CmpS32(rhs, Operand(-1));
  bne(&cont);
  CmpS32(lhs, Operand(kMinInt));
  beq(&trap_div_unrepresentable);

  // Continue noraml calculation.
  bind(&cont);
  ModS32(dst, lhs, rhs);
  bne(&done);

  // trap by kMinInt/-1 case.
  bind(&trap_div_unrepresentable);
  mov(dst, Operand(0));
  bind(&done);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);
  ModU32(dst, lhs, rhs);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  // Use r0 to check for kMinInt / -1.
  constexpr int64_t kMinInt64 = static_cast<int64_t>(1) << 63;
  Label cont;
  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS64(rhs.gp(), Operand(-1));
  bne(&cont);
  mov(r0, Operand(kMinInt64));
  CmpS64(lhs.gp(), r0);
  b(eq, trap_div_unrepresentable);

  bind(&cont);
  DivS64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  ltgr(r0, rhs.gp());
  b(eq, trap_div_by_zero);
  // Do div.
  DivU64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  constexpr int64_t kMinInt64 = static_cast<int64_t>(1) << 63;

  Label trap_div_unrepresentable;
  Label done;
  Label cont;

  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS64(rhs.gp(), Operand(-1));
  bne(&cont);
  mov(r0, Operand(kMinInt64));
  CmpS64(lhs.gp(), r0);
  beq(&trap_div_unrepresentable);

  bind(&cont);
  ModS64(dst.gp(), lhs.gp(), rhs.gp());
  bne(&done);

  bind(&trap_div_unrepresentable);
  mov(dst.gp(), Operand(0));
  bind(&done);
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);
  ModU64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint64_t kF64SignBit = uint64_t{1} << 63;
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.Acquire();
  MovDoubleToInt64(r0, lhs);
  // Clear sign bit in {r0}.
  AndP(r0, Operand(~kF64SignBit));

  MovDoubleToInt64(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  AndP(scratch2, Operand(kF64SignBit));
  // Combine {scratch2} into {r0}.
  OrP(r0, r0, scratch2);
  MovInt64ToDouble(dst, r0);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint64_t kF64SignBit = uint64_t{1} << 63;
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.Acquire();
  MovDoubleToInt64(r0, lhs);
  // Clear sign bit in {r0}.
  AndP(r0, Operand(~kF64SignBit));

  MovDoubleToInt64(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  AndP(scratch2, Operand(kF64SignBit));
  // Combine {scratch2} into {r0}.
  OrP(r0, r0, scratch2);
  MovInt64ToDouble(dst, r0);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      lgfr(dst.gp(), src.gp());
      return true;
    case kExprI32SConvertF32: {
      ConvertFloat32ToInt32(dst.gp(), src.fp(),
                            kRoundToZero);  // f32 -> i32 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI32UConvertF32: {
      ConvertFloat32ToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(Condition(1), trap);
      return true;
    }
    case kExprI32SConvertF64: {
      ConvertDoubleToInt32(dst.gp(), src.fp());
      b(Condition(1), trap);
      return true;
    }
    case kExprI32UConvertF64: {
      ConvertDoubleToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(Condition(1), trap);
      return true;
    }
    case kExprI32SConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToInt32(dst.gp(), src.fp(),
                            kRoundToZero);  // f32 -> i32 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32UConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32SConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToInt32(dst.gp(), src.fp());
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32UConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToUnsignedInt32(dst.gp(), src.fp());
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32ReinterpretF32:
      lgdr(dst.gp(), src.fp());
      srlg(dst.gp(), dst.gp(), Operand(32));
      return true;
    case kExprI64SConvertI32:
      LoadS32(dst.gp(), src.gp());
      return true;
    case kExprI64UConvertI32:
      llgfr(dst.gp(), src.gp());
      return true;
    case kExprI64ReinterpretF64:
      lgdr(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      ConvertIntToFloat(dst.fp(), src.gp());
      return true;
    }
    case kExprF32UConvertI32: {
      ConvertUnsignedIntToFloat(dst.fp(), src.gp());
      return true;
    }
    case kExprF32ConvertF64:
      ledbr(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32: {
      sllg(r0, src.gp(), Operand(32));
      ldgr(dst.fp(), r0);
      return true;
    }
    case kExprF64SConvertI32: {
      ConvertIntToDouble(dst.fp(), src.gp());
      return true;
    }
    case kExprF64UConvertI32: {
      ConvertUnsignedIntToDouble(dst.fp(), src.gp());
      return true;
    }
    case kExprF64ConvertF32:
      ldebr(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      ldgr(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI64:
      ConvertInt64ToDouble(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI64:
      ConvertUnsignedInt64ToDouble(dst.fp(), src.gp());
      return true;
    case kExprI64SConvertF32: {
      ConvertFloat32ToInt64(dst.gp(), src.fp());  // f32 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64UConvertF32: {
      ConvertFloat32ToUnsignedInt64(dst.gp(),
                                    src.fp());  // f32 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprF32SConvertI64:
      ConvertInt64ToFloat(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI64:
      ConvertUnsignedInt64ToFloat(dst.fp(), src.gp());
      return true;
    case kExprI64SConvertF64: {
      ConvertDoubleToInt64(dst.gp(), src.fp());  // f64 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64UConvertF64: {
      ConvertDoubleToUnsignedInt64(dst.gp(),
                                   src.fp());  // f64 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64SConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToInt64(dst.gp(), src.fp());  // f32 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64UConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToUnsignedInt64(dst.gp(),
                                    src.fp());  // f32 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64SConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToInt64(dst.gp(), src.fp());  // f64 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64UConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToUnsignedInt64(dst.gp(),
                                   src.fp());  // f64 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::emit_jump(Label* label) { b(al, label); }

void LiftoffAssembler::emit_jump(Register target) { Jump(target); }

void LiftoffAssembler::emit_cond_jump(LiftoffCondition liftoff_cond,
                                      Label* label, ValueKind kind,
                                      Register lhs, Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  bool use_signed = liftoff::UseSignedOp(liftoff_cond);

  if (rhs != no_reg) {
    switch (kind) {
      case kI32:
        if (use_signed) {
          CmpS32(lhs, rhs);
        } else {
          CmpU32(lhs, rhs);
        }
        break;
      case kRef:
      case kOptRef:
      case kRtt:
      case kRttWithDepth:
        DCHECK(liftoff_cond == kEqual || liftoff_cond == kUnequal);
        V8_FALLTHROUGH;
      case kI64:
        if (use_signed) {
          CmpS64(lhs, rhs);
        } else {
          CmpU64(lhs, rhs);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(kind, kI32);
    CHECK(use_signed);
    CmpS32(lhs, Operand::Zero());
  }

  b(cond, label);
}

void LiftoffAssembler::emit_i32_cond_jumpi(LiftoffCondition liftoff_cond,
                                           Label* label, Register lhs,
                                           int32_t imm) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  CmpS32(lhs, Operand(imm));
  b(cond, label);
}

#define EMIT_EQZ(test, src) \
  {                         \
    Label done;             \
    test(r0, src);          \
    mov(dst, Operand(1));   \
    beq(&done);             \
    mov(dst, Operand(0));   \
    bind(&done);            \
  }

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  EMIT_EQZ(ltr, src);
}

#define EMIT_SET_CONDITION(dst, cond) \
  {                                   \
    Label done;                       \
    lghi(dst, Operand(1));            \
    b(cond, &done);                   \
    lghi(dst, Operand(0));            \
    bind(&done);                      \
  }

void LiftoffAssembler::emit_i32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, Register lhs,
                                         Register rhs) {
  bool use_signed = liftoff::UseSignedOp(liftoff_cond);
  if (use_signed) {
    CmpS32(lhs, rhs);
  } else {
    CmpU32(lhs, rhs);
  }

  EMIT_SET_CONDITION(dst, liftoff::ToCondition(liftoff_cond));
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  EMIT_EQZ(ltgr, src.gp());
}

void LiftoffAssembler::emit_i64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  bool use_signed = liftoff::UseSignedOp(liftoff_cond);
  if (use_signed) {
    CmpS64(lhs.gp(), rhs.gp());
  } else {
    CmpU64(lhs.gp(), rhs.gp());
  }

  EMIT_SET_CONDITION(dst, liftoff::ToCondition(liftoff_cond));
}

void LiftoffAssembler::emit_f32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  cebr(lhs, rhs);
  EMIT_SET_CONDITION(dst, liftoff::ToCondition(liftoff_cond));
}

void LiftoffAssembler::emit_f64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  cdbr(lhs, rhs);
  EMIT_SET_CONDITION(dst, liftoff::ToCondition(liftoff_cond));
}

bool LiftoffAssembler::emit_select(LiftoffRegister dst, Register condition,
                                   LiftoffRegister true_value,
                                   LiftoffRegister false_value,
                                   ValueKind kind) {
  return false;
}

void LiftoffAssembler::emit_smi_check(Register obj, Label* target,
                                      SmiCheckMode mode) {
  bailout(kUnsupportedArchitecture, "emit_smi_check");
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  bailout(kSimd, "Load transform unimplemented");
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc) {
  bailout(kSimd, "loadlane");
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc) {
  bailout(kSimd, "store lane");
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_swizzle");
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f64x2splat");
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_f64x2extractlane");
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_f64x2replacelane");
}

void LiftoffAssembler::emit_f64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f64x2_abs");
}

void LiftoffAssembler::emit_f64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f64x2neg");
}

void LiftoffAssembler::emit_f64x2_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f64x2sqrt");
}

bool LiftoffAssembler::emit_f64x2_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kSimd, "f64x2.ceil");
  return true;
}

bool LiftoffAssembler::emit_f64x2_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "f64x2.floor");
  return true;
}

bool LiftoffAssembler::emit_f64x2_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "f64x2.trunc");
  return true;
}

bool LiftoffAssembler::emit_f64x2_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  bailout(kSimd, "f64x2.nearest_int");
  return true;
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2add");
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2sub");
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2mul");
}

void LiftoffAssembler::emit_f64x2_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2div");
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2min");
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2max");
}

void LiftoffAssembler::emit_f64x2_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "pmin unimplemented");
}

void LiftoffAssembler::emit_f64x2_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "pmax unimplemented");
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "f64x2.convert_low_i32x4_s");
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "f64x2.convert_low_i32x4_u");
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  bailout(kSimd, "f64x2.promote_low_f32x4");
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_splat");
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_f32x4extractlane");
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_f32x4replacelane");
}

void LiftoffAssembler::emit_f32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_abs");
}

void LiftoffAssembler::emit_f32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f32x4neg");
}

void LiftoffAssembler::emit_f32x4_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_f32x4sqrt");
}

bool LiftoffAssembler::emit_f32x4_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kSimd, "f32x4.ceil");
  return true;
}

bool LiftoffAssembler::emit_f32x4_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "f32x4.floor");
  return true;
}

bool LiftoffAssembler::emit_f32x4_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "f32x4.trunc");
  return true;
}

bool LiftoffAssembler::emit_f32x4_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  bailout(kSimd, "f32x4.nearest_int");
  return true;
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4add");
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4sub");
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4mul");
}

void LiftoffAssembler::emit_f32x4_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4div");
}

void LiftoffAssembler::emit_f32x4_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4min");
}

void LiftoffAssembler::emit_f32x4_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4max");
}

void LiftoffAssembler::emit_f32x4_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "pmin unimplemented");
}

void LiftoffAssembler::emit_f32x4_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "pmax unimplemented");
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64x2splat");
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i64x2extractlane");
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i64x2replacelane");
}

void LiftoffAssembler::emit_i64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64x2neg");
}

void LiftoffAssembler::emit_i64x2_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i64x2_alltrue");
}

void LiftoffAssembler::emit_i64x2_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "i64x2_shl");
}

void LiftoffAssembler::emit_i64x2_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "i64x2_shli");
}

void LiftoffAssembler::emit_i64x2_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i64x2_shr_s");
}

void LiftoffAssembler::emit_i64x2_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i64x2_shri_s");
}

void LiftoffAssembler::emit_i64x2_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i64x2_shr_u");
}

void LiftoffAssembler::emit_i64x2_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i64x2_shri_u");
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i64x2add");
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i64x2sub");
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i64x2mul");
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i64x2_extmul_low_i32x4_s unsupported");
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i64x2_extmul_low_i32x4_u unsupported");
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i64x2_extmul_high_i32x4_s unsupported");
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i64x2_bitmask");
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "i64x2_sconvert_i32x4_low");
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "i64x2_sconvert_i32x4_high");
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "i64x2_uconvert_i32x4_low");
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "i64x2_uconvert_i32x4_high");
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i64x2_extmul_high_i32x4_u unsupported");
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_splat");
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i32x4extractlane");
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i32x4replacelane");
}

void LiftoffAssembler::emit_i32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4neg");
}

void LiftoffAssembler::emit_i32x4_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i32x4_alltrue");
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i32x4_bitmask");
}

void LiftoffAssembler::emit_i32x4_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "i32x4_shl");
}

void LiftoffAssembler::emit_i32x4_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "i32x4_shli");
}

void LiftoffAssembler::emit_i32x4_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i32x4_shr_s");
}

void LiftoffAssembler::emit_i32x4_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i32x4_shri_s");
}

void LiftoffAssembler::emit_i32x4_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i32x4_shr_u");
}

void LiftoffAssembler::emit_i32x4_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i32x4_shri_u");
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4add");
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4sub");
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4mul");
}

void LiftoffAssembler::emit_i32x4_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_min_s");
}

void LiftoffAssembler::emit_i32x4_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_min_u");
}

void LiftoffAssembler::emit_i32x4_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_max_s");
}

void LiftoffAssembler::emit_i32x4_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_max_u");
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  bailout(kSimd, "i32x4_dot_i16x8_s");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "i32x4.extadd_pairwise_i16x8_s");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "i32x4.extadd_pairwise_i16x8_u");
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i32x4_extmul_low_i16x8_s unsupported");
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i32x4_extmul_low_i16x8_u unsupported");
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i32x4_extmul_high_i16x8_s unsupported");
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i32x4_extmul_high_i16x8_u unsupported");
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8splat");
}

void LiftoffAssembler::emit_i16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8neg");
}

void LiftoffAssembler::emit_i16x8_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i16x8_alltrue");
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i16x8_bitmask");
}

void LiftoffAssembler::emit_i16x8_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "i16x8_shl");
}

void LiftoffAssembler::emit_i16x8_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "i16x8_shli");
}

void LiftoffAssembler::emit_i16x8_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i16x8_shr_s");
}

void LiftoffAssembler::emit_i16x8_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i16x8_shri_s");
}

void LiftoffAssembler::emit_i16x8_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i16x8_shr_u");
}

void LiftoffAssembler::emit_i16x8_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i16x8_shri_u");
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8add");
}

void LiftoffAssembler::emit_i16x8_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8addsaturate_s");
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8sub");
}

void LiftoffAssembler::emit_i16x8_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8subsaturate_s");
}

void LiftoffAssembler::emit_i16x8_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8subsaturate_u");
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8mul");
}

void LiftoffAssembler::emit_i16x8_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8addsaturate_u");
}

void LiftoffAssembler::emit_i16x8_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_min_s");
}

void LiftoffAssembler::emit_i16x8_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_min_u");
}

void LiftoffAssembler::emit_i16x8_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_max_s");
}

void LiftoffAssembler::emit_i16x8_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_max_u");
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i16x8extractlane_u");
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i16x8replacelane");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "i16x8.extadd_pairwise_i8x16_s");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "i16x8.extadd_pairwise_i8x16_u");
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i16x8extractlane_s");
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i16x8.extmul_low_i8x16_s unsupported");
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  bailout(kSimd, "i16x8.extmul_low_i8x16_u unsupported");
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i16x8.extmul_high_i8x16_s unsupported");
}

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  bailout(kSimd, "i16x8_q15mulr_sat_s");
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  bailout(kSimd, "i16x8_extmul_high_i8x16_u unsupported");
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  bailout(kSimd, "i8x16_shuffle");
}

void LiftoffAssembler::emit_i8x16_popcnt(LiftoffRegister dst,
                                         LiftoffRegister src) {
  bailout(kSimd, "i8x16.popcnt");
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i8x16splat");
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i8x16extractlane_u");
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i8x16extractlane_s");
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kUnsupportedArchitecture, "emit_i8x16replacelane");
}

void LiftoffAssembler::emit_i8x16_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i8x16neg");
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  bailout(kSimd, "v8x16_anytrue");
}

void LiftoffAssembler::emit_i8x16_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i8x16_alltrue");
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "i8x16_bitmask");
}

void LiftoffAssembler::emit_i8x16_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "i8x16_shl");
}

void LiftoffAssembler::emit_i8x16_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "i8x16_shli");
}

void LiftoffAssembler::emit_i8x16_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i8x16_shr_s");
}

void LiftoffAssembler::emit_i8x16_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i8x16_shri_s");
}

void LiftoffAssembler::emit_i8x16_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "i8x16_shr_u");
}

void LiftoffAssembler::emit_i8x16_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "i8x16_shri_u");
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16add");
}

void LiftoffAssembler::emit_i8x16_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16addsaturate_s");
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16sub");
}

void LiftoffAssembler::emit_i8x16_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16subsaturate_s");
}

void LiftoffAssembler::emit_i8x16_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16subsaturate_u");
}

void LiftoffAssembler::emit_i8x16_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16addsaturate_u");
}

void LiftoffAssembler::emit_i8x16_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_min_s");
}

void LiftoffAssembler::emit_i8x16_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_min_u");
}

void LiftoffAssembler::emit_i8x16_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_max_s");
}

void LiftoffAssembler::emit_i8x16_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_max_u");
}

void LiftoffAssembler::emit_i8x16_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_eq");
}

void LiftoffAssembler::emit_i8x16_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_ne");
}

void LiftoffAssembler::emit_i8x16_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16gt_s");
}

void LiftoffAssembler::emit_i8x16_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16gt_u");
}

void LiftoffAssembler::emit_i8x16_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16ge_s");
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16ge_u");
}

void LiftoffAssembler::emit_i16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_eq");
}

void LiftoffAssembler::emit_i16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_ne");
}

void LiftoffAssembler::emit_i16x8_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8gt_s");
}

void LiftoffAssembler::emit_i16x8_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8gt_u");
}

void LiftoffAssembler::emit_i16x8_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8ge_s");
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8ge_u");
}

void LiftoffAssembler::emit_i32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_eq");
}

void LiftoffAssembler::emit_i32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_ne");
}

void LiftoffAssembler::emit_i32x4_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4gt_s");
}

void LiftoffAssembler::emit_i32x4_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4gt_u");
}

void LiftoffAssembler::emit_i32x4_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4ge_s");
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32x4ge_u");
}

void LiftoffAssembler::emit_i64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "i64x2.eq");
}

void LiftoffAssembler::emit_i64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "i64x2_ne");
}

void LiftoffAssembler::emit_i64x2_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "i64x2.gt_s");
}

void LiftoffAssembler::emit_i64x2_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "i64x2.ge_s");
}

void LiftoffAssembler::emit_f32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_eq");
}

void LiftoffAssembler::emit_f32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_ne");
}

void LiftoffAssembler::emit_f32x4_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_lt");
}

void LiftoffAssembler::emit_f32x4_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32x4_le");
}

void LiftoffAssembler::emit_f64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2_eq");
}

void LiftoffAssembler::emit_f64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2_ne");
}

void LiftoffAssembler::emit_f64x2_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2_lt");
}

void LiftoffAssembler::emit_f64x2_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64x2_le");
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  bailout(kUnsupportedArchitecture, "emit_s128_const");
}

void LiftoffAssembler::emit_s128_not(LiftoffRegister dst, LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_s128_not");
}

void LiftoffAssembler::emit_s128_and(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_s128_and");
}

void LiftoffAssembler::emit_s128_or(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_s128_or");
}

void LiftoffAssembler::emit_s128_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_s128_xor");
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  bailout(kUnsupportedArchitecture, "emit_s128select");
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "i32x4_sconvert_f32x4");
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "i32x4_uconvert_f32x4");
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "f32x4_sconvert_i32x4");
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "f32x4_uconvert_i32x4");
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  bailout(kSimd, "f32x4.demote_f64x2_zero");
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_sconvert_i16x8");
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_uconvert_i16x8");
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_sconvert_i32x4");
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_uconvert_i32x4");
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_sconvert_i8x16_low");
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_sconvert_i8x16_high");
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_uconvert_i8x16_low");
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_uconvert_i8x16_high");
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_sconvert_i16x8_low");
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_sconvert_i16x8_high");
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_uconvert_i16x8_low");
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_uconvert_i16x8_high");
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  bailout(kSimd, "i32x4.trunc_sat_f64x2_s_zero");
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  bailout(kSimd, "i32x4.trunc_sat_f64x2_u_zero");
}

void LiftoffAssembler::emit_s128_and_not(LiftoffRegister dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_s128_and_not");
}

void LiftoffAssembler::emit_i8x16_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_rounding_average_u");
}

void LiftoffAssembler::emit_i16x8_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_rounding_average_u");
}

void LiftoffAssembler::emit_i8x16_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i8x16_abs");
}

void LiftoffAssembler::emit_i16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i16x8_abs");
}

void LiftoffAssembler::emit_i32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i32x4_abs");
}

void LiftoffAssembler::emit_i64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "i64x2.abs");
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  LoadU64(limit_address, MemOperand(limit_address));
  CmpU64(sp, limit_address);
  b(le, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, 0, no_reg);
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  // Asserts unreachable within the wasm code.
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  MultiPush(regs.GetGpList());
  MultiPushDoubles(regs.GetFpList());
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  MultiPopDoubles(regs.GetFpList());
  MultiPop(regs.GetGpList());
}

void LiftoffAssembler::RecordSpillsInSafepoint(Safepoint& safepoint,
                                               LiftoffRegList all_spills,
                                               LiftoffRegList ref_spills,
                                               int spill_offset) {
  int spill_space_size = 0;
  while (!all_spills.is_empty()) {
    LiftoffRegister reg = all_spills.GetLastRegSet();
    if (ref_spills.has(reg)) {
      safepoint.DefinePointerSlot(spill_offset);
    }
    all_spills.clear(reg);
    ++spill_offset;
    spill_space_size += kSystemPointerSize;
  }
  // Record the number of additional spill slots.
  RecordOolSpillSpaceSize(spill_space_size);
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  Drop(num_stack_slots);
  Ret();
}

void LiftoffAssembler::CallC(const ValueKindSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueKind out_argument_kind, int stack_bytes,
                             ExternalReference ext_ref) {
  int total_size = RoundUp(stack_bytes, 8);

  int size = total_size;
  constexpr int kStackPageSize = 4 * KB;

  // Reserve space in the stack.
  while (size > kStackPageSize) {
    lay(sp, MemOperand(sp, -kStackPageSize));
    StoreU64(r0, MemOperand(sp));
    size -= kStackPageSize;
  }

  lay(sp, MemOperand(sp, -size));

  int arg_bytes = 0;
  for (ValueKind param_kind : sig->parameters()) {
    switch (param_kind) {
      case kI32:
        StoreU32(args->gp(), MemOperand(sp, arg_bytes));
        break;
      case kI64:
        StoreU64(args->gp(), MemOperand(sp, arg_bytes));
        break;
      case kF32:
        StoreF32(args->fp(), MemOperand(sp, arg_bytes));
        break;
      case kF64:
        StoreF64(args->fp(), MemOperand(sp, arg_bytes));
        break;
      default:
        UNREACHABLE();
    }
    args++;
    arg_bytes += element_size_bytes(param_kind);
  }

  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  mov(r2, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, no_reg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = r2;
    if (kReturnReg != rets->gp()) {
      Move(*rets, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    result_reg++;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    switch (out_argument_kind) {
      case kI32:
        LoadS32(result_reg->gp(), MemOperand(sp));
        break;
      case kI64:
      case kOptRef:
      case kRef:
      case kRtt:
      case kRttWithDepth:
        LoadU64(result_reg->gp(), MemOperand(sp));
        break;
      case kF32:
        LoadF32(result_reg->fp(), MemOperand(sp));
        break;
      case kF64:
        LoadF64(result_reg->fp(), MemOperand(sp));
        break;
      default:
        UNREACHABLE();
    }
  }
  lay(sp, MemOperand(sp, total_size));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  Call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::TailCallNativeWasmCode(Address addr) {
  Jump(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(const ValueKindSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  DCHECK(target != no_reg);
  Call(target);
}

void LiftoffAssembler::TailCallIndirect(Register target) {
  DCHECK(target != no_reg);
  Jump(target);
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  Call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  lay(sp, MemOperand(sp, -size));
  TurboAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  lay(sp, MemOperand(sp, size));
}

void LiftoffStackSlots::Construct(int param_slots) {
  DCHECK_LT(0, slots_.size());
  SortInPushOrder();
  int last_stack_slot = param_slots;
  for (auto& slot : slots_) {
    const int stack_slot = slot.dst_slot_;
    int stack_decrement = (last_stack_slot - stack_slot) * kSystemPointerSize;
    DCHECK_LT(0, stack_decrement);
    last_stack_slot = stack_slot;
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack: {
        switch (src.kind()) {
          case kI32:
          case kRef:
          case kOptRef:
          case kRtt:
          case kRttWithDepth:
          case kI64: {
            asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->LoadU64(scratch, liftoff::GetStackSlot(slot.src_offset_));
            asm_->Push(scratch);
            break;
          }
          case kF32: {
            asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
            asm_->LoadF32(kScratchDoubleReg,
                          liftoff::GetStackSlot(slot.src_offset_));
            asm_->lay(sp, MemOperand(sp, -kSystemPointerSize));
            asm_->StoreF32(kScratchDoubleReg, MemOperand(sp));
            break;
          }
          case kF64: {
            asm_->AllocateStackSpace(stack_decrement - kDoubleSize);
            asm_->LoadF64(kScratchDoubleReg,
                          liftoff::GetStackSlot(slot.src_offset_));
            asm_->push(kScratchDoubleReg);
            break;
          }
          case kS128: {
            asm_->AllocateStackSpace(stack_decrement - kSimd128Size);
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->LoadV128(kScratchDoubleReg,
                           liftoff::GetStackSlot(slot.src_offset_), scratch);
            asm_->lay(sp, MemOperand(sp, -kSimd128Size));
            asm_->StoreV128(kScratchDoubleReg, MemOperand(sp), scratch);
            break;
          }
          default:
            UNREACHABLE();
        }
        break;
      }
      case LiftoffAssembler::VarState::kRegister: {
        int pushed_bytes = SlotSizeInBytes(slot);
        asm_->AllocateStackSpace(stack_decrement - pushed_bytes);
        switch (src.kind()) {
          case kI64:
          case kI32:
          case kRef:
          case kOptRef:
          case kRtt:
          case kRttWithDepth:
            asm_->push(src.reg().gp());
            break;
          case kF32:
            asm_->lay(sp, MemOperand(sp, -kSystemPointerSize));
            asm_->StoreF32(src.reg().fp(), MemOperand(sp));
            break;
          case kF64:
            asm_->push(src.reg().fp());
            break;
          case kS128: {
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->lay(sp, MemOperand(sp, -kSimd128Size));
            asm_->StoreV128(src.reg().fp(), MemOperand(sp), scratch);
            break;
          }
          default:
            UNREACHABLE();
        }
        break;
      }
      case LiftoffAssembler::VarState::kIntConst: {
        asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        DCHECK(src.kind() == kI32 || src.kind() == kI64);
        UseScratchRegisterScope temps(asm_);
        Register scratch = temps.Acquire();

        switch (src.kind()) {
          case kI32:
            asm_->mov(scratch, Operand(src.i32_const()));
            break;
          case kI64:
            asm_->mov(scratch, Operand(int64_t{slot.src_.i32_const()}));
            break;
          default:
            UNREACHABLE();
        }
        asm_->push(scratch);
        break;
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_
