//===--- Implementation of a platform independent file data structure -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "file.h"

#include "hdr/errno_macros.h"
#include "hdr/func/realloc.h"
#include "hdr/stdio_macros.h"
#include "hdr/types/off_t.h"
#include "hdr/wchar_macros.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/span.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/wchar/character_converter.h"
#include "src/__support/wchar/wcrtomb.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
File *File::list_all = nullptr;
Mutex File::list_lock(/*timed=*/false, /*recursive=*/false, /*robust=*/false,
                      /*pshared=*/false);

// Adds a file to the global list of all open files.
void File::add_file(File *f) {
  File::list_lock.lock();
  f->next = File::list_all;
  f->prev = nullptr;
  if (File::list_all != nullptr)
    File::list_all->prev = f;
  File::list_all = f;
  File::list_lock.unlock();
}

// Removes a file from the global list of all open files.
void File::remove_file(File *f) {
  File::list_lock.lock();
  if (f->prev != nullptr)
    f->prev->next = f->next;
  if (f->next != nullptr)
    f->next->prev = f->prev;
  if (File::list_all == f)
    File::list_all = f->next;
  f->prev = nullptr;
  f->next = nullptr;
  File::list_lock.unlock();
}

File *File::get_first_file() { return File::list_all; }
void File::lock_list() { File::list_lock.lock(); }
void File::unlock_list() { File::list_lock.unlock(); }

FileIOResult File::write_unlocked(const void *data, size_t len) {
  switch (orientation) {
  case Orientation::WIDE:
    err = true;
    return {0, EINVAL};
  case Orientation::UNORIENTED:
    orientation = Orientation::BYTE;
    break;
  case Orientation::BYTE:
    break;
  }
  return write_unlocked_impl(data, len);
}

FileIOResult File::write_unlocked_impl(const void *data, size_t len) {
  if (!write_allowed()) {
    err = true;
    return {0, EBADF};
  }

  prev_op = FileOp::WRITE;

  if (bufmode == _IONBF) { // unbuffered.
    size_t ret_val =
        write_unlocked_nbf(static_cast<const uint8_t *>(data), len);
    flush_unlocked();
    return ret_val;
  }
  if (bufmode == _IOFBF) // fully buffered
    return write_unlocked_fbf(static_cast<const uint8_t *>(data), len);
  return write_unlocked_lbf(static_cast<const uint8_t *>(data),
                            len); // line buffered
}

FileIOResult File::write_unlocked_nbf(const uint8_t *data, size_t len) {
  if (pos > 0) { // If the buffer is not empty
    // Flush the buffer
    const size_t write_size = pos;
    FileIOResult write_result = platform_write(this, buf, write_size);
    pos = 0; // Buffer is now empty so reset pos to the beginning.
    // If less bytes were written than expected, then an error occurred.
    if (write_result < write_size) {
      err = true;
      // No bytes from data were written, so return 0.
      return {0, write_result.error};
    }
  }

  FileIOResult write_result = platform_write(this, data, len);
  if (write_result < len)
    err = true;
  return write_result;
}

