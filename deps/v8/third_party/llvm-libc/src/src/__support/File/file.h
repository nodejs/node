//===--- A platform independent file data structure -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_FILE_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_FILE_H

#include "hdr/stdint_proxy.h"
#include "hdr/stdio_macros.h"
#include "hdr/types/off_t.h"
#include "hdr/types/wchar_t.h"
#include "hdr/types/wint_t.h"
#include "src/__support/CPP/new.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/threads/mutex.h"
#include "src/__support/wchar/mbstate.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

struct FileIOResult {
  size_t value;
  int error;

  constexpr FileIOResult(size_t val) : value(val), error(0) {}
  constexpr FileIOResult(size_t val, int error) : value(val), error(error) {}

  constexpr bool has_error() { return error != 0; }

  constexpr operator size_t() { return value; }
};

// This a generic base class to encapsulate a platform independent file data
// structure. Platform specific specializations should create a subclass as
// suitable for their platform.
class File {
public:
  static void add_file(File *f);
  static void remove_file(File *f);
  static File *get_first_file();
  static void lock_list();
  static void unlock_list();

  static File *list_all;
  static Mutex list_lock;

  File *get_next() const { return next; }

  static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;

  enum class Orientation { UNORIENTED, BYTE, WIDE };

  using LockFunc = void(File *);
  using UnlockFunc = void(File *);

  using WriteFunc = FileIOResult(File *, const void *, size_t);
  using ReadFunc = FileIOResult(File *, void *, size_t);
  // The SeekFunc is expected to return the current offset of the external
  // file position indicator.
  using SeekFunc = ErrorOr<off_t>(File *, off_t, int);
  using CloseFunc = int(File *);

  using ModeFlags = uint32_t;

  // The three different types of flags below are to be used with '|' operator.
  // Their values correspond to mutually exclusive bits in a 32-bit unsigned
  // integer value. A flag set can include both READ and WRITE if the file
  // is opened in update mode (ie. if the file was opened with a '+' the mode
  // string.)
  enum class OpenMode : ModeFlags {
    READ = 0x1,
    WRITE = 0x2,
    APPEND = 0x4,
    PLUS = 0x8,
  };

  // Denotes a file opened in binary mode (which is specified by including
  // the 'b' character in teh mode string.)
  enum class ContentType : ModeFlags {
    BINARY = 0x10,
  };

  // Denotes a file to be created for writing.
  enum class CreateType : ModeFlags {
    EXCLUSIVE = 0x100,
  };

private:
  enum class FileOp : uint8_t { NONE, READ, WRITE, SEEK };

  // Platform specific functions which create new file objects should initialize
  // these fields suitably via the constructor. Typically, they should be simple
  // syscall wrappers for the corresponding functionality.
  WriteFunc *platform_write;
  ReadFunc *platform_read;
  SeekFunc *platform_seek;
  CloseFunc *platform_close;

  Mutex mutex;

  // For files which are readable, we should be able to support one ungetc
  // operation even if |buf| is nullptr. So, in the constructor of File, we
  // set |buf| to point to this buffer character. It needs to be at least 4
  // bytes so we can store a widechar.
  uint8_t ungetc_buf[4];

  uint8_t *buf;   // Pointer to the stream buffer for buffered streams
  size_t bufsize; // Size of the buffer pointed to by |buf|.

  // Buffering mode to used to buffer.
  int bufmode;

  // If own_buf is true, the |buf| is owned by the stream and will be
  // free-ed when close method is called on the stream.
  bool own_buf;

  // The mode in which the file was opened.
  ModeFlags mode;

  // Current read or write pointer.
  size_t pos;

  // Represents the previous operation that was performed.
  FileOp prev_op;

  // When the buffer is used as a read buffer, read_limit is the upper limit
  // of the index to which the buffer can be read until.
  size_t read_limit;

  bool eof;
  bool err;

  Orientation orientation;
  internal::mbstate mbstate;

  // This is a convenience RAII class to lock and unlock file objects.
  class FileLock {
    File *file;

  public:
    explicit FileLock(File *f) : file(f) { file->lock(); }

    ~FileLock() { file->unlock(); }

