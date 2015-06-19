// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPE_FEEDBACK_H_
#define V8_COMPILER_JS_TYPE_FEEDBACK_H_

#include "src/utils.h"

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

class TypeFeedbackOracle;
class SmallMapList;
class CompilationDependencies;

namespace compiler {

// Stores type feedback information for nodes in the graph in a separate
// data structure.
class JSTypeFeedbackTable : public ZoneObject {
 public:
  explicit JSTypeFeedbackTable(Zone* zone);

  // TODO(titzer): support recording the feedback vector slot.

  void Record(Node* node, TypeFeedbackId id);

 private:
  friend class JSTypeFeedbackSpecializer;
  typedef std::map<NodeId, TypeFeedbackId, std::less<NodeId>,
                   zone_allocator<TypeFeedbackId> > TypeFeedbackIdMap;

  TypeFeedbackIdMap map_;

  TypeFeedbackId find(Node* node) {
    TypeFeedbackIdMap::const_iterator it = map_.find(node->id());
    return it == map_.end() ? TypeFeedbackId::None() : it->second;
  }
};


// Specializes a graph to the type feedback recorded in the
// {js_type_feedback} provided to the constructor.
class JSTypeFeedbackSpecializer : public Reducer {
 public:
  JSTypeFeedbackSpecializer(JSGraph* jsgraph,
                            JSTypeFeedbackTable* js_type_feedback,
                            TypeFeedbackOracle* oracle,
                            Handle<GlobalObject> global_object,
                            CompilationDependencies* dependencies)
      : jsgraph_(jsgraph),
        simplified_(jsgraph->graph()->zone()),
        js_type_feedback_(js_type_feedback),
        oracle_(oracle),
        global_object_(global_object),
        dependencies_(dependencies) {
    CHECK(js_type_feedback);
  }

  Reduction Reduce(Node* node) override;

  // Visible for unit testing.
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSLoadNamedForGlobalVariable(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreNamed(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);

 private:
  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
  JSTypeFeedbackTable* js_type_feedback_;
  TypeFeedbackOracle* oracle_;
  Handle<GlobalObject> global_object_;
  CompilationDependencies* dependencies_;

  TypeFeedbackOracle* oracle() { return oracle_; }
  Graph* graph() { return jsgraph_->graph(); }
  JSGraph* jsgraph() { return jsgraph_; }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  void BuildMapCheck(Node* receiver, Handle<Map> map, bool smi_check,
                     Node* effect, Node* control, Node** success, Node** fail);

  void GatherReceiverTypes(Node* receiver, Node* effect, TypeFeedbackId id,
                           Handle<Name> property, SmallMapList* maps);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
