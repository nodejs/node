#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__loongarch64)

#include <sys/mman.h>
#include <unistd.h>

#include <stdint.h>

namespace {

using node::ffi::FastFFIType;

bool IsFloatType(FastFFIType type) {
  return type == FastFFIType::kFloat32 || type == FastFFIType::kFloat64;
}

bool IsNarrowType(FastFFIType type) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kInt8:
    case FastFFIType::kUint8:
    case FastFFIType::kInt16:
    case FastFFIType::kUint16:
      return true;
    default:
      return false;
  }
}

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

void Emit32(uint32_t** cursor, uint32_t value) {
  *(*cursor)++ = value;
}

void Emit64(uint32_t** cursor, uint64_t value) {
  uint64_t* slot = reinterpret_cast<uint64_t*>(*cursor);
  *slot = value;
  *cursor += 2;
}

void* AllocateCode(size_t code_size) {
  void* code = mmap(nullptr,
                    code_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON,
                    -1,
                    0);
  return code == MAP_FAILED ? nullptr : code;
}

void FreeCode(void* code, size_t code_size) {
  munmap(code, code_size);
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    void* target,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  if (target == nullptr || out == nullptr || IsNarrowType(result)) {
    return false;
  }

  size_t gp_count = 0;
  size_t fp_count = 0;
  for (size_t i = 0; i < argc; i++) {
    if (args[i] == FastFFIType::kBuffer) {
      return false;
    }
    if (IsFloatType(args[i])) {
      fp_count++;
    } else {
      gp_count++;
    }
  }

  // LoongArch64 passes integer arguments in a0..a7. V8's receiver occupies a0,
  // so user GP arguments arrive in a1..a7 and are shifted down before the tail
  // branch. FP arguments are already in fa0..fa7.
  if (gp_count > 7 || fp_count > 8) {
    return false;
  }

  constexpr size_t kCodeSize = 256;
  void* code = AllocateCode(kCodeSize);
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
  Emit64(&cursor, reinterpret_cast<uintptr_t>(target));

  const size_t written = reinterpret_cast<uint8_t*>(cursor) -
                         static_cast<uint8_t*>(code);
  __builtin___clear_cache(static_cast<char*>(code),
                          static_cast<char*>(code) + written);

  if (mprotect(code, kCodeSize, PROT_READ | PROT_EXEC) != 0) {
    FreeCode(code, kCodeSize);
    return false;
  }

  out->code = code;
  out->size = kCodeSize;
  return true;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  if (trampoline == nullptr || trampoline->code == nullptr) {
    return;
  }
  FreeCode(trampoline->code, trampoline->size);
  trampoline->code = nullptr;
  trampoline->size = 0;
}

#endif  // defined(__loongarch64)

#endif  // HAVE_FFI
