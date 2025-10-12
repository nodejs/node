// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-labeller.h"

namespace v8 {
namespace internal {
namespace maglev {

thread_local MaglevGraphLabeller* thread_graph_labeller;

MaglevGraphLabellerScope::MaglevGraphLabellerScope(
    MaglevGraphLabeller* graph_labeller) {
  DCHECK_NULL(thread_graph_labeller);
  DCHECK_NOT_NULL(graph_labeller);
  thread_graph_labeller = graph_labeller;
}
MaglevGraphLabellerScope::~MaglevGraphLabellerScope() {
  thread_graph_labeller = nullptr;
}

MaglevGraphLabeller* GetCurrentGraphLabeller() {
  DCHECK_NOT_NULL(thread_graph_labeller);
  return thread_graph_labeller;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
