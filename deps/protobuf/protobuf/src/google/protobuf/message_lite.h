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

// Authors: wink@google.com (Wink Saville),
//          kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// Defines MessageLite, the abstract interface implemented by all (lite
// and non-lite) protocol message objects.

#ifndef GOOGLE_PROTOBUF_MESSAGE_LITE_H__
#define GOOGLE_PROTOBUF_MESSAGE_LITE_H__

#include <climits>
#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/port.h>
#include <google/protobuf/stubs/strutil.h>


#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {

template <typename T>
class RepeatedPtrField;

namespace io {

class CodedInputStream;
class CodedOutputStream;
class ZeroCopyInputStream;
class ZeroCopyOutputStream;

}  // namespace io
namespace internal {

// See parse_context.h for explanation
class ParseContext;

class RepeatedPtrFieldBase;
class WireFormatLite;
class WeakFieldMap;

// We compute sizes as size_t but cache them as int.  This function converts a
// computed size to a cached size.  Since we don't proceed with serialization
// if the total size was > INT_MAX, it is not important what this function
// returns for inputs > INT_MAX.  However this case should not error or
// GOOGLE_CHECK-fail, because the full size_t resolution is still returned from
// ByteSizeLong() and checked against INT_MAX; we can catch the overflow
// there.
inline int ToCachedSize(size_t size) { return static_cast<int>(size); }

// We mainly calculate sizes in terms of size_t, but some functions that
// compute sizes return "int".  These int sizes are expected to always be
// positive. This function is more efficient than casting an int to size_t
// directly on 64-bit platforms because it avoids making the compiler emit a
// sign extending instruction, which we don't want and don't want to pay for.
inline size_t FromIntSize(int size) {
  // Convert to unsigned before widening so sign extension is not necessary.
  return static_cast<unsigned int>(size);
}

// For cases where a legacy function returns an integer size.  We GOOGLE_DCHECK()
// that the conversion will fit within an integer; if this is false then we
// are losing information.
inline int ToIntSize(size_t size) {
  GOOGLE_DCHECK_LE(size, static_cast<size_t>(INT_MAX));
  return static_cast<int>(size);
}

// This type wraps a variable whose constructor and destructor are explicitly
// called. It is particularly useful for a global variable, without its
// constructor and destructor run on start and end of the program lifetime.
// This circumvents the initial construction order fiasco, while keeping
// the address of the empty string a compile time constant.
//
// Pay special attention to the initialization state of the object.
// 1. The object is "uninitialized" to begin with.
// 2. Call Construct() or DefaultConstruct() only if the object is
//    uninitialized. After the call, the object becomes "initialized".
// 3. Call get() and get_mutable() only if the object is initialized.
// 4. Call Destruct() only if the object is initialized.
//    After the call, the object becomes uninitialized.
template <typename T>
class ExplicitlyConstructed {
 public:
  void DefaultConstruct() { new (&union_) T(); }

  template <typename... Args>
  void Construct(Args&&... args) {
    new (&union_) T(std::forward<Args>(args)...);
  }

  void Destruct() { get_mutable()->~T(); }

  constexpr const T& get() const { return reinterpret_cast<const T&>(union_); }
  T* get_mutable() { return reinterpret_cast<T*>(&union_); }

 private:
  // Prefer c++14 aligned_storage, but for compatibility this will do.
  union AlignedUnion {
    char space[sizeof(T)];
    int64 align_to_int64;
    void* align_to_ptr;
  } union_;
};

// Default empty string object. Don't use this directly. Instead, call
// GetEmptyString() to get the reference.
PROTOBUF_EXPORT extern ExplicitlyConstructed<std::string>
    fixed_address_empty_string;


PROTOBUF_EXPORT inline const std::string& GetEmptyStringAlreadyInited() {
  return fixed_address_empty_string.get();
}

PROTOBUF_EXPORT size_t StringSpaceUsedExcludingSelfLong(const std::string& str);

}  // namespace internal

