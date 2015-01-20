// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/mips/macro-assembler-mips.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


// TODO(plind): Possibly avoid using these lithium names.
#define kScratchReg kLithiumScratchReg
#define kScratchReg2 kLithiumScratchReg2
#define kScratchDoubleReg kLithiumScratchDouble


// TODO(plind): consider renaming these macros.
#define TRACE_MSG(msg)                                                      \
  PrintF("code_gen: \'%s\' in function %s at line %d\n", msg, __FUNCTION__, \
         __LINE__)

#define TRACE_UNIMPL()                                                       \
  PrintF("UNIMPLEMENTED code_generator_mips: %s at line %d\n", __FUNCTION__, \
         __LINE__)


// Adds Mips-specific methods to convert InstructionOperands.
class MipsOperandConverter FINAL : public InstructionOperandConverter {
 public:
  MipsOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  FloatRegister OutputSingleRegister(int index = 0) {
    return ToSingleRegister(instr_->OutputAt(index));
  }

  FloatRegister InputSingleRegister(int index) {
    return ToSingleRegister(instr_->InputAt(index));
  }

  FloatRegister ToSingleRegister(InstructionOperand* op) {
    // Single (Float) and Double register namespace is same on MIPS,
    // both are typedefs of FPURegister.
    return ToDoubleRegister(op);
  }

  Operand InputImmediate(int index) {
    Constant constant = ToConstant(instr_->InputAt(index));
    switch (constant.type()) {
      case Constant::kInt32:
        return Operand(constant.ToInt32());
      case Constant::kInt64:
        return Operand(constant.ToInt64());
      case Constant::kFloat32:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED));
      case Constant::kFloat64:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
      case Constant::kExternalReference:
      case Constant::kHeapObject:
        // TODO(plind): Maybe we should handle ExtRef & HeapObj here?
        //    maybe not done on arm due to const pool ??
        break;
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(titzer): RPO immediates on mips?
        break;
    }
    UNREACHABLE();
    return Operand(zero_reg);
  }

  Operand InputOperand(int index) {
    InstructionOperand* op = instr_->InputAt(index);
    if (op->IsRegister()) {
      return Operand(ToRegister(op));
    }
    return InputImmediate(index);
  }

  MemOperand MemoryOperand(int* first_index) {
    const int index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        // TODO(plind): r6 address mode, to be implemented ...
        UNREACHABLE();
    }
    UNREACHABLE();
    return MemOperand(no_reg);
  }

  MemOperand MemoryOperand(int index = 0) { return MemoryOperand(&index); }

  MemOperand ToMemOperand(InstructionOperand* op) const {
    DCHECK(op != NULL);
    DCHECK(!op->IsRegister());
    DCHECK(!op->IsDoubleRegister());
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    // The linkage computes where all spill slots are located.
    FrameOffset offset = linkage()->GetFrameOffset(op->index(), frame(), 0);
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }
};


static inline bool HasRegisterInput(Instruction* instr, int index) {
  return instr->InputAt(index)->IsRegister();
}


