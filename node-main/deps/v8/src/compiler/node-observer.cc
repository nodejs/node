// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-observer.h"

#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

ObservableNodeState::ObservableNodeState(const Node* node, Zone* zone)
    : id_(node->id()),
      op_(node->op()),
      type_(NodeProperties::GetTypeOrAny(node)) {}

void ObserveNodeManager::StartObserving(Node* node, NodeObserver* observer) {
  DCHECK_NOT_NULL(node);
  DCHECK_NOT_NULL(observer);
  DCHECK(observations_.find(node->id()) == observations_.end());

  observer->set_has_observed_changes();
  NodeObserver::Observation observation = observer->OnNodeCreated(node);
  if (observation == NodeObserver::Observation::kContinue) {
    observations_[node->id()] =
        zone_->New<NodeObservation>(observer, node, zone_);
  } else {
    DCHECK_EQ(observation, NodeObserver::Observation::kStop);
  }
}

void ObserveNodeManager::OnNodeChanged(const char* reducer_name,
                                       const Node* old_node,
                                       const Node* new_node) {
  const auto it = observations_.find(old_node->id());
  if (it == observations_.end()) return;

  ObservableNodeState new_state{new_node, zone_};
  NodeObservation* observation = it->second;
  if (observation->state == new_state) return;

  ObservableNodeState old_state = observation->state;
  observation->state = new_state;

  NodeObserver::Observation result =
      observation->observer->OnNodeChanged(reducer_name, new_node, old_state);
  if (result == NodeObserver::Observation::kStop) {
    observations_.erase(old_node->id());
  } else {
    DCHECK_EQ(result, NodeObserver::Observation::kContinue);
    if (old_node != new_node) {
      observations_.erase(old_node->id());
      observations_[new_node->id()] = observation;
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
