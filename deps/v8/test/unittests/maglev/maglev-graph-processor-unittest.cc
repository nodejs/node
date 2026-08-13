// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/maglev/maglev-graph-processor.h"

#include <vector>

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-optimizer.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "test/unittests/maglev/maglev-test.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphProcessorTest : public TestWithNativeContextAndZone {
 public:
  template <typename Function>
  void RunSubgraphTest(Function&& body) {
    i::v8_flags.allow_natives_syntax = true;
    HandleScope scope(isolate());
    const char* script = R"(
      function f(a) { return a; }
      %PrepareFunctionForOptimization(f);
      f(1);
      (f)
    )";
    Handle<JSFunction> function = RunJS<JSFunction>(script);
    auto info =
        MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
    Graph* graph = Graph::New(info.get());
    compiler::CurrentHeapBrokerScope current_broker(info->broker());
    MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                     info->toplevel_compilation_unit(), graph);
    PersistentHandlesScope persistent_scope(isolate());
    ASSERT_TRUE(graph_builder.Build());

    ReachableExceptionHandlerTracker tracker(graph);
    RecomputeKnownNodeAspectsProcessor kna_processor(graph, tracker);
    MaglevGraphOptimizer optimizer(graph, kna_processor);
    MaglevReducer<MaglevGraphOptimizer> reducer(
        &optimizer, graph, info->toplevel_compilation_unit());

    ASSERT_FALSE(graph->blocks().empty());
    BasicBlock* block = graph->blocks()[0];
    reducer.set_current_block(block);
    reducer.set_known_node_aspects(zone()->New<KnownNodeAspects>(zone()));

    body(reducer, block);

    persistent_scope.Detach();
  }
};

namespace {

struct EventRecorder {
  std::vector<std::string> log;
};

class FirstBackwardProcessor {
 public:
  explicit FirstBackwardProcessor(EventRecorder* recorder)
      : recorder_(recorder) {}

  void PreProcessGraph(Graph* graph) {
    recorder_->log.push_back("First::PreProcessGraph");
  }
  void PostProcessGraph(Graph* graph) {
    recorder_->log.push_back("First::PostProcessGraph");
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    recorder_->log.push_back("First::PreProcessBasicBlock");
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    recorder_->log.push_back("First::PostProcessBasicBlock");
    return BlockProcessResult::kContinue;
  }
  template <typename NodeT>
  ProcessResult Process(NodeT* node) {
    recorder_->log.push_back("First::Process");
    return ProcessResult::kContinue;
  }

 private:
  EventRecorder* recorder_;
};

class SecondBackwardProcessor {
 public:
  explicit SecondBackwardProcessor(EventRecorder* recorder)
      : recorder_(recorder) {}

  void PreProcessGraph(Graph* graph) {
    recorder_->log.push_back("Second::PreProcessGraph");
  }
  void PostProcessGraph(Graph* graph) {
    recorder_->log.push_back("Second::PostProcessGraph");
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    recorder_->log.push_back("Second::PreProcessBasicBlock");
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    recorder_->log.push_back("Second::PostProcessBasicBlock");
    return BlockProcessResult::kContinue;
  }
  template <typename NodeT>
  ProcessResult Process(NodeT* node) {
    recorder_->log.push_back("Second::Process");
    return ProcessResult::kContinue;
  }

 private:
  EventRecorder* recorder_;
};

}  // namespace

TEST_F(MaglevGraphProcessorTest, GraphBackwardMultiProcessorOrder) {
  i::v8_flags.allow_natives_syntax = true;
  HandleScope scope(isolate());
  const char* script = R"(
    function f(a, b) { return a + b; }
    %PrepareFunctionForOptimization(f);
    f(1, 2);
    (f)
  )";
  Handle<JSFunction> function = RunJS<JSFunction>(script);
  auto info =
      MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
  Graph* graph = Graph::New(info.get());
  compiler::CurrentHeapBrokerScope current_broker(info->broker());
  MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                   info->toplevel_compilation_unit(), graph);
  PersistentHandlesScope persistent_scope(isolate());
  ASSERT_TRUE(graph_builder.Build());

  EventRecorder recorder;
  GraphBackwardMultiProcessor<FirstBackwardProcessor, SecondBackwardProcessor>
      processor(FirstBackwardProcessor{&recorder},
                SecondBackwardProcessor{&recorder});
  processor.ProcessGraph(graph);

  ASSERT_FALSE(recorder.log.empty());
  // PreProcessGraph should run First then Second.
  EXPECT_EQ(recorder.log[0], "First::PreProcessGraph");
  EXPECT_EQ(recorder.log[1], "Second::PreProcessGraph");
  // PostProcessGraph should run Second then First (reverse order).
  EXPECT_EQ(recorder.log[recorder.log.size() - 2], "Second::PostProcessGraph");
  persistent_scope.Detach();
}

