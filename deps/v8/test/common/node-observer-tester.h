// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_NODEOBSERVER_TESTER_H_
#define V8_COMMON_NODEOBSERVER_TESTER_H_

#include "src/compiler/node-observer.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/type-hints.h"

namespace v8 {
namespace internal {
namespace compiler {

// Helpers to test TurboFan compilation using the %ObserveNode intrinsic.
struct ObserveNodeScope {
 public:
  ObserveNodeScope(Isolate* isolate, NodeObserver* node_observer)
      : isolate_(isolate) {
    DCHECK_NOT_NULL(isolate_);
    DCHECK_NULL(isolate_->node_observer());
    isolate_->set_node_observer(node_observer);
  }

  ~ObserveNodeScope() {
    DCHECK_NOT_NULL(isolate_->node_observer());

    // Checks that the code wrapped by %ObserveNode() was actually compiled in
    // the test.
    CHECK(isolate_->node_observer()->has_observed_changes());

    isolate_->set_node_observer(nullptr);
  }

 private:
  Isolate* isolate_;
};

class CreationObserver : public NodeObserver {
 public:
  explicit CreationObserver(std::function<void(const Node*)> handler)
      : handler_(handler) {
    DCHECK(handler_);
  }

  Observation OnNodeCreated(const Node* node) override {
    handler_(node);
    return Observation::kStop;
  }

 private:
  std::function<void(const Node*)> handler_;
};

class ModificationObserver : public NodeObserver {
 public:
  explicit ModificationObserver(
      std::function<void(const Node*)> on_created_handler,
      std::function<NodeObserver::Observation(
          const Node*, const ObservableNodeState& old_state)>
          on_changed_handler)
      : on_created_handler_(on_created_handler),
        on_changed_handler_(on_changed_handler) {
    DCHECK(on_created_handler_);
    DCHECK(on_changed_handler_);
  }

  Observation OnNodeCreated(const Node* node) override {
    on_created_handler_(node);
    return Observation::kContinue;
  }

  Observation OnNodeChanged(const char* reducer_name, const Node* node,
                            const ObservableNodeState& old_state) override {
    return on_changed_handler_(node, old_state);
  }

 private:
  std::function<void(const Node*)> on_created_handler_;
  std::function<NodeObserver::Observation(const Node*,
                                          const ObservableNodeState& old_state)>
      on_changed_handler_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_NODEOBSERVER_TESTER_H_
