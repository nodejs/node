/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_BINDER_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_BINDER_TRACKER_H_

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/destructible.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class BinderTracker : public Destructible {
 public:
  using SetArgsCallback = std::function<void(ArgsTracker::BoundInserter*)>;
  // Declared public for testing only.
  explicit BinderTracker(TraceProcessorContext*);
  BinderTracker(const BinderTracker&) = delete;
  BinderTracker& operator=(const BinderTracker&) = delete;
  ~BinderTracker() override;
  static BinderTracker* GetOrCreate(TraceProcessorContext* context) {
    if (!context->binder_tracker) {
      context->binder_tracker.reset(new BinderTracker(context));
    }
    return static_cast<BinderTracker*>(context->binder_tracker.get());
  }

  void Transaction(int64_t timestamp,
                   uint32_t tid,
                   int32_t transaction_id,
                   int32_t dest_node,
                   int32_t dest_tgid,
                   int32_t dest_tid,
                   bool is_reply,
                   uint32_t flags,
                   StringId code);
  void Locked(int64_t timestamp, uint32_t pid);
  void Lock(int64_t timestamp, uint32_t dest_pid);
  void Unlock(int64_t timestamp, uint32_t pid);
  void TransactionReceived(int64_t timestamp,
                           uint32_t tid,
                           int32_t transaction_id);
  void TransactionAllocBuf(int64_t timestamp,
                           uint32_t pid,
                           uint64_t data_size,
                           uint64_t offsets_size);

 private:
  TraceProcessorContext* const context_;
  std::unordered_set<int32_t> awaiting_rcv_for_reply_;

  std::unordered_set<int32_t> transaction_await_rcv;
  std::unordered_map<int32_t, SetArgsCallback> awaiting_async_rcv_;

  std::unordered_map<uint32_t, int64_t> attempt_lock_;

  std::unordered_map<uint32_t, int64_t> lock_acquired_;

  const StringId binder_category_id_;
  const StringId lock_waiting_id_;
  const StringId lock_held_id_;
  const StringId transaction_slice_id_;
  const StringId transaction_async_id_;
  const StringId reply_id_;
  const StringId async_rcv_id_;
  const StringId transaction_id_;
  const StringId dest_node_;
  const StringId dest_process_;
  const StringId dest_thread_;
  const StringId dest_name_;
  const StringId is_reply_;
  const StringId flags_;
  const StringId code_;
  const StringId calling_tid_;
  const StringId data_size_;
  const StringId offsets_size_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_BINDER_TRACKER_H_
