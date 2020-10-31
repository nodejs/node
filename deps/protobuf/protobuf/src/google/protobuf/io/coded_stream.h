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
// This file contains the CodedInputStream and CodedOutputStream classes,
// which wrap a ZeroCopyInputStream or ZeroCopyOutputStream, respectively,
// and allow you to read or write individual pieces of data in various
// formats.  In particular, these implement the varint encoding for
// integers, a simple variable-length encoding in which smaller numbers
// take fewer bytes.
//
// Typically these classes will only be used internally by the protocol
// buffer library in order to encode and decode protocol buffers.  Clients
// of the library only need to know about this class if they wish to write
// custom message parsing or serialization procedures.
//
// CodedOutputStream example:
//   // Write some data to "myfile".  First we write a 4-byte "magic number"
//   // to identify the file type, then write a length-delimited string.  The
//   // string is composed of a varint giving the length followed by the raw
//   // bytes.
//   int fd = open("myfile", O_CREAT | O_WRONLY);
//   ZeroCopyOutputStream* raw_output = new FileOutputStream(fd);
//   CodedOutputStream* coded_output = new CodedOutputStream(raw_output);
//
//   int magic_number = 1234;
//   char text[] = "Hello world!";
//   coded_output->WriteLittleEndian32(magic_number);
//   coded_output->WriteVarint32(strlen(text));
//   coded_output->WriteRaw(text, strlen(text));
//
//   delete coded_output;
//   delete raw_output;
//   close(fd);
//
// CodedInputStream example:
//   // Read a file created by the above code.
//   int fd = open("myfile", O_RDONLY);
//   ZeroCopyInputStream* raw_input = new FileInputStream(fd);
//   CodedInputStream* coded_input = new CodedInputStream(raw_input);
//
//   coded_input->ReadLittleEndian32(&magic_number);
//   if (magic_number != 1234) {
//     cerr << "File not in expected format." << endl;
//     return;
//   }
//
//   uint32 size;
//   coded_input->ReadVarint32(&size);
//
//   char* text = new char[size + 1];
//   coded_input->ReadRaw(buffer, size);
//   text[size] = '\0';
//
//   delete coded_input;
//   delete raw_input;
//   close(fd);
//
//   cout << "Text is: " << text << endl;
//   delete [] text;
//
// For those who are interested, varint encoding is defined as follows:
//
// The encoding operates on unsigned integers of up to 64 bits in length.
// Each byte of the encoded value has the format:
// * bits 0-6: Seven bits of the number being encoded.
// * bit 7: Zero if this is the last byte in the encoding (in which
//   case all remaining bits of the number are zero) or 1 if
//   more bytes follow.
// The first byte contains the least-significant 7 bits of the number, the
// second byte (if present) contains the next-least-significant 7 bits,
// and so on.  So, the binary number 1011000101011 would be encoded in two
// bytes as "10101011 00101100".
//
// In theory, varint could be used to encode integers of any length.
// However, for practicality we set a limit at 64 bits.  The maximum encoded
// length of a number is thus 10 bytes.

#ifndef GOOGLE_PROTOBUF_IO_CODED_STREAM_H__
#define GOOGLE_PROTOBUF_IO_CODED_STREAM_H__

