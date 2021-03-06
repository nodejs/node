// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/redundancy-elimination.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/feedback-source.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::NiceMock;

namespace v8 {
namespace internal {
namespace compiler {
namespace redundancy_elimination_unittest {

class RedundancyEliminationTest : public GraphTest {
 public:
  explicit RedundancyEliminationTest(int num_parameters = 4)
      : GraphTest(num_parameters),
        reducer_(&editor_, zone()),
        simplified_(zone()) {
    // Initialize the {reducer_} state for the Start node.
    reducer_.Reduce(graph()->start());

    // Create a feedback vector with two CALL_IC slots.
    FeedbackVectorSpec spec(zone());
    FeedbackSlot slot1 = spec.AddCallICSlot();
    FeedbackSlot slot2 = spec.AddCallICSlot();
    Handle<FeedbackMetadata> metadata = FeedbackMetadata::New(isolate(), &spec);
    Handle<SharedFunctionInfo> shared =
        isolate()->factory()->NewSharedFunctionInfoForBuiltin(
            isolate()->factory()->empty_string(), Builtins::kIllegal);
    shared->set_raw_outer_scope_info_or_feedback_metadata(*metadata);
    Handle<ClosureFeedbackCellArray> closure_feedback_cell_array =
        ClosureFeedbackCellArray::New(isolate(), shared);
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate()));
    Handle<FeedbackVector> feedback_vector = FeedbackVector::New(
        isolate(), shared, closure_feedback_cell_array, &is_compiled_scope);
    vector_slot_pairs_.push_back(FeedbackSource());
    vector_slot_pairs_.push_back(FeedbackSource(feedback_vector, slot1));
    vector_slot_pairs_.push_back(FeedbackSource(feedback_vector, slot2));
  }
  ~RedundancyEliminationTest() override = default;

 protected:
  Reduction Reduce(Node* node) { return reducer_.Reduce(node); }

