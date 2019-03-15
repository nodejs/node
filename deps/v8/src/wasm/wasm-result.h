// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_RESULT_H_
#define V8_WASM_WASM_RESULT_H_

#include <cstdarg>
#include <memory>

#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"

#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;
template <typename T>
class Handle;

namespace wasm {

class V8_EXPORT_PRIVATE WasmError {
 public:
  WasmError() = default;

  WasmError(uint32_t offset, std::string message)
      : offset_(offset), message_(std::move(message)) {
    // The error message must not be empty, otherwise {empty()} would be true.
    DCHECK(!message_.empty());
  }

  PRINTF_FORMAT(3, 4)
  WasmError(uint32_t offset, const char* format, ...) : offset_(offset) {
    va_list args;
    va_start(args, format);
    message_ = FormatError(format, args);
    va_end(args);
    // The error message must not be empty, otherwise {empty()} would be true.
    DCHECK(!message_.empty());
  }

  bool empty() const { return message_.empty(); }
  bool has_error() const { return !message_.empty(); }

  uint32_t offset() const { return offset_; }
  const std::string& message() const& { return message_; }
  std::string&& message() && { return std::move(message_); }

 protected:
  static std::string FormatError(const char* format, va_list args);

 private:
  uint32_t offset_ = 0;
  std::string message_;
};

// Either a result of type T, or a WasmError.
template <typename T>
class Result {
 public:
  Result() = default;

  template <typename S>
  explicit Result(S&& value) : value_(std::forward<S>(value)) {}

  template <typename S>
  Result(Result<S>&& other) V8_NOEXCEPT : value_(std::move(other.value_)),
                                          error_(std::move(other.error_)) {}

  explicit Result(WasmError error) : error_(std::move(error)) {}

  template <typename S>
  Result& operator=(Result<S>&& other) V8_NOEXCEPT {
    value_ = std::move(other.value_);
    error_ = std::move(other.error_);
    return *this;
  }

  bool ok() const { return error_.empty(); }
  bool failed() const { return error_.has_error(); }
  const WasmError& error() const& { return error_; }
  WasmError&& error() && { return std::move(error_); }

  // Accessor for the value. Returns const reference if {this} is l-value or
  // const, and returns r-value reference if {this} is r-value. This allows to
  // extract non-copyable values like {std::unique_ptr} by using
  // {std::move(result).value()}.
  const T& value() const & {
    DCHECK(ok());
    return value_;
  }
  T&& value() && {
    DCHECK(ok());
    return std::move(value_);
  }

 private:
  template <typename S>
  friend class Result;

  T value_ = T{};
  WasmError error_;

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

  void CompileFailed(const WasmError& error) {
    DCHECK(error.has_error());
    CompileError("%s @+%u", error.message().c_str(), error.offset());
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

  // ErrorThrower should always be stack-allocated, since it constitutes a scope
  // (things happen in the destructor).
  DISALLOW_NEW_AND_DELETE()
  DISALLOW_COPY_AND_ASSIGN(ErrorThrower);
};

// Use {nullptr_t} as data value to indicate that this only stores the error,
// but no result value (the only valid value is {nullptr}).
// [Storing {void} would require template specialization.]
using VoidResult = Result<std::nullptr_t>;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_RESULT_H_