#include <assert.h>
#include <atomic>
#include <climits>
#include <string>
#include <utility>
#ifdef _MSC_VER
// Assuming windows is always little-endian.
#if !defined(PROTOBUF_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
#define PROTOBUF_LITTLE_ENDIAN 1
#endif
#if _MSC_VER >= 1300 && !defined(__INTEL_COMPILER)
// If MSVC has "/RTCc" set, it will complain about truncating casts at
// runtime.  This file contains some intentional truncating casts.
#pragma runtime_checks("c", off)
#endif
#else
#include <sys/param.h>  // __BYTE_ORDER
#if ((defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)) ||    \
     (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN)) && \
    !defined(PROTOBUF_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
#define PROTOBUF_LITTLE_ENDIAN 1
#endif
#endif
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/port.h>
#include <google/protobuf/stubs/port.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class DescriptorPool;
class MessageFactory;
class ZeroCopyCodedInputStream;

namespace internal {
void MapTestForceDeterministic();
class EpsCopyByteStream;
}  // namespace internal

namespace io {

// Defined in this file.
class CodedInputStream;
class CodedOutputStream;

// Defined in other files.
class ZeroCopyInputStream;   // zero_copy_stream.h
class ZeroCopyOutputStream;  // zero_copy_stream.h

// Class which reads and decodes binary data which is composed of varint-
// encoded integers and fixed-width pieces.  Wraps a ZeroCopyInputStream.
// Most users will not need to deal with CodedInputStream.
//
// Most methods of CodedInputStream that return a bool return false if an
// underlying I/O error occurs or if the data is malformed.  Once such a
// failure occurs, the CodedInputStream is broken and is no longer useful.
// After a failure, callers also should assume writes to "out" args may have
// occurred, though nothing useful can be determined from those writes.
class PROTOBUF_EXPORT CodedInputStream {
 public:
  // Create a CodedInputStream that reads from the given ZeroCopyInputStream.
  explicit CodedInputStream(ZeroCopyInputStream* input);

  // Create a CodedInputStream that reads from the given flat array.  This is
  // faster than using an ArrayInputStream.  PushLimit(size) is implied by
  // this constructor.
  explicit CodedInputStream(const uint8* buffer, int size);

  // Destroy the CodedInputStream and position the underlying
  // ZeroCopyInputStream at the first unread byte.  If an error occurred while
  // reading (causing a method to return false), then the exact position of
  // the input stream may be anywhere between the last value that was read
  // successfully and the stream's byte limit.
  ~CodedInputStream();

  // Return true if this CodedInputStream reads from a flat array instead of
  // a ZeroCopyInputStream.
  inline bool IsFlat() const;

  // Skips a number of bytes.  Returns false if an underlying read error
  // occurs.
  inline bool Skip(int count);

  // Sets *data to point directly at the unread part of the CodedInputStream's
  // underlying buffer, and *size to the size of that buffer, but does not
  // advance the stream's current position.  This will always either produce
  // a non-empty buffer or return false.  If the caller consumes any of
  // this data, it should then call Skip() to skip over the consumed bytes.
  // This may be useful for implementing external fast parsing routines for
  // types of data not covered by the CodedInputStream interface.
  bool GetDirectBufferPointer(const void** data, int* size);

  // Like GetDirectBufferPointer, but this method is inlined, and does not
  // attempt to Refresh() if the buffer is currently empty.
  PROTOBUF_ALWAYS_INLINE
  void GetDirectBufferPointerInline(const void** data, int* size);

  // Read raw bytes, copying them into the given buffer.
  bool ReadRaw(void* buffer, int size);

  // Like the above, with inlined optimizations. This should only be used
  // by the protobuf implementation.
  PROTOBUF_ALWAYS_INLINE
  bool InternalReadRawInline(void* buffer, int size);

  // Like ReadRaw, but reads into a string.
  bool ReadString(std::string* buffer, int size);
  // Like the above, with inlined optimizations. This should only be used
  // by the protobuf implementation.
  PROTOBUF_ALWAYS_INLINE
  bool InternalReadStringInline(std::string* buffer, int size);


  // Read a 32-bit little-endian integer.
  bool ReadLittleEndian32(uint32* value);
  // Read a 64-bit little-endian integer.
  bool ReadLittleEndian64(uint64* value);

  // These methods read from an externally provided buffer. The caller is
  // responsible for ensuring that the buffer has sufficient space.
  // Read a 32-bit little-endian integer.
  static const uint8* ReadLittleEndian32FromArray(const uint8* buffer,
                                                  uint32* value);
  // Read a 64-bit little-endian integer.
  static const uint8* ReadLittleEndian64FromArray(const uint8* buffer,
                                                  uint64* value);

  // Read an unsigned integer with Varint encoding, truncating to 32 bits.
  // Reading a 32-bit value is equivalent to reading a 64-bit one and casting
  // it to uint32, but may be more efficient.
  bool ReadVarint32(uint32* value);
  // Read an unsigned integer with Varint encoding.
  bool ReadVarint64(uint64* value);

  // Reads a varint off the wire into an "int". This should be used for reading
  // sizes off the wire (sizes of strings, submessages, bytes fields, etc).
  //
  // The value from the wire is interpreted as unsigned.  If its value exceeds
  // the representable value of an integer on this platform, instead of
  // truncating we return false. Truncating (as performed by ReadVarint32()
  // above) is an acceptable approach for fields representing an integer, but
  // when we are parsing a size from the wire, truncating the value would result
  // in us misparsing the payload.
  bool ReadVarintSizeAsInt(int* value);

  // Read a tag.  This calls ReadVarint32() and returns the result, or returns
  // zero (which is not a valid tag) if ReadVarint32() fails.  Also, ReadTag
  // (but not ReadTagNoLastTag) updates the last tag value, which can be checked
  // with LastTagWas().
  //
  // Always inline because this is only called in one place per parse loop
  // but it is called for every iteration of said loop, so it should be fast.
  // GCC doesn't want to inline this by default.
  PROTOBUF_ALWAYS_INLINE uint32 ReadTag() {
    return last_tag_ = ReadTagNoLastTag();
  }

  PROTOBUF_ALWAYS_INLINE uint32 ReadTagNoLastTag();

  // This usually a faster alternative to ReadTag() when cutoff is a manifest
  // constant.  It does particularly well for cutoff >= 127.  The first part
  // of the return value is the tag that was read, though it can also be 0 in
  // the cases where ReadTag() would return 0.  If the second part is true
  // then the tag is known to be in [0, cutoff].  If not, the tag either is
  // above cutoff or is 0.  (There's intentional wiggle room when tag is 0,
  // because that can arise in several ways, and for best performance we want
  // to avoid an extra "is tag == 0?" check here.)
  PROTOBUF_ALWAYS_INLINE
  std::pair<uint32, bool> ReadTagWithCutoff(uint32 cutoff) {
    std::pair<uint32, bool> result = ReadTagWithCutoffNoLastTag(cutoff);
    last_tag_ = result.first;
    return result;
  }

  PROTOBUF_ALWAYS_INLINE
  std::pair<uint32, bool> ReadTagWithCutoffNoLastTag(uint32 cutoff);

  // Usually returns true if calling ReadVarint32() now would produce the given
  // value.  Will always return false if ReadVarint32() would not return the
  // given value.  If ExpectTag() returns true, it also advances past
  // the varint.  For best performance, use a compile-time constant as the
  // parameter.
  // Always inline because this collapses to a small number of instructions
  // when given a constant parameter, but GCC doesn't want to inline by default.
  PROTOBUF_ALWAYS_INLINE bool ExpectTag(uint32 expected);

  // Like above, except this reads from the specified buffer. The caller is
  // responsible for ensuring that the buffer is large enough to read a varint
  // of the expected size. For best performance, use a compile-time constant as
  // the expected tag parameter.
  //
  // Returns a pointer beyond the expected tag if it was found, or NULL if it
  // was not.
  PROTOBUF_ALWAYS_INLINE
  static const uint8* ExpectTagFromArray(const uint8* buffer, uint32 expected);

  // Usually returns true if no more bytes can be read.  Always returns false
  // if more bytes can be read.  If ExpectAtEnd() returns true, a subsequent
  // call to LastTagWas() will act as if ReadTag() had been called and returned
  // zero, and ConsumedEntireMessage() will return true.
  bool ExpectAtEnd();

  // If the last call to ReadTag() or ReadTagWithCutoff() returned the given
  // value, returns true.  Otherwise, returns false.
  // ReadTagNoLastTag/ReadTagWithCutoffNoLastTag do not preserve the last
  // returned value.
  //
  // This is needed because parsers for some types of embedded messages
  // (with field type TYPE_GROUP) don't actually know that they've reached the
  // end of a message until they see an ENDGROUP tag, which was actually part
  // of the enclosing message.  The enclosing message would like to check that
  // tag to make sure it had the right number, so it calls LastTagWas() on
  // return from the embedded parser to check.
  bool LastTagWas(uint32 expected);
  void SetLastTag(uint32 tag) { last_tag_ = tag; }

  // When parsing message (but NOT a group), this method must be called
  // immediately after MergeFromCodedStream() returns (if it returns true)
  // to further verify that the message ended in a legitimate way.  For
  // example, this verifies that parsing did not end on an end-group tag.
  // It also checks for some cases where, due to optimizations,
  // MergeFromCodedStream() can incorrectly return true.
  bool ConsumedEntireMessage();
  void SetConsumed() { legitimate_message_end_ = true; }

  // Limits ----------------------------------------------------------
  // Limits are used when parsing length-delimited embedded messages.
  // After the message's length is read, PushLimit() is used to prevent
  // the CodedInputStream from reading beyond that length.  Once the
  // embedded message has been parsed, PopLimit() is called to undo the
  // limit.

  // Opaque type used with PushLimit() and PopLimit().  Do not modify
  // values of this type yourself.  The only reason that this isn't a
  // struct with private internals is for efficiency.
  typedef int Limit;

  // Places a limit on the number of bytes that the stream may read,
  // starting from the current position.  Once the stream hits this limit,
  // it will act like the end of the input has been reached until PopLimit()
  // is called.
  //
  // As the names imply, the stream conceptually has a stack of limits.  The
  // shortest limit on the stack is always enforced, even if it is not the
  // top limit.
  //
  // The value returned by PushLimit() is opaque to the caller, and must
  // be passed unchanged to the corresponding call to PopLimit().
  Limit PushLimit(int byte_limit);

  // Pops the last limit pushed by PushLimit().  The input must be the value
  // returned by that call to PushLimit().
  void PopLimit(Limit limit);

  // Returns the number of bytes left until the nearest limit on the
  // stack is hit, or -1 if no limits are in place.
  int BytesUntilLimit() const;

  // Returns current position relative to the beginning of the input stream.
  int CurrentPosition() const;

  // Total Bytes Limit -----------------------------------------------
  // To prevent malicious users from sending excessively large messages
  // and causing memory exhaustion, CodedInputStream imposes a hard limit on
  // the total number of bytes it will read.

  // Sets the maximum number of bytes that this CodedInputStream will read
  // before refusing to continue.  To prevent servers from allocating enormous
  // amounts of memory to hold parsed messages, the maximum message length
  // should be limited to the shortest length that will not harm usability.
  // The default limit is INT_MAX (~2GB) and apps should set shorter limits
  // if possible. An error will always be printed to stderr if the limit is
  // reached.
  //
  // Note: setting a limit less than the current read position is interpreted
  // as a limit on the current position.
  //
  // This is unrelated to PushLimit()/PopLimit().
  void SetTotalBytesLimit(int total_bytes_limit);

  PROTOBUF_DEPRECATED_MSG(
      "Please use the single parameter version of SetTotalBytesLimit(). The "
      "second parameter is ignored.")
  void SetTotalBytesLimit(int total_bytes_limit, int) {
    SetTotalBytesLimit(total_bytes_limit);
  }

  // The Total Bytes Limit minus the Current Position, or -1 if the total bytes
  // limit is INT_MAX.
  int BytesUntilTotalBytesLimit() const;

  // Recursion Limit -------------------------------------------------
  // To prevent corrupt or malicious messages from causing stack overflows,
  // we must keep track of the depth of recursion when parsing embedded
  // messages and groups.  CodedInputStream keeps track of this because it
  // is the only object that is passed down the stack during parsing.

  // Sets the maximum recursion depth.  The default is 100.
  void SetRecursionLimit(int limit);
  int RecursionBudget() { return recursion_budget_; }

  static int GetDefaultRecursionLimit() { return default_recursion_limit_; }

  // Increments the current recursion depth.  Returns true if the depth is
  // under the limit, false if it has gone over.
  bool IncrementRecursionDepth();

  // Decrements the recursion depth if possible.
  void DecrementRecursionDepth();

  // Decrements the recursion depth blindly.  This is faster than
  // DecrementRecursionDepth().  It should be used only if all previous
  // increments to recursion depth were successful.
  void UnsafeDecrementRecursionDepth();

  // Shorthand for make_pair(PushLimit(byte_limit), --recursion_budget_).
  // Using this can reduce code size and complexity in some cases.  The caller
  // is expected to check that the second part of the result is non-negative (to
  // bail out if the depth of recursion is too high) and, if all is well, to
  // later pass the first part of the result to PopLimit() or similar.
  std::pair<CodedInputStream::Limit, int> IncrementRecursionDepthAndPushLimit(
      int byte_limit);

  // Shorthand for PushLimit(ReadVarint32(&length) ? length : 0).
  Limit ReadLengthAndPushLimit();

  // Helper that is equivalent to: {
  //  bool result = ConsumedEntireMessage();
  //  PopLimit(limit);
  //  UnsafeDecrementRecursionDepth();
  //  return result; }
  // Using this can reduce code size and complexity in some cases.
  // Do not use unless the current recursion depth is greater than zero.
  bool DecrementRecursionDepthAndPopLimit(Limit limit);

  // Helper that is equivalent to: {
  //  bool result = ConsumedEntireMessage();
  //  PopLimit(limit);
  //  return result; }
  // Using this can reduce code size and complexity in some cases.
  bool CheckEntireMessageConsumedAndPopLimit(Limit limit);

  // Extension Registry ----------------------------------------------
  // ADVANCED USAGE:  99.9% of people can ignore this section.
  //
  // By default, when parsing extensions, the parser looks for extension
  // definitions in the pool which owns the outer message's Descriptor.
  // However, you may call SetExtensionRegistry() to provide an alternative
  // pool instead.  This makes it possible, for example, to parse a message
  // using a generated class, but represent some extensions using
  // DynamicMessage.

  // Set the pool used to look up extensions.  Most users do not need to call
  // this as the correct pool will be chosen automatically.
  //
  // WARNING:  It is very easy to misuse this.  Carefully read the requirements
  //   below.  Do not use this unless you are sure you need it.  Almost no one
  //   does.
  //
  // Let's say you are parsing a message into message object m, and you want
  // to take advantage of SetExtensionRegistry().  You must follow these
  // requirements:
  //
  // The given DescriptorPool must contain m->GetDescriptor().  It is not
  // sufficient for it to simply contain a descriptor that has the same name
  // and content -- it must be the *exact object*.  In other words:
  //   assert(pool->FindMessageTypeByName(m->GetDescriptor()->full_name()) ==
  //          m->GetDescriptor());
  // There are two ways to satisfy this requirement:
  // 1) Use m->GetDescriptor()->pool() as the pool.  This is generally useless
  //    because this is the pool that would be used anyway if you didn't call
  //    SetExtensionRegistry() at all.
  // 2) Use a DescriptorPool which has m->GetDescriptor()->pool() as an
  //    "underlay".  Read the documentation for DescriptorPool for more
  //    information about underlays.
  //
  // You must also provide a MessageFactory.  This factory will be used to
  // construct Message objects representing extensions.  The factory's
  // GetPrototype() MUST return non-NULL for any Descriptor which can be found
  // through the provided pool.
  //
  // If the provided factory might return instances of protocol-compiler-
  // generated (i.e. compiled-in) types, or if the outer message object m is
  // a generated type, then the given factory MUST have this property:  If
  // GetPrototype() is given a Descriptor which resides in
  // DescriptorPool::generated_pool(), the factory MUST return the same
  // prototype which MessageFactory::generated_factory() would return.  That
  // is, given a descriptor for a generated type, the factory must return an
  // instance of the generated class (NOT DynamicMessage).  However, when
  // given a descriptor for a type that is NOT in generated_pool, the factory
  // is free to return any implementation.
  //
  // The reason for this requirement is that generated sub-objects may be
  // accessed via the standard (non-reflection) extension accessor methods,
  // and these methods will down-cast the object to the generated class type.
  // If the object is not actually of that type, the results would be undefined.
  // On the other hand, if an extension is not compiled in, then there is no
  // way the code could end up accessing it via the standard accessors -- the
  // only way to access the extension is via reflection.  When using reflection,
  // DynamicMessage and generated messages are indistinguishable, so it's fine
  // if these objects are represented using DynamicMessage.
  //
  // Using DynamicMessageFactory on which you have called
  // SetDelegateToGeneratedFactory(true) should be sufficient to satisfy the
  // above requirement.
  //
  // If either pool or factory is NULL, both must be NULL.
  //
  // Note that this feature is ignored when parsing "lite" messages as they do
  // not have descriptors.
  void SetExtensionRegistry(const DescriptorPool* pool,
                            MessageFactory* factory);

  // Get the DescriptorPool set via SetExtensionRegistry(), or NULL if no pool
  // has been provided.
  const DescriptorPool* GetExtensionPool();

  // Get the MessageFactory set via SetExtensionRegistry(), or NULL if no
  // factory has been provided.
  MessageFactory* GetExtensionFactory();

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CodedInputStream);

  const uint8* buffer_;
  const uint8* buffer_end_;  // pointer to the end of the buffer.
  ZeroCopyInputStream* input_;
  int total_bytes_read_;  // total bytes read from input_, including
                          // the current buffer

  // If total_bytes_read_ surpasses INT_MAX, we record the extra bytes here
  // so that we can BackUp() on destruction.
  int overflow_bytes_;

  // LastTagWas() stuff.
  uint32 last_tag_;  // result of last ReadTag() or ReadTagWithCutoff().

  // This is set true by ReadTag{Fallback/Slow}() if it is called when exactly
  // at EOF, or by ExpectAtEnd() when it returns true.  This happens when we
  // reach the end of a message and attempt to read another tag.
  bool legitimate_message_end_;

  // See EnableAliasing().
  bool aliasing_enabled_;

  // Limits
  Limit current_limit_;  // if position = -1, no limit is applied

  // For simplicity, if the current buffer crosses a limit (either a normal
  // limit created by PushLimit() or the total bytes limit), buffer_size_
  // only tracks the number of bytes before that limit.  This field
  // contains the number of bytes after it.  Note that this implies that if
  // buffer_size_ == 0 and buffer_size_after_limit_ > 0, we know we've
  // hit a limit.  However, if both are zero, it doesn't necessarily mean
  // we aren't at a limit -- the buffer may have ended exactly at the limit.
  int buffer_size_after_limit_;

  // Maximum number of bytes to read, period.  This is unrelated to
  // current_limit_.  Set using SetTotalBytesLimit().
  int total_bytes_limit_;

  // Current recursion budget, controlled by IncrementRecursionDepth() and
  // similar.  Starts at recursion_limit_ and goes down: if this reaches
  // -1 we are over budget.
  int recursion_budget_;
  // Recursion depth limit, set by SetRecursionLimit().
  int recursion_limit_;

  // See SetExtensionRegistry().
  const DescriptorPool* extension_pool_;
  MessageFactory* extension_factory_;

  // Private member functions.

  // Fallback when Skip() goes past the end of the current buffer.
  bool SkipFallback(int count, int original_buffer_size);

  // Advance the buffer by a given number of bytes.
  void Advance(int amount);

  // Back up input_ to the current buffer position.
  void BackUpInputToCurrentPosition();

  // Recomputes the value of buffer_size_after_limit_.  Must be called after
  // current_limit_ or total_bytes_limit_ changes.
  void RecomputeBufferLimits();

  // Writes an error message saying that we hit total_bytes_limit_.
  void PrintTotalBytesLimitError();

  // Called when the buffer runs out to request more data.  Implies an
  // Advance(BufferSize()).
  bool Refresh();

  // When parsing varints, we optimize for the common case of small values, and
  // then optimize for the case when the varint fits within the current buffer
  // piece. The Fallback method is used when we can't use the one-byte
  // optimization. The Slow method is yet another fallback when the buffer is
  // not large enough. Making the slow path out-of-line speeds up the common
  // case by 10-15%. The slow path is fairly uncommon: it only triggers when a
  // message crosses multiple buffers.  Note: ReadVarint32Fallback() and
  // ReadVarint64Fallback() are called frequently and generally not inlined, so
  // they have been optimized to avoid "out" parameters.  The former returns -1
  // if it fails and the uint32 it read otherwise.  The latter has a bool
  // indicating success or failure as part of its return type.
  int64 ReadVarint32Fallback(uint32 first_byte_or_zero);
  int ReadVarintSizeAsIntFallback();
  std::pair<uint64, bool> ReadVarint64Fallback();
  bool ReadVarint32Slow(uint32* value);
  bool ReadVarint64Slow(uint64* value);
  int ReadVarintSizeAsIntSlow();
  bool ReadLittleEndian32Fallback(uint32* value);
  bool ReadLittleEndian64Fallback(uint64* value);

  // Fallback/slow methods for reading tags. These do not update last_tag_,
  // but will set legitimate_message_end_ if we are at the end of the input
  // stream.
  uint32 ReadTagFallback(uint32 first_byte_or_zero);
  uint32 ReadTagSlow();
  bool ReadStringFallback(std::string* buffer, int size);

  // Return the size of the buffer.
  int BufferSize() const;

  static const int kDefaultTotalBytesLimit = INT_MAX;

  static int default_recursion_limit_;  // 100 by default.

  friend class google::protobuf::ZeroCopyCodedInputStream;
  friend class google::protobuf::internal::EpsCopyByteStream;
};

// Class which encodes and writes binary data which is composed of varint-
// encoded integers and fixed-width pieces.  Wraps a ZeroCopyOutputStream.
// Most users will not need to deal with CodedOutputStream.
//
// Most methods of CodedOutputStream which return a bool return false if an
// underlying I/O error occurs.  Once such a failure occurs, the
// CodedOutputStream is broken and is no longer useful. The Write* methods do
// not return the stream status, but will invalidate the stream if an error
// occurs. The client can probe HadError() to determine the status.
//
// Note that every method of CodedOutputStream which writes some data has
// a corresponding static "ToArray" version. These versions write directly
// to the provided buffer, returning a pointer past the last written byte.
// They require that the buffer has sufficient capacity for the encoded data.
// This allows an optimization where we check if an output stream has enough
// space for an entire message before we start writing and, if there is, we
// call only the ToArray methods to avoid doing bound checks for each
// individual value.
// i.e., in the example above:
//
//   CodedOutputStream* coded_output = new CodedOutputStream(raw_output);
//   int magic_number = 1234;
//   char text[] = "Hello world!";
//
//   int coded_size = sizeof(magic_number) +
//                    CodedOutputStream::VarintSize32(strlen(text)) +
//                    strlen(text);
//
//   uint8* buffer =
//       coded_output->GetDirectBufferForNBytesAndAdvance(coded_size);
//   if (buffer != NULL) {
//     // The output stream has enough space in the buffer: write directly to
//     // the array.
//     buffer = CodedOutputStream::WriteLittleEndian32ToArray(magic_number,
//                                                            buffer);
//     buffer = CodedOutputStream::WriteVarint32ToArray(strlen(text), buffer);
//     buffer = CodedOutputStream::WriteRawToArray(text, strlen(text), buffer);
//   } else {
//     // Make bound-checked writes, which will ask the underlying stream for
//     // more space as needed.
//     coded_output->WriteLittleEndian32(magic_number);
//     coded_output->WriteVarint32(strlen(text));
//     coded_output->WriteRaw(text, strlen(text));
//   }
//
//   delete coded_output;
class PROTOBUF_EXPORT CodedOutputStream {
 public:
  // Create an CodedOutputStream that writes to the given ZeroCopyOutputStream.
  explicit CodedOutputStream(ZeroCopyOutputStream* output);
  CodedOutputStream(ZeroCopyOutputStream* output, bool do_eager_refresh);

  // Destroy the CodedOutputStream and position the underlying
  // ZeroCopyOutputStream immediately after the last byte written.
  ~CodedOutputStream();

  // Trims any unused space in the underlying buffer so that its size matches
  // the number of bytes written by this stream. The underlying buffer will
  // automatically be trimmed when this stream is destroyed; this call is only
  // necessary if the underlying buffer is accessed *before* the stream is
  // destroyed.
  void Trim();

  // Skips a number of bytes, leaving the bytes unmodified in the underlying
  // buffer.  Returns false if an underlying write error occurs.  This is
  // mainly useful with GetDirectBufferPointer().
  // Note of caution, the skipped bytes may contain uninitialized data. The
  // caller must make sure that the skipped bytes are properly initialized,
  // otherwise you might leak bytes from your heap.
  bool Skip(int count);

  // Sets *data to point directly at the unwritten part of the
  // CodedOutputStream's underlying buffer, and *size to the size of that
  // buffer, but does not advance the stream's current position.  This will
  // always either produce a non-empty buffer or return false.  If the caller
  // writes any data to this buffer, it should then call Skip() to skip over
  // the consumed bytes.  This may be useful for implementing external fast
  // serialization routines for types of data not covered by the
  // CodedOutputStream interface.
  bool GetDirectBufferPointer(void** data, int* size);

  // If there are at least "size" bytes available in the current buffer,
  // returns a pointer directly into the buffer and advances over these bytes.
  // The caller may then write directly into this buffer (e.g. using the
  // *ToArray static methods) rather than go through CodedOutputStream.  If
  // there are not enough bytes available, returns NULL.  The return pointer is
  // invalidated as soon as any other non-const method of CodedOutputStream
  // is called.
  inline uint8* GetDirectBufferForNBytesAndAdvance(int size);

  // Write raw bytes, copying them from the given buffer.
  void WriteRaw(const void* buffer, int size);
  // Like WriteRaw()  but will try to write aliased data if aliasing is
  // turned on.
  void WriteRawMaybeAliased(const void* data, int size);
  // Like WriteRaw()  but writing directly to the target array.
  // This is _not_ inlined, as the compiler often optimizes memcpy into inline
  // copy loops. Since this gets called by every field with string or bytes
  // type, inlining may lead to a significant amount of code bloat, with only a
  // minor performance gain.
  static uint8* WriteRawToArray(const void* buffer, int size, uint8* target);

  // Equivalent to WriteRaw(str.data(), str.size()).
  void WriteString(const std::string& str);
  // Like WriteString()  but writing directly to the target array.
  static uint8* WriteStringToArray(const std::string& str, uint8* target);
  // Write the varint-encoded size of str followed by str.
  static uint8* WriteStringWithSizeToArray(const std::string& str,
                                           uint8* target);


  // Instructs the CodedOutputStream to allow the underlying
  // ZeroCopyOutputStream to hold pointers to the original structure instead of
  // copying, if it supports it (i.e. output->AllowsAliasing() is true).  If the
  // underlying stream does not support aliasing, then enabling it has no
  // affect.  For now, this only affects the behavior of
  // WriteRawMaybeAliased().
  //
  // NOTE: It is caller's responsibility to ensure that the chunk of memory
  // remains live until all of the data has been consumed from the stream.
  void EnableAliasing(bool enabled);

  // Write a 32-bit little-endian integer.
  void WriteLittleEndian32(uint32 value);
  // Like WriteLittleEndian32()  but writing directly to the target array.
  static uint8* WriteLittleEndian32ToArray(uint32 value, uint8* target);
  // Write a 64-bit little-endian integer.
  void WriteLittleEndian64(uint64 value);
  // Like WriteLittleEndian64()  but writing directly to the target array.
  static uint8* WriteLittleEndian64ToArray(uint64 value, uint8* target);

  // Write an unsigned integer with Varint encoding.  Writing a 32-bit value
  // is equivalent to casting it to uint64 and writing it as a 64-bit value,
  // but may be more efficient.
  void WriteVarint32(uint32 value);
  // Like WriteVarint32()  but writing directly to the target array.
  static uint8* WriteVarint32ToArray(uint32 value, uint8* target);
  // Write an unsigned integer with Varint encoding.
  void WriteVarint64(uint64 value);
  // Like WriteVarint64()  but writing directly to the target array.
  static uint8* WriteVarint64ToArray(uint64 value, uint8* target);

  // Equivalent to WriteVarint32() except when the value is negative,
  // in which case it must be sign-extended to a full 10 bytes.
  void WriteVarint32SignExtended(int32 value);
  // Like WriteVarint32SignExtended()  but writing directly to the target array.
  static uint8* WriteVarint32SignExtendedToArray(int32 value, uint8* target);

  // This is identical to WriteVarint32(), but optimized for writing tags.
  // In particular, if the input is a compile-time constant, this method
  // compiles down to a couple instructions.
  // Always inline because otherwise the aformentioned optimization can't work,
  // but GCC by default doesn't want to inline this.
  void WriteTag(uint32 value);
  // Like WriteTag()  but writing directly to the target array.
  PROTOBUF_ALWAYS_INLINE
  static uint8* WriteTagToArray(uint32 value, uint8* target);

  // Returns the number of bytes needed to encode the given value as a varint.
  static size_t VarintSize32(uint32 value);
  // Returns the number of bytes needed to encode the given value as a varint.
  static size_t VarintSize64(uint64 value);

  // If negative, 10 bytes.  Otheriwse, same as VarintSize32().
  static size_t VarintSize32SignExtended(int32 value);

  // Compile-time equivalent of VarintSize32().
  template <uint32 Value>
  struct StaticVarintSize32 {
    static const size_t value =
        (Value < (1 << 7))
            ? 1
            : (Value < (1 << 14))
                  ? 2
                  : (Value < (1 << 21)) ? 3 : (Value < (1 << 28)) ? 4 : 5;
  };

  // Returns the total number of bytes written since this object was created.
  inline int ByteCount() const;

  // Returns true if there was an underlying I/O error since this object was
  // created.
  bool HadError() const { return had_error_; }

  // Deterministic serialization, if requested, guarantees that for a given
  // binary, equal messages will always be serialized to the same bytes. This
  // implies:
  //   . repeated serialization of a message will return the same bytes
  //   . different processes of the same binary (which may be executing on
  //     different machines) will serialize equal messages to the same bytes.
  //
  // Note the deterministic serialization is NOT canonical across languages; it
  // is also unstable across different builds with schema changes due to unknown
  // fields. Users who need canonical serialization, e.g., persistent storage in
  // a canonical form, fingerprinting, etc., should define their own
  // canonicalization specification and implement the serializer using
  // reflection APIs rather than relying on this API.
  //
  // If deterministic serialization is requested, the serializer will
  // sort map entries by keys in lexicographical order or numerical order.
  // (This is an implementation detail and may subject to change.)
  //
  // There are two ways to determine whether serialization should be
  // deterministic for this CodedOutputStream.  If SetSerializationDeterministic
  // has not yet been called, then the default comes from the global default,
  // which is false, until SetDefaultSerializationDeterministic has been called.
  // Otherwise, SetSerializationDeterministic has been called, and the last
  // value passed to it is all that matters.
  void SetSerializationDeterministic(bool value) {
    is_serialization_deterministic_ = value;
  }
  // See above.  Also, note that users of this CodedOutputStream may need to
  // call IsSerializationDeterministic() to serialize in the intended way.  This
  // CodedOutputStream cannot enforce a desire for deterministic serialization
  // by itself.
  bool IsSerializationDeterministic() const {
    return is_serialization_deterministic_;
  }

  static bool IsDefaultSerializationDeterministic() {
    return default_serialization_deterministic_.load(
               std::memory_order_relaxed) != 0;
  }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CodedOutputStream);

  ZeroCopyOutputStream* output_;
  uint8* buffer_;
  int buffer_size_;
  int total_bytes_;        // Sum of sizes of all buffers seen so far.
  bool had_error_;         // Whether an error occurred during output.
  bool aliasing_enabled_;  // See EnableAliasing().
  bool is_serialization_deterministic_;
  static std::atomic<bool> default_serialization_deterministic_;

  // Advance the buffer by a given number of bytes.
  void Advance(int amount);

  // Called when the buffer runs out to request more data.  Implies an
  // Advance(buffer_size_).
  bool Refresh();

  // Like WriteRaw() but may avoid copying if the underlying
  // ZeroCopyOutputStream supports it.
  void WriteAliasedRaw(const void* buffer, int size);

  // If this write might cross the end of the buffer, we compose the bytes first
  // then use WriteRaw().
  void WriteVarint32SlowPath(uint32 value);
  void WriteVarint64SlowPath(uint64 value);

  // See above.  Other projects may use "friend" to allow them to call this.
  // After SetDefaultSerializationDeterministic() completes, all protocol
  // buffer serializations will be deterministic by default.  Thread safe.
  // However, the meaning of "after" is subtle here: to be safe, each thread
  // that wants deterministic serialization by default needs to call
  // SetDefaultSerializationDeterministic() or ensure on its own that another
  // thread has done so.
  friend void internal::MapTestForceDeterministic();
  static void SetDefaultSerializationDeterministic() {
    default_serialization_deterministic_.store(true, std::memory_order_relaxed);
  }
};

