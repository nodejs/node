// Formatting library for C++ - optional OS-specific functionality
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_OS_H_
#define FMT_OS_H_

#include "format.h"

#ifndef FMT_MODULE
#  include <cerrno>
#  include <cstddef>
#  include <cstdio>
#  include <system_error>  // std::system_error

#  if FMT_HAS_INCLUDE(<xlocale.h>)
#    include <xlocale.h>  // LC_NUMERIC_MASK on macOS
#  endif
#endif  // FMT_MODULE

#ifndef FMT_USE_FCNTL
// UWP doesn't provide _pipe.
#  if FMT_HAS_INCLUDE("winapifamily.h")
#    include <winapifamily.h>
#  endif
#  if (FMT_HAS_INCLUDE(<fcntl.h>) || defined(__APPLE__) || \
       defined(__linux__)) &&                              \
      (!defined(WINAPI_FAMILY) ||                          \
       (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)) &&    \
      !defined(__wasm__)
#    include <fcntl.h>  // for O_RDONLY
#    define FMT_USE_FCNTL 1
#  else
#    define FMT_USE_FCNTL 0
#  endif
#endif

#ifndef FMT_POSIX
#  if defined(_WIN32) && !defined(__MINGW32__)
// Fix warnings about deprecated symbols.
#    define FMT_POSIX(call) _##call
#  else
#    define FMT_POSIX(call) call
#  endif
#endif

// Calls to system functions are wrapped in FMT_SYSTEM for testability.
#ifdef FMT_SYSTEM
#  define FMT_HAS_SYSTEM
#  define FMT_POSIX_CALL(call) FMT_SYSTEM(call)
#else
#  define FMT_SYSTEM(call) ::call
#  ifdef _WIN32
// Fix warnings about deprecated symbols.
#    define FMT_POSIX_CALL(call) ::_##call
#  else
#    define FMT_POSIX_CALL(call) ::call
#  endif
#endif

// Retries the expression while it evaluates to error_result and errno
// equals to EINTR.
#ifndef _WIN32
#  define FMT_RETRY_VAL(result, expression, error_result) \
    do {                                                  \
      (result) = (expression);                            \
    } while ((result) == (error_result) && errno == EINTR)
#else
#  define FMT_RETRY_VAL(result, expression, error_result) result = (expression)
#endif

#define FMT_RETRY(result, expression) FMT_RETRY_VAL(result, expression, -1)

FMT_BEGIN_NAMESPACE
FMT_BEGIN_EXPORT

/**
 * A reference to a null-terminated string. It can be constructed from a C
 * string or `std::string`.
 *
 * You can use one of the following type aliases for common character types:
 *
 * +---------------+-----------------------------+
 * | Type          | Definition                  |
 * +===============+=============================+
 * | cstring_view  | basic_cstring_view<char>    |
 * +---------------+-----------------------------+
 * | wcstring_view | basic_cstring_view<wchar_t> |
 * +---------------+-----------------------------+
 *
 * This class is most useful as a parameter type for functions that wrap C APIs.
 */
template <typename Char> class basic_cstring_view {
 private:
  const Char* data_;

 public:
  /// Constructs a string reference object from a C string.
  basic_cstring_view(const Char* s) : data_(s) {}

  /// Constructs a string reference from an `std::string` object.
  basic_cstring_view(const std::basic_string<Char>& s) : data_(s.c_str()) {}

  /// Returns the pointer to a C string.
  auto c_str() const -> const Char* { return data_; }
};

using cstring_view = basic_cstring_view<char>;
using wcstring_view = basic_cstring_view<wchar_t>;

#ifdef _WIN32
FMT_API const std::error_category& system_category() noexcept;

namespace detail {
FMT_API void format_windows_error(buffer<char>& out, int error_code,
                                  const char* message) noexcept;
}

FMT_API std::system_error vwindows_error(int error_code, string_view fmt,
                                         format_args args);

