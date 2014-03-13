// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "macro-assembler.h"
#include "a64/utils-a64.h"
#include "cctest.h"
#include "test-utils-a64.h"

using namespace v8::internal;


#define __ masm->


bool Equal32(uint32_t expected, const RegisterDump*, uint32_t result) {
  if (result != expected) {
    printf("Expected 0x%08" PRIx32 "\t Found 0x%08" PRIx32 "\n",
           expected, result);
  }

  return expected == result;
}


bool Equal64(uint64_t expected, const RegisterDump*, uint64_t result) {
  if (result != expected) {
    printf("Expected 0x%016" PRIx64 "\t Found 0x%016" PRIx64 "\n",
           expected, result);
  }

  return expected == result;
}


bool EqualFP32(float expected, const RegisterDump*, float result) {
  if (float_to_rawbits(expected) == float_to_rawbits(result)) {
    return true;
  } else {
    if (std::isnan(expected) || (expected == 0.0)) {
      printf("Expected 0x%08" PRIx32 "\t Found 0x%08" PRIx32 "\n",
             float_to_rawbits(expected), float_to_rawbits(result));
    } else {
      printf("Expected %.9f (0x%08" PRIx32 ")\t "
             "Found %.9f (0x%08" PRIx32 ")\n",
             expected, float_to_rawbits(expected),
             result, float_to_rawbits(result));
    }
    return false;
  }
}


bool EqualFP64(double expected, const RegisterDump*, double result) {
  if (double_to_rawbits(expected) == double_to_rawbits(result)) {
    return true;
  }

  if (std::isnan(expected) || (expected == 0.0)) {
    printf("Expected 0x%016" PRIx64 "\t Found 0x%016" PRIx64 "\n",
           double_to_rawbits(expected), double_to_rawbits(result));
  } else {
    printf("Expected %.17f (0x%016" PRIx64 ")\t "
           "Found %.17f (0x%016" PRIx64 ")\n",
           expected, double_to_rawbits(expected),
           result, double_to_rawbits(result));
  }
  return false;
}


bool Equal32(uint32_t expected, const RegisterDump* core, const Register& reg) {
  ASSERT(reg.Is32Bits());
  // Retrieve the corresponding X register so we can check that the upper part
  // was properly cleared.
  int64_t result_x = core->xreg(reg.code());
  if ((result_x & 0xffffffff00000000L) != 0) {
    printf("Expected 0x%08" PRIx32 "\t Found 0x%016" PRIx64 "\n",
           expected, result_x);
    return false;
  }
  uint32_t result_w = core->wreg(reg.code());
  return Equal32(expected, core, result_w);
}


bool Equal64(uint64_t expected,
             const RegisterDump* core,
             const Register& reg) {
  ASSERT(reg.Is64Bits());
  uint64_t result = core->xreg(reg.code());
  return Equal64(expected, core, result);
}


bool EqualFP32(float expected,
               const RegisterDump* core,
               const FPRegister& fpreg) {
  ASSERT(fpreg.Is32Bits());
  // Retrieve the corresponding D register so we can check that the upper part
  // was properly cleared.
  uint64_t result_64 = core->dreg_bits(fpreg.code());
  if ((result_64 & 0xffffffff00000000L) != 0) {
    printf("Expected 0x%08" PRIx32 " (%f)\t Found 0x%016" PRIx64 "\n",
           float_to_rawbits(expected), expected, result_64);
    return false;
  }

  return EqualFP32(expected, core, core->sreg(fpreg.code()));
}


bool EqualFP64(double expected,
               const RegisterDump* core,
               const FPRegister& fpreg) {
  ASSERT(fpreg.Is64Bits());
  return EqualFP64(expected, core, core->dreg(fpreg.code()));
}


bool Equal64(const Register& reg0,
             const RegisterDump* core,
             const Register& reg1) {
  ASSERT(reg0.Is64Bits() && reg1.Is64Bits());
  int64_t expected = core->xreg(reg0.code());
  int64_t result = core->xreg(reg1.code());
  return Equal64(expected, core, result);
}


static char FlagN(uint32_t flags) {
  return (flags & NFlag) ? 'N' : 'n';
}


static char FlagZ(uint32_t flags) {
  return (flags & ZFlag) ? 'Z' : 'z';
}


static char FlagC(uint32_t flags) {
  return (flags & CFlag) ? 'C' : 'c';
}


static char FlagV(uint32_t flags) {
  return (flags & VFlag) ? 'V' : 'v';
}


