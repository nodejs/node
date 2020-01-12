#ifndef SRC_NODE_MEM_H_
#define SRC_NODE_MEM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>

namespace node {
namespace mem {

// Both ngtcp2 and nghttp2 allow custom allocators that
// follow exactly the same structure and behavior, but
// use different struct names. To allow for code re-use,
// the NgLibMemoryManager template class can be used for both.

template <typename Class, typename AllocatorStructName>
class NgLibMemoryManager {
 public:
  // Class needs to provide these methods:
  // void CheckAllocatedSize(size_t previous_size) const;
  // void IncreaseAllocatedSize(size_t size);
  // void DecreaseAllocatedSize(size_t size);
  // Environment* env() const;

  AllocatorStructName MakeAllocator();

  void StopTrackingMemory(void* ptr);

 private:
  static void* ReallocImpl(void* ptr, size_t size, void* user_data);
  static void* MallocImpl(size_t size, void* user_data);
  static void FreeImpl(void* ptr, void* user_data);
  static void* CallocImpl(size_t nmemb, size_t size, void* user_data);
};

}  // namespace mem
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_MEM_H_
