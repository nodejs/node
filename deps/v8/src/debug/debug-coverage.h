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
  explicit CoverageScript(Handle<Script> s) : script(s) {}
  Handle<Script> script;
  // Functions are sorted by start position, from outer to inner function.
  std::vector<CoverageFunction> functions;
};

class Coverage : public std::vector<CoverageScript> {
 public:
  // Collecting precise coverage only works if the modes kPreciseCount or
  // kPreciseBinary is selected. The invocation count is reset on collection.
  // In case of kPreciseCount, an updated count since last collection is
  // returned. In case of kPreciseBinary, a count of 1 is returned if a
  // function has been executed for the first time since last collection.
  static Coverage* CollectPrecise(Isolate* isolate);
  // Collecting best effort coverage always works, but may be imprecise
  // depending on selected mode. The invocation count is not reset.
  static Coverage* CollectBestEffort(Isolate* isolate);

  // Select code coverage mode.
  static void SelectMode(Isolate* isolate, debug::Coverage::Mode mode);

 private:
  static Coverage* Collect(Isolate* isolate,
                           v8::debug::Coverage::Mode collectionMode);

  Coverage() {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_COVERAGE_H_