FileIOResult File::write_unlocked_fbf(const uint8_t *data, size_t len) {
  const size_t init_pos = pos;
  const size_t bufspace = bufsize - pos;

  // If data is too large to be buffered at all, then just write it unbuffered.
  if (len > bufspace + bufsize)
    return write_unlocked_nbf(data, len);

  // we split |data| (conceptually) using the split point. Then we handle the
  // two pieces separately.
  const size_t split_point = len < bufspace ? len : bufspace;

  // The primary piece is the piece of |data| we want to write to the buffer
  // before flushing. It will always fit into the buffer, since the split point
  // is defined as being min(len, bufspace), and it will always exist if len is
  // non-zero.
  cpp::span<const uint8_t> primary(data, split_point);

  // The second piece is the remainder of |data|. It is written to the buffer if
  // it fits, or written directly to the output if it doesn't. If the primary
  // piece fits entirely in the buffer, the remainder may be nothing.
  cpp::span<const uint8_t> remainder(
      static_cast<const uint8_t *>(data) + split_point, len - split_point);

  cpp::span<uint8_t> bufref(static_cast<uint8_t *>(buf), bufsize);

  // Copy the first piece into the buffer.
  inline_memcpy(bufref.data() + pos, primary.data(), primary.size());
  pos += primary.size();

  // If there is no remainder, we can return early, since the first piece has
  // fit completely into the buffer.
  if (remainder.size() == 0)
    return len;

  // We need to flush the buffer now, since there is still data and the buffer
  // is full.
  const size_t write_size = pos;

  FileIOResult buf_result = platform_write(this, buf, write_size);
  size_t bytes_written = buf_result.value;

  pos = 0; // Buffer is now empty so reset pos to the beginning.
  // If less bytes were written than expected, then an error occurred. Return
  // the number of bytes that have been written from |data|.
  if (buf_result.has_error() || bytes_written < write_size) {
    err = true;
    return {bytes_written <= init_pos ? 0 : bytes_written - init_pos,
            buf_result.error};
  }

  // The second piece is handled basically the same as the first, although we
  // know that if the second piece has data in it then the buffer has been
  // flushed, meaning that pos is always 0.
  if (remainder.size() < bufsize) {
    inline_memcpy(bufref.data(), remainder.data(), remainder.size());
    pos = remainder.size();
  } else {

    FileIOResult result =
        platform_write(this, remainder.data(), remainder.size());
    bytes_written = result.value;

    // If less bytes were written than expected, then an error occurred. Return
    // the number of bytes that have been written from |data|.
    if (result.has_error() || bytes_written < remainder.size()) {
      err = true;
      return {primary.size() + bytes_written, result.error};
    }
  }

  return len;
}

FileIOResult File::write_unlocked_lbf(const uint8_t *data, size_t len) {
  constexpr uint8_t NEWLINE_CHAR = '\n';
  size_t last_newline = len;
  for (size_t i = len; i >= 1; --i) {
    if (data[i - 1] == NEWLINE_CHAR) {
      last_newline = i - 1;
      break;
    }
  }

  // If there is no newline, treat this as fully buffered.
  if (last_newline == len) {
    return write_unlocked_fbf(data, len);
  }

  // we split |data| (conceptually) using the split point. Then we handle the
  // two pieces separately.
  const size_t split_point = last_newline + 1;

  // The primary piece is everything in |data| up to the newline. It's written
  // unbuffered to the output.
  cpp::span<const uint8_t> primary(data, split_point);

  // The second piece is the remainder of |data|. It is written fully buffered,
  // meaning it may stay in the buffer if it fits.
  cpp::span<const uint8_t> remainder(
      static_cast<const uint8_t *>(data) + split_point, len - split_point);

  size_t written = 0;

  written = write_unlocked_nbf(primary.data(), primary.size());
  if (written < primary.size()) {
    err = true;
    return written;
  }

  flush_unlocked();

  written += write_unlocked_fbf(remainder.data(), remainder.size());
  if (written < len) {
    err = true;
    return written;
  }

  return len;
}

FileIOResult File::read_unlocked(void *data, size_t len) {
  switch (orientation) {
  case Orientation::WIDE:
    err = true;
    return {0, EINVAL};
  case Orientation::UNORIENTED:
    orientation = Orientation::BYTE;
    break;
  case Orientation::BYTE:
    break;
  }
  return read_unlocked_impl(data, len);
}

FileIOResult File::read_unlocked_impl(void *data, size_t len) {
  if (!read_allowed()) {
    err = true;
    return {0, EBADF};
  }

  prev_op = FileOp::READ;

  if (bufmode == _IONBF) { // unbuffered.
    return read_unlocked_nbf(static_cast<uint8_t *>(data), len);
  } else if (bufmode == _IOFBF) { // fully buffered
    return read_unlocked_fbf(static_cast<uint8_t *>(data), len);
  } else /*if (bufmode == _IOLBF) */ { // line buffered
    // There is no line buffered mode for read. Use fully buffered instead.
    return read_unlocked_fbf(static_cast<uint8_t *>(data), len);
  }
}

size_t File::copy_data_from_buf(uint8_t *data, size_t len) {
  cpp::span<uint8_t> bufref(static_cast<uint8_t *>(buf), bufsize);
  cpp::span<uint8_t> dataref(static_cast<uint8_t *>(data), len);

  // Because read_limit is always greater than equal to pos,
  // available_data is never a wrapped around value.
  size_t available_data = read_limit - pos;
  if (len <= available_data) {
    inline_memcpy(dataref.data(), bufref.data() + pos, len);
    pos += len;
    return len;
  }

  // Copy all of the available data.
  inline_memcpy(dataref.data(), bufref.data() + pos, available_data);
  read_limit = pos = 0; // Reset the pointers.

  return available_data;
}

