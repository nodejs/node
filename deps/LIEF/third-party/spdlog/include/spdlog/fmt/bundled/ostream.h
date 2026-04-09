// Formatting library for C++ - std::ostream support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_OSTREAM_H_
#define FMT_OSTREAM_H_

#ifndef FMT_MODULE
#  include <fstream>  // std::filebuf
#endif

#ifdef _WIN32
#  ifdef __GLIBCXX__
#    include <ext/stdio_filebuf.h>
#    include <ext/stdio_sync_filebuf.h>
#  endif
#  include <io.h>
#endif

#include "chrono.h"  // formatbuf

#ifdef _MSVC_STL_UPDATE
#  define FMT_MSVC_STL_UPDATE _MSVC_STL_UPDATE
#elif defined(_MSC_VER) && _MSC_VER < 1912  // VS 15.5
#  define FMT_MSVC_STL_UPDATE _MSVC_LANG
#else
#  define FMT_MSVC_STL_UPDATE 0
#endif

FMT_BEGIN_NAMESPACE
namespace detail {

// Generate a unique explicit instantion in every translation unit using a tag
// type in an anonymous namespace.
namespace {
struct file_access_tag {};
}  // namespace
template <typename Tag, typename BufType, FILE* BufType::*FileMemberPtr>
class file_access {
  friend auto get_file(BufType& obj) -> FILE* { return obj.*FileMemberPtr; }
};

#if FMT_MSVC_STL_UPDATE
template class file_access<file_access_tag, std::filebuf,
                           &std::filebuf::_Myfile>;
auto get_file(std::filebuf&) -> FILE*;
#endif

// Write the content of buf to os.
// It is a separate function rather than a part of vprint to simplify testing.
template <typename Char>
void write_buffer(std::basic_ostream<Char>& os, buffer<Char>& buf) {
  const Char* buf_data = buf.data();
  using unsigned_streamsize = make_unsigned_t<std::streamsize>;
  unsigned_streamsize size = buf.size();
  unsigned_streamsize max_size = to_unsigned(max_value<std::streamsize>());
  do {
    unsigned_streamsize n = size <= max_size ? size : max_size;
    os.write(buf_data, static_cast<std::streamsize>(n));
    buf_data += n;
    size -= n;
  } while (size != 0);
}

template <typename T> struct streamed_view {
  const T& value;
};
}  // namespace detail

// Formats an object of type T that has an overloaded ostream operator<<.
template <typename Char>
struct basic_ostream_formatter : formatter<basic_string_view<Char>, Char> {
  void set_debug_format() = delete;

  template <typename T, typename Context>
  auto format(const T& value, Context& ctx) const -> decltype(ctx.out()) {
    auto buffer = basic_memory_buffer<Char>();
    auto&& formatbuf = detail::formatbuf<std::basic_streambuf<Char>>(buffer);
    auto&& output = std::basic_ostream<Char>(&formatbuf);
    output.imbue(std::locale::classic());  // The default is always unlocalized.
    output << value;
    output.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    return formatter<basic_string_view<Char>, Char>::format(
        {buffer.data(), buffer.size()}, ctx);
  }
};

using ostream_formatter = basic_ostream_formatter<char>;

template <typename T, typename Char>
struct formatter<detail::streamed_view<T>, Char>
    : basic_ostream_formatter<Char> {
  template <typename Context>
  auto format(detail::streamed_view<T> view, Context& ctx) const
      -> decltype(ctx.out()) {
    return basic_ostream_formatter<Char>::format(view.value, ctx);
  }
};

/**
 * Returns a view that formats `value` via an ostream `operator<<`.
 *
 * **Example**:
 *
 *     fmt::print("Current thread id: {}\n",
 *                fmt::streamed(std::this_thread::get_id()));
 */
template <typename T>
constexpr auto streamed(const T& value) -> detail::streamed_view<T> {
  return {value};
}

inline void vprint(std::ostream& os, string_view fmt, format_args args) {
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt, args);
  FILE* f = nullptr;
#if FMT_MSVC_STL_UPDATE && FMT_USE_RTTI
  if (auto* buf = dynamic_cast<std::filebuf*>(os.rdbuf()))
    f = detail::get_file(*buf);
#elif defined(_WIN32) && defined(__GLIBCXX__) && FMT_USE_RTTI
  auto* rdbuf = os.rdbuf();
  if (auto* sfbuf = dynamic_cast<__gnu_cxx::stdio_sync_filebuf<char>*>(rdbuf))
    f = sfbuf->file();
  else if (auto* fbuf = dynamic_cast<__gnu_cxx::stdio_filebuf<char>*>(rdbuf))
    f = fbuf->file();
#endif
#ifdef _WIN32
  if (f) {
    int fd = _fileno(f);
    if (_isatty(fd)) {
      os.flush();
      if (detail::write_console(fd, {buffer.data(), buffer.size()})) return;
    }
  }
#endif
  detail::ignore_unused(f);
  detail::write_buffer(os, buffer);
}

/**
 * Prints formatted data to the stream `os`.
 *
 * **Example**:
 *
 *     fmt::print(cerr, "Don't {}!", "panic");
 */
FMT_EXPORT template <typename... T>
void print(std::ostream& os, format_string<T...> fmt, T&&... args) {
  fmt::vargs<T...> vargs = {{args...}};
  if (detail::const_check(detail::use_utf8)) return vprint(os, fmt.str, vargs);
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt.str, vargs);
  detail::write_buffer(os, buffer);
}

FMT_EXPORT template <typename... T>
void println(std::ostream& os, format_string<T...> fmt, T&&... args) {
  fmt::print(os, FMT_STRING("{}\n"),
             fmt::format(fmt, std::forward<T>(args)...));
}

FMT_END_NAMESPACE

#endif  // FMT_OSTREAM_H_