  std::vector<FeedbackSource> const& vector_slot_pairs() const {
    return vector_slot_pairs_;
  }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  NiceMock<MockAdvancedReducerEditor> editor_;
  std::vector<FeedbackSource> vector_slot_pairs_;
  FeedbackSource feedback2_;
  RedundancyElimination reducer_;
  SimplifiedOperatorBuilder simplified_;
};

namespace {

const CheckForMinusZeroMode kCheckForMinusZeroModes[] = {
    CheckForMinusZeroMode::kCheckForMinusZero,
    CheckForMinusZeroMode::kDontCheckForMinusZero,
};

const CheckTaggedInputMode kCheckTaggedInputModes[] = {
    CheckTaggedInputMode::kNumber, CheckTaggedInputMode::kNumberOrOddball};

const NumberOperationHint kNumberOperationHints[] = {
    NumberOperationHint::kSignedSmall,
    NumberOperationHint::kSignedSmallInputs,
    NumberOperationHint::kNumber,
    NumberOperationHint::kNumberOrOddball,
};

}  // namespace

// -----------------------------------------------------------------------------
// CheckBounds

TEST_F(RedundancyEliminationTest, CheckBounds) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* index = Parameter(0);
      Node* length = Parameter(1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), index, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), index, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckNumber

TEST_F(RedundancyEliminationTest, CheckNumberSubsumedByCheckSmi) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckSmi(feedback1), value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckNumber(feedback2), value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckReceiver

TEST_F(RedundancyEliminationTest, CheckReceiver) {
  Node* value = Parameter(0);
  Node* effect = graph()->start();
  Node* control = graph()->start();

  Node* check1 = effect =
      graph()->NewNode(simplified()->CheckReceiver(), value, effect, control);
  Reduction r1 = Reduce(check1);
  ASSERT_TRUE(r1.Changed());
  EXPECT_EQ(r1.replacement(), check1);

  Node* check2 = effect =
      graph()->NewNode(simplified()->CheckReceiver(), value, effect, control);
  Reduction r2 = Reduce(check2);
  ASSERT_TRUE(r2.Changed());
  EXPECT_EQ(r2.replacement(), check1);
}

// -----------------------------------------------------------------------------
// CheckReceiverOrNullOrUndefined

TEST_F(RedundancyEliminationTest, CheckReceiverOrNullOrUndefined) {
  Node* value = Parameter(0);
  Node* effect = graph()->start();
  Node* control = graph()->start();

  Node* check1 = effect = graph()->NewNode(
      simplified()->CheckReceiverOrNullOrUndefined(), value, effect, control);
  Reduction r1 = Reduce(check1);
  ASSERT_TRUE(r1.Changed());
  EXPECT_EQ(r1.replacement(), check1);

  Node* check2 = effect = graph()->NewNode(
      simplified()->CheckReceiverOrNullOrUndefined(), value, effect, control);
  Reduction r2 = Reduce(check2);
  ASSERT_TRUE(r2.Changed());
  EXPECT_EQ(r2.replacement(), check1);
}

TEST_F(RedundancyEliminationTest,
       CheckReceiverOrNullOrUndefinedSubsumedByCheckReceiver) {
  Node* value = Parameter(0);
  Node* effect = graph()->start();
  Node* control = graph()->start();

  Node* check1 = effect =
      graph()->NewNode(simplified()->CheckReceiver(), value, effect, control);
  Reduction r1 = Reduce(check1);
  ASSERT_TRUE(r1.Changed());
  EXPECT_EQ(r1.replacement(), check1);

  Node* check2 = effect = graph()->NewNode(
      simplified()->CheckReceiverOrNullOrUndefined(), value, effect, control);
  Reduction r2 = Reduce(check2);
  ASSERT_TRUE(r2.Changed());
  EXPECT_EQ(r2.replacement(), check1);
}

// -----------------------------------------------------------------------------
// CheckString

TEST_F(RedundancyEliminationTest,
       CheckStringSubsumedByCheckInternalizedString) {
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    Node* value = Parameter(0);
    Node* effect = graph()->start();
    Node* control = graph()->start();

    Node* check1 = effect = graph()->NewNode(
        simplified()->CheckInternalizedString(), value, effect, control);
    Reduction r1 = Reduce(check1);
    ASSERT_TRUE(r1.Changed());
    EXPECT_EQ(r1.replacement(), check1);

    Node* check2 = effect = graph()->NewNode(
        simplified()->CheckString(feedback), value, effect, control);
    Reduction r2 = Reduce(check2);
    ASSERT_TRUE(r2.Changed());
    EXPECT_EQ(r2.replacement(), check1);
  }
}

// -----------------------------------------------------------------------------
// CheckSymbol

TEST_F(RedundancyEliminationTest, CheckSymbol) {
  Node* value = Parameter(0);
  Node* effect = graph()->start();
  Node* control = graph()->start();

  Node* check1 = effect =
      graph()->NewNode(simplified()->CheckSymbol(), value, effect, control);
  Reduction r1 = Reduce(check1);
  ASSERT_TRUE(r1.Changed());
  EXPECT_EQ(r1.replacement(), check1);

  Node* check2 = effect =
      graph()->NewNode(simplified()->CheckSymbol(), value, effect, control);
  Reduction r2 = Reduce(check2);
  ASSERT_TRUE(r2.Changed());
  EXPECT_EQ(r2.replacement(), check1);
}

// -----------------------------------------------------------------------------
// CheckedFloat64ToInt32

