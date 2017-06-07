// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_RESULT_H_
#define V8_WASM_RESULT_H_

#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/utils.h"

#include "src/handles.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

namespace wasm {

// The overall result of decoding a function or a module.
template <typename T>
class Result {
 public:
  Result() = default;

  template <typename S>
  explicit Result(S&& value) : val(value) {}

  template <typename S>
  Result(Result<S>&& other)
      : val(std::move(other.val)),
        error_offset(other.error_offset),
        error_msg(std::move(other.error_msg)) {}

  Result& operator=(Result&& other) = default;

  T val = T{};
  uint32_t error_offset = 0;
  std::string error_msg;

  bool ok() const { return error_msg.empty(); }
  bool failed() const { return !ok(); }

  template <typename V>
  void MoveErrorFrom(Result<V>& that) {
    error_offset = that.error_offset;
    // Use {swap()} + {clear()} instead of move assign, as {that} might still be
    // used afterwards.
    error_msg.swap(that.error_msg);
    that.error_msg.clear();
  }

  void PRINTF_FORMAT(2, 3) error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    verror(format, args);
    va_end(args);
  }

  void PRINTF_FORMAT(2, 0) verror(const char* format, va_list args) {
    size_t len = base::bits::RoundUpToPowerOfTwo32(
        static_cast<uint32_t>(strlen(format)));
    // Allocate increasingly large buffers until the message fits.
    for (;; len *= 2) {
      DCHECK_GE(kMaxInt, len);
      error_msg.resize(len);
      int written =
          VSNPrintF(Vector<char>(&error_msg.front(), static_cast<int>(len)),
                    format, args);
      if (written < 0) continue;              // not enough space.
      if (written == 0) error_msg = "Error";  // assign default message.
      return;
    }
  }

  static Result<T> PRINTF_FORMAT(1, 2) Error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    Result<T> result;
    result.verror(format, args);
    va_end(args);
    return result;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Result);
};

// A helper for generating error messages that bubble up to JS exceptions.
class V8_EXPORT_PRIVATE ErrorThrower {
 public:
  ErrorThrower(i::Isolate* isolate, const char* context)
      : isolate_(isolate), context_(context) {}
  ~ErrorThrower();

  PRINTF_FORMAT(2, 3) void TypeError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void RangeError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void CompileError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void LinkError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void RuntimeError(const char* fmt, ...);

  template <typename T>
  void CompileFailed(const char* error, Result<T>& result) {
    DCHECK(result.failed());
    CompileError("%s: %s @+%u", error, result.error_msg.c_str(),
                 result.error_offset);
  }

  i::Handle<i::Object> Reify() {
    i::Handle<i::Object> result = exception_;
    exception_ = i::Handle<i::Object>::null();
    return result;
  }

  bool error() const { return !exception_.is_null(); }
  bool wasm_error() { return wasm_error_; }

 private:
  void Format(i::Handle<i::JSFunction> constructor, const char* fmt, va_list);

  i::Isolate* isolate_;
  const char* context_;
  i::Handle<i::Object> exception_;
  bool wasm_error_ = false;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif
