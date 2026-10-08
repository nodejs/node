//===-- Writer definition for printf ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITER_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

#define HANDLE_WRITE_MODE(MODE) MODE,
enum class WriteMode {
#include "src/__support/printf_core/write_modes.def"
};
#undef HANDLE_WRITE_MODE

// Helper to omit the template argument if we are using runtime dispatch and
// avoid multiple copies of the converter functions.
template <WriteMode write_mode> struct Mode {
#ifdef LIBC_COPT_PRINTF_RUNTIME_DISPATCH
  static constexpr WriteMode value = WriteMode::RUNTIME_DISPATCH;
#else
  static constexpr WriteMode value = write_mode;
#endif
};

template <WriteMode write_mode> class Writer;

template <WriteMode write_mode> struct WriteBuffer {
  char *buff;
  size_t buff_len;
  size_t buff_cur = 0;
  // The current writing mode in case the user wants runtime dispatch of the
  // stream writer with function pointers.
  [[maybe_unused]] WriteMode write_mode_;

protected:
  LIBC_INLINE WriteBuffer(char *buff, size_t buff_len, WriteMode mode)
      : buff(buff), buff_len(buff_len), write_mode_(mode) {}

private:
  friend class Writer<write_mode>;
  // The overflow_write method will handle the case when adding new_str to
  // the buffer would overflow it. Specific actions will depend on the buffer
  // type / write_mode.
  LIBC_INLINE int overflow_write(cpp::string_view new_str);
};

// Buffer variant that discards characters that don't fit into the buffer.
struct DropOverflowBuffer
    : public WriteBuffer<Mode<WriteMode::FILL_BUFF_AND_DROP_OVERFLOW>::value> {
  LIBC_INLINE DropOverflowBuffer(char *buff, size_t buff_len)
      : WriteBuffer<Mode<WriteMode::FILL_BUFF_AND_DROP_OVERFLOW>::value>(
            buff, buff_len, WriteMode::FILL_BUFF_AND_DROP_OVERFLOW) {}

  LIBC_INLINE int fill_remaining_to_buff(cpp::string_view new_str) {
    if (buff_cur < buff_len) {
      size_t bytes_to_write = buff_len - buff_cur;
      if (bytes_to_write > new_str.size()) {
        bytes_to_write = new_str.size();
      }
      inline_memcpy(buff + buff_cur, new_str.data(), bytes_to_write);
      buff_cur += bytes_to_write;
    }
    return WRITE_OK;
  }
};

// Buffer variant that flushes to stream when it gets full.
struct FlushingBuffer
    : public WriteBuffer<Mode<WriteMode::FLUSH_TO_STREAM>::value> {
  // The stream writer will be called when the buffer is full. It will be passed
  // string_views to write to the stream.
  using StreamWriter = int (*)(cpp::string_view, void *);
  const StreamWriter stream_writer;
  void *output_target;

  LIBC_INLINE FlushingBuffer(char *buff, size_t buff_len, StreamWriter hook,
                             void *target)
      : WriteBuffer<Mode<WriteMode::FLUSH_TO_STREAM>::value>(
            buff, buff_len, WriteMode::FLUSH_TO_STREAM),
        stream_writer(hook), output_target(target) {}

  // Flushes the entire current buffer to stream, followed by the new_str (if
  // non-empty).
  LIBC_INLINE int flush_to_stream(cpp::string_view new_str) {
    if (buff_cur > 0) {
      int retval = stream_writer({buff, buff_cur}, output_target);
      if (retval < 0)
        return retval;
    }
    if (new_str.size() > 0) {
      int retval = stream_writer(new_str, output_target);
      if (retval < 0)
        return retval;
    }
    buff_cur = 0;
    return WRITE_OK;
  }

  LIBC_INLINE int flush_to_stream() { return flush_to_stream({}); }
};

// Buffer variant that calls a resizing callback when it gets full.
struct ResizingBuffer
    : public WriteBuffer<Mode<WriteMode::RESIZE_AND_FILL_BUFF>::value> {
  using ResizeWriter = int (*)(cpp::string_view, ResizingBuffer *);
  const ResizeWriter resize_writer;
  const char *init_buff; // for checking when resize.

  LIBC_INLINE ResizingBuffer(char *buff, size_t buff_len, ResizeWriter hook)
      : WriteBuffer<Mode<WriteMode::RESIZE_AND_FILL_BUFF>::value>(
            buff, buff_len, WriteMode::RESIZE_AND_FILL_BUFF),
        resize_writer(hook), init_buff(buff) {}

  // Invokes the callback that is supposed to resize the buffer and make
  // it large enough to fit the new_str addition.
  LIBC_INLINE int resize_and_write(cpp::string_view new_str) {
    return resize_writer(new_str, this);
  }
};

