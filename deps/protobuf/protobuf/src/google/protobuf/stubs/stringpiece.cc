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
#include <google/protobuf/stubs/stringpiece.h>

#include <string.h>
#include <algorithm>
#include <climits>
#include <string>
#include <ostream>

#include <google/protobuf/stubs/logging.h>

namespace google {
namespace protobuf {
std::ostream& operator<<(std::ostream& o, StringPiece piece) {
  o.write(piece.data(), piece.size());
  return o;
}

// Out-of-line error path.
void StringPiece::LogFatalSizeTooBig(size_t size, const char* details) {
  GOOGLE_LOG(FATAL) << "size too big: " << size << " details: " << details;
}

StringPiece::StringPiece(StringPiece x, stringpiece_ssize_type pos)
    : ptr_(x.ptr_ + pos), length_(x.length_ - pos) {
  GOOGLE_DCHECK_LE(0, pos);
  GOOGLE_DCHECK_LE(pos, x.length_);
}

StringPiece::StringPiece(StringPiece x,
                         stringpiece_ssize_type pos,
                         stringpiece_ssize_type len)
    : ptr_(x.ptr_ + pos), length_(std::min(len, x.length_ - pos)) {
  GOOGLE_DCHECK_LE(0, pos);
  GOOGLE_DCHECK_LE(pos, x.length_);
  GOOGLE_DCHECK_GE(len, 0);
}

void StringPiece::CopyToString(string* target) const {
  target->assign(ptr_, length_);
}

void StringPiece::AppendToString(string* target) const {
  target->append(ptr_, length_);
}

bool StringPiece::Consume(StringPiece x) {
  if (starts_with(x)) {
    ptr_ += x.length_;
    length_ -= x.length_;
    return true;
  }
  return false;
}

bool StringPiece::ConsumeFromEnd(StringPiece x) {
  if (ends_with(x)) {
    length_ -= x.length_;
    return true;
  }
  return false;
}

stringpiece_ssize_type StringPiece::copy(char* buf,
                                         size_type n,
                                         size_type pos) const {
  stringpiece_ssize_type ret = std::min(length_ - pos, n);
  memcpy(buf, ptr_ + pos, ret);
  return ret;
}

bool StringPiece::contains(StringPiece s) const {
  return find(s, 0) != npos;
}

stringpiece_ssize_type StringPiece::find(StringPiece s, size_type pos) const {
  if (length_ <= 0 || pos > static_cast<size_type>(length_)) {
    if (length_ == 0 && pos == 0 && s.length_ == 0) return 0;
    return npos;
  }
  const char *result = std::search(ptr_ + pos, ptr_ + length_,
                                   s.ptr_, s.ptr_ + s.length_);
  return result == ptr_ + length_ ? npos : result - ptr_;
}

stringpiece_ssize_type StringPiece::find(char c, size_type pos) const {
  if (length_ <= 0 || pos >= static_cast<size_type>(length_)) {
    return npos;
  }
  const char* result = static_cast<const char*>(
      memchr(ptr_ + pos, c, length_ - pos));
  return result != nullptr ? result - ptr_ : npos;
}

stringpiece_ssize_type StringPiece::rfind(StringPiece s, size_type pos) const {
  if (length_ < s.length_) return npos;
  const size_t ulen = length_;
  if (s.length_ == 0) return std::min(ulen, pos);

  const char* last = ptr_ + std::min(ulen - s.length_, pos) + s.length_;
  const char* result = std::find_end(ptr_, last, s.ptr_, s.ptr_ + s.length_);
  return result != last ? result - ptr_ : npos;
}

// Search range is [0..pos] inclusive.  If pos == npos, search everything.
stringpiece_ssize_type StringPiece::rfind(char c, size_type pos) const {
  // Note: memrchr() is not available on Windows.
  if (length_ <= 0) return npos;
  for (stringpiece_ssize_type i =
      std::min(pos, static_cast<size_type>(length_ - 1));
       i >= 0; --i) {
    if (ptr_[i] == c) {
      return i;
    }
  }
  return npos;
}

// For each character in characters_wanted, sets the index corresponding
// to the ASCII code of that character to 1 in table.  This is used by
// the find_.*_of methods below to tell whether or not a character is in
// the lookup table in constant time.
// The argument `table' must be an array that is large enough to hold all
// the possible values of an unsigned char.  Thus it should be be declared
// as follows:
//   bool table[UCHAR_MAX + 1]
static inline void BuildLookupTable(StringPiece characters_wanted,
                                    bool* table) {
  const stringpiece_ssize_type length = characters_wanted.length();
  const char* const data = characters_wanted.data();
  for (stringpiece_ssize_type i = 0; i < length; ++i) {
    table[static_cast<unsigned char>(data[i])] = true;
  }
}

stringpiece_ssize_type StringPiece::find_first_of(StringPiece s,
                                                  size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) {
    return npos;
  }
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (stringpiece_ssize_type i = pos; i < length_; ++i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

stringpiece_ssize_type StringPiece::find_first_not_of(StringPiece s,
                                                      size_type pos) const {
  if (length_ <= 0) return npos;
  if (s.length_ <= 0) return 0;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (stringpiece_ssize_type i = pos; i < length_; ++i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

stringpiece_ssize_type StringPiece::find_first_not_of(char c,
                                                      size_type pos) const {
  if (length_ <= 0) return npos;

  for (; pos < static_cast<size_type>(length_); ++pos) {
    if (ptr_[pos] != c) {
      return pos;
    }
  }
  return npos;
}

stringpiece_ssize_type StringPiece::find_last_of(StringPiece s,
                                                 size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) return npos;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (stringpiece_ssize_type i =
       std::min(pos, static_cast<size_type>(length_ - 1)); i >= 0; --i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

stringpiece_ssize_type StringPiece::find_last_not_of(StringPiece s,
                                                     size_type pos) const {
  if (length_ <= 0) return npos;

  stringpiece_ssize_type i = std::min(pos, static_cast<size_type>(length_ - 1));
  if (s.length_ <= 0) return i;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (; i >= 0; --i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

stringpiece_ssize_type StringPiece::find_last_not_of(char c,
                                                     size_type pos) const {
  if (length_ <= 0) return npos;

  for (stringpiece_ssize_type i =
       std::min(pos, static_cast<size_type>(length_ - 1)); i >= 0; --i) {
    if (ptr_[i] != c) {
      return i;
    }
  }
  return npos;
}

StringPiece StringPiece::substr(size_type pos, size_type n) const {
  if (pos > length_) pos = length_;
  if (n > length_ - pos) n = length_ - pos;
  return StringPiece(ptr_ + pos, n);
}

const StringPiece::size_type StringPiece::npos = size_type(-1);

}  // namespace protobuf
}  // namespace google
