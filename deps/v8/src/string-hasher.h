// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_HASHER_H_
#define V8_STRING_HASHER_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class ConsString;
class String;

template <typename T>
class Vector;

class V8_EXPORT_PRIVATE StringHasher {
 public:
  explicit inline StringHasher(int length, uint32_t seed);

  template <typename schar>
  static inline uint32_t HashSequentialString(const schar* chars, int length,
                                              uint32_t seed);

  // Reads all the data, even for long strings and computes the utf16 length.
  static uint32_t ComputeUtf8Hash(Vector<const char> chars, uint32_t seed,
                                  int* utf16_length_out);

  // Calculated hash value for a string consisting of 1 to
  // String::kMaxArrayIndexSize digits with no leading zeros (except "0").
  // value is represented decimal value.
  static uint32_t MakeArrayIndexHash(uint32_t value, int length);

  // No string is allowed to have a hash of zero.  That value is reserved
  // for internal properties.  If the hash calculation yields zero then we
  // use 27 instead.
  static const int kZeroHash = 27;

  // Reusable parts of the hashing algorithm.
  INLINE(static uint32_t AddCharacterCore(uint32_t running_hash, uint16_t c));
  INLINE(static uint32_t GetHashCore(uint32_t running_hash));
  INLINE(static uint32_t ComputeRunningHash(uint32_t running_hash,
                                            const uc16* chars, int length));
  INLINE(static uint32_t ComputeRunningHashOneByte(uint32_t running_hash,
                                                   const char* chars,
                                                   int length));

 protected:
  // Returns the value to store in the hash field of a string with
  // the given length and contents.
  uint32_t GetHashField();
  // Returns true if the hash of this string can be computed without
  // looking at the contents.
  inline bool has_trivial_hash();
  // Adds a block of characters to the hash.
  template <typename Char>
  inline void AddCharacters(const Char* chars, int len);

 private:
  // Add a character to the hash.
  inline void AddCharacter(uint16_t c);
  // Update index. Returns true if string is still an index.
  inline bool UpdateIndex(uint16_t c);

  int length_;
  uint32_t raw_running_hash_;
  uint32_t array_index_;
  bool is_array_index_;
  bool is_first_char_;
  DISALLOW_COPY_AND_ASSIGN(StringHasher);
};

class IteratingStringHasher : public StringHasher {
 public:
  static inline uint32_t Hash(String* string, uint32_t seed);
  inline void VisitOneByteString(const uint8_t* chars, int length);
  inline void VisitTwoByteString(const uint16_t* chars, int length);

 private:
  inline IteratingStringHasher(int len, uint32_t seed);
  void VisitConsString(ConsString* cons_string);
  DISALLOW_COPY_AND_ASSIGN(IteratingStringHasher);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_HASHER_H_
