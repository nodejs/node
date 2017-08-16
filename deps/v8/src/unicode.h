// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_H_
#define V8_UNICODE_H_

#include <sys/types.h>
#include "src/globals.h"
#include "src/utils.h"
/**
 * \file
 * Definitions and convenience functions for working with unicode.
 */

namespace unibrow {

typedef unsigned int uchar;
typedef unsigned char byte;

/**
 * The max length of the result of converting the case of a single
 * character.
 */
const int kMaxMappingSize = 4;

template <class T, int size = 256>
class Predicate {
 public:
  inline Predicate() { }
  inline bool get(uchar c);

 private:
  friend class Test;
  bool CalculateValue(uchar c);
  class CacheEntry {
   public:
    inline CacheEntry()
        : bit_field_(CodePointField::encode(0) | ValueField::encode(0)) {}
    inline CacheEntry(uchar code_point, bool value)
        : bit_field_(CodePointField::encode(code_point) |
                     ValueField::encode(value)) {}

    uchar code_point() const { return CodePointField::decode(bit_field_); }
    bool value() const { return ValueField::decode(bit_field_); }

   private:
    class CodePointField : public v8::internal::BitField<uchar, 0, 21> {};
    class ValueField : public v8::internal::BitField<bool, 21, 1> {};

    uint32_t bit_field_;
  };
  static const int kSize = size;
  static const int kMask = kSize - 1;
  CacheEntry entries_[kSize];
};


// A cache used in case conversion.  It caches the value for characters
// that either have no mapping or map to a single character independent
// of context.  Characters that map to more than one character or that
// map differently depending on context are always looked up.
template <class T, int size = 256>
class Mapping {
 public:
  inline Mapping() { }
  inline int get(uchar c, uchar n, uchar* result);
 private:
  friend class Test;
  int CalculateValue(uchar c, uchar n, uchar* result);
  struct CacheEntry {
    inline CacheEntry() : code_point_(kNoChar), offset_(0) { }
    inline CacheEntry(uchar code_point, signed offset)
      : code_point_(code_point),
        offset_(offset) { }
    uchar code_point_;
    signed offset_;
    static const int kNoChar = (1 << 21) - 1;
  };
  static const int kSize = size;
  static const int kMask = kSize - 1;
  CacheEntry entries_[kSize];
};


class UnicodeData {
 private:
  friend class Test;
  static int GetByteCount();
  static const uchar kMaxCodePoint;
};


class Utf16 {
 public:
  static inline bool IsSurrogatePair(int lead, int trail) {
    return IsLeadSurrogate(lead) && IsTrailSurrogate(trail);
  }
  static inline bool IsLeadSurrogate(int code) {
    if (code == kNoPreviousCharacter) return false;
    return (code & 0xfc00) == 0xd800;
  }
  static inline bool IsTrailSurrogate(int code) {
    if (code == kNoPreviousCharacter) return false;
    return (code & 0xfc00) == 0xdc00;
  }

  static inline int CombineSurrogatePair(uchar lead, uchar trail) {
    return 0x10000 + ((lead & 0x3ff) << 10) + (trail & 0x3ff);
  }
  static const int kNoPreviousCharacter = -1;
  static const uchar kMaxNonSurrogateCharCode = 0xffff;
  // Encoding a single UTF-16 code unit will produce 1, 2 or 3 bytes
  // of UTF-8 data.  The special case where the unit is a surrogate
  // trail produces 1 byte net, because the encoding of the pair is
  // 4 bytes and the 3 bytes that were used to encode the lead surrogate
  // can be reclaimed.
  static const int kMaxExtraUtf8BytesForOneUtf16CodeUnit = 3;
  // One UTF-16 surrogate is endoded (illegally) as 3 UTF-8 bytes.
  // The illegality stems from the surrogate not being part of a pair.
  static const int kUtf8BytesToCodeASurrogate = 3;
  static inline uint16_t LeadSurrogate(uint32_t char_code) {
    return 0xd800 + (((char_code - 0x10000) >> 10) & 0x3ff);
  }
  static inline uint16_t TrailSurrogate(uint32_t char_code) {
    return 0xdc00 + (char_code & 0x3ff);
  }
};


class Utf8 {
 public:
  static inline uchar Length(uchar chr, int previous);
  static inline unsigned EncodeOneByte(char* out, uint8_t c);
  static inline unsigned Encode(char* out,
                                uchar c,
                                int previous,
                                bool replace_invalid = false);
  static uchar CalculateValue(const byte* str, size_t length, size_t* cursor);

  // The unicode replacement character, used to signal invalid unicode
  // sequences (e.g. an orphan surrogate) when converting to a UTF-8 encoding.
  static const uchar kBadChar = 0xFFFD;
  static const uchar kBufferEmpty = 0x0;
  static const uchar kIncomplete = 0xFFFFFFFC;  // any non-valid code point.
  static const unsigned kMaxEncodedSize   = 4;
  static const unsigned kMaxOneByteChar   = 0x7f;
  static const unsigned kMaxTwoByteChar   = 0x7ff;
  static const unsigned kMaxThreeByteChar = 0xffff;
  static const unsigned kMaxFourByteChar  = 0x1fffff;

  // A single surrogate is coded as a 3 byte UTF-8 sequence, but two together
  // that match are coded as a 4 byte UTF-8 sequence.
  static const unsigned kBytesSavedByCombiningSurrogates = 2;
  static const unsigned kSizeOfUnmatchedSurrogate = 3;
  // The maximum size a single UTF-16 code unit may take up when encoded as
  // UTF-8.
  static const unsigned kMax16BitCodeUnitSize  = 3;
  static inline uchar ValueOf(const byte* str, size_t length, size_t* cursor);

  typedef uint32_t Utf8IncrementalBuffer;
  static uchar ValueOfIncremental(byte next_byte,
                                  Utf8IncrementalBuffer* buffer);
  static uchar ValueOfIncrementalFinish(Utf8IncrementalBuffer* buffer);

  // Excludes non-characters from the set of valid code points.
  static inline bool IsValidCharacter(uchar c);

  static bool Validate(const byte* str, size_t length);
};

struct Uppercase {
  static bool Is(uchar c);
};
struct Lowercase {
  static bool Is(uchar c);
};
struct Letter {
  static bool Is(uchar c);
};
struct V8_EXPORT_PRIVATE ID_Start {
  static bool Is(uchar c);
};
struct V8_EXPORT_PRIVATE ID_Continue {
  static bool Is(uchar c);
};
struct V8_EXPORT_PRIVATE WhiteSpace {
  static bool Is(uchar c);
};
struct V8_EXPORT_PRIVATE LineTerminator {
  static bool Is(uchar c);
};
struct ToLowercase {
  static const int kMaxWidth = 3;
  static const bool kIsToLower = true;
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};
struct ToUppercase {
  static const int kMaxWidth = 3;
  static const bool kIsToLower = false;
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};
struct Ecma262Canonicalize {
  static const int kMaxWidth = 1;
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};
struct Ecma262UnCanonicalize {
  static const int kMaxWidth = 4;
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};
struct CanonicalizationRange {
  static const int kMaxWidth = 1;
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};

}  // namespace unibrow

#endif  // V8_UNICODE_H_