FileIOResult File::read_unlocked_fbf(uint8_t *data, size_t len) {
  // Read data from the buffer first.
  size_t available_data = copy_data_from_buf(data, len);
  if (available_data == len)
    return available_data;

  // Update the dataref to reflect that fact that we have already
  // copied |available_data| into |data|.
  size_t to_fetch = len - available_data;
  cpp::span<uint8_t> dataref(static_cast<uint8_t *>(data) + available_data,
                             to_fetch);

  if (to_fetch > bufsize) {
    FileIOResult result = platform_read(this, dataref.data(), to_fetch);
    size_t fetched_size = result.value;
    if (result.has_error() || fetched_size < to_fetch) {
      if (!result.has_error())
        eof = true;
      else
        err = true;
      return {available_data + fetched_size, result.error};
    }
    return len;
  }

  // Fetch and buffer another buffer worth of data.
  FileIOResult result = platform_read(this, buf, bufsize);
  size_t fetched_size = result.value;
  read_limit += fetched_size;
  size_t transfer_size = fetched_size >= to_fetch ? to_fetch : fetched_size;
  inline_memcpy(dataref.data(), buf, transfer_size);
  pos += transfer_size;
  if (result.has_error() || fetched_size < to_fetch) {
    if (!result.has_error())
      eof = true;
    else
      err = true;
  }
  return {transfer_size + available_data, result.error};
}

FileIOResult File::read_unlocked_nbf(uint8_t *data, size_t len) {
  // Check whether there is a character in the ungetc buffer.
  size_t available_data = copy_data_from_buf(data, len);
  if (available_data == len)
    return available_data;

  // Directly copy the data into |data|.
  cpp::span<uint8_t> dataref(static_cast<uint8_t *>(data) + available_data,
                             len - available_data);
  FileIOResult result = platform_read(this, dataref.data(), dataref.size());

  if (result.has_error() || result < dataref.size()) {
    if (!result.has_error())
      eof = true;
    else
      err = true;
  }
  return {result + available_data, result.error};
}

int File::ungetc_unlocked(int c) {
  // There is no meaning to unget if:
  // 1. You are trying to push back EOF.
  // 2. Read operations are not allowed on this file.
  // 3. The previous operation was a write operation.
  if (c == EOF || !read_allowed() || (prev_op == FileOp::WRITE))
    return EOF;

  switch (orientation) {
  case Orientation::WIDE:
    err = true;
    return EOF;
  case Orientation::UNORIENTED:
    orientation = Orientation::BYTE;
    break;
  case Orientation::BYTE:
    break;
  }

  cpp::span<uint8_t> bufref(static_cast<uint8_t *>(buf), bufsize);
  if (read_limit == 0) {
    // If |read_limit| is zero, it can mean three things:
    //   a. This file was just created.
    //   b. The previous operation was a seek operation.
    //   c. The previous operation was a read operation which emptied
    //      the buffer.
    // For all the above cases, we simply write |c| at the beginning
    // of the buffer and bump |read_limit|. Note that |pos| will also
    // be zero in this case, so we don't need to adjust it.
    bufref[0] = static_cast<unsigned char>(c);
    ++read_limit;
  } else {
    // If |read_limit| is non-zero, it means that there is data in the buffer
    // from a previous read operation. Which would also mean that |pos| is not
    // zero. So, we decrement |pos| and write |c| in to the buffer at the new
    // |pos|. If too many ungetc operations are performed without reads, it
    // can lead to (pos == 0 but read_limit != 0). We will just error out in
    // such a case.
    if (pos == 0)
      return EOF;
    --pos;
    bufref[pos] = static_cast<unsigned char>(c);
  }

  eof = false; // There is atleast one character that can be read now.
  err = false; // This operation was a success.
  return c;
}