// inline methods ====================================================
// The vast majority of varints are only one byte.  These inline
// methods optimize for that case.

inline bool CodedInputStream::ReadVarint32(uint32* value) {
  uint32 v = 0;
  if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_)) {
    v = *buffer_;
    if (v < 0x80) {
      *value = v;
      Advance(1);
      return true;
    }
  }
  int64 result = ReadVarint32Fallback(v);
  *value = static_cast<uint32>(result);
  return result >= 0;
}

inline bool CodedInputStream::ReadVarint64(uint64* value) {
  if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_) && *buffer_ < 0x80) {
    *value = *buffer_;
    Advance(1);
    return true;
  }
  std::pair<uint64, bool> p = ReadVarint64Fallback();
  *value = p.first;
  return p.second;
}

inline bool CodedInputStream::ReadVarintSizeAsInt(int* value) {
  if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_)) {
    int v = *buffer_;
    if (v < 0x80) {
      *value = v;
      Advance(1);
      return true;
    }
  }
  *value = ReadVarintSizeAsIntFallback();
  return *value >= 0;
}

// static
inline const uint8* CodedInputStream::ReadLittleEndian32FromArray(
    const uint8* buffer, uint32* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  *value = (static_cast<uint32>(buffer[0])) |
           (static_cast<uint32>(buffer[1]) << 8) |
           (static_cast<uint32>(buffer[2]) << 16) |
           (static_cast<uint32>(buffer[3]) << 24);
  return buffer + sizeof(*value);
#endif
}
// static
inline const uint8* CodedInputStream::ReadLittleEndian64FromArray(
    const uint8* buffer, uint64* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  uint32 part0 = (static_cast<uint32>(buffer[0])) |
                 (static_cast<uint32>(buffer[1]) << 8) |
                 (static_cast<uint32>(buffer[2]) << 16) |
                 (static_cast<uint32>(buffer[3]) << 24);
  uint32 part1 = (static_cast<uint32>(buffer[4])) |
                 (static_cast<uint32>(buffer[5]) << 8) |
                 (static_cast<uint32>(buffer[6]) << 16) |
                 (static_cast<uint32>(buffer[7]) << 24);
  *value = static_cast<uint64>(part0) | (static_cast<uint64>(part1) << 32);
  return buffer + sizeof(*value);
#endif
}

