// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file contains common implementations of the interfaces defined in
// zero_copy_stream.h which are included in the "lite" protobuf library.
// These implementations cover I/O on raw arrays and strings, as well as
// adaptors which make it easy to implement streams based on traditional
// streams.  Of course, many users will probably want to write their own
// implementations of these interfaces specific to the particular I/O
// abstractions they prefer to use, but these should cover the most common
// cases.

#ifndef GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_LITE_H__
#define GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_LITE_H__

#include <iosfwd>
#include <memory>
#include <string>
#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/stl_util.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace io {

// ===================================================================

// A ZeroCopyInputStream backed by an in-memory array of bytes.
class PROTOBUF_EXPORT ArrayInputStream : public ZeroCopyInputStream {
 public:
  // Create an InputStream that returns the bytes pointed to by "data".
  // "data" remains the property of the caller but must remain valid until
  // the stream is destroyed.  If a block_size is given, calls to Next()
  // will return data blocks no larger than the given size.  Otherwise, the
  // first call to Next() returns the entire array.  block_size is mainly
  // useful for testing; in production you would probably never want to set
  // it.
  ArrayInputStream(const void* data, int size, int block_size = -1);
  ~ArrayInputStream() override = default;

  // implements ZeroCopyInputStream ----------------------------------
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64 ByteCount() const override;


 private:
  const uint8* const data_;  // The byte array.
  const int size_;           // Total size of the array.
  const int block_size_;     // How many bytes to return at a time.

  int position_;
  int last_returned_size_;  // How many bytes we returned last time Next()
                            // was called (used for error checking only).

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArrayInputStream);
};

// ===================================================================

// A ZeroCopyOutputStream backed by an in-memory array of bytes.
class PROTOBUF_EXPORT ArrayOutputStream : public ZeroCopyOutputStream {
 public:
  // Create an OutputStream that writes to the bytes pointed to by "data".
  // "data" remains the property of the caller but must remain valid until
  // the stream is destroyed.  If a block_size is given, calls to Next()
  // will return data blocks no larger than the given size.  Otherwise, the
  // first call to Next() returns the entire array.  block_size is mainly
  // useful for testing; in production you would probably never want to set
  // it.
  ArrayOutputStream(void* data, int size, int block_size = -1);
  ~ArrayOutputStream() override = default;

  // implements ZeroCopyOutputStream ---------------------------------
  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  int64 ByteCount() const override;

 private:
  uint8* const data_;     // The byte array.
  const int size_;        // Total size of the array.
  const int block_size_;  // How many bytes to return at a time.

  int position_;
  int last_returned_size_;  // How many bytes we returned last time Next()
                            // was called (used for error checking only).

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArrayOutputStream);
};

// ===================================================================

// A ZeroCopyOutputStream which appends bytes to a string.
class PROTOBUF_EXPORT StringOutputStream : public ZeroCopyOutputStream {
 public:
  // Create a StringOutputStream which appends bytes to the given string.
  // The string remains property of the caller, but it is mutated in arbitrary
  // ways and MUST NOT be accessed in any way until you're done with the
  // stream. Either be sure there's no further usage, or (safest) destroy the
  // stream before using the contents.
  //
  // Hint:  If you call target->reserve(n) before creating the stream,
  //   the first call to Next() will return at least n bytes of buffer
  //   space.
  explicit StringOutputStream(std::string* target);
  ~StringOutputStream() override = default;

  // implements ZeroCopyOutputStream ---------------------------------
  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  int64 ByteCount() const override;

 private:
  static const int kMinimumSize = 16;

  std::string* target_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(StringOutputStream);
};

// Note:  There is no StringInputStream.  Instead, just create an
// ArrayInputStream as follows:
//   ArrayInputStream input(str.data(), str.size());

// ===================================================================

// A generic traditional input stream interface.
//
// Lots of traditional input streams (e.g. file descriptors, C stdio
// streams, and C++ iostreams) expose an interface where every read
// involves copying bytes into a buffer.  If you want to take such an
// interface and make a ZeroCopyInputStream based on it, simply implement
// CopyingInputStream and then use CopyingInputStreamAdaptor.
//
// CopyingInputStream implementations should avoid buffering if possible.
// CopyingInputStreamAdaptor does its own buffering and will read data
// in large blocks.
class PROTOBUF_EXPORT CopyingInputStream {
 public:
  virtual ~CopyingInputStream() {}

