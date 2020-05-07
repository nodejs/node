#ifndef SRC_ALLOCATED_BUFFER_H_
#define SRC_ALLOCATED_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "uv.h"
#include "v8.h"

namespace node {

class Environment;

// A unique-pointer-ish object that is compatible with the JS engine's
// ArrayBuffer::Allocator.
struct AllocatedBuffer {
 public:
  enum AllocateManagedFlags {
    ALLOCATE_MANAGED_FLAG_NONE,
    ALLOCATE_MANAGED_UNCHECKED
  };

  // Utilities that allocate memory using the Isolate's ArrayBuffer::Allocator.
  // In particular, using AllocateManaged() will provide a RAII-style object
  // with easy conversion to `Buffer` and `ArrayBuffer` objects.
  inline static AllocatedBuffer AllocateManaged(
      Environment* env,
      size_t size,
      int flags = ALLOCATE_MANAGED_FLAG_NONE);

  explicit inline AllocatedBuffer(Environment* env = nullptr);
  inline AllocatedBuffer(Environment* env, uv_buf_t buf);
  inline ~AllocatedBuffer();
  inline void Resize(size_t len);

  inline uv_buf_t release();
  inline char* data();
  inline const char* data() const;
  inline size_t size() const;
  inline void clear();

  inline v8::MaybeLocal<v8::Object> ToBuffer();
  inline v8::Local<v8::ArrayBuffer> ToArrayBuffer();

  inline AllocatedBuffer(AllocatedBuffer&& other);
  inline AllocatedBuffer& operator=(AllocatedBuffer&& other);
  AllocatedBuffer(const AllocatedBuffer& other) = delete;
  AllocatedBuffer& operator=(const AllocatedBuffer& other) = delete;

 private:
  Environment* env_;
  // We do not pass this to libuv directly, but uv_buf_t is a convenient way
  // to represent a chunk of memory, and plays nicely with other parts of core.
  uv_buf_t buffer_;

  friend class Environment;
};

}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_ALLOCATED_BUFFER_H_
