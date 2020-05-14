#ifndef SRC_ALLOCATED_BUFFER_H_
#define SRC_ALLOCATED_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "uv.h"
#include "v8.h"
#include "env.h"

namespace node {

class Environment;

// Disables zero-filling for ArrayBuffer allocations in this scope. This is
// similar to how we implement Buffer.allocUnsafe() in JS land.
class NoArrayBufferZeroFillScope{
 public:
  inline explicit NoArrayBufferZeroFillScope(IsolateData* isolate_data);
  inline ~NoArrayBufferZeroFillScope();

 private:
  NodeArrayBufferAllocator* node_allocator_;

  friend class Environment;
};

// A unique-pointer-ish object that is compatible with the JS engine's
// ArrayBuffer::Allocator.
// TODO(addaleax): We may want to start phasing this out as it's only a
// thin wrapper around v8::BackingStore at this point
struct AllocatedBuffer {
 public:
  // Utilities that allocate memory using the Isolate's ArrayBuffer::Allocator.
  // In particular, using AllocateManaged() will provide a RAII-style object
  // with easy conversion to `Buffer` and `ArrayBuffer` objects.
  inline static AllocatedBuffer AllocateManaged(Environment* env, size_t size);

  AllocatedBuffer() = default;
  inline AllocatedBuffer(
      Environment* env, std::unique_ptr<v8::BackingStore> bs);
  // For this constructor variant, `buffer` *must* come from an earlier call
  // to .release
  inline AllocatedBuffer(Environment* env, uv_buf_t buffer);

  inline void Resize(size_t len);

  inline uv_buf_t release();
  inline char* data();
  inline const char* data() const;
  inline size_t size() const;
  inline void clear();

  inline v8::MaybeLocal<v8::Object> ToBuffer();
  inline v8::Local<v8::ArrayBuffer> ToArrayBuffer();

  AllocatedBuffer(AllocatedBuffer&& other) = default;
  AllocatedBuffer& operator=(AllocatedBuffer&& other) = default;
  AllocatedBuffer(const AllocatedBuffer& other) = delete;
  AllocatedBuffer& operator=(const AllocatedBuffer& other) = delete;

 private:
  Environment* env_ = nullptr;
  std::unique_ptr<v8::BackingStore> backing_store_;

  friend class Environment;
};

}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_ALLOCATED_BUFFER_H_