inline bool CodedInputStream::ReadLittleEndian32(uint32* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  if (PROTOBUF_PREDICT_TRUE(BufferSize() >= static_cast<int>(sizeof(*value)))) {
    buffer_ = ReadLittleEndian32FromArray(buffer_, value);
    return true;
  } else {
    return ReadLittleEndian32Fallback(value);
  }
#else
  return ReadLittleEndian32Fallback(value);
#endif
}

inline bool CodedInputStream::ReadLittleEndian64(uint64* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  if (PROTOBUF_PREDICT_TRUE(BufferSize() >= static_cast<int>(sizeof(*value)))) {
    buffer_ = ReadLittleEndian64FromArray(buffer_, value);
    return true;
  } else {
    return ReadLittleEndian64Fallback(value);
  }
#else
  return ReadLittleEndian64Fallback(value);
#endif
}

inline uint32 CodedInputStream::ReadTagNoLastTag() {
  uint32 v = 0;
  if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_)) {
    v = *buffer_;
    if (v < 0x80) {
      Advance(1);
      return v;
    }
  }
  v = ReadTagFallback(v);
  return v;
}

inline std::pair<uint32, bool> CodedInputStream::ReadTagWithCutoffNoLastTag(
    uint32 cutoff) {
  // In performance-sensitive code we can expect cutoff to be a compile-time
  // constant, and things like "cutoff >= kMax1ByteVarint" to be evaluated at
  // compile time.
  uint32 first_byte_or_zero = 0;
  if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_)) {
    // Hot case: buffer_ non_empty, buffer_[0] in [1, 128).
    // TODO(gpike): Is it worth rearranging this? E.g., if the number of fields
    // is large enough then is it better to check for the two-byte case first?
    first_byte_or_zero = buffer_[0];
    if (static_cast<int8>(buffer_[0]) > 0) {
      const uint32 kMax1ByteVarint = 0x7f;
      uint32 tag = buffer_[0];
      Advance(1);
      return std::make_pair(tag, cutoff >= kMax1ByteVarint || tag <= cutoff);
    }
    // Other hot case: cutoff >= 0x80, buffer_ has at least two bytes available,
    // and tag is two bytes.  The latter is tested by bitwise-and-not of the
    // first byte and the second byte.
    if (cutoff >= 0x80 && PROTOBUF_PREDICT_TRUE(buffer_ + 1 < buffer_end_) &&
        PROTOBUF_PREDICT_TRUE((buffer_[0] & ~buffer_[1]) >= 0x80)) {
      const uint32 kMax2ByteVarint = (0x7f << 7) + 0x7f;
      uint32 tag = (1u << 7) * buffer_[1] + (buffer_[0] - 0x80);
      Advance(2);
      // It might make sense to test for tag == 0 now, but it is so rare that
      // that we don't bother.  A varint-encoded 0 should be one byte unless
      // the encoder lost its mind.  The second part of the return value of
      // this function is allowed to be either true or false if the tag is 0,
      // so we don't have to check for tag == 0.  We may need to check whether
      // it exceeds cutoff.
      bool at_or_below_cutoff = cutoff >= kMax2ByteVarint || tag <= cutoff;
      return std::make_pair(tag, at_or_below_cutoff);
    }
  }
  // Slow path
  const uint32 tag = ReadTagFallback(first_byte_or_zero);
  return std::make_pair(tag, static_cast<uint32>(tag - 1) < cutoff);
}

