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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_crypto.h"
#include "openssl/bio.h"
#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"

namespace node {
namespace crypto {

// This class represents buffers for OpenSSL I/O, implemented as a singly-linked
// list of chunks. It can be used either for writing data from Node to OpenSSL,
// or for reading data back, but not both.
// The structure is only accessed, and owned by, the OpenSSL BIOPointer
// (a.k.a. std::unique_ptr<BIO>).
class NodeBIO : public MemoryRetainer {
 public:
  ~NodeBIO();

  static BIOPointer New(Environment* env = nullptr);

  // NewFixed takes a copy of `len` bytes from `data` and returns a BIO that,
  // when read from, returns those bytes followed by EOF.
  static BIOPointer NewFixed(const char* data, size_t len,
                             Environment* env = nullptr);

  // Move read head to next buffer if needed
  void TryMoveReadHead();

  // Allocate new buffer for write if needed
  void TryAllocateForWrite(size_t hint);

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

  // Return pointer to contiguous block of reserved data and the size available
  // for future writes. Call Commit() once the write is complete.
  char* PeekWritable(size_t* size);

  // Specify how much data was written into the block returned by
  // PeekWritable().
  void Commit(size_t size);


  // Return size of buffer in bytes
  inline size_t Length() const {
    return length_;
  }

  inline void set_eof_return(int num) {
    eof_return_ = num;
  }

  inline int eof_return() {
    return eof_return_;
  }

  inline void set_initial(size_t initial) {
    initial_ = initial;
  }

  static NodeBIO* FromBIO(BIO* bio);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("buffer", length_, "NodeBIO::Buffer");
  }

  SET_MEMORY_INFO_NAME(NodeBIO)
  SET_SELF_SIZE(NodeBIO)

 private:
  static int New(BIO* bio);
  static int Free(BIO* bio);
  static int Read(BIO* bio, char* out, int len);
  static int Write(BIO* bio, const char* data, int len);
  static int Puts(BIO* bio, const char* str);
  static int Gets(BIO* bio, char* out, int size);
  static long Ctrl(BIO* bio, int cmd, long num,  // NOLINT(runtime/int)
                   void* ptr);

  static const BIO_METHOD* GetMethod();

  // Enough to handle the most of the client hellos
  static const size_t kInitialBufferLength = 1024;
  static const size_t kThroughputBufferLength = 16384;

  class Buffer {
   public:
    Buffer(Environment* env, size_t len) : env_(env),
                                           read_pos_(0),
                                           write_pos_(0),
                                           len_(len),
                                           next_(nullptr) {
      data_ = new char[len];
      if (env_ != nullptr)
        env_->isolate()->AdjustAmountOfExternalAllocatedMemory(len);
    }

    ~Buffer() {
      delete[] data_;
      if (env_ != nullptr) {
        const int64_t len = static_cast<int64_t>(len_);
        env_->isolate()->AdjustAmountOfExternalAllocatedMemory(-len);
      }
    }

    Environment* env_;
    size_t read_pos_;
    size_t write_pos_;
    size_t len_;
    Buffer* next_;
    char* data_;
  };

  Environment* env_ = nullptr;
  size_t initial_ = kInitialBufferLength;
  size_t length_ = 0;
  int eof_return_ = -1;
  Buffer* read_head_ = nullptr;
  Buffer* write_head_ = nullptr;

  friend void node::crypto::InitCryptoOnce();
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_BIO_H_
