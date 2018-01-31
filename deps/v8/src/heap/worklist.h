// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_WORKLIST_
#define V8_HEAP_WORKLIST_

#include <cstddef>
#include <utility>

#include "src/base/atomic-utils.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

// A concurrent worklist based on segments. Each tasks gets private
// push and pop segments. Empty pop segments are swapped with their
// corresponding push segments. Full push segments are published to a global
// pool of segments and replaced with empty segments.
//
// Work stealing is best effort, i.e., there is no way to inform other tasks
// of the need of items.
template <typename EntryType, int SEGMENT_SIZE>
class Worklist {
 public:
  class View {
   public:
    View(Worklist<EntryType, SEGMENT_SIZE>* worklist, int task_id)
        : worklist_(worklist), task_id_(task_id) {}

    // Pushes an entry onto the worklist.
    bool Push(EntryType entry) { return worklist_->Push(task_id_, entry); }

    // Pops an entry from the worklist.
    bool Pop(EntryType* entry) { return worklist_->Pop(task_id_, entry); }

    // Returns true if the local portion of the worklist is empty.
    bool IsLocalEmpty() { return worklist_->IsLocalEmpty(task_id_); }

    // Returns true if the worklist is empty. Can only be used from the main
    // thread without concurrent access.
    bool IsGlobalEmpty() { return worklist_->IsGlobalEmpty(); }

    bool IsGlobalPoolEmpty() { return worklist_->IsGlobalPoolEmpty(); }

    size_t LocalPushSegmentSize() {
      return worklist_->LocalPushSegmentSize(task_id_);
    }

   private:
    Worklist<EntryType, SEGMENT_SIZE>* worklist_;
    int task_id_;
  };

  static const int kMaxNumTasks = 8;
  static const size_t kSegmentCapacity = SEGMENT_SIZE;

  Worklist() : Worklist(kMaxNumTasks) {}

  explicit Worklist(int num_tasks) : num_tasks_(num_tasks) {
    for (int i = 0; i < num_tasks_; i++) {
      private_push_segment(i) = NewSegment();
      private_pop_segment(i) = NewSegment();
    }
  }

  ~Worklist() {
    CHECK(IsGlobalEmpty());
    for (int i = 0; i < num_tasks_; i++) {
      DCHECK_NOT_NULL(private_push_segment(i));
      DCHECK_NOT_NULL(private_pop_segment(i));
      delete private_push_segment(i);
      delete private_pop_segment(i);
    }
  }

  bool Push(int task_id, EntryType entry) {
    DCHECK_LT(task_id, num_tasks_);
    DCHECK_NOT_NULL(private_push_segment(task_id));
    if (!private_push_segment(task_id)->Push(entry)) {
      PublishPushSegmentToGlobal(task_id);
      bool success = private_push_segment(task_id)->Push(entry);
      USE(success);
      DCHECK(success);
    }
    return true;
  }

  bool Pop(int task_id, EntryType* entry) {
    DCHECK_LT(task_id, num_tasks_);
    DCHECK_NOT_NULL(private_pop_segment(task_id));
    if (!private_pop_segment(task_id)->Pop(entry)) {
      if (!private_push_segment(task_id)->IsEmpty()) {
        Segment* tmp = private_pop_segment(task_id);
        private_pop_segment(task_id) = private_push_segment(task_id);
        private_push_segment(task_id) = tmp;
      } else if (!StealPopSegmentFromGlobal(task_id)) {
        return false;
      }
      bool success = private_pop_segment(task_id)->Pop(entry);
      USE(success);
      DCHECK(success);
    }
    return true;
  }

  size_t LocalPushSegmentSize(int task_id) {
    return private_push_segment(task_id)->Size();
  }

  bool IsLocalEmpty(int task_id) {
    return private_pop_segment(task_id)->IsEmpty() &&
           private_push_segment(task_id)->IsEmpty();
  }

