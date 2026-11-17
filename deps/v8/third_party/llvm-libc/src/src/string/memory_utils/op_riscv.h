//===-- RISC-V implementation of memory function building blocks ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides x86 specific building blocks to compose memory functions.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_RISCV_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_RISCV_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)

#include "src/__support/common.h"
#include "src/string/memory_utils/op_generic.h"

namespace LIBC_NAMESPACE_DECL {
namespace generic {

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint16_t
template <> struct cmp_is_expensive<uint16_t> : public cpp::false_type {};
template <> LIBC_INLINE bool eq<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint16_t>(p1, offset) == load<uint16_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint16_t>(p1, offset) ^ load<uint16_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return static_cast<int32_t>(load_be<uint16_t>(p1, offset)) -
         static_cast<int32_t>(load_be<uint16_t>(p2, offset));
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint16_t>(CPtr p1, CPtr p2, size_t offset);

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint32_t
template <> struct cmp_is_expensive<uint32_t> : public cpp::false_type {};
template <> LIBC_INLINE bool eq<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint32_t>(p1, offset) == load<uint32_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint32_t>(p1, offset) ^ load<uint32_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load_be<uint32_t>(p1, offset);
  const auto b = load_be<uint32_t>(p2, offset);
  return cmp_uint32_t(a, b);
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint32_t>(CPtr p1, CPtr p2, size_t offset);

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint64_t
template <> struct cmp_is_expensive<uint64_t> : public cpp::true_type {};
template <> LIBC_INLINE bool eq<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint64_t>(p1, offset) == load<uint64_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  return !eq<uint64_t>(p1, p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint64_t>(CPtr p1, CPtr p2, size_t offset);
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint64_t>(CPtr p1, CPtr p2,
                                               size_t offset) {
  const auto a = load_be<uint64_t>(p1, offset);
  const auto b = load_be<uint64_t>(p2, offset);
  return cmp_neq_uint64_t(a, b);
}

} // namespace generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_ARCH_IS_ANY_RISCV
#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_RISCV_H