inline bool CodedInputStream::LastTagWas(uint32 expected) {
  return last_tag_ == expected;
}

inline bool CodedInputStream::ConsumedEntireMessage() {
  return legitimate_message_end_;
}

inline bool CodedInputStream::ExpectTag(uint32 expected) {
  if (expected < (1 << 7)) {
    if (PROTOBUF_PREDICT_TRUE(buffer_ < buffer_end_) &&
        buffer_[0] == expected) {
      Advance(1);
      return true;
    } else {
      return false;
    }
  } else if (expected < (1 << 14)) {
    if (PROTOBUF_PREDICT_TRUE(BufferSize() >= 2) &&
        buffer_[0] == static_cast<uint8>(expected | 0x80) &&
        buffer_[1] == static_cast<uint8>(expected >> 7)) {
      Advance(2);
      return true;
    } else {
      return false;
    }
  } else {
    // Don't bother optimizing for larger values.
    return false;
  }
}

inline const uint8* CodedInputStream::ExpectTagFromArray(const uint8* buffer,
                                                         uint32 expected) {
  if (expected < (1 << 7)) {
    if (buffer[0] == expected) {
      return buffer + 1;
    }
  } else if (expected < (1 << 14)) {
    if (buffer[0] == static_cast<uint8>(expected | 0x80) &&
        buffer[1] == static_cast<uint8>(expected >> 7)) {
      return buffer + 2;
    }
  }
  return NULL;
}

