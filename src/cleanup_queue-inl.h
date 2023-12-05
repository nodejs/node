#ifndef SRC_CLEANUP_QUEUE_INL_H_
#define SRC_CLEANUP_QUEUE_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "cleanup_queue.h"
#include "memory_tracker-inl.h"
#include "util.h"

namespace node {

inline void CleanupQueue::MemoryInfo(MemoryTracker* tracker) const {
  ForEachBaseObject([&](BaseObject* obj) {
    if (obj->IsDoneInitializing()) tracker->Track(obj);
  });
}

inline size_t CleanupQueue::SelfSize() const {
  return sizeof(CleanupQueue) +
         cleanup_hooks_.size() * sizeof(CleanupHookCallback);
}

bool CleanupQueue::empty() const {
  return cleanup_hooks_.empty();
}

void CleanupQueue::Add(Callback cb, void* arg) {
  auto insertion_info =
      cleanup_hooks_.emplace(cb, arg, cleanup_hook_counter_++);
  // Make sure there was no existing element with these values.
  CHECK_EQ(insertion_info.second, true);
}

void CleanupQueue::Remove(Callback cb, void* arg) {
  CleanupHookCallback search{cb, arg, 0};
  cleanup_hooks_.erase(search);
}

template <typename T>
void CleanupQueue::ForEachBaseObject(T&& iterator) const {
  std::vector<CleanupHookCallback> callbacks = GetOrdered();

  for (const auto& hook : callbacks) {
    BaseObject* obj = GetBaseObject(hook);
    if (obj != nullptr) iterator(obj);
  }
}

BaseObject* CleanupQueue::GetBaseObject(
    const CleanupHookCallback& callback) const {
  if (callback.fn_ == BaseObject::DeleteMe)
    return static_cast<BaseObject*>(callback.arg_);
  else
    return nullptr;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CLEANUP_QUEUE_INL_H_
