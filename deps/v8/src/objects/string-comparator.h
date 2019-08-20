// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_COMPARATOR_H_
#define V8_OBJECTS_STRING_COMPARATOR_H_

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/objects/string.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// Compares the contents of two strings by reading and comparing
// int-sized blocks of characters.
template <typename Char>
static inline bool CompareRawStringContents(const Char* const a,
                                            const Char* const b, int length) {
  return CompareChars(a, b, length) == 0;
}

template <typename Chars1, typename Chars2>
class RawStringComparator : public AllStatic {
 public:
  static inline bool compare(const Chars1* a, const Chars2* b, int len) {
    DCHECK(sizeof(Chars1) != sizeof(Chars2));
    for (int i = 0; i < len; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }
};

template <>
class RawStringComparator<uint16_t, uint16_t> {
 public:
  static inline bool compare(const uint16_t* a, const uint16_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};

template <>
class RawStringComparator<uint8_t, uint8_t> {
 public:
  static inline bool compare(const uint8_t* a, const uint8_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};

class StringComparator {
  class State {
   public:
    State() : is_one_byte_(true), length_(0), buffer8_(nullptr) {}

    void Init(String string);

    inline void VisitOneByteString(const uint8_t* chars, int length) {
      is_one_byte_ = true;
      buffer8_ = chars;
      length_ = length;
    }

    inline void VisitTwoByteString(const uint16_t* chars, int length) {
      is_one_byte_ = false;
      buffer16_ = chars;
      length_ = length;
    }

    void Advance(int consumed);

    ConsStringIterator iter_;
    bool is_one_byte_;
    int length_;
    union {
      const uint8_t* buffer8_;
      const uint16_t* buffer16_;
    };

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

 public:
  inline StringComparator() = default;

  template <typename Chars1, typename Chars2>
  static inline bool Equals(State* state_1, State* state_2, int to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return RawStringComparator<Chars1, Chars2>::compare(a, b, to_check);
  }

  bool Equals(String string_1, String string_2);

 private:
  State state_1_;
  State state_2_;

  DISALLOW_COPY_AND_ASSIGN(StringComparator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_STRING_COMPARATOR_H_