inline void CodedInputStream::GetDirectBufferPointerInline(const void** data,
                                                           int* size) {
  *data = buffer_;
  *size = static_cast<int>(buffer_end_ - buffer_);
}

inline bool CodedInputStream::ExpectAtEnd() {
  // If we are at a limit we know no more bytes can be read.  Otherwise, it's
  // hard to say without calling Refresh(), and we'd rather not do that.

  if (buffer_ == buffer_end_ && ((buffer_size_after_limit_ != 0) ||
                                 (total_bytes_read_ == current_limit_))) {
    last_tag_ = 0;                   // Pretend we called ReadTag()...
    legitimate_message_end_ = true;  // ... and it hit EOF.
    return true;
  } else {
    return false;
  }
}

inline int CodedInputStream::CurrentPosition() const {
  return total_bytes_read_ - (BufferSize() + buffer_size_after_limit_);
}

inline uint8* CodedOutputStream::GetDirectBufferForNBytesAndAdvance(int size) {
  if (buffer_size_ < size) {
    return NULL;
  } else {
    uint8* result = buffer_;
    Advance(size);
    return result;
  }
}

inline uint8* CodedOutputStream::WriteVarint32ToArray(uint32 value,
                                                      uint8* target) {
  while (value >= 0x80) {
    *target = static_cast<uint8>(value | 0x80);
    value >>= 7;
    ++target;
  }
  *target = static_cast<uint8>(value);
  return target + 1;
}