TEST_F(MaglevGraphProcessorTest, SubgraphBufferStashingNodesFromBefore) {
  RunSubgraphTest(
      [](MaglevReducer<MaglevGraphOptimizer>& reducer, BasicBlock* block) {
        reducer.SetNewNodePosition(BasicBlockPosition::End());
        (void)reducer.AddNewNode<Float64Constant>({}, 1.0);

        size_t original_nodes_size = block->nodes().size();

        {
          // Top-level Subgraph directly instantiated in C++. Stashes buffered
          // nodes.
          Subgraph<MaglevGraphOptimizer> top_sg(&reducer, 0);
          EXPECT_EQ(original_nodes_size, block->nodes().size());

          {
            // Nested Subgraph directly instantiated in C++.
            Subgraph<MaglevGraphOptimizer> nested_sg(&reducer, 0);
          }
        }

        // Destruction of empty top-level Subgraph leaves block->nodes()
        // unmutated.
        EXPECT_EQ(original_nodes_size, block->nodes().size());

        reducer.FlushNodesToBlock();
      });
}

TEST_F(MaglevGraphProcessorTest, SubgraphBufferStashingNestedParentAndChild) {
  RunSubgraphTest(
      [](MaglevReducer<MaglevGraphOptimizer>& reducer, BasicBlock* block) {
        {
          Subgraph<MaglevGraphOptimizer> parent_sg(&reducer, 0);
          (void)reducer.AddNewNode<Float64Constant>({}, 2.0);

          {
            Subgraph<MaglevGraphOptimizer> child_sg(&reducer, 0);
            (void)reducer.AddNewNode<Float64Constant>({}, 3.0);
          }
        }

        // Non-empty subgraphs recorded pending splices cleanly.
        EXPECT_TRUE(reducer.HasPendingSplice());

        reducer.FlushNodesToBlock();
      });
}

TEST_F(MaglevGraphProcessorTest, SubgraphEmptyTopLevelWithNestedNonEmpty) {
  RunSubgraphTest(
      [](MaglevReducer<MaglevGraphOptimizer>& reducer, BasicBlock* block) {
        // Emit node before top_sg.
        (void)reducer.AddNewNode<Float64Constant>({}, 1.0);
        size_t original_nodes_size = block->nodes().size();

        {
          Subgraph<MaglevGraphOptimizer> top_sg(&reducer, 0);

          {
            Subgraph<MaglevGraphOptimizer> child_sg(&reducer, 0);
            (void)reducer.AddNewNode<Float64Constant>({}, 4.0);
          }
        }

        // Destruction of top_sg restored outer node buffer and flushed it
        // during splice creation.
        EXPECT_EQ(original_nodes_size + 1, block->nodes().size());
        EXPECT_TRUE(reducer.HasPendingSplice());

        reducer.FlushNodesToBlock();
      });
}

TEST_F(MaglevGraphProcessorTest, SubgraphAbruptExit) {
  RunSubgraphTest(
      [](MaglevReducer<MaglevGraphOptimizer>& reducer, BasicBlock* block) {
        {
          Subgraph<MaglevGraphOptimizer> sg(&reducer, 0);
          (void)reducer.AddNewNode<Float64Constant>({}, 5.0);
          // Abrupt exit path (e.g. Throw/Deopt) flushes nodes before setting
          // current_block to null.
          reducer.FlushNodesToBlock();
          reducer.set_current_block(nullptr);
        }

        // Destructor of abrupt exit subgraph completes cleanly.
        EXPECT_EQ(block, reducer.current_block());
      });
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
