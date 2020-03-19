// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_H_
#define V8_OBJECTS_STRING_H_

#include <memory>

#include "src/base/bits.h"
#include "src/base/export-template.h"
#include "src/objects/instance-type.h"
#include "src/objects/name.h"
#include "src/objects/smi.h"
#include "src/strings/unicode-decoder.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum InstanceType : uint16_t;

enum AllowNullsFlag { ALLOW_NULLS, DISALLOW_NULLS };
enum RobustnessFlag { ROBUST_STRING_TRAVERSAL, FAST_STRING_TRAVERSAL };

// The characteristics of a string are stored in its map.  Retrieving these
// few bits of information is moderately expensive, involving two memory
// loads where the second is dependent on the first.  To improve efficiency
// the shape of the string is given its own class so that it can be retrieved
// once and used for several string operations.  A StringShape is small enough
// to be passed by value and is immutable, but be aware that flattening a
// string can potentially alter its shape.  Also be aware that a GC caused by
// something else can alter the shape of a string due to ConsString
// shortcutting.  Keeping these restrictions in mind has proven to be error-
// prone and so we no longer put StringShapes in variables unless there is a
// concrete performance benefit at that particular point in the code.
class StringShape {
 public:
  inline explicit StringShape(const String s);
  inline explicit StringShape(Map s);
  inline explicit StringShape(InstanceType t);
  inline bool IsSequential();
  inline bool IsExternal();
  inline bool IsCons();
  inline bool IsSliced();
  inline bool IsThin();
  inline bool IsIndirect();
  inline bool IsExternalOneByte();
  inline bool IsExternalTwoByte();
  inline bool IsSequentialOneByte();
  inline bool IsSequentialTwoByte();
  inline bool IsInternalized();
  inline StringRepresentationTag representation_tag();
  inline uint32_t encoding_tag();
  inline uint32_t full_representation_tag();
#ifdef DEBUG
  inline uint32_t type() { return type_; }
  inline void invalidate() { valid_ = false; }
  inline bool valid() { return valid_; }
#else
  inline void invalidate() {}
#endif

  // Run different behavior for each concrete string class type, as defined by
  // the dispatcher.
  template <typename TDispatcher, typename TResult, typename... TArgs>
  inline TResult DispatchToSpecificTypeWithoutCast(TArgs&&... args);
  template <typename TDispatcher, typename TResult, typename... TArgs>
  inline TResult DispatchToSpecificType(String str, TArgs&&... args);

 private:
  uint32_t type_;
#ifdef DEBUG
  inline void set_valid() { valid_ = true; }
  bool valid_;
#else
  inline void set_valid() {}
#endif
};

// The String abstract class captures JavaScript string values:
//
// Ecma-262:
//  4.3.16 String Value
//    A string value is a member of the type String and is a finite
//    ordered sequence of zero or more 16-bit unsigned integer values.
//
// All string values have a length field.
class String : public TorqueGeneratedString<String, Name> {
 public:
  enum Encoding { ONE_BYTE_ENCODING, TWO_BYTE_ENCODING };

  // Representation of the flat content of a String.
  // A non-flat string doesn't have flat content.
  // A flat string has content that's encoded as a sequence of either
  // one-byte chars or two-byte UC16.
  // Returned by String::GetFlatContent().
  class FlatContent {
   public:
    // Returns true if the string is flat and this structure contains content.
    bool IsFlat() const { return state_ != NON_FLAT; }
    // Returns true if the structure contains one-byte content.
    bool IsOneByte() const { return state_ == ONE_BYTE; }
    // Returns true if the structure contains two-byte content.
    bool IsTwoByte() const { return state_ == TWO_BYTE; }

    // Return the one byte content of the string. Only use if IsOneByte()
    // returns true.
    Vector<const uint8_t> ToOneByteVector() const {
      DCHECK_EQ(ONE_BYTE, state_);
      return Vector<const uint8_t>(onebyte_start, length_);
    }
    // Return the two-byte content of the string. Only use if IsTwoByte()
    // returns true.
    Vector<const uc16> ToUC16Vector() const {
      DCHECK_EQ(TWO_BYTE, state_);
      return Vector<const uc16>(twobyte_start, length_);
    }

