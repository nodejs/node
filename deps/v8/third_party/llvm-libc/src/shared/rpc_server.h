//===-- Shared memory RPC server instantiation ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_RPC_SERVER_H
#define LLVM_LIBC_SHARED_RPC_SERVER_H

#include "rpc.h"
#include "rpc_opcodes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define flockfile _lock_file
#define funlockfile _unlock_file
#define fwrite_unlocked _fwrite_nolock
#elif defined(__APPLE__)
// MacOS doesn't have an equivalent of fwrite_unlocked so we just use
// fwrite.
#define fwrite_unlocked fwrite
#endif

namespace rpc {
namespace internal {

// Minimal replacement for 'std::vector' that works for trivial types.
template <typename T> class TempVector {
  T *data_ = nullptr;
  size_t current = 0;
  size_t capacity = 0;

public:
  ~TempVector() { ::free(data_); }

  void push_back(const T &value) {
    if (current == capacity)
      grow();
    data_[current++] = value;
  }

  void pop_back() { --current; }

  bool empty() const { return current == 0; }

  size_t size() const { return current; }

  T &operator[](size_t index) { return data_[index]; }

  T &back() { return data_[current - 1]; }

  T *begin() const { return data_; }

  T *end() const { return data_ + current; }

private:
  void grow() {
    size_t new_capacity = capacity ? capacity * 2 : 1;
    void *new_data = ::realloc(data_, new_capacity * sizeof(T));
    data_ = static_cast<T *>(new_data);
    capacity = new_capacity;
  }
};

struct TempStorage {
  char *alloc(size_t size) {
    storage.push_back(reinterpret_cast<char *>(::malloc(size)));
    return storage.back();
  }

  ~TempStorage() {
    for (char *ptr : storage)
      ::free(ptr);
  }

