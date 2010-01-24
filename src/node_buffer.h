#ifndef NODE_BUFFER
#define NODE_BUFFER

#include <v8.h>

namespace node {

#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* A buffer is a chunk of memory stored outside the V8 heap, mirrored by an
 * object in javascript. The object is not totally opaque, one can access
 * individual bytes with [] and slice it into substrings or sub-buffers
 * without copying memory.
 *
 * // return an ascii encoded string - no memory iscopied
 * buffer.asciiSlide(0, 3) 
 *
 * // returns another buffer - no memory is copied
 * buffer.slice(0, 3)
 *
 * Interally, each javascript buffer object is backed by a "struct buffer"
 * object.  These "struct buffer" objects are either a root buffer (in the
 * case that buffer->root == NULL) or slice objects (in which case
 * buffer->root != NULL).  A root buffer is only GCed once all its slices
 * are GCed.
 */

struct buffer {
  v8::Persistent<v8::Object> handle;  // both
  bool weak;                          // both
  struct buffer *root;                // both (NULL for root)
  size_t offset;                      // both (0 for root)
  size_t length;                      // both
  unsigned int refs;                  // root only
  char bytes[1];                      // root only
};

void InitBuffer(v8::Handle<v8::Object> target);

struct buffer* BufferUnwrap(v8::Handle<v8::Value> val);
bool IsBuffer(v8::Handle<v8::Value> val);

static inline struct buffer * buffer_root(struct buffer *buffer) {
  return buffer->root ? buffer->root : buffer;
}

static inline char * buffer_p(struct buffer *buffer, size_t off) {
  struct buffer *root = buffer_root(buffer);
  if (buffer->offset + off >= root->length) return NULL;
  return reinterpret_cast<char*>(&(root->bytes) + buffer->offset + off);
}

static inline size_t buffer_remaining(struct buffer *buffer, size_t off) {
  struct buffer *root = buffer_root(buffer);
  char *end = reinterpret_cast<char*>(&(root->bytes) + root->length);
  return end - buffer_p(buffer, off);
}

}

#endif  // NODE_BUFFER
