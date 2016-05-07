// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler.h"
#include "src/macro-assembler.h"

#include "src/wasm/wasm-module.h"

#include "src/compiler/linkage.h"

#include "src/zone.h"

namespace v8 {
namespace internal {
// TODO(titzer): this should not be in the WASM namespace.
namespace wasm {

using compiler::LocationSignature;
using compiler::CallDescriptor;
using compiler::LinkageLocation;

namespace {
MachineType MachineTypeFor(LocalType type) {
  switch (type) {
    case kAstI32:
      return MachineType::Int32();
    case kAstI64:
      return MachineType::Int64();
    case kAstF64:
      return MachineType::Float64();
    case kAstF32:
      return MachineType::Float32();
    default:
      UNREACHABLE();
      return MachineType::AnyTagged();
  }
}


// Platform-specific configuration for C calling convention.
LinkageLocation regloc(Register reg) {
  return LinkageLocation::ForRegister(reg.code());
}


LinkageLocation regloc(DoubleRegister reg) {
  return LinkageLocation::ForRegister(reg.code());
}


LinkageLocation stackloc(int i) {
  return LinkageLocation::ForCallerFrameSlot(i);
}


#if V8_TARGET_ARCH_IA32
// ===========================================================================
// == ia32 ===================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS eax, edx, ecx, ebx, esi
#define GP_RETURN_REGISTERS eax, edx
#define FP_PARAM_REGISTERS xmm1, xmm2, xmm3, xmm4, xmm5, xmm6
#define FP_RETURN_REGISTERS xmm1, xmm2

#elif V8_TARGET_ARCH_X64
// ===========================================================================
// == x64 ====================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS rax, rdx, rcx, rbx, rsi, rdi
#define GP_RETURN_REGISTERS rax, rdx
#define FP_PARAM_REGISTERS xmm1, xmm2, xmm3, xmm4, xmm5, xmm6
#define FP_RETURN_REGISTERS xmm1, xmm2

#elif V8_TARGET_ARCH_X87
// ===========================================================================
// == x87 ====================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS eax, edx, ecx, ebx, esi
#define GP_RETURN_REGISTERS eax, edx
#define FP_RETURN_REGISTERS stX_0

#elif V8_TARGET_ARCH_ARM
// ===========================================================================
// == arm ====================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS r0, r1, r2, r3
#define GP_RETURN_REGISTERS r0, r1
#define FP_PARAM_REGISTERS d0, d1, d2, d3, d4, d5, d6, d7
#define FP_RETURN_REGISTERS d0, d1

#elif V8_TARGET_ARCH_ARM64
// ===========================================================================
// == arm64 ====================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS x0, x1, x2, x3, x4, x5, x6, x7
#define GP_RETURN_REGISTERS x0, x1
#define FP_PARAM_REGISTERS d0, d1, d2, d3, d4, d5, d6, d7
#define FP_RETURN_REGISTERS d0, d1

#elif V8_TARGET_ARCH_MIPS
// ===========================================================================
// == mips ===================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS a0, a1, a2, a3
#define GP_RETURN_REGISTERS v0, v1
#define FP_PARAM_REGISTERS f2, f4, f6, f8, f10, f12, f14
#define FP_RETURN_REGISTERS f2, f4

#elif V8_TARGET_ARCH_MIPS64
// ===========================================================================
// == mips64 =================================================================
// ===========================================================================
#define GP_PARAM_REGISTERS a0, a1, a2, a3, a4, a5, a6, a7
#define GP_RETURN_REGISTERS v0, v1
#define FP_PARAM_REGISTERS f2, f4, f6, f8, f10, f12, f14
#define FP_RETURN_REGISTERS f2, f4

#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
// ===========================================================================
// == ppc & ppc64 ============================================================
// ===========================================================================
#define GP_PARAM_REGISTERS r3, r4, r5, r6, r7, r8, r9, r10
#define GP_RETURN_REGISTERS r3, r4
#define FP_PARAM_REGISTERS d1, d2, d3, d4, d5, d6, d7, d8
#define FP_RETURN_REGISTERS d1, d2

#else
// ===========================================================================
// == unknown ================================================================
// ===========================================================================
// Don't define anything. We'll just always use the stack.
#endif


// Helper for allocating either an GP or FP reg, or the next stack slot.
struct Allocator {
  Allocator(const Register* gp, int gpc, const DoubleRegister* fp, int fpc)
      : gp_count(gpc),
        gp_offset(0),
        gp_regs(gp),
        fp_count(fpc),
        fp_offset(0),
        fp_regs(fp),
        stack_offset(0) {}

  int gp_count;
  int gp_offset;
  const Register* gp_regs;

  int fp_count;
  int fp_offset;
  const DoubleRegister* fp_regs;

  int stack_offset;

  LinkageLocation Next(LocalType type) {
    if (IsFloatingPoint(type)) {
      // Allocate a floating point register/stack location.
      if (fp_offset < fp_count) {
        return regloc(fp_regs[fp_offset++]);
      } else {
        int offset = -1 - stack_offset;
        stack_offset += Words(type);
        return stackloc(offset);
      }
    } else {
      // Allocate a general purpose register/stack location.
      if (gp_offset < gp_count) {
        return regloc(gp_regs[gp_offset++]);
      } else {
        int offset = -1 - stack_offset;
        stack_offset += Words(type);
        return stackloc(offset);
      }
    }
  }
  bool IsFloatingPoint(LocalType type) {
    return type == kAstF32 || type == kAstF64;
  }
  int Words(LocalType type) {
    // The code generation for pushing parameters on the stack does not
    // distinguish between float32 and float64. Therefore also float32 needs
    // two words.
    if (kPointerSize < 8 &&
        (type == kAstI64 || type == kAstF64 || type == kAstF32)) {
      return 2;
    }
    return 1;
  }
};
}  // namespace

static Allocator GetReturnRegisters() {
#ifdef GP_RETURN_REGISTERS
  static const Register kGPReturnRegisters[] = {GP_RETURN_REGISTERS};
  static const int kGPReturnRegistersCount =
      static_cast<int>(arraysize(kGPReturnRegisters));
#else
  static const Register* kGPReturnRegisters = nullptr;
  static const int kGPReturnRegistersCount = 0;
#endif

#ifdef FP_RETURN_REGISTERS
  static const DoubleRegister kFPReturnRegisters[] = {FP_RETURN_REGISTERS};
  static const int kFPReturnRegistersCount =
      static_cast<int>(arraysize(kFPReturnRegisters));
#else
  static const DoubleRegister* kFPReturnRegisters = nullptr;
  static const int kFPReturnRegistersCount = 0;
#endif

  Allocator rets(kGPReturnRegisters, kGPReturnRegistersCount,
                 kFPReturnRegisters, kFPReturnRegistersCount);

  return rets;
}

static Allocator GetParameterRegisters() {
#ifdef GP_PARAM_REGISTERS
  static const Register kGPParamRegisters[] = {GP_PARAM_REGISTERS};
  static const int kGPParamRegistersCount =
      static_cast<int>(arraysize(kGPParamRegisters));
#else
  static const Register* kGPParamRegisters = nullptr;
  static const int kGPParamRegistersCount = 0;
#endif

#ifdef FP_PARAM_REGISTERS
  static const DoubleRegister kFPParamRegisters[] = {FP_PARAM_REGISTERS};
  static const int kFPParamRegistersCount =
      static_cast<int>(arraysize(kFPParamRegisters));
#else
  static const DoubleRegister* kFPParamRegisters = nullptr;
  static const int kFPParamRegistersCount = 0;
#endif

  Allocator params(kGPParamRegisters, kGPParamRegistersCount, kFPParamRegisters,
                   kFPParamRegistersCount);

  return params;
}

// General code uses the above configuration data.
CallDescriptor* ModuleEnv::GetWasmCallDescriptor(Zone* zone,
                                                 FunctionSig* fsig) {
  MachineSignature::Builder msig(zone, fsig->return_count(),
                                 fsig->parameter_count());
  LocationSignature::Builder locations(zone, fsig->return_count(),
                                       fsig->parameter_count());

  Allocator rets = GetReturnRegisters();

  // Add return location(s).
  const int return_count = static_cast<int>(locations.return_count_);
  for (int i = 0; i < return_count; i++) {
    LocalType ret = fsig->GetReturn(i);
    msig.AddReturn(MachineTypeFor(ret));
    locations.AddReturn(rets.Next(ret));
  }

  Allocator params = GetParameterRegisters();

  // Add register and/or stack parameter(s).
  const int parameter_count = static_cast<int>(fsig->parameter_count());
  for (int i = 0; i < parameter_count; i++) {
    LocalType param = fsig->GetParam(i);
    msig.AddParam(MachineTypeFor(param));
    locations.AddParam(params.Next(param));
  }

  const RegList kCalleeSaveRegisters = 0;
  const RegList kCalleeSaveFPRegisters = 0;

  // The target for WASM calls is always a code object.
  MachineType target_type = MachineType::AnyTagged();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();

  return new (zone) CallDescriptor(       // --
      CallDescriptor::kCallCodeObject,    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
      msig.Build(),                       // machine_sig
      locations.Build(),                  // location_sig
      params.stack_offset,                // stack_parameter_count
      compiler::Operator::kNoProperties,  // properties
      kCalleeSaveRegisters,               // callee-saved registers
      kCalleeSaveFPRegisters,             // callee-saved fp regs
      CallDescriptor::kUseNativeStack,    // flags
      "wasm-call");
}

CallDescriptor* ModuleEnv::GetI32WasmCallDescriptor(
    Zone* zone, CallDescriptor* descriptor) {
  const MachineSignature* signature = descriptor->GetMachineSignature();
  size_t parameter_count = signature->parameter_count();
  size_t return_count = signature->return_count();
  for (size_t i = 0; i < signature->parameter_count(); i++) {
    if (signature->GetParam(i) == MachineType::Int64()) {
      // For each int64 input we get two int32 inputs.
      parameter_count++;
    }
  }
  for (size_t i = 0; i < signature->return_count(); i++) {
    if (signature->GetReturn(i) == MachineType::Int64()) {
      // For each int64 return we get two int32 returns.
      return_count++;
    }
  }
  if (parameter_count == signature->parameter_count() &&
      return_count == signature->return_count()) {
    // If there is no int64 parameter or return value, we can just return the
    // original descriptor.
    return descriptor;
  }

  MachineSignature::Builder msig(zone, return_count, parameter_count);
  LocationSignature::Builder locations(zone, return_count, parameter_count);

  Allocator rets = GetReturnRegisters();

  for (size_t i = 0; i < signature->return_count(); i++) {
    if (signature->GetReturn(i) == MachineType::Int64()) {
      // For each int64 return we get two int32 returns.
      msig.AddReturn(MachineType::Int32());
      msig.AddReturn(MachineType::Int32());
      locations.AddReturn(rets.Next(MachineRepresentation::kWord32));
      locations.AddReturn(rets.Next(MachineRepresentation::kWord32));
    } else {
      msig.AddReturn(signature->GetReturn(i));
      locations.AddReturn(rets.Next(signature->GetReturn(i).representation()));
    }
  }

  Allocator params = GetParameterRegisters();

  for (size_t i = 0; i < signature->parameter_count(); i++) {
    if (signature->GetParam(i) == MachineType::Int64()) {
      // For each int64 input we get two int32 inputs.
      msig.AddParam(MachineType::Int32());
      msig.AddParam(MachineType::Int32());
      locations.AddParam(params.Next(MachineRepresentation::kWord32));
      locations.AddParam(params.Next(MachineRepresentation::kWord32));
    } else {
      msig.AddParam(signature->GetParam(i));
      locations.AddParam(params.Next(signature->GetParam(i).representation()));
    }
  }

  return new (zone) CallDescriptor(          // --
      descriptor->kind(),                    // kind
      descriptor->GetInputType(0),           // target MachineType
      descriptor->GetInputLocation(0),       // target location
      msig.Build(),                          // machine_sig
      locations.Build(),                     // location_sig
      params.stack_offset,                   // stack_parameter_count
      descriptor->properties(),              // properties
      descriptor->CalleeSavedRegisters(),    // callee-saved registers
      descriptor->CalleeSavedFPRegisters(),  // callee-saved fp regs
      descriptor->flags(),                   // flags
      descriptor->debug_name());

  return descriptor;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
