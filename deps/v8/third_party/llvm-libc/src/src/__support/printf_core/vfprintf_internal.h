//===-- Internal implementation header of vfprintf --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_VFPRINTF_INTERNAL_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_VFPRINTF_INTERNAL_H

#include "src/__support/File/file.h"
#include "src/__support/arg_list.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h" // For LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/printf_main.h"
#include "src/__support/printf_core/writer.h"

#include "hdr/types/FILE.h"

namespace LIBC_NAMESPACE_DECL {

namespace internal {
#ifndef LIBC_COPT_STDIO_USE_SYSTEM_FILE
LIBC_INLINE int ferror_unlocked(FILE *f) {
  return reinterpret_cast<LIBC_NAMESPACE::File *>(f)->error_unlocked();
}

LIBC_INLINE void flockfile(FILE *f) {
  reinterpret_cast<LIBC_NAMESPACE::File *>(f)->lock();
}

LIBC_INLINE void funlockfile(FILE *f) {
  reinterpret_cast<LIBC_NAMESPACE::File *>(f)->unlock();
}

LIBC_INLINE FileIOResult fwrite_unlocked(const void *ptr, size_t size,
                                         size_t nmemb, FILE *f) {
  return reinterpret_cast<LIBC_NAMESPACE::File *>(f)->write_unlocked(
      ptr, size * nmemb);
}
#else  // defined(LIBC_COPT_STDIO_USE_SYSTEM_FILE)
LIBC_INLINE int ferror_unlocked(::FILE *f) { return ::ferror_unlocked(f); }

LIBC_INLINE void flockfile(::FILE *f) { ::flockfile(f); }

LIBC_INLINE void funlockfile(::FILE *f) { ::funlockfile(f); }

LIBC_INLINE FileIOResult fwrite_unlocked(const void *ptr, size_t size,
                                         size_t nmemb, ::FILE *f) {
  // Need to use system errno in this case, as system write will set this errno
  // which we need to propagate back into our code. fwrite only modifies errno
  // if there was an error, and errno may have previously been nonzero. Only
  // return errno if there was an error.
  size_t members_written = ::fwrite_unlocked(ptr, size, nmemb, f);
  return {members_written, members_written == nmemb ? 0 : errno};
}
#endif // LIBC_COPT_STDIO_USE_SYSTEM_FILE
} // namespace internal

namespace printf_core {

LIBC_INLINE int file_write_hook(cpp::string_view new_str, void *fp) {
  ::FILE *target_file = reinterpret_cast<::FILE *>(fp);
  // Write new_str to the target file. The logic preventing a zero-length write
  // is in the writer, so we don't check here.
  auto write_result = internal::fwrite_unlocked(new_str.data(), sizeof(char),
                                                new_str.size(), target_file);
  // Propagate actual system error in FileIOResult.
  if (write_result.has_error())
    return -write_result.error;

  // In case short write occured or error was not set on FileIOResult for some
  // reason.
  if (write_result.value != new_str.size() ||
      internal::ferror_unlocked(target_file))
    return FILE_WRITE_ERROR;

  return WRITE_OK;
}

LIBC_INLINE ErrorOr<size_t> vfprintf_internal(::FILE *__restrict stream,
                                              const char *__restrict format,
                                              internal::ArgList &args) {
  constexpr size_t BUFF_SIZE = 1024;
  char buffer[BUFF_SIZE];
  printf_core::FlushingBuffer wb(buffer, BUFF_SIZE, &file_write_hook,
                                 reinterpret_cast<void *>(stream));
  Writer writer(wb);
  internal::flockfile(stream);
  auto retval = printf_main(&writer, format, args);
  if (!retval.has_value()) {
    internal::funlockfile(stream);
    return retval;
  }
  int flushval = wb.flush_to_stream();
  if (flushval != WRITE_OK)
    retval = Error(-flushval);
  internal::funlockfile(stream);
  return retval;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_VFPRINTF_INTERNAL_H
