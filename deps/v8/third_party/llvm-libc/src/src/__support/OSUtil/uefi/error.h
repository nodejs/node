//===----------- UEFI implementation of error utils --------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H

#include "hdr/errno_macros.h"
#include "include/llvm-libc-types/EFI_STATUS.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

static constexpr int EFI_ERROR_MAX_BIT = cpp::numeric_limits<EFI_STATUS>::max();

static constexpr int EFI_ENCODE_ERROR(int value) {
  return EFI_ERROR_MAX_BIT | (EFI_ERROR_MAX_BIT >> 2) | (value);
}

static constexpr int EFI_ENCODE_WARNING(int value) {
  return (EFI_ERROR_MAX_BIT >> 2) | (value);
}

struct UefiStatusErrnoEntry {
  EFI_STATUS status;
  int errno_value;
};

static constexpr cpp::array<UefiStatusErrnoEntry, 43> UEFI_STATUS_ERRNO_MAP = {{
    {EFI_SUCCESS, 0},
    {EFI_ENCODE_ERROR(EFI_LOAD_ERROR), EINVAL},
    {EFI_ENCODE_ERROR(EFI_INVALID_PARAMETER), EINVAL},
    {EFI_ENCODE_ERROR(EFI_BAD_BUFFER_SIZE), EINVAL},
    {EFI_ENCODE_ERROR(EFI_NOT_READY), EBUSY},
    {EFI_ENCODE_ERROR(EFI_DEVICE_ERROR), EIO},
    {EFI_ENCODE_ERROR(EFI_WRITE_PROTECTED), EPERM},
    {EFI_ENCODE_ERROR(EFI_OUT_OF_RESOURCES), ENOMEM},
    {EFI_ENCODE_ERROR(EFI_VOLUME_CORRUPTED), EROFS},
    {EFI_ENCODE_ERROR(EFI_VOLUME_FULL), ENOSPC},
    {EFI_ENCODE_ERROR(EFI_NO_MEDIA), ENODEV},
    {EFI_ENCODE_ERROR(EFI_MEDIA_CHANGED), ENXIO},
    {EFI_ENCODE_ERROR(EFI_NOT_FOUND), ENOENT},
    {EFI_ENCODE_ERROR(EFI_ACCESS_DENIED), EACCES},
    {EFI_ENCODE_ERROR(EFI_NO_RESPONSE), EBUSY},
    {EFI_ENCODE_ERROR(EFI_NO_MAPPING), ENODEV},
    {EFI_ENCODE_ERROR(EFI_TIMEOUT), EBUSY},
    {EFI_ENCODE_ERROR(EFI_NOT_STARTED), EAGAIN},
    {EFI_ENCODE_ERROR(EFI_ALREADY_STARTED), EINVAL},
    {EFI_ENCODE_ERROR(EFI_ABORTED), EFAULT},
    {EFI_ENCODE_ERROR(EFI_ICMP_ERROR), EIO},
    {EFI_ENCODE_ERROR(EFI_TFTP_ERROR), EIO},
    {EFI_ENCODE_ERROR(EFI_PROTOCOL_ERROR), EINVAL},
    {EFI_ENCODE_ERROR(EFI_INCOMPATIBLE_VERSION), EINVAL},
    {EFI_ENCODE_ERROR(EFI_SECURITY_VIOLATION), EPERM},
    {EFI_ENCODE_ERROR(EFI_CRC_ERROR), EINVAL},
    {EFI_ENCODE_ERROR(EFI_END_OF_MEDIA), EPIPE},
    {EFI_ENCODE_ERROR(EFI_END_OF_FILE), EPIPE},
    {EFI_ENCODE_ERROR(EFI_INVALID_LANGUAGE), EINVAL},
    {EFI_ENCODE_ERROR(EFI_COMPROMISED_DATA), EINVAL},
    {EFI_ENCODE_ERROR(EFI_IP_ADDRESS_CONFLICT), EINVAL},
    {EFI_ENCODE_ERROR(EFI_HTTP_ERROR), EIO},
    {EFI_ENCODE_WARNING(EFI_WARN_UNKNOWN_GLYPH), EINVAL},
    {EFI_ENCODE_WARNING(EFI_WARN_DELETE_FAILURE), EROFS},
    {EFI_ENCODE_WARNING(EFI_WARN_WRITE_FAILURE), EROFS},
    {EFI_ENCODE_WARNING(EFI_WARN_BUFFER_TOO_SMALL), E2BIG},
    {EFI_ENCODE_WARNING(EFI_WARN_STALE_DATA), EINVAL},
    {EFI_ENCODE_WARNING(EFI_WARN_FILE_SYSTEM), EROFS},
    {EFI_ENCODE_WARNING(EFI_WARN_RESET_REQUIRED), EINTR},
}};

LIBC_INLINE int uefi_status_to_errno(EFI_STATUS status) {
  for (auto it = UEFI_STATUS_ERRNO_MAP.begin();
       it != UEFI_STATUS_ERRNO_MAP.end(); it++) {
    const struct UefiStatusErrnoEntry entry = *it;
    if (entry.status == status)
      return entry.errno_value;
  }

  // Unknown type
  return EINVAL;
}

LIBC_INLINE EFI_STATUS errno_to_uefi_status(int errno_value) {
  for (auto it = UEFI_STATUS_ERRNO_MAP.begin();
       it != UEFI_STATUS_ERRNO_MAP.end(); it++) {
    const struct UefiStatusErrnoEntry entry = *it;
    if (entry.errno_value == errno_value)
      return entry.status;
  }

  // Unknown type
  return EFI_INVALID_PARAMETER;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_ERROR_H