    uc16 Get(int i) const {
      DCHECK(i < length_);
      DCHECK(state_ != NON_FLAT);
      if (state_ == ONE_BYTE) return onebyte_start[i];
      return twobyte_start[i];
    }

    bool UsesSameString(const FlatContent& other) const {
      return onebyte_start == other.onebyte_start;
    }

   private:
    enum State { NON_FLAT, ONE_BYTE, TWO_BYTE };

    // Constructors only used by String::GetFlatContent().
    explicit FlatContent(const uint8_t* start, int length)
        : onebyte_start(start), length_(length), state_(ONE_BYTE) {}
    explicit FlatContent(const uc16* start, int length)
        : twobyte_start(start), length_(length), state_(TWO_BYTE) {}
    FlatContent() : onebyte_start(nullptr), length_(0), state_(NON_FLAT) {}

    union {
      const uint8_t* onebyte_start;
      const uc16* twobyte_start;
    };
    int length_;
    State state_;

    friend class String;
    friend class IterableSubString;
  };

  void MakeThin(Isolate* isolate, String canonical);

  template <typename Char>
  V8_INLINE Vector<const Char> GetCharVector(
      const DisallowHeapAllocation& no_gc);

  // Get chars from sequential or external strings.
  template <typename Char>
  inline const Char* GetChars(const DisallowHeapAllocation& no_gc);

  // Get and set the length of the string using acquire loads and release
  // stores.
  DECL_SYNCHRONIZED_INT_ACCESSORS(length)

  // Returns whether this string has only one-byte chars, i.e. all of them can
  // be one-byte encoded.  This might be the case even if the string is
  // two-byte.  Such strings may appear when the embedder prefers
  // two-byte external representations even for one-byte data.
  DECL_GETTER(IsOneByteRepresentation, bool)
  DECL_GETTER(IsTwoByteRepresentation, bool)

  // Cons and slices have an encoding flag that may not represent the actual
  // encoding of the underlying string.  This is taken into account here.
  // This function is static because that helps it get inlined.
  // Requires: string.IsFlat()
  static inline bool IsOneByteRepresentationUnderneath(String string);

  // Get and set individual two byte chars in the string.
  inline void Set(int index, uint16_t value);
  // Get individual two byte char in the string.  Repeated calls
  // to this method are not efficient unless the string is flat.
  V8_INLINE uint16_t Get(int index);

  // ES6 section 7.1.3.1 ToNumber Applied to the String Type
  static Handle<Object> ToNumber(Isolate* isolate, Handle<String> subject);

  // Flattens the string.  Checks first inline to see if it is
  // necessary.  Does nothing if the string is not a cons string.
  // Flattening allocates a sequential string with the same data as
  // the given string and mutates the cons string to a degenerate
  // form, where the first component is the new sequential string and
  // the second component is the empty string.  If allocation fails,
  // this function returns a failure.  If flattening succeeds, this
  // function returns the sequential string that is now the first
  // component of the cons string.
  //
  // Degenerate cons strings are handled specially by the garbage
  // collector (see IsShortcutCandidate).

  static inline Handle<String> Flatten(
      Isolate* isolate, Handle<String> string,
      AllocationType allocation = AllocationType::kYoung);
  static inline Handle<String> Flatten(
      OffThreadIsolate* isolate, Handle<String> string,
      AllocationType allocation = AllocationType::kYoung);

  // Tries to return the content of a flat string as a structure holding either
  // a flat vector of char or of uc16.
  // If the string isn't flat, and therefore doesn't have flat content, the
  // returned structure will report so, and can't provide a vector of either
  // kind.
  V8_EXPORT_PRIVATE FlatContent
  GetFlatContent(const DisallowHeapAllocation& no_gc);

  // Returns the parent of a sliced string or first part of a flat cons string.
  // Requires: StringShape(this).IsIndirect() && this->IsFlat()
  inline String GetUnderlying();

