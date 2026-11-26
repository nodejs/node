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
#include "src/__support/CPP/functional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/File/file.h"
#include "src/__support/File/line_reader.h"
#include "src/__support/dynamic_buffer.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace pwd {

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
  using Matcher = cpp::function_ref<bool(const EntryType &)>;

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
  // is reported as ERANGE and leaves the stream positioned at the start of
  // that record so an iterating caller can retry with a larger buffer; single-
  // shot lookup callers close their scoped stream on return.
  LIBC_INLINE ErrorOr<bool> getnext(EntryType *entry, cpp::span<char> buffer) {
    if (!entry)
      return Error(EINVAL);
    if (buffer.size() < 2)
      return Error(ERANGE);

    if (!file) {
      auto res = setdb();
      if (!res.has_value())
        return Error(res.error());
    }

    while (true) {
      auto original_pos = file->tell();
      auto result = read_line(file, buffer.first(buffer.size() - 1));
      if (!result.has_value())
        return Error(result.error());

      ReadLineResult res = result.value();
      if (res.eof)
        return false; // EOF

      // Skip blank lines.
      if (res.bytes_read == 0)
        continue;

      if (res.truncated) {
        if (original_pos.has_value())
          file->seek(original_pos.value(), SEEK_SET);
        return Error(ERANGE);
      }

      buffer[res.bytes_read] = '\0';

      // Slicing at bytes_read + 1 includes the terminating null byte; the
      // remainder of the buffer serves as scratch space.
      auto parse_res =
          parse_line<EntryType>(buffer.first(res.bytes_read + 1),
                                buffer.subspan(res.bytes_read + 1), entry);
      if (!parse_res.has_value()) {
        if (parse_res.error() == ERANGE && original_pos.has_value())
          file->seek(original_pos.value(), SEEK_SET);
        return Error(parse_res.error());
      }
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
      auto result = read_line(file, buffer);
      if (!result.has_value())
        return Error(result.error());

      ReadLineResult res = result.value();
      if (res.eof)
        return false; // EOF

      // Skip blank lines.
      if (res.bytes_read == 0)
        continue;

      if (!buffer.reserve(res.bytes_read + 1))
        return Error(ENOMEM);
      buffer.span()[res.bytes_read] = '\0';

      while (true) {
        // Slicing at bytes_read + 1 includes the terminating null byte; the
        // remainder of the buffer serves as scratch space.
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
