#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__s390x__)

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

  // Linux s390x passes integer arguments in r2..r6. V8's receiver occupies r2,
  // so user GP arguments arrive in r3..r6 and are shifted down before the tail
  // branch. FP arguments are already in f0, f2, f4, and f6.
  if (gp_count > 4 || fp_count > 4) {
    return false;
  }

  constexpr size_t kCodeSize = 256;
  void* code = AllocateCode(kCodeSize);
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
  Emit64(&cursor, reinterpret_cast<uintptr_t>(target));

  const size_t written = cursor - static_cast<uint8_t*>(code);
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

#endif  // defined(__s390x__)

#endif  // HAVE_FFI
