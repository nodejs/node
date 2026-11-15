#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// Shared helpers for the POSIX platforms whose fast FFI trampoline only shifts
// scalar GP arguments down by one register (dropping V8's receiver) and then
// tail-branches to the native target: loong64, ppc64, riscv64 and s390x.

#include "ffi/fast.h"

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>

namespace node::ffi::scalar_trampoline {

// Size of the executable mapping allocated for each trampoline.
constexpr size_t kCodeSize = 256;

inline bool IsFloatType(FastFFIType type) {
  return type == FastFFIType::kFloat32 || type == FastFFIType::kFloat64;
}

inline bool IsNarrowType(FastFFIType type) {
  switch (type) {
    case FastFFIType::kInt8:
    case FastFFIType::kUint8:
    case FastFFIType::kInt16:
    case FastFFIType::kUint16:
      return true;
    default:
      return false;
  }
}

// Validates the signature for the scalar-only fast path. Returns false when
// the trampoline cannot be generated, either because the inputs are invalid,
// the result needs narrowing, an argument is a buffer, or there are more GP or
// FP arguments than the platform passes in registers.
inline bool CanCreate(const FastFFITrampolineConfig& config,
                      const FastFFIType* args,
                      size_t argc,
                      FastFFIType result,
                      const FastFFITrampoline* out,
                      size_t max_gp_args,
                      size_t max_fp_args,
                      size_t* gp_count_out) {
  if (config.target == nullptr || out == nullptr || IsNarrowType(result)) {
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

  if (gp_count > max_gp_args || fp_count > max_fp_args) {
    return false;
  }
  *gp_count_out = gp_count;
  return true;
}

inline void* AllocateCode() {
  void* code = mmap(nullptr,
                    kCodeSize,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON,
                    -1,
                    0);
  return code == MAP_FAILED ? nullptr : code;
}

// Flushes the instruction cache for the `written` bytes emitted at `code`,
// makes the mapping executable and stores it in `out`. On failure the mapping
// is released and false is returned.
inline bool FinalizeCode(void* code, size_t written, FastFFITrampoline* out) {
  __builtin___clear_cache(static_cast<char*>(code),
                          static_cast<char*>(code) + written);

  if (mprotect(code, kCodeSize, PROT_READ | PROT_EXEC) != 0) {
    munmap(code, kCodeSize);
    return false;
  }

  out->code = code;
  out->size = kCodeSize;
  return true;
}

inline void FreeTrampoline(FastFFITrampoline* trampoline) {
  if (trampoline == nullptr || trampoline->code == nullptr) {
    return;
  }
  munmap(trampoline->code, trampoline->size);
  trampoline->code = nullptr;
  trampoline->size = 0;
}

// Word emitters for platforms with fixed-width 32-bit, native-endian
// instructions. The 64-bit literal must be 8-byte aligned by the caller.
inline void Emit32(uint32_t** cursor, uint32_t value) {
  *(*cursor)++ = value;
}

inline void Emit64(uint32_t** cursor, uint64_t value) {
  uint64_t* slot = reinterpret_cast<uint64_t*>(*cursor);
  *slot = value;
  *cursor += 2;
}

}  // namespace node::ffi::scalar_trampoline

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
