//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// UEFI implementation of the internal exit function.
///
//===----------------------------------------------------------------------===//

#include "src/__support/OSUtil/exit.h"
#include "config/app.h"
#include "include/llvm-libc-types/EFI_STATUS.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

[[noreturn]] void exit(int status) {
  // The exit status is handed back to the firmware as the image's EFI_STATUS.
  // Zero maps to EFI_SUCCESS; any other value is reported as EFI_ABORTED so
  // that it is reported as an error by the loader instead of being misread
  // as an unrelated EFI status or warning code.
  EFI_STATUS efi_status = status == 0 ? EFI_SUCCESS : EFI_ABORTED;
  app.system_table->BootServices->Exit(app.image_handle, efi_status, 0,
                                       nullptr);
  __builtin_unreachable();
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
