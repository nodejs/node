// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-result.h"

#include "src/factory.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects-inl.h"  // TODO(mstarzinger): Temporary cycle breaker!
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


void ErrorThrower::Error(const char* format, ...) {
  if (error_) return;  // only report the first error.
  error_ = true;
  char buffer[256];

  va_list arguments;
  va_start(arguments, format);
  base::OS::VSNPrintF(buffer, 255, format, arguments);
  va_end(arguments);

  std::ostringstream str;
  if (context_ != nullptr) {
    str << context_ << ": ";
  }
  str << buffer;

  isolate_->ScheduleThrow(
      *isolate_->factory()->NewStringFromAsciiChecked(str.str().c_str()));
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