  bool IsGlobalPoolEmpty() { return global_pool_.IsEmpty(); }

  bool IsGlobalEmpty() {
    for (int i = 0; i < num_tasks_; i++) {
      if (!IsLocalEmpty(i)) return false;
    }
    return global_pool_.IsEmpty();
  }

  size_t LocalSize(int task_id) {
    return private_pop_segment(task_id)->Size() +
           private_push_segment(task_id)->Size();
  }

  // Clears all segments. Frees the global segment pool.
  //
  // Assumes that no other tasks are running.
  void Clear() {
    for (int i = 0; i < num_tasks_; i++) {
      private_pop_segment(i)->Clear();
      private_push_segment(i)->Clear();
    }
    global_pool_.Clear();
  }

  // Calls the specified callback on each element of the deques and replaces
  // the element with the result of the callback.
  // The signature of the callback is
  //   bool Callback(EntryType old, EntryType* new).
  // If the callback returns |false| then the element is removed from the
  // worklist. Otherwise the |new| entry is updated.
  //
  // Assumes that no other tasks are running.
  template <typename Callback>
  void Update(Callback callback) {
    for (int i = 0; i < num_tasks_; i++) {
      private_pop_segment(i)->Update(callback);
      private_push_segment(i)->Update(callback);
    }
    global_pool_.Update(callback);
  }

  template <typename Callback>
  void IterateGlobalPool(Callback callback) {
    global_pool_.Iterate(callback);
  }

  void FlushToGlobal(int task_id) {
    PublishPushSegmentToGlobal(task_id);
    PublishPopSegmentToGlobal(task_id);
  }

  void MergeGlobalPool(Worklist* other) {
    auto pair = other->global_pool_.Extract();
    global_pool_.MergeList(pair.first, pair.second);
  }

 private:
  FRIEND_TEST(WorkListTest, SegmentCreate);
  FRIEND_TEST(WorkListTest, SegmentPush);
  FRIEND_TEST(WorkListTest, SegmentPushPop);
  FRIEND_TEST(WorkListTest, SegmentIsEmpty);
  FRIEND_TEST(WorkListTest, SegmentIsFull);
  FRIEND_TEST(WorkListTest, SegmentClear);
  FRIEND_TEST(WorkListTest, SegmentFullPushFails);
  FRIEND_TEST(WorkListTest, SegmentEmptyPopFails);
  FRIEND_TEST(WorkListTest, SegmentUpdateFalse);
  FRIEND_TEST(WorkListTest, SegmentUpdate);

  class Segment {
   public:
    static const size_t kCapacity = kSegmentCapacity;

    Segment() : index_(0) {}

    bool Push(EntryType entry) {
      if (IsFull()) return false;
      entries_[index_++] = entry;
      return true;
    }

    bool Pop(EntryType* entry) {
      if (IsEmpty()) return false;
      *entry = entries_[--index_];
      return true;
    }

    size_t Size() const { return index_; }
    bool IsEmpty() const { return index_ == 0; }
    bool IsFull() const { return index_ == kCapacity; }
    void Clear() { index_ = 0; }

    template <typename Callback>
    void Update(Callback callback) {
      size_t new_index = 0;
      for (size_t i = 0; i < index_; i++) {
        if (callback(entries_[i], &entries_[new_index])) {
          new_index++;
        }
      }
      index_ = new_index;
    }

    template <typename Callback>
    void Iterate(Callback callback) const {
      for (size_t i = 0; i < index_; i++) {
        callback(entries_[i]);
      }
    }

    Segment* next() const { return next_; }
    void set_next(Segment* segment) { next_ = segment; }

   private:
    Segment* next_;
    size_t index_;
    EntryType entries_[kCapacity];
  };

  struct PrivateSegmentHolder {
    Segment* private_push_segment;
    Segment* private_pop_segment;
    char cache_line_padding[64];
  };

  class GlobalPool {
   public:
    GlobalPool() : top_(nullptr) {}

