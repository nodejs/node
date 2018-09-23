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

  void Record(Node* node, TypeFeedbackId id);
  void Record(Node* node, FeedbackVectorICSlot slot);

 private:
  friend class JSTypeFeedbackSpecializer;
  typedef std::map<NodeId, TypeFeedbackId, std::less<NodeId>,
                   zone_allocator<TypeFeedbackId> > TypeFeedbackIdMap;
  typedef std::map<NodeId, FeedbackVectorICSlot, std::less<NodeId>,
                   zone_allocator<FeedbackVectorICSlot> >
      FeedbackVectorICSlotMap;

  TypeFeedbackIdMap type_feedback_id_map_;
  FeedbackVectorICSlotMap feedback_vector_ic_slot_map_;

  TypeFeedbackId FindTypeFeedbackId(Node* node) {
    TypeFeedbackIdMap::const_iterator it =
        type_feedback_id_map_.find(node->id());
    return it == type_feedback_id_map_.end() ? TypeFeedbackId::None()
                                             : it->second;
  }

  FeedbackVectorICSlot FindFeedbackVectorICSlot(Node* node) {
    FeedbackVectorICSlotMap::const_iterator it =
        feedback_vector_ic_slot_map_.find(node->id());
    return it == feedback_vector_ic_slot_map_.end()
               ? FeedbackVectorICSlot::Invalid()
               : it->second;
  }
};


// Specializes a graph to the type feedback recorded in the
// {js_type_feedback} provided to the constructor.
class JSTypeFeedbackSpecializer : public AdvancedReducer {
 public:
  enum DeoptimizationMode { kDeoptimizationEnabled, kDeoptimizationDisabled };

  JSTypeFeedbackSpecializer(Editor* editor, JSGraph* jsgraph,
                            JSTypeFeedbackTable* js_type_feedback,
                            TypeFeedbackOracle* oracle,
                            Handle<GlobalObject> global_object,
                            DeoptimizationMode mode,
                            CompilationDependencies* dependencies)
      : AdvancedReducer(editor),
        jsgraph_(jsgraph),
        simplified_(jsgraph->graph()->zone()),
        js_type_feedback_(js_type_feedback),
        oracle_(oracle),
        global_object_(global_object),
        mode_(mode),
        dependencies_(dependencies) {
    CHECK_NOT_NULL(js_type_feedback);
  }

  Reduction Reduce(Node* node) override;

  // Visible for unit testing.
  Reduction ReduceJSLoadGlobal(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreNamed(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);

 private:
  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
  JSTypeFeedbackTable* js_type_feedback_;
  TypeFeedbackOracle* oracle_;
  Handle<GlobalObject> global_object_;
  DeoptimizationMode const mode_;
  CompilationDependencies* dependencies_;

  TypeFeedbackOracle* oracle() { return oracle_; }
  Graph* graph() { return jsgraph_->graph(); }
  JSGraph* jsgraph() { return jsgraph_; }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  DeoptimizationMode mode() const { return mode_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  void BuildMapCheck(Node* receiver, Handle<Map> map, bool smi_check,
                     Node* effect, Node* control, Node** success, Node** fail);

  Node* GetFrameStateBefore(Node* node);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