  TempVector<char *> storage;
};

// Counts the bytes consumed from a variadic argument list without reading data.
template <bool packed> struct DummyArgList {
  size_t count = 0;

  template <class T> inline T next_var() {
    count =
        packed ? count + sizeof(T) : align_up(count, alignof(T)) + sizeof(T);
    return T(count);
  }

  size_t read_count() const { return count; }
};

// Reads variadic arguments from a pre-built byte buffer.
template <bool packed> struct StructArgList {
  void *ptr;
  void *end;

  StructArgList() = default;
  inline StructArgList(void *ptr, size_t size)
      : ptr(ptr), end(static_cast<unsigned char *>(ptr) + size) {}

  template <class T> inline T next_var() {
    if (!packed)
      ptr = reinterpret_cast<void *>(
          align_up(reinterpret_cast<uintptr_t>(ptr), alignof(T)));
    if (ptr >= end)
      return T(-1);
    T val;
    ::memcpy(&val, ptr, sizeof(T));
    ptr =
        reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + sizeof(T));
    return val;
  }
};

// Get the associated stream out of an encoded number.
inline FILE *to_stream(uintptr_t f) {
  enum Stream { File = 0, Stdin = 1, Stdout = 2, Stderr = 3 };
  FILE *stream = reinterpret_cast<FILE *>(f & ~0x3ull);
  Stream type = static_cast<Stream>(f & 0x3ull);
  if (type == Stdin)
    return stdin;
  if (type == Stdout)
    return stdout;
  if (type == Stderr)
    return stderr;
  return stream;
}

inline constexpr bool is_format_flag(char c) {
  return c == ' ' || c == '-' || c == '+' || c == '#' || c == '0';
}

inline constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

enum class LengthModifier { none, l };
enum class SizeArgument { finished, width, precision };

struct Specifier {
  uintptr_t raw_value = 0;
  char conv_name = '\0';
  bool is_string = false;
  bool is_finished = false;
  bool is_star = false;
  bool is_long = false;
};

// Minimal printf format string parser. Walks the format and extracts the type
// and size of each variadic argument consumed by a conversion specifier.
template <typename ArgProvider> struct MicroParser {
  inline MicroParser(const char *format, ArgProvider args)
      : format(format), args(args) {}

  inline uint32_t pos() const { return cur_pos; }
  inline uint32_t spec_start() const { return spec_begin; }

  inline Specifier get_next_specifier() {
    Specifier specifier{};

    while (format[cur_pos] != '\0' && format[cur_pos] != '%' &&
           size_pos == SizeArgument::finished)
      ++cur_pos;

    if (format[cur_pos] == '\0') {
      specifier.is_finished = true;
      return specifier;
    }

    if (size_pos == SizeArgument::finished)
      spec_begin = cur_pos;

    cur_pos++;

    if (size_pos == SizeArgument::finished) {
      while (format[cur_pos] != '\0' && is_format_flag(format[cur_pos]))
        ++cur_pos;

      if (format[cur_pos] == '*') {
        specifier.raw_value =
            static_cast<uintptr_t>(args.template next_var<uint32_t>());
        specifier.is_star = true;
        size_pos = SizeArgument::width;
        return specifier;
      }

      while (format[cur_pos] != '\0' && is_digit(format[cur_pos]))
        ++cur_pos;
    }

    if (format[cur_pos] == '.' && size_pos != SizeArgument::precision) {
      ++cur_pos;
      if (format[cur_pos] == '*') {
        specifier.raw_value =
            static_cast<uintptr_t>(args.template next_var<uint32_t>());
        specifier.is_star = true;
        size_pos = SizeArgument::precision;
        return specifier;
      }
      while (format[cur_pos] != '\0' && is_digit(format[cur_pos]))
        ++cur_pos;
    }

    LengthModifier lm = parse_length_modifier();
    specifier.is_long = lm == LengthModifier::l;
    specifier.conv_name = format[cur_pos];

    switch (format[cur_pos]) {
    case 'c':
      specifier.raw_value =
          static_cast<uintptr_t>(args.template next_var<uint32_t>());
      break;
    case 'd':
    case 'b':
    case 'B':
    case 'i':
    case 'o':
    case 'x':
    case 'X':
    case 'u':
      if (lm == LengthModifier::none)
        specifier.raw_value =
            static_cast<uintptr_t>(args.template next_var<uint32_t>());
      else
        specifier.raw_value =
            static_cast<uintptr_t>(args.template next_var<uint64_t>());
      break;
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'a':
    case 'A':
    case 'g':
    case 'G': {
      double d = args.template next_var<double>();
      ::memcpy(&specifier.raw_value, &d, sizeof(double));
      break;
    }
    case 'p':
      specifier.raw_value =
          reinterpret_cast<uintptr_t>(args.template next_var<void *>());
      break;
    case 's':
      specifier.raw_value =
          reinterpret_cast<uintptr_t>(args.template next_var<void *>());
      specifier.is_string = true;
      break;
    case 'n':
      specifier.raw_value =
          reinterpret_cast<uintptr_t>(args.template next_var<void *>());
      break;
    case '%':
      break;
    default:
      if (format[cur_pos] == '\0') {
        specifier.is_finished = true;
        return specifier;
      }
      break;
    }

    cur_pos++;
    size_pos = SizeArgument::finished;
    return specifier;
  }

private:
  inline LengthModifier parse_length_modifier() {
    switch (format[cur_pos]) {
    case 'l':
      if (format[cur_pos + 1] == 'l')
        ++cur_pos;
      [[fallthrough]];
    case 't':
    case 'j':
    case 'z':
      ++cur_pos;
      return LengthModifier::l;
    case 'h':
      if (format[cur_pos + 1] == 'h')
        ++cur_pos;
      ++cur_pos;
      return LengthModifier::none;
    case 'q':
    case 'L':
      ++cur_pos;
      return LengthModifier::l;
    default:
      return LengthModifier::none;
    }
  }

  const char *const format;
  ArgProvider args;
  uint32_t cur_pos = 0;
  uint32_t spec_begin = 0;
  SizeArgument size_pos = SizeArgument::finished;
};

// The format strings were already checked and warned on the device side.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"

// Dispatch helper that passes dynamic '*' width/precision values to fprintf.
template <typename T>
[[gnu::format(printf, 2, 0)]] inline int
fprintf_with_stars(FILE *file, const char *fmt, int num_stars, int *star_vals,
                   T val) {
  if (num_stars == 2)
    return ::fprintf(file, fmt, star_vals[0], star_vals[1], val);
  if (num_stars == 1)
    return ::fprintf(file, fmt, star_vals[0], val);
  return ::fprintf(file, fmt, val);
}

// Walks a printf format string using the MicroParser and emits output in
// chunks via fprintf. Literal text is written directly with fwrite_unlocked.
// The caller must hold the stream lock via flockfile.
template <bool packed>
inline int print_format(FILE *file, const char *fmt, StructArgList<packed> args,
                        TempVector<void *> &copied_strs) {
  MicroParser<StructArgList<packed>> parser(fmt, args);
  int ret = 0;
  size_t prev = 0;
  int star_vals[2];
  int num_stars = 0;

  // Accumulate any errors so we correctly return on failure.
  auto accum = [&](int rc) {
    if (ret >= 0)
      ret = (rc < 0) ? rc : ret + rc;
  };
  auto write = [&](const char *s, size_t n) -> int {
    size_t w = ::fwrite_unlocked(s, 1, n, file);
    return w == n ? static_cast<int>(w) : -1;
  };

  for (Specifier spec = parser.get_next_specifier(); !spec.is_finished;
       spec = parser.get_next_specifier()) {
    if (spec.is_star) {
      if (num_stars < 2)
        star_vals[num_stars++] = static_cast<int>(spec.raw_value);
      continue;
    }

    size_t start = parser.spec_start();
    size_t end = parser.pos();

    if (start > prev)
      accum(write(fmt + prev, start - prev));

    // Null-terminated copy of the specifier substring for fprintf. Use a
    // stack buffer for the common case; heap-allocate only for overlong specs.
    size_t len = end - start;
    char local_buf[32];
    char *buf = len < sizeof(local_buf) ? local_buf : new char[len + 1];
    ::memcpy(buf, fmt + start, len);
    buf[len] = '\0';

    switch (spec.conv_name) {
    case 's': {
      const char *str = reinterpret_cast<const char *>(spec.raw_value);
      if (str) {
        str = reinterpret_cast<const char *>(copied_strs.back());
        copied_strs.pop_back();
      }
      accum(fprintf_with_stars(file, buf, num_stars, star_vals, str));
      break;
    }
    case 'n':
      break;
    case 'p':
      accum(fprintf_with_stars(file, buf, num_stars, star_vals,
                               reinterpret_cast<void *>(spec.raw_value)));
      break;
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'a':
    case 'A':
    case 'g':
    case 'G': {
      double d;
      ::memcpy(&d, &spec.raw_value, sizeof(double));
      if (spec.is_long)
        accum(fprintf_with_stars(file, buf, num_stars, star_vals,
                                 static_cast<long double>(d)));
      else
        accum(fprintf_with_stars(file, buf, num_stars, star_vals, d));
      break;
    }
    default:
      if (spec.is_long)
        accum(fprintf_with_stars(file, buf, num_stars, star_vals,
                                 spec.raw_value));
      else
        accum(fprintf_with_stars(file, buf, num_stars, star_vals,
                                 static_cast<uint32_t>(spec.raw_value)));
      break;
    }
    if (buf != local_buf)
      delete[] buf;
    num_stars = 0;
    prev = end;
  }

  if (parser.pos() > prev)
    accum(write(fmt + prev, parser.pos() - prev));

  return ret;
}

#pragma GCC diagnostic pop

template <bool packed, uint32_t num_lanes>
inline void handle_printf(Server::Port &port, TempStorage &temp_storage) {
  FILE *files[num_lanes] = {nullptr};
  // Get the appropriate output stream to use.
  if (port.get_opcode() == LIBC_PRINTF_TO_STREAM ||
      port.get_opcode() == LIBC_PRINTF_TO_STREAM_PACKED) {
    port.recv([&](Buffer *buffer, uint32_t id) {
      files[id] = reinterpret_cast<FILE *>(buffer->data[0]);
    });
  } else if (port.get_opcode() == LIBC_PRINTF_TO_STDOUT ||
             port.get_opcode() == LIBC_PRINTF_TO_STDOUT_PACKED) {
    for (uint32_t i = 0; i < num_lanes; ++i)
      files[i] = stdout;
  } else {
    for (uint32_t i = 0; i < num_lanes; ++i)
      files[i] = stderr;
  }

  uint64_t format_sizes[num_lanes] = {0};
  void *format[num_lanes] = {nullptr};

  uint64_t args_sizes[num_lanes] = {0};
  void *args[num_lanes] = {nullptr};

  // Receive the format string from the client.
  port.recv_n(format, format_sizes,
              [&](uint64_t size) { return temp_storage.alloc(size); });

  // Parse the format string to determine the expected argument buffer size.
  for (uint32_t lane = 0; lane < num_lanes; ++lane) {
    if (!format[lane])
      continue;

    DummyArgList<packed> dummy_args;
    MicroParser<DummyArgList<packed> &> parser(
        reinterpret_cast<const char *>(format[lane]), dummy_args);
    for (Specifier spec = parser.get_next_specifier(); !spec.is_finished;
         spec = parser.get_next_specifier())
      ;
    args_sizes[lane] = dummy_args.read_count();
  }
  port.send(
      [&](Buffer *buffer, uint32_t id) { buffer->data[0] = args_sizes[id]; });
  port.recv_n(args, args_sizes,
              [&](uint64_t size) { return temp_storage.alloc(size); });

  // Identify any arguments that are actually pointers to strings on the client.
  TempVector<void *> strs_to_copy[num_lanes];
  for (uint32_t lane = 0; lane < num_lanes; ++lane) {
    if (!format[lane])
      continue;

    StructArgList<packed> struct_args(args[lane], args_sizes[lane]);
    MicroParser<StructArgList<packed>> parser(
        reinterpret_cast<const char *>(format[lane]), struct_args);
    for (Specifier spec = parser.get_next_specifier(); !spec.is_finished;
         spec = parser.get_next_specifier()) {
      if (spec.is_string && spec.raw_value)
        strs_to_copy[lane].push_back(reinterpret_cast<void *>(spec.raw_value));
    }
  }

  // Receive any strings from the client and push them into a buffer.
  TempVector<void *> copied_strs[num_lanes];
  auto has_pending = [](TempVector<void *> v[num_lanes]) {
    for (uint32_t i = 0; i < num_lanes; ++i)
      if (!v[i].empty() && v[i].back())
        return true;
    return false;
  };
  while (has_pending(strs_to_copy)) {
    port.send([&](Buffer *buffer, uint32_t id) {
      void *ptr = !strs_to_copy[id].empty() ? strs_to_copy[id].back() : nullptr;
      buffer->data[1] = reinterpret_cast<uintptr_t>(ptr);
      if (!strs_to_copy[id].empty())
        strs_to_copy[id].pop_back();
    });
    uint64_t str_sizes[num_lanes] = {0};
    void *strs[num_lanes] = {nullptr};
    port.recv_n(strs, str_sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    for (uint32_t lane = 0; lane < num_lanes; ++lane) {
      if (!strs[lane])
        continue;
      copied_strs[lane].push_back(strs[lane]);
    }
  }

  // Print using a locked stream, emitting each format chunk via fprintf.
  int results[num_lanes] = {0};
  for (uint32_t lane = 0; lane < num_lanes; ++lane) {
    if (!format[lane])
      continue;

    StructArgList<packed> printf_args(args[lane], args_sizes[lane]);
    ::flockfile(files[lane]);
    results[lane] = print_format<packed>(
        files[lane], reinterpret_cast<const char *>(format[lane]), printf_args,
        copied_strs[lane]);
    ::funlockfile(files[lane]);
  }

  // Send the final return value and signal completion by setting the string
  // argument to null.
  port.send([&](Buffer *buffer, uint32_t id) {
    buffer->data[0] = static_cast<uint64_t>(results[id]);
    buffer->data[1] = reinterpret_cast<uintptr_t>(nullptr);
  });
}

template <uint32_t num_lanes>
inline RPCStatus handle_port_impl(Server::Port &port) {
  TempStorage temp_storage;

  switch (port.get_opcode()) {
  case LIBC_WRITE_TO_STREAM:
  case LIBC_WRITE_TO_STDERR:
  case LIBC_WRITE_TO_STDOUT:
  case LIBC_WRITE_TO_STDOUT_NEWLINE: {
    uint64_t sizes[num_lanes] = {0};
    void *strs[num_lanes] = {nullptr};
    FILE *files[num_lanes] = {nullptr};
    if (port.get_opcode() == LIBC_WRITE_TO_STREAM) {
      port.recv([&](Buffer *buffer, uint32_t id) {
        files[id] = reinterpret_cast<FILE *>(buffer->data[0]);
      });
    } else {
      for (uint32_t i = 0; i < num_lanes; ++i)
        files[i] = port.get_opcode() == LIBC_WRITE_TO_STDERR ? stderr : stdout;
    }

    port.recv_n(strs, sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.send([&](Buffer *buffer, uint32_t id) {
      ::flockfile(files[id]);
      buffer->data[0] = ::fwrite_unlocked(strs[id], 1, sizes[id], files[id]);
      if (port.get_opcode() == LIBC_WRITE_TO_STDOUT_NEWLINE &&
          buffer->data[0] == sizes[id])
        buffer->data[0] += ::fwrite_unlocked("\n", 1, 1, files[id]);
      ::funlockfile(files[id]);
    });
    break;
  }
  case LIBC_READ_FROM_STREAM: {
    uint64_t sizes[num_lanes] = {0};
    void *data[num_lanes] = {nullptr};
    port.recv([&](Buffer *buffer, uint32_t id) {
      data[id] = temp_storage.alloc(buffer->data[0]);
      sizes[id] =
          ::fread(data[id], 1, buffer->data[0], to_stream(buffer->data[1]));
    });
    port.send_n(data, sizes);
    port.send([&](Buffer *buffer, uint32_t id) {
      ::memcpy(buffer->data, &sizes[id], sizeof(uint64_t));
    });
    break;
  }
  case LIBC_READ_FGETS: {
    uint64_t sizes[num_lanes] = {0};
    void *data[num_lanes] = {nullptr};
    port.recv([&](Buffer *buffer, uint32_t id) {
      data[id] = temp_storage.alloc(buffer->data[0]);
      const char *str = ::fgets(reinterpret_cast<char *>(data[id]),
                                static_cast<int>(buffer->data[0]),
                                to_stream(buffer->data[1]));
      sizes[id] = !str ? 0 : __builtin_strlen(str) + 1;
    });
    port.send_n(data, sizes);
    break;
  }
  case LIBC_OPEN_FILE: {
    uint64_t sizes[num_lanes] = {0};
    void *paths[num_lanes] = {nullptr};
    port.recv_n(paths, sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.recv_and_send([&](Buffer *buffer, uint32_t id) {
      FILE *file = ::fopen(reinterpret_cast<char *>(paths[id]),
                           reinterpret_cast<char *>(buffer->data));
      buffer->data[0] = reinterpret_cast<uintptr_t>(file);
    });
    break;
  }
  case LIBC_CLOSE_FILE: {
    port.recv_and_send([&](Buffer *buffer, uint32_t) {
      FILE *file = reinterpret_cast<FILE *>(buffer->data[0]);
      buffer->data[0] = static_cast<uint64_t>(::fclose(file));
    });
    break;
  }
  case LIBC_EXIT: {
    port.recv_and_send([](Buffer *, uint32_t) {});
    port.recv([](Buffer *buffer, uint32_t) {
      int status = 0;
      ::memcpy(&status, buffer->data, sizeof(int));
      ::quick_exit(status);
    });
    break;
  }
  case LIBC_ABORT: {
    port.recv_and_send([](Buffer *, uint32_t) {});
    port.recv([](Buffer *, uint32_t) {});
    ::abort();
    break;
  }
  case LIBC_HOST_CALL: {
    uint64_t sizes[num_lanes] = {0};
    unsigned long long results[num_lanes] = {0};
    void *args[num_lanes] = {nullptr};
    port.recv_n(args, sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.recv([&](Buffer *buffer, uint32_t id) {
      using func_ptr_t = unsigned long long (*)(void *);
      auto func = reinterpret_cast<func_ptr_t>(buffer->data[0]);
      results[id] = func(args[id]);
    });
    port.send([&](Buffer *buffer, uint32_t id) {
      buffer->data[0] = static_cast<uint64_t>(results[id]);
    });
    break;
  }
  case LIBC_FEOF: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] =
          static_cast<uint64_t>(::feof(to_stream(buffer->data[0])));
    });
    break;
  }
  case LIBC_FERROR: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] =
          static_cast<uint64_t>(::ferror(to_stream(buffer->data[0])));
    });
    break;
  }
  case LIBC_CLEARERR: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      ::clearerr(to_stream(buffer->data[0]));
    });
    break;
  }
  case LIBC_FSEEK: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] = static_cast<uint64_t>(::fseek(
          to_stream(buffer->data[0]), static_cast<long>(buffer->data[1]),
          static_cast<int>(buffer->data[2])));
    });
    break;
  }
  case LIBC_FTELL: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] =
          static_cast<uint64_t>(::ftell(to_stream(buffer->data[0])));
    });
    break;
  }
  case LIBC_FFLUSH: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] =
          static_cast<uint64_t>(::fflush(to_stream(buffer->data[0])));
    });
    break;
  }
  case LIBC_UNGETC: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      buffer->data[0] = static_cast<uint64_t>(::ungetc(
          static_cast<int>(buffer->data[0]), to_stream(buffer->data[1])));
    });
    break;
  }
  case LIBC_PRINTF_TO_STREAM_PACKED:
  case LIBC_PRINTF_TO_STDOUT_PACKED:
  case LIBC_PRINTF_TO_STDERR_PACKED: {
    handle_printf<true, num_lanes>(port, temp_storage);
    break;
  }
  case LIBC_PRINTF_TO_STREAM:
  case LIBC_PRINTF_TO_STDOUT:
  case LIBC_PRINTF_TO_STDERR: {
    handle_printf<false, num_lanes>(port, temp_storage);
    break;
  }
  case LIBC_REMOVE: {
    uint64_t sizes[num_lanes] = {0};
    void *args[num_lanes] = {nullptr};
    port.recv_n(args, sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.send([&](Buffer *buffer, uint32_t id) {
      buffer->data[0] = static_cast<uint64_t>(
          ::remove(reinterpret_cast<const char *>(args[id])));
    });
    break;
  }
  case LIBC_RENAME: {
    uint64_t oldsizes[num_lanes] = {0};
    uint64_t newsizes[num_lanes] = {0};
    void *oldpath[num_lanes] = {nullptr};
    void *newpath[num_lanes] = {nullptr};
    port.recv_n(oldpath, oldsizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.recv_n(newpath, newsizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.send([&](Buffer *buffer, uint32_t id) {
      buffer->data[0] = static_cast<uint64_t>(
          ::rename(reinterpret_cast<const char *>(oldpath[id]),
                   reinterpret_cast<const char *>(newpath[id])));
    });
    break;
  }
  case LIBC_SYSTEM: {
    uint64_t sizes[num_lanes] = {0};
    void *args[num_lanes] = {nullptr};
    port.recv_n(args, sizes,
                [&](uint64_t size) { return temp_storage.alloc(size); });
    port.send([&](Buffer *buffer, uint32_t id) {
      buffer->data[0] = static_cast<uint64_t>(
          ::system(reinterpret_cast<const char *>(args[id])));
    });
    break;
  }
  case LIBC_TEST_INCREMENT: {
    port.recv_and_send([](Buffer *buffer, uint32_t) {
      reinterpret_cast<uint64_t *>(buffer->data)[0] += 1;
    });
    break;
  }
  case LIBC_TEST_INTERFACE: {
    bool end_with_recv;
    uint64_t cnt;
    port.recv(
        [&](Buffer *buffer, uint32_t) { end_with_recv = buffer->data[0]; });
    port.recv([&](Buffer *buffer, uint32_t) { cnt = buffer->data[0]; });
    port.send(
        [&](Buffer *buffer, uint32_t) { buffer->data[0] = cnt = cnt + 1; });
    port.recv([&](Buffer *buffer, uint32_t) { cnt = buffer->data[0]; });
    port.send(
        [&](Buffer *buffer, uint32_t) { buffer->data[0] = cnt = cnt + 1; });
    port.recv([&](Buffer *buffer, uint32_t) { cnt = buffer->data[0]; });
    port.recv([&](Buffer *buffer, uint32_t) { cnt = buffer->data[0]; });
    port.send(
        [&](Buffer *buffer, uint32_t) { buffer->data[0] = cnt = cnt + 1; });
    port.send(
        [&](Buffer *buffer, uint32_t) { buffer->data[0] = cnt = cnt + 1; });
    if (end_with_recv)
      port.recv([&](Buffer *buffer, uint32_t) { cnt = buffer->data[0]; });
    else
      port.send(
          [&](Buffer *buffer, uint32_t) { buffer->data[0] = cnt = cnt + 1; });

    break;
  }
  case LIBC_TEST_STREAM: {
    uint64_t sizes[num_lanes] = {0};
    void *dst[num_lanes] = {nullptr};
    port.recv_n(dst, sizes,
                [](uint64_t size) -> void * { return new char[size]; });
    port.send_n(dst, sizes);
    for (uint64_t i = 0; i < num_lanes; ++i) {
      if (dst[i])
        delete[] reinterpret_cast<uint8_t *>(dst[i]);
    }
    break;
  }
  case LIBC_NOOP: {
    port.recv([](Buffer *, uint32_t) {});
    break;
  }
  default:
    return RPC_UNHANDLED_OPCODE;
  }

  return RPC_SUCCESS;
}
} // namespace internal

// Handles any opcode generated from the 'libc' client code.
inline RPCStatus handle_libc_opcodes(Server::Port &port, uint32_t num_lanes) {
  switch (num_lanes) {
  case 1:
    return internal::handle_port_impl<1>(port);
  case 32:
    return internal::handle_port_impl<32>(port);
  case 64:
    return internal::handle_port_impl<64>(port);
  default:
    return RPC_ERROR;
  }
}

} // namespace rpc

#ifdef _WIN32
#undef flockfile
#undef funlockfile
#undef fwrite_unlocked
#endif

#endif // LLVM_LIBC_SHARED_RPC_SERVER_H
