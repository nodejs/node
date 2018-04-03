// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/debug/stack_trace.h"

#include <string.h>

#include <algorithm>
#include <sstream>

#include "src/base/macros.h"

namespace v8 {
namespace base {
namespace debug {

StackTrace::StackTrace(const void* const* trace, size_t count) {
  count = std::min(count, arraysize(trace_));
  if (count) memcpy(trace_, trace, count * sizeof(trace_[0]));
  count_ = count;
}

StackTrace::~StackTrace() {}

const void* const* StackTrace::Addresses(size_t* count) const {
  *count = count_;
  if (count_) return trace_;
  return nullptr;
}

std::string StackTrace::ToString() const {
  std::stringstream stream;
  OutputToStream(&stream);
  return stream.str();
}

}  // namespace debug
}  // namespace base
}  // namespace v8