  // String relational comparison, implemented according to ES6 section 7.2.11
  // Abstract Relational Comparison (step 5): The comparison of Strings uses a
  // simple lexicographic ordering on sequences of code unit values. There is no
  // attempt to use the more complex, semantically oriented definitions of
  // character or string equality and collating order defined in the Unicode
  // specification. Therefore String values that are canonically equal according
  // to the Unicode standard could test as unequal. In effect this algorithm
  // assumes that both Strings are already in normalized form. Also, note that
  // for strings containing supplementary characters, lexicographic ordering on
  // sequences of UTF-16 code unit values differs from that on sequences of code
  // point values.
  V8_WARN_UNUSED_RESULT static ComparisonResult Compare(Isolate* isolate,
                                                        Handle<String> x,
                                                        Handle<String> y);

  // Perform ES6 21.1.3.8, including checking arguments.
  static Object IndexOf(Isolate* isolate, Handle<Object> receiver,
                        Handle<Object> search, Handle<Object> position);
  // Perform string match of pattern on subject, starting at start index.
  // Caller must ensure that 0 <= start_index <= sub->length(), as this does not
  // check any arguments.
  static int IndexOf(Isolate* isolate, Handle<String> receiver,
                     Handle<String> search, int start_index);

  static Object LastIndexOf(Isolate* isolate, Handle<Object> receiver,
                            Handle<Object> search, Handle<Object> position);

  // Encapsulates logic related to a match and its capture groups as required
  // by GetSubstitution.
  class Match {
   public:
    virtual Handle<String> GetMatch() = 0;
    virtual Handle<String> GetPrefix() = 0;
    virtual Handle<String> GetSuffix() = 0;

    // A named capture can be invalid (if it is not specified in the pattern),
    // unmatched (specified but not matched in the current string), and matched.
    enum CaptureState { INVALID, UNMATCHED, MATCHED };

    virtual int CaptureCount() = 0;
    virtual bool HasNamedCaptures() = 0;
    virtual MaybeHandle<String> GetCapture(int i, bool* capture_exists) = 0;
    virtual MaybeHandle<String> GetNamedCapture(Handle<String> name,
                                                CaptureState* state) = 0;

    virtual ~Match() = default;
  };

  // ES#sec-getsubstitution
  // GetSubstitution(matched, str, position, captures, replacement)
  // Expand the $-expressions in the string and return a new string with
  // the result.
  // A {start_index} can be passed to specify where to start scanning the
  // replacement string.
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> GetSubstitution(
      Isolate* isolate, Match* match, Handle<String> replacement,
      int start_index = 0);

  // String equality operations.
  inline bool Equals(String other);
  inline static bool Equals(Isolate* isolate, Handle<String> one,
                            Handle<String> two);

  // Dispatches to Is{One,Two}ByteEqualTo.
  template <typename Char>
  bool IsEqualTo(Vector<const Char> str);

  V8_EXPORT_PRIVATE bool HasOneBytePrefix(Vector<const char> str);
  V8_EXPORT_PRIVATE bool IsOneByteEqualTo(Vector<const uint8_t> str);
  V8_EXPORT_PRIVATE bool IsOneByteEqualTo(Vector<const char> str) {
    return IsOneByteEqualTo(Vector<const uint8_t>::cast(str));
  }
  bool IsTwoByteEqualTo(Vector<const uc16> str);

  // Return a UTF8 representation of the string.  The string is null
  // terminated but may optionally contain nulls.  Length is returned
  // in length_output if length_output is not a null pointer  The string
  // should be nearly flat, otherwise the performance of this method may
  // be very slow (quadratic in the length).  Setting robustness_flag to
  // ROBUST_STRING_TRAVERSAL invokes behaviour that is robust  This means it
  // handles unexpected data without causing assert failures and it does not
  // do any heap allocations.  This is useful when printing stack traces.
  std::unique_ptr<char[]> ToCString(AllowNullsFlag allow_nulls,
                                    RobustnessFlag robustness_flag, int offset,
                                    int length, int* length_output = nullptr);
  V8_EXPORT_PRIVATE std::unique_ptr<char[]> ToCString(
      AllowNullsFlag allow_nulls = DISALLOW_NULLS,
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL,
      int* length_output = nullptr);

  // Externalization.
  V8_EXPORT_PRIVATE bool MakeExternal(
      v8::String::ExternalStringResource* resource);
  V8_EXPORT_PRIVATE bool MakeExternal(
      v8::String::ExternalOneByteStringResource* resource);
  bool SupportsExternalization();

