// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-result.h"

#include "src/factory.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/objects.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {
namespace wasm {

std::ostream& operator<<(std::ostream& os, const ErrorCode& error_code) {
  switch (error_code) {
    case kSuccess:
      os << "Success";
      break;
    default:  // TODO(titzer): render error codes
      os << "Error";
      break;
  }
  return os;
}

void ErrorThrower::Format(i::Handle<i::JSFunction> constructor,
                          const char* format, va_list args) {
  // Only report the first error.
  if (error()) return;

  char buffer[256];
  base::OS::VSNPrintF(buffer, 255, format, args);

  std::ostringstream str;
  if (context_ != nullptr) {
    str << context_ << ": ";
  }
  str << buffer;

  i::Handle<i::String> message =
      isolate_->factory()->NewStringFromAsciiChecked(str.str().c_str());
  exception_ = isolate_->factory()->NewError(constructor, message);
}

void ErrorThrower::TypeError(const char* format, ...) {
  if (error()) return;
  va_list arguments;
  va_start(arguments, format);
  Format(isolate_->type_error_function(), format, arguments);
  va_end(arguments);
}

void ErrorThrower::RangeError(const char* format, ...) {
  if (error()) return;
  va_list arguments;
  va_start(arguments, format);
  Format(isolate_->range_error_function(), format, arguments);
  va_end(arguments);
}

void ErrorThrower::CompileError(const char* format, ...) {
  if (error()) return;
  wasm_error_ = true;
  va_list arguments;
  va_start(arguments, format);
  Format(isolate_->wasm_compile_error_function(), format, arguments);
  va_end(arguments);
}

void ErrorThrower::LinkError(const char* format, ...) {
  if (error()) return;
  wasm_error_ = true;
  va_list arguments;
  va_start(arguments, format);
  Format(isolate_->wasm_link_error_function(), format, arguments);
  va_end(arguments);
}

void ErrorThrower::RuntimeError(const char* format, ...) {
  if (error()) return;
  wasm_error_ = true;
  va_list arguments;
  va_start(arguments, format);
  Format(isolate_->wasm_runtime_error_function(), format, arguments);
  va_end(arguments);
}

ErrorThrower::~ErrorThrower() {
  if (error() && !isolate_->has_pending_exception()) {
    isolate_->ScheduleThrow(*exception_);
  }
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
