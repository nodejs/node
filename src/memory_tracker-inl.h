#ifndef SRC_MEMORY_TRACKER_INL_H_
#define SRC_MEMORY_TRACKER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker.h"

namespace node {

template <typename T>
void MemoryTracker::TrackThis(const T* obj) {
  accumulated_size_ += sizeof(T);
}

void MemoryTracker::TrackFieldWithSize(const char* name, size_t size) {
  accumulated_size_ += size;
}

void MemoryTracker::TrackField(const char* name, const MemoryRetainer& value) {
  TrackField(name, &value);
}

void MemoryTracker::TrackField(const char* name, const MemoryRetainer* value) {
  if (track_only_self_ || value == nullptr || seen_.count(value) > 0) return;
  seen_.insert(value);
  Track(value);
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const std::unique_ptr<T>& value) {
  TrackField(name, value.get());
}

template <typename T, typename Iterator>
void MemoryTracker::TrackField(const char* name, const T& value) {
  if (value.begin() == value.end()) return;
  size_t index = 0;
  for (Iterator it = value.begin(); it != value.end(); ++it)
    TrackField(std::to_string(index++).c_str(), *it);
}

template <typename T>
void MemoryTracker::TrackField(const char* name, const std::queue<T>& value) {
  struct ContainerGetter : public std::queue<T> {
    static const typename std::queue<T>::container_type& Get(
        const std::queue<T>& value) {
      return value.*&ContainerGetter::c;
    }
  };

  const auto& container = ContainerGetter::Get(value);
  TrackField(name, container);
}

template <typename T, typename test_for_number, typename dummy>
void MemoryTracker::TrackField(const char* name, const T& value) {
  // For numbers, creating new nodes is not worth the overhead.
  TrackFieldWithSize(name, sizeof(T));
}

template <typename T, typename U>
void MemoryTracker::TrackField(const char* name, const std::pair<T, U>& value) {
  TrackField("first", value.first);
  TrackField("second", value.second);
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const std::basic_string<T>& value) {
  TrackFieldWithSize(name, value.size() * sizeof(T));
}

template <typename T, typename Traits>
void MemoryTracker::TrackField(const char* name,
                               const v8::Persistent<T, Traits>& value) {
}

template <typename T>
void MemoryTracker::TrackField(const char* name, const v8::Local<T>& value) {
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const MallocedBuffer<T>& value) {
  TrackFieldWithSize(name, value.size);
}

void MemoryTracker::TrackField(const char* name, const uv_buf_t& value) {
  TrackFieldWithSize(name, value.len);
}

template <class NativeT, class V8T>
void MemoryTracker::TrackField(const char* name,
                       const AliasedBuffer<NativeT, V8T>& value) {
  TrackField(name, value.GetJSArray());
}

void MemoryTracker::Track(const MemoryRetainer* value) {
  value->MemoryInfo(this);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MEMORY_TRACKER_INL_H_
