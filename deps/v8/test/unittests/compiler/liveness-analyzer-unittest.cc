// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/state-values-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::StringMatchResultListener;

namespace v8 {
namespace internal {
namespace compiler {

class LivenessAnalysisTest : public GraphTest {
 public:
  explicit LivenessAnalysisTest(int locals_count = 4)
      : locals_count_(locals_count),
        machine_(zone(), MachineRepresentation::kWord32),
        javascript_(zone()),
        jsgraph_(isolate(), graph(), common(), &javascript_, nullptr,
                 &machine_),
        analyzer_(locals_count, false, zone()),
        empty_values_(graph()->NewNode(
            common()->StateValues(0, SparseInputMask::Dense()), 0, nullptr)),
        next_checkpoint_id_(0),
        current_block_(nullptr) {}

 protected:
  JSGraph* jsgraph() { return &jsgraph_; }

  LivenessAnalyzer* analyzer() { return &analyzer_; }
  void Run() {
    StateValuesCache cache(jsgraph());
    NonLiveFrameStateSlotReplacer replacer(
        &cache, jsgraph()->UndefinedConstant(), analyzer()->local_count(),
        false, zone());
    analyzer()->Run(&replacer);
  }

  Node* Checkpoint() {
    int ast_num = next_checkpoint_id_++;
    int first_const = intconst_from_bailout_id(ast_num, locals_count_);

    const Operator* locals_op =
        common()->StateValues(locals_count_, SparseInputMask::Dense());

    ZoneVector<Node*> local_inputs(locals_count_, nullptr, zone());
    for (int i = 0; i < locals_count_; i++) {
      local_inputs[i] = jsgraph()->Int32Constant(i + first_const);
    }
    Node* locals =
        graph()->NewNode(locals_op, locals_count_, &local_inputs.front());

    const FrameStateFunctionInfo* state_info =
        common()->CreateFrameStateFunctionInfo(
            FrameStateType::kJavaScriptFunction, 0, locals_count_,
            Handle<SharedFunctionInfo>());

    const Operator* op = common()->FrameState(
        BailoutId(ast_num), OutputFrameStateCombine::Ignore(), state_info);
    Node* result =
        graph()->NewNode(op, empty_values_, locals, empty_values_,
                         jsgraph()->UndefinedConstant(),
                         jsgraph()->UndefinedConstant(), graph()->start());

    current_block_->Checkpoint(result);
    return result;
  }

  void Bind(int var) { current_block()->Bind(var); }
  void Lookup(int var) { current_block()->Lookup(var); }

  class CheckpointMatcher : public MatcherInterface<Node*> {
   public:
    explicit CheckpointMatcher(const char* liveness, Node* empty_values,
                               int locals_count, Node* replacement)
        : liveness_(liveness),
          empty_values_(empty_values),
          locals_count_(locals_count),
          replacement_(replacement) {}

    void DescribeTo(std::ostream* os) const override {
      *os << "is a frame state with '" << liveness_
          << "' liveness, empty "
             "parameters and empty expression stack";
    }

    bool MatchAndExplain(Node* frame_state,
                         MatchResultListener* listener) const override {
      if (frame_state == NULL) {
        *listener << "which is NULL";
        return false;
      }
      DCHECK(frame_state->opcode() == IrOpcode::kFrameState);

      FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
      int ast_num = state_info.bailout_id().ToInt();
      int first_const = intconst_from_bailout_id(ast_num, locals_count_);

      if (empty_values_ != frame_state->InputAt(0)) {
        *listener << "whose parameters are " << frame_state->InputAt(0)
                  << " but should have been " << empty_values_ << " (empty)";
        return false;
      }
      if (empty_values_ != frame_state->InputAt(2)) {
        *listener << "whose expression stack is " << frame_state->InputAt(2)
                  << " but should have been " << empty_values_ << " (empty)";
        return false;
      }
      StateValuesAccess locals(frame_state->InputAt(1));
      if (locals_count_ != static_cast<int>(locals.size())) {
        *listener << "whose number of locals is " << locals.size()
                  << " but should have been " << locals_count_;
        return false;
      }
      int i = 0;
      for (StateValuesAccess::TypedNode value : locals) {
        if (liveness_[i] == 'L') {
          StringMatchResultListener value_listener;
          if (value.node == replacement_) {
            *listener << "whose local #" << i << " was " << value.node->opcode()
                      << " but should have been 'undefined'";
            return false;
          } else if (!IsInt32Constant(first_const + i)
                          .MatchAndExplain(value.node, &value_listener)) {
            *listener << "whose local #" << i << " does not match";
            if (value_listener.str() != "") {
              *listener << ", " << value_listener.str();
            }
            return false;
          }
        } else if (liveness_[i] == '.') {
          if (value.node != replacement_) {
            *listener << "whose local #" << i << " is " << value.node
                      << " but should have been " << replacement_
                      << " (undefined)";
            return false;
          }
        } else {
          UNREACHABLE();
        }
        i++;
      }
      return true;
    }

