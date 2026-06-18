#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#if (defined(__LITTLE_ENDIAN__) ||                                      \
     (defined(__BYTE_ORDER__) &&                                        \
      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) &&                    \
    !defined(_AIX)

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

uint32_t Or(unsigned ra, unsigned rs, unsigned rb) {
  return (31u << 26) | (rs << 21) | (ra << 16) | (rb << 11) | (444u << 1);
}

uint32_t Mr(unsigned ra, unsigned rs) {
  return Or(ra, rs, rs);
}

uint32_t Bl(unsigned instruction_offset) {
  return (18u << 26) | ((instruction_offset & 0x00ffffffu) << 2) | 1u;
}

uint32_t Mfspr(unsigned rt, unsigned spr) {
  return (31u << 26) | (rt << 21) | ((spr & 0x1f) << 16) |
         ((spr >> 5) << 11) | (339u << 1);
}

uint32_t Mtspr(unsigned spr, unsigned rs) {
  return (31u << 26) | (rs << 21) | ((spr & 0x1f) << 16) |
         ((spr >> 5) << 11) | (467u << 1);
}

uint32_t Ld(unsigned rt, unsigned ra, unsigned offset) {
  return (58u << 26) | (rt << 21) | (ra << 16) | (offset & 0xfffcu);
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

  // ELFv2 PPC64LE passes integer arguments in r3..r10. V8's receiver occupies
  // r3, so the scalar-only fast path keeps user GP arguments in r4..r10 and
  // shifts them down before tail-branching to the native target.
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
    const unsigned target_reg = 3 + gp_index;
    const unsigned incoming_reg = target_reg + 1;
    Emit32(&cursor, Mr(target_reg, incoming_reg));
    gp_index++;
  }

  // Load the target address from the literal pool into r12, then branch through
  // CTR. ELFv2 functions can use r12 to establish their TOC on global entry.
  Emit32(&cursor, Bl(1));              // bl .+4
  Emit32(&cursor, Mfspr(12, 8));       // mflr r12
  Emit32(&cursor, Ld(12, 12, 20));     // ld r12, literal-mflr(r12)
  Emit32(&cursor, Mtspr(9, 12));       // mtctr r12
  Emit32(&cursor, 0x4e800420);         // bctr
  Emit32(&cursor, 0x60000000);         // nop; align literal to 8 bytes
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

#else

extern "C" bool node_ffi_create_fast_trampoline(
    void* target,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  return false;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {}

#endif  // PPC64LE non-AIX
#endif  // defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)

#endif  // HAVE_FFI
