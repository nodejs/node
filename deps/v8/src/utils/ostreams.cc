// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/ostreams.h"

#include <cinttypes>

#include "src/base/lazy-instance.h"
#include "src/objects/objects.h"
#include "src/objects/string.h"

#if V8_OS_WIN
#include <windows.h>
#if _MSC_VER < 1900
#define snprintf sprintf_s
#endif
#endif

#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define LOG_TAG "v8"
#include <android/log.h>  // NOLINT
#endif

namespace v8 {
namespace internal {

DbgStreamBuf::DbgStreamBuf() { setp(data_, data_ + sizeof(data_)); }

DbgStreamBuf::~DbgStreamBuf() { sync(); }

int DbgStreamBuf::overflow(int c) {
#if V8_OS_WIN
  if (!IsDebuggerPresent()) {
    return 0;
  }

  sync();

  if (c != EOF) {
    if (pbase() == epptr()) {
      auto as_char = static_cast<char>(c);
      OutputDebugStringA(&as_char);
    } else {
      sputc(static_cast<char>(c));
    }
  }
#endif
  return 0;
}

int DbgStreamBuf::sync() {
#if V8_OS_WIN
  if (!IsDebuggerPresent()) {
    return 0;
  }

  if (pbase() != pptr()) {
    OutputDebugStringA(std::string(pbase(), static_cast<std::string::size_type>(
                                                pptr() - pbase()))
                           .c_str());
    setp(pbase(), epptr());
  }
#endif
  return 0;
}

DbgStdoutStream::DbgStdoutStream() : std::ostream(&streambuf_) {}

OFStreamBase::OFStreamBase(FILE* f) : f_(f) {}

int OFStreamBase::sync() {
  std::fflush(f_);
  return 0;
}

OFStreamBase::int_type OFStreamBase::overflow(int_type c) {
  return (c != EOF) ? std::fputc(c, f_) : c;
}

std::streamsize OFStreamBase::xsputn(const char* s, std::streamsize n) {
  return static_cast<std::streamsize>(
      std::fwrite(s, 1, static_cast<size_t>(n), f_));
}

OFStream::OFStream(FILE* f) : std::ostream(nullptr), buf_(f) {
  DCHECK_NOT_NULL(f);
  rdbuf(&buf_);
}

#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
AndroidLogStream::~AndroidLogStream() {
  // If there is anything left in the line buffer, print it now, even though it
  // was not terminated by a newline.
  if (!line_buffer_.empty()) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, line_buffer_.c_str());
  }
}

std::streamsize AndroidLogStream::xsputn(const char* s, std::streamsize n) {
  const char* const e = s + n;
  while (s < e) {
    const char* newline = reinterpret_cast<const char*>(memchr(s, '\n', e - s));
    size_t line_chars = (newline ? newline : e) - s;
    line_buffer_.append(s, line_chars);
    // Without terminating newline, keep the characters in the buffer for the
    // next invocation.
    if (!newline) break;
    // Otherwise, write out the first line, then continue.
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, line_buffer_.c_str());
    line_buffer_.clear();
    s = newline + 1;
  }
  return n;
}
#endif

DEFINE_LAZY_LEAKY_OBJECT_GETTER(base::RecursiveMutex,
                                StdoutStream::GetStdoutMutex)

namespace {

// Locale-independent predicates.
bool IsPrint(uint16_t c) { return 0x20 <= c && c <= 0x7E; }
bool IsSpace(uint16_t c) { return (0x9 <= c && c <= 0xD) || c == 0x20; }
bool IsOK(uint16_t c) { return (IsPrint(c) || IsSpace(c)) && c != '\\'; }

std::ostream& PrintUC16(std::ostream& os, uint16_t c, bool (*pred)(uint16_t)) {
  char buf[10];
  const char* format = pred(c) ? "%c" : (c <= 0xFF) ? "\\x%02x" : "\\u%04x";
  snprintf(buf, sizeof(buf), format, c);
  return os << buf;
}

std::ostream& PrintUC16ForJSON(std::ostream& os, uint16_t c,
                               bool (*pred)(uint16_t)) {
  // JSON does not allow \x99; must use \u0099.
  char buf[10];
  const char* format = pred(c) ? "%c" : "\\u%04x";
  snprintf(buf, sizeof(buf), format, c);
  return os << buf;
}

std::ostream& PrintUC32(std::ostream& os, int32_t c, bool (*pred)(uint16_t)) {
  if (c <= String::kMaxUtf16CodeUnit) {
    return PrintUC16(os, static_cast<uint16_t>(c), pred);
  }
  char buf[13];
  snprintf(buf, sizeof(buf), "\\u{%06x}", c);
  return os << buf;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const AsReversiblyEscapedUC16& c) {
  return PrintUC16(os, c.value, IsOK);
}

std::ostream& operator<<(std::ostream& os, const AsEscapedUC16ForJSON& c) {
  if (c.value == '\n') return os << "\\n";
  if (c.value == '\r') return os << "\\r";
  if (c.value == '\t') return os << "\\t";
  if (c.value == '\"') return os << "\\\"";
  return PrintUC16ForJSON(os, c.value, IsOK);
}

std::ostream& operator<<(std::ostream& os, const AsUC16& c) {
  return PrintUC16(os, c.value, IsPrint);
}

std::ostream& operator<<(std::ostream& os, const AsUC32& c) {
  return PrintUC32(os, c.value, IsPrint);
}

std::ostream& operator<<(std::ostream& os, const AsHex& hex) {
  // Each byte uses up to two characters. Plus two characters for the prefix,
  // plus null terminator.
  DCHECK_GE(sizeof(hex.value) * 2, hex.min_width);
  static constexpr size_t kMaxHexLength = 3 + sizeof(hex.value) * 2;
  char buf[kMaxHexLength];
  snprintf(buf, kMaxHexLength, "%s%.*" PRIx64, hex.with_prefix ? "0x" : "",
           hex.min_width, hex.value);
  return os << buf;
}

std::ostream& operator<<(std::ostream& os, const AsHexBytes& hex) {
  uint8_t bytes = hex.min_bytes;
  while (bytes < sizeof(hex.value) && (hex.value >> (bytes * 8) != 0)) ++bytes;
  for (uint8_t b = 0; b < bytes; ++b) {
    if (b) os << " ";
    uint8_t printed_byte =
        hex.byte_order == AsHexBytes::kLittleEndian ? b : bytes - b - 1;
    os << AsHex((hex.value >> (8 * printed_byte)) & 0xFF, 2);
  }
  return os;
}

}  // namespace internal
}  // namespace v8

#undef snprintf
#undef LOG_TAG
