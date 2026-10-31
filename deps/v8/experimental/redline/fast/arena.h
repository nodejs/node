// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_ARENA_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_ARENA_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace redline {
namespace fast {

// A bump allocator owning every node of one program. Objects allocated here
// live as long as the arena; objects with non-trivial destructors are
// destroyed when the arena dies (in reverse allocation order). Mutators are
// therefore free to detach subtrees and forget about them.
//
// Memory comes in fixed-size segments that are recycled through a per-thread
// pool (see arena.cc): a fuzzing worker creates and drops thousands of trees
// per second, and with the pool a tree costs bump allocation only, no trips
// to malloc.
class Arena {
 public:
  Arena() = default;
  ~Arena();
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

  template <typename T, typename... Args>
  T* New(Args&&... args) {
    void* memory = Allocate(sizeof(T), alignof(T));
    T* object = new (memory) T(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      destructors_.push_back(
          {object, [](void* p) { static_cast<T*>(p)->~T(); }});
    }
    return object;
  }

  // Copies `text` into the arena; the returned view lives as long as it.
  std::string_view CopyString(std::string_view text);

  size_t bytes_allocated() const { return bytes_allocated_; }

  // Size of a pooled segment; requests larger than this get a segment of
  // their own that is not pooled.
  static constexpr size_t kSegmentSize = static_cast<size_t>(64) * 1024;

 private:
  struct Destructor {
    void* object;
    void (*destroy)(void*);
  };

  struct Segment {
    char* memory;
    size_t size;
  };

  std::vector<Segment> segments_;
  std::vector<Destructor> destructors_;
  char* cursor_ = nullptr;
  char* limit_ = nullptr;
  size_t bytes_allocated_ = 0;
};

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_ARENA_H_
