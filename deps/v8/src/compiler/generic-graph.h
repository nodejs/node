// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_GRAPH_H_
#define V8_COMPILER_GENERIC_GRAPH_H_

#include "src/compiler/generic-node.h"

namespace v8 {
namespace internal {

class Zone;

namespace compiler {

class GenericGraphBase : public ZoneObject {
 public:
  explicit GenericGraphBase(Zone* zone) : zone_(zone), next_node_id_(0) {}

  Zone* zone() const { return zone_; }

  NodeId NextNodeID() { return next_node_id_++; }
  NodeId NodeCount() const { return next_node_id_; }

 private:
  Zone* zone_;
  NodeId next_node_id_;
};

template <class V>
class GenericGraph : public GenericGraphBase {
 public:
  explicit GenericGraph(Zone* zone)
      : GenericGraphBase(zone), start_(NULL), end_(NULL) {}

  V* start() const { return start_; }
  V* end() const { return end_; }

  void SetStart(V* start) { start_ = start; }
  void SetEnd(V* end) { end_ = end; }

 private:
  V* start_;
  V* end_;

  DISALLOW_COPY_AND_ASSIGN(GenericGraph);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GENERIC_GRAPH_H_