inline uint8* CodedOutputStream::WriteVarint64ToArray(uint64 value,
                                                      uint8* target) {
  while (value >= 0x80) {
    *target = static_cast<uint8>(value | 0x80);
    value >>= 7;
    ++target;
  }
  *target = static_cast<uint8>(value);
  return target + 1;
}

inline void CodedOutputStream::WriteVarint32SignExtended(int32 value) {
  WriteVarint64(static_cast<uint64>(value));
}

inline uint8* CodedOutputStream::WriteVarint32SignExtendedToArray(
    int32 value, uint8* target) {
  return WriteVarint64ToArray(static_cast<uint64>(value), target);
}

inline uint8* CodedOutputStream::WriteLittleEndian32ToArray(uint32 value,
                                                            uint8* target) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(target, &value, sizeof(value));
#else
  target[0] = static_cast<uint8>(value);
  target[1] = static_cast<uint8>(value >> 8);
  target[2] = static_cast<uint8>(value >> 16);
  target[3] = static_cast<uint8>(value >> 24);
#endif
  return target + sizeof(value);
}

inline uint8* CodedOutputStream::WriteLittleEndian64ToArray(uint64 value,
                                                            uint8* target) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(target, &value, sizeof(value));
#else
  uint32 part0 = static_cast<uint32>(value);
  uint32 part1 = static_cast<uint32>(value >> 32);

  target[0] = static_cast<uint8>(part0);
  target[1] = static_cast<uint8>(part0 >> 8);
  target[2] = static_cast<uint8>(part0 >> 16);
  target[3] = static_cast<uint8>(part0 >> 24);
  target[4] = static_cast<uint8>(part1);
  target[5] = static_cast<uint8>(part1 >> 8);
  target[6] = static_cast<uint8>(part1 >> 16);
  target[7] = static_cast<uint8>(part1 >> 24);