  // Conversion.
  // "array index": an index allowed by the ES spec for JSArrays.
  inline bool AsArrayIndex(uint32_t* index);

  // This is used for calculating array indices but differs from an
  // Array Index in the regard that this does not support the full
  // array index range. This only supports positive numbers less than
  // or equal to INT_MAX.
  //
  // String::AsArrayIndex might be a better fit if you're looking to
  // calculate the array index.
  //
  // if val < 0 or val > INT_MAX, returns -1
  // if 0 <= val <= INT_MAX, returns val
  static int32_t ToArrayIndex(Address addr);

  uint32_t inline ToValidIndex(Object number);
  // "integer index": the string is the decimal representation of an
  // integer in the range of a size_t. Useful for TypedArray accesses.
  inline bool AsIntegerIndex(size_t* index);

  // Trimming.
  enum TrimMode { kTrim, kTrimStart, kTrimEnd };
  static Handle<String> Trim(Isolate* isolate, Handle<String> string,
                             TrimMode mode);

  V8_EXPORT_PRIVATE void PrintOn(FILE* out);

  // For use during stack traces.  Performs rudimentary sanity check.
  bool LooksValid();

  // Dispatched behavior.
  void StringShortPrint(StringStream* accumulator, bool show_details = true);
  void PrintUC16(std::ostream& os, int start = 0, int end = -1);  // NOLINT
#if defined(DEBUG) || defined(OBJECT_PRINT)
  char* ToAsciiArray();
#endif
  DECL_PRINTER(String)
  DECL_VERIFIER(String)

  inline bool IsFlat();

  // Max char codes.
  static const int32_t kMaxOneByteCharCode = unibrow::Latin1::kMaxChar;
  static const uint32_t kMaxOneByteCharCodeU = unibrow::Latin1::kMaxChar;
  static const int kMaxUtf16CodeUnit = 0xffff;
  static const uint32_t kMaxUtf16CodeUnitU = kMaxUtf16CodeUnit;
  static const uc32 kMaxCodePoint = 0x10ffff;

  // Maximal string length.
  // The max length is different on 32 and 64 bit platforms. Max length for
  // 32-bit platforms is ~268.4M chars. On 64-bit platforms, max length is
  // ~536.8M chars.
  // See include/v8.h for the definition.
  static const int kMaxLength = v8::String::kMaxLength;
  // There are several defining limits imposed by our current implementation:
  // - any string's length must fit into a Smi.
  static_assert(kMaxLength <= kSmiMaxValue,
                "String length must fit into a Smi");
  // - adding two string lengths must still fit into a 32-bit int without
  //   overflow
  static_assert(kMaxLength * 2 <= kMaxInt,
                "String::kMaxLength * 2 must fit into an int32");
  // - any heap object's size in bytes must be able to fit into a Smi, because
  //   its space on the heap might be filled with a Filler; for strings this
  //   means SeqTwoByteString::kMaxSize must be able to fit into a Smi.
  static_assert(kMaxLength * 2 + kHeaderSize <= kSmiMaxValue,
                "String object size in bytes must fit into a Smi");
  // - any heap object's size in bytes must be able to fit into an int, because
  //   that's what our object handling code uses almost everywhere.
  static_assert(kMaxLength * 2 + kHeaderSize <= kMaxInt,
                "String object size in bytes must fit into an int");

  // Max length for computing hash. For strings longer than this limit the
  // string length is used as the hash value.
  static const int kMaxHashCalcLength = 16383;

  // Limit for truncation in short printing.
  static const int kMaxShortPrintLength = 1024;

  // Helper function for flattening strings.
  template <typename sinkchar>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  static void WriteToFlat(String source, sinkchar* sink, int from, int to);

  static inline bool IsAscii(const char* chars, int length) {
    return IsAscii(reinterpret_cast<const uint8_t*>(chars), length);
  }

  static inline bool IsAscii(const uint8_t* chars, int length) {
    return NonAsciiStart(chars, length) >= length;
  }