/**
 * Constructs a `std::system_error` object with the description of the form
 *
 *     <message>: <system-message>
 *
 * where `<message>` is the formatted message and `<system-message>` is the
 * system message corresponding to the error code.
 * `error_code` is a Windows error code as given by `GetLastError`.
 * If `error_code` is not a valid error code such as -1, the system message
 * will look like "error -1".
 *
 * **Example**:
 *
 *     // This throws a system_error with the description
 *     //   cannot open file 'foo': The system cannot find the file specified.
 *     // or similar (system message may vary) if the file doesn't exist.
 *     const char *filename = "foo";
 *     LPOFSTRUCT of = LPOFSTRUCT();
 *     HFILE file = OpenFile(filename, &of, OF_READ);
 *     if (file == HFILE_ERROR) {
 *       throw fmt::windows_error(GetLastError(),
 *                                "cannot open file '{}'", filename);
 *     }
 */
template <typename... T>
auto windows_error(int error_code, string_view message, const T&... args)
    -> std::system_error {
  return vwindows_error(error_code, message, vargs<T...>{{args...}});
}

// Reports a Windows error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_windows_error(int error_code, const char* message) noexcept;
#else
inline auto system_category() noexcept -> const std::error_category& {
  return std::system_category();
}
#endif  // _WIN32

// std::system is not available on some platforms such as iOS (#2248).
#ifdef __OSX__
template <typename S, typename... Args, typename Char = char_t<S>>
void say(const S& fmt, Args&&... args) {
  std::system(format("say \"{}\"", format(fmt, args...)).c_str());
}
#endif

// A buffered file.
class buffered_file {
 private:
  FILE* file_;

  friend class file;

  inline explicit buffered_file(FILE* f) : file_(f) {}

 public:
  buffered_file(const buffered_file&) = delete;
  void operator=(const buffered_file&) = delete;

  // Constructs a buffered_file object which doesn't represent any file.
  inline buffered_file() noexcept : file_(nullptr) {}

  // Destroys the object closing the file it represents if any.
  FMT_API ~buffered_file() noexcept;

 public:
  inline buffered_file(buffered_file&& other) noexcept : file_(other.file_) {
    other.file_ = nullptr;
  }

  inline auto operator=(buffered_file&& other) -> buffered_file& {
    close();
    file_ = other.file_;
    other.file_ = nullptr;
    return *this;
  }

  // Opens a file.
  FMT_API buffered_file(cstring_view filename, cstring_view mode);

  // Closes the file.
  FMT_API void close();

  // Returns the pointer to a FILE object representing this file.
  inline auto get() const noexcept -> FILE* { return file_; }

  FMT_API auto descriptor() const -> int;

  template <typename... T>
  inline void print(string_view fmt, const T&... args) {
    fmt::vargs<T...> vargs = {{args...}};
    detail::is_locking<T...>() ? fmt::vprint_buffered(file_, fmt, vargs)
                               : fmt::vprint(file_, fmt, vargs);
  }
};

#if FMT_USE_FCNTL

// A file. Closed file is represented by a file object with descriptor -1.
// Methods that are not declared with noexcept may throw
// fmt::system_error in case of failure. Note that some errors such as
// closing the file multiple times will cause a crash on Windows rather
// than an exception. You can get standard behavior by overriding the
// invalid parameter handler with _set_invalid_parameter_handler.
class FMT_API file {
 private:
  int fd_;  // File descriptor.

  // Constructs a file object with a given descriptor.
  explicit file(int fd) : fd_(fd) {}

  friend struct pipe;

 public:
  // Possible values for the oflag argument to the constructor.
  enum {
    RDONLY = FMT_POSIX(O_RDONLY),  // Open for reading only.
    WRONLY = FMT_POSIX(O_WRONLY),  // Open for writing only.
    RDWR = FMT_POSIX(O_RDWR),      // Open for reading and writing.
    CREATE = FMT_POSIX(O_CREAT),   // Create if the file doesn't exist.
    APPEND = FMT_POSIX(O_APPEND),  // Open in append mode.
    TRUNC = FMT_POSIX(O_TRUNC)     // Truncate the content of the file.
  };

  // Constructs a file object which doesn't represent any file.
  inline file() noexcept : fd_(-1) {}

  // Opens a file and constructs a file object representing this file.
  file(cstring_view path, int oflag);

 public:
  file(const file&) = delete;
  void operator=(const file&) = delete;

  inline file(file&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  // Move assignment is not noexcept because close may throw.
  inline auto operator=(file&& other) -> file& {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
  }

  // Destroys the object closing the file it represents if any.
  ~file() noexcept;

  // Returns the file descriptor.
  inline auto descriptor() const noexcept -> int { return fd_; }

  // Closes the file.
  void close();

  // Returns the file size. The size has signed type for consistency with
  // stat::st_size.
  auto size() const -> long long;

  // Attempts to read count bytes from the file into the specified buffer.
  auto read(void* buffer, size_t count) -> size_t;

  // Attempts to write count bytes from the specified buffer to the file.
  auto write(const void* buffer, size_t count) -> size_t;

  // Duplicates a file descriptor with the dup function and returns
  // the duplicate as a file object.
  static auto dup(int fd) -> file;

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  void dup2(int fd);

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  void dup2(int fd, std::error_code& ec) noexcept;

  // Creates a buffered_file object associated with this file and detaches
  // this file object from the file.
  auto fdopen(const char* mode) -> buffered_file;

#  if defined(_WIN32) && !defined(__MINGW32__)
  // Opens a file and constructs a file object representing this file by
  // wcstring_view filename. Windows only.
  static file open_windows_file(wcstring_view path, int oflag);
#  endif
};

struct FMT_API pipe {
  file read_end;
  file write_end;

  // Creates a pipe setting up read_end and write_end file objects for reading
  // and writing respectively.
  pipe();
};

// Returns the memory page size.
auto getpagesize() -> long;

namespace detail {

struct buffer_size {
  constexpr buffer_size() = default;
  size_t value = 0;
  FMT_CONSTEXPR auto operator=(size_t val) const -> buffer_size {
    auto bs = buffer_size();
    bs.value = val;
    return bs;
  }
};

struct ostream_params {
  int oflag = file::WRONLY | file::CREATE | file::TRUNC;
  size_t buffer_size = BUFSIZ > 32768 ? BUFSIZ : 32768;

  constexpr ostream_params() {}

  template <typename... T>
  ostream_params(T... params, int new_oflag) : ostream_params(params...) {
    oflag = new_oflag;
  }

  template <typename... T>
  ostream_params(T... params, detail::buffer_size bs)
      : ostream_params(params...) {
    this->buffer_size = bs.value;
  }

// Intel has a bug that results in failure to deduce a constructor
// for empty parameter packs.
#  if defined(__INTEL_COMPILER) && __INTEL_COMPILER < 2000
  ostream_params(int new_oflag) : oflag(new_oflag) {}
  ostream_params(detail::buffer_size bs) : buffer_size(bs.value) {}
#  endif
};

}  // namespace detail

FMT_INLINE_VARIABLE constexpr auto buffer_size = detail::buffer_size();

/// A fast buffered output stream for writing from a single thread. Writing from
/// multiple threads without external synchronization may result in a data race.
class ostream : private detail::buffer<char> {
 private:
  file file_;

  FMT_API ostream(cstring_view path, const detail::ostream_params& params);

  FMT_API static void grow(buffer<char>& buf, size_t);

 public:
  FMT_API ostream(ostream&& other) noexcept;
  FMT_API ~ostream();

  operator writer() {
    detail::buffer<char>& buf = *this;
    return buf;
  }

  inline void flush() {
    if (size() == 0) return;
    file_.write(data(), size() * sizeof(data()[0]));
    clear();
  }

  template <typename... T>
  friend auto output_file(cstring_view path, T... params) -> ostream;

  inline void close() {
    flush();
    file_.close();
  }

  /// Formats `args` according to specifications in `fmt` and writes the
  /// output to the file.
  template <typename... T> void print(format_string<T...> fmt, T&&... args) {
    vformat_to(appender(*this), fmt.str, vargs<T...>{{args...}});
  }
};

/**
 * Opens a file for writing. Supported parameters passed in `params`:
 *
 * - `<integer>`: Flags passed to [open](
 *   https://pubs.opengroup.org/onlinepubs/007904875/functions/open.html)
 *   (`file::WRONLY | file::CREATE | file::TRUNC` by default)
 * - `buffer_size=<integer>`: Output buffer size
 *
 * **Example**:
 *
 *     auto out = fmt::output_file("guide.txt");
 *     out.print("Don't {}", "Panic");
 */
template <typename... T>
inline auto output_file(cstring_view path, T... params) -> ostream {
  return {path, detail::ostream_params(params...)};
}
#endif  // FMT_USE_FCNTL

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_OS_H_