TEST_F(RedundancyEliminationTest, CheckedFloat64ToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckForMinusZeroMode, mode, kCheckForMinusZeroModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedFloat64ToInt32(mode, feedback1), value, effect,
            control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedFloat64ToInt32(mode, feedback2), value, effect,
            control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedFloat64ToInt64

TEST_F(RedundancyEliminationTest, CheckedFloat64ToInt64) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckForMinusZeroMode, mode, kCheckForMinusZeroModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedFloat64ToInt64(mode, feedback1), value, effect,
            control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedFloat64ToInt64(mode, feedback2), value, effect,
            control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedInt32ToTaggedSigned

TEST_F(RedundancyEliminationTest, CheckedInt32ToTaggedSigned) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedInt32ToTaggedSigned(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedInt32ToTaggedSigned(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedInt64ToInt32

TEST_F(RedundancyEliminationTest, CheckedInt64ToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckedInt64ToInt32(feedback1), value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckedInt64ToInt32(feedback2), value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedInt64ToTaggedSigned

TEST_F(RedundancyEliminationTest, CheckedInt64ToTaggedSigned) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedInt64ToTaggedSigned(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedInt64ToTaggedSigned(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedSignedToInt32

TEST_F(RedundancyEliminationTest, CheckedTaggedSignedToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedTaggedSignedToInt32(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedTaggedSignedToInt32(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedToFloat64

TEST_F(RedundancyEliminationTest, CheckedTaggedToFloat64) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckTaggedInputMode, mode, kCheckTaggedInputModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToFloat64(mode, feedback1), value,
            effect, control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToFloat64(mode, feedback2), value,
            effect, control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

TEST_F(RedundancyEliminationTest,
       CheckedTaggedToFloat64SubsubmedByCheckedTaggedToFloat64) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      // If the check passed for CheckTaggedInputMode::kNumber, it'll
      // also pass later for CheckTaggedInputMode::kNumberOrOddball.
      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedTaggedToFloat64(
                               CheckTaggedInputMode::kNumber, feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckedTaggedToFloat64(
              CheckTaggedInputMode::kNumberOrOddball, feedback2),
          value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedToInt32

TEST_F(RedundancyEliminationTest, CheckedTaggedToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckForMinusZeroMode, mode, kCheckForMinusZeroModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToInt32(mode, feedback1), value, effect,
            control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToInt32(mode, feedback2), value, effect,
            control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

TEST_F(RedundancyEliminationTest,
       CheckedTaggedToInt32SubsumedByCheckedTaggedSignedToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckForMinusZeroMode, mode, kCheckForMinusZeroModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedTaggedSignedToInt32(feedback1), value, effect,
            control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToInt32(mode, feedback2), value, effect,
            control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedToInt64

TEST_F(RedundancyEliminationTest, CheckedTaggedToInt64) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckForMinusZeroMode, mode, kCheckForMinusZeroModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToInt64(mode, feedback1), value, effect,
            control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedTaggedToInt64(mode, feedback2), value, effect,
            control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedToTaggedPointer

TEST_F(RedundancyEliminationTest, CheckedTaggedToTaggedPointer) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckedTaggedToTaggedPointer(feedback1), value, effect,
          control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckedTaggedToTaggedPointer(feedback2), value, effect,
          control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTaggedToTaggedSigned

TEST_F(RedundancyEliminationTest, CheckedTaggedToTaggedSigned) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedTaggedToTaggedSigned(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedTaggedToTaggedSigned(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedTruncateTaggedToWord32

TEST_F(RedundancyEliminationTest, CheckedTruncateTaggedToWord32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(CheckTaggedInputMode, mode, kCheckTaggedInputModes) {
        Node* value = Parameter(0);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect = graph()->NewNode(
            simplified()->CheckedTruncateTaggedToWord32(mode, feedback1), value,
            effect, control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* check2 = effect = graph()->NewNode(
            simplified()->CheckedTruncateTaggedToWord32(mode, feedback2), value,
            effect, control);
        Reduction r2 = Reduce(check2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_EQ(r2.replacement(), check1);
      }
    }
  }
}

TEST_F(RedundancyEliminationTest,
       CheckedTruncateTaggedToWord32SubsumedByCheckedTruncateTaggedToWord32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      // If the check passed for CheckTaggedInputMode::kNumber, it'll
      // also pass later for CheckTaggedInputMode::kNumberOrOddball.
      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedTruncateTaggedToWord32(
                               CheckTaggedInputMode::kNumber, feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckedTruncateTaggedToWord32(
              CheckTaggedInputMode::kNumberOrOddball, feedback2),
          value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint32Bounds

TEST_F(RedundancyEliminationTest, CheckedUint32Bounds) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* index = Parameter(0);
      Node* length = Parameter(1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint32Bounds(feedback1, {}),
                           index, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint32Bounds(feedback2, {}),
                           index, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint32ToInt32

TEST_F(RedundancyEliminationTest, CheckedUint32ToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint32ToInt32(feedback1), value,
                           effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint32ToInt32(feedback2), value,
                           effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint32ToTaggedSigned

TEST_F(RedundancyEliminationTest, CheckedUint32ToTaggedSigned) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint32ToTaggedSigned(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint32ToTaggedSigned(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint64Bounds

TEST_F(RedundancyEliminationTest, CheckedUint64Bounds) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* index = Parameter(0);
      Node* length = Parameter(1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint64Bounds(feedback1, {}),
                           index, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint64Bounds(feedback2, {}),
                           index, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint64ToInt32

TEST_F(RedundancyEliminationTest, CheckedUint64ToInt32) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint64ToInt32(feedback1), value,
                           effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint64ToInt32(feedback2), value,
                           effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// CheckedUint64ToTaggedSigned

TEST_F(RedundancyEliminationTest, CheckedUint64ToTaggedSigned) {
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* value = Parameter(0);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect =
          graph()->NewNode(simplified()->CheckedUint64ToTaggedSigned(feedback1),
                           value, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect =
          graph()->NewNode(simplified()->CheckedUint64ToTaggedSigned(feedback2),
                           value, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check1);
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeNumberEqual

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberEqualWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberEqual(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberEqual(NumberOperationHint::kSignedSmall,
                                           check1, check2, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberEqualWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::UnsignedSmall(), 0);
      Node* rhs = Parameter(Type::UnsignedSmall(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberEqual(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberEqual(NumberOperationHint::kSignedSmall,
                                           lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeNumberLessThan

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberLessThanWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberLessThan(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberLessThan(NumberOperationHint::kSignedSmall,
                                              check1, check2, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberLessThanWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::UnsignedSmall(), 0);
      Node* rhs = Parameter(Type::UnsignedSmall(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberLessThan(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberLessThan(NumberOperationHint::kSignedSmall,
                                              lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeNumberLessThanOrEqual

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberLessThanOrEqualWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberLessThanOrEqual(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberLessThanOrEqual(
                      NumberOperationHint::kSignedSmall, check1, check2, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberLessThanOrEqualWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      Node* lhs = Parameter(Type::UnsignedSmall(), 0);
      Node* rhs = Parameter(Type::UnsignedSmall(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback1), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* check2 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback2), rhs, length, effect, control);
      Reduction r2 = Reduce(check2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_EQ(r2.replacement(), check2);

      Node* cmp3 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberLessThanOrEqual(
                               NumberOperationHint::kSignedSmall),
                           lhs, rhs, effect, control);
      Reduction r3 = Reduce(cmp3);
      ASSERT_TRUE(r3.Changed());
      EXPECT_THAT(r3.replacement(),
                  IsSpeculativeNumberLessThanOrEqual(
                      NumberOperationHint::kSignedSmall, lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeNumberAdd

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberAddWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* add2 = effect = graph()->NewNode(
          simplified()->SpeculativeNumberAdd(hint), lhs, rhs, effect, control);
      Reduction r2 = Reduce(add2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeNumberAdd(hint, check1, rhs, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest, SpeculativeNumberAddWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Range(42.0, 42.0, zone()), 0);
      Node* rhs = Parameter(Type::Any(), 0);
      Node* length = Parameter(Type::Unsigned31(), 1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* add2 = effect = graph()->NewNode(
          simplified()->SpeculativeNumberAdd(hint), lhs, rhs, effect, control);
      Reduction r2 = Reduce(add2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeNumberAdd(hint, lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeNumberSubtract

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberSubtractWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* subtract2 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberSubtract(hint), lhs,
                           rhs, effect, control);
      Reduction r2 = Reduce(subtract2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeNumberSubtract(hint, check1, rhs, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeNumberSubtractWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Range(42.0, 42.0, zone()), 0);
      Node* rhs = Parameter(Type::Any(), 0);
      Node* length = Parameter(Type::Unsigned31(), 1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* subtract2 = effect =
          graph()->NewNode(simplified()->SpeculativeNumberSubtract(hint), lhs,
                           rhs, effect, control);
      Reduction r2 = Reduce(subtract2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeNumberSubtract(hint, lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeSafeIntegerAdd

TEST_F(RedundancyEliminationTest,
       SpeculativeSafeIntegerAddWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* add2 = effect =
          graph()->NewNode(simplified()->SpeculativeSafeIntegerAdd(hint), lhs,
                           rhs, effect, control);
      Reduction r2 = Reduce(add2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeSafeIntegerAdd(hint, check1, rhs, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeSafeIntegerAddWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Range(42.0, 42.0, zone()), 0);
      Node* rhs = Parameter(Type::Any(), 0);
      Node* length = Parameter(Type::Unsigned31(), 1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* add2 = effect =
          graph()->NewNode(simplified()->SpeculativeSafeIntegerAdd(hint), lhs,
                           rhs, effect, control);
      Reduction r2 = Reduce(add2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeSafeIntegerAdd(hint, lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeSafeIntegerSubtract

TEST_F(RedundancyEliminationTest,
       SpeculativeSafeIntegerSubtractWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Any(), 0);
      Node* rhs = Parameter(Type::Any(), 1);
      Node* length = Parameter(Type::Unsigned31(), 2);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* subtract2 = effect =
          graph()->NewNode(simplified()->SpeculativeSafeIntegerSubtract(hint),
                           lhs, rhs, effect, control);
      Reduction r2 = Reduce(subtract2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeSafeIntegerSubtract(hint, check1, rhs, _, _));
    }
  }
}

TEST_F(RedundancyEliminationTest,
       SpeculativeSafeIntegerSubtractWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback, vector_slot_pairs()) {
    TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
      Node* lhs = Parameter(Type::Range(42.0, 42.0, zone()), 0);
      Node* rhs = Parameter(Type::Any(), 0);
      Node* length = Parameter(Type::Unsigned31(), 1);
      Node* effect = graph()->start();
      Node* control = graph()->start();

      Node* check1 = effect = graph()->NewNode(
          simplified()->CheckBounds(feedback), lhs, length, effect, control);
      Reduction r1 = Reduce(check1);
      ASSERT_TRUE(r1.Changed());
      EXPECT_EQ(r1.replacement(), check1);

      Node* subtract2 = effect =
          graph()->NewNode(simplified()->SpeculativeSafeIntegerSubtract(hint),
                           lhs, rhs, effect, control);
      Reduction r2 = Reduce(subtract2);
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsSpeculativeSafeIntegerSubtract(hint, lhs, rhs, _, _));
    }
  }
}

// -----------------------------------------------------------------------------
// SpeculativeToNumber

TEST_F(RedundancyEliminationTest,
       SpeculativeToNumberWithCheckBoundsBetterType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
        Node* index = Parameter(Type::Any(), 0);
        Node* length = Parameter(Type::Unsigned31(), 1);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect =
            graph()->NewNode(simplified()->CheckBounds(feedback1), index,
                             length, effect, control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* to_number2 = effect =
            graph()->NewNode(simplified()->SpeculativeToNumber(hint, feedback2),
                             index, effect, control);
        Reduction r2 = Reduce(to_number2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_THAT(r2.replacement(), IsSpeculativeToNumber(check1));
      }
    }
  }
}

TEST_F(RedundancyEliminationTest, SpeculativeToNumberWithCheckBoundsSameType) {
  Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
  TRACED_FOREACH(FeedbackSource, feedback1, vector_slot_pairs()) {
    TRACED_FOREACH(FeedbackSource, feedback2, vector_slot_pairs()) {
      TRACED_FOREACH(NumberOperationHint, hint, kNumberOperationHints) {
        Node* index = Parameter(Type::Range(42.0, 42.0, zone()), 0);
        Node* length = Parameter(Type::Unsigned31(), 1);
        Node* effect = graph()->start();
        Node* control = graph()->start();

        Node* check1 = effect =
            graph()->NewNode(simplified()->CheckBounds(feedback1), index,
                             length, effect, control);
        Reduction r1 = Reduce(check1);
        ASSERT_TRUE(r1.Changed());
        EXPECT_EQ(r1.replacement(), check1);

        Node* to_number2 = effect =
            graph()->NewNode(simplified()->SpeculativeToNumber(hint, feedback2),
                             index, effect, control);
        Reduction r2 = Reduce(to_number2);
        ASSERT_TRUE(r2.Changed());
        EXPECT_THAT(r2.replacement(), IsSpeculativeToNumber(index));
      }
    }
  }
}

}  // namespace redundancy_elimination_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