bool EqualNzcv(uint32_t expected, uint32_t result) {
  ASSERT((expected & ~NZCVFlag) == 0);
  ASSERT((result & ~NZCVFlag) == 0);
  if (result != expected) {
    printf("Expected: %c%c%c%c\t Found: %c%c%c%c\n",
        FlagN(expected), FlagZ(expected), FlagC(expected), FlagV(expected),
        FlagN(result), FlagZ(result), FlagC(result), FlagV(result));
    return false;
  }

  return true;
}


bool EqualRegisters(const RegisterDump* a, const RegisterDump* b) {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    if (a->xreg(i) != b->xreg(i)) {
      printf("x%d\t Expected 0x%016" PRIx64 "\t Found 0x%016" PRIx64 "\n",
             i, a->xreg(i), b->xreg(i));
      return false;
    }
  }

  for (unsigned i = 0; i < kNumberOfFPRegisters; i++) {
    uint64_t a_bits = a->dreg_bits(i);
    uint64_t b_bits = b->dreg_bits(i);
    if (a_bits != b_bits) {
      printf("d%d\t Expected 0x%016" PRIx64 "\t Found 0x%016" PRIx64 "\n",
             i, a_bits, b_bits);
      return false;
    }
  }

  return true;
}


RegList PopulateRegisterArray(Register* w, Register* x, Register* r,
                              int reg_size, int reg_count, RegList allowed) {
  RegList list = 0;
  int i = 0;
  for (unsigned n = 0; (n < kNumberOfRegisters) && (i < reg_count); n++) {
    if (((1UL << n) & allowed) != 0) {
      // Only assign allowed registers.
      if (r) {
        r[i] = Register::Create(n, reg_size);
      }
      if (x) {
        x[i] = Register::Create(n, kXRegSize);
      }
      if (w) {
        w[i] = Register::Create(n, kWRegSize);
      }
      list |= (1UL << n);
      i++;
    }
  }
  // Check that we got enough registers.
  ASSERT(CountSetBits(list, kNumberOfRegisters) == reg_count);

  return list;
}


RegList PopulateFPRegisterArray(FPRegister* s, FPRegister* d, FPRegister* v,
                                int reg_size, int reg_count, RegList allowed) {
  RegList list = 0;
  int i = 0;
  for (unsigned n = 0; (n < kNumberOfFPRegisters) && (i < reg_count); n++) {
    if (((1UL << n) & allowed) != 0) {
      // Only assigned allowed registers.
      if (v) {
        v[i] = FPRegister::Create(n, reg_size);
      }
      if (d) {
        d[i] = FPRegister::Create(n, kDRegSize);
      }
      if (s) {
        s[i] = FPRegister::Create(n, kSRegSize);
      }
      list |= (1UL << n);
      i++;
    }
  }
  // Check that we got enough registers.
  ASSERT(CountSetBits(list, kNumberOfFPRegisters) == reg_count);

  return list;
}


void Clobber(MacroAssembler* masm, RegList reg_list, uint64_t const value) {
  Register first = NoReg;
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    if (reg_list & (1UL << i)) {
      Register xn = Register::Create(i, kXRegSize);
      // We should never write into csp here.
      ASSERT(!xn.Is(csp));
      if (!xn.IsZero()) {
        if (!first.IsValid()) {
          // This is the first register we've hit, so construct the literal.
          __ Mov(xn, value);
          first = xn;
        } else {
          // We've already loaded the literal, so re-use the value already
          // loaded into the first register we hit.
          __ Mov(xn, first);
        }
      }
    }
  }
}


void ClobberFP(MacroAssembler* masm, RegList reg_list, double const value) {
  FPRegister first = NoFPReg;
  for (unsigned i = 0; i < kNumberOfFPRegisters; i++) {
    if (reg_list & (1UL << i)) {
      FPRegister dn = FPRegister::Create(i, kDRegSize);
      if (!first.IsValid()) {
        // This is the first register we've hit, so construct the literal.
        __ Fmov(dn, value);
        first = dn;
      } else {
        // We've already loaded the literal, so re-use the value already loaded
        // into the first register we hit.
        __ Fmov(dn, first);
      }
    }
  }
}


void Clobber(MacroAssembler* masm, CPURegList reg_list) {
  if (reg_list.type() == CPURegister::kRegister) {
    // This will always clobber X registers.
    Clobber(masm, reg_list.list());
  } else if (reg_list.type() == CPURegister::kFPRegister) {
    // This will always clobber D registers.
    ClobberFP(masm, reg_list.list());
  } else {
    UNREACHABLE();
  }
}


