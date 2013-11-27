// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_CRYPTO_BIO_H_
#define SRC_NODE_CRYPTO_BIO_H_

#include "openssl/bio.h"
#include <assert.h>

namespace node {

class NodeBIO {
 public:
  NodeBIO() : length_(0), read_head_(&head_), write_head_(&head_) {
    // Loop head
    head_.next_ = &head_;
  }

  ~NodeBIO();

  static BIO* New();

  // Move read head to next buffer if needed
  void TryMoveReadHead();

  // Allocate new buffer for write if needed
  void TryAllocateForWrite();

  // Read `len` bytes maximum into `out`, return actual number of read bytes
  size_t Read(char* out, size_t size);

  // Memory optimization:
  // Deallocate children of write head's child if they're empty
  void FreeEmpty();

  // Return pointer to internal data and amount of
  // contiguous data available to read
  char* Peek(size_t* size);

  // Return pointers and sizes of multiple internal data chunks available for
  // reading
  size_t PeekMultiple(char** out, size_t* size, size_t* count);

  // Find first appearance of `delim` in buffer or `limit` if `delim`
  // wasn't found.
  size_t IndexOf(char delim, size_t limit);

  // Discard all available data
  void Reset();

  // Put `len` bytes from `data` into buffer
  void Write(const char* data, size_t size);

  // Return pointer to internal data and amount of
  // contiguous data available for future writes
  char* PeekWritable(size_t* size);

  // Commit reserved data
  void Commit(size_t size);

  // Return size of buffer in bytes
  size_t inline Length() {
    return length_;
  }

  static inline NodeBIO* FromBIO(BIO* bio) {
    assert(bio->ptr != NULL);
    return static_cast<NodeBIO*>(bio->ptr);
  }

 private:
  static int New(BIO* bio);
  static int Free(BIO* bio);
  static int Read(BIO* bio, char* out, int len);
  static int Write(BIO* bio, const char* data, int len);
  static int Puts(BIO* bio, const char* str);
  static int Gets(BIO* bio, char* out, int size);
  static long Ctrl(BIO* bio, int cmd, long num, void* ptr);

  // NOTE: Size is maximum TLS frame length, this is required if we want
  // to fit whole ClientHello into one Buffer of NodeBIO.
  static const size_t kBufferLength = 16 * 1024 + 5;
  static const BIO_METHOD method;

  class Buffer {
   public:
    Buffer() : read_pos_(0), write_pos_(0), next_(NULL) {
    }

    size_t read_pos_;
    size_t write_pos_;
    Buffer* next_;
    char data_[kBufferLength];
  };

  size_t length_;
  Buffer head_;
  Buffer* read_head_;
  Buffer* write_head_;
};

}  // namespace node

#endif  // SRC_NODE_CRYPTO_BIO_H_
