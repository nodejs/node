/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_TRACE_PROCESSOR_BASIC_TYPES_H_
#define INCLUDE_PERFETTO_TRACE_PROCESSOR_BASIC_TYPES_H_

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <functional>
#include <string>

#include "perfetto/base/export.h"
#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

// Various places in trace processor assume a max number of CPUs to keep code
// simpler (e.g. use arrays instead of vectors).
constexpr size_t kMaxCpus = 128;

// Struct for configuring a TraceProcessor instance (see trace_processor.h).
struct PERFETTO_EXPORT Config {
  // When set to true, this option forces trace processor to perform a full
  // sort ignoring any internal heureustics to skip sorting parts of the data.
  bool force_full_sort = false;

  // When set to false, this option makes the trace processor not include ftrace
  // events in the raw table; this makes converting events back to the systrace
  // text format impossible. On the other hand, it also saves ~50% of memory
  // usage of trace processor. For reference, Studio intends to use this option.
  //
  // Note: "generic" ftrace events will be parsed into the raw table even if
  // this flag is false and all other events which parse into the raw table are
  // unaffected by this flag.
  bool ingest_ftrace_in_raw_table = true;
};

// Represents a dynamically typed value returned by SQL.
struct PERFETTO_EXPORT SqlValue {
  // Represents the type of the value.
  enum Type {
    kNull = 0,
    kLong,
    kDouble,
    kString,
    kBytes,
  };

  SqlValue() = default;

  static SqlValue Long(int64_t v) {
    SqlValue value;
    value.long_value = v;
    value.type = Type::kLong;
    return value;
  }

  static SqlValue Double(double v) {
    SqlValue value;
    value.double_value = v;
    value.type = Type::kDouble;
    return value;
  }

  static SqlValue String(const char* v) {
    SqlValue value;
    value.string_value = v;
    value.type = Type::kString;
    return value;
  }

  double AsDouble() const {
    PERFETTO_CHECK(type == kDouble);
    return double_value;
  }
  int64_t AsLong() const {
    PERFETTO_CHECK(type == kLong);
    return long_value;
  }
  const char* AsString() const {
    PERFETTO_CHECK(type == kString);
    return string_value;
  }
  const void* AsBytes() const {
    PERFETTO_CHECK(type == kBytes);
    return bytes_value;
  }

  bool is_null() const { return type == Type::kNull; }

  // Up to 1 of these fields can be accessed depending on |type|.
  union {
    // This string will be owned by the iterator that returned it and is valid
    // as long until the subsequent call to Next().
    const char* string_value;
    int64_t long_value;
    double double_value;
    const void* bytes_value;
  };
  // The size of bytes_value. Only valid when |type == kBytes|.
  size_t bytes_count = 0;
  Type type = kNull;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACE_PROCESSOR_BASIC_TYPES_H_