    FileLock(const FileLock &) = delete;
    FileLock(FileLock &&) = delete;
  };

protected:
  constexpr bool write_allowed() const {
    return mode & (static_cast<ModeFlags>(OpenMode::WRITE) |
                   static_cast<ModeFlags>(OpenMode::APPEND) |
                   static_cast<ModeFlags>(OpenMode::PLUS));
  }

  constexpr bool read_allowed() const {
    return mode & (static_cast<ModeFlags>(OpenMode::READ) |
                   static_cast<ModeFlags>(OpenMode::PLUS));
  }

public:
  // We want this constructor to be constexpr so that global file objects
  // like stdout do not require invocation of the constructor which can
  // potentially lead to static initialization order fiasco. Consequently,
  // we will assume that the |buffer| and |buffer_size| argument are
  // meaningful - that is, |buffer| is nullptr if and only if |buffer_size|
  // is zero. This way, we will not have to employ the semantics of
  // the set_buffer method and allocate a buffer.
  constexpr File(WriteFunc *wf, ReadFunc *rf, SeekFunc *sf, CloseFunc *cf,
                 uint8_t *buffer, size_t buffer_size, int buffer_mode,
                 bool owned, ModeFlags modeflags)
      : platform_write(wf), platform_read(rf), platform_seek(sf),
        platform_close(cf), mutex(/*timed=*/false, /*recursive=*/false,
                                  /*robust=*/false, /*pshared=*/false),
        ungetc_buf{}, buf(buffer), bufsize(buffer_size), bufmode(buffer_mode),
        own_buf(owned), mode(modeflags), pos(0), prev_op(FileOp::NONE),
        read_limit(0), eof(false), err(false),
        orientation(Orientation::UNORIENTED), mbstate(), prev(nullptr),
        next(nullptr) {
    adjust_buf();
  }

  // Buffered write of |len| bytes from |data| without the file lock.
  FileIOResult write_unlocked(const void *data, size_t len);

  // Buffered write of |len| bytes from |data| under the file lock.
  FileIOResult write(const void *data, size_t len) {
    FileLock l(this);
    return write_unlocked(data, len);
  }

  // Buffered read of |len| bytes into |data| without the file lock.
  FileIOResult read_unlocked(void *data, size_t len);

  // Buffered read of |len| bytes into |data| under the file lock.
  FileIOResult read(void *data, size_t len) {
    FileLock l(this);
    return read_unlocked(data, len);
  }

  ErrorOr<int> seek(off_t offset, int whence);

  ErrorOr<off_t> tell();

  // If buffer has data written to it, flush it out. Does nothing if the
  // buffer is currently being used as a read buffer.
  int flush() {
    FileLock lock(this);
    return flush_unlocked();
  }

  int flush_unlocked();

  // Returns EOF on error and keeps the file unchanged.
  int ungetc_unlocked(int c);

  int ungetc(int c) {
    FileLock lock(this);
    return ungetc_unlocked(c);
  }

  FileIOResult write_unlocked(const wchar_t *ws, size_t len);

  FileIOResult write(const wchar_t *ws, size_t len) {
    FileLock l(this);
    return write_unlocked(ws, len);
  }

  FileIOResult read_unlocked(wchar_t *ws, size_t len);

  FileIOResult read(wchar_t *ws, size_t len) {
    FileLock l(this);
    return read_unlocked(ws, len);
  }

  wint_t ungetwc_unlocked(wint_t wc);

  wint_t ungetwc(wint_t wc) {
    FileLock lock(this);
    return ungetwc_unlocked(wc);
  }

  // Does the following:
  // 1. If in write mode, Write out any data present in the buffer.
  // 2. Call platform_close.
  // platform_close is expected to cleanup the complete file object.
  int close() {
    {
      FileLock lock(this);
      if (prev_op == FileOp::WRITE && pos > 0) {
        auto buf_result = platform_write(this, buf, pos);
        if (buf_result.has_error() || buf_result.value < pos) {
          err = true;
          return buf_result.error;
        }
      }
    }

    // If we own the buffer, delete it before calling the platform close
    // implementation. The platform close should not need to access the buffer
    // and we need to clean it up before the entire structure is removed.
    if (own_buf)
      delete buf;

    // Platform close is expected to cleanup the file data structure which
    // includes the file mutex. Hence, we call platform_close after releasing
    // the file lock. Another thread doing file operations while a thread is
    // closing the file is undefined behavior as per POSIX.
    return platform_close(this);
  }

