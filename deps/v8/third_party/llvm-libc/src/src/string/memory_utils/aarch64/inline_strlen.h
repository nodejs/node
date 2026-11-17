//===-- Strlen implementation for aarch64 ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_STRLEN_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_STRLEN_H

#include "src/__support/macros/properties/cpu_features.h"

#if defined(__ARM_NEON)
#include "src/__support/CPP/bit.h" // countr_zero
#include <arm_neon.h>
#include <stddef.h> // size_t
namespace LIBC_NAMESPACE_DECL {
namespace internal::neon {
[[maybe_unused]] LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE static size_t
string_length(const char *src) {
  using Vector __attribute__((may_alias)) = uint8x8_t;

  uintptr_t misalign_bytes = reinterpret_cast<uintptr_t>(src) % sizeof(Vector);
  const Vector *block_ptr =
      reinterpret_cast<const Vector *>(src - misalign_bytes);
  Vector v = *block_ptr;
  Vector vcmp = vceqz_u8(v);
  uint64x1_t cmp_mask = vreinterpret_u64_u8(vcmp);
  uint64_t cmp = vget_lane_u64(cmp_mask, 0);
  cmp = cmp >> (misalign_bytes << 3);
  if (cmp)
    return cpp::countr_zero(cmp) >> 3;

  while (true) {
    ++block_ptr;
    v = *block_ptr;
    vcmp = vceqz_u8(v);
    cmp_mask = vreinterpret_u64_u8(vcmp);
    cmp = vget_lane_u64(cmp_mask, 0);
    if (cmp)
      return static_cast<size_t>(reinterpret_cast<uintptr_t>(block_ptr) -
                                 reinterpret_cast<uintptr_t>(src) +
                                 (cpp::countr_zero(cmp) >> 3));
  }
}
} // namespace internal::neon
} // namespace LIBC_NAMESPACE_DECL
#endif // __ARM_NEON

#ifdef LIBC_TARGET_CPU_HAS_SVE
#include "src/__support/macros/optimization.h"
#include <arm_sve.h>
namespace LIBC_NAMESPACE_DECL {
namespace internal::sve {
[[maybe_unused]] LIBC_INLINE static size_t string_length(const char *src) {
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(src);
  // Initialize the first-fault register to all true
  svsetffr();
  const svbool_t all_true = svptrue_b8(); // all true predicate
  svbool_t cmp_zero;
  size_t len = 0;

  for (;;) {
    // Read a vector's worth of bytes, stopping on first fault.
    svuint8_t data = svldff1_u8(all_true, &ptr[len]);
    svbool_t fault_mask = svrdffr_z(all_true);
    bool has_no_fault = svptest_last(all_true, fault_mask);
    if (LIBC_LIKELY(has_no_fault)) {
      // First fault did not fail: the whole vector is valid.
      // Avoid depending on the contents of FFR beyond the branch.
      len += svcntb(); // speculative increment
      cmp_zero = svcmpeq_n_u8(all_true, data, 0);
      bool has_no_zero = !svptest_any(all_true, cmp_zero);
      if (LIBC_LIKELY(has_no_zero))
        continue;
      len -= svcntb(); // undo speculative increment
      break;
    } else {
      // First fault failed: only some of the vector is valid.
      // Perform the comparison only on the valid bytes.
      cmp_zero = svcmpeq_n_u8(fault_mask, data, 0);
      bool has_zero = svptest_any(fault_mask, cmp_zero);
      if (LIBC_LIKELY(has_zero))
        break;
      svsetffr();
      len += svcntp_b8(all_true, fault_mask);
      continue;
    }
  }
  // Select the bytes before the first and count them.
  svbool_t before_zero = svbrkb_z(all_true, cmp_zero);
  len += svcntp_b8(all_true, before_zero);
  return len;
}
} // namespace internal::sve
} // namespace LIBC_NAMESPACE_DECL
#endif // LIBC_TARGET_CPU_HAS_SVE

namespace LIBC_NAMESPACE_DECL {
namespace internal::arch_vector {
[[maybe_unused]] LIBC_INLINE size_t string_length(const char *src) {
#ifdef LIBC_TARGET_CPU_HAS_SVE
  return sve::string_length(src);
#elif defined(__ARM_NEON)
  return neon::string_length(src);
#endif
}
} // namespace internal::arch_vector
} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_STRLEN_H
