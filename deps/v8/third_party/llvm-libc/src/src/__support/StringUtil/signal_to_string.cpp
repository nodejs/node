//===-- Implementation of a class for mapping signals to strings ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "signal_to_string.h"

#include <signal.h>
#include <stddef.h>

#include "platform_signals.h"
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
  constexpr size_t base_str_len = sizeof("Real-time signal");
  // the buffer should be able to hold "Real-time signal" + ' ' + num_str
  return (base_str_len + 1 + IntegerToString<int>::buffer_size()) *
         sizeof(char);
}

// This is to hold signal strings that have to be custom built. It may be
// rewritten on every call to strsignal (or other signal to string function).
constexpr size_t SIG_BUFFER_SIZE = max_buff_size();
LIBC_THREAD_LOCAL char signal_buffer[SIG_BUFFER_SIZE];

constexpr size_t TOTAL_STR_LEN = total_str_len(PLATFORM_SIGNALS);

constexpr size_t SIG_ARRAY_SIZE = max_key_val(PLATFORM_SIGNALS) + 1;

constexpr MessageMapper<SIG_ARRAY_SIZE, TOTAL_STR_LEN>
    signal_mapper(PLATFORM_SIGNALS);

cpp::string_view build_signal_string(int sig_num, cpp::span<char> buffer) {
  cpp::string_view base_str;
  if (sig_num >= SIGRTMIN && sig_num <= SIGRTMAX) {
    base_str = cpp::string_view("Real-time signal");
    sig_num -= SIGRTMIN;
  } else {
    base_str = cpp::string_view("Unknown signal");
  }

  // if the buffer can't hold "Unknown signal" + ' ' + num_str, then just
  // return "Unknown signal".
  if (buffer.size() <
      (base_str.size() + 1 + IntegerToString<int>::buffer_size()))
    return base_str;

  cpp::StringStream buffer_stream(
      {const_cast<char *>(buffer.data()), buffer.size()});
  buffer_stream << base_str << ' ' << sig_num << '\0';
  return buffer_stream.str();
}

} // namespace

cpp::string_view get_signal_string(int sig_num) {
  return get_signal_string(sig_num, {signal_buffer, SIG_BUFFER_SIZE});
}

cpp::string_view get_signal_string(int sig_num, cpp::span<char> buffer) {
  auto opt_str = signal_mapper.get_str(sig_num);
  if (opt_str)
    return *opt_str;
  else
    return build_signal_string(sig_num, buffer);
}

} // namespace LIBC_NAMESPACE_DECL