  // Sets the internal buffer to |buffer| with buffering mode |mode|.
  // |size| is the size of |buffer|. If |size| is non-zero, but |buffer|
  // is nullptr, then a buffer owned by this file will be allocated.
  // Else, |buffer| will not be owned by this file.
  //
  // Will return zero on success, or an error value on failure. Will fail
  // if:
  //   1. |buffer| is not a nullptr but |size| is zero.
  //   2. |buffer_mode| is not one of _IOLBF, IOFBF or _IONBF.
  //   3. If an allocation was required but the allocation failed.
  // For cases 1 and 2, the error returned in EINVAL. For case 3, error returned
  // is ENOMEM.
  int set_buffer(void *buffer, size_t size, int buffer_mode);

  void lock() { mutex.lock(); }
  void unlock() { mutex.unlock(); }

  bool error_unlocked() const { return err; }

  bool error() {
    FileLock l(this);
    return error_unlocked();
  }

  // TODO: https://github.com/llvm/llvm-project/issues/172302
  // MacOS defines clearerr_unlocked as a macro. While pre-processing, the
  // identifier below is substituted for the definition in the SDK, which leads
  // to compile time errors due to ill-formed statements. This is a workaround
  // for the pre-processor.
#pragma push_macro("clearerr_unlocked")
#undef clearerr_unlocked
  void clearerr_unlocked() { err = false; }
#pragma pop_macro("clearerr_unlocked")

  void clearerr() {
    FileLock l(this);
    clearerr_unlocked();
  }

  bool iseof_unlocked() { return eof; }

  bool iseof() {
    FileLock l(this);
    return iseof_unlocked();
  }

  Orientation get_orientation_unlocked() const { return orientation; }

  Orientation get_orientation() {
    FileLock l(this);
    return get_orientation_unlocked();
  }

  Orientation try_set_orientation_unlocked(Orientation o) {
    if (orientation == Orientation::UNORIENTED)
      orientation = o;
    return orientation;
  }

  Orientation try_set_orientation(Orientation o) {
    FileLock l(this);
    return try_set_orientation_unlocked(o);
  }

  // Returns an bit map of flags corresponding to enumerations of
  // OpenMode, ContentType and CreateType.
  static ModeFlags mode_flags(const char *mode);

private:
  FileIOResult write_unlocked_impl(const void *data, size_t len);
  FileIOResult read_unlocked_impl(void *data, size_t len);

  FileIOResult write_unlocked_lbf(const uint8_t *data, size_t len);
  FileIOResult write_unlocked_fbf(const uint8_t *data, size_t len);
  FileIOResult write_unlocked_nbf(const uint8_t *data, size_t len);

  FileIOResult read_unlocked_fbf(uint8_t *data, size_t len);
  FileIOResult read_unlocked_nbf(uint8_t *data, size_t len);
  size_t copy_data_from_buf(uint8_t *data, size_t len);

  constexpr void adjust_buf() {
    if (read_allowed() && (buf == nullptr || bufsize == 0)) {
      // We should allow atleast one ungetc operation.
      // This might give an impression that a buffer will be used even when
      // the user does not want a buffer. But, that will not be the case.
      // For reading, the buffering does not come into play. For writing, let
      // us take up the three different kinds of buffering separately:
      // 1. If user wants _IOFBF but gives a zero buffer, buffering still
      //    happens in the OS layer until the user flushes. So, from the user's
      //    point of view, this single byte buffer does not affect their
      //    experience.
      // 2. If user wants _IOLBF but gives a zero buffer, the reasoning is
      //    very similar to the _IOFBF case.
      // 3. If user wants _IONBF, then the buffer is ignored for writing.
      // So, all of the above cases, having a single ungetc buffer does not
      // affect the behavior experienced by the user.
      buf = ungetc_buf;
      bufsize = sizeof(ungetc_buf);
      own_buf = false; // We shouldn't call free on |buf| when closing the file.
    }
  }

  File *prev;
  File *next;
};

// The implementation of this function is provided by the platform_file
// library.
ErrorOr<File *> openfile(const char *path, const char *mode);

// The platform_file library should implement it if it relevant for that
// platform.
int get_fileno(File *f);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_FILE_H
