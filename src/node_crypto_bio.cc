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

#include "node_crypto_bio.h"
#include "openssl/bio.h"
#include <string.h>

namespace node {

const BIO_METHOD NodeBIO::method = {
  BIO_TYPE_MEM,
  "node.js SSL buffer",
  NodeBIO::Write,
  NodeBIO::Read,
  NodeBIO::Puts,
  NodeBIO::Gets,
  NodeBIO::Ctrl,
  NodeBIO::New,
  NodeBIO::Free,
  NULL
};


BIO* NodeBIO::New() {
  // The const_cast doesn't violate const correctness.  OpenSSL's usage of
  // BIO_METHOD is effectively const but BIO_new() takes a non-const argument.
  return BIO_new(const_cast<BIO_METHOD*>(&method));
}


int NodeBIO::New(BIO* bio) {
  bio->ptr = new NodeBIO();

  // XXX Why am I doing it?!
  bio->shutdown = 1;
  bio->init = 1;
  bio->num = -1;

  return 1;
}


int NodeBIO::Free(BIO* bio) {
  if (bio == NULL)
    return 0;

  if (bio->shutdown) {
    if (bio->init && bio->ptr != NULL) {
      delete FromBIO(bio);
      bio->ptr = NULL;
    }
  }

  return 1;
}


int NodeBIO::Read(BIO* bio, char* out, int len) {
  int bytes;
  BIO_clear_retry_flags(bio);

  bytes = FromBIO(bio)->Read(out, len);

  if (bytes == 0) {
    bytes = bio->num;
    if (bytes != 0) {
      BIO_set_retry_read(bio);
    }
  }

  return bytes;
}


char* NodeBIO::Peek(size_t* size) {
  *size = read_head_->write_pos_ - read_head_->read_pos_;
  return read_head_->data_ + read_head_->read_pos_;
}


size_t NodeBIO::PeekMultiple(char** out, size_t* size, size_t* count) {
  Buffer* pos = read_head_;
  size_t max = *count;
  size_t total = 0;

  size_t i;
  for (i = 0; i < max; i++) {
    size[i] = pos->write_pos_ - pos->read_pos_;
    total += size[i];
    out[i] = pos->data_ + pos->read_pos_;

    /* Don't get past write head */
    if (pos == write_head_)
      break;
    else
      pos = pos->next_;
  }

  if (i == max)
    *count = i;
  else
    *count = i + 1;

  return total;
}


int NodeBIO::Write(BIO* bio, const char* data, int len) {
  BIO_clear_retry_flags(bio);

  FromBIO(bio)->Write(data, len);

  return len;
}


int NodeBIO::Puts(BIO* bio, const char* str) {
  return Write(bio, str, strlen(str));
}


int NodeBIO::Gets(BIO* bio, char* out, int size) {
  NodeBIO* nbio =  FromBIO(bio);

  if (nbio->Length() == 0)
    return 0;

  int i = nbio->IndexOf('\n', size);

  // Include '\n', if it's there.  If not, don't read off the end.
  if (i < size && i >= 0 && static_cast<size_t>(i) < nbio->Length())
    i++;

  // Shift `i` a bit to NULL-terminate string later
  if (size == i)
    i--;

  // Flush read data
  nbio->Read(out, i);

  out[i] = 0;

  return i;
}


long NodeBIO::Ctrl(BIO* bio, int cmd, long num, void* ptr) {
  NodeBIO* nbio;
  long ret;

  nbio = FromBIO(bio);
  ret = 1;

  switch (cmd) {
    case BIO_CTRL_RESET:
      nbio->Reset();
      break;
    case BIO_CTRL_EOF:
      ret = nbio->Length() == 0;
      break;
    case BIO_C_SET_BUF_MEM_EOF_RETURN:
      bio->num = num;
      break;
    case BIO_CTRL_INFO:
      ret = nbio->Length();
      if (ptr != NULL)
        *reinterpret_cast<void**>(ptr) = NULL;
      break;
    case BIO_C_SET_BUF_MEM:
      assert(0 && "Can't use SET_BUF_MEM_PTR with NodeBIO");
      abort();
      break;
    case BIO_C_GET_BUF_MEM_PTR:
      assert(0 && "Can't use GET_BUF_MEM_PTR with NodeBIO");
      ret = 0;
      break;
    case BIO_CTRL_GET_CLOSE:
      ret = bio->shutdown;
      break;
    case BIO_CTRL_SET_CLOSE:
      bio->shutdown = num;
      break;
    case BIO_CTRL_WPENDING:
      ret = 0;
      break;
    case BIO_CTRL_PENDING:
      ret = nbio->Length();
      break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      ret = 1;
      break;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
      ret = 0;
      break;
  }
  return ret;
}


void NodeBIO::TryMoveReadHead() {
  // `read_pos_` and `write_pos_` means the position of the reader and writer
  // inside the buffer, respectively. When they're equal - its safe to reset
  // them, because both reader and writer will continue doing their stuff
  // from new (zero) positions.
  while (read_head_->read_pos_ != 0 &&
         read_head_->read_pos_ == read_head_->write_pos_) {
    // Reset positions
    read_head_->read_pos_ = 0;
    read_head_->write_pos_ = 0;

    // Move read_head_ forward, just in case if there're still some data to
    // read in the next buffer.
    if (read_head_ != write_head_)
      read_head_ = read_head_->next_;
  }
}


size_t NodeBIO::Read(char* out, size_t size) {
  size_t bytes_read = 0;
  size_t expected = Length() > size ? size : Length();
  size_t offset = 0;
  size_t left = size;

  while (bytes_read < expected) {
    assert(read_head_->read_pos_ <= read_head_->write_pos_);
    size_t avail = read_head_->write_pos_ - read_head_->read_pos_;
    if (avail > left)
      avail = left;

    // Copy data
    if (out != NULL)
      memcpy(out + offset, read_head_->data_ + read_head_->read_pos_, avail);
    read_head_->read_pos_ += avail;

    // Move pointers
    bytes_read += avail;
    offset += avail;
    left -= avail;

    TryMoveReadHead();
  }
  assert(expected == bytes_read);
  length_ -= bytes_read;

  // Free all empty buffers, but write_head's child
  FreeEmpty();

  return bytes_read;
}


void NodeBIO::FreeEmpty() {
  if (write_head_ == NULL)
    return;
  Buffer* child = write_head_->next_;
  if (child == write_head_ || child == read_head_)
    return;
  Buffer* cur = child->next_;
  if (cur == write_head_ || cur == read_head_)
    return;

  Buffer* prev = child;
  while (cur != read_head_) {
    assert(cur != write_head_);
    assert(cur->write_pos_ == cur->read_pos_);

    Buffer* next = cur->next_;
    delete cur;
    cur = next;
  }
  prev->next_ = cur;
}


size_t NodeBIO::IndexOf(char delim, size_t limit) {
  size_t bytes_read = 0;
  size_t max = Length() > limit ? limit : Length();
  size_t left = limit;
  Buffer* current = read_head_;

  while (bytes_read < max) {
    assert(current->read_pos_ <= current->write_pos_);
    size_t avail = current->write_pos_ - current->read_pos_;
    if (avail > left)
      avail = left;

    // Walk through data
    char* tmp = current->data_ + current->read_pos_;
    size_t off = 0;
    while (off < avail && *tmp != delim) {
      off++;
      tmp++;
    }

    // Move pointers
    bytes_read += off;
    left -= off;

    // Found `delim`
    if (off != avail) {
      return bytes_read;
    }

    // Move to next buffer
    if (current->read_pos_ + avail == current->len_) {
      current = current->next_;
    }
  }
  assert(max == bytes_read);

  return max;
}


void NodeBIO::Write(const char* data, size_t size) {
  size_t offset = 0;
  size_t left = size;

  // Allocate initial buffer if the ring is empty
  TryAllocateForWrite(left);

  while (left > 0) {
    size_t to_write = left;
    assert(write_head_->write_pos_ <= write_head_->len_);
    size_t avail = write_head_->len_ - write_head_->write_pos_;

    if (to_write > avail)
      to_write = avail;

    // Copy data
    memcpy(write_head_->data_ + write_head_->write_pos_,
           data + offset,
           to_write);

    // Move pointers
    left -= to_write;
    offset += to_write;
    length_ += to_write;
    write_head_->write_pos_ += to_write;
    assert(write_head_->write_pos_ <= write_head_->len_);

    // Go to next buffer if there still are some bytes to write
    if (left != 0) {
      assert(write_head_->write_pos_ == write_head_->len_);
      TryAllocateForWrite(left);
      write_head_ = write_head_->next_;

      // Additionally, since we're moved to the next buffer, read head
      // may be moved as well.
      TryMoveReadHead();
    }
  }
  assert(left == 0);
}


char* NodeBIO::PeekWritable(size_t* size) {
  TryAllocateForWrite(*size);

  size_t available = write_head_->len_ - write_head_->write_pos_;
  if (*size != 0 && available > *size)
    available = *size;
  else
    *size = available;

  return write_head_->data_ + write_head_->write_pos_;
}


void NodeBIO::Commit(size_t size) {
  write_head_->write_pos_ += size;
  length_ += size;
  assert(write_head_->write_pos_ <= write_head_->len_);

  // Allocate new buffer if write head is full,
  // and there're no other place to go
  TryAllocateForWrite(0);
  if (write_head_->write_pos_ == write_head_->len_) {
    write_head_ = write_head_->next_;

    // Additionally, since we're moved to the next buffer, read head
    // may be moved as well.
    TryMoveReadHead();
  }
}


void NodeBIO::TryAllocateForWrite(size_t hint) {
  Buffer* w = write_head_;
  Buffer* r = read_head_;
  // If write head is full, next buffer is either read head or not empty.
  if (w == NULL ||
      (w->write_pos_ == w->len_ &&
       (w->next_ == r || w->next_->write_pos_ != 0))) {
    size_t len = w == NULL ? initial_ :
                             kThroughputBufferLength;
    if (len < hint)
      len = hint;
    Buffer* next = new Buffer(len);

    if (w == NULL) {
      next->next_ = next;
      write_head_ = next;
      read_head_ = next;
    } else {
      next->next_ = w->next_;
      w->next_ = next;
    }
  }
}


void NodeBIO::Reset() {
  if (read_head_ == NULL)
    return;

  while (read_head_->read_pos_ != read_head_->write_pos_) {
    assert(read_head_->write_pos_ > read_head_->read_pos_);

    length_ -= read_head_->write_pos_ - read_head_->read_pos_;
    read_head_->write_pos_ = 0;
    read_head_->read_pos_ = 0;

    read_head_ = read_head_->next_;
  }
  write_head_ = read_head_;
  assert(length_ == 0);
}


NodeBIO::~NodeBIO() {
  if (read_head_ == NULL)
    return;

  Buffer* current = read_head_;
  do {
    Buffer* next = current->next_;
    delete current;
    current = next;
  } while (current != read_head_);

  read_head_ = NULL;
  write_head_ = NULL;
}

}  // namespace node
