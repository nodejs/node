// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_LITERAL_BUFFER_H_
#define V8_PARSING_LITERAL_BUFFER_H_

#include "src/strings/unicode-decoder.h"
#include "src/utils/vector.h"

namespace v8 {
namespace internal {

// LiteralBuffer -  Collector of chars of literals.
class LiteralBuffer final {
 public:
  LiteralBuffer() : backing_store_(), position_(0), is_one_byte_(true) {}

  ~LiteralBuffer() { backing_store_.Dispose(); }

  LiteralBuffer(const LiteralBuffer&) = delete;
  LiteralBuffer& operator=(const LiteralBuffer&) = delete;

  V8_INLINE void AddChar(char code_unit) {
    DCHECK(IsValidAscii(code_unit));
    AddOneByteChar(static_cast<byte>(code_unit));
  }

  V8_INLINE void AddChar(uc32 code_unit) {
    if (is_one_byte()) {
      if (code_unit <= static_cast<uc32>(unibrow::Latin1::kMaxChar)) {
        AddOneByteChar(static_cast<byte>(code_unit));
        return;
      }
      ConvertToTwoByte();
    }
    AddTwoByteChar(code_unit);
  }

  bool is_one_byte() const { return is_one_byte_; }

  bool Equals(Vector<const char> keyword) const {
    return is_one_byte() && keyword.length() == position_ &&
           (memcmp(keyword.begin(), backing_store_.begin(), position_) == 0);
  }

  Vector<const uint16_t> two_byte_literal() const {
    return literal<uint16_t>();
  }

  Vector<const uint8_t> one_byte_literal() const { return literal<uint8_t>(); }

  template <typename Char>
  Vector<const Char> literal() const {
    DCHECK_EQ(is_one_byte_, sizeof(Char) == 1);
    DCHECK_EQ(position_ & (sizeof(Char) - 1), 0);
    return Vector<const Char>(
        reinterpret_cast<const Char*>(backing_store_.begin()),
        position_ >> (sizeof(Char) - 1));
  }

  int length() const { return is_one_byte() ? position_ : (position_ >> 1); }

  void Start() {
    position_ = 0;
    is_one_byte_ = true;
  }

  template <typename LocalIsolate>
  Handle<String> Internalize(LocalIsolate* isolate) const;

 private:
  static const int kInitialCapacity = 16;
  static const int kGrowthFactor = 4;
  static const int kMaxGrowth = 1 * MB;

  inline bool IsValidAscii(char code_unit) {
    // Control characters and printable characters span the range of
    // valid ASCII characters (0-127). Chars are unsigned on some
    // platforms which causes compiler warnings if the validity check
    // tests the lower bound >= 0 as it's always true.
    return iscntrl(code_unit) || isprint(code_unit);
  }

  V8_INLINE void AddOneByteChar(byte one_byte_char) {
    DCHECK(is_one_byte());
    if (position_ >= backing_store_.length()) ExpandBuffer();
    backing_store_[position_] = one_byte_char;
    position_ += kOneByteSize;
  }

  void AddTwoByteChar(uc32 code_unit);
  int NewCapacity(int min_capacity);
  void ExpandBuffer();
  void ConvertToTwoByte();

  Vector<byte> backing_store_;
  int position_;

  bool is_one_byte_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_LITERAL_BUFFER_H_
