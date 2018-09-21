// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_RESULT_H_
#define V8_WASM_WASM_RESULT_H_

#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/utils.h"

#include "src/globals.h"
#include "src/handles.h"

namespace v8 {
namespace internal {

class Isolate;

namespace wasm {

// Base class for Result<T>.
class V8_EXPORT_PRIVATE ResultBase {
 protected:
  ResultBase() = default;

  ResultBase& operator=(ResultBase&& other) V8_NOEXCEPT = default;

 public:
  ResultBase(ResultBase&& other) V8_NOEXCEPT
      : error_offset_(other.error_offset_),
        error_msg_(std::move(other.error_msg_)) {}

  void error(uint32_t offset, std::string error_msg);

  void PRINTF_FORMAT(2, 3) error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    verror(format, args);
    va_end(args);
  }

  void PRINTF_FORMAT(2, 0) verror(const char* format, va_list args);

  void MoveErrorFrom(ResultBase& that) {
    error_offset_ = that.error_offset_;
    // Use {swap()} + {clear()} instead of move assign, as {that} might still
    // be used afterwards.
    error_msg_.swap(that.error_msg_);
    that.error_msg_.clear();
  }

  bool ok() const { return error_msg_.empty(); }
  bool failed() const { return !ok(); }

  uint32_t error_offset() const { return error_offset_; }
  const std::string& error_msg() const { return error_msg_; }

 private:
  uint32_t error_offset_ = 0;
  std::string error_msg_;
};

// The overall result of decoding a function or a module.
template <typename T>
class Result : public ResultBase {
 public:
  Result() = default;

  template <typename S>
  explicit Result(S&& value) : val(std::forward<S>(value)) {}

  template <typename S>
  Result(Result<S>&& other) V8_NOEXCEPT : ResultBase(std::move(other)),
                                          val(std::move(other.val)) {}

  Result& operator=(Result&& other) V8_NOEXCEPT = default;

  static Result<T> PRINTF_FORMAT(1, 2) Error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    Result<T> result;
    result.verror(format, args);
    va_end(args);
    return result;
  }

  T val = T{};

 private:
  DISALLOW_COPY_AND_ASSIGN(Result);
};

// A helper for generating error messages that bubble up to JS exceptions.
class V8_EXPORT_PRIVATE ErrorThrower {
 public:
  ErrorThrower(Isolate* isolate, const char* context)
      : isolate_(isolate), context_(context) {}
  // Explicitly allow move-construction. Disallow copy (below).
  ErrorThrower(ErrorThrower&& other) V8_NOEXCEPT;
  ~ErrorThrower();

  PRINTF_FORMAT(2, 3) void TypeError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void RangeError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void CompileError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void LinkError(const char* fmt, ...);
  PRINTF_FORMAT(2, 3) void RuntimeError(const char* fmt, ...);

  template <typename T>
  void CompileFailed(const char* error, Result<T>& result) {
    DCHECK(result.failed());
    CompileError("%s: %s @+%u", error, result.error_msg().c_str(),
                 result.error_offset());
  }

  // Create and return exception object.
  V8_WARN_UNUSED_RESULT Handle<Object> Reify();

  // Reset any error which was set on this thrower.
  void Reset();

  bool error() const { return error_type_ != kNone; }
  bool wasm_error() { return error_type_ >= kFirstWasmError; }
  const char* error_msg() { return error_msg_.c_str(); }

  Isolate* isolate() const { return isolate_; }

 private:
  enum ErrorType {
    kNone,
    // General errors.
    kTypeError,
    kRangeError,
    // Wasm errors.
    kCompileError,
    kLinkError,
    kRuntimeError,

    // Marker.
    kFirstWasmError = kCompileError
  };

  void Format(ErrorType error_type_, const char* fmt, va_list);

  Isolate* isolate_;
  const char* context_;
  ErrorType error_type_ = kNone;
  std::string error_msg_;

  DISALLOW_COPY_AND_ASSIGN(ErrorThrower);
  // ErrorThrower should always be stack-allocated, since it constitutes a scope
  // (things happen in the destructor).
  DISALLOW_NEW_AND_DELETE();
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_RESULT_H_
