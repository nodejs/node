#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__riscv) && __riscv_xlen == 64

#include "ffi/platforms/scalar_trampoline.h"

#include <stdint.h>

namespace {

using node::ffi::FastFFIType;
using node::ffi::scalar_trampoline::AllocateCode;
using node::ffi::scalar_trampoline::CanCreate;
using node::ffi::scalar_trampoline::Emit32;
using node::ffi::scalar_trampoline::Emit64;
using node::ffi::scalar_trampoline::FinalizeCode;
using node::ffi::scalar_trampoline::FreeTrampoline;
using node::ffi::scalar_trampoline::IsFloatType;

uint32_t Addi(unsigned rd, unsigned rs1, int imm) {
  return ((static_cast<uint32_t>(imm) & 0xfff) << 20) | (rs1 << 15) |
         (rd << 7) | 0x13;
}

uint32_t Ld(unsigned rd, unsigned rs1, int imm) {
  return ((static_cast<uint32_t>(imm) & 0xfff) << 20) | (rs1 << 15) |
         (3u << 12) | (rd << 7) | 0x03;
}

uint32_t Auipc(unsigned rd, int imm20) {
  return (static_cast<uint32_t>(imm20) << 12) | (rd << 7) | 0x17;
}

uint32_t Jalr(unsigned rd, unsigned rs1, int imm) {
  return ((static_cast<uint32_t>(imm) & 0xfff) << 20) | (rs1 << 15) |
         (rd << 7) | 0x67;
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    const node::ffi::FastFFITrampolineConfig& config,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  // RISC-V LP64D passes integer arguments in a0..a7. V8's receiver occupies
  // a0, so user GP arguments arrive in a1..a7 and are shifted down before the
  // tail branch. FP arguments are already in fa0..fa7.
  size_t gp_count;
  if (!CanCreate(config, args, argc, result, out, 7, 8, &gp_count)) {
    return false;
  }

  void* code = AllocateCode();
  if (code == nullptr) {
    return false;
  }

  uint32_t* cursor = static_cast<uint32_t*>(code);
  unsigned gp_index = 0;
  for (size_t i = 0; i < argc; i++) {
    if (IsFloatType(args[i])) {
      continue;
    }
    const unsigned target_reg = 10 + gp_index;
    const unsigned incoming_reg = target_reg + 1;
    Emit32(&cursor, Addi(target_reg, incoming_reg, 0));
    gp_index++;
  }

  // Load the target address from a nearby literal into t0 and tail-branch.
  Emit32(&cursor, Auipc(5, 0));         // auipc t0, 0
  Emit32(&cursor, Ld(5, 5, 16));        // ld t0, literal(t0)
  Emit32(&cursor, Jalr(0, 5, 0));       // jr t0
  Emit32(&cursor, Addi(0, 0, 0));       // nop; align literal to 8 bytes
  Emit64(&cursor, reinterpret_cast<uintptr_t>(config.target));

  const size_t written = reinterpret_cast<uint8_t*>(cursor) -
                         static_cast<uint8_t*>(code);
  return FinalizeCode(code, written, out);
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  FreeTrampoline(trampoline);
}

#endif  // defined(__riscv) && __riscv_xlen == 64

#endif  // HAVE_FFI
