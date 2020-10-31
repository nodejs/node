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

// This file declares the ByteSink and ByteSource abstract interfaces. These
// interfaces represent objects that consume (ByteSink) or produce (ByteSource)
// a sequence of bytes. Using these abstract interfaces in your APIs can help
// make your code work with a variety of input and output types.
//
// This file also declares the following commonly used implementations of these
// interfaces.
//
//   ByteSink:
//      UncheckedArrayByteSink  Writes to an array, without bounds checking
//      CheckedArrayByteSink    Writes to an array, with bounds checking
//      GrowingArrayByteSink    Allocates and writes to a growable buffer
//      StringByteSink          Writes to an STL string
//      NullByteSink            Consumes a never-ending stream of bytes
//
//   ByteSource:
//      ArrayByteSource         Reads from an array or string/StringPiece
//      LimitedByteSource       Limits the number of bytes read from an

#ifndef GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_
#define GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_

#include <stddef.h>
#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringpiece.h>

#include <google/protobuf/port_def.inc>

class CordByteSink;

namespace google {
namespace protobuf {
namespace strings {

// An abstract interface for an object that consumes a sequence of bytes. This
// interface offers a way to append data as well as a Flush() function.
//
// Example:
//
//   string my_data;
//   ...
//   ByteSink* sink = ...
//   sink->Append(my_data.data(), my_data.size());
//   sink->Flush();
//
class PROTOBUF_EXPORT ByteSink {
 public:
  ByteSink() {}
  virtual ~ByteSink() {}

  // Appends the "n" bytes starting at "bytes".
  virtual void Append(const char* bytes, size_t n) = 0;

  // Flushes internal buffers. The default implemenation does nothing. ByteSink
  // subclasses may use internal buffers that require calling Flush() at the end
  // of the stream.
  virtual void Flush();

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ByteSink);
};

// An abstract interface for an object that produces a fixed-size sequence of
// bytes.
//
// Example:
//
//   ByteSource* source = ...
//   while (source->Available() > 0) {
//     StringPiece data = source->Peek();
//     ... do something with "data" ...
//     source->Skip(data.length());
//   }
//
class PROTOBUF_EXPORT ByteSource {
 public:
  ByteSource() {}
  virtual ~ByteSource() {}

  // Returns the number of bytes left to read from the source. Available()
  // should decrease by N each time Skip(N) is called. Available() may not
  // increase. Available() returning 0 indicates that the ByteSource is
  // exhausted.
  //
  // Note: Size() may have been a more appropriate name as it's more
  //       indicative of the fixed-size nature of a ByteSource.
  virtual size_t Available() const = 0;

  // Returns a StringPiece of the next contiguous region of the source. Does not
  // reposition the source. The returned region is empty iff Available() == 0.
  //
  // The returned region is valid until the next call to Skip() or until this
  // object is destroyed, whichever occurs first.
  //
  // The length of the returned StringPiece will be <= Available().
  virtual StringPiece Peek() = 0;

  // Skips the next n bytes. Invalidates any StringPiece returned by a previous
  // call to Peek().
  //
  // REQUIRES: Available() >= n
  virtual void Skip(size_t n) = 0;

  // Writes the next n bytes in this ByteSource to the given ByteSink, and
  // advances this ByteSource past the copied bytes. The default implementation
  // of this method just copies the bytes normally, but subclasses might
  // override CopyTo to optimize certain cases.
  //
  // REQUIRES: Available() >= n
  virtual void CopyTo(ByteSink* sink, size_t n);

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ByteSource);
};

//
// Some commonly used implementations of ByteSink
//

// Implementation of ByteSink that writes to an unsized byte array. No
// bounds-checking is performed--it is the caller's responsibility to ensure
// that the destination array is large enough.
//
// Example:
//
//   char buf[10];
//   UncheckedArrayByteSink sink(buf);
//   sink.Append("hi", 2);    // OK
//   sink.Append(data, 100);  // WOOPS! Overflows buf[10].
//
class PROTOBUF_EXPORT UncheckedArrayByteSink : public ByteSink {
 public:
  explicit UncheckedArrayByteSink(char* dest) : dest_(dest) {}
  virtual void Append(const char* data, size_t n) override;

  // Returns the current output pointer so that a caller can see how many bytes
  // were produced.
  //
  // Note: this method is not part of the ByteSink interface.
  char* CurrentDestination() const { return dest_; }

 private:
  char* dest_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(UncheckedArrayByteSink);
};

// Implementation of ByteSink that writes to a sized byte array. This sink will
// not write more than "capacity" bytes to outbuf. Once "capacity" bytes are
// appended, subsequent bytes will be ignored and Overflowed() will return true.
// Overflowed() does not cause a runtime error (i.e., it does not CHECK fail).
//
// Example:
//
//   char buf[10];
//   CheckedArrayByteSink sink(buf, 10);
//   sink.Append("hi", 2);    // OK
//   sink.Append(data, 100);  // Will only write 8 more bytes
//
class PROTOBUF_EXPORT CheckedArrayByteSink : public ByteSink {
 public:
  CheckedArrayByteSink(char* outbuf, size_t capacity);
  virtual void Append(const char* bytes, size_t n) override;