// Interface to light weight protocol messages.
//
// This interface is implemented by all protocol message objects.  Non-lite
// messages additionally implement the Message interface, which is a
// subclass of MessageLite.  Use MessageLite instead when you only need
// the subset of features which it supports -- namely, nothing that uses
// descriptors or reflection.  You can instruct the protocol compiler
// to generate classes which implement only MessageLite, not the full
// Message interface, by adding the following line to the .proto file:
//
//   option optimize_for = LITE_RUNTIME;
//
// This is particularly useful on resource-constrained systems where
// the full protocol buffers runtime library is too big.
//
// Note that on non-constrained systems (e.g. servers) when you need
// to link in lots of protocol definitions, a better way to reduce
// total code footprint is to use optimize_for = CODE_SIZE.  This
// will make the generated code smaller while still supporting all the
// same features (at the expense of speed).  optimize_for = LITE_RUNTIME
// is best when you only have a small number of message types linked
// into your binary, in which case the size of the protocol buffers
// runtime itself is the biggest problem.
class PROTOBUF_EXPORT MessageLite {
 public:
  inline MessageLite() {}
  virtual ~MessageLite() {}

  // Basic Operations ------------------------------------------------

  // Get the name of this message type, e.g. "foo.bar.BazProto".
  virtual std::string GetTypeName() const = 0;

  // Construct a new instance of the same type.  Ownership is passed to the
  // caller.
  virtual MessageLite* New() const = 0;

  // Construct a new instance on the arena. Ownership is passed to the caller
  // if arena is a NULL. Default implementation for backwards compatibility.
  virtual MessageLite* New(Arena* arena) const;

  // Get the arena, if any, associated with this message. Virtual method
  // required for generic operations but most arena-related operations should
  // use the GetArenaNoVirtual() generated-code method. Default implementation
  // to reduce code size by avoiding the need for per-type implementations
  // when types do not implement arena support.
  virtual Arena* GetArena() const { return NULL; }

  // Get a pointer that may be equal to this message's arena, or may not be.
  // If the value returned by this method is equal to some arena pointer, then
  // this message is on that arena; however, if this message is on some arena,
  // this method may or may not return that arena's pointer. As a tradeoff,
  // this method may be more efficient than GetArena(). The intent is to allow
  // underlying representations that use e.g. tagged pointers to sometimes
  // store the arena pointer directly, and sometimes in a more indirect way,
  // and allow a fastpath comparison against the arena pointer when it's easy
  // to obtain.
  virtual void* GetMaybeArenaPointer() const { return GetArena(); }

  // Clear all fields of the message and set them to their default values.
  // Clear() avoids freeing memory, assuming that any memory allocated
  // to hold parts of the message will be needed again to hold the next
  // message.  If you actually want to free the memory used by a Message,
  // you must delete it.
  virtual void Clear() = 0;

  // Quickly check if all required fields have values set.
  virtual bool IsInitialized() const = 0;

  // This is not implemented for Lite messages -- it just returns "(cannot
  // determine missing fields for lite message)".  However, it is implemented
  // for full messages.  See message.h.
  virtual std::string InitializationErrorString() const;

  // If |other| is the exact same class as this, calls MergeFrom(). Otherwise,
  // results are undefined (probably crash).
  virtual void CheckTypeAndMergeFrom(const MessageLite& other) = 0;

  // These methods return a human-readable summary of the message. Note that
  // since the MessageLite interface does not support reflection, there is very
  // little information that these methods can provide. They are shadowed by
  // methods of the same name on the Message interface which provide much more
  // information. The methods here are intended primarily to facilitate code
  // reuse for logic that needs to interoperate with both full and lite protos.
  //
  // The format of the returned string is subject to change, so please do not
  // assume it will remain stable over time.
  std::string DebugString() const;
  std::string ShortDebugString() const { return DebugString(); }
  // MessageLite::DebugString is already Utf8 Safe. This is to add compatibility
  // with Message.
  std::string Utf8DebugString() const { return DebugString(); }

