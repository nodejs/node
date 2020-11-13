// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_WORKLIST_H_
#define V8_HEAP_BASE_WORKLIST_H_

#include <cstddef>
#include <utility>

#include "src/base/atomic-utils.h"
#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace heap {
namespace base {

namespace internal {
class V8_EXPORT_PRIVATE SegmentBase {
 public:
  static SegmentBase* GetSentinelSegmentAddress();

  explicit SegmentBase(uint16_t capacity) : capacity_(capacity) {}

  size_t Size() const { return index_; }
  bool IsEmpty() const { return index_ == 0; }
  bool IsFull() const { return index_ == capacity_; }
  void Clear() { index_ = 0; }

 protected:
  const uint16_t capacity_;
  uint16_t index_ = 0;
};
}  // namespace internal

// A global marking worklist that is similar the existing Worklist
// but does not reserve space and keep track of the local segments.
// Eventually this will replace Worklist after all its current uses
// are migrated.
template <typename EntryType, uint16_t SegmentSize>
class Worklist {
 public:
  static const int kSegmentSize = SegmentSize;
  class Segment;
  class Local;

  Worklist() = default;
  ~Worklist() { CHECK(IsEmpty()); }

  void Push(Segment* segment);
  bool Pop(Segment** segment);

  // Returns true if the list of segments is empty.
  bool IsEmpty();
  // Returns the number of segments in the list.
  size_t Size();

  // Moves the segments of the given marking worklist into this
  // marking worklist.
  void Merge(Worklist<EntryType, SegmentSize>* other);

  // These functions are not thread-safe. They should be called only
  // if all local marking worklists that use the current worklist have
  // been published and are empty.
  void Clear();
  template <typename Callback>
  void Update(Callback callback);
  template <typename Callback>
  void Iterate(Callback callback);

 private:
  void set_top(Segment* segment) {
    v8::base::AsAtomicPtr(&top_)->store(segment, std::memory_order_relaxed);
  }

