// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_RESULT_H_
#define V8_WASM_RESULT_H_

#include <memory>

#include "src/base/compiler-specific.h"

#include "src/handles.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

namespace wasm {

// Error codes for programmatic checking of the decoder's verification.
enum ErrorCode {
  kSuccess,
  kError,  // TODO(titzer): introduce real error codes
};

// The overall result of decoding a function or a module.
template <typename T>
struct Result {
  Result() : val(), error_code(kSuccess), start(nullptr), error_pc(nullptr) {}
  Result(Result&& other) { *this = std::move(other); }
  Result& operator=(Result&& other) {
    MoveFrom(other);
    val = other.val;
    return *this;
  }

  T val;
  ErrorCode error_code;
  const byte* start;
  const byte* error_pc;
  const byte* error_pt;
  std::unique_ptr<char[]> error_msg;

  bool ok() const { return error_code == kSuccess; }
  bool failed() const { return error_code != kSuccess; }

  template <typename V>
  void MoveFrom(Result<V>& that) {
    error_code = that.error_code;
    start = that.start;
    error_pc = that.error_pc;
    error_pt = that.error_pt;
    error_msg = std::move(that.error_msg);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Result);
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const Result<T>& result) {
  os << "Result = ";
  if (result.ok()) {
    if (result.val != nullptr) {
      os << *result.val;
    } else {
      os << "success (no value)";
    }
  } else if (result.error_msg.get() != nullptr) {
    ptrdiff_t offset = result.error_pc - result.start;
    if (offset < 0) {
      os << result.error_msg.get() << " @" << offset;
    } else {
      os << result.error_msg.get() << " @+" << offset;
    }
  } else {
    os << result.error_code;
  }
  os << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const ErrorCode& error_code);

// A helper for generating error messages that bubble up to JS exceptions.
class V8_EXPORT_PRIVATE ErrorThrower {
 public:
  ErrorThrower(i::Isolate* isolate, const char* context)
      : isolate_(isolate), context_(context) {}
  ~ErrorThrower();

  PRINTF_FORMAT(2, 3) void Error(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void TypeError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void RangeError(const char* fmt, ...);

  template <typename T>
  void Failed(const char* error, Result<T>& result) {
    std::ostringstream str;
    str << error << result;
    Error("%s", str.str().c_str());
  }

  i::Handle<i::Object> Reify() {
    i::Handle<i::Object> result = exception_;
    exception_ = i::Handle<i::Object>::null();
    return result;
  }

  bool error() const { return !exception_.is_null(); }

 private:
  void Format(i::Handle<i::JSFunction> constructor, const char* fmt, va_list);

  i::Isolate* isolate_;
  const char* context_;
  i::Handle<i::Object> exception_;
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif
