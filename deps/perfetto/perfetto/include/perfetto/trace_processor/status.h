/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACE_PROCESSOR_STATUS_H_
#define INCLUDE_PERFETTO_TRACE_PROCESSOR_STATUS_H_

#include <stdarg.h>
#include <string>

#include "perfetto/base/export.h"

namespace perfetto {
namespace trace_processor {

// Status and related methods are inside util for consistency with embedders of
// trace processor.
namespace util {

// Represents either the success or the failure message of a function.
// This can used as the return type of functions which would usually return an
// bool for success or int for errno but also wants to add some string context
// (ususally for logging).
class PERFETTO_EXPORT Status {
 public:
  Status() : ok_(true) {}
  explicit Status(std::string error) : ok_(false), message_(std::move(error)) {}

  // Copy operations.
  Status(const Status&) = default;
  Status& operator=(const Status&) = default;

  // Move operations. The moved-from state is valid but unspecified.
  Status(Status&&) noexcept = default;
  Status& operator=(Status&&) = default;

  bool ok() const { return ok_; }

  // Only valid to call when this message has an Err status (i.e. ok() returned
  // false or operator bool() returned true).
  const std::string& message() const { return message_; }

  // Only valid to call when this message has an Err status (i.e. ok() returned
  // false or operator bool() returned true).
  const char* c_message() const { return message_.c_str(); }

 private:
  bool ok_ = false;
  std::string message_;
};

// Returns a status object which represents the Ok status.
inline Status OkStatus() {
  return Status();
}

// Returns a status object which represents an error with the given message
// formatted using printf.
__attribute__((__format__(__printf__, 1, 2))) inline Status ErrStatus(
    const char* format,
    ...) {
  va_list ap;
  va_start(ap, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, ap);
  return Status(std::string(buffer));
}

}  // namespace util
}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACE_PROCESSOR_STATUS_H_
