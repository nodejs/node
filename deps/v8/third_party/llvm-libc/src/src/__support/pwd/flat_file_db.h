//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Generic flat-file database template engine.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PWD_FLAT_FILE_DB_H
#define LLVM_LIBC_SRC___SUPPORT_PWD_FLAT_FILE_DB_H

#include "hdr/errno_macros.h"
#include "hdr/stdio_macros.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/functional.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/span.h"
#include "src/__support/File/file.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/pwd/dynamic_buffer.h"

namespace LIBC_NAMESPACE_DECL {
namespace pwd {

// Struct to hold the result of a line read operation.
struct ReadLineResult {
  size_t bytes_read;
  bool truncated;
  // True only when zero raw bytes were read because the stream was already at
  // EOF. A blank line ("\n") or a final line without a trailing newline
  // consumes at least one raw byte and returns eof == false (with
  // bytes_read == 0 for "\n"); the following call returns eof == true.
  bool eof;
};

// Parses a record in place and fills entry.
// line is the record bytes including the terminating NUL (line.back() == '\0').
// scratch is spare writable storage for auxiliary structures (such as pointer
// arrays) and may be empty. Specialisations that require scratch storage must
// return Error(ERANGE) prior to modifying the buffer if scratch is
// insufficient.
template <typename EntryType>
ErrorOr<void> parse_line(cpp::span<char> line, cpp::span<char> scratch,
                         EntryType *entry);

// Generic flat colon-delimited database engine.
template <typename EntryType> class FlatFileDatabase {
public:
  using Matcher = cpp::function<bool(const EntryType &)>;

private:
  const char *file_path;
  File *file = nullptr;

  // Closes the file stream if open.
  LIBC_INLINE ErrorOr<void> clear_file_stream() {
    if (file) {
      int result = file->close();
      file = nullptr;
      if (result != 0)
        return Error(result);
    }
    return {};
  }

  // Reads a single line from the given file into the provided buffer, stripping
  // any trailing '\n' and ensuring the result is null-terminated. A line too
  // long for the buffer is reported as truncated.
  //
  // Note: POSIX getline/getdelim cannot be used here because user database
  // iteration and lookups must operate in-place within a fixed, bounded buffer
  // without dynamic heap allocations during getnext. See read_line_growing for
  // the variant used by the non-reentrant interfaces, which own their buffer
  // and may grow it.
  LIBC_INLINE static ErrorOr<ReadLineResult> read_line(File *f,
                                                       cpp::span<char> buf) {
    if (!f)
      return Error(EINVAL);
    if (buf.size() < 2)
      return Error(ERANGE);

    File::FileLock lock(f);
    size_t bytes_read = 0;
    FileIOResult result(0);
    bool truncated = false;

    for (char &ch : buf.first(buf.size() - 1)) {
      result = f->read_unlocked(&ch, 1);
      if (result.has_error())
        return Error(result.error);
      if (result.value != 1)
        break;
      ++bytes_read;
      if (ch == '\n')
        break;
    }

    bool eof = (bytes_read == 0);

    auto read_span = buf.first(bytes_read);
    if (result.value == 1 && !read_span.empty() && read_span.back() != '\n') {
      char c = '\0';
      while (true) {
        result = f->read_unlocked(&c, 1);
        if (result.has_error())
          return Error(result.error);
        if (result.value != 1 || c == '\n')
          break;
        truncated = true;
      }
    }

    if (f->error_unlocked())
      return Error(EIO);

    // If the line ended with a newline, strip it.
    if (!read_span.empty() && read_span.back() == '\n')
      --bytes_read;

    buf[bytes_read] = '\0';
    return ReadLineResult{bytes_read, truncated, eof};
  }

  // Reads a single line into a caller-owned buffer, growing it as needed so
  // that arbitrarily long records can be read. Otherwise behaves as read_line;
  // the result is never truncated.
  LIBC_INLINE static ErrorOr<ReadLineResult>
  read_line_growing(File *f, DynamicBuffer &buf) {
    if (!f)
      return Error(EINVAL);

    File::FileLock lock(f);
    size_t bytes_read = 0;

    while (true) {
      // One byte for the character about to be read, one for the terminator.
      if (bytes_read > cpp::numeric_limits<size_t>::max() - 2 ||
          !buf.reserve(bytes_read + 2)) {
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

      char ch = '\0';
      FileIOResult result = f->read_unlocked(&ch, 1);
      if (result.has_error())
        return Error(result.error);
      if (result.value != 1)
        break;

      buf.span()[bytes_read++] = ch;
      if (ch == '\n')
        break;
    }

    if (f->error_unlocked())
      return Error(EIO);

    bool eof = (bytes_read == 0);

    // If the line ended with a newline, strip it.
    if (bytes_read > 0 && buf.span()[bytes_read - 1] == '\n')
      --bytes_read;

    buf.span()[bytes_read] = '\0';
    return ReadLineResult{bytes_read, /*truncated=*/false, eof};
  }

public:
  LIBC_INLINE constexpr explicit FlatFileDatabase(const char *path)
      : file_path(path) {}

  FlatFileDatabase(const FlatFileDatabase &) = delete;
  FlatFileDatabase &operator=(const FlatFileDatabase &) = delete;

  // Sets or overrides the file path for database operations.
  LIBC_INLINE void set_path(const char *path) {
    if (!path)
      return;
    clear_file_stream();
    file_path = path;
  }

  // Opens or rewinds the database file stream.
  LIBC_INLINE ErrorOr<void> setdb() {
    if (!file) {
      auto result = openfile(file_path, "r");
      if (!result.has_value())
        return Error(result.error());
      file = result.value();
      return {};
    }
    auto result = file->seek(0, SEEK_SET);
    if (!result.has_value())
      return Error(result.error());
    file->clearerr();
    return {};
  }

  // Closes the database file stream.
  LIBC_INLINE ErrorOr<void> enddb() { return clear_file_stream(); }

  // Reads and parses the next record from the database into a fixed buffer.
  // Returns true if an entry was read, false if EOF was reached, or an Error on
  // failure. Blank lines are skipped. A record that does not fit in the buffer
  // is reported as ERANGE.
  LIBC_INLINE ErrorOr<bool> getnext(EntryType *entry, cpp::span<char> buffer) {
    if (!entry)
      return Error(EINVAL);

    if (!file) {
      auto res = setdb();
      if (!res.has_value())
        return Error(res.error());
    }

    while (true) {
      auto result = read_line(file, buffer);
      if (!result.has_value())
        return Error(result.error());

      ReadLineResult res = result.value();
      if (res.eof)
        return false; // EOF

      // Skip blank lines.
      if (res.bytes_read == 0)
        continue;

      if (res.truncated)
        return Error(ERANGE);

      // Slicing at bytes_read + 1 includes the terminating null byte written
      // by read_line; the remainder of the buffer serves as scratch space.
      auto parse_res =
          parse_line<EntryType>(buffer.first(res.bytes_read + 1),
                                buffer.subspan(res.bytes_read + 1), entry);
      if (!parse_res.has_value())
        return Error(parse_res.error());
      return true;
    }
  }

  // Reads and parses the next record from the database into a caller-owned
  // buffer, growing it as needed. Behaves as the fixed-buffer overload except
  // that a long record grows the buffer rather than producing ERANGE.
  LIBC_INLINE ErrorOr<bool> getnext(EntryType *entry, DynamicBuffer &buffer) {
    if (!entry)
      return Error(EINVAL);

    if (!file) {
      auto res = setdb();
      if (!res.has_value())
        return Error(res.error());
    }

    while (true) {
      auto result = read_line_growing(file, buffer);
      if (!result.has_value())
        return Error(result.error());

      ReadLineResult res = result.value();
      if (res.eof)
        return false; // EOF

      // Skip blank lines.
      if (res.bytes_read == 0)
        continue;

      while (true) {
        // Slicing at bytes_read + 1 includes the terminating null byte written
        // by read_line_growing; the remainder of the buffer serves as scratch
        // space.
        auto parse_res = parse_line<EntryType>(
            buffer.span().first(res.bytes_read + 1),
            buffer.span().subspan(res.bytes_read + 1), entry);
        if (parse_res.has_value())
          return true;
        if (parse_res.error() != ERANGE)
          return Error(parse_res.error());
        if (!buffer.grow())
          return Error(ENOMEM);
      }
    }
  }

  // Searches for a record matching a given predicate. Returns true if the
  // entry was found, false if it's missing, or an Error if lookup failed.
  //
  // When BufferType is cpp::span<char>, lookup never allocates and returns
  // ERANGE if any record encountered exceeds buffer (matching glibc and
  // FreeBSD). When BufferType is DynamicBuffer, the buffer grows as needed.
  template <typename BufferType>
  LIBC_INLINE ErrorOr<bool> lookup(const Matcher &matcher, EntryType *entry,
                                   BufferType &buffer) {
    if (!entry)
      return Error(EINVAL);

    auto res = setdb();
    if (!res.has_value())
      return Error(res.error());

    while (true) {
      auto next_res = getnext(entry, buffer);
      if (!next_res.has_value())
        return Error(next_res.error());
      if (!next_res.value())
        return false; // EOF without match
      if (matcher(*entry))
        return true;
    }
  }
};

// RAII wrapper around FlatFileDatabase for stack-local database operations.
// Automatically closes the database file stream on destruction.
template <typename EntryType>
class ScopedFlatFileDatabase : public FlatFileDatabase<EntryType> {
public:
  using FlatFileDatabase<EntryType>::FlatFileDatabase;

  LIBC_INLINE ~ScopedFlatFileDatabase() { this->enddb(); }
};

} // namespace pwd
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PWD_FLAT_FILE_DB_H