#endif
  return target + sizeof(value);
}

inline void CodedOutputStream::WriteVarint32(uint32 value) {
  if (buffer_size_ >= 5) {
    // Fast path:  We have enough bytes left in the buffer to guarantee that
    // this write won't cross the end, so we can skip the checks.
    uint8* target = buffer_;
    uint8* end = WriteVarint32ToArray(value, target);
    int size = static_cast<int>(end - target);
    Advance(size);
  } else {
    WriteVarint32SlowPath(value);
  }
}

inline void CodedOutputStream::WriteVarint64(uint64 value) {
  if (buffer_size_ >= 10) {
    // Fast path:  We have enough bytes left in the buffer to guarantee that
    // this write won't cross the end, so we can skip the checks.
    uint8* target = buffer_;
    uint8* end = WriteVarint64ToArray(value, target);
    int size = static_cast<int>(end - target);
    Advance(size);
  } else {
    WriteVarint64SlowPath(value);
  }
}

inline void CodedOutputStream::WriteTag(uint32 value) { WriteVarint32(value); }

inline uint8* CodedOutputStream::WriteTagToArray(uint32 value, uint8* target) {
  return WriteVarint32ToArray(value, target);
}

inline size_t CodedOutputStream::VarintSize32(uint32 value) {
  // This computes value == 0 ? 1 : floor(log2(value)) / 7 + 1
  // Use an explicit multiplication to implement the divide of
  // a number in the 1..31 range.
  // Explicit OR 0x1 to avoid calling Bits::Log2FloorNonZero(0), which is
  // undefined.
  uint32 log2value = Bits::Log2FloorNonZero(value | 0x1);
  return static_cast<size_t>((log2value * 9 + 73) / 64);
}

inline size_t CodedOutputStream::VarintSize64(uint64 value) {
  // This computes value == 0 ? 1 : floor(log2(value)) / 7 + 1
  // Use an explicit multiplication to implement the divide of
  // a number in the 1..63 range.
  // Explicit OR 0x1 to avoid calling Bits::Log2FloorNonZero(0), which is
  // undefined.
  uint32 log2value = Bits::Log2FloorNonZero64(value | 0x1);
  return static_cast<size_t>((log2value * 9 + 73) / 64);
}

inline size_t CodedOutputStream::VarintSize32SignExtended(int32 value) {
  if (value < 0) {
    return 10;  // TODO(kenton):  Make this a symbolic constant.
  } else {
    return VarintSize32(static_cast<uint32>(value));
  }
}

inline void CodedOutputStream::WriteString(const std::string& str) {
  WriteRaw(str.data(), static_cast<int>(str.size()));
}

inline void CodedOutputStream::WriteRawMaybeAliased(const void* data,
                                                    int size) {
  if (aliasing_enabled_) {
    WriteAliasedRaw(data, size);
  } else {
    WriteRaw(data, size);
  }
}

inline uint8* CodedOutputStream::WriteStringToArray(const std::string& str,
                                                    uint8* target) {
  return WriteRawToArray(str.data(), static_cast<int>(str.size()), target);
}

inline int CodedOutputStream::ByteCount() const {
  return total_bytes_ - buffer_size_;
}

inline void CodedInputStream::Advance(int amount) { buffer_ += amount; }

inline void CodedOutputStream::Advance(int amount) {
  buffer_ += amount;
  buffer_size_ -= amount;
}

inline void CodedInputStream::SetRecursionLimit(int limit) {
  recursion_budget_ += limit - recursion_limit_;
  recursion_limit_ = limit;
}

inline bool CodedInputStream::IncrementRecursionDepth() {
  --recursion_budget_;
  return recursion_budget_ >= 0;
}

inline void CodedInputStream::DecrementRecursionDepth() {
  if (recursion_budget_ < recursion_limit_) ++recursion_budget_;
}

inline void CodedInputStream::UnsafeDecrementRecursionDepth() {
  assert(recursion_budget_ < recursion_limit_);
  ++recursion_budget_;
}

inline void CodedInputStream::SetExtensionRegistry(const DescriptorPool* pool,
                                                   MessageFactory* factory) {
  extension_pool_ = pool;
  extension_factory_ = factory;
}

inline const DescriptorPool* CodedInputStream::GetExtensionPool() {
  return extension_pool_;
}

inline MessageFactory* CodedInputStream::GetExtensionFactory() {
  return extension_factory_;
}

inline int CodedInputStream::BufferSize() const {
  return static_cast<int>(buffer_end_ - buffer_);
}

inline CodedInputStream::CodedInputStream(ZeroCopyInputStream* input)
    : buffer_(NULL),
      buffer_end_(NULL),
      input_(input),
      total_bytes_read_(0),
      overflow_bytes_(0),
      last_tag_(0),
      legitimate_message_end_(false),
      aliasing_enabled_(false),
      current_limit_(kint32max),
      buffer_size_after_limit_(0),
      total_bytes_limit_(kDefaultTotalBytesLimit),
      recursion_budget_(default_recursion_limit_),
      recursion_limit_(default_recursion_limit_),
      extension_pool_(NULL),
      extension_factory_(NULL) {
  // Eagerly Refresh() so buffer space is immediately available.
  Refresh();
}

inline CodedInputStream::CodedInputStream(const uint8* buffer, int size)
    : buffer_(buffer),
      buffer_end_(buffer + size),
      input_(NULL),
      total_bytes_read_(size),
      overflow_bytes_(0),
      last_tag_(0),
      legitimate_message_end_(false),
      aliasing_enabled_(false),
      current_limit_(size),
      buffer_size_after_limit_(0),
      total_bytes_limit_(kDefaultTotalBytesLimit),
      recursion_budget_(default_recursion_limit_),
      recursion_limit_(default_recursion_limit_),
      extension_pool_(NULL),
      extension_factory_(NULL) {
  // Note that setting current_limit_ == size is important to prevent some
  // code paths from trying to access input_ and segfaulting.
}

inline bool CodedInputStream::IsFlat() const { return input_ == NULL; }

inline bool CodedInputStream::Skip(int count) {
  if (count < 0) return false;  // security: count is often user-supplied

  const int original_buffer_size = BufferSize();

  if (count <= original_buffer_size) {
    // Just skipping within the current buffer.  Easy.
    Advance(count);
    return true;
  }

  return SkipFallback(count, original_buffer_size);
}

}  // namespace io
}  // namespace protobuf
}  // namespace google

#if defined(_MSC_VER) && _MSC_VER >= 1300 && !defined(__INTEL_COMPILER)
#pragma runtime_checks("c", restore)
#endif  // _MSC_VER && !defined(__INTEL_COMPILER)

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_IO_CODED_STREAM_H__
