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

namespace v8 {
namespace internal {

class Isolate;
template <typename T>
class Handle;

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

  bool ok() const { return error_msg_.empty(); }
  bool failed() const { return !ok(); }

  uint32_t error_offset() const { return error_offset_; }
  const std::string& error_msg() const & { return error_msg_; }
  std::string&& error_msg() && { return std::move(error_msg_); }

 protected:
  ResultBase(uint32_t error_offset, std::string error_msg)
      : error_offset_(error_offset), error_msg_(std::move(error_msg)) {
    // The error message must not be empty, otherwise {failed()} will be false.
    DCHECK(!error_msg_.empty());
  }

  static std::string FormatError(const char* format, va_list args);

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
  explicit Result(S&& value) : value_(std::forward<S>(value)) {}

  template <typename S>
  Result(Result<S>&& other) V8_NOEXCEPT : ResultBase(std::move(other)),
                                          value_(std::move(other).value()) {}

  Result& operator=(Result&& other) V8_NOEXCEPT = default;

  static Result<T> PRINTF_FORMAT(2, 3)
      Error(uint32_t offset, const char* format, ...) {
    va_list args;
    va_start(args, format);
    Result<T> error_result{offset, FormatError(format, args)};
    va_end(args);
    return error_result;
  }

  static Result<T> Error(uint32_t error_offset, std::string error_msg) {
    // Call private constructor.
    return Result<T>{error_offset, std::move(error_msg)};
  }

  static Result<T> ErrorFrom(ResultBase&& error_result) {
    return Error(error_result.error_offset(),
                 std::move(error_result).error_msg());
  }

  static Result<T> ErrorFrom(const ResultBase& error_result) {
    return Error(error_result.error_offset(), error_result.error_msg());
  }

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
  T value_ = T{};

  Result(uint32_t error_offset, std::string error_msg)
      : ResultBase(error_offset, std::move(error_msg)) {}

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

  void CompileFailed(const char* error, const ResultBase& result) {
    DCHECK(result.failed());
    CompileError("%s: %s @+%u", error, result.error_msg().c_str(),
                 result.error_offset());
  }

  void CompileFailed(const ResultBase& result) {
    DCHECK(result.failed());
    CompileError("%s @+%u", result.error_msg().c_str(), result.error_offset());
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
  DISALLOW_NEW_AND_DELETE();
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
