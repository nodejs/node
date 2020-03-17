// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "util.h"  // NOLINT(build/include_inline)
#include "util-inl.h"

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_internals.h"
#include "string_bytes.h"
#include "uv.h"

#ifdef _WIN32
#include <io.h>  // _S_IREAD _S_IWRITE
#include <time.h>
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif  // S_IRUSR
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif  // S_IWUSR
#else
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

static std::atomic_int seq = {0};  // Sequence number for diagnostic filenames.

namespace node {

using v8::ArrayBufferView;
using v8::Isolate;
using v8::Local;
using v8::String;
using v8::Value;

template <typename T>
static void MakeUtf8String(Isolate* isolate,
                           Local<Value> value,
                           MaybeStackBuffer<T>* target) {
  Local<String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) return;

  size_t storage;
  if (!StringBytes::StorageSize(isolate, string, UTF8).To(&storage)) return;
  storage += 1;
  target->AllocateSufficientStorage(storage);
  const int flags =
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  const int length =
      string->WriteUtf8(isolate, target->out(), storage, nullptr, flags);
  target->SetLengthAndZeroTerminate(length);
}

Utf8Value::Utf8Value(Isolate* isolate, Local<Value> value) {
  if (value.IsEmpty())
    return;

  MakeUtf8String(isolate, value, this);
}


TwoByteValue::TwoByteValue(Isolate* isolate, Local<Value> value) {
  if (value.IsEmpty()) {
    return;
  }

  Local<String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) return;

  // Allocate enough space to include the null terminator
  const size_t storage = string->Length() + 1;
  AllocateSufficientStorage(storage);

  const int flags = String::NO_NULL_TERMINATION;
  const int length = string->Write(isolate, out(), 0, storage, flags);
  SetLengthAndZeroTerminate(length);
}

BufferValue::BufferValue(Isolate* isolate, Local<Value> value) {
  // Slightly different take on Utf8Value. If value is a String,
  // it will return a Utf8 encoded string. If value is a Buffer,
  // it will copy the data out of the Buffer as is.
  if (value.IsEmpty()) {
    // Dereferencing this object will return nullptr.
    Invalidate();
    return;
  }

  if (value->IsString()) {
    MakeUtf8String(isolate, value, this);
  } else if (value->IsArrayBufferView()) {
    const size_t len = value.As<ArrayBufferView>()->ByteLength();
    // Leave place for the terminating '\0' byte.
    AllocateSufficientStorage(len + 1);
    value.As<ArrayBufferView>()->CopyContents(out(), len);
    SetLengthAndZeroTerminate(len);
  } else {
    Invalidate();
  }
}

void LowMemoryNotification() {
  if (per_process::v8_initialized) {
    auto isolate = Isolate::GetCurrent();
    if (isolate != nullptr) {
      isolate->LowMemoryNotification();
    }
  }
}

std::string GetProcessTitle(const char* default_title) {
  std::string buf(16, '\0');

  for (;;) {
    const int rc = uv_get_process_title(&buf[0], buf.size());

    if (rc == 0)
      break;

    if (rc != UV_ENOBUFS)
      return default_title;

    buf.resize(2 * buf.size());
  }

  // Strip excess trailing nul bytes. Using strlen() here is safe,
  // uv_get_process_title() always zero-terminates the result.
  buf.resize(strlen(&buf[0]));

  return buf;
}

std::string GetHumanReadableProcessName() {
  return SPrintF("%s[%d]", GetProcessTitle("Node.js"), uv_os_getpid());
}

std::vector<std::string> SplitString(const std::string& in, char delim) {
  std::vector<std::string> out;
  if (in.empty())
    return out;
  std::istringstream in_stream(in);
  while (in_stream.good()) {
    std::string item;
    std::getline(in_stream, item, delim);
    if (item.empty()) continue;
    out.emplace_back(std::move(item));
  }
  return out;
}

void ThrowErrStringTooLong(Isolate* isolate) {
  isolate->ThrowException(ERR_STRING_TOO_LONG(isolate));
}

double GetCurrentTimeInMicroseconds() {
  constexpr double kMicrosecondsPerSecond = 1e6;
  uv_timeval64_t tv;
  CHECK_EQ(0, uv_gettimeofday(&tv));
  return kMicrosecondsPerSecond * tv.tv_sec + tv.tv_usec;
}

int WriteFileSync(const char* path, uv_buf_t buf) {
  uv_fs_t req;
  int fd = uv_fs_open(nullptr,
                      &req,
                      path,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      S_IWUSR | S_IRUSR,
                      nullptr);
  uv_fs_req_cleanup(&req);
  if (fd < 0) {
    return fd;
  }

  int err = uv_fs_write(nullptr, &req, fd, &buf, 1, 0, nullptr);
  uv_fs_req_cleanup(&req);
  if (err < 0) {
    return err;
  }

  err = uv_fs_close(nullptr, &req, fd, nullptr);
  uv_fs_req_cleanup(&req);
  return err;
}

int WriteFileSync(v8::Isolate* isolate,
                  const char* path,
                  v8::Local<v8::String> string) {
  node::Utf8Value utf8(isolate, string);
  uv_buf_t buf = uv_buf_init(utf8.out(), utf8.length());
  return WriteFileSync(path, buf);
}

void DiagnosticFilename::LocalTime(TIME_TYPE* tm_struct) {
#ifdef _WIN32
  GetLocalTime(tm_struct);
#else  // UNIX, OSX
  struct timeval time_val;
  gettimeofday(&time_val, nullptr);
  localtime_r(&time_val.tv_sec, tm_struct);
#endif
}

// Defined in node_internals.h
std::string DiagnosticFilename::MakeFilename(
    uint64_t thread_id,
    const char* prefix,
    const char* ext) {
  std::ostringstream oss;
  TIME_TYPE tm_struct;
  LocalTime(&tm_struct);
  oss << prefix;
#ifdef _WIN32
  oss << "." << std::setfill('0') << std::setw(4) << tm_struct.wYear;
  oss << std::setfill('0') << std::setw(2) << tm_struct.wMonth;
  oss << std::setfill('0') << std::setw(2) << tm_struct.wDay;
  oss << "." << std::setfill('0') << std::setw(2) << tm_struct.wHour;
  oss << std::setfill('0') << std::setw(2) << tm_struct.wMinute;
  oss << std::setfill('0') << std::setw(2) << tm_struct.wSecond;
#else  // UNIX, OSX
  oss << "."
            << std::setfill('0')
            << std::setw(4)
            << tm_struct.tm_year + 1900;
  oss << std::setfill('0')
            << std::setw(2)
            << tm_struct.tm_mon + 1;
  oss << std::setfill('0')
            << std::setw(2)
            << tm_struct.tm_mday;
  oss << "."
            << std::setfill('0')
            << std::setw(2)
            << tm_struct.tm_hour;
  oss << std::setfill('0')
            << std::setw(2)
            << tm_struct.tm_min;
  oss << std::setfill('0')
            << std::setw(2)
            << tm_struct.tm_sec;
#endif
  oss << "." << uv_os_getpid();
  oss << "." << thread_id;
  oss << "." << std::setfill('0') << std::setw(3) << ++seq;
  oss << "." << ext;
  return oss.str();
}

}  // namespace node
