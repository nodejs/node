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

// --- U t f   8   a n d   16 ---

template <typename Data>
class Buffer {
 public:
  inline Buffer(Data data, unsigned length) : data_(data), length_(length) { }
  inline Buffer() : data_(0), length_(0) { }
  Data data() { return data_; }
  unsigned length() { return length_; }
 private:
  Data data_;
  unsigned length_;
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
  static inline uchar LeadSurrogate(int char_code) {
    return 0xd800 + (((char_code - 0x10000) >> 10) & 0x3ff);
  }
  static inline uchar TrailSurrogate(int char_code) {
    return 0xdc00 + (char_code & 0x3ff);
  }
};


class Utf8 {
 public:
  static inline uchar Length(uchar chr, int previous);
  static inline unsigned Encode(
      char* out, uchar c, int previous);
  static const byte* ReadBlock(Buffer<const char*> str, byte* buffer,
      unsigned capacity, unsigned* chars_read, unsigned* offset);
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

 private:
  template <unsigned s> friend class Utf8InputBuffer;
  friend class Test;
  static inline uchar ValueOf(const byte* str,
                              unsigned length,
                              unsigned* cursor);
};

// --- C h a r a c t e r   S t r e a m ---

class CharacterStream {
 public:
  inline uchar GetNext();
  inline bool has_more() { return remaining_ != 0; }
  // Note that default implementation is not efficient.
  virtual void Seek(unsigned);
  unsigned Length();
  unsigned Utf16Length();
  virtual ~CharacterStream() { }
  static inline bool EncodeCharacter(uchar c, byte* buffer, unsigned capacity,
      unsigned& offset);
  static inline bool EncodeAsciiCharacter(uchar c, byte* buffer,
      unsigned capacity, unsigned& offset);
  static inline bool EncodeNonAsciiCharacter(uchar c, byte* buffer,
      unsigned capacity, unsigned& offset);
  static inline uchar DecodeCharacter(const byte* buffer, unsigned* offset);
  virtual void Rewind() = 0;

 protected:
  virtual void FillBuffer() = 0;
  // The number of characters left in the current buffer
  unsigned remaining_;
  // The current offset within the buffer
  unsigned cursor_;
  // The buffer containing the decoded characters.
  const byte* buffer_;
};

// --- I n p u t   B u f f e r ---

/**
 * Provides efficient access to encoded characters in strings.  It
 * does so by reading characters one block at a time, rather than one
 * character at a time, which gives string implementations an
 * opportunity to optimize the decoding.
 */
template <class Reader, class Input = Reader*, unsigned kSize = 256>
class InputBuffer : public CharacterStream {
 public:
  virtual void Rewind();
  inline void Reset(Input input);
  void Seek(unsigned position);
  inline void Reset(unsigned position, Input input);
 protected:
  InputBuffer() { }
  explicit InputBuffer(Input input) { Reset(input); }
  virtual void FillBuffer();

  // A custom offset that can be used by the string implementation to
  // mark progress within the encoded string.
  unsigned offset_;
  // The input string
  Input input_;
  // To avoid heap allocation, we keep an internal buffer to which
  // the encoded string can write its characters.  The string
  // implementation is free to decide whether it wants to use this
  // buffer or not.
  byte util_buffer_[kSize];
};

// --- U t f 8   I n p u t   B u f f e r ---

template <unsigned s = 256>
class Utf8InputBuffer : public InputBuffer<Utf8, Buffer<const char*>, s> {
 public:
  inline Utf8InputBuffer() { }
  inline Utf8InputBuffer(const char* data, unsigned length);
  inline void Reset(const char* data, unsigned length) {
    InputBuffer<Utf8, Buffer<const char*>, s>::Reset(
        Buffer<const char*>(data, length));
  }
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
  static int Convert(uchar c,
                     uchar n,
                     uchar* result,
                     bool* allow_caching_ptr);
};
struct ToUppercase {
  static const int kMaxWidth = 3;
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