    V8_INLINE void Push(Segment* segment) {
      base::LockGuard<base::Mutex> guard(&lock_);
      segment->set_next(top_);
      set_top(segment);
    }

    V8_INLINE bool Pop(Segment** segment) {
      base::LockGuard<base::Mutex> guard(&lock_);
      if (top_ != nullptr) {
        *segment = top_;
        set_top(top_->next());
        return true;
      }
      return false;
    }

    V8_INLINE bool IsEmpty() {
      return base::AsAtomicPointer::Relaxed_Load(&top_) == nullptr;
    }

    void Clear() {
      base::LockGuard<base::Mutex> guard(&lock_);
      Segment* current = top_;
      while (current != nullptr) {
        Segment* tmp = current;
        current = current->next();
        delete tmp;
      }
      set_top(nullptr);
    }

    // See Worklist::Update.
    template <typename Callback>
    void Update(Callback callback) {
      base::LockGuard<base::Mutex> guard(&lock_);
      Segment* prev = nullptr;
      Segment* current = top_;
      while (current != nullptr) {
        current->Update(callback);
        if (current->IsEmpty()) {
          if (prev == nullptr) {
            top_ = current->next();
          } else {
            prev->set_next(current->next());
          }
          Segment* tmp = current;
          current = current->next();
          delete tmp;
        } else {
          prev = current;
          current = current->next();
        }
      }
    }

    // See Worklist::Iterate.
    template <typename Callback>
    void Iterate(Callback callback) {
      base::LockGuard<base::Mutex> guard(&lock_);
      for (Segment* current = top_; current != nullptr;
           current = current->next()) {
        current->Iterate(callback);
      }
    }

    std::pair<Segment*, Segment*> Extract() {
      Segment* top = nullptr;
      {
        base::LockGuard<base::Mutex> guard(&lock_);
        if (top_ == nullptr) return std::make_pair(nullptr, nullptr);
        top = top_;
        set_top(nullptr);
      }
      Segment* end = top;
      while (end->next() != nullptr) end = end->next();
      return std::make_pair(top, end);
    }

    void MergeList(Segment* start, Segment* end) {
      if (start == nullptr) return;
      {
        base::LockGuard<base::Mutex> guard(&lock_);
        end->set_next(top_);
        set_top(start);
      }
    }

   private:
    void set_top(Segment* segment) {
      base::AsAtomicPointer::Relaxed_Store(&top_, segment);
    }

    base::Mutex lock_;
    Segment* top_;
  };

  V8_INLINE Segment*& private_push_segment(int task_id) {
    return private_segments_[task_id].private_push_segment;
  }

  V8_INLINE Segment*& private_pop_segment(int task_id) {
    return private_segments_[task_id].private_pop_segment;
  }

  V8_INLINE void PublishPushSegmentToGlobal(int task_id) {
    if (!private_push_segment(task_id)->IsEmpty()) {
      global_pool_.Push(private_push_segment(task_id));
      private_push_segment(task_id) = NewSegment();
    }
  }

  V8_INLINE void PublishPopSegmentToGlobal(int task_id) {
    if (!private_pop_segment(task_id)->IsEmpty()) {
      global_pool_.Push(private_pop_segment(task_id));
      private_pop_segment(task_id) = NewSegment();
    }
  }

  V8_INLINE bool StealPopSegmentFromGlobal(int task_id) {
    if (global_pool_.IsEmpty()) return false;
    Segment* new_segment = nullptr;
    if (global_pool_.Pop(&new_segment)) {
      delete private_pop_segment(task_id);
      private_pop_segment(task_id) = new_segment;
      return true;
    }
    return false;
  }

  V8_INLINE Segment* NewSegment() {
    // Bottleneck for filtering in crash dumps.
    return new Segment();
  }

  PrivateSegmentHolder private_segments_[kMaxNumTasks];
  GlobalPool global_pool_;
  int num_tasks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_WORKLIST_
