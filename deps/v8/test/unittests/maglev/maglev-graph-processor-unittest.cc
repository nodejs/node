// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/maglev/maglev-graph-processor.h"

#include <vector>

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "test/unittests/maglev/maglev-test.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphProcessorTest : public TestWithNativeContextAndZone {};

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
  EXPECT_EQ(recorder.log[recorder.log.size() - 1], "First::PostProcessGraph");
  persistent_scope.Detach();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
