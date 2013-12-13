// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_UNICODE_H_
#define V8_UNICODE_H_

#include <sys/types.h>
#include "globals.h"
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
  struct CacheEntry {
    inline CacheEntry() : code_point_(0), value_(0) { }
    inline CacheEntry(uchar code_point, bool value)
      : code_point_(code_point),
        value_(value) { }
    uchar code_point_ : 21;
    bool value_ : 1;
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

class Latin1 {
 public:
  static const unsigned kMaxChar = 0xff;
  // Returns 0 if character does not convert to single latin-1 character
  // or if the character doesn't not convert back to latin-1 via inverse
  // operation (upper to lower, etc).
  static inline uint16_t ConvertNonLatin1ToLatin1(uint16_t);
};

class Utf8 {
 public:
  static inline uchar Length(uchar chr, int previous);
  static inline unsigned EncodeOneByte(char* out, uint8_t c);
  static inline unsigned Encode(
      char* out, uchar c, int previous);
  static uchar CalculateValue(const byte* str,
                              unsigned length,
                              unsigned* cursor);
  static const uchar kBadChar = 0xFFFD;
  static const unsigned kMaxEncodedSize   = 4;
  static const unsigned kMaxOneByteChar   = 0x7f;
  static const unsigned kMaxTwoByteChar   = 0x7ff;
  static const unsigned kMaxThreeByteChar = 0xffff;
  static const unsigned kMaxFourByteChar  = 0x1fffff;

  // A single surrogate is coded as a 3 byte UTF-8 sequence, but two together
  // that match are coded as a 4 byte UTF-8 sequence.
  static const unsigned kBytesSavedByCombiningSurrogates = 2;
  static const unsigned kSizeOfUnmatchedSurrogate = 3;
  static inline uchar ValueOf(const byte* str,
                              unsigned length,
                              unsigned* cursor);
};


class Utf8DecoderBase {
 public:
  // Initialization done in subclass.
  inline Utf8DecoderBase();
  inline Utf8DecoderBase(uint16_t* buffer,
                         unsigned buffer_length,
                         const uint8_t* stream,
                         unsigned stream_length);
  inline unsigned Utf16Length() const { return utf16_length_; }
 protected:
  // This reads all characters and sets the utf16_length_.
  // The first buffer_length utf16 chars are cached in the buffer.
  void Reset(uint16_t* buffer,
             unsigned buffer_length,
             const uint8_t* stream,
             unsigned stream_length);
  static void WriteUtf16Slow(const uint8_t* stream,
                             uint16_t* data,
                             unsigned length);
  const uint8_t* unbuffered_start_;
  unsigned utf16_length_;
  bool last_byte_of_buffer_unused_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Utf8DecoderBase);
};

template <unsigned kBufferSize>
class Utf8Decoder : public Utf8DecoderBase {
 public:
  inline Utf8Decoder() {}
  inline Utf8Decoder(const char* stream, unsigned length);
  inline void Reset(const char* stream, unsigned length);
  inline unsigned WriteUtf16(uint16_t* data, unsigned length) const;
 private:
  uint16_t buffer_[kBufferSize];
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
struct Space {
  static bool Is(uchar c);
};
struct Number {
  static bool Is(uchar c);
};
struct WhiteSpace {
  static bool Is(uchar c);
};
struct LineTerminator {
  static bool Is(uchar c);
};
struct CombiningMark {
  static bool Is(uchar c);
};
struct ConnectorPunctuation {
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
