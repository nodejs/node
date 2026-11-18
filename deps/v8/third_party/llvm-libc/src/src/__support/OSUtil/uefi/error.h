//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// UEFI implementation of error utils.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H

#include "hdr/errno_macros.h"
#include "include/llvm-libc-types/EFI_STATUS.h"
#include "src/__support/CPP/array.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

struct UefiStatusErrnoEntry {
  EFI_STATUS status;
  int errno_value;
};

// The EFI_* macros already carry the error bit as defined by the UEFI
// specification; warnings are plain values. Several statuses share an errno
// value, and errno_to_uefi_status returns the first entry that matches, so
// the preferred status for each errno value must come first.
LIBC_INLINE_VAR constexpr cpp::array<UefiStatusErrnoEntry, 41>
    UEFI_STATUS_ERRNO_MAP = {{
        {EFI_SUCCESS, 0},
        {EFI_INVALID_PARAMETER, EINVAL},
        {EFI_LOAD_ERROR, EINVAL},
        {EFI_UNSUPPORTED, EINVAL},
        {EFI_BAD_BUFFER_SIZE, EINVAL},
        {EFI_BUFFER_TOO_SMALL, ERANGE},
        {EFI_NOT_READY, EBUSY},
        {EFI_DEVICE_ERROR, EIO},
        {EFI_WRITE_PROTECTED, EPERM},
        {EFI_OUT_OF_RESOURCES, ENOMEM},
        {EFI_VOLUME_CORRUPTED, EROFS},
        {EFI_VOLUME_FULL, ENOSPC},
        {EFI_NO_MEDIA, ENODEV},
        {EFI_MEDIA_CHANGED, ENXIO},
        {EFI_NOT_FOUND, ENOENT},
        {EFI_ACCESS_DENIED, EACCES},
        {EFI_NO_RESPONSE, EBUSY},
        {EFI_NO_MAPPING, ENODEV},
        {EFI_TIMEOUT, EBUSY},
        {EFI_NOT_STARTED, EAGAIN},
        {EFI_ALREADY_STARTED, EINVAL},
        {EFI_ABORTED, EFAULT},
        {EFI_ICMP_ERROR, EIO},
        {EFI_TFTP_ERROR, EIO},
        {EFI_PROTOCOL_ERROR, EINVAL},
        {EFI_INCOMPATIBLE_VERSION, EINVAL},
        {EFI_SECURITY_VIOLATION, EPERM},
        {EFI_CRC_ERROR, EINVAL},
        {EFI_END_OF_MEDIA, EPIPE},
        {EFI_END_OF_FILE, EPIPE},
        {EFI_INVALID_LANGUAGE, EINVAL},
        {EFI_COMPROMISED_DATA, EINVAL},
        {EFI_IP_ADDRESS_CONFLICT, EINVAL},
        {EFI_HTTP_ERROR, EIO},
        {EFI_WARN_UNKNOWN_GLYPH, EINVAL},
        {EFI_WARN_DELETE_FAILURE, EROFS},
        {EFI_WARN_WRITE_FAILURE, EROFS},
        {EFI_WARN_BUFFER_TOO_SMALL, E2BIG},
        {EFI_WARN_STALE_DATA, EINVAL},
        {EFI_WARN_FILE_SYSTEM, EROFS},
        {EFI_WARN_RESET_REQUIRED, EINTR},
    }};

LIBC_INLINE constexpr int uefi_status_to_errno(EFI_STATUS status) {
  for (const UefiStatusErrnoEntry &entry : UEFI_STATUS_ERRNO_MAP)
    if (entry.status == status)
      return entry.errno_value;

  // Unknown status; report it as an I/O error if it is an error code and as
  // success otherwise, since warnings do not fail the operation.
  return EFI_ERROR(status) ? EIO : 0;
}

LIBC_INLINE constexpr EFI_STATUS errno_to_uefi_status(int errno_value) {
  for (const UefiStatusErrnoEntry &entry : UEFI_STATUS_ERRNO_MAP)
    if (entry.errno_value == errno_value)
      return entry.status;

  // Unknown errno
  return EFI_INVALID_PARAMETER;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H