  static inline int NonOneByteStart(const uc16* chars, int length) {
    DCHECK(IsAligned(reinterpret_cast<Address>(chars), sizeof(uc16)));
    const uint16_t* start = chars;
    const uint16_t* limit = chars + length;

    if (static_cast<size_t>(length) >= kUIntptrSize) {
      // Check unaligned chars.
      while (!IsAligned(reinterpret_cast<Address>(chars), kUIntptrSize)) {
        if (*chars > unibrow::Latin1::kMaxChar) {
          return static_cast<int>(chars - start);
        }
        ++chars;
      }

      // Check aligned words.
      STATIC_ASSERT(unibrow::Latin1::kMaxChar == 0xFF);
#ifdef V8_TARGET_LITTLE_ENDIAN
      const uintptr_t non_one_byte_mask = kUintptrAllBitsSet / 0xFFFF * 0xFF00;
#else
      const uintptr_t non_one_byte_mask = kUintptrAllBitsSet / 0xFFFF * 0x00FF;
#endif
      while (chars + sizeof(uintptr_t) <= limit) {
        if (*reinterpret_cast<const uintptr_t*>(chars) & non_one_byte_mask) {
          break;
        }
        chars += (sizeof(uintptr_t) / sizeof(uc16));
      }
    }

    // Check remaining unaligned chars, or find non-one-byte char in word.
    while (chars < limit) {
      if (*chars > unibrow::Latin1::kMaxChar) {
        return static_cast<int>(chars - start);
      }
      ++chars;
    }

    return static_cast<int>(chars - start);
  }

  static inline bool IsOneByte(const uc16* chars, int length) {
    return NonOneByteStart(chars, length) >= length;
  }

  template <class Visitor>
  static inline ConsString VisitFlat(Visitor* visitor, String string,
                                     int offset = 0);

  template <typename LocalIsolate>
  static Handle<FixedArray> CalculateLineEnds(LocalIsolate* isolate,
                                              Handle<String> string,
                                              bool include_ending_line);

 private:
  friend class Name;
  friend class StringTableInsertionKey;
  friend class InternalizedStringKey;

  V8_EXPORT_PRIVATE static Handle<String> SlowFlatten(
      Isolate* isolate, Handle<ConsString> cons, AllocationType allocation);

  // Slow case of String::Equals.  This implementation works on any strings
  // but it is most efficient on strings that are almost flat.
  V8_EXPORT_PRIVATE bool SlowEquals(String other);

  V8_EXPORT_PRIVATE static bool SlowEquals(Isolate* isolate, Handle<String> one,
                                           Handle<String> two);

  // Slow case of AsArrayIndex.
  V8_EXPORT_PRIVATE bool SlowAsArrayIndex(uint32_t* index);
  V8_EXPORT_PRIVATE bool SlowAsIntegerIndex(size_t* index);

  // Compute and set the hash code.
  V8_EXPORT_PRIVATE uint32_t ComputeAndSetHash();

  TQ_OBJECT_CONSTRUCTORS(String)
};

// clang-format off
extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
void String::WriteToFlat(String source, uint16_t* sink, int from, int to);
// clang-format on

class SubStringRange {
 public:
  inline SubStringRange(String string, const DisallowHeapAllocation& no_gc,
                        int first = 0, int length = -1);
  class iterator;
  inline iterator begin();
  inline iterator end();

 private:
  String string_;
  int first_;
  int length_;
  const DisallowHeapAllocation& no_gc_;
};

// The SeqString abstract class captures sequential string values.
class SeqString : public TorqueGeneratedSeqString<SeqString, String> {
 public:
  // Truncate the string in-place if possible and return the result.
  // In case of new_length == 0, the empty string is returned without
  // truncating the original string.
  V8_WARN_UNUSED_RESULT static Handle<String> Truncate(Handle<SeqString> string,
                                                       int new_length);

  TQ_OBJECT_CONSTRUCTORS(SeqString)
};

class InternalizedString
    : public TorqueGeneratedInternalizedString<InternalizedString, String> {
 public:
  // TODO(neis): Possibly move some stuff from String here.

  TQ_OBJECT_CONSTRUCTORS(InternalizedString)
};