  // Parsing ---------------------------------------------------------
  // Methods for parsing in protocol buffer format.  Most of these are
  // just simple wrappers around MergeFromCodedStream().  Clear() will be
  // called before merging the input.

  // Fill the message with a protocol buffer parsed from the given input
  // stream. Returns false on a read error or if the input is in the wrong
  // format.  A successful return does not indicate the entire input is
  // consumed, ensure you call ConsumedEntireMessage() to check that if
  // applicable.
  bool ParseFromCodedStream(io::CodedInputStream* input);
  // Like ParseFromCodedStream(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromCodedStream(io::CodedInputStream* input);
  // Read a protocol buffer from the given zero-copy input stream.  If
  // successful, the entire input will be consumed.
  bool ParseFromZeroCopyStream(io::ZeroCopyInputStream* input);
  // Like ParseFromZeroCopyStream(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromZeroCopyStream(io::ZeroCopyInputStream* input);
  // Parse a protocol buffer from a file descriptor.  If successful, the entire
  // input will be consumed.
  bool ParseFromFileDescriptor(int file_descriptor);
  // Like ParseFromFileDescriptor(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromFileDescriptor(int file_descriptor);
  // Parse a protocol buffer from a C++ istream.  If successful, the entire
  // input will be consumed.
  bool ParseFromIstream(std::istream* input);
  // Like ParseFromIstream(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromIstream(std::istream* input);
  // Read a protocol buffer from the given zero-copy input stream, expecting
  // the message to be exactly "size" bytes long.  If successful, exactly
  // this many bytes will have been consumed from the input.
  bool MergePartialFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input,
                                             int size);
  // Like ParseFromBoundedZeroCopyStream(), but accepts messages that are
  // missing required fields.
  bool MergeFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input, int size);
  bool ParseFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input, int size);
  // Like ParseFromBoundedZeroCopyStream(), but accepts messages that are
  // missing required fields.
  bool ParsePartialFromBoundedZeroCopyStream(io::ZeroCopyInputStream* input,
                                             int size);
  // Parses a protocol buffer contained in a string. Returns true on success.
  // This function takes a string in the (non-human-readable) binary wire
  // format, matching the encoding output by MessageLite::SerializeToString().
  // If you'd like to convert a human-readable string into a protocol buffer
  // object, see google::protobuf::TextFormat::ParseFromString().
  bool ParseFromString(const std::string& data);
  // Like ParseFromString(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromString(const std::string& data);
  // Parse a protocol buffer contained in an array of bytes.
  bool ParseFromArray(const void* data, int size);
  // Like ParseFromArray(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromArray(const void* data, int size);


  // Reads a protocol buffer from the stream and merges it into this
  // Message.  Singular fields read from the what is
  // already in the Message and repeated fields are appended to those
  // already present.
  //
  // It is the responsibility of the caller to call input->LastTagWas()
  // (for groups) or input->ConsumedEntireMessage() (for non-groups) after
  // this returns to verify that the message's end was delimited correctly.
  //
  // ParsefromCodedStream() is implemented as Clear() followed by
  // MergeFromCodedStream().
  bool MergeFromCodedStream(io::CodedInputStream* input);

  // Like MergeFromCodedStream(), but succeeds even if required fields are
  // missing in the input.
  //
  // MergeFromCodedStream() is just implemented as MergePartialFromCodedStream()
  // followed by IsInitialized().
#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
  bool MergePartialFromCodedStream(io::CodedInputStream* input);
