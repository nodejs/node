// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_COVERAGE_H_
#define V8_DEBUG_DEBUG_COVERAGE_H_

#include <vector>

#include "src/debug/debug-interface.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declaration.
class Isolate;

struct CoverageFunction {
  CoverageFunction(int s, int e, uint32_t c, Handle<String> n)
      : start(s), end(e), count(c), name(n) {}
  int start;
  int end;
  uint32_t count;
  Handle<String> name;
};

struct CoverageScript {
  // Initialize top-level function in case it has been garbage-collected.
  CoverageScript(Isolate* isolate, Handle<Script> s) : script(s) {}
  Handle<Script> script;
  // Functions are sorted by start position, from outer to inner function.
  std::vector<CoverageFunction> functions;
};

class Coverage : public std::vector<CoverageScript> {
 public:
  // Allocate a new Coverage object and populate with result.
  // The ownership is transferred to the caller.
  static Coverage* Collect(Isolate* isolate, bool reset_count);

  // Enable precise code coverage. This disables optimization and makes sure
  // invocation count is not affected by GC.
  static void TogglePrecise(Isolate* isolate, bool enable);

 private:
  Coverage() {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_COVERAGE_H_