// The OneByteString class captures sequential one-byte string objects.
// Each character in the OneByteString is an one-byte character.
class SeqOneByteString
    : public TorqueGeneratedSeqOneByteString<SeqOneByteString, SeqString> {
 public:
  static const bool kHasOneByteEncoding = true;
  using Char = uint8_t;

  // Dispatched behavior.
  inline uint8_t Get(int index);
  inline void SeqOneByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uint8_t* GetChars(const DisallowHeapAllocation& no_gc);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  void clear_padding();

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of an OneByteString
  // instance.
  inline int SeqOneByteStringSize(InstanceType instance_type);

  // Computes the size for an OneByteString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length * kCharSize);
  }

  // Maximal memory usage for a single sequential one-byte string.
  static const int kMaxCharsSize = kMaxLength;
  static const int kMaxSize = OBJECT_POINTER_ALIGN(kMaxCharsSize + kHeaderSize);
  STATIC_ASSERT((kMaxSize - kHeaderSize) >= String::kMaxLength);

  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(SeqOneByteString)
};

// The TwoByteString class captures sequential unicode string objects.
// Each character in the TwoByteString is a two-byte uint16_t.
class SeqTwoByteString
    : public TorqueGeneratedSeqTwoByteString<SeqTwoByteString, SeqString> {
 public:
  static const bool kHasOneByteEncoding = false;
  using Char = uint16_t;

  // Dispatched behavior.
  inline uint16_t Get(int index);
  inline void SeqTwoByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uc16* GetChars(const DisallowHeapAllocation& no_gc);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  void clear_padding();

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of a TwoByteString
  // instance.
  inline int SeqTwoByteStringSize(InstanceType instance_type);

  // Computes the size for a TwoByteString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length * kShortSize);
  }

  // Maximal memory usage for a single sequential two-byte string.
  static const int kMaxCharsSize = kMaxLength * 2;
  static const int kMaxSize = OBJECT_POINTER_ALIGN(kMaxCharsSize + kHeaderSize);
  STATIC_ASSERT(static_cast<int>((kMaxSize - kHeaderSize) / sizeof(uint16_t)) >=
                String::kMaxLength);

  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(SeqTwoByteString)
};

// The ConsString class describes string values built by using the
// addition operator on strings.  A ConsString is a pair where the
// first and second components are pointers to other string values.
// One or both components of a ConsString can be pointers to other
// ConsStrings, creating a binary tree of ConsStrings where the leaves
// are non-ConsString string values.  The string value represented by
// a ConsString can be obtained by concatenating the leaf string
// values in a left-to-right depth-first traversal of the tree.
class ConsString : public TorqueGeneratedConsString<ConsString, String> {
 public:
  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object unchecked_first();

  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object unchecked_second();

  // Dispatched behavior.
  V8_EXPORT_PRIVATE uint16_t Get(int index);

  // Minimum length for a cons string.
  static const int kMinLength = 13;

  using BodyDescriptor = FixedBodyDescriptor<kFirstOffset, kSize, kSize>;

  DECL_VERIFIER(ConsString)

  TQ_OBJECT_CONSTRUCTORS(ConsString)
};

// The ThinString class describes string objects that are just references
// to another string object. They are used for in-place internalization when
// the original string cannot actually be internalized in-place: in these
// cases, the original string is converted to a ThinString pointing at its
// internalized version (which is allocated as a new object).
// In terms of memory layout and most algorithms operating on strings,
// ThinStrings can be thought of as "one-part cons strings".
class ThinString : public TorqueGeneratedThinString<ThinString, String> {
 public:
  DECL_GETTER(unchecked_actual, HeapObject)

  V8_EXPORT_PRIVATE uint16_t Get(int index);

  DECL_VERIFIER(ThinString)

  using BodyDescriptor = FixedBodyDescriptor<kActualOffset, kSize, kSize>;

  TQ_OBJECT_CONSTRUCTORS(ThinString)
};