  v8::base::Mutex lock_;
  Segment* top_ = nullptr;
  std::atomic<size_t> size_{0};
};

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Push(Segment* segment) {
  DCHECK(!segment->IsEmpty());
  v8::base::MutexGuard guard(&lock_);
  segment->set_next(top_);
  set_top(segment);
  size_.fetch_add(1, std::memory_order_relaxed);
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Pop(Segment** segment) {
  v8::base::MutexGuard guard(&lock_);
  if (top_ == nullptr) return false;
  DCHECK_LT(0U, size_);
  size_.fetch_sub(1, std::memory_order_relaxed);
  *segment = top_;
  set_top(top_->next());
  return true;
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::IsEmpty() {
  return v8::base::AsAtomicPtr(&top_)->load(std::memory_order_relaxed) ==
         nullptr;
}

template <typename EntryType, uint16_t SegmentSize>
size_t Worklist<EntryType, SegmentSize>::Size() {
  // It is safe to read |size_| without a lock since this variable is
  // atomic, keeping in mind that threads may not immediately see the new
  // value when it is updated.
  return size_.load(std::memory_order_relaxed);
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Clear() {
  v8::base::MutexGuard guard(&lock_);
  size_.store(0, std::memory_order_relaxed);
  Segment* current = top_;
  while (current != nullptr) {
    Segment* tmp = current;
    current = current->next();
    delete tmp;
  }
  set_top(nullptr);
}

template <typename EntryType, uint16_t SegmentSize>
template <typename Callback>
void Worklist<EntryType, SegmentSize>::Update(Callback callback) {
  v8::base::MutexGuard guard(&lock_);
  Segment* prev = nullptr;
  Segment* current = top_;
  size_t num_deleted = 0;
  while (current != nullptr) {
    current->Update(callback);
    if (current->IsEmpty()) {
      DCHECK_LT(0U, size_);
      ++num_deleted;
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
  size_.fetch_sub(num_deleted, std::memory_order_relaxed);
}

template <typename EntryType, uint16_t SegmentSize>
template <typename Callback>
void Worklist<EntryType, SegmentSize>::Iterate(Callback callback) {
  v8::base::MutexGuard guard(&lock_);
  for (Segment* current = top_; current != nullptr; current = current->next()) {
    current->Iterate(callback);
  }
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Merge(
    Worklist<EntryType, SegmentSize>* other) {
  Segment* top = nullptr;
  size_t other_size = 0;
  {
    v8::base::MutexGuard guard(&other->lock_);
    if (!other->top_) return;
    top = other->top_;
    other_size = other->size_.load(std::memory_order_relaxed);
    other->size_.store(0, std::memory_order_relaxed);
    other->set_top(nullptr);
  }

  // It's safe to iterate through these segments because the top was
  // extracted from |other|.
  Segment* end = top;
  while (end->next()) end = end->next();

  {
    v8::base::MutexGuard guard(&lock_);
    size_.fetch_add(other_size, std::memory_order_relaxed);
    end->set_next(top_);
    set_top(top);
  }
}

template <typename EntryType, uint16_t SegmentSize>
class Worklist<EntryType, SegmentSize>::Segment : public internal::SegmentBase {
 public:
  static const uint16_t kSize = SegmentSize;

  void Push(EntryType entry);
  void Pop(EntryType* entry);

  template <typename Callback>
  void Update(Callback callback);
  template <typename Callback>
  void Iterate(Callback callback) const;

  Segment* next() const { return next_; }
  void set_next(Segment* segment) { next_ = segment; }

 private:
  Segment() : internal::SegmentBase(kSize) {}

  Segment* next_ = nullptr;
  EntryType entries_[kSize];

  friend class Worklist<EntryType, SegmentSize>::Local;

  FRIEND_TEST(CppgcWorkListTest, SegmentCreate);
  FRIEND_TEST(CppgcWorkListTest, SegmentPush);
  FRIEND_TEST(CppgcWorkListTest, SegmentPushPop);
  FRIEND_TEST(CppgcWorkListTest, SegmentIsEmpty);
  FRIEND_TEST(CppgcWorkListTest, SegmentIsFull);
  FRIEND_TEST(CppgcWorkListTest, SegmentClear);
  FRIEND_TEST(CppgcWorkListTest, SegmentUpdateFalse);
  FRIEND_TEST(CppgcWorkListTest, SegmentUpdate);
};

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Segment::Push(EntryType entry) {
  DCHECK(!IsFull());
  entries_[index_++] = entry;
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Segment::Pop(EntryType* entry) {
  DCHECK(!IsEmpty());
  *entry = entries_[--index_];
}

template <typename EntryType, uint16_t SegmentSize>
template <typename Callback>
void Worklist<EntryType, SegmentSize>::Segment::Update(Callback callback) {
  size_t new_index = 0;
  for (size_t i = 0; i < index_; i++) {
    if (callback(entries_[i], &entries_[new_index])) {
      new_index++;
    }
  }
  index_ = new_index;
}

template <typename EntryType, uint16_t SegmentSize>
template <typename Callback>
void Worklist<EntryType, SegmentSize>::Segment::Iterate(
    Callback callback) const {
  for (size_t i = 0; i < index_; i++) {
    callback(entries_[i]);
  }
}

// A thread-local view of the marking worklist.
template <typename EntryType, uint16_t SegmentSize>
class Worklist<EntryType, SegmentSize>::Local {
 public:
  using ItemType = EntryType;

  Local() = default;
  explicit Local(Worklist<EntryType, SegmentSize>* worklist);
  ~Local();

  Local(Local&&) V8_NOEXCEPT;
  Local& operator=(Local&&) V8_NOEXCEPT;

  // Disable copying since having multiple copies of the same
  // local marking worklist is unsafe.
  Local(const Local&) = delete;
  Local& operator=(const Local& other) = delete;

  void Push(EntryType entry);
  bool Pop(EntryType* entry);

  bool IsLocalAndGlobalEmpty() const;
  bool IsLocalEmpty() const;
  bool IsGlobalEmpty() const;

  void Publish();
  void Merge(Worklist<EntryType, SegmentSize>::Local* other);

  size_t PushSegmentSize() const { return push_segment_->Size(); }

 private:
  void PublishPushSegment();
  void PublishPopSegment();
  bool StealPopSegment();

  Segment* NewSegment() const {
    // Bottleneck for filtering in crash dumps.
    return new Segment();
  }
  void DeleteSegment(internal::SegmentBase* segment) const {
    if (segment == internal::SegmentBase::GetSentinelSegmentAddress()) return;
    delete static_cast<Segment*>(segment);
  }

  inline Segment* push_segment() {
    DCHECK_NE(internal::SegmentBase::GetSentinelSegmentAddress(),
              push_segment_);
    return static_cast<Segment*>(push_segment_);
  }
  inline const Segment* push_segment() const {
    DCHECK_NE(internal::SegmentBase::GetSentinelSegmentAddress(),
              push_segment_);
    return static_cast<const Segment*>(push_segment_);
  }

  inline Segment* pop_segment() {
    DCHECK_NE(internal::SegmentBase::GetSentinelSegmentAddress(), pop_segment_);
    return static_cast<Segment*>(pop_segment_);
  }
  inline const Segment* pop_segment() const {
    DCHECK_NE(internal::SegmentBase::GetSentinelSegmentAddress(), pop_segment_);
    return static_cast<const Segment*>(pop_segment_);
  }

  Worklist<EntryType, SegmentSize>* worklist_ = nullptr;
  internal::SegmentBase* push_segment_ = nullptr;
  internal::SegmentBase* pop_segment_ = nullptr;
};

template <typename EntryType, uint16_t SegmentSize>
Worklist<EntryType, SegmentSize>::Local::Local(
    Worklist<EntryType, SegmentSize>* worklist)
    : worklist_(worklist),
      push_segment_(internal::SegmentBase::GetSentinelSegmentAddress()),
      pop_segment_(internal::SegmentBase::GetSentinelSegmentAddress()) {}

template <typename EntryType, uint16_t SegmentSize>
Worklist<EntryType, SegmentSize>::Local::~Local() {
  CHECK_IMPLIES(push_segment_, push_segment_->IsEmpty());
  CHECK_IMPLIES(pop_segment_, pop_segment_->IsEmpty());
  DeleteSegment(push_segment_);
  DeleteSegment(pop_segment_);
}

template <typename EntryType, uint16_t SegmentSize>
Worklist<EntryType, SegmentSize>::Local::Local(
    Worklist<EntryType, SegmentSize>::Local&& other) V8_NOEXCEPT {
  worklist_ = other.worklist_;
  push_segment_ = other.push_segment_;
  pop_segment_ = other.pop_segment_;
  other.worklist_ = nullptr;
  other.push_segment_ = nullptr;
  other.pop_segment_ = nullptr;
}

template <typename EntryType, uint16_t SegmentSize>
typename Worklist<EntryType, SegmentSize>::Local&
Worklist<EntryType, SegmentSize>::Local::operator=(
    Worklist<EntryType, SegmentSize>::Local&& other) V8_NOEXCEPT {
  if (this != &other) {
    DCHECK_NULL(worklist_);
    DCHECK_NULL(push_segment_);
    DCHECK_NULL(pop_segment_);
    worklist_ = other.worklist_;
    push_segment_ = other.push_segment_;
    pop_segment_ = other.pop_segment_;
    other.worklist_ = nullptr;
    other.push_segment_ = nullptr;
    other.pop_segment_ = nullptr;
  }
  return *this;
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Local::Push(EntryType entry) {
  if (V8_UNLIKELY(push_segment_->IsFull())) {
    PublishPushSegment();
  }
  push_segment()->Push(entry);
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Local::Pop(EntryType* entry) {
  if (pop_segment_->IsEmpty()) {
    if (!push_segment_->IsEmpty()) {
      std::swap(push_segment_, pop_segment_);
    } else if (!StealPopSegment()) {
      return false;
    }
  }
  pop_segment()->Pop(entry);
  return true;
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Local::IsLocalAndGlobalEmpty() const {
  return IsLocalEmpty() && IsGlobalEmpty();
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Local::IsLocalEmpty() const {
  return push_segment_->IsEmpty() && pop_segment_->IsEmpty();
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Local::IsGlobalEmpty() const {
  return worklist_->IsEmpty();
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Local::Publish() {
  if (!push_segment_->IsEmpty()) PublishPushSegment();
  if (!pop_segment_->IsEmpty()) PublishPopSegment();
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Local::Merge(
    Worklist<EntryType, SegmentSize>::Local* other) {
  other->Publish();
  worklist_->Merge(other->worklist_);
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Local::PublishPushSegment() {
  if (push_segment_ != internal::SegmentBase::GetSentinelSegmentAddress())
    worklist_->Push(push_segment());
  push_segment_ = NewSegment();
}

template <typename EntryType, uint16_t SegmentSize>
void Worklist<EntryType, SegmentSize>::Local::PublishPopSegment() {
  if (pop_segment_ != internal::SegmentBase::GetSentinelSegmentAddress())
    worklist_->Push(pop_segment());
  pop_segment_ = NewSegment();
}

template <typename EntryType, uint16_t SegmentSize>
bool Worklist<EntryType, SegmentSize>::Local::StealPopSegment() {
  if (worklist_->IsEmpty()) return false;
  Segment* new_segment = nullptr;
  if (worklist_->Pop(&new_segment)) {
    DeleteSegment(pop_segment_);
    pop_segment_ = new_segment;
    return true;
  }
  return false;
}

}  // namespace base
}  // namespace heap

#endif  // V8_HEAP_BASE_WORKLIST_H_
