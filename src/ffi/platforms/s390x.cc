#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__s390x__)

#include "ffi/platforms/scalar_trampoline.h"

#include <stdint.h>

namespace {

using node::ffi::FastFFIType;
using node::ffi::scalar_trampoline::AllocateCode;
using node::ffi::scalar_trampoline::CanCreate;
using node::ffi::scalar_trampoline::FinalizeCode;
using node::ffi::scalar_trampoline::FreeTrampoline;
using node::ffi::scalar_trampoline::IsFloatType;

uint32_t Lgr(unsigned r1, unsigned r2) {
  return 0xb9040000u | (r1 << 4) | r2;
}

uint16_t Br(unsigned r2) {
  return 0x07f0u | r2;
}

uint64_t Lgrl(unsigned r1, int imm) {
  return 0xc40800000000ull | (static_cast<uint64_t>(r1) << 36) |
         (static_cast<uint32_t>(imm));
}

void Emit16(uint8_t** cursor, uint16_t value) {
  *(*cursor)++ = value >> 8;
  *(*cursor)++ = value;
}

void Emit32(uint8_t** cursor, uint32_t value) {
  *(*cursor)++ = value >> 24;
  *(*cursor)++ = value >> 16;
  *(*cursor)++ = value >> 8;
  *(*cursor)++ = value;
}

void Emit48(uint8_t** cursor, uint64_t value) {
  *(*cursor)++ = value >> 40;
  *(*cursor)++ = value >> 32;
  *(*cursor)++ = value >> 24;
  *(*cursor)++ = value >> 16;
  *(*cursor)++ = value >> 8;
  *(*cursor)++ = value;
}

void Emit64(uint8_t** cursor, uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    *(*cursor)++ = value >> shift;
  }
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    const node::ffi::FastFFITrampolineConfig& config,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  // Linux s390x passes integer arguments in r2..r6. V8's receiver occupies r2,
  // so user GP arguments arrive in r3..r6 and are shifted down before the tail
  // branch. FP arguments are already in f0, f2, f4, and f6.
  size_t gp_count;
  if (!CanCreate(config, args, argc, result, out, 4, 4, &gp_count)) {
    return false;
  }

  void* code = AllocateCode();
  if (code == nullptr) {
    return false;
  }

  uint8_t* cursor = static_cast<uint8_t*>(code);
  unsigned gp_index = 0;
  for (size_t i = 0; i < argc; i++) {
    if (IsFloatType(args[i])) {
      continue;
    }
    const unsigned target_reg = 2 + gp_index;
    const unsigned incoming_reg = target_reg + 1;
    Emit32(&cursor, Lgr(target_reg, incoming_reg));
    gp_index++;
  }

  // Load the target address from the literal pool into r1 and tail-branch.
  Emit48(&cursor, Lgrl(1, 4));           // lgrl r1, literal
  Emit16(&cursor, Br(1));                // br r1
  Emit64(&cursor, reinterpret_cast<uintptr_t>(config.target));

  const size_t written = cursor - static_cast<uint8_t*>(code);
  return FinalizeCode(code, written, out);
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  FreeTrampoline(trampoline);
}

#endif  // defined(__s390x__)

#endif  // HAVE_FFI