namespace {

class OutOfLineLoadSingle FINAL : public OutOfLineCode {
 public:
  OutOfLineLoadSingle(CodeGenerator* gen, FloatRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() FINAL {
    __ Move(result_, std::numeric_limits<float>::quiet_NaN());
  }

 private:
  FloatRegister const result_;
};


class OutOfLineLoadDouble FINAL : public OutOfLineCode {
 public:
  OutOfLineLoadDouble(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() FINAL {
    __ Move(result_, std::numeric_limits<double>::quiet_NaN());
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadInteger FINAL : public OutOfLineCode {
 public:
  OutOfLineLoadInteger(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() FINAL { __ mov(result_, zero_reg); }

 private:
  Register const result_;
};


class OutOfLineRound : public OutOfLineCode {
 public:
  OutOfLineRound(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() FINAL {
    // Handle rounding to zero case where sign has to be preserved.
    // High bits of double input already in kScratchReg.
    __ dsrl(at, kScratchReg, 31);
    __ dsll(at, at, 31);
    __ mthc1(at, result_);
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineTruncate FINAL : public OutOfLineRound {
 public:
  OutOfLineTruncate(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineRound(gen, result) {}
};


class OutOfLineFloor FINAL : public OutOfLineRound {
 public:
  OutOfLineFloor(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineRound(gen, result) {}
};


class OutOfLineCeil FINAL : public OutOfLineRound {
 public:
  OutOfLineCeil(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineRound(gen, result) {}
};


}  // namespace


#define ASSEMBLE_CHECKED_LOAD_FLOAT(width, asm_instr)                         \
  do {                                                                        \
    auto result = i.Output##width##Register();                                \
    auto ool = new (zone()) OutOfLineLoad##width(this, result);               \
    if (instr->InputAt(0)->IsRegister()) {                                    \
      auto offset = i.InputRegister(0);                                       \
      __ Branch(USE_DELAY_SLOT, ool->entry(), hs, offset, i.InputOperand(1)); \
      __ Daddu(at, i.InputRegister(2), offset);                               \
      __ asm_instr(result, MemOperand(at, 0));                                \
    } else {                                                                  \
      auto offset = i.InputOperand(0).immediate();                            \
      __ Branch(ool->entry(), ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(result, MemOperand(i.InputRegister(2), offset));           \
    }                                                                         \
    __ bind(ool->exit());                                                     \
  } while (0)


#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)                              \
  do {                                                                        \
    auto result = i.OutputRegister();                                         \
    auto ool = new (zone()) OutOfLineLoadInteger(this, result);               \
    if (instr->InputAt(0)->IsRegister()) {                                    \
      auto offset = i.InputRegister(0);                                       \
      __ Branch(USE_DELAY_SLOT, ool->entry(), hs, offset, i.InputOperand(1)); \
      __ Daddu(at, i.InputRegister(2), offset);                               \
      __ asm_instr(result, MemOperand(at, 0));                                \
    } else {                                                                  \
      auto offset = i.InputOperand(0).immediate();                            \
      __ Branch(ool->entry(), ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(result, MemOperand(i.InputRegister(2), offset));           \
    }                                                                         \
    __ bind(ool->exit());                                                     \
  } while (0)


#define ASSEMBLE_CHECKED_STORE_FLOAT(width, asm_instr)                 \
  do {                                                                 \
    Label done;                                                        \
    if (instr->InputAt(0)->IsRegister()) {                             \
      auto offset = i.InputRegister(0);                                \
      auto value = i.Input##width##Register(2);                        \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ Daddu(at, i.InputRegister(3), offset);                        \
      __ asm_instr(value, MemOperand(at, 0));                          \
    } else {                                                           \
      auto offset = i.InputOperand(0).immediate();                     \
      auto value = i.Input##width##Register(2);                        \
      __ Branch(&done, ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(value, MemOperand(i.InputRegister(3), offset));     \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)


#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr)                      \
  do {                                                                 \
    Label done;                                                        \
    if (instr->InputAt(0)->IsRegister()) {                             \
      auto offset = i.InputRegister(0);                                \
      auto value = i.InputRegister(2);                                 \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ Daddu(at, i.InputRegister(3), offset);                        \
      __ asm_instr(value, MemOperand(at, 0));                          \
    } else {                                                           \
      auto offset = i.InputOperand(0).immediate();                     \
      auto value = i.InputRegister(2);                                 \
      __ Branch(&done, ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(value, MemOperand(i.InputRegister(3), offset));     \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)


#define ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(asm_instr, operation)                  \
  do {                                                                         \
    auto ool =                                                                 \
        new (zone()) OutOfLine##operation(this, i.OutputDoubleRegister());     \
    Label done;                                                                \
    __ mfhc1(kScratchReg, i.InputDoubleRegister(0));                           \
    __ Ext(at, kScratchReg, HeapNumber::kExponentShift,                        \
           HeapNumber::kExponentBits);                                         \
    __ Branch(USE_DELAY_SLOT, &done, hs, at,                                   \
              Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits)); \
    __ mov_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));              \
    __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));          \
    __ dmfc1(at, i.OutputDoubleRegister());                                    \
    __ Branch(USE_DELAY_SLOT, ool->entry(), eq, at, Operand(zero_reg));        \
    __ cvt_d_l(i.OutputDoubleRegister(), i.OutputDoubleRegister());            \
    __ bind(ool->exit());                                                      \
    __ bind(&done);                                                            \
  } while (0)


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  MipsOperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();

  switch (ArchOpcodeField::decode(opcode)) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ daddiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
        __ Call(at);
      }
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ ld(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, kWrongFunctionContext, cp, Operand(kScratchReg));
      }

      __ ld(at, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(at);
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchRet:
      AssembleReturn();
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), sp);
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64Add:
      __ Addu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dadd:
      __ Daddu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Sub:
      __ Subu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dsub:
      __ Dsubu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Mul:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64MulHigh:
      __ Mulh(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64MulHighU:
      __ Mulhu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Div:
      __ Div(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DivU:
      __ Divu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Mod:
      __ Mod(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64ModU:
      __ Modu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dmul:
      __ Dmul(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Ddiv:
      __ Ddiv(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DdivU:
      __ Ddivu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dmod:
      __ Dmod(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DmodU:
      __ Dmodu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64And:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Or:
      __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Xor:
      __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Shl:
      if (instr->InputAt(1)->IsRegister()) {
        __ sllv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ sll(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMips64Shr:
      if (instr->InputAt(1)->IsRegister()) {
        __ srlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ srl(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMips64Sar:
      if (instr->InputAt(1)->IsRegister()) {
        __ srav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ sra(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMips64Ext:
      __ Ext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
             i.InputInt8(2));
      break;
    case kMips64Dext:
      __ Dext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      break;
    case kMips64Dshl:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsllv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsll(i.OutputRegister(), i.InputRegister(0), imm);
        } else {
          __ dsll32(i.OutputRegister(), i.InputRegister(0), imm - 32);
        }
      }
      break;
    case kMips64Dshr:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsrlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsrl(i.OutputRegister(), i.InputRegister(0), imm);
        } else {
          __ dsrl32(i.OutputRegister(), i.InputRegister(0), imm - 32);
        }
      }
      break;
    case kMips64Dsar:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsrav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsra(i.OutputRegister(), i.InputRegister(0), imm);
        } else {
          __ dsra32(i.OutputRegister(), i.InputRegister(0), imm - 32);
        }
      }
      break;
    case kMips64Ror:
      __ Ror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dror:
      __ Dror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Tst:
    case kMips64Tst32:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kMips64Cmp:
    case kMips64Cmp32:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kMips64Mov:
      // TODO(plind): Should we combine mov/li like this, or use separate instr?
      //    - Also see x64 ASSEMBLE_BINOP & RegisterOrOperandType
      if (HasRegisterInput(instr, 0)) {
        __ mov(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ li(i.OutputRegister(), i.InputOperand(0));
      }
      break;

    case kMips64CmpD:
      // Psuedo-instruction used for FP cmp/branch. No opcode emitted here.
      break;
    case kMips64AddD:
      // TODO(plind): add special case: combine mult & add.
      __ add_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64SubD:
      __ sub_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64MulD:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ mul_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64DivD:
      __ div_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64ModD: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(masm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputDoubleRegister());
      break;
    }
    case kMips64Float64Floor: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(floor_l_d, Floor);
      break;
    }
    case kMips64Float64Ceil: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(ceil_l_d, Ceil);
      break;
    }
    case kMips64Float64RoundTruncate: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(trunc_l_d, Truncate);
      break;
    }
    case kMips64SqrtD: {
      __ sqrt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMips64CvtSD: {
      __ cvt_s_d(i.OutputSingleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMips64CvtDS: {
      __ cvt_d_s(i.OutputDoubleRegister(), i.InputSingleRegister(0));
      break;
    }
    case kMips64CvtDW: {
      FPURegister scratch = kScratchDoubleReg;
      __ mtc1(i.InputRegister(0), scratch);
      __ cvt_d_w(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMips64CvtDUw: {
      FPURegister scratch = kScratchDoubleReg;
      __ Cvt_d_uw(i.OutputDoubleRegister(), i.InputRegister(0), scratch);
      break;
    }
    case kMips64TruncWD: {
      FPURegister scratch = kScratchDoubleReg;
      // Other arches use round to zero here, so we follow.
      __ trunc_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64TruncUwD: {
      FPURegister scratch = kScratchDoubleReg;
      // TODO(plind): Fix wrong param order of Trunc_uw_d() macro-asm function.
      __ Trunc_uw_d(i.InputDoubleRegister(0), i.OutputRegister(), scratch);
      break;
    }
    // ... more basic instructions ...

    case kMips64Lbu:
      __ lbu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lb:
      __ lb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sb:
      __ sb(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lhu:
      __ lhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lh:
      __ lh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sh:
      __ sh(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lw:
      __ lw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ld:
      __ ld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sw:
      __ sw(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Sd:
      __ sd(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lwc1: {
      __ lwc1(i.OutputSingleRegister(), i.MemoryOperand());
      break;
    }
    case kMips64Swc1: {
      int index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      __ swc1(i.InputSingleRegister(index), operand);
      break;
    }
    case kMips64Ldc1:
      __ ldc1(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kMips64Sdc1:
      __ sdc1(i.InputDoubleRegister(2), i.MemoryOperand());
      break;
    case kMips64Push:
      __ Push(i.InputRegister(0));
      break;
    case kMips64StackClaim: {
      int words = MiscField::decode(instr->opcode());
      __ Dsubu(sp, sp, Operand(words << kPointerSizeLog2));
      break;
    }
    case kMips64StoreToStackSlot: {
      int slot = MiscField::decode(instr->opcode());
      __ sd(i.InputRegister(0), MemOperand(sp, slot << kPointerSizeLog2));
      break;
    }
    case kMips64StoreWriteBarrier: {
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      __ daddu(index, object, index);
      __ sd(value, MemOperand(index));
      SaveFPRegsMode mode =
          frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
      RAStatus ra_status = kRAHasNotBeenSaved;
      __ RecordWrite(object, index, value, ra_status, mode);
      break;
    }
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lb);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lbu);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lh);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lhu);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lw);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Single, lwc1);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Double, ldc1);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(sb);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(sh);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(sw);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(Single, swc1);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(Double, sdc1);
      break;
  }
}


#define UNSUPPORTED_COND(opcode, condition)                                  \
  OFStream out(stdout);                                                      \
  out << "Unsupported " << #opcode << " condition: \"" << condition << "\""; \
  UNIMPLEMENTED();

// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  MipsOperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  Condition cc = kNoCondition;

  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips psuedo-instructions, which are handled here by branch
  // instructions that do the actual comparison. Essential that the input
  // registers to compare psuedo-op are not modified before this branch op, as
  // they are tested here.
  // TODO(plind): Add CHECK() to ensure that test/cmp and this branch were
  //    not separated by other instructions.

  if (instr->arch_opcode() == kMips64Tst) {
    switch (branch->condition) {
      case kNotEqual:
        cc = ne;
        break;
      case kEqual:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64Tst, branch->condition);
        break;
    }
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Branch(tlabel, cc, at, Operand(zero_reg));
  } else if (instr->arch_opcode() == kMips64Tst32) {
    switch (branch->condition) {
      case kNotEqual:
        cc = ne;
        break;
      case kEqual:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64Tst32, branch->condition);
        break;
    }
    // Zero-extend registers on MIPS64 only 64-bit operand
    // branch and compare op. is available.
    // This is a disadvantage to perform 32-bit operation on MIPS64.
    // Try to force globally in front-end Word64 representation to be preferred
    // for MIPS64 even for Word32.
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Dext(at, at, 0, 32);
    __ Branch(tlabel, cc, at, Operand(zero_reg));
  } else if (instr->arch_opcode() == kMips64Dadd ||
             instr->arch_opcode() == kMips64Dsub) {
    switch (branch->condition) {
      case kOverflow:
        cc = ne;
        break;
      case kNotOverflow:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64Dadd, branch->condition);
        break;
    }

    __ dsra32(kScratchReg, i.OutputRegister(), 0);
    __ sra(at, i.OutputRegister(), 31);
    __ Branch(tlabel, cc, at, Operand(kScratchReg));
  } else if (instr->arch_opcode() == kMips64Cmp) {
    switch (branch->condition) {
      case kEqual:
        cc = eq;
        break;
      case kNotEqual:
        cc = ne;
        break;
      case kSignedLessThan:
        cc = lt;
        break;
      case kSignedGreaterThanOrEqual:
        cc = ge;
        break;
      case kSignedLessThanOrEqual:
        cc = le;
        break;
      case kSignedGreaterThan:
        cc = gt;
        break;
      case kUnsignedLessThan:
        cc = lo;
        break;
      case kUnsignedGreaterThanOrEqual:
        cc = hs;
        break;
      case kUnsignedLessThanOrEqual:
        cc = ls;
        break;
      case kUnsignedGreaterThan:
        cc = hi;
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp, branch->condition);
        break;
    }
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));

    if (!branch->fallthru) __ Branch(flabel);  // no fallthru to flabel.

  } else if (instr->arch_opcode() == kMips64Cmp32) {
    switch (branch->condition) {
      case kEqual:
        cc = eq;
        break;
      case kNotEqual:
        cc = ne;
        break;
      case kSignedLessThan:
        cc = lt;
        break;
      case kSignedGreaterThanOrEqual:
        cc = ge;
        break;
      case kSignedLessThanOrEqual:
        cc = le;
        break;
      case kSignedGreaterThan:
        cc = gt;
        break;
      case kUnsignedLessThan:
        cc = lo;
        break;
      case kUnsignedGreaterThanOrEqual:
        cc = hs;
        break;
      case kUnsignedLessThanOrEqual:
        cc = ls;
        break;
      case kUnsignedGreaterThan:
        cc = hi;
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp32, branch->condition);
        break;
    }

    switch (branch->condition) {
      case kEqual:
      case kNotEqual:
      case kSignedLessThan:
      case kSignedGreaterThanOrEqual:
      case kSignedLessThanOrEqual:
      case kSignedGreaterThan:
        // Sign-extend registers on MIPS64 only 64-bit operand
        // branch and compare op. is available.
        __ sll(i.InputRegister(0), i.InputRegister(0), 0);
        if (instr->InputAt(1)->IsRegister()) {
          __ sll(i.InputRegister(1), i.InputRegister(1), 0);
        }
        break;
      case kUnsignedLessThan:
      case kUnsignedGreaterThanOrEqual:
      case kUnsignedLessThanOrEqual:
      case kUnsignedGreaterThan:
        // Zero-extend registers on MIPS64 only 64-bit operand
        // branch and compare op. is available.
        __ Dext(i.InputRegister(0), i.InputRegister(0), 0, 32);
        if (instr->InputAt(1)->IsRegister()) {
          __ Dext(i.InputRegister(1), i.InputRegister(1), 0, 32);
        }
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp, branch->condition);
        break;
    }
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));

    if (!branch->fallthru) __ Branch(flabel);  // no fallthru to flabel.
  } else if (instr->arch_opcode() == kMips64CmpD) {
    // TODO(dusmil) optimize unordered checks to use less instructions
    // even if we have to unfold BranchF macro.
    Label* nan = flabel;
    switch (branch->condition) {
      case kUnorderedEqual:
        cc = eq;
        break;
      case kUnorderedNotEqual:
        cc = ne;
        nan = tlabel;
        break;
      case kUnorderedLessThan:
        cc = lt;
        break;
      case kUnorderedGreaterThanOrEqual:
        cc = ge;
        nan = tlabel;
        break;
      case kUnorderedLessThanOrEqual:
        cc = le;
        break;
      case kUnorderedGreaterThan:
        cc = gt;
        nan = tlabel;
        break;
      default:
        UNSUPPORTED_COND(kMips64CmpD, branch->condition);
        break;
    }
    __ BranchF(tlabel, nan, cc, i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));