   private:
    const char* liveness_;
    Node* empty_values_;
    int locals_count_;
    Node* replacement_;
  };

  Matcher<Node*> IsCheckpointModuloLiveness(const char* liveness) {
    return MakeMatcher(new CheckpointMatcher(liveness, empty_values_,
                                             locals_count_,
                                             jsgraph()->UndefinedConstant()));
  }

  LivenessAnalyzerBlock* current_block() { return current_block_; }
  void set_current_block(LivenessAnalyzerBlock* block) {
    current_block_ = block;
  }

 private:
  static int intconst_from_bailout_id(int ast_num, int locals_count) {
    return (locals_count + 1) * ast_num + 1;
  }

  int locals_count_;
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  JSGraph jsgraph_;
  LivenessAnalyzer analyzer_;
  Node* empty_values_;
  int next_checkpoint_id_;
  LivenessAnalyzerBlock* current_block_;
};


TEST_F(LivenessAnalysisTest, EmptyBlock) {
  set_current_block(analyzer()->NewBlock());

  Node* c1 = Checkpoint();

  Run();

  // Nothing is live.
  EXPECT_THAT(c1, IsCheckpointModuloLiveness("...."));
}


TEST_F(LivenessAnalysisTest, SimpleLookup) {
  set_current_block(analyzer()->NewBlock());

  Node* c1 = Checkpoint();
  Lookup(1);
  Node* c2 = Checkpoint();

  Run();

  EXPECT_THAT(c1, IsCheckpointModuloLiveness(".L.."));
  EXPECT_THAT(c2, IsCheckpointModuloLiveness("...."));
}


TEST_F(LivenessAnalysisTest, DiamondLookups) {
  // Start block.
  LivenessAnalyzerBlock* start = analyzer()->NewBlock();
  set_current_block(start);
  Node* c1_start = Checkpoint();

  // First branch.
  LivenessAnalyzerBlock* b1 = analyzer()->NewBlock(start);
  set_current_block(b1);

  Node* c1_b1 = Checkpoint();
  Lookup(1);
  Node* c2_b1 = Checkpoint();
  Lookup(3);
  Node* c3_b1 = Checkpoint();

  // Second branch.
  LivenessAnalyzerBlock* b2 = analyzer()->NewBlock(start);
  set_current_block(b2);

  Node* c1_b2 = Checkpoint();
  Lookup(3);
  Node* c2_b2 = Checkpoint();
  Lookup(2);
  Node* c3_b2 = Checkpoint();

  // Merge block.
  LivenessAnalyzerBlock* m = analyzer()->NewBlock(b1);
  m->AddPredecessor(b2);
  set_current_block(m);
  Node* c1_m = Checkpoint();
  Lookup(0);
  Node* c2_m = Checkpoint();

  Run();

  EXPECT_THAT(c1_start, IsCheckpointModuloLiveness("LLLL"));

  EXPECT_THAT(c1_b1, IsCheckpointModuloLiveness("LL.L"));
  EXPECT_THAT(c2_b1, IsCheckpointModuloLiveness("L..L"));
  EXPECT_THAT(c3_b1, IsCheckpointModuloLiveness("L..."));

  EXPECT_THAT(c1_b2, IsCheckpointModuloLiveness("L.LL"));
  EXPECT_THAT(c2_b2, IsCheckpointModuloLiveness("L.L."));
  EXPECT_THAT(c3_b2, IsCheckpointModuloLiveness("L..."));

  EXPECT_THAT(c1_m, IsCheckpointModuloLiveness("L..."));
  EXPECT_THAT(c2_m, IsCheckpointModuloLiveness("...."));
}


TEST_F(LivenessAnalysisTest, DiamondLookupsAndBinds) {
  // Start block.
  LivenessAnalyzerBlock* start = analyzer()->NewBlock();
  set_current_block(start);
  Node* c1_start = Checkpoint();
  Bind(0);
  Node* c2_start = Checkpoint();

  // First branch.
  LivenessAnalyzerBlock* b1 = analyzer()->NewBlock(start);
  set_current_block(b1);

  Node* c1_b1 = Checkpoint();
  Bind(2);
  Bind(1);
  Node* c2_b1 = Checkpoint();
  Bind(3);
  Node* c3_b1 = Checkpoint();

  // Second branch.
  LivenessAnalyzerBlock* b2 = analyzer()->NewBlock(start);
  set_current_block(b2);

  Node* c1_b2 = Checkpoint();
  Lookup(2);
  Node* c2_b2 = Checkpoint();
  Bind(2);
  Bind(3);
  Node* c3_b2 = Checkpoint();

  // Merge block.
  LivenessAnalyzerBlock* m = analyzer()->NewBlock(b1);
  m->AddPredecessor(b2);
  set_current_block(m);
  Node* c1_m = Checkpoint();
  Lookup(0);
  Lookup(1);
  Lookup(2);
  Lookup(3);
  Node* c2_m = Checkpoint();

  Run();

  EXPECT_THAT(c1_start, IsCheckpointModuloLiveness(".LL."));
  EXPECT_THAT(c2_start, IsCheckpointModuloLiveness("LLL."));

  EXPECT_THAT(c1_b1, IsCheckpointModuloLiveness("L..."));
  EXPECT_THAT(c2_b1, IsCheckpointModuloLiveness("LLL."));
  EXPECT_THAT(c3_b1, IsCheckpointModuloLiveness("LLLL"));

  EXPECT_THAT(c1_b2, IsCheckpointModuloLiveness("LLL."));
  EXPECT_THAT(c2_b2, IsCheckpointModuloLiveness("LL.."));
  EXPECT_THAT(c3_b2, IsCheckpointModuloLiveness("LLLL"));

  EXPECT_THAT(c1_m, IsCheckpointModuloLiveness("LLLL"));
  EXPECT_THAT(c2_m, IsCheckpointModuloLiveness("...."));
}


TEST_F(LivenessAnalysisTest, SimpleLoop) {
  // Start block.
  LivenessAnalyzerBlock* start = analyzer()->NewBlock();
  set_current_block(start);
  Node* c1_start = Checkpoint();
  Bind(0);
  Bind(1);
  Bind(2);
  Bind(3);
  Node* c2_start = Checkpoint();

  // Loop header block.
  LivenessAnalyzerBlock* header = analyzer()->NewBlock(start);
  set_current_block(header);
  Node* c1_header = Checkpoint();
  Lookup(0);
  Bind(2);
  Node* c2_header = Checkpoint();

  // Inside-loop block.
  LivenessAnalyzerBlock* in_loop = analyzer()->NewBlock(header);
  set_current_block(in_loop);
  Node* c1_in_loop = Checkpoint();
  Bind(0);
  Lookup(3);
  Node* c2_in_loop = Checkpoint();

  // Add back edge.
  header->AddPredecessor(in_loop);

  // After-loop block.
  LivenessAnalyzerBlock* end = analyzer()->NewBlock(header);
  set_current_block(end);
  Node* c1_end = Checkpoint();
  Lookup(1);
  Lookup(2);
  Node* c2_end = Checkpoint();

  Run();

  EXPECT_THAT(c1_start, IsCheckpointModuloLiveness("...."));
  EXPECT_THAT(c2_start, IsCheckpointModuloLiveness("LL.L"));

  EXPECT_THAT(c1_header, IsCheckpointModuloLiveness("LL.L"));
  EXPECT_THAT(c2_header, IsCheckpointModuloLiveness(".LLL"));

  EXPECT_THAT(c1_in_loop, IsCheckpointModuloLiveness(".L.L"));
  EXPECT_THAT(c2_in_loop, IsCheckpointModuloLiveness("LL.L"));

  EXPECT_THAT(c1_end, IsCheckpointModuloLiveness(".LL."));
  EXPECT_THAT(c2_end, IsCheckpointModuloLiveness("...."));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
