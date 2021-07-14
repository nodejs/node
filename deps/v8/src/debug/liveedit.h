// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_LIVEEDIT_H_
#define V8_DEBUG_LIVEEDIT_H_

#include <vector>

#include "src/common/globals.h"
#include "src/handles/handles.h"

namespace v8 {
namespace debug {
struct LiveEditResult;
}  // namespace debug
namespace internal {

class Script;
class String;
class Debug;
class JavaScriptFrame;

struct SourceChangeRange {
  int start_position;
  int end_position;
  int new_start_position;
  int new_end_position;
};

/**
  Liveedit step-by-step:
  1. calculate diff between old source and new source,
  2. map function literals from old source to new source,
  3. create new script for new_source,
  4. mark literals with changed code as changed, all others as unchanged,
  5. check that for changed literals there are no:
    - running generators in the heap,
    - non droppable frames (e.g. running generator) above them on stack.
  6. mark the bottom most frame with changed function as scheduled for restart
     if any,
  7. for unchanged functions:
    - deoptimize,
    - remove from cache,
    - update source positions,
    - move to new script,
    - reset feedback information and preparsed scope information if any,
    - replace any sfi in constant pool with changed one if any.
  8. for changed functions:
    - deoptimize
    - remove from cache,
    - reset feedback information,
    - update all links from js functions to old shared with new one.
  9. swap scripts.
 */

class V8_EXPORT_PRIVATE LiveEdit : AllStatic {
 public:
  static void CompareStrings(Isolate* isolate, Handle<String> a,
                             Handle<String> b,
                             std::vector<SourceChangeRange>* diffs);
  static int TranslatePosition(const std::vector<SourceChangeRange>& changed,
                               int position);
  static void PatchScript(Isolate* isolate, Handle<Script> script,
                          Handle<String> source, bool preview,
                          debug::LiveEditResult* result);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_LIVEEDIT_H_
