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

#include "src/trace_processor/importers/fuchsia/fuchsia_trace_utils.h"

namespace perfetto {
namespace trace_processor {
namespace fuchsia_trace_utils {

namespace {
constexpr uint32_t kInlineStringMarker = 0x8000;
constexpr uint32_t kInlineStringLengthMask = 0x7FFF;
}  // namespace

bool IsInlineString(uint32_t string_ref) {
  // Treat a string ref of 0 (the empty string) as inline. The empty string is
  // not a true entry in the string table.
  return (string_ref & kInlineStringMarker) || (string_ref == 0);
}

bool IsInlineThread(uint32_t thread_ref) {
  return thread_ref == 0;
}

// Converts a tick count to nanoseconds. Returns -1 if the result would not
// fit in a nonnegative int64_t. Negative timestamps are not allowed by the
// Fuchsia trace format. Also returns -1 if ticks_per_second is zero.
int64_t TicksToNs(uint64_t ticks, uint64_t ticks_per_second) {
  uint64_t ticks_hi = ticks >> 32;
  uint64_t ticks_lo = ticks & ((uint64_t(1) << 32) - 1);
  uint64_t ns_per_sec = 1000000000;
  if (ticks_per_second == 0) {
    return -1;
  }
  // This multiplication may overflow.
  uint64_t result_hi = ticks_hi * ((ns_per_sec << 32) / ticks_per_second);
  if (ticks_hi != 0 &&
      result_hi / ticks_hi != ((ns_per_sec << 32) / ticks_per_second)) {
    return -1;
  }
  // This computation never overflows, because ticks_lo is less than 2^32, and
  // ns_per_sec = 10^9 < 2^32.
  uint64_t result_lo = ticks_lo * ns_per_sec / ticks_per_second;
  // Performing addition before the cast avoids undefined behavior.
  int64_t result = static_cast<int64_t>(result_hi + result_lo);
  // Check for addition overflow.
  if (result < 0) {
    return -1;
  }
  return result;
}

Variadic ArgValue::ToStorageVariadic(TraceStorage* storage) const {
  switch (type_) {
    case ArgType::kNull:
      return Variadic::String(storage->InternString("null"));
    case ArgType::kInt32:
      return Variadic::Integer(static_cast<int64_t>(int32_));
    case ArgType::kUint32:
      return Variadic::Integer(static_cast<int64_t>(uint32_));
    case ArgType::kInt64:
      return Variadic::Integer(int64_);
    case ArgType::kUint64:
      return Variadic::Integer(static_cast<int64_t>(uint64_));
    case ArgType::kDouble:
      return Variadic::Real(double_);
    case ArgType::kString:
      return Variadic::String(string_);
    case ArgType::kPointer:
      return Variadic::Integer(static_cast<int64_t>(pointer_));
    case ArgType::kKoid:
      return Variadic::Integer(static_cast<int64_t>(koid_));
    case ArgType::kUnknown:
      return Variadic::String(storage->InternString("unknown"));
  }
  PERFETTO_FATAL("Not reached");  // Make GCC happy.
}

size_t RecordCursor::WordIndex() {
  return word_index_;
}

void RecordCursor::SetWordIndex(size_t index) {
  word_index_ = index;
}

bool RecordCursor::ReadTimestamp(uint64_t ticks_per_second, int64_t* ts_out) {
  const uint8_t* ts_data;
  if (!ReadWords(1, &ts_data)) {
    return false;
  }
  if (ts_out != nullptr) {
    uint64_t ticks;
    memcpy(&ticks, ts_data, sizeof(uint64_t));
    *ts_out = TicksToNs(ticks, ticks_per_second);
  }
  return true;
}

bool RecordCursor::ReadInlineString(uint32_t string_ref_or_len,
                                    base::StringView* string_out) {
  // Note that this works correctly for the empty string, where string_ref is 0.
  size_t len = string_ref_or_len & kInlineStringLengthMask;
  size_t len_words = (len + 7) / 8;
  const uint8_t* string_data;
  if (!ReadWords(len_words, &string_data)) {
    return false;
  }
  if (string_out != nullptr) {
    *string_out =
        base::StringView(reinterpret_cast<const char*>(string_data), len);
  }
  return true;
}

bool RecordCursor::ReadInlineThread(ThreadInfo* thread_out) {
  const uint8_t* thread_data;
  if (!ReadWords(2, &thread_data)) {
    return false;
  }
  if (thread_out != nullptr) {
    memcpy(&thread_out->pid, thread_data, sizeof(uint64_t));
    memcpy(&thread_out->tid, thread_data + sizeof(uint64_t), sizeof(uint64_t));
  }
  return true;
}

bool RecordCursor::ReadInt64(int64_t* out) {
  const uint8_t* out_data;
  if (!ReadWords(1, &out_data)) {
    return false;
  }
  if (out != nullptr) {
    memcpy(out, out_data, sizeof(int64_t));
  }
  return true;
}

bool RecordCursor::ReadUint64(uint64_t* out) {
  const uint8_t* out_data;
  if (!ReadWords(1, &out_data)) {
    return false;
  }
  if (out != nullptr) {
    memcpy(out, out_data, sizeof(uint64_t));
  }
  return true;
}

bool RecordCursor::ReadDouble(double* out) {
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits");

  const uint8_t* out_data;
  if (!ReadWords(1, &out_data)) {
    return false;
  }
  if (out != nullptr) {
    memcpy(out, out_data, sizeof(double));
  }
  return true;
}

bool RecordCursor::ReadWords(size_t num_words, const uint8_t** data_out) {
  const uint8_t* data = begin_ + sizeof(uint64_t) * word_index_;
  // This addition is unconditional so that callers with data_out == nullptr do
  // not necessarily have to check the return value, as future calls will fail
  // due to attempting to read out of bounds.
  word_index_ += num_words;
  if (data + sizeof(uint64_t) * num_words <= end_) {
    if (data_out != nullptr) {
      *data_out = data;
    }
    return true;
  } else {
    return false;
  }
}

}  // namespace fuchsia_trace_utils
}  // namespace trace_processor
}  // namespace perfetto
