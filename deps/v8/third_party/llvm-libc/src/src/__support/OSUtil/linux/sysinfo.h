//===------------- Linux sysinfo support -------------------------------------//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSINFO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSINFO_H

#include "hdr/errno_macros.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/open.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/read.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/sched_getaffinity.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace sysinfo {

LIBC_INLINE_VAR constexpr char POSSIBLE_NPROC_PATH[] =
    "/sys/devices/system/cpu/possible";
LIBC_INLINE_VAR constexpr char ONLINE_NPROC_PATH[] =
    "/sys/devices/system/cpu/online";

// Parses Linux CPU-list syntax:
//   list  := item (',' item)*
//   item  := number | number '-' number
//   number := [0-9]+
class ProcParser {
  enum class ProcParserState {
    ParseUnstarted,
    ParseNumber,
    ParseRangeSeparator,
    ParseRangeEnd
  };

  ProcParserState state;
  cpp::array<char, 128> buffer;
  int fd;
  size_t cursor;
  size_t buffer_end;
  size_t cpu_count;
  size_t current_number;
  size_t range_start;
  bool has_error;

  LIBC_INLINE static int open_path(const char *path) {
    ErrorOr<int> open_result =
        linux_syscalls::open(path, O_RDONLY | O_CLOEXEC, 0);
    return open_result ? *open_result : -1;
  }

  LIBC_INLINE cpp::optional<char> next_char() {
    if (fd < 0)
      return cpp::nullopt;

    while (cursor == buffer_end) {
      ErrorOr<ssize_t> bytes_read =
          linux_syscalls::read(fd, buffer.data(), buffer.size());
      if (!bytes_read) {
        if (bytes_read.error() == EINTR)
          continue;
        has_error = true;
        return cpp::nullopt;
      }

      if (*bytes_read == 0)
        return cpp::nullopt;

      cursor = 0;
      buffer_end = static_cast<size_t>(*bytes_read);
    }

    return buffer[cursor++];
  }

  LIBC_INLINE bool finish_group() {
    if (state == ProcParserState::ParseUnstarted)
      return true;
    if (state == ProcParserState::ParseRangeSeparator)
      return false;

    if (state == ProcParserState::ParseRangeEnd) {
      if (current_number < range_start)
        return false;
      cpu_count += current_number - range_start + 1;
    } else {
      ++cpu_count;
    }

    current_number = 0;
    range_start = 0;
    state = ProcParserState::ParseUnstarted;
    return true;
  }

  LIBC_INLINE bool consume(char ch) {
    if (internal::isdigit(ch)) {
      // Not using internal::strtointeger here because a number can be across
      // two reads in rare cases.
      current_number = current_number * 10 + static_cast<size_t>(ch - '0');
      if (state == ProcParserState::ParseUnstarted)
        state = ProcParserState::ParseNumber;
      else if (state == ProcParserState::ParseRangeSeparator)
        state = ProcParserState::ParseRangeEnd;
      return true;
    }

    if (ch == '-') {
      if (state != ProcParserState::ParseNumber)
        return false;
      range_start = current_number;
      current_number = 0;
      state = ProcParserState::ParseRangeSeparator;
      return true;
    }

    if (ch == ',' || ch == '\n')
      return finish_group();

    if (ch == ' ' || ch == '\t' || ch == '\r')
      return state == ProcParserState::ParseUnstarted ? true : finish_group();

    return false;
  }

public:
  // Using string view isn't exactly correct because we demands null-terminated
  // strings.
  LIBC_INLINE explicit ProcParser(const char *path)
      : state(ProcParserState::ParseUnstarted), buffer{}, fd(open_path(path)),
        cursor(buffer.size()), buffer_end(buffer.size()), cpu_count(0),
        current_number(0), range_start(0), has_error(fd < 0) {}

  LIBC_INLINE ~ProcParser() {
    if (fd >= 0)
      linux_syscalls::close(fd);
  }

  LIBC_INLINE cpp::optional<size_t> parse() {
    if (fd < 0)
      return cpp::nullopt;

    while (cpp::optional<char> ch = next_char())
      if (!consume(*ch))
        return cpp::nullopt;

    if (has_error)
      return cpp::nullopt;

    if (!finish_group())
      return cpp::nullopt;

    if (cpu_count == 0)
      return cpp::nullopt;
    return cpu_count;
  }
};

LIBC_INLINE cpp::optional<size_t> parse_nproc_from(const char *path) {
  return ProcParser(path).parse();
}

LIBC_INLINE size_t parse_nproc_with_fallback_from(const char *path) {
  if (cpp::optional<size_t> cpu_count = parse_nproc_from(path))
    return *cpu_count;

  cpp::array<unsigned char, 128> mask_buffer = {};

  ErrorOr<int> affinity_result =
      linux_syscalls::sched_getaffinity(0, mask_buffer);
  if (!affinity_result)
    return 1;

  size_t cpu_count = 0;
  for (unsigned char byte : mask_buffer)
    cpu_count += static_cast<size_t>(cpp::popcount(byte));

  return cpu_count > 0 ? cpu_count : 1;
}

} // namespace sysinfo
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSINFO_H
