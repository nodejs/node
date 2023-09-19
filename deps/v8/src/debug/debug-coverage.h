// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_COVERAGE_H_
#define V8_DEBUG_DEBUG_COVERAGE_H_

#include <memory>
#include <vector>

#include "src/debug/debug-interface.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

// Forward declaration.
class Isolate;

struct CoverageBlock {
  CoverageBlock(int s, int e, uint32_t c) : start(s), end(e), count(c) {}
  CoverageBlock() : CoverageBlock(kNoSourcePosition, kNoSourcePosition, 0) {}

  int start;
  int end;
  uint32_t count;
};

struct CoverageFunction {
  CoverageFunction(int s, int e, uint32_t c, Handle<String> n)
      : start(s), end(e), count(c), name(n), has_block_coverage(false) {}

  bool HasNonEmptySourceRange() const { return start < end && start >= 0; }
  bool HasBlocks() const { return !blocks.empty(); }

  int start;
  int end;
  uint32_t count;
  Handle<String> name;
  // Blocks are sorted by start position, from outer to inner blocks.
  std::vector<CoverageBlock> blocks;
  bool has_block_coverage;
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
  static std::unique_ptr<Coverage> CollectPrecise(Isolate* isolate);
  // Collecting best effort coverage always works, but may be imprecise
  // depending on selected mode. The invocation count is not reset.
  static std::unique_ptr<Coverage> CollectBestEffort(Isolate* isolate);

  // Select code coverage mode.
  static void SelectMode(Isolate* isolate, debug::CoverageMode mode);

 private:
  static std::unique_ptr<Coverage> Collect(
      Isolate* isolate, v8::debug::CoverageMode collectionMode);

  Coverage() = default;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_COVERAGE_H_