template <>
LIBC_INLINE int WriteBuffer<WriteMode::RUNTIME_DISPATCH>::overflow_write(
    cpp::string_view new_str) {
  if (write_mode_ == WriteMode::FILL_BUFF_AND_DROP_OVERFLOW)
    return reinterpret_cast<DropOverflowBuffer *>(this)->fill_remaining_to_buff(
        new_str);
  else if (write_mode_ == WriteMode::FLUSH_TO_STREAM)
    return reinterpret_cast<FlushingBuffer *>(this)->flush_to_stream(new_str);
  else if (write_mode_ == WriteMode::RESIZE_AND_FILL_BUFF)
    return reinterpret_cast<ResizingBuffer *>(this)->resize_and_write(new_str);
  __builtin_unreachable();
}

template <>
LIBC_INLINE int
WriteBuffer<WriteMode::FILL_BUFF_AND_DROP_OVERFLOW>::overflow_write(
    cpp::string_view new_str) {
  return reinterpret_cast<DropOverflowBuffer *>(this)->fill_remaining_to_buff(
      new_str);
}

template <>
LIBC_INLINE int WriteBuffer<WriteMode::FLUSH_TO_STREAM>::overflow_write(
    cpp::string_view new_str) {
  return reinterpret_cast<FlushingBuffer *>(this)->flush_to_stream(new_str);
}

template <>
LIBC_INLINE int WriteBuffer<WriteMode::RESIZE_AND_FILL_BUFF>::overflow_write(
    cpp::string_view new_str) {
  return reinterpret_cast<ResizingBuffer *>(this)->resize_and_write(new_str);
}

template <WriteMode write_mode> class Writer final {
  WriteBuffer<write_mode> &wb;
  size_t chars_written = 0;

  LIBC_INLINE int pad(char new_char, size_t length) {
    // First, fill as much of the buffer as possible with the padding char.
    size_t written = 0;
    const size_t buff_space = wb.buff_len - wb.buff_cur;
    // ASSERT: length > buff_space
    if (buff_space > 0) {
      inline_memset(wb.buff + wb.buff_cur, new_char, buff_space);
      wb.buff_cur += buff_space;
      written = buff_space;
    }

    // Next, overflow write the rest of length using the mini_buff.
    constexpr size_t MINI_BUFF_SIZE = 64;
    char mini_buff[MINI_BUFF_SIZE];
    inline_memset(mini_buff, new_char, MINI_BUFF_SIZE);
    cpp::string_view mb_string_view(mini_buff, MINI_BUFF_SIZE);
    while (written + MINI_BUFF_SIZE < length) {
      int result = wb.overflow_write(mb_string_view);
      if (result != WRITE_OK)
        return result;
      written += MINI_BUFF_SIZE;
    }
    cpp::string_view mb_substr = mb_string_view.substr(0, length - written);
    return wb.overflow_write(mb_substr);
  }

public:
  LIBC_INLINE Writer(WriteBuffer<write_mode> &wb) : wb(wb) {}

  // Takes a string, copies it into the buffer if there is space, else passes it
  // to the overflow mechanism to be handled separately.
  LIBC_INLINE int write(cpp::string_view new_string) {
    chars_written += new_string.size();
    if (LIBC_LIKELY(wb.buff_cur + new_string.size() <= wb.buff_len)) {
      inline_memcpy(wb.buff + wb.buff_cur, new_string.data(),
                    new_string.size());
      wb.buff_cur += new_string.size();
      return WRITE_OK;
    }
    return wb.overflow_write(new_string);
  }

  // Takes a char and a length, memsets the next length characters of the buffer
  // if there is space, else calls pad which will loop and call the overflow
  // mechanism on a secondary buffer.
  LIBC_INLINE int write(char new_char, size_t length) {
    chars_written += length;

    if (LIBC_LIKELY(wb.buff_cur + length <= wb.buff_len)) {
      inline_memset(wb.buff + wb.buff_cur, static_cast<unsigned char>(new_char),
                    length);
      wb.buff_cur += length;
      return WRITE_OK;
    }
    return pad(new_char, length);
  }

  // Takes a char, copies it into the buffer if there is space, else passes it
  // to the overflow mechanism to be handled separately.
  LIBC_INLINE int write(char new_char) {
    chars_written += 1;
    if (LIBC_LIKELY(wb.buff_cur + 1 <= wb.buff_len)) {
      wb.buff[wb.buff_cur] = new_char;
      wb.buff_cur += 1;
      return WRITE_OK;
    }
    cpp::string_view char_string_view(&new_char, 1);
    return wb.overflow_write(char_string_view);
  }

  LIBC_INLINE size_t get_chars_written() { return chars_written; }
};

// Class-template auto deduction helpers.
Writer(WriteBuffer<WriteMode::FILL_BUFF_AND_DROP_OVERFLOW>)
    -> Writer<WriteMode::FILL_BUFF_AND_DROP_OVERFLOW>;
Writer(WriteBuffer<WriteMode::RESIZE_AND_FILL_BUFF>)
    -> Writer<WriteMode::RESIZE_AND_FILL_BUFF>;
Writer(WriteBuffer<WriteMode::FLUSH_TO_STREAM>)
    -> Writer<WriteMode::FLUSH_TO_STREAM>;

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITER_H