  // Returns the number of bytes actually written to the sink.
  size_t NumberOfBytesWritten() const { return size_; }

  // Returns true if any bytes were discarded, i.e., if there was an
  // attempt to write more than 'capacity' bytes.
  bool Overflowed() const { return overflowed_; }

 private:
  char* outbuf_;
  const size_t capacity_;
  size_t size_;
  bool overflowed_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CheckedArrayByteSink);
};

// Implementation of ByteSink that allocates an internal buffer (a char array)
// and expands it as needed to accommodate appended data (similar to a string),
// and allows the caller to take ownership of the internal buffer via the
// GetBuffer() method. The buffer returned from GetBuffer() must be deleted by
// the caller with delete[]. GetBuffer() also sets the internal buffer to be
// empty, and subsequent appends to the sink will create a new buffer. The
// destructor will free the internal buffer if GetBuffer() was not called.
//
// Example:
//
//   GrowingArrayByteSink sink(10);
//   sink.Append("hi", 2);
//   sink.Append(data, n);
//   const char* buf = sink.GetBuffer();  // Ownership transferred
//   delete[] buf;
//
class PROTOBUF_EXPORT GrowingArrayByteSink : public strings::ByteSink {
 public:
  explicit GrowingArrayByteSink(size_t estimated_size);
  virtual ~GrowingArrayByteSink();
  virtual void Append(const char* bytes, size_t n) override;

  // Returns the allocated buffer, and sets nbytes to its size. The caller takes
  // ownership of the buffer and must delete it with delete[].
  char* GetBuffer(size_t* nbytes);

 private:
  void Expand(size_t amount);
  void ShrinkToFit();

  size_t capacity_;
  char* buf_;
  size_t size_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(GrowingArrayByteSink);
};

// Implementation of ByteSink that appends to the given string.
// Existing contents of "dest" are not modified; new data is appended.
//
// Example:
//
//   string dest = "Hello ";
//   StringByteSink sink(&dest);
//   sink.Append("World", 5);
//   assert(dest == "Hello World");
//
class PROTOBUF_EXPORT StringByteSink : public ByteSink {
 public:
  explicit StringByteSink(string* dest) : dest_(dest) {}
  virtual void Append(const char* data, size_t n) override;

 private:
  string* dest_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(StringByteSink);
};

// Implementation of ByteSink that discards all data.
//
// Example:
//
//   NullByteSink sink;
//   sink.Append(data, data.size());  // All data ignored.
//
class PROTOBUF_EXPORT NullByteSink : public ByteSink {
 public:
  NullByteSink() {}
  virtual void Append(const char *data, size_t n) override {}

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(NullByteSink);
};

//
// Some commonly used implementations of ByteSource
//

// Implementation of ByteSource that reads from a StringPiece.
//
// Example:
//
//   string data = "Hello";
//   ArrayByteSource source(data);
//   assert(source.Available() == 5);
//   assert(source.Peek() == "Hello");
//
class PROTOBUF_EXPORT ArrayByteSource : public ByteSource {
 public:
  explicit ArrayByteSource(StringPiece s) : input_(s) {}

  virtual size_t Available() const override;
  virtual StringPiece Peek() override;
  virtual void Skip(size_t n) override;

 private:
  StringPiece   input_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArrayByteSource);
};

// Implementation of ByteSource that wraps another ByteSource, limiting the
// number of bytes returned.
//
// The caller maintains ownership of the underlying source, and may not use the
// underlying source while using the LimitByteSource object.  The underlying
// source's pointer is advanced by n bytes every time this LimitByteSource
// object is advanced by n.
//
// Example:
//
//   string data = "Hello World";
//   ArrayByteSource abs(data);
//   assert(abs.Available() == data.size());
//
//   LimitByteSource limit(abs, 5);
//   assert(limit.Available() == 5);
//   assert(limit.Peek() == "Hello");
//
class PROTOBUF_EXPORT LimitByteSource : public ByteSource {
 public:
  // Returns at most "limit" bytes from "source".
  LimitByteSource(ByteSource* source, size_t limit);

  virtual size_t Available() const override;
  virtual StringPiece Peek() override;
  virtual void Skip(size_t n) override;

  // We override CopyTo so that we can forward to the underlying source, in
  // case it has an efficient implementation of CopyTo.
  virtual void CopyTo(ByteSink* sink, size_t n) override;

 private:
  ByteSource* source_;
  size_t limit_;
};

}  // namespace strings
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_