    if (!branch->fallthru) __ Branch(flabel);  // no fallthru to flabel.
  } else {
    PrintF("AssembleArchBranch Unimplemented arch_opcode: %d\n",
           instr->arch_opcode());
    UNIMPLEMENTED();
  }
}


void CodeGenerator::AssembleArchJump(BasicBlock::RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ Branch(GetLabel(target));
}


// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  MipsOperandConverter i(this, instr);
  Label done;

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label false_value;
  DCHECK_NE(0, instr->OutputCount());
  Register result = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = kNoCondition;

  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips pseudo-instructions, which are checked and handled here.

  // For materializations, we use delay slot to set the result true, and
  // in the false case, where we fall through the branch, we reset the result
  // false.

  if (instr->arch_opcode() == kMips64Tst) {
    switch (condition) {
      case kNotEqual:
        cc = ne;
        break;
      case kEqual:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64Tst, condition);
        break;
    }
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Branch(USE_DELAY_SLOT, &done, cc, at, Operand(zero_reg));
    __ li(result, Operand(1));  // In delay slot.
  } else if (instr->arch_opcode() == kMips64Tst32) {
    switch (condition) {
      case kNotEqual:
        cc = ne;
        break;
      case kEqual:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64Tst, condition);
        break;
    }
    // Zero-extend register on MIPS64 only 64-bit operand
    // branch and compare op. is available.
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Dext(at, at, 0, 32);
    __ Branch(USE_DELAY_SLOT, &done, cc, at, Operand(zero_reg));
    __ li(result, Operand(1));  // In delay slot.
  } else if (instr->arch_opcode() == kMips64Dadd ||
             instr->arch_opcode() == kMips64Dsub) {
    switch (condition) {
      case kOverflow:
        cc = ne;
        break;
      case kNotOverflow:
        cc = eq;
        break;
      default:
        UNSUPPORTED_COND(kMips64DAdd, condition);
        break;
    }
    __ dsra32(kScratchReg, i.OutputRegister(), 0);
    __ sra(at, i.OutputRegister(), 31);
    __ Branch(USE_DELAY_SLOT, &done, cc, at, Operand(kScratchReg));
    __ li(result, Operand(1));  // In delay slot.
  } else if (instr->arch_opcode() == kMips64Cmp) {
    Register left = i.InputRegister(0);
    Operand right = i.InputOperand(1);
    switch (condition) {
      case kEqual:
        cc = eq;
        break;
      case kNotEqual:
        cc = ne;
        break;
      case kSignedLessThan:
        cc = lt;
        break;
      case kSignedGreaterThanOrEqual:
        cc = ge;
        break;
      case kSignedLessThanOrEqual:
        cc = le;
        break;
      case kSignedGreaterThan:
        cc = gt;
        break;
      case kUnsignedLessThan:
        cc = lo;
        break;
      case kUnsignedGreaterThanOrEqual:
        cc = hs;
        break;
      case kUnsignedLessThanOrEqual:
        cc = ls;
        break;
      case kUnsignedGreaterThan:
        cc = hi;
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp, condition);
        break;
    }
    __ Branch(USE_DELAY_SLOT, &done, cc, left, right);
    __ li(result, Operand(1));  // In delay slot.
  } else if (instr->arch_opcode() == kMips64Cmp32) {
    Register left = i.InputRegister(0);
    Operand right = i.InputOperand(1);
    switch (condition) {
      case kEqual:
        cc = eq;
        break;
      case kNotEqual:
        cc = ne;
        break;
      case kSignedLessThan:
        cc = lt;
        break;
      case kSignedGreaterThanOrEqual:
        cc = ge;
        break;
      case kSignedLessThanOrEqual:
        cc = le;
        break;
      case kSignedGreaterThan:
        cc = gt;
        break;
      case kUnsignedLessThan:
        cc = lo;
        break;
      case kUnsignedGreaterThanOrEqual:
        cc = hs;
        break;
      case kUnsignedLessThanOrEqual:
        cc = ls;
        break;
      case kUnsignedGreaterThan:
        cc = hi;
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp, condition);
        break;
    }

    switch (condition) {
      case kEqual:
      case kNotEqual:
      case kSignedLessThan:
      case kSignedGreaterThanOrEqual:
      case kSignedLessThanOrEqual:
      case kSignedGreaterThan:
        // Sign-extend registers on MIPS64 only 64-bit operand
        // branch and compare op. is available.
        __ sll(left, left, 0);
        if (instr->InputAt(1)->IsRegister()) {
          __ sll(i.InputRegister(1), i.InputRegister(1), 0);
        }
        break;
      case kUnsignedLessThan:
      case kUnsignedGreaterThanOrEqual:
      case kUnsignedLessThanOrEqual:
      case kUnsignedGreaterThan:
        // Zero-extend registers on MIPS64 only 64-bit operand
        // branch and compare op. is available.
        __ Dext(left, left, 0, 32);
        if (instr->InputAt(1)->IsRegister()) {
          __ Dext(i.InputRegister(1), i.InputRegister(1), 0, 32);
        }
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp32, condition);
        break;
    }
    __ Branch(USE_DELAY_SLOT, &done, cc, left, right);
    __ li(result, Operand(1));  // In delay slot.
  } else if (instr->arch_opcode() == kMips64CmpD) {
    FPURegister left = i.InputDoubleRegister(0);
    FPURegister right = i.InputDoubleRegister(1);
    // TODO(plind): Provide NaN-testing macro-asm function without need for
    // BranchF.
    FPURegister dummy1 = f0;
    FPURegister dummy2 = f2;
    switch (condition) {
      case kUnorderedEqual:
        // TODO(plind):  improve the NaN testing throughout this function.
        __ BranchF(NULL, &false_value, kNoCondition, dummy1, dummy2);
        cc = eq;
        break;
      case kUnorderedNotEqual:
        __ BranchF(USE_DELAY_SLOT, NULL, &done, kNoCondition, dummy1, dummy2);
        __ li(result, Operand(1));  // In delay slot - returns 1 on NaN.
        cc = ne;
        break;
      case kUnorderedLessThan:
        __ BranchF(NULL, &false_value, kNoCondition, dummy1, dummy2);
        cc = lt;
        break;
      case kUnorderedGreaterThanOrEqual:
        __ BranchF(USE_DELAY_SLOT, NULL, &done, kNoCondition, dummy1, dummy2);
        __ li(result, Operand(1));  // In delay slot - returns 1 on NaN.
        cc = ge;
        break;
      case kUnorderedLessThanOrEqual:
        __ BranchF(NULL, &false_value, kNoCondition, dummy1, dummy2);
        cc = le;
        break;
      case kUnorderedGreaterThan:
        __ BranchF(USE_DELAY_SLOT, NULL, &done, kNoCondition, dummy1, dummy2);
        __ li(result, Operand(1));  // In delay slot - returns 1 on NaN.
        cc = gt;
        break;
      default:
        UNSUPPORTED_COND(kMips64Cmp, condition);
        break;
    }
    __ BranchF(USE_DELAY_SLOT, &done, NULL, cc, left, right);
    __ li(result, Operand(1));  // In delay slot - branch taken returns 1.
                                // Fall-thru (branch not taken) returns 0.

  } else {
    PrintF("AssembleArchBranch Unimplemented arch_opcode is : %d\n",
           instr->arch_opcode());
    TRACE_UNIMPL();
    UNIMPLEMENTED();
  }
  // Fallthru case is the false materialization.
  __ bind(&false_value);
  __ li(result, Operand(static_cast<int64_t>(0)));
  __ bind(&done);
}


