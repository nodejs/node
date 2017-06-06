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

void ErrorThrower::Format(i::Handle<i::JSFunction> constructor,
                          const char* format, va_list args) {
  // Only report the first error.
  if (error()) return;

  constexpr int kMaxErrorMessageLength = 256;
  EmbeddedVector<char, kMaxErrorMessageLength> buffer;

  int context_len = 0;
  if (context_) {
    context_len = SNPrintF(buffer, "%s: ", context_);
    CHECK_LE(0, context_len);  // check for overflow.
  }

  int message_len =
      VSNPrintF(buffer.SubVector(context_len, buffer.length()), format, args);
  CHECK_LE(0, message_len);  // check for overflow.

  Vector<char> whole_message = buffer.SubVector(0, context_len + message_len);
  i::Handle<i::String> message =
      isolate_->factory()
          ->NewStringFromOneByte(Vector<uint8_t>::cast(whole_message))
          .ToHandleChecked();
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