void RegisterDump::Dump(MacroAssembler* masm) {
  ASSERT(__ StackPointer().Is(csp));

  // Ensure that we don't unintentionally clobber any registers.
  Register old_tmp0 = __ Tmp0();
  Register old_tmp1 = __ Tmp1();
  FPRegister old_fptmp0 = __ FPTmp0();
  __ SetScratchRegisters(NoReg, NoReg);
  __ SetFPScratchRegister(NoFPReg);

  // Preserve some temporary registers.
  Register dump_base = x0;
  Register dump = x1;
  Register tmp = x2;
  Register dump_base_w = dump_base.W();
  Register dump_w = dump.W();
  Register tmp_w = tmp.W();

  // Offsets into the dump_ structure.
  const int x_offset = offsetof(dump_t, x_);
  const int w_offset = offsetof(dump_t, w_);
  const int d_offset = offsetof(dump_t, d_);
  const int s_offset = offsetof(dump_t, s_);
  const int sp_offset = offsetof(dump_t, sp_);
  const int wsp_offset = offsetof(dump_t, wsp_);
  const int flags_offset = offsetof(dump_t, flags_);

  __ Push(xzr, dump_base, dump, tmp);

  // Load the address where we will dump the state.
  __ Mov(dump_base, reinterpret_cast<uint64_t>(&dump_));

  // Dump the stack pointer (csp and wcsp).
  // The stack pointer cannot be stored directly; it needs to be moved into
  // another register first. Also, we pushed four X registers, so we need to
  // compensate here.
  __ Add(tmp, csp, 4 * kXRegSizeInBytes);
  __ Str(tmp, MemOperand(dump_base, sp_offset));
  __ Add(tmp_w, wcsp, 4 * kXRegSizeInBytes);
  __ Str(tmp_w, MemOperand(dump_base, wsp_offset));

  // Dump X registers.
  __ Add(dump, dump_base, x_offset);
  for (unsigned i = 0; i < kNumberOfRegisters; i += 2) {
    __ Stp(Register::XRegFromCode(i), Register::XRegFromCode(i + 1),
           MemOperand(dump, i * kXRegSizeInBytes));
  }

  // Dump W registers.
  __ Add(dump, dump_base, w_offset);
  for (unsigned i = 0; i < kNumberOfRegisters; i += 2) {
    __ Stp(Register::WRegFromCode(i), Register::WRegFromCode(i + 1),
           MemOperand(dump, i * kWRegSizeInBytes));
  }

  // Dump D registers.
  __ Add(dump, dump_base, d_offset);
  for (unsigned i = 0; i < kNumberOfFPRegisters; i += 2) {
    __ Stp(FPRegister::DRegFromCode(i), FPRegister::DRegFromCode(i + 1),
           MemOperand(dump, i * kDRegSizeInBytes));
  }

  // Dump S registers.
  __ Add(dump, dump_base, s_offset);
  for (unsigned i = 0; i < kNumberOfFPRegisters; i += 2) {
    __ Stp(FPRegister::SRegFromCode(i), FPRegister::SRegFromCode(i + 1),
           MemOperand(dump, i * kSRegSizeInBytes));
  }

  // Dump the flags.
  __ Mrs(tmp, NZCV);
  __ Str(tmp, MemOperand(dump_base, flags_offset));

  // To dump the values that were in tmp amd dump, we need a new scratch
  // register.  We can use any of the already dumped registers since we can
  // easily restore them.
  Register dump2_base = x10;
  Register dump2 = x11;
  ASSERT(!AreAliased(dump_base, dump, tmp, dump2_base, dump2));

  // Don't lose the dump_ address.
  __ Mov(dump2_base, dump_base);

  __ Pop(tmp, dump, dump_base, xzr);

  __ Add(dump2, dump2_base, w_offset);
  __ Str(dump_base_w, MemOperand(dump2, dump_base.code() * kWRegSizeInBytes));
  __ Str(dump_w, MemOperand(dump2, dump.code() * kWRegSizeInBytes));
  __ Str(tmp_w, MemOperand(dump2, tmp.code() * kWRegSizeInBytes));

  __ Add(dump2, dump2_base, x_offset);
  __ Str(dump_base, MemOperand(dump2, dump_base.code() * kXRegSizeInBytes));
  __ Str(dump, MemOperand(dump2, dump.code() * kXRegSizeInBytes));
  __ Str(tmp, MemOperand(dump2, tmp.code() * kXRegSizeInBytes));

  // Finally, restore dump2_base and dump2.
  __ Ldr(dump2_base, MemOperand(dump2, dump2_base.code() * kXRegSizeInBytes));
  __ Ldr(dump2, MemOperand(dump2, dump2.code() * kXRegSizeInBytes));

  // Restore the MacroAssembler's scratch registers.
  __ SetScratchRegisters(old_tmp0, old_tmp1);
  __ SetFPScratchRegister(old_fptmp0);

  completed_ = true;
}