void CodeGenerator::AssembleDeoptimizerCall(int deoptimization_id) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, Deoptimizer::LAZY);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    __ Push(ra, fp);
    __ mov(fp, sp);
    const RegList saves = descriptor->CalleeSavedRegisters();
    if (saves != 0) {  // Save callee-saved registers.
      // TODO(plind): make callee save size const, possibly DCHECK it.
      int register_save_area_size = 0;
      for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
        if (!((1 << i) & saves)) continue;
        register_save_area_size += kPointerSize;
      }
      frame()->SetRegisterSaveAreaSize(register_save_area_size);
      __ MultiPush(saves);
    }
  } else if (descriptor->IsJSFunctionCall()) {
    CompilationInfo* info = this->info();
    __ Prologue(info->IsCodePreAgingActive());
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);

    // Sloppy mode functions and builtins need to replace the receiver with the
    // global proxy when called as functions (without an explicit receiver
    // object).
    // TODO(mstarzinger/verwaest): Should this be moved back into the CallIC?
    if (info->strict_mode() == SLOPPY && !info->is_native()) {
      Label ok;
      // +2 for return address and saved frame pointer.
      int receiver_slot = info->scope()->num_parameters() + 2;
      __ ld(a2, MemOperand(fp, receiver_slot * kPointerSize));
      __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
      __ Branch(&ok, ne, a2, Operand(at));

      __ ld(a2, GlobalObjectOperand());
      __ ld(a2, FieldMemOperand(a2, GlobalObject::kGlobalProxyOffset));
      __ sd(a2, MemOperand(fp, receiver_slot * kPointerSize));
      __ bind(&ok);
    }
  } else {
    __ StubPrologue();
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  }
  int stack_slots = frame()->GetSpillSlotCount();
  if (stack_slots > 0) {
    __ Dsubu(sp, sp, Operand(stack_slots * kPointerSize));
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    if (frame()->GetRegisterSaveAreaSize() > 0) {
      // Remove this frame's spill slots first.
      int stack_slots = frame()->GetSpillSlotCount();
      if (stack_slots > 0) {
        __ Daddu(sp, sp, Operand(stack_slots * kPointerSize));
      }
      // Restore registers.
      const RegList saves = descriptor->CalleeSavedRegisters();
      if (saves != 0) {
        __ MultiPop(saves);
      }
    }
    __ mov(sp, fp);
    __ Pop(ra, fp);
    __ Ret();
  } else {
    __ mov(sp, fp);
    __ Pop(ra, fp);
    int pop_count = descriptor->IsJSFunctionCall()
                        ? static_cast<int>(descriptor->JSParameterCount())
                        : 0;
    __ DropAndRet(pop_count);
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  MipsOperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ mov(g.ToRegister(destination), src);
    } else {
      __ sd(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ ld(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ ld(temp, src);
      __ sd(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
          __ li(dst, Operand(src.ToInt32()));
          break;
        case Constant::kFloat32:
          __ li(dst, isolate()->factory()->NewNumber(src.ToFloat32(), TENURED));
          break;
        case Constant::kInt64:
          __ li(dst, Operand(src.ToInt64()));
          break;
        case Constant::kFloat64:
          __ li(dst, isolate()->factory()->NewNumber(src.ToFloat64(), TENURED));
          break;
        case Constant::kExternalReference:
          __ li(dst, Operand(src.ToExternalReference()));
          break;
        case Constant::kHeapObject:
          __ li(dst, src.ToHeapObject());
          break;
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(titzer): loading RPO numbers on mips64.
          break;
      }
      if (destination->IsStackSlot()) __ sd(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsDoubleStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        __ li(at, Operand(bit_cast<int32_t>(src.ToFloat32())));
        __ sw(at, dst);
      } else {
        FloatRegister dst = g.ToSingleRegister(destination);
        __ Move(dst, src.ToFloat32());
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      DoubleRegister dst = destination->IsDoubleRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      __ Move(dst, src.ToFloat64());
      if (destination->IsDoubleStackSlot()) {
        __ sdc1(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsDoubleRegister()) {
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ sdc1(src, g.ToMemOperand(destination));
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ ldc1(g.ToDoubleRegister(destination), src);
    } else {
      FPURegister temp = kScratchDoubleReg;
      __ ldc1(temp, src);
      __ sdc1(temp, g.ToMemOperand(destination));
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  MipsOperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    // Register-register.
    Register temp = kScratchReg;
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ mov(temp, src);
      __ ld(src, dst);
      __ sd(temp, dst);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    Register temp_1 = kScratchReg2;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ ld(temp_0, src);
    __ ld(temp_1, dst);
    __ sd(temp_0, dst);
    __ sd(temp_1, src);
  } else if (source->IsDoubleRegister()) {
    FPURegister temp = kScratchDoubleReg;
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ Move(temp, src);
      __ ldc1(src, dst);
      __ sdc1(temp, dst);
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleStackSlot());
    Register temp_0 = kScratchReg;
    FPURegister temp_1 = kScratchDoubleReg;
    MemOperand src0 = g.ToMemOperand(source);
    MemOperand src1(src0.rm(), src0.offset() + kPointerSize);
    MemOperand dst0 = g.ToMemOperand(destination);
    MemOperand dst1(dst0.rm(), dst0.offset() + kPointerSize);
    __ ldc1(temp_1, dst0);  // Save destination in temp_1.
    __ lw(temp_0, src0);    // Then use temp_0 to copy source to destination.
    __ sw(temp_0, dst0);
    __ lw(temp_0, src1);
    __ sw(temp_0, dst1);
    __ sdc1(temp_1, src0);
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AddNopForSmiCodeInlining() {
  // Unused on 32-bit ARM. Still exists on 64-bit arm.
  // TODO(plind): Unclear when this is called now. Understand, fix if needed.
  __ nop();  // Maybe PROPERTY_ACCESS_INLINED?
}


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  int space_needed = Deoptimizer::patch_size();
  if (!info()->IsStub()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      // Block tramoline pool emission for duration of padding.
      v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
          masm());
      int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      DCHECK_EQ(0, padding_size % v8::internal::Assembler::kInstrSize);
      while (padding_size > 0) {
        __ nop();
        padding_size -= v8::internal::Assembler::kInstrSize;
      }
    }
  }
  MarkLazyDeoptSite();
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
