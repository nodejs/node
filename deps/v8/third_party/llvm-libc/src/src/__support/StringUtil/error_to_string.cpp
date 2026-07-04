//===-- Implementation of a class for mapping errors to strings -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "error_to_string.h"

#include <stddef.h>

#include "platform_errors.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/StringUtil/message_mapper.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace {

constexpr size_t max_buff_size() {
  constexpr size_t unknown_str_len = sizeof("Unknown error");
  // the buffer should be able to hold "Unknown error" + ' ' + num_str
  return (unknown_str_len + 1 + IntegerToString<int>::buffer_size()) *
         sizeof(char);
}

// This is to hold error strings that have to be custom built. It may be
// rewritten on every call to strerror (or other error to string function).
constexpr size_t ERR_BUFFER_SIZE = max_buff_size();
LIBC_THREAD_LOCAL char error_buffer[ERR_BUFFER_SIZE];

constexpr size_t TOTAL_STR_LEN = total_str_len(PLATFORM_ERRORS);

// Since the StringMappings array is a map from error numbers to their
// corresponding strings, we have to have an array large enough we can use the
// error numbers as indexes. The current linux configuration has 132 values with
// the maximum value being 133 (41 and 58 are skipped). If other platforms use
// negative numbers or discontiguous ranges, then the array should be turned
// into a proper hashmap.
constexpr size_t ERR_ARRAY_SIZE = max_key_val(PLATFORM_ERRORS) + 1;

constexpr MessageMapper<ERR_ARRAY_SIZE, TOTAL_STR_LEN>
    ERROR_MAPPER(PLATFORM_ERRORS);

constexpr MessageMapper<ERR_ARRAY_SIZE, TOTAL_STR_LEN>
    ERRNO_NAME_MAPPER(PLATFORM_ERRNO_NAMES);

cpp::string_view build_error_string(int err_num, cpp::span<char> buffer) {
  // if the buffer can't hold "Unknown error" + ' ' + num_str, then just
  // return "Unknown error".
  if (buffer.size() <
      (sizeof("Unknown error") + 1 + IntegerToString<int>::buffer_size()))
    return const_cast<char *>("Unknown error");

  cpp::StringStream buffer_stream(
      {const_cast<char *>(buffer.data()), buffer.size()});
  buffer_stream << "Unknown error" << ' ' << err_num << '\0';
  return buffer_stream.str();
}

} // namespace

cpp::string_view get_error_string(int err_num) {
  return get_error_string(err_num, {error_buffer, ERR_BUFFER_SIZE});
}

cpp::string_view get_error_string(int err_num, cpp::span<char> buffer) {
  auto opt_str = ERROR_MAPPER.get_str(err_num);
  if (opt_str)
    return *opt_str;
  else
    return build_error_string(err_num, buffer);
}

cpp::optional<cpp::string_view> try_get_errno_name(int err_num) {
  return ERRNO_NAME_MAPPER.get_str(err_num);
}

} // namespace LIBC_NAMESPACE_DECL