// The Sliced String class describes strings that are substrings of another
// sequential string.  The motivation is to save time and memory when creating
// a substring.  A Sliced String is described as a pointer to the parent,
// the offset from the start of the parent string and the length.  Using
// a Sliced String therefore requires unpacking of the parent string and
// adding the offset to the start address.  A substring of a Sliced String
// are not nested since the double indirection is simplified when creating
// such a substring.
// Currently missing features are:
//  - handling externalized parent strings
//  - external strings as parent
//  - truncating sliced string to enable otherwise unneeded parent to be GC'ed.
class SlicedString : public TorqueGeneratedSlicedString<SlicedString, String> {
 public:
  inline void set_parent(String parent,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  DECL_INT_ACCESSORS(offset)
  // Dispatched behavior.
  V8_EXPORT_PRIVATE uint16_t Get(int index);

  // Minimum length for a sliced string.
  static const int kMinLength = 13;

  using BodyDescriptor = FixedBodyDescriptor<kParentOffset, kSize, kSize>;

  DECL_VERIFIER(SlicedString)

  TQ_OBJECT_CONSTRUCTORS(SlicedString)
};

// The ExternalString class describes string values that are backed by
// a string resource that lies outside the V8 heap.  ExternalStrings
// consist of the length field common to all strings, a pointer to the
// external resource.  It is important to ensure (externally) that the
// resource is not deallocated while the ExternalString is live in the
// V8 heap.
//
// The API expects that all ExternalStrings are created through the
// API.  Therefore, ExternalStrings should not be used internally.
class ExternalString : public String {
 public:
  DECL_CAST(ExternalString)
  DECL_VERIFIER(ExternalString)

  DEFINE_FIELD_OFFSET_CONSTANTS(String::kHeaderSize,
                                TORQUE_GENERATED_EXTERNAL_STRING_FIELDS)

  // Size of uncached external strings.
  static const int kUncachedSize =
      kResourceOffset + FIELD_SIZE(kResourceOffset);

  // Return whether the external string data pointer is not cached.
  inline bool is_uncached() const;
  // Size in bytes of the external payload.
  int ExternalPayloadSize() const;

  // Used in the serializer/deserializer.
  inline Address resource_as_address();
  inline void set_address_as_resource(Address address);
  inline uint32_t resource_as_uint32();
  inline void set_uint32_as_resource(uint32_t value);

  // Disposes string's resource object if it has not already been disposed.
  inline void DisposeResource();

  STATIC_ASSERT(kResourceOffset == Internals::kStringResourceOffset);
  static const int kSizeOfAllExternalStrings = kHeaderSize;

  OBJECT_CONSTRUCTORS(ExternalString, String);
};

// The ExternalOneByteString class is an external string backed by an
// one-byte string.
class ExternalOneByteString : public ExternalString {
 public:
  static const bool kHasOneByteEncoding = true;

  using Resource = v8::String::ExternalOneByteStringResource;

  // The underlying resource.
  inline const Resource* resource();

  // It is assumed that the previous resource is null. If it is not null, then
  // it is the responsability of the caller the handle the previous resource.
  inline void SetResource(Isolate* isolate, const Resource* buffer);
  // Used only during serialization.
  inline void set_resource(const Resource* buffer);

  // Update the pointer cache to the external character array.
  // The cached pointer is always valid, as the external character array does =
  // not move during lifetime.  Deserialization is the only exception, after
  // which the pointer cache has to be refreshed.
  inline void update_data_cache();

  inline const uint8_t* GetChars();

  // Dispatched behavior.
  inline uint8_t Get(int index);

  DECL_CAST(ExternalOneByteString)

  class BodyDescriptor;

  DEFINE_FIELD_OFFSET_CONSTANTS(
      ExternalString::kHeaderSize,
      TORQUE_GENERATED_EXTERNAL_ONE_BYTE_STRING_FIELDS)

  STATIC_ASSERT(kSize == kSizeOfAllExternalStrings);

  OBJECT_CONSTRUCTORS(ExternalOneByteString, ExternalString);
};

// The ExternalTwoByteString class is an external string backed by a UTF-16
// encoded string.
class ExternalTwoByteString : public ExternalString {
 public:
  static const bool kHasOneByteEncoding = false;

  using Resource = v8::String::ExternalStringResource;

  // The underlying string resource.
  inline const Resource* resource();

  // It is assumed that the previous resource is null. If it is not null, then
  // it is the responsability of the caller the handle the previous resource.
  inline void SetResource(Isolate* isolate, const Resource* buffer);
  // Used only during serialization.
  inline void set_resource(const Resource* buffer);