  // Reads up to "size" bytes into the given buffer.  Returns the number of
  // bytes read.  Read() waits until at least one byte is available, or
  // returns zero if no bytes will ever become available (EOF), or -1 if a
  // permanent read error occurred.
  virtual int Read(void* buffer, int size) = 0;

  // Skips the next "count" bytes of input.  Returns the number of bytes
  // actually skipped.  This will always be exactly equal to "count" unless
  // EOF was reached or a permanent read error occurred.
  //
  // The default implementation just repeatedly calls Read() into a scratch
  // buffer.
  virtual int Skip(int count);
};

// A ZeroCopyInputStream which reads from a CopyingInputStream.  This is
// useful for implementing ZeroCopyInputStreams that read from traditional
// streams.  Note that this class is not really zero-copy.
//
// If you want to read from file descriptors or C++ istreams, this is
// already implemented for you:  use FileInputStream or IstreamInputStream
// respectively.
class PROTOBUF_EXPORT CopyingInputStreamAdaptor : public ZeroCopyInputStream {
 public:
  // Creates a stream that reads from the given CopyingInputStream.
  // If a block_size is given, it specifies the number of bytes that
  // should be read and returned with each call to Next().  Otherwise,
  // a reasonable default is used.  The caller retains ownership of
  // copying_stream unless SetOwnsCopyingStream(true) is called.
  explicit CopyingInputStreamAdaptor(CopyingInputStream* copying_stream,
                                     int block_size = -1);
  ~CopyingInputStreamAdaptor() override;

  // Call SetOwnsCopyingStream(true) to tell the CopyingInputStreamAdaptor to
  // delete the underlying CopyingInputStream when it is destroyed.
  void SetOwnsCopyingStream(bool value) { owns_copying_stream_ = value; }

  // implements ZeroCopyInputStream ----------------------------------
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64 ByteCount() const override;

 private:
  // Insures that buffer_ is not NULL.
  void AllocateBufferIfNeeded();
  // Frees the buffer and resets buffer_used_.
  void FreeBuffer();

  // The underlying copying stream.
  CopyingInputStream* copying_stream_;
  bool owns_copying_stream_;

  // True if we have seen a permenant error from the underlying stream.
  bool failed_;

  // The current position of copying_stream_, relative to the point where
  // we started reading.
  int64 position_;

  // Data is read into this buffer.  It may be NULL if no buffer is currently
  // in use.  Otherwise, it points to an array of size buffer_size_.
  std::unique_ptr<uint8[]> buffer_;
  const int buffer_size_;

  // Number of valid bytes currently in the buffer (i.e. the size last
  // returned by Next()).  0 <= buffer_used_ <= buffer_size_.
  int buffer_used_;

  // Number of bytes in the buffer which were backed up over by a call to
  // BackUp().  These need to be returned again.
  // 0 <= backup_bytes_ <= buffer_used_
  int backup_bytes_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingInputStreamAdaptor);
};

// ===================================================================

// A generic traditional output stream interface.
//
// Lots of traditional output streams (e.g. file descriptors, C stdio
// streams, and C++ iostreams) expose an interface where every write
// involves copying bytes from a buffer.  If you want to take such an
// interface and make a ZeroCopyOutputStream based on it, simply implement
// CopyingOutputStream and then use CopyingOutputStreamAdaptor.
//
// CopyingOutputStream implementations should avoid buffering if possible.
// CopyingOutputStreamAdaptor does its own buffering and will write data
// in large blocks.
class PROTOBUF_EXPORT CopyingOutputStream {
 public:
  virtual ~CopyingOutputStream() {}

  // Writes "size" bytes from the given buffer to the output.  Returns true
  // if successful, false on a write error.
  virtual bool Write(const void* buffer, int size) = 0;
};

