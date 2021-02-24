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

class StringComparator {
  class State {
   public:
    State() : is_one_byte_(true), length_(0), buffer8_(nullptr) {}
    State(const State&) = delete;
    State& operator=(const State&) = delete;

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
  };

 public:
  inline StringComparator() = default;
  StringComparator(const StringComparator&) = delete;
  StringComparator& operator=(const StringComparator&) = delete;

  template <typename Chars1, typename Chars2>
  static inline bool Equals(State* state_1, State* state_2, int to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return CompareCharsEqual(a, b, to_check);
  }

  bool Equals(String string_1, String string_2);

 private:
  State state_1_;
  State state_2_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_STRING_COMPARATOR_H_
