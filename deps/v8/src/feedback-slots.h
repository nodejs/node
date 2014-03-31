// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_FEEDBACK_SLOTS_H_
#define V8_FEEDBACK_SLOTS_H_

#include "v8.h"

#include "isolate.h"

namespace v8 {
namespace internal {

enum ComputablePhase {
  DURING_PARSE,
  AFTER_SCOPING
};


class FeedbackSlotInterface {
 public:
  static const int kInvalidFeedbackSlot = -1;

  virtual ~FeedbackSlotInterface() {}

  // When can we ask how many feedback slots are necessary?
  virtual ComputablePhase GetComputablePhase() = 0;
  virtual int ComputeFeedbackSlotCount(Isolate* isolate) = 0;
  virtual void SetFirstFeedbackSlot(int slot) = 0;
};


class DeferredFeedbackSlotProcessor {
 public:
  DeferredFeedbackSlotProcessor()
    : slot_nodes_(NULL),
      slot_count_(0) { }

  void add_slot_node(Zone* zone, FeedbackSlotInterface* slot) {
    if (slot->GetComputablePhase() == DURING_PARSE) {
      // No need to add to the list
      int count = slot->ComputeFeedbackSlotCount(zone->isolate());
      slot->SetFirstFeedbackSlot(slot_count_);
      slot_count_ += count;
    } else {
      if (slot_nodes_ == NULL) {
        slot_nodes_ = new(zone) ZoneList<FeedbackSlotInterface*>(10, zone);
      }
      slot_nodes_->Add(slot, zone);
    }
  }

  void ProcessFeedbackSlots(Isolate* isolate) {
    // Scope analysis must have been done.
    if (slot_nodes_ == NULL) {
      return;
    }

    int current_slot = slot_count_;
    for (int i = 0; i < slot_nodes_->length(); i++) {
      FeedbackSlotInterface* slot_interface = slot_nodes_->at(i);
      int count = slot_interface->ComputeFeedbackSlotCount(isolate);
      if (count > 0) {
        slot_interface->SetFirstFeedbackSlot(current_slot);
        current_slot += count;
      }
    }

    slot_count_ = current_slot;
    slot_nodes_->Clear();
  }

  int slot_count() {
    ASSERT(slot_count_ >= 0);
    return slot_count_;
  }

 private:
  ZoneList<FeedbackSlotInterface*>* slot_nodes_;
  int slot_count_;
};


} }  // namespace v8::internal

#endif  // V8_FEEDBACK_SLOTS_H_
