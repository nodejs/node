//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Line-oriented reader for File streams.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_LINE_READER_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_LINE_READER_H

#include "hdr/errno_macros.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/span.h"
#include "src/__support/File/file.h"
#include "src/__support/dynamic_buffer.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

// Struct to hold the result of a line read operation.
struct ReadLineResult {
  // Byte length of the returned line after stripping any trailing '\n'.
  // Embedded '\0' bytes and preceding '\r' bytes are preserved and included in
  // this count.
  size_t bytes_read;
  bool truncated;
  // True only when zero raw bytes were read because the stream was already at
  // EOF. A blank line ("\n") or a final line without a trailing newline
  // consumes at least one raw byte and returns eof == false (with
  // bytes_read == 0 for "\n"); the following call returns eof == true.
  bool eof;
};

// Reads a single line from the given file into the provided buffer, stripping
// any trailing '\n'. Does not null-terminate buf; callers that need a C string
// should pass a span with room for a trailing NUL and write buf[bytes_read]. A
// line too long for the buffer is reported as truncated after draining the
// remainder of the line up to '\n' or EOF.
//
// Note: POSIX getline/getdelim cannot be used here because bounded callers
// must operate in-place within a fixed buffer without dynamic heap
// allocations. See the DynamicBuffer overload for callers that own their
// buffer and may grow it.
[[nodiscard]] LIBC_INLINE ErrorOr<ReadLineResult>
read_line(File *f, cpp::span<char> buf) {
  LIBC_ASSERT(f != nullptr);

  File::FileLock lock(f);
  size_t bytes_read = 0;
  bool saw_bytes = false;
  bool found_newline = false;
  bool truncated = false;

  for (char &ch : buf) {
    FileIOResult result = f->read_unlocked(&ch, 1);
    if (result.has_error())
      return Error(result.error);
    if (result.value != 1)
      break;
    saw_bytes = true;
    if (ch == '\n') {
      found_newline = true;
      break;
    }
    ++bytes_read;
  }

  if (!found_newline && !f->iseof_unlocked()) {
    char c = '\0';
    while (true) {
      FileIOResult result = f->read_unlocked(&c, 1);
      if (result.has_error())
        return Error(result.error);
      if (result.value != 1)
        break;
      saw_bytes = true;
      if (c == '\n')
        break;
      truncated = true;
    }
  }

  return ReadLineResult{bytes_read, truncated, !saw_bytes};
}

// Reads a single line into a caller-owned buffer, growing it as needed so
// that arbitrarily long lines can be read. Otherwise behaves as the
// fixed-buffer read_line overload; the result is never truncated and not
// null-terminated. Does not allocate on immediate EOF or blank lines. On
// allocation failure, drains the rest of the current line up to '\n' or EOF
// before returning ENOMEM.
[[nodiscard]] LIBC_INLINE ErrorOr<ReadLineResult>
read_line(File *f, DynamicBuffer &buf) {
  LIBC_ASSERT(f != nullptr);

  File::FileLock lock(f);
  size_t bytes_read = 0;
  bool saw_bytes = false;

  while (true) {
    char ch = '\0';
    FileIOResult result = f->read_unlocked(&ch, 1);
    if (result.has_error())
      return Error(result.error);
    if (result.value != 1)
      break;
    saw_bytes = true;
    if (ch == '\n')
      break;

    if (LIBC_UNLIKELY(bytes_read == cpp::numeric_limits<size_t>::max()) ||
        !buf.reserve(bytes_read + 1)) {
      char c = '\0';
      while (true) {
        FileIOResult drain = f->read_unlocked(&c, 1);
        if (drain.has_error())
          return Error(drain.error);
        if (drain.value != 1 || c == '\n')
          break;
      }
      return Error(ENOMEM);
    }

    buf.span()[bytes_read++] = ch;
  }

  return ReadLineResult{bytes_read, /*truncated=*/false, !saw_bytes};
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_LINE_READER_H
