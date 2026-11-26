// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_ISOLATE_DATA_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_ISOLATE_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "experimental/d8/mock-dom/dom.h"
#include "include/cppgc/persistent.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-template.h"

namespace v8 {
namespace mock_dom {

class MutationObserver;

struct PendingReaction {
  cppgc::Persistent<Element> element;
  CEReactionsScope::Kind kind;
  AtomicString name;
  AtomicString old_value;
  AtomicString new_value;
};

// State shared by all realms of the isolate that hosts the mock DOM. d8 only
// installs the mock DOM on its main isolate, so there is exactly one of these.
struct IsolateData {
  Isolate* isolate = nullptr;
  Global<FunctionTemplate> templates[kInterfaceCount];

  int uncaught_error_count = 0;
  bool reporting_exception = false;

  // Custom element reactions, see CEReactionsScope.
  int reactions_scope_depth = 0;
  std::vector<PendingReaction> reactions;

  // Parsed selectors, keyed by selector text.
  absl::flat_hash_map<AtomicString, std::shared_ptr<SelectorList>,
                      AtomicString::Hash>
      selectors;

  // Mutation observers with undelivered records.
  std::vector<cppgc::Persistent<MutationObserver>> active_observers;
  bool observer_microtask_queued = false;
  // Number of live observer registrations. Zero lets tree mutations skip the
  // observer bookkeeping entirely.
  int observer_registration_count = 0;

  static IsolateData* Get();
  static void Create(Isolate* isolate);
  static void Destroy(Isolate* isolate);
};

// Milliseconds since the process started, for Event.timeStamp.
double MonotonicNowMs();

// Calls |function| and reports any exception it throws to |window| (or the
// function's own window when |window| is null). Returns the result, or an
// empty handle if it threw.
MaybeLocal<Value> CallAndReport(Isolate* isolate, Window* window,
                                Local<Function> function, Local<Value> receiver,
                                int argc, Local<Value> argv[]);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_ISOLATE_DATA_H_
