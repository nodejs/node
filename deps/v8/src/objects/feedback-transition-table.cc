// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/globals.h"

namespace v8 {
namespace internal {

inline const CompareOperationFeedback::TransitionMap
    CompareOperationFeedback::kTransitionMap =
        CompareOperationFeedback::BuildTransitionMap();

inline const CompareOperationFeedback::FeedbackEncodeTable
    CompareOperationFeedback::kFeedbackEncodeTable =
        CompareOperationFeedback::BuildFeedbackEncodeTable();

// static
Address CompareOperationFeedback::GetTransitionMapAddress() {
  return reinterpret_cast<Address>(kTransitionMap.map);
}

// static
Address CompareOperationFeedback::GetFeedbackEncodeTableAddress() {
  return reinterpret_cast<Address>(kFeedbackEncodeTable.lut);
}

inline const BinaryOperationFeedback::TransitionMap
    BinaryOperationFeedback::kTransitionMap =
        BinaryOperationFeedback::BuildTransitionMap();

inline const BinaryOperationFeedback::FeedbackEncodeTable
    BinaryOperationFeedback::kFeedbackEncodeTable =
        BinaryOperationFeedback::BuildFeedbackEncodeTable();

// static
Address BinaryOperationFeedback::GetTransitionMapAddress() {
  return reinterpret_cast<Address>(kTransitionMap.map);
}

// static
Address BinaryOperationFeedback::GetFeedbackEncodeTableAddress() {
  return reinterpret_cast<Address>(kFeedbackEncodeTable.lut);
}

}  // namespace internal
}  // namespace v8