ErrorOr<int> File::seek(off_t offset, int whence) {
  FileLock lock(this);
  if (prev_op == FileOp::WRITE && pos > 0) {

    FileIOResult buf_result = platform_write(this, buf, pos);
    if (buf_result.has_error() || buf_result.value < pos) {
      err = true;
      return Error(buf_result.error);
    }
  } else if (prev_op == FileOp::READ && whence == SEEK_CUR) {
    // More data could have been read out from the platform file than was
    // required. So, we have to adjust the offset we pass to platform seek
    // function. Note that read_limit >= pos is always true.
    offset -= (read_limit - pos);
  }
  auto result = platform_seek(this, offset, whence);
  if (!result.has_value())
    return Error(result.error());

  pos = read_limit = 0;
  prev_op = FileOp::SEEK;
  // Reset the eof flag as a seek might move the file position to some place
  // readable.
  eof = false;
  if (orientation == Orientation::WIDE ||
      orientation == Orientation::UNORIENTED)
    mbstate = internal::mbstate();

  return 0;
}

ErrorOr<off_t> File::tell() {
  FileLock lock(this);
  auto seek_target = eof ? SEEK_END : SEEK_CUR;
  auto result = platform_seek(this, 0, seek_target);
  if (!result.has_value() || result.value() < 0)
    return Error(result.error());
  off_t platform_offset = result.value();
  if (prev_op == FileOp::READ)
    return platform_offset - (read_limit - pos);
  if (prev_op == FileOp::WRITE)
    return platform_offset + pos;
  return platform_offset;
}

int File::flush_unlocked() {
  if (prev_op == FileOp::WRITE && pos > 0) {
    FileIOResult buf_result = platform_write(this, buf, pos);
    if (buf_result.has_error() || buf_result.value < pos) {
      err = true;
      return buf_result.error;
    }
    pos = 0;
  } else if (prev_op == FileOp::READ) {
    if (read_limit > pos) {
      if (!platform_seek(this, -static_cast<off_t>(read_limit - pos), SEEK_CUR)
               .has_value()) {
        // We ignore seek errors for non-seekable files (like pipes) as per
        // POSIX.
      }
    }
    pos = read_limit = 0;
  }
  return 0;
}

int File::set_buffer(void *buffer, size_t size, int buffer_mode) {
  // We do not need to lock the file as this method should be called before
  // other operations are performed on the file.
  if (buffer != nullptr && size == 0)
    return EINVAL;

  switch (buffer_mode) {
  case _IOFBF:
  case _IOLBF:
  case _IONBF:
    break;
  default:
    return EINVAL;
  }

  if (buffer == nullptr && size != 0 && buffer_mode != _IONBF) {
    // We exclude the case of buffer_mode == _IONBF in this branch
    // because we don't need to allocate buffer in such a case.
    if (own_buf) {
      // This is one of the places where a C allocation function is used
      // as C++ does not have an equivalent of realloc.
      buf = reinterpret_cast<uint8_t *>(realloc(buf, size));
      if (buf == nullptr)
        return ENOMEM;
    } else {
      AllocChecker ac;
      buf = new (ac) uint8_t[size];
      if (!ac)
        return ENOMEM;
      own_buf = true;
    }
    bufsize = size;
    // TODO: Handle allocation failures.
  } else {
    if (own_buf)
      delete buf;
    if (buffer_mode != _IONBF) {
      buf = static_cast<uint8_t *>(buffer);
      bufsize = size;
    } else {
      // We don't need any buffer.
      buf = nullptr;
      bufsize = 0;
    }
    own_buf = false;
  }
  bufmode = buffer_mode;
  adjust_buf();
  return 0;
}

File::ModeFlags File::mode_flags(const char *mode) {
  // First character in |mode| should be 'a', 'r' or 'w'.
  if (*mode != 'a' && *mode != 'r' && *mode != 'w')
    return 0;

  // There should be exaclty one main mode ('a', 'r' or 'w') character.
  // If there are more than one main mode characters listed, then
  // we will consider |mode| as incorrect and return 0;
  int main_mode_count = 0;

  ModeFlags flags = 0;
  for (; *mode != '\0'; ++mode) {
    switch (*mode) {
    case 'r':
      flags |= static_cast<ModeFlags>(OpenMode::READ);
      ++main_mode_count;
      break;
    case 'w':
      flags |= static_cast<ModeFlags>(OpenMode::WRITE);
      ++main_mode_count;
      break;
    case '+':
      flags |= static_cast<ModeFlags>(OpenMode::PLUS);
      break;
    case 'b':
      flags |= static_cast<ModeFlags>(ContentType::BINARY);
      break;
    case 'a':
      flags |= static_cast<ModeFlags>(OpenMode::APPEND);
      ++main_mode_count;
      break;
    case 'x':
      flags |= static_cast<ModeFlags>(CreateType::EXCLUSIVE);
      break;
    default:
      return 0;
    }
  }

  if (main_mode_count != 1)
    return 0;

  return flags;
}