  // Update the pointer cache to the external character array.
  // The cached pointer is always valid, as the external character array does =
  // not move during lifetime.  Deserialization is the only exception, after
  // which the pointer cache has to be refreshed.
  inline void update_data_cache();

  inline const uint16_t* GetChars();

  // Dispatched behavior.
  inline uint16_t Get(int index);

  // For regexp code.
  inline const uint16_t* ExternalTwoByteStringGetData(unsigned start);

  DECL_CAST(ExternalTwoByteString)

  class BodyDescriptor;

  DEFINE_FIELD_OFFSET_CONSTANTS(
      ExternalString::kHeaderSize,
      TORQUE_GENERATED_EXTERNAL_TWO_BYTE_STRING_FIELDS)

  STATIC_ASSERT(kSize == kSizeOfAllExternalStrings);

  OBJECT_CONSTRUCTORS(ExternalTwoByteString, ExternalString);
};

// A flat string reader provides random access to the contents of a
// string independent of the character width of the string.  The handle
// must be valid as long as the reader is being used.
class V8_EXPORT_PRIVATE FlatStringReader : public Relocatable {
 public:
  FlatStringReader(Isolate* isolate, Handle<String> str);
  FlatStringReader(Isolate* isolate, Vector<const char> input);
  void PostGarbageCollection() override;
  inline uc32 Get(int index);
  template <typename Char>
  inline Char Get(int index);
  int length() { return length_; }

 private:
  Address* str_;
  bool is_one_byte_;
  int length_;
  const void* start_;
};

// This maintains an off-stack representation of the stack frames required
// to traverse a ConsString, allowing an entirely iterative and restartable
// traversal of the entire string
class ConsStringIterator {
 public:
  inline ConsStringIterator() = default;
  inline explicit ConsStringIterator(ConsString cons_string, int offset = 0) {
    Reset(cons_string, offset);
  }
  inline void Reset(ConsString cons_string, int offset = 0) {
    depth_ = 0;
    // Next will always return nullptr.
    if (cons_string.is_null()) return;
    Initialize(cons_string, offset);
  }
  // Returns nullptr when complete.
  inline String Next(int* offset_out) {
    *offset_out = 0;
    if (depth_ == 0) return String();
    return Continue(offset_out);
  }

 private:
  static const int kStackSize = 32;
  // Use a mask instead of doing modulo operations for stack wrapping.
  static const int kDepthMask = kStackSize - 1;
  static_assert(base::bits::IsPowerOfTwo(kStackSize),
                "kStackSize must be power of two");
  static inline int OffsetForDepth(int depth);

  inline void PushLeft(ConsString string);
  inline void PushRight(ConsString string);
  inline void AdjustMaximumDepth();
  inline void Pop();
  inline bool StackBlown() { return maximum_depth_ - depth_ == kStackSize; }
  V8_EXPORT_PRIVATE void Initialize(ConsString cons_string, int offset);
  V8_EXPORT_PRIVATE String Continue(int* offset_out);
  String NextLeaf(bool* blew_stack);
  String Search(int* offset_out);

  // Stack must always contain only frames for which right traversal
  // has not yet been performed.
  ConsString frames_[kStackSize];
  ConsString root_;
  int depth_;
  int maximum_depth_;
  int consumed_;
  DISALLOW_COPY_AND_ASSIGN(ConsStringIterator);
};

class StringCharacterStream {
 public:
  inline explicit StringCharacterStream(String string, int offset = 0);
  inline uint16_t GetNext();
  inline bool HasMore();
  inline void Reset(String string, int offset = 0);
  inline void VisitOneByteString(const uint8_t* chars, int length);
  inline void VisitTwoByteString(const uint16_t* chars, int length);

 private:
  ConsStringIterator iter_;
  bool is_one_byte_;
  union {
    const uint8_t* buffer8_;
    const uint16_t* buffer16_;
  };
  const uint8_t* end_;
  DISALLOW_COPY_AND_ASSIGN(StringCharacterStream);
};

template <typename Char>
struct CharTraits;

template <>
struct CharTraits<uint8_t> {
  using String = SeqOneByteString;
  using ExternalString = ExternalOneByteString;
};

template <>
struct CharTraits<uint16_t> {
  using String = SeqTwoByteString;
  using ExternalString = ExternalTwoByteString;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_H_