// A ZeroCopyOutputStream which writes to a CopyingOutputStream.  This is
// useful for implementing ZeroCopyOutputStreams that write to traditional
// streams.  Note that this class is not really zero-copy.
//
// If you want to write to file descriptors or C++ ostreams, this is
// already implemented for you:  use FileOutputStream or OstreamOutputStream
// respectively.
class PROTOBUF_EXPORT CopyingOutputStreamAdaptor : public ZeroCopyOutputStream {
 public:
  // Creates a stream that writes to the given Unix file descriptor.
  // If a block_size is given, it specifies the size of the buffers
  // that should be returned by Next().  Otherwise, a reasonable default
  // is used.
  explicit CopyingOutputStreamAdaptor(CopyingOutputStream* copying_stream,
                                      int block_size = -1);
  ~CopyingOutputStreamAdaptor() override;

  // Writes all pending data to the underlying stream.  Returns false if a
  // write error occurred on the underlying stream.  (The underlying
  // stream itself is not necessarily flushed.)
  bool Flush();

  // Call SetOwnsCopyingStream(true) to tell the CopyingOutputStreamAdaptor to
  // delete the underlying CopyingOutputStream when it is destroyed.
  void SetOwnsCopyingStream(bool value) { owns_copying_stream_ = value; }

  // implements ZeroCopyOutputStream ---------------------------------
  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  int64 ByteCount() const override;

 private:
  // Write the current buffer, if it is present.
  bool WriteBuffer();
  // Insures that buffer_ is not NULL.
  void AllocateBufferIfNeeded();
  // Frees the buffer.
  void FreeBuffer();

  // The underlying copying stream.
  CopyingOutputStream* copying_stream_;
  bool owns_copying_stream_;

  // True if we have seen a permenant error from the underlying stream.
  bool failed_;

  // The current position of copying_stream_, relative to the point where
  // we started writing.
  int64 position_;

  // Data is written from this buffer.  It may be NULL if no buffer is
  // currently in use.  Otherwise, it points to an array of size buffer_size_.
  std::unique_ptr<uint8[]> buffer_;
  const int buffer_size_;

  // Number of valid bytes currently in the buffer (i.e. the size last
  // returned by Next()).  When BackUp() is called, we just reduce this.
  // 0 <= buffer_used_ <= buffer_size_.
  int buffer_used_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingOutputStreamAdaptor);
};

// ===================================================================

// A ZeroCopyInputStream which wraps some other stream and limits it to
// a particular byte count.
class PROTOBUF_EXPORT LimitingInputStream : public ZeroCopyInputStream {
 public:
  LimitingInputStream(ZeroCopyInputStream* input, int64 limit);
  ~LimitingInputStream() override;

  // implements ZeroCopyInputStream ----------------------------------
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64 ByteCount() const override;


 private:
  ZeroCopyInputStream* input_;
  int64 limit_;  // Decreases as we go, becomes negative if we overshoot.
  int64 prior_bytes_read_;  // Bytes read on underlying stream at construction

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(LimitingInputStream);
};


// ===================================================================

// mutable_string_data() and as_string_data() are workarounds to improve
// the performance of writing new data to an existing string.  Unfortunately
// the methods provided by the string class are suboptimal, and using memcpy()
// is mildly annoying because it requires its pointer args to be non-NULL even
// if we ask it to copy 0 bytes.  Furthermore, string_as_array() has the
// property that it always returns NULL if its arg is the empty string, exactly
// what we want to avoid if we're using it in conjunction with memcpy()!
// With C++11, the desired memcpy() boils down to memcpy(..., &(*s)[0], size),
// where s is a string*.  Without C++11, &(*s)[0] is not guaranteed to be safe,
// so we use string_as_array(), and live with the extra logic that tests whether
// *s is empty.

// Return a pointer to mutable characters underlying the given string.  The
// return value is valid until the next time the string is resized.  We
// trust the caller to treat the return value as an array of length s->size().
inline char* mutable_string_data(std::string* s) {
  // This should be simpler & faster than string_as_array() because the latter
  // is guaranteed to return NULL when *s is empty, so it has to check for that.
  return &(*s)[0];
}

// as_string_data(s) is equivalent to
//  ({ char* p = mutable_string_data(s); make_pair(p, p != NULL); })
// Sometimes it's faster: in some scenarios p cannot be NULL, and then the
// code can avoid that check.
inline std::pair<char*, bool> as_string_data(std::string* s) {
  char* p = mutable_string_data(s);
  return std::make_pair(p, true);
}

}  // namespace io
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_LITE_H__