FileIOResult File::write_unlocked(const wchar_t *ws, size_t len) {
  switch (orientation) {
  case Orientation::BYTE:
    err = true;
    return {0, EINVAL};
  case Orientation::UNORIENTED:
    orientation = Orientation::WIDE;
    break;
  case Orientation::WIDE:
    break;
  }

  size_t written = 0;
  for (size_t i = 0; i < len; ++i) {
    internal::CharacterConverter cr(&mbstate);
    int push_err = cr.push(static_cast<char32_t>(ws[i]));
    if (push_err != 0) {
      err = true;
      return {written, push_err};
    }
    // buffer the whole wchar to save on calls to write.
    char buffer[4];
    size_t char_size = 0;
    while (!cr.isEmpty()) {
      auto pop_res = cr.pop<char8_t>();
      if (!pop_res.has_value()) {
        err = true;
        return {written, pop_res.error()};
      }
      char8_t byte = pop_res.value();
      buffer[char_size] = byte;
      ++char_size;
    }
    auto write_res = write_unlocked_impl(buffer, char_size);
    if (write_res.has_error())
      return {written, write_res.error};
    if (write_res.value < char_size) {
      err = true;
      return {written, EIO};
    }
    ++written;
  }
  return {written, 0};
}

FileIOResult File::read_unlocked(wchar_t *ws, size_t len) {
  switch (orientation) {
  case Orientation::BYTE:
    err = true;
    return {0, EINVAL};
  case Orientation::UNORIENTED:
    orientation = Orientation::WIDE;
    break;
  case Orientation::WIDE:
    break;
  }

  size_t read_count = 0;
  for (size_t i = 0; i < len; ++i) {
    internal::CharacterConverter cr(&mbstate);
    while (!cr.isFull()) {
      uint8_t byte;
      auto read_res = read_unlocked_impl(&byte, 1);
      if (read_res.has_error())
        return {read_count, read_res.error};
      if (read_res.value == 0) { // EOF
        if (cr.isEmpty())
          return {read_count, 0};
        err = true;
        return {read_count, EILSEQ}; // Incomplete character at EOF
      }
      int push_err = cr.push(static_cast<char8_t>(byte));
      if (push_err != 0) {
        err = true;
        return {read_count, push_err};
      }
    }
    auto pop_res = cr.pop<char32_t>();
    if (!pop_res.has_value()) {
      err = true;
      return {read_count, pop_res.error()};
    }
    ws[i] = static_cast<wchar_t>(pop_res.value());
    ++read_count;
  }
  return {read_count, 0};
}

ErrorOr<wint_t> File::ungetwc_unlocked(wint_t wc) {
  // There is no meaning to unget if:
  // 1. You are trying to push back EOF.
  // 2. Read operations are not allowed on this file.
  // 3. The previous operation was a write operation.
  if (wc == WEOF || !read_allowed() || (prev_op == FileOp::WRITE))
    return WEOF;
  switch (orientation) {
  case Orientation::BYTE:
    err = true;
    return WEOF;
  case Orientation::UNORIENTED:
    orientation = Orientation::WIDE;
    break;
  case Orientation::WIDE:
    break;
  }

  char mb_buf[4];
  auto result = internal::wcrtomb(mb_buf, static_cast<wchar_t>(wc), &mbstate);
  if (!result.has_value())
    return Error(result.error());

  size_t n = result.value();

  if (read_limit == 0) {
    if (n > bufsize)
      return WEOF;

    for (size_t i = 0; i < n; ++i)
      buf[i] = static_cast<uint8_t>(mb_buf[i]);

    read_limit = n;
    pos = 0;
  } else {
    if (pos < n)
      return WEOF;
    pos -= n;
    for (size_t i = 0; i < n; ++i)
      buf[pos + i] = static_cast<uint8_t>(mb_buf[i]);
  }
  eof = false;
  err = false;
  return wc;
}

} // namespace LIBC_NAMESPACE_DECL
