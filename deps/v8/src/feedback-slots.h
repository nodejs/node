// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FEEDBACK_SLOTS_H_
#define V8_FEEDBACK_SLOTS_H_

#include "src/v8.h"

#include "src/isolate.h"

namespace v8 {
namespace internal {

class FeedbackSlotInterface {
 public:
  static const int kInvalidFeedbackSlot = -1;

  virtual ~FeedbackSlotInterface() {}

  virtual int ComputeFeedbackSlotCount() = 0;
  virtual void SetFirstFeedbackSlot(int slot) = 0;
};

} }  // namespace v8::internal

#endif  // V8_FEEDBACK_SLOTS_H_
