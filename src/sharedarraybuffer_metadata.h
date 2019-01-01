#ifndef SRC_SHAREDARRAYBUFFER_METADATA_H_
#define SRC_SHAREDARRAYBUFFER_METADATA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include <memory>

namespace node {
namespace worker {

class SharedArrayBufferMetadata;

// This is an object associated with a SharedArrayBuffer, which keeps track
// of a cross-thread reference count. Once a SharedArrayBuffer is transferred
// for the first time (or is attempted to be transferred), one of these objects
// is created, and the SharedArrayBuffer is moved from internalized mode into
// externalized mode (i.e. the JS engine no longer frees the memory on its own).
//
// This will always be referred to using a std::shared_ptr, since it keeps
// a reference count and is guaranteed to be thread-safe.
typedef std::shared_ptr<SharedArrayBufferMetadata>
    SharedArrayBufferMetadataReference;

class SharedArrayBufferMetadata
    : public std::enable_shared_from_this<SharedArrayBufferMetadata> {
 public:
  static SharedArrayBufferMetadataReference ForSharedArrayBuffer(
      Environment* env,
      v8::Local<v8::Context> context,
      v8::Local<v8::SharedArrayBuffer> source);
  ~SharedArrayBufferMetadata();

  // Create a SharedArrayBuffer object for a specific Environment and Context.
  // The created SharedArrayBuffer will be in externalized mode and has
  // a hidden object attached to it, during whose lifetime the reference
  // count is increased by 1.
  v8::MaybeLocal<v8::SharedArrayBuffer> GetSharedArrayBuffer(
      Environment* env, v8::Local<v8::Context> context);

  SharedArrayBufferMetadata(SharedArrayBufferMetadata&& other) = delete;
  SharedArrayBufferMetadata& operator=(
      SharedArrayBufferMetadata&& other) = delete;
  SharedArrayBufferMetadata& operator=(
      const SharedArrayBufferMetadata&) = delete;
  SharedArrayBufferMetadata(const SharedArrayBufferMetadata&) = delete;

 private:
  explicit SharedArrayBufferMetadata(const v8::SharedArrayBuffer::Contents&);

  // Attach a lifetime tracker object with a reference count to `target`.
  v8::Maybe<bool> AssignToSharedArrayBuffer(
      Environment* env,
      v8::Local<v8::Context> context,
      v8::Local<v8::SharedArrayBuffer> target);

  v8::SharedArrayBuffer::Contents contents_;
};

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_SHAREDARRAYBUFFER_METADATA_H_