#else
  virtual bool MergePartialFromCodedStream(io::CodedInputStream* input) = 0;
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

  // Merge a protocol buffer contained in a string.
  bool MergeFromString(const std::string& data);


  // Serialization ---------------------------------------------------
  // Methods for serializing in protocol buffer format.  Most of these
  // are just simple wrappers around ByteSize() and SerializeWithCachedSizes().

  // Write a protocol buffer of this message to the given output.  Returns
  // false on a write error.  If the message is missing required fields,
  // this may GOOGLE_CHECK-fail.
  bool SerializeToCodedStream(io::CodedOutputStream* output) const;
  // Like SerializeToCodedStream(), but allows missing required fields.
  bool SerializePartialToCodedStream(io::CodedOutputStream* output) const;
  // Write the message to the given zero-copy output stream.  All required
  // fields must be set.
  bool SerializeToZeroCopyStream(io::ZeroCopyOutputStream* output) const;
  // Like SerializeToZeroCopyStream(), but allows missing required fields.
  bool SerializePartialToZeroCopyStream(io::ZeroCopyOutputStream* output) const;
  // Serialize the message and store it in the given string.  All required
  // fields must be set.
  bool SerializeToString(std::string* output) const;
  // Like SerializeToString(), but allows missing required fields.
  bool SerializePartialToString(std::string* output) const;
  // Serialize the message and store it in the given byte array.  All required
  // fields must be set.
  bool SerializeToArray(void* data, int size) const;
  // Like SerializeToArray(), but allows missing required fields.
  bool SerializePartialToArray(void* data, int size) const;

  // Make a string encoding the message. Is equivalent to calling
  // SerializeToString() on a string and using that.  Returns the empty
  // string if SerializeToString() would have returned an error.
  // Note: If you intend to generate many such strings, you may
  // reduce heap fragmentation by instead re-using the same string
  // object with calls to SerializeToString().
  std::string SerializeAsString() const;
  // Like SerializeAsString(), but allows missing required fields.
  std::string SerializePartialAsString() const;

  // Serialize the message and write it to the given file descriptor.  All
  // required fields must be set.
  bool SerializeToFileDescriptor(int file_descriptor) const;
  // Like SerializeToFileDescriptor(), but allows missing required fields.
  bool SerializePartialToFileDescriptor(int file_descriptor) const;
  // Serialize the message and write it to the given C++ ostream.  All
  // required fields must be set.
  bool SerializeToOstream(std::ostream* output) const;
  // Like SerializeToOstream(), but allows missing required fields.
  bool SerializePartialToOstream(std::ostream* output) const;

  // Like SerializeToString(), but appends to the data to the string's
  // existing contents.  All required fields must be set.
  bool AppendToString(std::string* output) const;
  // Like AppendToString(), but allows missing required fields.
  bool AppendPartialToString(std::string* output) const;


  // Computes the serialized size of the message.  This recursively calls
  // ByteSizeLong() on all embedded messages.
  //
  // ByteSizeLong() is generally linear in the number of fields defined for the
  // proto.
  virtual size_t ByteSizeLong() const = 0;

  // Legacy ByteSize() API.
  PROTOBUF_DEPRECATED_MSG("Please use ByteSizeLong() instead")
  int ByteSize() const { return internal::ToIntSize(ByteSizeLong()); }

  // Serializes the message without recomputing the size.  The message must not
  // have changed since the last call to ByteSize(), and the value returned by
  // ByteSize must be non-negative.  Otherwise the results are undefined.
  virtual void SerializeWithCachedSizes(io::CodedOutputStream* output) const;

  // Functions below here are not part of the public interface.  It isn't
  // enforced, but they should be treated as private, and will be private
  // at some future time.  Unfortunately the implementation of the "friend"
  // keyword in GCC is broken at the moment, but we expect it will be fixed.

  // Like SerializeWithCachedSizes, but writes directly to *target, returning
  // a pointer to the byte immediately after the last byte written.  "target"
  // must point at a byte array of at least ByteSize() bytes.  Whether to use
  // deterministic serialization, e.g., maps in sorted order, is determined by
  // CodedOutputStream::IsDefaultSerializationDeterministic().
  virtual uint8* SerializeWithCachedSizesToArray(uint8* target) const;

  // Returns the result of the last call to ByteSize().  An embedded message's
  // size is needed both to serialize it (because embedded messages are
  // length-delimited) and to compute the outer message's size.  Caching
  // the size avoids computing it multiple times.
  //
  // ByteSize() does not automatically use the cached size when available
  // because this would require invalidating it every time the message was
  // modified, which would be too hard and expensive.  (E.g. if a deeply-nested
  // sub-message is changed, all of its parents' cached sizes would need to be
  // invalidated, which is too much work for an otherwise inlined setter
  // method.)
  virtual int GetCachedSize() const = 0;

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
  virtual const char* _InternalParse(const char* ptr,
                                     internal::ParseContext* ctx) {
    return nullptr;
  }
