//===------------- Linux VDSO Header ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_VDSO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_VDSO_H
#include "src/__support/CPP/array.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/threads/callonce.h"

#if defined(LIBC_TARGET_ARCH_IS_X86)
#include "x86_64/vdso.h"
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "aarch64/vdso.h"
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
#include "arm/vdso.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "riscv/vdso.h"
#else
#error "unknown arch"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace vdso {

class Symbol {
  VDSOSym sym;

public:
  LIBC_INLINE_VAR static constexpr size_t COUNT =
      static_cast<size_t>(VDSOSym::VDSOSymCount);
  LIBC_INLINE constexpr explicit Symbol(VDSOSym sym) : sym(sym) {}
  LIBC_INLINE constexpr Symbol(size_t idx) : sym(static_cast<VDSOSym>(idx)) {}
  LIBC_INLINE constexpr cpp::string_view name() const {
    return symbol_name(sym);
  }
  LIBC_INLINE constexpr cpp::string_view version() const {
    return symbol_version(sym);
  }
  LIBC_INLINE constexpr operator size_t() const {
    return static_cast<size_t>(sym);
  }
  LIBC_INLINE constexpr bool is_valid() const {
    return *this < Symbol::global_cache.size();
  }
  using VDSOArray = cpp::array<void *, Symbol::COUNT>;

private:
  static CallOnceFlag once_flag;
  static VDSOArray global_cache;
  static void initialize_vdso_global_cache();

  LIBC_INLINE void *get() const {
    if (name().empty() || !is_valid())
      return nullptr;

    callonce(&once_flag, Symbol::initialize_vdso_global_cache);
    return (global_cache[*this]);
  }
  template <VDSOSym sym> friend struct TypedSymbol;
};

template <VDSOSym sym> struct TypedSymbol {
  LIBC_INLINE constexpr operator VDSOSymType<sym>() const {
    return cpp::bit_cast<VDSOSymType<sym>>(Symbol{sym}.get());
  }
  template <typename... Args>
  LIBC_INLINE auto operator()(Args &&...args) const {
    return this->operator VDSOSymType<sym>()(cpp::forward<Args>(args)...);
  }
};

} // namespace vdso

} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_VDSO_H
