#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__loongarch64)

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

uint32_t Or(unsigned rd, unsigned rj, unsigned rk) {
  return (0x2au << 15) | (rk << 10) | (rj << 5) | rd;
}

uint32_t Pcaddu12i(unsigned rd, int imm20) {
  return (0x0eu << 25) | ((static_cast<uint32_t>(imm20) & 0xfffff) << 5) | rd;
}

uint32_t LdD(unsigned rd, unsigned rj, int imm12) {
  return (0xa3u << 22) | ((static_cast<uint32_t>(imm12) & 0xfff) << 10) |
         (rj << 5) | rd;
}

uint32_t Jirl(unsigned rd, unsigned rj, int imm16) {
  return (0x13u << 26) | ((static_cast<uint32_t>(imm16) & 0xffff) << 10) |
         (rj << 5) | rd;
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    const node::ffi::FastFFITrampolineConfig& config,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  // LoongArch64 passes integer arguments in a0..a7. V8's receiver occupies a0,
  // so user GP arguments arrive in a1..a7 and are shifted down before the tail
  // branch. FP arguments are already in fa0..fa7.
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
    const unsigned target_reg = 4 + gp_index;
    const unsigned incoming_reg = target_reg + 1;
    Emit32(&cursor, Or(target_reg, incoming_reg, 0));
    gp_index++;
  }

  // Load the target address from a nearby literal into t0 and tail-branch.
  Emit32(&cursor, Pcaddu12i(12, 0));    // pcaddu12i t0, 0
  Emit32(&cursor, LdD(12, 12, 16));     // ld.d t0, t0, literal
  Emit32(&cursor, Jirl(0, 12, 0));      // jr t0
  Emit32(&cursor, Or(0, 0, 0));         // nop; align literal to 8 bytes
  Emit64(&cursor, reinterpret_cast<uintptr_t>(config.target));

  const size_t written = reinterpret_cast<uint8_t*>(cursor) -
                         static_cast<uint8_t*>(code);
  return FinalizeCode(code, written, out);
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  FreeTrampoline(trampoline);
}

#endif  // defined(__loongarch64)

#endif  // HAVE_FFI