#endif  // GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER

 protected:
  // CastToBase allows generated code to cast a RepeatedPtrField<T> to
  // RepeatedPtrFieldBase. We try to restrict access to RepeatedPtrFieldBase
  // because it is an implementation detail that user code should not access
  // directly.
  template <typename T>
  static internal::RepeatedPtrFieldBase* CastToBase(
      RepeatedPtrField<T>* repeated) {
    return repeated;
  }
  template <typename T>
  static const internal::RepeatedPtrFieldBase& CastToBase(
      const RepeatedPtrField<T>& repeated) {
    return repeated;
  }

  template <typename T>
  static T* CreateMaybeMessage(Arena* arena) {
    return Arena::CreateMaybeMessage<T>(arena);
  }

 public:
  enum ParseFlags {
    kMerge = 0,
    kParse = 1,
    kMergePartial = 2,
    kParsePartial = 3,
    kMergeWithAliasing = 4,
    kParseWithAliasing = 5,
    kMergePartialWithAliasing = 6,
    kParsePartialWithAliasing = 7
  };

  template <ParseFlags flags, typename T>
  bool ParseFrom(const T& input);

 private:
  // TODO(gerbens) make this a pure abstract function
  virtual const void* InternalGetTable() const { return NULL; }

  // Fast path when conditions match (ie. non-deterministic)
 public:
  virtual uint8* InternalSerializeWithCachedSizesToArray(uint8* target) const;

 private:
  friend class internal::WireFormatLite;
  friend class Message;
  friend class internal::WeakFieldMap;

  bool IsInitializedWithErrors() const {
    if (IsInitialized()) return true;
    LogInitializationErrorMessage();
    return false;
  }

  void LogInitializationErrorMessage() const;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MessageLite);
};

namespace internal {

template <bool alias>
bool MergePartialFromImpl(StringPiece input, MessageLite* msg);
extern template bool MergePartialFromImpl<false>(StringPiece input,
                                                 MessageLite* msg);
extern template bool MergePartialFromImpl<true>(StringPiece input,
                                                MessageLite* msg);

template <bool alias>
bool MergePartialFromImpl(io::ZeroCopyInputStream* input, MessageLite* msg);
extern template bool MergePartialFromImpl<false>(io::ZeroCopyInputStream* input,
                                                 MessageLite* msg);
extern template bool MergePartialFromImpl<true>(io::ZeroCopyInputStream* input,
                                                MessageLite* msg);

struct BoundedZCIS {
  io::ZeroCopyInputStream* zcis;
  int limit;
};

template <bool alias>
bool MergePartialFromImpl(BoundedZCIS input, MessageLite* msg);
extern template bool MergePartialFromImpl<false>(BoundedZCIS input,
                                                 MessageLite* msg);
extern template bool MergePartialFromImpl<true>(BoundedZCIS input,
                                                MessageLite* msg);

template <typename T>
struct SourceWrapper;

template <bool alias, typename T>
bool MergePartialFromImpl(const SourceWrapper<T>& input, MessageLite* msg) {
  return input.template MergePartialInto<alias>(msg);
}

}  // namespace internal

template <MessageLite::ParseFlags flags, typename T>
bool MessageLite::ParseFrom(const T& input) {
  if (flags & kParse) Clear();
  constexpr bool alias = flags & kMergeWithAliasing;
  bool res = internal::MergePartialFromImpl<alias>(input, this);
  return res && ((flags & kMergePartial) || IsInitializedWithErrors());
}

}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_MESSAGE_LITE_H__
